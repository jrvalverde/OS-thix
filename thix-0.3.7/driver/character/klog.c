/* klog.c -- The kernel log driver.  */

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
#include <thix/errno.h>
#include <thix/fcntl.h>
#include <thix/string.h>
#include <thix/irq.h>
#include <thix/klog.h>
#include <thix/gendrv.h>


static minor_info klog_minor_info[1] =
{
    {0, 0, 1},
};


static character_driver klog_driver =
{
    "kernel log",
    klog_open,
    klog_close,
    klog_read,
    NULL,
    NULL,
    NULL,
    klog_fcntl,
    klog_lseek,
    NULL,
    KLOG_MAJOR,
    NOIRQ,
    CHR_DRV,
    1,
    klog_minor_info,
};


static int   klog_size = 8 * PAGE_SIZE;
static int   klog_kpid;          /* The process that has opened /dev/klog.  */
static int   klog_opened;        /* Flag saying if /dev/klog is opened.  */
static int   klog_flags;
static char *klog_buf;           /* The kernel log buffer.  */
static volatile char *klog_rptr; /* The read pointer.  */
static volatile char *klog_wptr; /* The write pointer.  */
static volatile int   klog_free; /* Count of free bytes in the buffer.  */


/*
 * A *very* simple driver for kernel message logging.
 */


static void
klog_reset(void)
{
    klog_free = klog_size;
    klog_rptr = klog_wptr = klog_buf;
}


int
klog_open(int minor, int flags)
{
    DEBUG(9, "(%d/%d)\n", KLOG_MAJOR, minor);

    /* Only one process is allowed to read from /dev/klog.  That process
       will be klogd, the kernel log daemon.  */
    if (klog_opened)
	return -EPERM;

    /* Store its kernel process id.  */
    klog_kpid = current_kpid;

    klog_opened = 1;
    return 0;
}


int
klog_close(int minor)
{
    DEBUG(9, "(%d/%d)\n", KLOG_MAJOR, minor);

    klog_opened = 0;
    return 0;
}


int
klog_read(int minor, chr_request *cr)
{
    int count = cr->count;

    if (!klog_opened)
	return -EIO;

    /* Handle non-blocking mode.  */
    if ((klog_rptr == klog_wptr) && (klog_flags & O_NONBLOCK))
    {
	klog_reset();
	return -EAGAIN;
    }

    while (count)
    {
	while (klog_rptr == klog_wptr)
	    if (sleep(&klog_rptr, WAIT_KLOG_INPUT))
		return -EINTR;

	if (count < klog_wptr - klog_rptr)
	{
	    memcpy(cr->buf + cr->count - count, (char *)klog_rptr, count);
	    klog_rptr += count;
	    klog_free -= count;
	    break;
	}
	else
	{
	    memcpy(cr->buf + cr->count - count,
		   (char *)klog_rptr, klog_wptr - klog_rptr);
	    count -= klog_wptr - klog_rptr;
	    klog_reset();
	}
    }

    return cr->count;
}


/* Only the kernel  has access to this function and we assume it will not
   abuse it, that  is, it will not try to write more than klog_size bytes
   at once.  Also  note that klog_write() cannot sleep because printk  is
   usually  called in functions  where  interrupts  are  supposed  to  be
   disabled, and sleeping involves activating them.  On the  other  hand,
   if a process tries to write something to the kernel log and finds  the
   buffer full, it will give up and the message will be lost.  With debug
   levels that generate a small number of messages there is  no  problem,
   but if we want to generate lots of messages there is a  big chance  to
   loose messages, so we have to increase the klog buffer size.  */

int
klog_write(int minor, chr_request *cr)
{
    if (!klog_opened)
	return -EIO;

    /* Don't write to the klog device on behalf of the process that is
       supposed to read from it.  With the  current, extremely  simple
       implementation, the buffer might be full and the process  go to
       sleep, never having a chance to empty it.  The system will then
       hang.  */
    if (current_kpid == klog_kpid)
	return -EIO;

    /* Check if there is enough free space for the new message.  Cry if
       there isn't :-( .  */
    if (klog_free < cr->count)
	return -EPERM;

    memcpy((char *)klog_wptr, cr->buf, cr->count);

    klog_wptr += cr->count;
    klog_free -= cr->count;

    wakeup(&klog_rptr);

    return cr->count;
}


int
klog_fcntl(int minor, int flags)
{
    if (!klog_opened)
	return -EIO;

    klog_flags = flags;

    return 0;
}


int
klog_lseek(int minor, __off_t offset)
{
    if (minor < klog_driver.minors)
	return -EINVAL;

    return -ESPIPE;
}


int
klog_init(void)
{
    int mem, current_page, last_page;
    int registered = register_driver((generic_driver *)&klog_driver);

    klog_flags = klog_opened = 0;

    current_page = get_page();
    klog_buf = (char *)(current_page << PAGE_SHIFT);

    for (mem = PAGE_SIZE; mem < klog_size; mem += PAGE_SIZE)
    {
	last_page    = current_page;
	current_page = get_page();

	if (current_page != last_page + 1)
	{
	    release_page(current_page);
	    break;
	}
    }

    klog_size = mem;
    klog_reset();

    printk("KLOG: internal /dev/klog buffer size=%d bytes\n", klog_size);

    return registered;
}
