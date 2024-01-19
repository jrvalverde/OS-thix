/* hdc.c -- The (E)IDE hard disk driver.  */

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


#include <thix/klib.h>
#include <thix/vm.h>
#include <thix/string.h>
#include <thix/errno.h>
#include <thix/sched.h>
#include <thix/sleep.h>
#include <thix/irq.h>
#include <thix/hdc.h>
#include <thix/buffer.h>
#include <thix/gendrv.h>
#include <thix/semaphore.h>
#include <thix/part.h>
#include <thix/ioctl.h>
#include <thix/proc/i386.h>
#include <thix/arch/pc/cmos.h>


#define ERR_BADREQ      error[0]

static char *error[] =
{
    "bad request",
};


#define HDPORT_DATA     0x1F0
#define HDPORT_ERROR    0x1F1   /* Read.  */
#define HDPORT_WPCOMP   0x1F1   /* Write.  */
#define HDPORT_SECTORS  0x1F2
#define HDPORT_SECTOR   0x1F3
#define HDPORT_LOWCYL   0x1F4
#define HDPORT_HICYL    0x1F5
#define HDPORT_DRVHD    0x1F6
#define HDPORT_CMD      0x1F7   /* Write.  */
#define HDPORT_STATUS   0x1F7   /* Read.  */


#define HDCTRL_NOMARK   0x01
#define HDCTRL_BADTRK0  0x02
#define HDCTRL_CMDABORT 0x04
#define HDCTRL_BADID    0x10
#define HDCTRL_BADECC   0x40
#define HDCTRL_BADBLK   0x80


#define HDCMD_RESTORE   0x10
#define HDCMD_READ      0x20
#define HDCMD_WRITE     0x30
#define HDCMD_VERIFY    0x40
#define HDCMD_FORMAT    0x50
#define HDCMD_SEEK      0x70
#define HDCMD_DIAGNOSE  0x90
#define HDCMD_SPECS     0x91


#define HDSTAT_ERROR    0x01
#define HDSTAT_INDEX    0x02
#define HDSTAT_ECC      0x04
#define HDSTAT_DATAREQ  0x08
#define HDSTAT_SEEKOK   0x10
#define HDSTAT_WRFAULT  0x20
#define HDSTAT_READY    0x40
#define HDSTAT_BUSY     0x80


#define MAX_RESET_RETRIES       10
#define MAX_SEND_CMD_RETRIES     8
#define MAX_COMMAND_RETRIES      8
#define MAX_READ_RETRIES        40
#define MAX_WRITE_RETRIES       40


#define HDC_READY_MASK          (HDSTAT_READY  | HDSTAT_SEEKOK)

#define HDC_STATUS_MASK         (HDSTAT_READY  | HDSTAT_WRFAULT |\
				 HDSTAT_SEEKOK | HDSTAT_ERROR)

#define HDC_BLKSZ              512


void hdc_interrupt();
static int  hdc_reset(void);


typedef struct
{
    int cylinders;      /* Maximum number of cyliners.           */
    int heads;          /* Maximum number of heads.              */
    int sectors;        /* Number of sectors per track.          */
    int wp_cylinder;    /* Write precompensation cylinder.       */
    unsigned char dso;  /* Drive step options.                   */
    int total_sectors;  /* Total number of sectors.              */
    int present;        /* The hard disk is installed.           */
    int lbaf;           /* The LBA (large block access) factor.  */
} struct_hdd;


static semaphore hdc_oc_mutex = 1;
static semaphore hdc_rw_mutex = 1;


static int hdc_spb;
static int hdc_drive;
static int hdc_buf_no;
static int hdc_command;
static int hdc_buf_index;
static int hdd_count = 0;


static volatile int hdc_ticks           = 0;
static volatile int hdc_buf_no          = 0;
static volatile int hdc_timeout         = 0;
static volatile int hdc_need_reset      = 0;
static volatile int hdc_waits_interrupt = 0;
static volatile int hdc_interrupts_left = 0;


static struct_hdd hdd[2];
static partition  hdd_partition[8];


static minor_info hdc_minor_info[10] =
{
    { 0, 0, 1 },
    { 0, 0, 1 },
    { 0, 0, 1 },
    { 0, 0, 1 },
    { 0, 0, 1 },
    { 0, 0, 1 },
    { 0, 0, 1 },
    { 0, 0, 1 },
    { 0, 0, 1 },
    { 0, 0, 1 },
};


static block_driver hdc_driver =
{
    "IDE hard disk",
    hdc_open,
    hdc_close,
    hdc_read,
    hdc_write,
    NULL,
    hdc_ioctl,
    NULL,
    hdc_lseek,
    hdc_timer,
    HDC_MAJOR,
    HDC_IRQ,
    BLK_DRV,
    10,
    hdc_minor_info,
};


static int hdc_buf_index = 0;


#define DRIVE_HEAD(drive, head) (drive ? (0xB0 | head) : (0xA0 | head))


/*
 * The hard disk timer.  Used only to detect timeouts.
 */

void
hdc_timer(void)
{
    if (hdc_waits_interrupt && ++hdc_ticks > HZ * 2)
    {
	hdc_waits_interrupt = 0;
	hdc_timeout = 1;
	wakeup(&hdc_interrupt);
    }
}


/*
 * Handle unexpected hard disk interrupts.  Force a controller reset if
 * something unusual happens.
 */

static void
hdc_surprise(void)
{
    printk("HDC: surprise: [error=%x status=%x]\n",
	   inb(HDPORT_ERROR), inb(HDPORT_STATUS));
    hdc_need_reset = 1;
}


static int
hdc_data_request(void)
{
    int retries = 0x1000;

    do
    {
	if (inb(HDPORT_STATUS) & HDSTAT_DATAREQ) return 1;
	while ((inb(0x61) & 0x10) == 0);
	if (!--retries == 0) break;
	if (inb(HDPORT_STATUS) & HDSTAT_DATAREQ) return 1;
	while ((inb(0x61) & 0x10));
    }
    while (--retries);

    return 0;
}


/*
 * The hard disk interrupt handler.
 */

void
hdc_interrupt(void)
{
    int status, retries = 30000;
    static unsigned short *address;


    if (!hdc_waits_interrupt)
    {
	hdc_surprise();
	return;
    }

    switch (hdc_command)
    {
	case HDCMD_SEEK:
	case HDCMD_RESTORE:
	case HDCMD_SPECS:
	case HDCMD_WRITE:
	    status = inb(HDPORT_STATUS);
	    if ((status & HDC_STATUS_MASK) != HDC_READY_MASK)
		printk("HDC: controller error: [%x]\n", status);
	    break;

	case HDCMD_READ:
	    if (hdc_buf_no == 0)        /* Something bad happened... */
	    {
		printk("HDC: got buffer 0, trying to recover...\n");
		break;
	    }

	    while (--retries)
	    {
		if ((status = inb(HDPORT_STATUS)) & HDSTAT_BUSY)
		    continue;

		if ((status & HDC_STATUS_MASK) != HDC_READY_MASK)
		{
		    printk("HDC: controller error: [%x]\n", status);
		    retries = 0;
		    break;
		}

		if (hdc_data_request())
		{
		    if (hdc_buf_index == 0)
			address = (unsigned short *)buf_address(hdc_buf_no);

		    insw(HDPORT_DATA,
			 address + (hdc_buf_index * HDC_BLKSZ) / 2,
			 HDC_BLKSZ / 2);

		    hdc_buf_index++;
		    break;
		}

		printk("HDC: retrying... [%x]\n", retries);
	    }

	    if (retries)
	    {
		buf_vect[hdc_buf_no].valid = 1;

		if (hdc_buf_index == hdc_spb)
		{
		    hdc_buf_no = buf_vect[hdc_buf_no].next;
		    hdc_buf_index = 0;
		}

		if (--hdc_interrupts_left == 0)
		    break;

		hdc_ticks = 0;
		return;
	    }
	    else
		buf_vect[hdc_buf_no].valid = 0;

	    break;

	default:
	    printk("HDC: unknown command %x\n", hdc_command);
	    hdc_surprise();
	    return;
    }

    hdc_ticks = 0;
    hdc_waits_interrupt = 0;
    wakeup(&hdc_interrupt);
}


/*
 * Wait for a hard disk interrupt.  Force a controller reset on timeout.
 */

static int
wait_hdc_interrupt(void)
{
    hdc_ticks = 0;
    hdc_waits_interrupt = 1;
    sleep(&hdc_interrupt, WAIT_DEVICE_IO);

    if (hdc_timeout)
    {
	hdc_need_reset = 1;
	printk("HDC: drive %d timeout\n", hdc_drive);
    }

    return hdc_timeout;
}


static int
hdc_busy(void)
{
    int retries = 0xFFFFF;

    do
    {
	if ((inb(HDPORT_STATUS) & HDSTAT_BUSY) == 0)
	    return 0;

	while ((inb(0x61) & 0x10) == 0);

	if (--retries == 0)
	    break;

	if ((inb(HDPORT_STATUS) & HDSTAT_BUSY) == 0)
	    return 0;

	while ((inb(0x61) & 0x10));
    }
    while (--retries);

    return 1;
}


static int
hdc_ready(int drive, int head)
{
    int status, retries = 0xC00;

    do
    {
	if (hdc_busy())
	    break;

	outb(HDPORT_DRVHD, (unsigned char)DRIVE_HEAD(drive, head));
	io_delay(4);
	status = inb(HDPORT_STATUS);

	if (status & (HDSTAT_READY | HDSTAT_SEEKOK))
	    return 1;
    }
    while (--retries);

    return 0;
}


static int
hdc_do_send_command(unsigned char cmd,
		    int drive, int cylinder, int head,
		    int starting_sector, int sector_count)
{
    int retries;

    if (hdd[drive].lbaf > 1)
    {
	/* Recompute the cylinder and head numbers if using LBA.  */
	cylinder = cylinder * hdd[drive].lbaf + head / 16;
	head %= 16;
    }

    hdc_command  = cmd;
    hdc_drive    = drive;

    disable();

    for (retries = 0; retries < MAX_SEND_CMD_RETRIES; retries++)
	if (hdc_ready(drive, head))
	{
	    outb_delay(HDC_CONTROL,    hdd[drive].dso & 8);
	    outb_delay(HDPORT_SECTORS, (unsigned char)sector_count);
	    outb_delay(HDPORT_SECTOR,  (unsigned char)starting_sector);
	    outb_delay(HDPORT_LOWCYL,  (unsigned char)cylinder);
	    outb_delay(HDPORT_HICYL,   (unsigned char)(cylinder >> 8));
	    outb_delay(HDPORT_DRVHD,   (unsigned char)DRIVE_HEAD(drive, head));
	    outb_delay(HDPORT_WPCOMP,  hdd[drive].wp_cylinder >> 2);
	    outb_delay(HDPORT_CMD,     cmd);
	    break;
	}
	else
	{
	    printk("HDC: can't send command %w [d: %d/%d/%d (%d)]\n",
		   cmd, drive, cylinder, head, starting_sector, sector_count);
	    hdc_need_reset = 1;
	}

    return (retries == MAX_SEND_CMD_RETRIES) ? 0 : 1;
}


static int
hdc_send_command(unsigned char cmd,
		 int drive, int cylinder, int head,
		 int starting_sector, int sector_count)
{
    int hdc_cmd_ok, retries;

    for (retries = 0; retries < MAX_COMMAND_RETRIES; retries++)
    {
	hdc_cmd_ok = hdc_do_send_command(cmd, drive, cylinder, head,
					 starting_sector, sector_count);

	if (hdc_cmd_ok)
	    break;

	/* Can't send command to controller, reset it.  */
	hdc_reset();
    }

    return hdc_cmd_ok;
}


static int
hdc_send_specs(int drive)
{
    int hdc_cmd_ok;

    hdc_cmd_ok = hdc_do_send_command(HDCMD_SPECS, drive, hdd[drive].cylinders,
				     hdd[drive].heads - 1, hdd[drive].sectors,
				     hdd[drive].sectors);

    if (hdc_cmd_ok && !wait_hdc_interrupt())
	return 1;

    printk("HDC: can't send specs to drive %d\n", drive);
    return 0;
}


static int
hdc_recalibrate(int drive)
{
    int hdc_cmd_ok;

    printk("HDC: recalibrating drive %d...\n", drive);

    hdc_cmd_ok = hdc_do_send_command(HDCMD_RESTORE, drive, 0, 0, 1,
				     hdd[drive].sectors);

    if (hdc_cmd_ok && !wait_hdc_interrupt())
	return 1;

    printk("HDC: can't recalibrate drive %d\n", drive);
    return 0;
}


#if 0
/*
 * Used only for tests, not really needed...
 */

static int
hdc_seek(int drive, int cylinder)
{
    int hdc_cmd_ok;

    DEBUG(9, "HDC: (%d,%d)\n", drive, cylinder);

    hdc_cmd_ok = hdc_send_command(HDCMD_SEEK, drive, cylinder, 0, 1,
				  hdd[drive].sectors);

    if (hdc_cmd_ok && !wait_hdc_interrupt())
	return 1;

    printk("HDC: seek timeout\n");
    hdc_reset();
    return 0;
}
#endif


static int
hdc_do_reset(void)
{
    int err, drive;


    hdc_ticks            = 0;
    hdc_timeout          = 0;
    hdc_need_reset       = 1;

    disable();

    outb(HDC_CONTROL, 0x04);
    io_delay(0x26);
    outb(HDC_CONTROL, 0x00);

    printk("HDC: reseting...\n");

    for (err = 0; err < 32766; err++);

    if (hdc_busy())
    {
	printk("HDC: reset failed, controller busy\n");
	return 0;
    }

    if ((err = inb(HDPORT_ERROR)) != 1)
    {
	printk("HDC: err = %x.\n", err);
	return 0;
    }

    for (err = drive = 0; drive < hdd_count; drive++)
    {
	if (!hdc_recalibrate(drive))
	    err = 1;

	if (!hdc_send_specs(drive))
	    err = 1;
    }

    if (err)
	return 0;

    printk("HDC: reset ok.\n");

    hdc_need_reset = 0;
    return 1;
}


static int
hdc_reset(void)
{
    int retries;

    for (retries = 0; retries < MAX_RESET_RETRIES; retries++)
	if (hdc_do_reset())
	    break;

    if (retries == MAX_RESET_RETRIES)
    {
	printk("HDC: can't reset controller, too many retries\n");
	return 0;
    }

    return 1;
}


static inline int
hdc_get_first_block(int minor)
{
    return (minor < 2) ? 0 : hdd_partition[minor - 2].first_block;
}


static inline int
hdc_get_total_blocks(int minor)
{
    return (minor < 2) ? hdd[minor].total_sectors :
			 hdd_partition[minor - 2].total_blocks;
}


static inline int
hdc_get_device_no(int minor)
{
    if ((minor == 0) || (2 <= minor && minor <= 5))
	return 0;

    if ((minor == 1) || (6 <= minor && minor <= 9))
	return 1;

    PANIC("HDC: bad minor no %d", minor);
    return -1;
}


int
hdc_open(int minor, int flags)
{
    int device;

    DEBUG(9, "(%d/%d)\n", HDC_MAJOR, minor);

    device = hdc_get_device_no(minor);

    if (device == -1 || !hdd[device].present)
    {
	DEBUG(9, "no such device\n");
	return -ENXIO;
    }

    if (hdc_get_total_blocks(minor) == 0)
    {
	DEBUG(9, "invalid device\n");
	return -ENXIO;
    }

    down(&hdc_oc_mutex, WAIT_DEVICE_BUSY);

    hdc_minor_info[minor].in_use = 1;

    up(&hdc_oc_mutex);

    return 0;
}


int
hdc_close(int minor)
{
    DEBUG(9, "(%d/%d)\n", HDC_MAJOR, minor);

    down(&hdc_oc_mutex, WAIT_DEVICE_BUSY);

    /* It should never enter on the 'true' branch of this 'if'.
       Maybe this should be removed.  */

    if (!hdc_minor_info[minor].in_use)
    {
	printk("HDC: trying to close unused device %d\n", minor);
	up(&hdc_oc_mutex);
	return -1;
    }

    hdc_minor_info[minor].in_use = 0;

    up(&hdc_oc_mutex);

    return 0;
}


int
hdc_read(int minor, blk_request *br)
{
    int tpc;                            /* Tracks  per cylinder.  */
    int spt;                            /* Sectors per track.  */
    int spb;                            /* Sectors per file system block.  */
    int device, ccyl, ctrack, csector;
    int retries, total_good_sectors = 0;
    int block, cblock, nblocks, cnblocks, first_block, total_blocks;


    down(&hdc_rw_mutex, WAIT_DEVICE_BUSY);

    if (hdc_need_reset && !hdc_reset())
	return 0;

    first_block  = hdc_get_first_block(minor);
    total_blocks = hdc_get_total_blocks(minor);
    device       = hdc_get_device_no(minor);

    spb     = br->blksz / HDC_BLKSZ;
    block   = br->block   * spb + first_block;
    nblocks = br->nblocks * spb;

    /* Check the request.  */

    if (block   <  0 || block   >= (first_block + total_blocks) ||
	nblocks <= 0 || nblocks >  (              total_blocks) ||
	block + nblocks         >  (first_block + total_blocks))
	PANIC("%s (cmd=%c dev=%d blk=%d nblk=%d totblk=%d buf=%d)",
	      ERR_BADREQ, 'r', device, block, nblocks,
	      total_blocks, br->buf_no);

    tpc = hdd[device].heads;
    spt = hdd[device].sectors;

    hdc_spb       = spb;
    hdc_buf_index = 0;
    hdc_buf_no    = br->buf_no;

    for (cblock = block; nblocks; cblock += cnblocks, nblocks -= cnblocks)
    {
	ccyl     = cblock / (tpc * spt);
	ctrack   = (cblock % (tpc * spt)) / spt;
	csector  = cblock % spt;
	cnblocks = min(nblocks, spt - csector);

	DEBUG(9, "HDC: cmd=%c drv=%d cyl=%d track=%d sect=%d nblks=%d\n",
	      'r', device, ccyl, ctrack, csector + 1, cnblocks);

	retries = MAX_READ_RETRIES;
	hdc_interrupts_left = cnblocks;

      retry:

	if (hdc_send_command(HDCMD_READ, device, ccyl, ctrack,
			     csector + 1 + cnblocks - hdc_interrupts_left,
			     hdc_interrupts_left))
	    if (wait_hdc_interrupt())
	    {
		hdc_reset();

		if (--retries)
		    goto retry;

		printk("HDC: cmd=%c drv=%d cyl=%d head=%d sect=%d\n",
		       'r', device, ccyl, ctrack,
		       csector + 1 + cnblocks - hdc_interrupts_left);
	    }

	total_good_sectors += cnblocks - hdc_interrupts_left;

	if (hdc_interrupts_left)
	    break;
    }

    up(&hdc_rw_mutex);

    return total_good_sectors * HDC_BLKSZ;
}


int
hdc_write(int minor, blk_request *br)
{
    int tpc;                            /* Tracks  per cylinder.  */
    int spt;                            /* Sectors per track.  */
    int spb;                            /* Sectors per file system block.  */
    unsigned short *address = NULL;
    int device, ccyl, ctrack, csector;
    int retries, total_good_sectors = 0;
    int block, cblock, nblocks, cnblocks, first_block, total_blocks;


    down(&hdc_rw_mutex, WAIT_DEVICE_BUSY);

    if (hdc_need_reset && !hdc_reset())
	return 0;

    first_block  = hdc_get_first_block(minor);
    total_blocks = hdc_get_total_blocks(minor);
    device       = hdc_get_device_no(minor);

    spb     = br->blksz / HDC_BLKSZ;
    block   = br->block   * spb + first_block;
    nblocks = br->nblocks * spb;

    /* Check the request.  */

    if (block   <  0 || block   >= (first_block + total_blocks) ||
	nblocks <= 0 || nblocks >  (              total_blocks) ||
	block + nblocks         >  (first_block + total_blocks))
	PANIC("%s (cmd=%c dev=%d blk=%d nblk=%d totblk=%d buf=%d)",
	      ERR_BADREQ, 'w', device, block, nblocks,
	      total_blocks, br->buf_no);

    tpc = hdd[device].heads;
    spt = hdd[device].sectors;

    hdc_buf_index = 0;
    hdc_buf_no    = br->buf_no;

    for (cblock = block; nblocks; cblock += cnblocks, nblocks -= cnblocks)
    {
	ccyl     = cblock / (tpc * spt);
	ctrack   = (cblock % (tpc * spt)) / spt;
	csector  = cblock % spt;
	cnblocks = min(nblocks, spt - csector);

	DEBUG(9, "HDC: cmd=%c drv=%d cyl=%d track=%d sector=%d nblocks=%d\n",
	      'w', device, ccyl, ctrack, csector + 1, cnblocks);

	retries = MAX_WRITE_RETRIES;
	hdc_interrupts_left = cnblocks;

      retry:

	/* HDD PROTECTION TEST.  THIS WILL PANIC THE KERNEL AT THE
	   FIRST ATTEMPT TO WRITE ON THE HARD DISK IN THE WRONG PLACE.
	   THE EXAMPLE REFFERS TO MY THIX PARTITION WHICH IS BETWEEN
	   CYLINDER 398 AND CYLINDER 430 ON THE PRIMARY MASTER HARD
	   DISK.  CHANGE THIS AS APPROPRIATE.  */

	if (ccyl < 570 || ccyl > 602 || device != 0)
	{
	    printk("HDC: cmd=%c drv=%d cyl=%d track=%d sect=%d nblocks=%d\n",
		   'w', device, ccyl, ctrack, csector + 1, cnblocks);
	    PANIC("WRITE ATTEMPT AT CYLINDER %d NOT PERMITTED!!!\n", ccyl);
	}

	if (hdc_send_command(HDCMD_WRITE, device, ccyl, ctrack,
			     csector + 1 + cnblocks - hdc_interrupts_left,
			     hdc_interrupts_left))
	    for (; hdc_interrupts_left; hdc_interrupts_left--, hdc_buf_index++)
	    {
		if (!hdc_data_request())
		    break;

		if (hdc_buf_index == 0)
		    address = (unsigned short *)buf_address(hdc_buf_no);

		outsw(HDPORT_DATA, address + (hdc_buf_index * HDC_BLKSZ) / 2,
		      HDC_BLKSZ / 2);

		if (wait_hdc_interrupt() ||
		    ((inb(HDPORT_STATUS) & HDC_STATUS_MASK) != HDC_READY_MASK))
		{
		    hdc_reset();

		    if (--retries)
			goto retry;

		    printk("HDC: cmd=%c drv=%d cyl=%d head=%d sect=%d\n",
			   'r', device, ccyl, ctrack,
			   csector + 1 + cnblocks - hdc_interrupts_left);
		    break;
		}

		if (hdc_buf_index == spb - 1)
		{
		    hdc_buf_no = buf_vect[hdc_buf_no].next;
		    hdc_buf_index = -1;
		}
	    }

	total_good_sectors += cnblocks - hdc_interrupts_left;

	if (hdc_interrupts_left)
	    break;
    }

    up(&hdc_rw_mutex);

    return total_good_sectors * HDC_BLKSZ;
}


int
hdc_ioctl(int minor, int cmd, void *argp)
{
    switch (cmd)
    {
	case IOCTL_GETBLKSZ:      /* Get the device block size.  */
	    *(int *)argp = HDC_BLKSZ;
	    break;

	case IOCTL_GETDEVSZ:      /* Get the device size (in # of blocks).  */
	    *(int *)argp = hdc_get_total_blocks(minor);
	    break;

	default:
	    return -EINVAL;
    }

    return 0;
}


int
hdc_lseek(int minor, __off_t offset)
{
    if (minor < hdc_driver.minors)
	return offset;

    return -ESPIPE;
}


/*
 * Initialize the driver data structures.  Also figure out the number
 * of drives and read the partition  tables.  If  everything  is  ok,
 * register the driver.
 */

int
hdc_init(void)
{
    int heads;
    int registered;
    unsigned char cmos_data;


    memset(hdd, 0, sizeof(hdd));
    memset(hdd_partition, 0, sizeof(hdd_partition));

    if ((cmos_data = read_cmos(0x12)))
    {
	printk("HDC: first  IDE hard disk: ");

	if (cmos_data & 0xf0)
	{
	    hdd[0].cylinders     = *(unsigned short *)0xC20;
	    hdd[0].heads         = *(unsigned char  *)0xC22;
	    hdd[0].wp_cylinder   = *(unsigned short *)0xC25;
	    hdd[0].dso           = *(unsigned char  *)0xC28;
	    hdd[0].sectors       = *(unsigned char  *)0xC2E;
	    hdd[0].total_sectors = hdd[0].cylinders *
				   hdd[0].heads     *
				   hdd[0].sectors;

	    printk("type %d ", ((cmos_data & 0xf0) == 0xf0) ?
			       read_cmos(0x19) : (cmos_data >> 4));
	    printk("(%d/%d/%d)",
		   hdd[0].cylinders, hdd[0].heads, hdd[0].sectors);

	    if (hdd[0].heads > 16 && hdd[0].heads % 2)
	    {
		printk("\nHDC: WARNING: invalid number of heads, disabled\n");
	    }
	    else
	    {
		for (heads = hdd[0].heads; heads > 16; heads /= 2);
		hdd[0].lbaf = hdd[0].heads / heads;

		if (hdd[0].lbaf > 1)
		    printk(" LBA (geometry: %d/%d/%d)\n",
			   hdd[0].cylinders * hdd[0].lbaf,
			   hdd[0].heads     / hdd[0].lbaf,
			   hdd[0].sectors);
		else
		    printk("\n");

		hdd_count++;
		hdd[0].present = 1;
	    }
	}
	else
	    printk("not installed\n");


	printk("HDC: second IDE hard disk: ");

	if (cmos_data & 0x0f)
	{
	    hdd[1].cylinders     = *(unsigned short *)0xC30;
	    hdd[1].heads         = *(unsigned char  *)0xC32;
	    hdd[1].wp_cylinder   = *(unsigned short *)0xC35;
	    hdd[1].dso           = *(unsigned char  *)0xC38;
	    hdd[1].sectors       = *(unsigned char  *)0xC3E;
	    hdd[1].total_sectors = hdd[1].cylinders *
				   hdd[1].heads     *
				   hdd[1].sectors;

	    printk("type %d ", ((cmos_data & 0x0f) == 0x0f) ?
			       read_cmos(0x1a) : (cmos_data & 0x0f));
	    printk("(%d/%d/%d)",
		   hdd[1].cylinders, hdd[1].heads, hdd[1].sectors);

	    if (hdd[1].heads > 16 && hdd[1].heads % 2)
	    {
		printk("\nHDC: WARNING: invalid number of heads, disabled\n");
	    }
	    else
	    {
		for (heads = hdd[1].heads; heads > 16; heads /= 2);
		hdd[1].lbaf = hdd[1].heads / heads;

		if (hdd[1].lbaf > 1)
		    printk(" LBA (geometry: %d/%d/%d)\n",
			   hdd[1].cylinders * hdd[1].lbaf,
			   hdd[1].heads     / hdd[1].lbaf,
			   hdd[1].sectors);
		else
		    printk("\n");

		hdd_count++;
		hdd[1].present = 1;
	    }
	}
	else
	    printk("not installed\n");

	registered = register_driver((generic_driver *)&hdc_driver);

	if (!registered)
	    return 0;

	if (hdd[0].present)
	    partitions_setup(&hdc_driver, makedev(HDC_MAJOR, 0),
			     &hdd_partition[0], hdd[0].cylinders,
			     hdd[0].heads, hdd[0].sectors);

	if (hdd[1].present)
	    partitions_setup(&hdc_driver, makedev(HDC_MAJOR, 1),
			     &hdd_partition[4], hdd[1].cylinders,
			     hdd[1].heads, hdd[1].sectors);

	{
	    int i;

	    if (hdd[0].present)
	    {
		printk("HDC: hdd 0: ");

		for (i = 2; i < 6; i++)
		    printk("[%d,%d] ", hdd_partition[i-2].first_block,
				       hdd_partition[i-2].total_blocks);
		printk("\n");
	    }

	    if (hdd[1].present)
	    {
		printk("HDC: hdd 1: ");

		for (i = 6; i < 10; i++)
		    printk("[%d,%d] ", hdd_partition[i-2].first_block,
				       hdd_partition[i-2].total_blocks);
		printk("\n");
	    }
	}

	return 1;
    }

    return 0;
}
