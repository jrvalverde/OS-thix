/* driver.c -- Generic device drivers management.  */

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
#include <thix/sched.h>
#include <thix/klib.h>
#include <thix/string.h>
#include <thix/errno.h>
#include <thix/irq.h>
#include <thix/gendrv.h>
#include <thix/blkdrv.h>


#define ERR_NODEV       error[0]
#define ERR_BADDRV      error[1]
#define ERR_ISREG       error[2]
#define ERR_NOIRQ       error[3]
#define ERR_BADMAJ      error[4]
#define ERR_NOTIMER     error[5]

static char *error[] =
{
    "bad major device number",
    "attempt to register an invalid driver",
    "driver already registered",
    "driver IRQ already registered",
    "major device number too big",
    "can't register driver timer function",
};


/* Too small !?  */
#define MAX_BLKDEV_REQUESTS     128

/*
blk_request br_vect[MAX_BLKDEV_REQUESTS];

blk_request *br_head;
blk_request *br_tail;
*/


generic_driver driver[MAX_DRIVERS];


#ifdef __DEBUG__
void
display_driver_info(generic_driver *gd)
{
    DEBUG(0, "Driver    : %s\n",    gd->name);
    DEBUG(0, "open/close: %x/%x\n", gd->open,  gd->close);
    DEBUG(0, "read/write: %x/%x\n", gd->read,  gd->write);
    DEBUG(0, "sync/ioctl: %x/%x\n", gd->sync,  gd->ioctl);
    DEBUG(0, "timer     : %x/%x\n", gd->timer);
    DEBUG(0, "major/irq : %x/%x\n", gd->major, gd->irq);
}
#endif


/*
 * Check if a driver with the given major number has been registered.
 */

static inline int
registered(int major)
{
    return major >= 0 && major < MAX_DRIVERS && driver[major].open;
}


/*
 * Check if the given major number corresponds to a block device.
 */

int
is_block_device(int major)
{
    if (registered(major))
	return (driver[major].type == BLK_DRV);
    else
	return 0;
}


/*
 * Return the number of minor numbers for the given major number.  That is
 * the number of devices that have the given major number.
 */

int
get_minors(int major)
{
    if (registered(major))
	return driver[major].minors;
    else
	return 0;
}


/*
 * Register a driver.  This is called at boot time by all the device drivers.
 */

int
register_driver(generic_driver *gd)
{
    if (gd->major >= MAX_DRIVERS)
    {
	printk("%x: %s\n", gd->major, ERR_BADMAJ);
	return 0;
    }

    if (registered(gd->major))
    {
	printk("%x: %s\n", gd->major, ERR_ISREG);
	return 0;
    }

    if (gd->open == NULL || gd->close == NULL || gd->read == NULL)
    {
	printk("%x: %s\n", gd->major, ERR_BADDRV);
	return 0;
    }

    if (!irq_register(gd->irq))
    {
	printk("%x: %s\n", gd->major, ERR_NOIRQ);
	return 0;
    }

    if (!register_timer(gd->timer))
    {
	irq_unregister(gd->irq);
	printk("%x: %s\n", gd->major, ERR_NOTIMER);
	return 0;
    }

    driver[gd->major] = *gd;
    return 1;
}


int
check_device(__dev_t device)
{
    if (!registered(major(device)))
	return 0;

    if (minor(device) >= get_minors(major(device)))
	return 0;

    return 1;
}


minor_info *
get_minor_info(__dev_t device)
{
    return &driver[major(device)].minor[minor(device)];
}


void
set_minor_info(__dev_t device, int mp)
{
    driver[major(device)].minor[minor(device)].mp = mp;
}


/*
 * Open a device.  Return 0 if ok, -1 or -ENXIO on error.
 */

int
drv_open(__dev_t device, int flags)
{
    unsigned char major = major(device);
    unsigned char minor = minor(device);

    if (registered(major))
	return driver[major].open(minor, flags);
    else
	return -ENXIO;
}


/*
 * Close a device.  Return 0 if ok, -1 or -ENXIO on error.
 */

int
drv_close(__dev_t device)
{
    unsigned char major = major(device);
    unsigned char minor = minor(device);

    if (registered(major))
	return driver[major].close(minor);
    else
	return -ENXIO;
}


/*
 * For block devices:
 * read()/write() return the number of bytes read/written.
 * For character devices:
 * read()/write() return the number of bytes read/written or -1 if an error
 * occured or the driver has been interrupted by a signal.
 */

int
drv_read(__dev_t device, void *dr)
{
    unsigned char major = major(device);
    unsigned char minor = minor(device);

    DEBUG(6, "(%d/%d) block=%x nblocks=%d\n", major, minor,
	  ((blk_request *)dr)->block, ((blk_request *)dr)->nblocks);

    if (registered(major))
	if (driver[major].read)
	{
	    int bytes = driver[major].read(minor, (drv_request *)dr);

	    if (driver[major].type == BLK_DRV && bytes > 0)
	    {
		/* Statistics.  */
		this->usage.ru_inblock += bytes / ((blk_request *)dr)->blksz;
	    }

	    return bytes;
	}
	else
	    return -EINVAL;

    PANIC("(%d/%d,%x) %s\n", major, minor, dr, ERR_NODEV);

    /* Not reached.  */
    return -ENXIO;
}


int
drv_write(__dev_t device, void *dr)
{
    unsigned char major = major(device);
    unsigned char minor = minor(device);

    DEBUG(6, "(%d/%d) block=%x nblocks=%d\n", major, minor,
	  ((blk_request *)dr)->block, ((blk_request *)dr)->nblocks);

    if (registered(major))
	if (driver[major].write)
	{
	    int bytes = driver[major].write(minor, (drv_request *)dr);

	    if (driver[major].type == BLK_DRV && bytes > 0)
	    {
		/* Statistics.  */
		this->usage.ru_oublock += bytes / ((blk_request *)dr)->blksz;
	    }

	    return bytes;
	}
	else
	    return -EINVAL;

    PANIC("(%d/%d,%x) %s\n", major, minor, dr, ERR_NODEV);

    /* Not reached.  */
    return -ENXIO;
}


/*
 * Synchronize a device.
 */

void
drv_sync(__dev_t device)
{
    unsigned char major = major(device);
    unsigned char minor = minor(device);

    if (registered(major))
    {
	if (driver[major].sync)
	    driver[major].sync(minor);

	return;
    }

    PANIC("(%d/%d) %s\n", major, minor, ERR_NODEV);
}


int
drv_ioctl(__dev_t device, int cmd, void *argp)
{
    unsigned char major = major(device);
    unsigned char minor = minor(device);

    if (registered(major))
	if (driver[major].ioctl)
	    return driver[major].ioctl(minor, cmd, argp);
	else
	    return -EINVAL;

    PANIC("(%d/%d,%d,%x) %s\n", major, minor, cmd, argp, ERR_NODEV);

    /* Not reached.  */
    return -ENXIO;
}


int
drv_fcntl(__dev_t device, int flags)
{
    unsigned char major = major(device);
    unsigned char minor = minor(device);

    if (registered(major))
	if (driver[major].fcntl)
	    return driver[major].fcntl(minor, flags);
	else
	    return -EINVAL;

    PANIC("(%w,%o) %s %d\n", device, flags, ERR_NODEV, major);

    /* Not reached.  */
    return -ENXIO;
}


int
drv_lseek(__dev_t device, __off_t offset)
{
    unsigned char major = major(device);
    unsigned char minor = minor(device);

    if (registered(major))
	if (driver[major].lseek)
	    return driver[major].lseek(minor, offset);
	else
	    return -ESPIPE;

    PANIC("(%w,%d) %s %d\n", device, offset, ERR_NODEV, major);

    /* Not reached.  */
    return -ENXIO;
}


int
drv_init(void)
{
/*
    int i;

    for (i = 0; i < MAX_BLKDEV_REQUESTS; i++)
	br_vect[i].block = -1;
*/
    memset(driver, 0, sizeof(driver));
    return 1;
}
