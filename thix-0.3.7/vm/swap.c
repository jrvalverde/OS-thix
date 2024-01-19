/* swap.c -- Swaping routines.  */

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
#include <thix/process.h>
#include <thix/limits.h>
#include <thix/a.out.h>
#include <thix/gendrv.h>
#include <thix/fs.h>
#include <thix/ioctl.h>
#include <thix/buffer.h>
#include <thix/blkdrv.h>
#include <thix/regular.h>
#include <thix/fcntl.h>
#include <thix/stat.h>
#include <thix/kpid.h>
#include <thix/semaphore.h>
#include <thix/hdc.h>
#include <thix/proc/i386.h>


unsigned SWAP_DATA_MAP_ADDRESS;         /* dynamically allocated, 1M.  */


int total_free_swap_blocks = 0;
static int current_vmdev = -1;
static int current_block = -1;
pgdata_t *current_pgd = NULL;


swap_t sdt[SYSTEM_SWAP_MAX];


unsigned work_set[SYSTEM_OPEN_MAX][WORK_SET_SIZE];
unsigned *work_set_ptr[SYSTEM_OPEN_MAX];

int swapper_running = 1;
blk_request swapper_br;


void pg_hash_insert(int page);
void pg_hash_delete(int page);
int  pg_hash_search(int vmdev, int block);


/*
 * Swap device entries allocation.
 * A swap device entry is free if it's total field is 0.
 */

int
get_sd(int vmdev)
{
    int sd;     /* swap devices table index. */

    for (sd = 0; sd < SYSTEM_SWAP_MAX; sd++)
	if (sdt[sd].total == 0)
	{
	    sdt[sd].vmdev  = vmdev;
	    sdt[sd].total  = 1;         /* Prevent reallocation.  */
	    sdt[sd].free   = 0;
	    sdt[sd].latest = 0;
	    return sd;
	}
	else
	    if (sdt[sd].vmdev == vmdev)
	    {
		DEBUG(1, "**** DEVICE %w ALREADY IN USE ****\n", sd);
		return -1;
	    }

    return -1;
}


void
release_sd(int sd)
{
#ifdef __PARANOIA__
    if (sd < 0 || sd >= SYSTEM_SWAP_MAX)
	PANIC("**** Trying to release unused swap device %x ****\n", sd);
#endif

    sdt[sd].total = 0;
}


/*
 * Initialize swapping.
 */

void
swap_init(void)
{
    int sd;

    /* Reserve space for swapping.  */
    SWAP_DATA_MAP_ADDRESS = (unsigned)vm_static_alloc(0,
						      SYSTEM_SWAP_MAX *
						      SYSTEM_SWAPBLK_MAX *
						      sizeof(char),
						      PAGE_SIZE);

    memset(sdt, 0, sizeof(sdt));

    swapper_br.blksz   = PAGE_SIZE;
    swapper_br.nblocks = 1;
    swapper_br.next    = 0;
    swapper_br.buf_no  = MAX_BUFFERS + BUFFERS_BUCKETS_NO + SYSTEM_SWAP_MAX;

    for (sd = 0; sd < SYSTEM_SWAP_MAX; sd++)
    {
	sdt[sd].bitmap = (char *)(SWAP_DATA_MAP_ADDRESS +
				  SYSTEM_SWAPBLK_MAX * sizeof(char) * sd);
	sdt[sd].mutex  = 1;
    }
}


/*
 * Insert 'page' into the process working set.  I'm not really sure that
 * all this working set stuff improves performance,  but I remember that
 * I've managed avoiding some races with it.  I certainly have to take a
 * look at this one day.
 */

void
work_set_insert(int kpid, unsigned page)
{
   if (work_set_ptr[kpid] - work_set[kpid] >= WORK_SET_SIZE)
       work_set_ptr[kpid] = work_set[kpid];

   *work_set_ptr[kpid]++ = page;
}


/*
 * Search for 'page' in the working set of the process 'kpid'.  If we
 * are really going to use this, it should be a 'rep scasl'.
 */

int
page_in_work_set(int kpid, unsigned page)
{
    int i;

    for (i = 0; i < WORK_SET_SIZE; i++)
	if (work_set[kpid][i] == page)
	    return 1;

    return 0;
}


/*
 * Search a free swap block.  Return its number if found.  PANIC otherwise
 * because get_swap_block() is only called when there is at least one free
 * swap block on that device, in order to improve search speed. If 'block'
 * is specified, only increment its count into the  bitmap  and  decrement
 * both local and global 'free' counters.  The actual  free  block  search
 * starts from the point where the last one ended, trying to keep swapping
 * from becoming scattered all over the swap partition.
 */

int
get_swap_block(int sd, int block)
{
    int page;
    pgdata_t *pgd;

    DEBUG(6, "(%x,%x)\n", sd, block);

    if (block)
    {
	if (sdt[sd].bitmap[block] == 0)
	{
	    sdt[sd].free--;
	    total_free_swap_blocks--;
	}

	sdt[sd].bitmap[block]++;
	return block;
    }

    for (block = sdt[sd].latest + 1; block < sdt[sd].total; block++)
	if (sdt[sd].bitmap[block] == 0)
	    goto found;

    for (block = 1; block <= sdt[sd].latest; block++)
	if (sdt[sd].bitmap[block] == 0)
	    goto found;

    printk("\nsd: %w\n", sd);

    for (sd = 0; sd < 4; sd++)
	printk("sdt[%w].free: %x\n", sd, sdt[sd].free);

    printk("total_free_swap_blocks: %x\n", total_free_swap_blocks);
    PANIC("get_swap_block() ERROR");

  found:

    while ((page = pg_hash_search(sdt[sd].vmdev, block)))
    {
	pgd = &pgdata[page];
	pg_hash_delete(page);
	pgd->block = 0;
    }

    sdt[sd].free--;
    sdt[sd].latest = block;
    sdt[sd].bitmap[block] = 1;
    total_free_swap_blocks--;
    return block;
}


/*
 * Release a swap block.  Decrement its bitmap count.
 */

void
release_swap_block(int sd, int block)
{
    if (--sdt[sd].bitmap[block])
	return;

    sdt[sd].free++;
    total_free_swap_blocks++;
}


/*
 * Search a free swap block.  Called only when there is at least one free
 * swap block on the swap devices. So just PANIC if it fails to find one.
 */

void
swap_page(pgdata_t *pgd)
{
    int sd;

    for (sd = 0; sd < SYSTEM_SWAP_MAX; sd++)
	if (sdt[sd].total && sdt[sd].free)
	{
	    pgd->vmdev = sdt[sd].vmdev;
	    pgd->block = get_swap_block(sd, 0);
	    return;
	}

    PANIC("swap_page() ERROR");
}


/*
 * Load a page from a swap device.
 */

int
load_page(pgdata_t *pgd)
{
    int sd;
    __dev_t device;
    blk_request br;


    if (pgd->block == current_block && pgd->vmdev == current_vmdev)
    {
	DEBUG(6, "reading current (vmdev/block=%d/%x) -> %x\n",
	      pgd->vmdev, pgd->block, pgd_address(pgd));
	memcpy(pgd_address(pgd), pgd_address(current_pgd), PAGE_SIZE);
	return 1;
    }

    sd = vmdev_aux(pgd->vmdev);

    br.blksz   = PAGE_SIZE;
    br.nblocks = 1;
    br.next    = 0;
    br.buf_no  = MAX_BUFFERS + BUFFERS_BUCKETS_NO + sd;
    br.block   = pgd->block;

    DEBUG(0, "reading from swap (vmdev/block=%d/%w) -> %x\n",
	  pgd->vmdev, pgd->block, pgd_address(pgd));

    /* Convert the VM device number to a real 16-bit device number.  */
    device = vmdev_device(pgd->vmdev);

    down(&sdt[sd].mutex, WAIT_SWAPPER);
    buf_address(br.buf_no) = pgd_address(pgd);
    drv_read(device, &br);
    up(&sdt[sd].mutex);

    /* Statistics. */
    this->usage.ru_majflt++;

    /* We should check if the read operation went ok here !!!  */

    return 1;
}


/*
 * Write the page to the swap device.
 */

int
save_page(pgdata_t *pgd)
{
    /* Convert the VM device number to a real 16-bit device number.  */
    __dev_t device = vmdev_device(pgd->vmdev);

    current_vmdev = pgd->vmdev;
    current_block = pgd->block;
    current_pgd   = pgd;

    DEBUG(0, "writing to swap (vmdev/block=%d/%w) <- %x\n",
	  pgd->vmdev, pgd->block, (pgd - pgdata) << PAGE_SHIFT);

    swapper_br.block = pgd->block;
    buf_address(swapper_br.buf_no) = (void *)((pgd - pgdata) << PAGE_SHIFT);
    drv_write(device, &swapper_br);

    if (release_page(pgd - pgdata))
	wakeup(&get_page);

    current_block = 0;
    return 1;
}


/*
 * This is the main swapper function.
 */

void
swapper(void)
{
    pgdata_t *pgd;
    unsigned brk, stk;
    extpgtbl_t *extpgtbl;
    pgtbl_t *pgtbl, **pgdir;
    int kpid, pid, page_table, page;
    int limit, next_page_table, flag, shifted_page_table;


    /* Initialize the file system.  Mount /.  */
    fs_init();

    /* Load "/bin/init".  */
    startinit();

    DEBUG(1, "Swapper started.\n\n");

    for (;;) {

  main_loop:

    disable();

    if (free_pages > HIGH_PAGES_LIMIT)
    {
	swapper_running = 0;
	sleep(&swapper, WAIT_SWAPPER);
    }

    swapper_running = 1;

    /* Try to get some memory shrinking the buffer cache.  */
    buf_shrink(HIGH_PAGES_LIMIT + 1);

    enable();

    for (kpid = INIT; kpid; kpid = NEXT(kpid))
    {
	disable();
	if (status(kpid) == PROCESS_ZOMBIE)
	{
	    enable();
	    continue;
	}

	/* We might want to remove the exceeding stack  here  using
	   release_memory(u_area[kpid].stk, ESP - 0x80 - 1, STACK).
	   ESP can be computed using CS.  Don't  do  it  while  the
	   process is in sys_fork().  */

	/* Initialize some local variables used to avoid having
	   u_are[*].*  all over the place.  */

	pid   = pid(kpid);
	pgdir = u_area[kpid].pgdir;
	brk   = (u_area[kpid].brk + KERNEL_RESERVED_SPACE) >> PAGE_SHIFT;
	stk   = (u_area[kpid].stk + KERNEL_RESERVED_SPACE) >> PAGE_SHIFT;
	enable();
	next_page_table = KERNEL_PAGE_TABLES + 1;
	flag = 1;

	for (page_table = KERNEL_PAGE_TABLES;
	     page_table < 1024;
	     page_table = next_page_table++)
	{
	    disable();

	    /* Check if we are dealing with the same process and if it is
	       still swapable.  */
	    if (pid(kpid) != pid || !u_area[kpid].swapable)
		break;

	    /* Is this page table used ?  */
	    if (pgdir[page_table] == 0)
	    {
		enable();
		continue;
	    }

	    pgtbl    = (pgtbl_t *)((unsigned)pgdir[page_table] & 0xFFFFF000);
	    extpgtbl = (extpgtbl_t *)pgtbl;
	    enable();
	    shifted_page_table = (page_table + 1) << 10;
	    limit = 1024;

	    if (flag && shifted_page_table > brk)
	    {
		limit -= shifted_page_table - brk - 1;
		next_page_table = stk >> 10;
		flag = 0;
	    }

	    page = (page_table == (stk>>10) && limit==1024) ? stk & 0x3FF : 0;

	    for (; page < limit; page++)
	    {
		disable();

		/* Check if we are dealing with the same process and if it is
		   still swapable.  */
		if (pid(kpid) != pid || !u_area[kpid].swapable)
		    break;

		/* If the current page is not present or busy, continue.  */
		if (!pgtbl[page].present || pgtbl[page].busy)
		{
		    enable();
		    continue;
		}

		/* Have we reached the upper threshold limit?  Stop swapping
		   if we did.  */
		if (free_pages > HIGH_PAGES_LIMIT)
		    goto main_loop;
/*
		if (page_in_work_set(kpid, (page_table << 10) + page))
		{
		    enable();
		    continue;
		}
*/
		DEBUG(7, "(%w,%w)<%w>{%w}(%w)[%w]\r",
		      page_table, page, processes, kpid,
		      free_pages, total_free_swap_blocks);

		/* If the page have been accessed,  it means that it is
		   part of the current working set.  Don't swap it out!  */
		if (pgtbl[page].accessed)
		{
		    pgtbl[page].accessed = 0;
		    pgdata[pgtbl[page].address].aux.age = 0;
		    invalidate_cache();
		    enable();
		    continue;
		}

		/* If the page is old enough, swap it out.  */
		if (pgdata[pgtbl[page].address].aux.age == MAX_PAGE_AGE)
		{
		    /* Ok, we are going to swap this page out.  */
		    pgd = &pgdata[pgtbl[page].address];

		    switch (pgd->type)
		    {
			case PAGE_DZ:
			case PAGE_FILE:

			    /* Statistics. */
			    u_area[kpid].usage.ru_nswap++;

			    /* Keep track of its location and its type.  */
			    extpgtbl[page].vmdev = pgd->vmdev;
			    extpgtbl[page].block = pgd->block;
			    extpgtbl[page].type  = pgd->type;

			    /* Mark it "not present".  Also invalidate
			       the processor cache.  */
			    pgtbl[page].present = 0;
			    invalidate_cache();

			    /* Release the page.  */
			    if (release_page(pgd - pgdata))
				wakeup(&get_page);

			    break;

			case PAGE_SWAP:

			    if (pgd->block)
			    {
				/* This page can already  be found  on a
				   swap block, so we only have to adjust
				   the counters.  */
				get_swap_block(vmdev_aux(pgd->vmdev),
					       pgd->block);

				/* Statistics. */
				u_area[kpid].usage.ru_nswap++;

				/* Keep track of its location and its type.  */
				extpgtbl[page].vmdev = pgd->vmdev;
				extpgtbl[page].block = pgd->block;
				extpgtbl[page].type  = pgd->type;

				/* Mark it "not present".  Also invalidate
				   the processor cache.  */
				pgtbl[page].present = 0;
				invalidate_cache();

				if (release_page(pgd - pgdata))
				    wakeup(&get_page);

				break;
			     }

			    /* We need a swap block, but there is no free
			       block on the swap devices.  Give up.  */
			    if (total_free_swap_blocks == 0)
			    {
				enable();
				continue;
			    }

			    /* Initialize pgd with the vmdev/block numbers.  */
			    swap_page(pgd);
			    pg_hash_insert(pgd - pgdata);

			    /* Statistics. */
			    u_area[kpid].usage.ru_nswap++;

			    /* Mark the page "not present".  */
			    pgtbl[page].present = 0;

			    /* Keep track of its location and its type.  */
			    extpgtbl[page].vmdev = pgd->vmdev;
			    extpgtbl[page].block = pgd->block;
			    extpgtbl[page].type  = PAGE_SWAP;

			    /* Invalidate the processor cache.  */
			    invalidate_cache();

			    /* Write the page to disk.  */
			    save_page(pgd);
			    break;

			default:

			    PANIC("BAD PAGE TYPE... ");
		    }
		}
		else
		    pgdata[pgtbl[page].address].aux.age++;

		enable();
	    }
	    enable();
	}
	enable();
    }
    }
}


/*
 * Start swapping on 'device_name'.
 *
 * WARNING:  the current implementation is not yet finished, it can
 * swap on a device, but it doesn't check that the given device  is
 * indeed a swap device, so use it with care !
 */

int
sys_swapon(char *device_name)
{
    __dev_t device;
    int i, i_no, sd, vmdev;
    int requested_bytes, requested_pages, blksz, devsz;


    i_no = namei(device_name);

    if (i_no < 0)
	return i_no;

    if (!S_ISBLK(i_vect[i_no].i_mode))
    {
	i_get_rid_of(i_no);
	return -ENOTBLK;
    }

    device = i_vect[i_no].i_rdev;

    i_get_rid_of(i_no);

    if (device != makedev(HDC_MAJOR, 7))
	return -EINVAL;

    if (mounted(device))
	return -EBUSY;

    drv_open(device, O_RDWR);

    vmdev = get_vmdev(device, SWAP_DEVICE, 0);

    if (vmdev == -1)
    {
	drv_close(device);
	return -EINVAL;
    }

    sd = get_sd(vmdev);

    if (sd == -1)
    {
	drv_close(device);
	release_vmdev(vmdev);
	return -EINVAL;
    }

    vmdevt[vmdev].aux = sd;

    drv_ioctl(device, IOCTL_GETBLKSZ, &blksz);
    drv_ioctl(device, IOCTL_GETDEVSZ, &devsz);

    sdt[sd].total = ((blksz * devsz) / PAGE_SIZE);
    DEBUG(1, "sd=%x, vmdev=%x\n", sd, vmdev);
    DEBUG(1, "blksz=%x, devsz=%x, total=%x\n", blksz, devsz, sdt[sd].total);

    requested_bytes = sdt[sd].total * sizeof(char);
    requested_pages = (requested_bytes >> PAGE_SHIFT) +
		      ((requested_bytes & (PAGE_SIZE - 1)) ? 1 : 0);

    /* Maybe we should use reserve_pages() here...  */
    if (free_pages < requested_pages)
	return -ENOMEM;

    for (i = 0; i < requested_pages; i++)
	map_page(sdt[sd].bitmap + i * PAGE_SIZE, get_page() << PAGE_SHIFT);

    memset(sdt[sd].bitmap, 0, sdt[sd].total);

    total_free_swap_blocks += sdt[sd].total - 1;
    sdt[sd].free            = sdt[sd].total - 1;
    sdt[sd].latest          = 0;
    printk("Swap device: %s (%dk)\n",
	   device_name, (sdt[sd].total - 1) * PAGE_SIZE / 1024);
    return 0;
}


/*
 * Stop swapping to 'device_name'.  Not implemented yet.
 */

int
sys_swapoff(char *device_name)
{
    return -ENOSYS;
}


/*
 * Fill all the page .age fields with zeros.
 * Not implemented yet.
 */

void
initialize_pgtbl(void)
{
}
