/* buffer.c -- Buffer cache management routines.  */

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
#include <thix/sleep.h>
#include <thix/sched.h>
#include <thix/buffer.h>
#include <thix/gendrv.h>
#include <thix/blkdrv.h>
#include <thix/vmalloc.h>
#include <thix/semaphore.h>
#include <thix/stat.h>
#include <thix/limits.h>


#define BUF_DEDICATED_PAGES             16
#define FLUSH_ALL_BUFFERS       0x7FFFFFFF


buffer_t *buf_vect;


volatile int free_buffers = 0;
volatile int buf_free_pages = 0;
volatile int master_free_buffers = 0;

pgdata_t *buf_pghead;
pgdata_t *buf_pgtail;


/*
 * Insert a page into the buffer cache dedicated page list.
 */

void
bufpg_list_insert(int page)
{
    pgdata[page].lprev = pgdata[1].lprev;
    pgdata[page].lnext = 1;
    pgdata[pgdata[1].lprev].lnext = page;
    pgdata[1].lprev = page;
    buf_free_pages++;
}


/*
 * Delete a page from the buffer cache dedicated page list.
 */

void
bufpg_list_delete(int page)
{
    pgdata[pgdata[page].lprev].lnext = pgdata[page].lnext;
    pgdata[pgdata[page].lnext].lprev = pgdata[page].lprev;
    buf_free_pages--;
}


/*
 * Get a page from the buffer cache dedicated page list.
 */

int
buf_get_page(void)
{
    int page;


    if (buf_free_pages == 0)
	return 0;

    page = pgdata[1].lnext;

    bufpg_list_delete(page);

    return page;
}


void
buf_release_page(int page)
{
    bufpg_list_insert(page);

#ifdef __PARANOIA__
    if (buf_free_pages > BUF_DEDICATED_PAGES)
	PANIC("Trying to release too many dedicated pages");
#endif
}


/*
 * Insert a buffer into the 'master list' of free buffers.
 */

inline void
buf_master_list_insert(int buf_no)
{
    buf_vect[buf_no].lprev = buf_vect[0].lprev;
    buf_vect[buf_no].lnext = 0;
    buf_vect[buf_vect[0].lprev].lnext = buf_no;
    buf_vect[0].lprev = buf_no;
    master_free_buffers++;
}


/*
 * Delete a buffer from the 'master list' of free buffers.
 */

inline void
buf_master_list_delete(int buf_no)
{
    buf_vect[buf_vect[buf_no].lprev].lnext = buf_vect[buf_no].lnext;
    buf_vect[buf_vect[buf_no].lnext].lprev = buf_vect[buf_no].lprev;
    master_free_buffers--;
}


/*
 * Insert a buffer into the head of the list of free buffers.  The  buffer
 * is inserted in the head of the list in order to be quickly reallocated.
 * This is usually done when the contents of that buffer is currupted, for
 * example as a result of a disk error.
 */

inline void
buf_list_insert_head(int buf_no)
{
    buf_vect[buf_no].lnext = buf_vect[1].lnext;
    buf_vect[buf_no].lprev = 1;
    buf_vect[buf_vect[1].lnext].lprev = buf_no;
    buf_vect[1].lnext = buf_no;
    free_buffers++;
}


/*
 * Insert a buffer into the tail of the list of free buffers.  Since the
 * buffer will be inserted in the tail of the list, its contents will be
 * available until reallocation.
 */

inline void
buf_list_insert_tail(int buf_no)
{
    buf_vect[buf_no].lprev = buf_vect[1].lprev;
    buf_vect[buf_no].lnext = 1;
    buf_vect[buf_vect[1].lprev].lnext = buf_no;
    buf_vect[1].lprev = buf_no;
    free_buffers++;
}

/*
 * Delete a buffer from the list of free buffers.
 */

inline void
buf_list_delete(int buf_no)
{
    buf_vect[buf_vect[buf_no].lprev].lnext = buf_vect[buf_no].lnext;
    buf_vect[buf_vect[buf_no].lnext].lprev = buf_vect[buf_no].lprev;
    free_buffers--;
}


/*
 * Insert a buffer into the buffer hash table.
 */

inline void
buf_hash_insert(int buf_no)
{
    int index = MAX_BUFFERS +
		(buf_vect[buf_no].device ^ buf_vect[buf_no].block) %
		BUFFERS_BUCKETS_NO;

    buf_vect[buf_no].hnext = buf_vect[index].hnext;
    buf_vect[buf_no].hprev = index;
    buf_vect[buf_vect[index].hnext].hprev = buf_no;
    buf_vect[index].hnext = buf_no;
}


/*
 * Delete a buffer from the buffer hash table.
 */

inline void
buf_hash_delete(int buf_no)
{
    if (buf_vect[buf_no].hnext == 0xFFFF)
	return;

    buf_vect[buf_vect[buf_no].hprev].hnext = buf_vect[buf_no].hnext;
    buf_vect[buf_vect[buf_no].hnext].hprev = buf_vect[buf_no].hprev;
    buf_vect[buf_no].hnext = 0xFFFF;
}


/*
 * Search a buffer into the buffer hash table.
 */

int
buf_hash_search(__dev_t device, int block)
{
    int buf_no, index = MAX_BUFFERS + (device ^ block) % BUFFERS_BUCKETS_NO;

    for (buf_no = buf_vect[index].hnext;
	 buf_no != index &&
	 (buf_vect[buf_no].device != device ||
	  buf_vect[buf_no].block  != block);
	 buf_no = buf_vect[buf_no].hnext);

    return buf_no == index ? 0 : buf_no;
}


/*
 * Update the buffer's memory pointer (.address).  If the buffer used to
 * have the same size, it remains  unchanged.  Otherwise, the  old  data
 * area is release and a new one (of different size) is allocated.
 */

void
buf_update_mem(int buf_no, size_t size)
{
    int buf_type;

    if (buf_address(buf_no))
    {
	if (size != buf_vect[buf_no].size)
	{
	    DEBUG(6, "buf_no=%x size=%x dedicated=%x block=%x device\n",
		  buf_no, buf_vect[buf_no].size, buf_vect[buf_no].dedicated,
		  buf_vect[buf_no].block, buf_vect[buf_no].device);

	    vm_free(buf_vect[buf_no].address, buf_release_page);
	    buf_vect[buf_no].address = (char *)vm_alloc(size, buf_get_page,
							&buf_type);
	    buf_vect[buf_no].dedicated = buf_type;
	    buf_vect[buf_no].size = size;
	}
    }
    else
    {
	buf_vect[buf_no].address = (char *)vm_alloc(size, buf_get_page,
						    &buf_type);
	buf_vect[buf_no].dedicated = buf_type;
	buf_vect[buf_no].size = size;
    }

    buf_vect[buf_no].valid = 0;

#ifdef __PARANOIA__
    if (buf_vect[buf_no].address == NULL)
	PANIC("can't allocate buffer\n");
#endif
}


/*
 * Update all the buffer structure.  Also insert it into the buffer hash
 * table.
 */

inline int
buf_update(int buf_no, __dev_t device, int block, size_t size)
{
    buf_vect[buf_no].device = device;
    buf_vect[buf_no].block  = block;
    buf_vect[buf_no].busy   = 1;
    buf_update_mem(buf_no, size);
    buf_hash_insert(buf_no);
    return buf_no;
}


/*
 * buf_remove() frees a buffer, making it available for  reallocation.  This
 * function is normally used when the kernel tries to free up the space used
 * by buffers or when synchronizing file systems.
 */

void
buf_remove(int buf_no)
{
    buf_list_delete(buf_no);
    buf_hash_delete(buf_no);

    DEBUG(6, "buf_no=%w size=%d dedicated=%d block=%w device=%d/%d\n",
	  buf_no, buf_vect[buf_no].size,
	  buf_vect[buf_no].dedicated,
	  buf_vect[buf_no].block,
	  major(buf_vect[buf_no].device),
	  minor(buf_vect[buf_no].device));

    vm_free(buf_vect[buf_no].address, buf_release_page);
    buf_vect[buf_no].address = NULL;
    buf_master_list_insert(buf_no);
    wakeup(&buf_get);
}


/*
 * buf_read() returns a *busy* buffer which contain the specified block from
 * the specified device.
 *
 * WARNING:
 * buf_read() might return 0 if the specified block could not be read due to
 * an I/O error.  Kernel functions that call this routine should  check  the
 * return value.
 */

int
buf_read(__dev_t device, int block, size_t size)
{
    blk_request br;
    int buf_no = buf_get(device, block, size);

    br.blksz   = size;
    br.block   = block;
    br.nblocks = 1;
    br.buf_no  = buf_no;
    br.next    = 0;

    if (!buf_vect[buf_no].valid)
	drv_read(device, &br);

    if (buf_vect[buf_no].valid)
	return buf_no;
    else
    {
	/* read error, getting rid of the buffer. */
	buf_vect[buf_no].delayed_write = 0;
	buf_release(buf_no);
	return 0;
    }
}


/*
 * _buf_read() does the same thing as buf_read(), the only difference is
 * that it receives an already allocated buffer as an argument.  This is
 * useful when we already have the buffer corresponding to a given  file
 * system block and all we want to do is read the block into it.
 */

int
_buf_read(int buf_no)
{
    blk_request br;

    br.blksz   = buf_vect[buf_no].size;
    br.block   = buf_vect[buf_no].block;
    br.nblocks = 1;
    br.buf_no  = buf_no;
    br.next    = 0;

    if (!buf_vect[buf_no].valid)
	drv_read(buf_vect[buf_no].device, &br);

    if (buf_vect[buf_no].valid)
	return buf_no;
    else
    {
	/*  Read error, getting rid of the buffer.  */
	buf_vect[buf_no].delayed_write = 0;
	buf_release(buf_no);
	return 0;
    }
}


/*
 * buf_write() marks the given buffer as 'delayed_write' and then  releases
 * it.  When, at a later moment, the system will need a new buffer and this
 * one will be allocated, the process receiving this buffer will write this
 * buffer to disk before using it.
 */

void
buf_write(int buf_no)
{
    buf_vect[buf_no].valid = buf_vect[buf_no].delayed_write = 1;
    buf_release(buf_no);
}


/*
 * buf_flush() deletes  'count'  buffers from the free list, flushing  them if
 * necessary.  Whenever  possible, buf_flush() groups buffers  into  requests,
 * writing them with drv_write() to speed up things.
 *
 * In buf_flush() and in any other function that parses the double linked list
 * of buffers, if the interrupts are enabled the list can change its structure
 * and  the  `next'  field  can  become  invalid.  Therefore,  after  enabling
 * interrupts the parse loop should be restarted.  This is a serious  drawback
 * of this algorithm that should be fixed one day...
 *
 * buf_flush() is reentrant because when adding a new buffer  to  the  current
 * request the `delayed_write' bit of that buffer  is  reset.  Interrupts  are
 * only enabled when the request is ready and we are about to call drv_write()
 * The buffers are marked `busy' during drv_write() so that there are no races.
 */

int
buf_flush(__dev_t device, int count, int remove)
{
    blk_request br;
    int buf_no, first_buf_no = 0, prev_buf_no = 0;
    int next, request = 0, prev_block = 0, requested = count;


    DEBUG(6, "(%d/%d,%d,%d) - [fp=%d,fdp=%d,fb=%d]\n",
	  major(device), minor(device), count, remove,
	  free_pages, buf_free_pages, free_buffers);

  restart:

    for (buf_no = buf_vect[1].lnext; buf_no != 1 && count > 0; buf_no = next)
    {
	next = buf_vect[buf_no].lnext;

	if (buf_vect[buf_no].delayed_write)
	{
	    if (request == 0)
	    {
		if (device == 0)
		    device = buf_vect[buf_no].device;
		else
		    if (buf_vect[buf_no].device != device)
			continue;

		/* Initialize the request, set up the first buffer.  */
		buf_vect[buf_no].busy          = 1;
		buf_vect[buf_no].delayed_write = 0;
		buf_vect[buf_no].next          = 0;

		prev_buf_no = first_buf_no = buf_no;
		prev_block  =
		br.block    = buf_vect[buf_no].block;
		br.blksz    = buf_vect[buf_no].size;
		br.nblocks  = 1;
		br.buf_no   = buf_no;
		br.next     = 0;
		request     = 1;
		count--;
	    }
	    else
	    {
		if (buf_vect[buf_no].device == device &&
		    buf_vect[buf_no].block  == prev_block + 1)
		{
		    /* Add to the current request.  */
		    buf_vect[prev_buf_no].next     = buf_no;
		    buf_vect[buf_no].next          = 0;
		    buf_vect[buf_no].delayed_write = 0;
		    buf_vect[buf_no].busy          = 1;

		    prev_block  = buf_vect[buf_no].block;
		    prev_buf_no = buf_no;
		    br.nblocks++;
		    count--;
		}
		else
		{
		  flush_request:

		    /* No match, write the current request and restart.  */
		    drv_write(device, &br);
		    request = 0;

		    for (buf_no = first_buf_no; buf_no;
			 buf_no = buf_vect[buf_no].next)
		    {
			buf_vect[buf_no].busy = 0;
			wakeup(&buf_vect[buf_no]);

			if (remove)
			    buf_remove(buf_no);
		    }

		    goto restart;
		}
	    }
	}
	else
	    if (remove && !buf_vect[buf_no].busy)
	    {
		/* Don't wait for the buffer to become  unlocked.  Busy
		   buffers are skiped.  As a matter of fact, remove  is
		   specified only when we want to  close  a  device, so
		   there should be no busy buffers.  IS THIS TRUE?  */
		buf_remove(buf_no);
		count--;
	    }
    }

    if (request)
	goto flush_request;

    /* Returns the number of buffers flushed/removed.  */
    return requested - count;
}


/*
 * buf_get() returns a buffer for the specified block/device.  This
 * function always returns the requested buffer (it sleeps until a
 * buffer becomes available).  buf_get() doesn't read the given block
 * from the disk!
 */

int
buf_get(__dev_t device, int block, size_t size)
{
    int buf_no;

    /* I really have to think about this!  */
    if (size != 1024)
	PANIC("TFS: trying to get a %d bytes buffer.\n", size);

    for (;;)
    {
	if ((buf_no = buf_hash_search(device, block)))
	{
	    if (buf_vect[buf_no].busy)
	    {
		buf_vect[buf_no].in_demand = 1;
		sleep(&buf_vect[buf_no], WAIT_BUFFER);
		continue;
	    }

	    buf_vect[buf_no].busy = 1;
	    buf_list_delete(buf_no);
	    return buf_no;
	}

	if (free_pages > HIGH_PAGES_LIMIT || buf_free_pages)
	{
	    if (master_free_buffers)
	    {
		buf_master_list_delete(buf_no = buf_vect[0].lnext);
		return buf_update(buf_no, device, block, size);
	    }

	    if (free_buffers)
	    {
		if (buf_vect[buf_no = buf_vect[1].lnext].busy)
		{
		    buf_vect[buf_no].in_demand = 1;
		    sleep(&buf_vect[buf_no], WAIT_BUFFER);
		    continue;
		}

		if (buf_vect[buf_no].delayed_write)
		{
		    buf_flush(0, 16, 0);
		    continue;
		}
		else
		{
		    buf_list_delete(buf_no);
		    buf_hash_delete(buf_no);
		    return buf_update(buf_no, device, block, size);
		}
	    }

	    sleep(&buf_get, WAIT_BUFFER);
	    continue;
	}
	else
	{
	    DEBUG(2, "POSTPONED - [fp=%d,fdp=%d,fb=%d]\n",
		  free_pages, buf_free_pages, free_buffers);

	    /* We should release here enough buffers to be sure that
	       at the next step we will be able to get a buffer using
	       a dedicated page.  If something else happens, it is
	       possible to loop forever with interrupts disabled.  Be
	       careful.

	       If we will be out of buffer structures, we will loop
	       forever anyway, so we just reschedule to give other
	       processes the opportunity to release some buffers &
	       memory (or ask for more :-).  */

	    while (buf_free_pages == 0)
		if (buf_flush(0, 32, 1) == 0)
		    reschedule();

	    continue;
	}
    }
}


/*
 * If buf_release() will ever enable interrupts we should modify file_read()
 * because I've used there the next field from buf_vect[buf_no] *after*
 * calling buf_release().  I know it is ugly, but is faster :-).
 */

void
buf_release(int buf_no)
{
    wakeup(&buf_get);

    if (buf_vect[buf_no].in_demand)
    {
	wakeup(&buf_vect[buf_no]);
	buf_vect[buf_no].in_demand = 0;
    }

    if (buf_vect[buf_no].valid)
	buf_list_insert_tail(buf_no);
    else
	buf_list_insert_head(buf_no);

    buf_vect[buf_no].busy = 0;
}


/*
 * buf_shrink() tries to free up some buffers until the number of
 * system free pages equals free_pages_request.  It doesn't try it too
 * hard, as this function gets called from the swapper and it is
 * important to return as soon as possible.
 */

void
buf_shrink(int free_pages_request)
{
    DEBUG(6, "(%d) - [fp=%d,fdp=%d,fb=%d]\n",
	  free_pages_request, free_pages, buf_free_pages, free_buffers);

    /* Try to flush until free_pages >= free_pages_request, give up
       immediately if there is nothing that can be done.  */
    while (free_pages < free_pages_request && buf_flush(0, 32, 1));
}


/*
 * Synchronize the given device.
 */

void
buf_sync_device(__dev_t device, int remove)
{
    int buf_no, count;

    DEBUG(6, "(%d/%d,%d)\n", major(device), minor(device), remove);

    down_sync_mutex(device);

    /* Remove the buffers residing in dedicated pages too if remove is
       specified. This happens only if buf_sync_device() was called to
       remove the specified device.  */

    /* Try to flush all the buffers, but don't be paranoid.  If we try
       to flush everything in buf_flush(), since  interrupts  will  be
       enabled from time to time, new buffers might  be  added  to the
       list and buf_flush() will try to sync them too.  If  a  process
       continuously writes something  to  the  disk, buf_flush()  will
       keep going, making disk writes completely synchronous.
       Limiting the number of buffer to be flushed to  the  number  of
       delayed-write buffers before the  buf_flush()  call  seems fair
       enough.  */

    if (!remove)
	for (buf_no = buf_vect[1].lnext, count = 0;
	     buf_no != 1;
	     buf_no = buf_vect[buf_no].lnext)
	    count += buf_vect[buf_no].delayed_write;
    else
	count = FLUSH_ALL_BUFFERS;

    buf_flush(device, count, remove);
    drv_sync(device);

#if 0
    /* This might slow things down a little.  */
    if (remove)
    {
	for (buf_no = buf_vect[1].lnext;
	     buf_no != 1;
	     buf_no = buf_vect[buf_no].lnext)
	    if (buf_vect[buf_no].device == device)
		PANIC("device %d/%d still has a block (%x) in the cache\n",
		      major(device), minor(device),
		      buf_vect[buf_no].block);
    }
#endif

    up_sync_mutex(device);
}


/*
 * buf_sync() writes to the disk all the specified device buffers. If
 * 'device' == SYNC_ALL_DEVICES,  it  synchronize  all  the  devices.
 * The 'remove' argument specifies how to handle  the  buffers  after
 * writting them to disk.  If it has a nonzero value, the buffers are
 * removed.  This is useful when closing special block device files.
 */

void
buf_sync(__dev_t device, int remove)
{
    int dev_maj, dev_min, minors;

    if (device == SYNC_ALL_DEVICES)
    {
	/* Flush all the devices.  */

	for (dev_maj = 0; dev_maj < MAX_DRIVERS; dev_maj++)
	    if (is_block_device(dev_maj))
	    {
		minors = get_minors(dev_maj);

		for (dev_min = 0; dev_min < minors; dev_min++)
		{
		    device = makedev(dev_maj, dev_min);

		    if (device_in_use(device))
			buf_sync_device(device, remove);
		}
	    }

	return;
    }

    buf_sync_device(device, remove);
}


/*
 * Clean up the buffer cache-related data structures.  Return the number
 * of errors encountered.  Actually this only performs some  consistency
 * checkings.
 */

int
buf_cleanup(void)
{
    int i;
    int errors = 0;

    /* Check for delayed write buffers.  */
    for (i = 0; i < MAX_BUFFERS; i++)
	if (buf_vect[i].delayed_write)
	{
	   printk("WARNING: buffer %x(%x,%x) is still marked delayed write.\n",
		  i, buf_vect[i].device, buf_vect[i].block);
	   errors++;
	}

    return errors;
}


/*
 * Initialize the buffer cache.
 */

void
buf_init(void)
{
    int i;
    int size = (MAX_BUFFERS + BUFFERS_BUCKETS_NO + SYSTEM_SWAP_MAX + 1) *
	       sizeof(buffer_t);

    /* Allocate space for buf_vect[].  */
    buf_vect = (buffer_t *)vm_static_alloc(size, size, sizeof(buffer_t *));

    memset(buf_vect, 0, size);

    for (i = 0; i < BUF_DEDICATED_PAGES; i++)
	buf_release_page(get_page());

    for (i = 0; i < MAX_BUFFERS; buf_vect[i++].hnext = 0xFFFF);

    buf_vect[0].lnext = buf_vect[0].lprev = 0;
    buf_vect[1].lnext = buf_vect[1].lprev = 1;

    for (i = 2; i < MAX_BUFFERS; buf_master_list_insert(i++));

    for (i = 0; i < BUFFERS_BUCKETS_NO; i++)
	buf_vect[MAX_BUFFERS + i].hnext =
	buf_vect[MAX_BUFFERS + i].hprev = MAX_BUFFERS + i;

    for (i = 0; i < SYSTEM_SWAP_MAX + 1; i++)
	buf_vect[MAX_BUFFERS + BUFFERS_BUCKETS_NO + i].size = PAGE_SIZE;
}
