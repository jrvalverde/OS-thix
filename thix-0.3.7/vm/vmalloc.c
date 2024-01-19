/* vmalloc.c -- Virtual memory sub-allocation routines.  */

/*
 * Thix Operating System
 * Copyright (C) 1993,1996 Tudor Hulubei
 *
 * This program is  free software; you can  redistribute it and/or modify
 * it under the terms  of the GNU General Public  License as published by
 * the  Free Software Foundation; either version  2,  or (at your option)
 * any later version.
 *
 * This program is distributed  in the hope that  it will be  useful, but
 * WITHOUT    ANY  WARRANTY;  without   even   the  implied  warranty  of
 * MERCHANTABILITY  or  FITNESS FOR  A PARTICULAR   PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should  have received a  copy of  the GNU  General Public  License
 * along with   this  program; if   not,  write  to   the Free   Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#include <thix/vm.h>
#include <thix/klib.h>
#include <thix/string.h>
#include <thix/sched.h>
#include <thix/sleep.h>
#include <thix/vmalloc.h>
#include <thix/proc/i386.h>


unsigned short m_vect[6];
int __vm_alloc_dummy__;


/*
 * The idea is to divide 4 Kb physical memory pages  into  smaller pieces
 * and allocate them as needed.  We are keeping  track  of 4 Kb pages  of
 * memory used for sub-allocation by  inserting  them  in  some  per-size
 * organized lists.  The heads of  these lists are kept in m_vect[].  The
 * implementation also allow the use  of  user  defined  allocators, just
 * because some kernel algorithms might need an amount of memory reserved
 * for their use.  When a pointer to such a memory allocator is provided,
 * the first attempt to get some memory is done using it.  If we fail  to
 * get the required amount of memory this way, the  normal  procedure  is
 * used (memory is allocated from the free pool).
 */


/*
 * Initialize the memory sub-allocator.
 */

void
vm_alloc_init(void)
{
    lmemset(m_vect, 0, sizeof(m_vect) >> 2);
}


/*
 * Insert the given PGDATA structure in the head of the 'index' list.
 */

static inline void
vm_alloc_insert(pgdata_t *pgd, int index)
{
    if ((pgd->lnext = m_vect[index]))
	pgdata[m_vect[index]].lprev = pgd - pgdata;

    pgd->lprev = 0;
    m_vect[index] = pgd - pgdata;
}


/*
 * Delete the given PGDATA structure from the head of the 'index' list.
 */

static inline void
vm_alloc_delete(pgdata_t *pgd, int index)
{
    if (pgd->lprev)
    {
	if ((pgdata[pgd->lprev].lnext = pgd->lnext))
	    pgdata[pgd->lnext].lprev = pgd->lprev;
    }
    else
	if ((m_vect[index] = pgd->lnext))
	    pgdata[pgd->lnext].lprev = 0;
}


/*
 * Try to get a memory chunk of size 'size'.  If  get_dedicated_page is not
 * NULL, try to use it to get a page of memory (but only  if  there  is  no
 * half allocated page on the m_vect[] list).  Use *type to report the type
 * of memory allocated (dedicated or not).
 */

void *
vm_alloc(size_t size, int (*get_dedicated_page)(void), int *type)
{
    pgdata_t *pgd = NULL;
    int page = 0, offset, index = bsf(size >> 9) + 1;


    if (m_vect[index])
    {
	/* There are still half allocated pages in the given size list.
	   Pick up the first one.  */
	pgd = &pgdata[m_vect[index]];
	offset = bsf(pgd->aux.map);

	pgd->block = size;
	pgd->aux.map &= ~((1 << ((size >> 9) + offset)) - (1 << offset));
	page = pgd - pgdata;

	/* If the page is full, just delete it from the head of the list.  */
	if (pgd->aux.map == M_PAGE_FULL)
	    vm_alloc_delete(pgd, index);

	*type = pgd->dedicated;
	return (void *)((page << PAGE_SHIFT) + (offset << 9));
    }
    else
    {
	if (get_dedicated_page && (page = get_dedicated_page()))
	{
	    /* We prefer getting pages from the dedicated pool, when
	       possible. The caller specified a local allocator  and
	       we got a page from it, so use it.  */
	    pgd = &pgdata[page];
	    pgd->dedicated = *type = 1;
	}

	if (pgd == NULL)
	{
	    if (free_pages == 0)
	    {
		/* Ok, there is no free memory available.  Return
		   NULL.  The buffer cache will not hang since it
		   always calls vm_alloc() after checking for free
		   memory. exec() should return -ENOMEM.  Returning
		   NULL is much more flexible because it gives anyone
		   a chance to handle this in its own way.  Really !?  */
		return NULL;
	    }

	    /* We do have some free pages, so just pick up one.  */
	    pgd = &pgdata[page = get_page()];
	    pgd->dedicated = *type = 0;
	}

	/* Now the dirty stuff...  */
	pgd->block = size;
	pgd->aux.map = 0xFF & ~((1 << (size >> 9)) - 1);
	vm_alloc_insert(pgd, index);
	return (void *)(page << PAGE_SHIFT);
    }
}


/*
 * Free up a chunk of memory, releasing the page used to its original
 * pool when empty.
 */

void
vm_free(void *address, void (*release_dedicated_page)(int))
{
    pgdata_t *pgd = &pgdata[(unsigned)address >> PAGE_SHIFT];
    int offset = ((unsigned)address & (PAGE_SIZE - 1)) >> 9;
    size_t size = pgd->block;
    int index = bsf(size >> 9) + 1;


#ifdef __PARANOIA__
    if (address == NULL)
	PANIC("trying to free a NULL pointer");
#endif

    if (pgd->aux.map == M_PAGE_FULL)
    {
	pgd->aux.map |= (1 << ((size >> 9) + offset)) - (1 << offset);

	if (pgd->aux.map == M_PAGE_FULL)
	    PANIC("can't free page (address=%x)", address);

	vm_alloc_insert(pgd, index);
	return;
    }
    else
    {
	pgd->aux.map |= (1 << ((size >> 9) + offset)) - (1 << offset);

	if (pgd->aux.map == M_PAGE_EMPTY)
	{
	    /* The page is empty now. Delete it from the list and put  it
	       back in its original pool. Use the local deallocator, when
	       necessary, otherwise release the page for general use.  */
	    vm_alloc_delete(pgd, index);
	    pgd->block = 0;

	    if (pgd->dedicated)
		release_dedicated_page(pgd - pgdata);
	    else
		if (release_page(pgd - pgdata))
		    wakeup(&get_page);
	}
    }
}
