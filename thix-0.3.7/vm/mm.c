/* mm.c -- Memory management routines.  */

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
#include <thix/errno.h>
#include <thix/sleep.h>
#include <thix/sched.h>
#include <thix/kpid.h>
#include <thix/a.out.h>
#include <thix/regular.h>
#include <thix/process.h>
#include <thix/vmalloc.h>
#include <thix/limits.h>
#include <thix/vmdev.h>
#include <thix/vtty.h>
#include <thix/gendrv.h>
#include <thix/proc/i386.h>


extern unsigned KERNEL_TEXT_SIZE;
extern unsigned KERNEL_DATA_SIZE;
extern unsigned KERNEL_BSS_SIZE;
extern unsigned EXTENDED_MEMORY_SIZE;
extern unsigned SWAP_DATA_MAP_ADDRESS;

extern unsigned work_set[SYSTEM_PROCESS_MAX][WORK_SET_SIZE];
extern unsigned *work_set_ptr[];
extern unsigned user_mode_ESP[];

extern descriptor_t gdt[];
extern descriptor_t ldt_descr[];
extern descriptor_t tss_descr[];
extern descriptor_t internal_stack_descr[];

extern int swapper_running;


extern void swapper(void);


unsigned KERNEL_PAGE_TABLES;
unsigned KERNEL_RESERVED_SPACE;

unsigned kernel_static_data;
unsigned *kernel_page_dir;
unsigned *kernel_page_table;

int MAX_PAGES;
#define PG_BUCKETS_NO   199

pgdata_t *pgdata;		/* One entry for each physical page.  */

volatile int free_pages = 0;		/* Free memory pages.  */
int memory_pages;			/* Total memory pages.  */
unsigned free_list_clear_time = 0;	/* Free list clear time stamp.  */

/*
 * ldt[kpid][0] = NULL SELECTOR
 * ldt[kpid][1] = text segment (read/only !?)
 * ldt[kpid][2] = data, bss & stack segment
 */
descriptor_t ldt[SYSTEM_PROCESS_MAX][DESCRIPTORS_PER_LDT];

tss_t u_area[SYSTEM_PROCESS_MAX];
tss_t *this = &u_area[IDLE];


/*
 * Insert a page into the list.
 */

void
pg_list_insert(int page)
{
    pgdata[page].lprev = pgdata[0].lprev;
    pgdata[page].lnext = 0;
    pgdata[pgdata[0].lprev].lnext = page;
    pgdata[0].lprev = page;
    free_pages++;
}


/*
 * Delete a page from the list.
 */

void
pg_list_delete(int page)
{
    pgdata[pgdata[page].lprev].lnext = pgdata[page].lnext;
    pgdata[pgdata[page].lnext].lprev = pgdata[page].lprev;
    pgdata[page].lnext = 0xFFFF;        /* the page is allocated
					   (not in the free list). */
    free_pages--;
}


/*
 * Insert a page into the hash table.
 */

void
pg_hash_insert(int page)
{
    int index = MAX_PAGES +
		(pgdata[page].vmdev ^ pgdata[page].block) % PG_BUCKETS_NO;

    pgdata[page].hnext = pgdata[index].hnext;
    pgdata[page].hprev = index;
    pgdata[pgdata[index].hnext].hprev = page;
    pgdata[index].hnext = page;
}


/*
 * Delete a page from the hash table.
 */

void
pg_hash_delete(int page)
{
    if (pgdata[page].hnext == 0xFFFF)
	return;

    pgdata[pgdata[page].hprev].hnext = pgdata[page].hnext;
    pgdata[pgdata[page].hnext].hprev = pgdata[page].hprev;
    pgdata[page].hnext = 0xFFFF;
}


/*
 * Search in the hash table a page with the supplied vmdev and
 * block number.
 */

int
pg_hash_search(int vmdev, int block)
{
    int page, index = MAX_PAGES + (vmdev ^ block) % PG_BUCKETS_NO;

    for (page = pgdata[index].hnext;
	 page != index &&
	 (pgdata[page].vmdev != vmdev || pgdata[page].block != block);
	 page = pgdata[page].hnext);

    return page == index ? 0 : page;
}


/*
 * Initialize the pages list data structures.
 */

void
pg_init(void)
{
    int i;

    memset(pgdata, 0, (MAX_PAGES + PG_BUCKETS_NO) * sizeof(pgdata_t));

    for (i = 0; i < MAX_PAGES; pgdata[i++].hnext = 0xFFFF);

    pgdata[0].lnext = pgdata[0].lprev = 0;      /* Memory management.  */
    pgdata[1].lnext = pgdata[1].lprev = 1;      /* Buffer cache.  */

    for (i = 0; i < PG_BUCKETS_NO; i++)
	pgdata[MAX_PAGES + i].hnext =
	pgdata[MAX_PAGES + i].hprev = MAX_PAGES + i;
}


/*
 * Get a page from the free list.
 */

int
get_page(void)
{
    int page;
    pgdata_t *pgd;


    while (free_pages == 0)
    {
	PANIC("Going to sleep...\n");

	if (!swapper_running)
	    wakeup(&swapper);

	sleep(&get_page, WAIT_PAGE);
    }

    pgd = &pgdata[page = pgdata[0].lnext];

#ifdef __PARANOIA__
    if (page == 0xFFFF)
	PANIC("allocating an in use page (0xFFFF).\n");
#endif

    if (pgd->block)
    {
	pg_hash_delete(page);
	pgd->block = 0;
    }

#ifdef __PARANOIA__
    if (pgdata[page].count)
	PANIC("reusing page %x with count %x\n", page, pgdata[page].count);
#endif

    pg_list_delete(page);

    pgdata[page].count = 1;

    if (free_pages < LOW_PAGES_LIMIT && !swapper_running)
	wakeup(&swapper);

    return page;
}


/*
 * Release a page back to the free list.
 */

int
release_page(int page)
{
    if (--pgdata[page].count)
	return 0;

    pg_list_insert(page);
    return free_pages;
}


/*
 * Reuse a page that have been previously reinserted into the  free list:
 * a process reclaimed it before the page had a chance to be reallocated.
 * This is going to improve the page cache performance.  It is in fact  a
 * simple call to pg_list_delete(), but with some additional checks.
 */

void
reuse_page(int page)
{
    DEBUG(6, "(%x) vmdev=%x block=%x lnext=%x\n",
	  page, pgdata[page].vmdev, pgdata[page].block, pgdata[page].lnext);

    if (pgdata[page].count)
	PANIC("reusing page with count %x\n", pgdata[page].count);

    pg_list_delete(page);
}


/*
 * This function gets called from sys_exec() when the inode of the  binary
 * image about to be started has been modified *AFTER* the last  time  the
 * free list page has been cleared (after the latest call to this function
 * (obviously, the list is considered cleared at boot time)).
 *
 * The purpose of this function is to prevent strange interactions between
 * the two cache mechanisms active in the system: the page cache  and  the
 * buffer  cache.  When  a  page  is  inserted  into  the  free  list, the
 * connection between it and its swap block or file system block  (if any)
 * is not broken, in the idea of being able to find the page very  quickly
 * if it is requested again before being reallocated.  If  we  broke  that
 * connection (that is, deleting the page from the hash table and reseting
 * the 'vmdev'  and  'block' fields in its  pgdata_t  structure),  if some
 * process needs this page, it will try to find it using its  'vmdev'  and
 * 'block', but we  won't  be  able to find it, even though the page might
 * still contain the information in that swap or file system block.  There
 * is another situation when the page - swap/fs block connection might  be
 * useful: suppose a program exits and then it is started  again.  If  the
 * amount of time elapsed is short or those pages  were  not  required  by
 * some other program, we will be able to find them into the free list and
 * never need to actually load them from the disk, not even searching them
 * into the buffer cache.
 *
 * However, this leads to a strange situation.  Suppose we start a program
 * called 'A'.  It loads its pages  as  needed,  using  the  demand paging
 * mechanism provided by the kernel, runs and then exits.  Its  pages  are
 * now in both the page and buffer cache ('A''s pages can appear  into the
 * page free list even while 'A' is still running, as a result of  a  data
 * or stack segment shrink or swapper actions).  If someone  modifies  the
 * executable binary image (e.g. as  a  result  of  compiling  a  new  'A'
 * version), there is a possibility for the new binary image to  use  some
 * or all the file system blocks used by the previous version.  If 'A'  is
 * restarted, the kernel will find pages with the same 'vmdev' and 'block'
 * numbers in the  page free list  (the cache)  and  load  them, which  is
 * definitely wrong because these pages actually belong to the old binary.
 * In order to get arround the problem, the kernel resets the  page  cache
 * each time a binary with a create time newer than the last time the page
 * cache was cleared is started.  Since we can't trust the inode's 'mtime'
 * field (users can modify it by means of the 'utimes' system call) I have
 * added a new inode field, 'ktime', used to keep track of the file create
 * time.  Maybe a better solution would be to simply reset the page  cache
 * each time a new binary is started, because the 'ktime' trick will  only
 * work with TFS file systems.
 */

void
clear_free_list(void)
{
    pgdata_t *pgd;

    DEBUG(1, "Clearing the free list...\x7\x7\n");

    for (pgd = &pgdata[pgdata[0].lnext];
	 pgd != pgdata;
	 pgd = &pgdata[pgd->lnext])
	if (pgd->block)
	{
	    pg_hash_delete(pgd - pgdata);
	    pgd->block = 0;
	}

    free_list_clear_time = seconds;
}


/*
 * Goes to sleep until at least 'pages' free pages are available.
 */

void
reserve_pages(int pages)
{
    while (free_pages < pages)
    {
	if (!swapper_running)
	    wakeup(&swapper);

	sleep(&get_page, WAIT_PAGE);
    }
}


/*
 * Check if a given 'block' is currently being loaded from the 'vmdev'
 * device.  This function should be a 'rep scasl'.
 */

int
block_is_loading(int vmdev, int block)
{
    int kpid;

    for (kpid = INIT; kpid; kpid = NEXT(kpid))
	if (status(kpid) != PROCESS_ZOMBIE  &&
	    u_area[kpid].busyblock.vmdev == vmdev &&
	    u_area[kpid].busyblock.block == block)
	    return kpid;

    return 0;
}


/*
 * Allocate data structures in the kernel reserved VM space.  This is used
 * to allocate data for the life time of the system - no free procedure is
 * provided.  The `map' parameter specifies whether we should 
 */

void *
vm_static_alloc(int size, int max_size, int alignment)
{
    char *pointer, *address;
    int mod = kernel_static_data % alignment;

    if (mod)
	kernel_static_data += alignment - mod;

    /* This is where we are going to allocate the required data.  */
    address = (char *)kernel_static_data;

    /* Map all the pages required to cover `size'.  If size == 0, just
       reserve the required space (max_size).  */
    for (pointer = address; size > 0; size -= PAGE_SIZE)
    {
	if (!is_mapped(pointer))
	    map_page(pointer, get_page() << PAGE_SHIFT);

	pointer += (size >= PAGE_SIZE) ? PAGE_SIZE : size;
    }

    /* Reserve max_size bytes.  */
    kernel_static_data += max_size;

    return address;
}


/*
 * This function initialize the u_area fields needed in order to be able
 * to run the kernel processes.  Right now it only includes the IDLE and
 * SWAPPER processes.
 */

void
u_area_init(void)
{
    u_area[IDLE].pgdir          = (pgtbl_t **)kernel_page_dir;
    u_area[IDLE].ctty           = makedev(VTTY_MAJOR, 0);

    strcpy(u_area[IDLE].args, "idle");

    u_area[SWAPPER].istack      = get_page() << PAGE_SHIFT;
    u_area[SWAPPER].status      = PROCESS_RUNNING;
    u_area[SWAPPER].esp0        = u_area[SWAPPER].istack + PAGE_SIZE - 4;
    u_area[SWAPPER].pgdir       = (pgtbl_t **)kernel_page_dir;
    u_area[SWAPPER].eip         = (unsigned)swapper;
    u_area[SWAPPER].eflags      = 512;
    u_area[SWAPPER].esp         = u_area[SWAPPER].esp0;
    u_area[SWAPPER].es          = 0x10;
    u_area[SWAPPER].cs          = 0x08;
    u_area[SWAPPER].ss          = 0x10;
    u_area[SWAPPER].ds          = 0x10;
    u_area[SWAPPER].fs          = 0x10;
    u_area[SWAPPER].gs          = 0x10;
    u_area[SWAPPER].cwd_inode   = 1;
    u_area[SWAPPER].ctty        = makedev(VTTY_MAJOR, 0);
    strcpy(u_area[SWAPPER].args, "swapper");

    u_area[INIT].ctty           = makedev(VTTY_MAJOR, 0);
}


/*
 * Clear the kernel bss.
 */

void
bss_init(void)
{
    memset((void *)(KERNEL_TEXT_SIZE + KERNEL_DATA_SIZE), 0, KERNEL_BSS_SIZE);
}


/*
 * Right now, init_vm() assumes that we can always find memory between
 * 640 Kb and 1 Mb.  Unfortunately, this is not always  the  case.  We
 * should check for such brain-damaged PCs one day...
 */

void
vm_init(void)
{
    unsigned pgdata_pointer;
    int pgdata_size, pgdata_pages, pgdata_pages_cnt;
    int i, kernel_size, kernel_pages;

    memory_pages = ((EXTENDED_MEMORY_SIZE & 0xFFFF) + 1024) >> 2;

    MAX_PAGES    = memory_pages;

    kernel_size  = KERNEL_TEXT_SIZE +
		   KERNEL_DATA_SIZE +
		   KERNEL_BSS_SIZE;

    kernel_pages = (kernel_size >> PAGE_SHIFT) +
		   ((kernel_size & (PAGE_SIZE - 1)) ? 1 : 0);

    /* Fill u_area[] with zeros.  */
    memset(u_area, 0, sizeof(u_area));

    /* A  small trick  is used here:  the first page tables of any process
       address  space  maps to  the  physical  computer  memory.  This  is
       probably a trick that can be used on any processor with VM support,
       so it is not very ugly.  And  has  many  advantages :-).  When  the
       kernel writes / reads a physical memory page it doesn't have to map
       that page into the virtual  address  space  because  that  page  is
       already there, at the same address.  So we don't have to invalidate
       the processor page cache at every such access.  */

    /* Compute the number of page tables required.  */
    KERNEL_PAGE_TABLES = (memory_pages/1024) + ((memory_pages%1024) ? 1 : 0);

    /* In fact, we can compute this a little bit better.  If the
       amount of free memory is not multiple of 4 Mb, the last page
       table will contain some free entries.  We can start allocating
       from there, but it won't make any difference, so why bother?  */
    kernel_static_data = (unsigned)KERNEL_PAGE_TABLES * 0x400000;

    /* The kernel code/data/bss should fit in the first 640Kb - 4Kb.
       There is not enough space under 640Kb to allocate all the data
       structures required by the kernel.  For example, we might need
       a lot of space to allocate the pgdata (a vector that contains a
       control structure for each page of memory in the system).  If
       the system has 256Mb of memory, pgdata will be of about 1.5Mb.
       Therefore, I think it is best to reserve about 16Mb.  With the
       current technology is reasonable to assume that a PC will not
       have more than 2Gb of physical memory.  */
    KERNEL_PAGE_TABLES += 4;
    KERNEL_RESERVED_SPACE = KERNEL_PAGE_TABLES * 0x400000;

    /* The kernel page directory is allocated at 1Mb.  Fill it with
       zeros.  */
    kernel_page_dir = (unsigned *)0x100000;
    memset(kernel_page_dir, 0, PAGE_SIZE);

    /* The kernel page table(s) are allocated at 1Mb + 4Kb.  Fill them
       with zeros.  */
    kernel_page_table = (unsigned *)0x101000;
    memset(kernel_page_table, 0, KERNEL_PAGE_TABLES * PAGE_SIZE);

    /* Set up the kernel page tables into the page directory.  */
    for (i = 0; i < KERNEL_PAGE_TABLES; i++)
	kernel_page_dir[i] = (unsigned)&kernel_page_table[i * 1024] | PAGE_SWP;

    /* Set up the physical pages entries into the kernel_page_table.
       Page 0 contains the BIOS data area.  */
    for (i = 0; i < memory_pages; i++)
	kernel_page_table[i] = (i << PAGE_SHIFT) | PAGE_SWP;

    /* Allocate pgdata.  We have to do it by hand as we cannot call
       vm_static_alloc().  */
    pgdata = (pgdata_t *)kernel_static_data;
    pgdata_size = (MAX_PAGES + PG_BUCKETS_NO) * sizeof(pgdata_t);
    pgdata_pages = (pgdata_size >> PAGE_SHIFT) +
		   ((pgdata_size & (PAGE_SIZE - 1)) ? 1 : 0);

    i = 0x101 + KERNEL_PAGE_TABLES;
    pgdata_pages_cnt = pgdata_pages;

    for (pgdata_pointer = (unsigned)pgdata;
	 pgdata_pages_cnt;
	 pgdata_pages_cnt--)
    {
	map_page(pgdata_pointer, i++ << PAGE_SHIFT);
	pgdata_pointer += PAGE_SIZE;
    }

    /* Update kernel_static_data to reflect the changes.  */
    kernel_static_data += pgdata_size;

    /* Initialize i386 paging mechanism.  */
    paging_init();

    /* Inititalize pgdata structures.  */
    pg_init();

    for (i =  0x101 + KERNEL_PAGE_TABLES + pgdata_pages; i < memory_pages; i++)
    {
	pgdata[i].count = 1;
	release_page(i);
    }

    /* Add to the free pool all the pages between 'kernel_pages' and
       the floppy data transfer area (0x9A-0x9E).  The kernel startup
       stack also starts there (0x9F).

       Note: The floppy driver data transfer area is hard coded here
       because the DMA needs it between 64 Kb boundaries and under 16
       Mb.  Unfortunately 18 Kb is not a 4 Kb multiple so we are not
       using those 2 Kb at the end of it.  (0x9A000-0x9A800).  */

    for (i = kernel_pages; i < 0x9A; i++)
    {
	pgdata[i].count = 1;
	release_page(i);
    }

    /* Initialize i386 descriptor tables, processes internal stacks and
       u_area[].  */
    memset(ldt, 0, sizeof(ldt));

    for (i = 0; i < SYSTEM_PROCESS_MAX; i++)
    {
	ldt_descr[i].low_limit   = DESCRIPTORS_PER_LDT * sizeof(descriptor_t);
	ldt_descr[i].low_address = (short)(unsigned)(ldt + i);
	ldt_descr[i].med_address = (char)((int)(ldt + i) >> 16);
	ldt_descr[i].low_flags   = LDT_ARB;
	ldt_descr[i].high_flags  = 4;  /* 32 bits data, byte aligned limit.  */
				 
	tss_descr[i].low_limit   = sizeof(tss_t) - 1;
	tss_descr[i].low_address = (short)(unsigned)(u_area + i);
	tss_descr[i].med_address = (char)((int)(u_area + i) >> 16);
	tss_descr[i].high_flags  = 4;  /* 32 bits data, byte aligned limit.  */

	internal_stack_descr[i].low_limit  = 0xFFFF;
	internal_stack_descr[i].low_flags  = DATA_ARB; /* Good enough.  */
	internal_stack_descr[i].high_limit = 0xF;
	internal_stack_descr[i].high_flags = 0xC; /* 32 bits data,
						     page aligned limit.  */

	ldt[i][1].low_flags    = CODE_ARB | 0x60;       /* DPL = 3 */
	ldt[i][1].high_flags   = 0xC;   /* 32 bits data, page aligned limit. */
	ldt[i][1].low_address  = KERNEL_RESERVED_SPACE % 0x10000;
	ldt[i][1].med_address  = KERNEL_RESERVED_SPACE / 0x10000;
	ldt[i][1].high_address = KERNEL_RESERVED_SPACE / 0x1000000;

	ldt[i][2].low_flags    = DATA_ARB | 0x60;       /* DPL = 3 */
	ldt[i][2].high_flags   = 0xC;   /* 32 bits data, page aligned limit. */
	ldt[i][2].low_address  = KERNEL_RESERVED_SPACE % 0x10000;
	ldt[i][2].med_address  = KERNEL_RESERVED_SPACE / 0x10000;
	ldt[i][2].high_address = KERNEL_RESERVED_SPACE / 0x1000000;

	u_area[i].backlink = 0;
	u_area[i].ss0      = (SYSTEM_PROCESS_MAX * 3 + i) << 3;
	u_area[i].ss1      = 0;
	u_area[i].ss2      = 0;
	u_area[i].esp1     = 0;
	u_area[i].esp2     = 0;
	u_area[i].ldt      = (SYSTEM_PROCESS_MAX + i) << 3;
	u_area[i].io_bitmap_offset = 0x8000;    /* After u_area limit.  */

	u_area[i].status   = PROCESS_UNUSED;
    }

    /* TR (task register) can't be loaded without this.  */
    tss_descr[IDLE].low_flags = VALID_TSS_ARB;

    ldt0tr_init();	/* Initialize the first LDT entry and the task reg.  */
    vmdev_init();	/* Initialize the VM device numbers handling.  */
    swap_init();	/* Initialize the swapping data structures.  */
    vm_alloc_init();	/* Initialize the sub-allocator.  */
    u_area_init();	/* Initialize the u_area[] array.  */


    printk("VM: total system memory: %dk\n",
	   (memory_pages << PAGE_SHIFT) / 1024);

    printk("VM: kernel text/data/bss size: %dk/%dk/%dk\n",
	   KERNEL_TEXT_SIZE / 1024,
	   KERNEL_DATA_SIZE / 1024,
	   KERNEL_BSS_SIZE  / 1024);

    printk("VM: free system memory: %dk\n",
	   free_pages * PAGE_SIZE / 1024);
}


void
page_protection_fault(vmaddr_t address)
{
/*
    int i, j; unsigned *sp;
*/
    pgdata_t *pgd;
    pgtbl_t *pgtbl, **pgdir;
    unsigned new_page, old_page_addr, new_page_addr;

    if (*(unsigned *)&address - KERNEL_RESERVED_SPACE <
	((1 + this->text_pages) << PAGE_SHIFT))
    {
	DEBUG(0, "trying to write to a text page!\n", current_kpid);
	DEBUG(0, "offending address: %x.\n", address);
	kill_kpid(current_kpid, SIGSEGV);
	return;
    }
    else
    {
	pgdir = this->pgdir + address.pgdir_index;
	pgtbl = (pgtbl_t *)((unsigned)*pgdir & 0xFFFFF000)+address.pgtbl_index;
	pgd   = &pgdata[pgtbl->address];

	if (pgd->count == 1)
	{
	  not_shared:
	    pgtbl->rw = 1;

	    if (pgd->block)
	    {
		pg_hash_delete(pgd - pgdata);
		pgd->block = 0;
	    }

	    pgd->type = PAGE_SWAP;

	    invalidate_cache();

	    return;
	}

	pgtbl->busy = 1;
	invalidate_cache();
	reserve_pages(1);

	if (pgd->count == 1)
	{
	    pgtbl->busy = 0;
	    goto not_shared;
	}

	if (pgd->count == 0)
	    PANIC("invalid pgd count 0\n");

	pgd->count--;
	old_page_addr = (pgd - pgdata) << PAGE_SHIFT;

	new_page_addr = (new_page = get_page()) << PAGE_SHIFT;
	*(unsigned *)pgtbl = new_page_addr | PAGE_UWP;

	pgdata[new_page].type  = PAGE_SWAP;
	pgdata[new_page].block = 0;

	memcpy((void *)new_page_addr, (void *)old_page_addr, PAGE_SIZE);
	invalidate_cache();
    }
}


/*
 * This is the page not present fault handler.  It handles both  page  and
 * page tables faults and returns 1 in the first case and 0 in the second.
 */

int
page_not_present_fault(vmaddr_t address)
{
    int page;
    pgdata_t *pgd;
    int kpid, page_is_missing;
    unsigned pgtbl_addr;
    pgtbl_t *pgtbl, **pgdir;
    int vmdev, block, type;
    unsigned new_page, new_page_addr;


    pgdir = this->pgdir + address.pgdir_index;

    if ((page_is_missing = ((pgtbl_t *)pgdir)->present))
    {
	/* The page is missing.  */

	pgtbl_addr = (unsigned)*pgdir & 0xFFFFF000;

      restart:
	pgtbl = (pgtbl_t *)pgtbl_addr + address.pgtbl_index;

	/* Find out the page type and location (if any).  */
	vmdev = ((extpgtbl_t *)pgtbl)->vmdev;
	block = ((extpgtbl_t *)pgtbl)->block;
	type  = ((extpgtbl_t *)pgtbl)->type;

	DEBUG(5, "a/m/b/t=%x/%d/%x/%d\n", address, vmdev, block, type);
	DEBUG(5, "args=%s\n", this->args);

	if (block)
	    for(;;)
		if ((page = pg_hash_search(vmdev, block)))
		{
		    /* A page with the given vmdev/block numbers has been
		       found in the hash table.  If no other  process  is
		       using it, we have to get it  back  from  the  free
		       pool.  */
		    pgd = &pgdata[page];

		    if (pgd->lnext != 0xFFFF)
			reuse_page(pgd - pgdata);

		    pgd->count++;

		    if (type == PAGE_SWAP)
			release_swap_block(vmdev_aux(vmdev), block);

		    /* Mark the page read-only.  We might want to share it.  */
		    *(unsigned *)pgtbl = ((pgd - pgdata) << PAGE_SHIFT) |
					 PAGE_URP;

		    /* Statistics. */
		    this->usage.ru_minflt++;

		    invalidate_cache();
		    return page_is_missing;
		}
		else
		    if ((kpid = block_is_loading(vmdev, block)))
		    {
			sleep(&u_area[kpid].busyblock, WAIT_PAGE);
			/* There is a possibility that the page was swapped  in
			   by some other process but the swapper swapped it out
			   *before* we had a chance  to  wake up.  Restart  the
			   fault handler from the point where it checks out the
			   page table entry. */
			goto restart;
		    }
		    else
		    {
			/* Notify other instances of the same process that
			   we are going to load this block.  */
			this->busyblock.vmdev = vmdev;
			this->busyblock.block = block;
			break;
		    }

	pgtbl->busy = 1;
	invalidate_cache();
	reserve_pages(1);

	/* Get a new page from the free pool.  */
	new_page_addr = (new_page = get_page()) << PAGE_SHIFT;
	*(unsigned *)pgtbl = new_page_addr | PAGE_URP;
	pgtbl->busy = 1;

	invalidate_cache();

	/* Get and initialize the pgdata_t structure.  */
	pgd = &pgdata[new_page];
	pgd->vmdev = vmdev;
	pgd->block = block;
	pgd->type  = type;

	switch (type)
	{
	    case PAGE_DZ:

		lmemset((void *)new_page_addr, 0, PAGE_SIZE >> 2);

		/* Statistics. */
		/* This doesn't seem to fit neither in the hard  page
		   faults nor in the soft page faults as described by
		   the GNU resource.h.  Right now, I'm considering it
		   a soft one. */
		this->usage.ru_minflt++;

		break;

	    case PAGE_FILE:

		DEBUG(6, "reading from fs (block=%w) -> %x\n",
		      block, new_page_addr);

		/* Load the page from the binary image.  */
		a_out_read(this->a_out_inode, (char *)new_page_addr,
			   ((*(unsigned *)&address - KERNEL_RESERVED_SPACE
						   - PAGE_SIZE) &
			   ~(PAGE_SIZE - 1)),
			   PAGE_SIZE);

		/* Statistics. */
		this->usage.ru_majflt++;
		goto common;

	    case PAGE_SWAP:

		/* Load the page from disk.  */
		load_page(pgd);

	      common:

		/* Insert the page into the hash table.  */
		pg_hash_insert(new_page);

		/* If the page have a swap block, release it, we no longer
		   need it.  */
		if (type == PAGE_SWAP)
		    release_swap_block(vmdev_aux(vmdev), block);

		/* Wake up all the processes waiting for this block to be
		   loaded.  */
		this->busyblock.vmdev = 0;
		this->busyblock.block = 0;
		wakeup(&this->busyblock);
/*
		work_set_insert(current_kpid,
				*(unsigned *)&address >> PAGE_SHIFT);
*/
		break;

	    default:
		PANIC("invalid extended page table entry %x!", type);
		break;
	}

	pgtbl->rw   = 0;
	pgtbl->busy = 0;
	invalidate_cache();
    }
    else
    {
	/* The page table is missing.  Allocate & initialize one.  */

	DEBUG(5, "%x\n", address);

	reserve_pages(1);

	new_page_addr = get_page() << PAGE_SHIFT;
	*(unsigned *)pgdir = new_page_addr | PAGE_UWP;
	lmemset((void *)new_page_addr, 0, PAGE_SIZE >> 2);

	invalidate_cache();
    }

    return page_is_missing;
}


/*
 * This is the i386 page fault handler.
 */

void
page_fault(unsigned cr2, unsigned   es, unsigned   ds, unsigned edi,
	   unsigned esi, unsigned _ebp, unsigned _esp, unsigned ebx,
	   unsigned edx, unsigned  ecx, unsigned  eax, unsigned ebp,
	   unsigned err, unsigned  eip, unsigned   cs, unsigned eflags,
	   unsigned esp, unsigned   ss)
{
    vmaddr_t address;

    int protection_violation    = err & 1;
    int page_fault_in_user_mode = err & 4;

    int page_was_missing = 0;

    unsigned BRK = this->brk;
    unsigned ESP = (page_fault_in_user_mode ? esp :
		    user_mode_ESP[current_kpid]) - 4;

    int BRK_page = BRK >> PAGE_SHIFT;
    int ESP_page = ESP >> PAGE_SHIFT;

    /* Keep gcc happy :-)  */
    *(unsigned *)&address = cr2;

    /* I hope this works :-)  */
    if ((i386VM_PAGES - (KERNEL_RESERVED_SPACE >> PAGE_SHIFT) - ESP_page + 1 >
	 this->max_stack) ||
	((BRK + KERNEL_RESERVED_SPACE <= cr2) &&
	 (cr2 < KERNEL_RESERVED_SPACE + ESP)) ||
	(cr2  < KERNEL_RESERVED_SPACE + PAGE_SIZE) ||
	(ESP_page - BRK_page < SAFE_PAGES))
    {
	display_exception_header("page fault");

	printk("%d-%d-%d-%d\n",
	       (i386VM_PAGES - (KERNEL_RESERVED_SPACE >> PAGE_SHIFT) -
		ESP_page + 1 > this->max_stack),
	       ((BRK + KERNEL_RESERVED_SPACE <= cr2) &&
		(cr2 < KERNEL_RESERVED_SPACE + ESP)),
	       (cr2  < KERNEL_RESERVED_SPACE + PAGE_SIZE),
	       (ESP_page - BRK_page < SAFE_PAGES));

	printk("BRK=%x ESP=%x\n", BRK, ESP);

	display_registers(cs, ds, es, ss,
			  eax, ebx, ecx, edx, esi, edi, ebp, esp, _esp,
			  eip, eflags);
	printk("cr2=%x\tecode=%w\n", cr2, err);

	printk("processes=%d,free_pages=%d,free_swap_blocks=%d,"
	       "page_table=%d,page=%d",
	       processes, free_pages, total_free_swap_blocks,
	       cr2 >> 22, (cr2 >> PAGE_SHIFT) & 0x3FF);

	if (this->pid > 1)
	    kill_kpid(current_kpid, SIGSEGV);
	else
	    PANIC("Stop.\n");

	return;
    }

    /* Release unused stack memory.  */

    if (this->stk < ESP)
	release_memory(this->stk, ESP - 1, MEM_STACK);

    this->stk = ESP;

    if (protection_violation)
	page_protection_fault(address);
    else
	page_was_missing = page_not_present_fault(address);

#ifdef __i386k__
    /* Intel 80386 is stupid enough not to generate a page protection fault
       when trying to write to a read-only page from kernel mode.  We  need
       to get arround this, so, if there was a page (not page table) fault,
       we are calling the page protection fault handler "by hand" in  order
       to simulate a write attempt to that page.  Basically  this  is  what
       this->kwreq does:  it is simply set on each system call  that  might
       try to write into user memory so that if the corresponding  page  is
       missing and a page fault occurs, it will handle both the page  fault
       and the yet to come page protection fault.  The case where the  page
       is present is handled in the ok_to_write_area() function.  */

    if (page_was_missing && this->kwreq)
	page_protection_fault(address);
#endif
}


/*
 * Detach a memory page from the virtual address space.  Release its swap
 * block, if any.
 */

int
delete_page(pgtbl_t *pgtbl, int page)
{
    pgdata_t *pgd;
    extpgtbl_t *extpgtbl = (extpgtbl_t *)pgtbl;

    if (pgtbl[page].present)
    {
	pgd = &pgdata[pgtbl[page].address];
	((unsigned *)pgtbl)[page] = 0;
	invalidate_cache();
	return release_page(pgd - pgdata);
    }
    else
	if (extpgtbl[page].type == PAGE_SWAP && extpgtbl[page].block)
	    release_swap_block(vmdev_aux(extpgtbl[page].vmdev),
			       extpgtbl[page].block);

    ((unsigned *)pgtbl)[page] = 0;
    invalidate_cache();

    return 0;
}


/*
 * Release memory from the current process address space.  'type' can be
 * one of MEM_DATA and MEM_STACK.  All the pages between 'first_address'
 * and 'last_address' are returned to the free pool.
 */

void
release_memory(unsigned first_address, unsigned last_address, int type)
{
    pgtbl_t *pgtbl, **pgdir;
    int do_wakeup, previous_swapable_status;
    int first_page_table, last_page_table, page_table, limit;
    int first_address_page, last_address_page, page, start_page;


    /* Check if we can *really* release some memory.  */

    first_address_page = (first_address + KERNEL_RESERVED_SPACE) >> PAGE_SHIFT;
    last_address_page  = ( last_address + KERNEL_RESERVED_SPACE) >> PAGE_SHIFT;

    if (type == MEM_DATA)
    {
	if (first_address & 0xFFF)
	    if (first_address_page == last_address_page)
		return;
	    else
		first_address_page++;
    }
    else
    {
	if ((last_address & 0xFFF) != 0xFFF)
	    if (first_address_page == last_address_page)
		return;
	    else
		last_address_page--;
    }

    /* Ok, it seems we can. Start by marking the current process 'unswapable'
       in order to prevent races. We will restore the  current  status  after
       we finish the job.

       Note:  I don't think we really need this save/restore stuff because
       this function is supposed to run with interrupts disabled.  Anyway,
       it's harmless.  */

    previous_swapable_status = this->swapable;

    this->swapable = 0;


    do_wakeup        = 0;
    pgdir            = this->pgdir;
    page             = first_address_page & 0x3FF;
    first_page_table = first_address_page >> 10;
    last_page_table  = last_address_page  >> 10;

    if (pgdir[first_page_table])
    {
	pgtbl = (pgtbl_t *)((unsigned)pgdir[first_page_table] & 0xFFFFF000);

	limit = (first_page_table == last_page_table) ?
		(last_address_page & 0x3FF) + 1 : 0x400;

	for (start_page = page; page < limit; page++)
	    if (delete_page(pgtbl, page))
		do_wakeup = 1;

	if ((type == MEM_STACK && limit      == 0x400) ||
	    (type == MEM_DATA  && start_page == 0x000))
	{
	    release_page((unsigned)pgdir[first_page_table] >> PAGE_SHIFT);

	    pgdir[first_page_table] = NULL;
	    invalidate_cache();
	    do_wakeup = 1;
	}

	if (first_page_table == last_page_table)
	{
	    if (do_wakeup)
		wakeup(&get_page);
	    goto done;
	}

	first_page_table++;
    }

    for (page_table = first_page_table;
	 page_table < last_page_table;
	 page_table++)
    {
	if (pgdir[page_table] == 0)
	    continue;

	pgtbl = (pgtbl_t *)((unsigned)pgdir[page_table] & 0xFFFFF000);

	for (page = 0; page < 0x400; page++)
	    if (delete_page(pgtbl, page))
		do_wakeup = 1;

	release_page((unsigned)pgdir[page_table] >> PAGE_SHIFT);

	pgdir[page_table] = NULL;
	invalidate_cache();
	do_wakeup = 1;
    }

    if (pgdir[last_page_table])
    {
	pgtbl = (pgtbl_t *)((unsigned)pgdir[last_page_table] & 0xFFFFF000);

	limit = (last_address_page & 0x3FF) + 1;

	for (page = 0; page < limit; page++)
	    if (delete_page(pgtbl, page))
		do_wakeup = 1;

	if ((type == MEM_STACK && limit == 0x400) || type == MEM_DATA)
	{
	    release_page((unsigned)pgdir[last_page_table] >> PAGE_SHIFT);
	    pgdir[last_page_table] = NULL;
	    invalidate_cache();
	    do_wakeup = 1;
	}
    }

    if (do_wakeup)
	wakeup(&get_page);

  done:

    /* Restore the 'swapable' flag. */
    this->swapable = previous_swapable_status;
}


int
sys_brk(unsigned BRK, unsigned ESP)
{
    int BRK_page = BRK >> PAGE_SHIFT;
    int ESP_page = ESP >> PAGE_SHIFT;

    DEBUG(5, "(%x)\n", BRK);

    if (BRK == 0)
	return this->brk;

    if ((ESP_page - BRK_page >= SAFE_PAGES) &&
	(BRK_page <= this->max_brk) &&
	(BRK_page >= 1 + this->text_pages))
    {
	if (BRK < this->brk)
	    release_memory(BRK + 1, this->brk, MEM_DATA);

	this->brk = BRK;
	return 0;
    }

    return -EINVAL;
}


/*
 * Search the end of a string in the first 'bytes_to_search' bytes.
 */

int
string_end_found(char *pointer, unsigned bytes_to_search)
{
    for (; bytes_to_search; bytes_to_search -= sizeof(char))
	if (*pointer++ == 0)
	    return 1;

    return 0;
}


/*
 * Search the end of a list in the next 'bytes_to_search' bytes.
 */

int
pointer_list_end_found(char **pointer, unsigned bytes_to_search)
{
    for (; bytes_to_search >= sizeof(char*); bytes_to_search -= sizeof(char*))
	if (*pointer++ == NULL)
	    return 1;

    return 0;
}


/*
 * Return 1 if the the area between 'pointer' and 'pointer + size' can
 * be read ('pointer' is a valid memory reference).  Otherwise  return
 * 0.
 */

int
ok_to_read_area(void *pointer, unsigned size)
{
    unsigned end = (unsigned)pointer + size;
    unsigned brk = this->brk + KERNEL_RESERVED_SPACE;
    unsigned stk = user_mode_ESP[current_kpid] + KERNEL_RESERVED_SPACE;

    if ((unsigned)pointer < KERNEL_RESERVED_SPACE)
	return 0;

    if ((unsigned)pointer >= stk && end >= stk && (unsigned)pointer <= end)
	return 1;

    if ((unsigned)pointer <= brk && end <= brk && (unsigned)pointer <= end)
	return 1;

    return 0;
}


/*
 * Return 1 if the the area between 'pointer'  and  'pointer + size'  can
 * be written ('pointer' is a valid memory reference).  Otherwise  return
 * 0.  A process is not allowed to overwrite its text segment.
 */

int
ok_to_write_area(void *pointer, unsigned size)
{
    unsigned end = (unsigned)pointer + size;
    unsigned brk = this->brk + KERNEL_RESERVED_SPACE;
    unsigned stk = user_mode_ESP[current_kpid] + KERNEL_RESERVED_SPACE;

    if ((unsigned)pointer < KERNEL_RESERVED_SPACE +
			    ((1 + this->text_pages) << PAGE_SHIFT))
	return 0;

    if (((unsigned)pointer >= stk && end >= stk && (unsigned)pointer <= end) ||
	((unsigned)pointer <= brk && end <= brk && (unsigned)pointer <= end))
    {
#ifdef __i386k__
	/* Check for write protected pages.  This is due to a 386 flaw
	   that prevents the system from receiving page protection
	   faults when writing to read/only pages from kernel mode.  */
	unsigned first_address = ((unsigned)pointer)        & ~0xFFF;
	unsigned last_address  = ((unsigned)pointer + size) & ~0xFFF;
	unsigned address       = first_address;

	for (; address <= last_address; address += PAGE_SIZE)
	{
	    vmaddr_t vmaddr;
	    pgtbl_t **pgdir, *pgtbl;

	    *(unsigned *)&vmaddr = address;
	    pgdir = this->pgdir + vmaddr.pgdir_index;
	    pgtbl = (pgtbl_t *)((unsigned) * pgdir&0xFFFFF000) +
		    vmaddr.pgtbl_index;

	    if (pgtbl->present && !pgtbl->rw)
		page_protection_fault(vmaddr);
	}
#endif /* __i386k__ */
	return 1;
    }

    return 0;
}


/*
 * Check if a the kernel can read a string from 'pointer'.  It searches for
 * the string end first.
 */

int
ok_to_read_string(char *pointer)
{
    unsigned bytes_to_search;
    unsigned brk = this->brk + KERNEL_RESERVED_SPACE;
    unsigned stk = user_mode_ESP[current_kpid] + KERNEL_RESERVED_SPACE;

    if ((unsigned)pointer < KERNEL_RESERVED_SPACE)
	return 0;

    if ((unsigned)pointer <= brk)
	bytes_to_search = brk - (unsigned)pointer + 1;
    else
	if ((unsigned)pointer >= stk)
	    bytes_to_search = (0xFFFFFFFF - (unsigned)pointer) + 1 + 1;
	else
	    return 0;

    if (bytes_to_search > ARG_MAX)
	bytes_to_search = ARG_MAX;

    return string_end_found(pointer, bytes_to_search);
}


/*
 * Check if the kernel can read a list of pointers.  The NULL pointer
 * should mark the end of the list.
 */

int
ok_to_read_pointer_list(char **pointer)
{
    unsigned bytes_to_search;
    unsigned brk = this->brk + KERNEL_RESERVED_SPACE;
    unsigned stk = user_mode_ESP[current_kpid] + KERNEL_RESERVED_SPACE;

    if ((unsigned)pointer < KERNEL_RESERVED_SPACE)
	return 0;

    if ((unsigned)pointer <= brk)
	bytes_to_search = brk - (unsigned)pointer + 1;
    else
	if ((unsigned)pointer >= stk)
	    bytes_to_search = (0xFFFFFFFF - (unsigned)pointer) + 1 + 1;
	else
	    return 0;

    if (bytes_to_search > ARG_MAX)
	bytes_to_search = ARG_MAX;

    return pointer_list_end_found(pointer, bytes_to_search);
}
