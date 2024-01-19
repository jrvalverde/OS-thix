/* fdc.c -- The floppy disk driver.  */

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

/*
 * Some help from Andrei Pitis.
 */


#include <thix/klib.h>
#include <thix/vm.h>
#include <thix/errno.h>
#include <thix/string.h>
#include <thix/sched.h>
#include <thix/buffer.h>
#include <thix/sleep.h>
#include <thix/semaphore.h>
#include <thix/irq.h>
#include <thix/fdc.h>
#include <thix/ioctl.h>
#include <thix/gendrv.h>
#include <thix/arch/pc/dma.h>
#include <thix/arch/pc/cmos.h>
#include <thix/proc/i386.h>


#define ERR_BADREQ      error[0]

static char *error[] =
{
    "bad request",
};


#define FLOPPY_OUTPUT                   0x3f2

#define FLOPPY_CONTROLLER_ENABLE        0x04
#define FLOPPY_DMAINT_ENABLE            0x08

#define FLOPPY_STATUS                   0x3f4

#define FLOPPY_CONTROLLER_BUSY          0x10
#define FLOPPY_DMA_MODE                 0x20
#define FLOPPY_DIRECTION                0x40
#define FLOPPY_MASTER                   0x80
#define FLOPPY_NEW_BYTE                 (FLOPPY_MASTER         |   \
					 FLOPPY_DIRECTION      |   \
					 FLOPPY_CONTROLLER_BUSY)

#define FLOPPY_COMMAND                  0x3f5

#define FLOPPY_READ                     0xe6
#define FLOPPY_WRITE                    0xc5
#define FLOPPY_FORMAT                   0x4d
#define FLOPPY_RECALIBRATE              0x07
#define FLOPPY_SEEK                     0x0f
#define FLOPPY_SENSE                    0x08
#define FLOPPY_SPECIFY                  0x03

#define FLOPPY_RATE                     0x3f7
#define FLOPPY_INPUT                    0x3f7

#define FLOPPY_CHANGE_LINE              0x80
#define FLOPPY_CHANNEL                  0x02
#define FLOPPY_DTL                      0xFF

#define FLOPPY_SPEC2                    0x06

#define MAX_FLOPPY_RESULTS              0x07
#define MAX_FLOPPY_ERRORS               0x08

#define FLOPPY_TYPES                    0x02

#define TEST_BITS                       0xf8
#define SEEK_OK                         0x20
#define TRANSFER_OK                     0x00
#define WRITE_PROTECTED                 0x02

#define INVALID_TRACK                   -1

#define FDC_BLKSZ       		512

#define MAX_FDCS			2


/*
 * fdc_buf is hard coded because the DMA needs it between 64k boundaries.
 * See vm_init() in mm.c for details.
 */

static char *fdc_buf = (char *)0x9a800;


static int  fdc_getresults(void);
static void fdc_out(unsigned char data);
static void fdc_surprise(void);


typedef struct
{
    int total_sectors;
    int tracks;
    int sectors;
    int sectorsize;
    int trackstep;
    int rate;
    int gap;
    int spec1;
} struct_fdd_type;


typedef struct
{
    int present;
    int calibrated;
    int motor;
    int current_track;
    int type;
} struct_fdd;


static int fdc_cmd;
static int fdc_drive;
static int fdc_track;
static int fdc_head;
static int fdc_sector;
static int fdc_nsects;


static semaphore fdc_oc_mutex = 1;
static semaphore fdc_rw_mutex = 1;


static volatile int fdc_ticks                  = 0;
static volatile int fdc_timeout                = 0;
static volatile int fdc_need_reset             = 0;
static volatile int fdc_waits_interrupt        = 0;
static volatile int fdc_ticks_till_motor_stops = 0;


static char *fdd_drive_name[5] =
{
    "not installed",
    "360K (not supported)",
    "1.2M",
    "720K (not supported)",
    "1.44M"
};


static struct_fdd_type fdd_type[FLOPPY_TYPES] =
{
    { 80*15*2, 80, 15, 2, 0, 0, 0x1B, 0xDF },           /* 1.2M   */
    { 80*18*2, 80, 18, 2, 0, 0, 0x1B, 0xCF },           /* 1.44M  */
};

static struct_fdd_type *current_fdd_type;


static struct_fdd fdd[2] =
{
    { 0, 0, 0, INVALID_TRACK, 0 },
    { 0, 0, 0, INVALID_TRACK, 0 }
};

static struct_fdd *current_fdd;


static unsigned char fdc_results[MAX_FLOPPY_RESULTS];
static unsigned char current_OUTPUT;


static minor_info fdc_minor_info[2] =
{
    { 0, 0, 1 },
    { 0, 0, 1 },
};


static block_driver fdc_driver =
{
    "floppy disk",
    fdc_open,
    fdc_close,
    fdc_read,
    fdc_write,
    NULL,
    fdc_ioctl,
    NULL,
    fdc_lseek,
    fdc_timer,
    FDC_MAJOR,
    FDC_IRQ,
    BLK_DRV,
    2,
    fdc_minor_info,
};


/*
 * Initialize the DMA floppy channel.
 */

static void
dma_init(char *address, int count)
{
    disable_dma_channel(FLOPPY_CHANNEL);
    clear_dma_channel_flipflop(FLOPPY_CHANNEL);
    set_dma_channel_mode(FLOPPY_CHANNEL,
			 (fdc_cmd == FLOPPY_READ) ? DMA_READ_MODE :
						    DMA_WRITE_MODE);
    set_dma_channel_count(FLOPPY_CHANNEL, count);
    set_dma_channel_address(FLOPPY_CHANNEL, address);
    set_dma_channel_page(FLOPPY_CHANNEL, (unsigned)address >> 16);
    enable_dma_channel(FLOPPY_CHANNEL);
}


/*
 * An unexpected interrupted occured.  Force a controller reset.
 */

static void
fdc_surprise(void)
{
/*    printk("FDC: surprise\n");*/
    fdc_need_reset = 1;
}


/*
 * The floppy interrupt handler.  Call fdc_surprise() if this interrupt
 * was not expected.  wakeup() the process sleeping on  &fdc_interrupt,
 * if any.
 */

void
fdc_interrupt(void)
{
    fdc_timeout = 0;

    if (!fdc_waits_interrupt)
	fdc_surprise();

    fdc_waits_interrupt = 0;
    wakeup(&fdc_interrupt);
}


static void
fdc_start_motor(void)
{
    int i;

    if (current_fdd->motor) return;
    current_OUTPUT = FLOPPY_CONTROLLER_ENABLE | FLOPPY_DMAINT_ENABLE |
		     fdc_drive | (16 << fdc_drive);
    outb_delay(FLOPPY_OUTPUT, current_OUTPUT);
    current_fdd->motor = 1;
    fdd[!fdc_drive].motor = 0;
    for (i = 0; i < 500000; i++);
}


static void
fdc_stop_motor(void)
{
    outb_delay(FLOPPY_OUTPUT,
	 current_OUTPUT = FLOPPY_CONTROLLER_ENABLE | FLOPPY_DMAINT_ENABLE |
			  fdc_drive);

    fdd[0].motor = fdd[1].motor = 0;
}


/*
 * The floppy timer.  Called in order to stop the floppy motor if
 * there has been no floppy access in the past two seconds.
 */

void
fdc_timer(void)
{
    if (fdc_waits_interrupt && ++fdc_ticks > HZ * 2)
    {
	fdc_waits_interrupt = 0;
	fdc_timeout = 1;
	wakeup(&fdc_interrupt);
    }
    else
	if ((fdd[0].motor | fdd[1].motor) &&
	    fdc_rw_mutex && !--fdc_ticks_till_motor_stops)
	    fdc_stop_motor();
}


/*
 * Wait for an interrupt.  Force a reset on timeout.
 */

static int
wait_fdc_interrupt(void)
{
    fdc_ticks = 0;
    fdc_waits_interrupt = 1;
    sleep(&fdc_interrupt, WAIT_DEVICE_IO);

    if (fdc_timeout)
    {
	fdc_need_reset = 1;
	printk("FDC: drive %d timeout\n", fdc_drive);
    }

    return fdc_timeout;
}


/*
 * Read the results from the floppy controller.
 */

static int
fdc_getresults(void)
{
    int i, results;
    unsigned char status;

    if (fdc_need_reset) return 0;

    for (results = i = 0; i < 30000; i++)
    {
	status = inb_delay(FLOPPY_STATUS) & FLOPPY_NEW_BYTE;
	if (status == FLOPPY_MASTER) return 1;
	if (status != FLOPPY_NEW_BYTE) continue;
	if (results == MAX_FLOPPY_RESULTS) break;
	fdc_results[results++] = inb_delay(FLOPPY_COMMAND);
	DEBUG(9, "%w ", fdc_results[results - 1]);
    }

    printk("FDC: reply error\n");
    fdc_need_reset = 1;
    return 0;
}


/*
 * Send a byte to the floppy controller.
 */

static void
fdc_out(unsigned char data)
{
    int i;
    unsigned char status;

    for (i = 0; i < 10000; i++)
    {
	status = inb_delay(FLOPPY_STATUS) & (FLOPPY_MASTER | FLOPPY_DIRECTION);
	if (status != FLOPPY_MASTER) continue;
	outb_delay(FLOPPY_COMMAND, data);
	return;
    }

    printk("FDC: can't send byte %w to controller\n", data);
    fdc_need_reset = 1;
}


static void
fdc_mode(void)
{
    fdc_out(FLOPPY_SPECIFY);
    fdc_out(current_fdd_type->spec1);
    fdc_out(FLOPPY_SPEC2);
    outb_delay(FLOPPY_RATE, current_fdd_type->rate & ~0x40);
}


static void
fdc_reset(void)
{
    int i;

    DEBUG(1, "FDC: reseting...\n");

    disable();
    outb_delay(FLOPPY_OUTPUT, FLOPPY_DMAINT_ENABLE);

    for(i = 0; i < 10000; i++) __asm__("nop");

    outb_delay(FLOPPY_OUTPUT,
	 current_OUTPUT = FLOPPY_CONTROLLER_ENABLE | FLOPPY_DMAINT_ENABLE);

    fdc_need_reset = 0;

    fdd[0].calibrated = fdd[1].calibrated = 0;
    fdd[0].motor      = fdd[1].motor      = 0;

    if (wait_fdc_interrupt())
	printk("FDC: can't reset controller (timeout)\n");

    fdc_out(FLOPPY_SENSE);

    if (!fdc_getresults())
	printk("FDC: can't reset controller\n");
}


static int
fdc_recalibrate(void)
{
    if (fdc_need_reset)
	return 0;

    DEBUG(1, "FDC: recalibrating drive %d...\n", fdc_drive);

    disable();
    fdc_start_motor();
    fdc_out(FLOPPY_RECALIBRATE);
    fdc_out(fdc_drive);

    if (fdc_need_reset)
	return 0;

    if (wait_fdc_interrupt())
	return 0;

    fdc_out(FLOPPY_SENSE);

    if (!fdc_getresults())
	goto bad_recalibration;

    if ((fdc_results[0] & TEST_BITS) != SEEK_OK || fdc_results[1])
	goto bad_recalibration;

    current_fdd->current_track = INVALID_TRACK;
    return current_fdd->calibrated = 1;

  bad_recalibration:

    printk("FDC: can't recalibrate\n");
    fdc_need_reset = 1;
    return 0;
}


static int
fdc_seek(void)
{
    if (fdc_need_reset)
	return 0;

    if (!current_fdd->calibrated)
	if (!fdc_recalibrate())
	    return 0;

    if (fdc_track == current_fdd->current_track)
	return 1;

    disable();

    if (!current_fdd->motor)
	return 0;

    fdc_out(FLOPPY_SEEK);
    fdc_out(fdc_head << 2 | fdc_drive);
    fdc_out(fdc_track);

    if (fdc_need_reset)
	return 0;

    if (wait_fdc_interrupt())
	return 0;

    current_fdd->current_track = fdc_track;
    fdc_out(FLOPPY_SENSE);

    if (!fdc_getresults())
	return 0;

    if ((fdc_results[0] & TEST_BITS) != SEEK_OK ||
	fdc_results[1] != fdc_track * (current_fdd_type->trackstep + 1))
	return 0;

    return 1;
}


/*
 * Perform the actual transfer.
 */

static int
fdc_transfer(void)
{
    int sectors;

    disable();

    if (fdc_need_reset || !current_fdd->motor || !current_fdd->calibrated)
	return 0;

    dma_init(fdc_buf +
	     ((fdc_head * current_fdd_type->sectors + fdc_sector - 1) << 9),
	     fdc_nsects * (1 << (current_fdd_type->sectorsize + 7)));

    fdc_mode();
    fdc_out(fdc_cmd);
    fdc_out(fdc_head << 2 | fdc_drive);
    fdc_out(fdc_track);
    fdc_out(fdc_head);
    fdc_out(fdc_sector);
    fdc_out(current_fdd_type->sectorsize);      /* 2 = 512 bytes/sector.  */
    fdc_out(current_fdd_type->sectors);         /* End of track.  */
    fdc_out(current_fdd_type->gap);             /* Gap length.  */
    fdc_out(FLOPPY_DTL);                        /* Data length.  */

    if (fdc_need_reset)
	return 0;

    if (wait_fdc_interrupt())
	return 0;

    if (!fdc_getresults())
	return 0;

    if (fdc_cmd == FLOPPY_WRITE && fdc_results[1] & WRITE_PROTECTED)
    {
	fdc_out(FLOPPY_SENSE);
	fdc_getresults();
	return -1;
    }

    if ((fdc_results[0] & TEST_BITS) != TRANSFER_OK ||
	fdc_results[1] || fdc_results[2])
	goto bad_transfer;

    sectors = (fdc_results[3] - fdc_track) * (current_fdd_type->sectors << 1) +
	      (fdc_results[4] - fdc_head) * current_fdd_type->sectors +
	       fdc_results[5] - fdc_sector;

    if (sectors == fdc_nsects)
	return 1;

  bad_transfer:

    current_fdd->calibrated = 0;
    return 0;
}


static int
fdc_command(int cmd, int drive, int track, int sector, int nsects)
{
    int err;

    fdc_cmd    = cmd;
    fdc_drive  = drive;
    fdc_track  = track;
    fdc_head   = sector / current_fdd_type->sectors;
    fdc_sector = sector % current_fdd_type->sectors + 1;
    fdc_nsects = nsects;

    fdc_ticks_till_motor_stops = 3 * HZ;

    for (err = 0; err < MAX_FLOPPY_ERRORS; err++)
    {
	if (fdc_need_reset)
	    fdc_reset();

	fdc_start_motor();

	if (!fdc_seek())
	{
	    printk("FDC: seek error on drive %d\n", fdc_drive);
	    continue;
	}

	switch (fdc_transfer())
	{
	    case -1:    printk("FDC: disk in drive %d is write protected\n",
			       fdc_drive);
			return 0;
	    case  0:    continue;
	    case  1:    return 1;
	}
    }
    return 0;
}


int
fdc_open(int minor, int flags)
{
    DEBUG(9, "(%d/%d)\n", FDC_MAJOR, minor);

    if (minor < 0 || minor >= MAX_FDCS || !fdd[minor].present)
    {
	DEBUG(9, "no such device\n");
	return -ENXIO;
    }

    down(&fdc_oc_mutex, WAIT_DEVICE_BUSY);

    if (!fdc_minor_info[minor].in_use)
    {
	fdd[minor].calibrated    = 0;
	fdd[minor].current_track = INVALID_TRACK;
	fdd[minor].motor         = 0;
	fdc_minor_info[minor].in_use = 1;
    }

    up(&fdc_oc_mutex);

    return 0;
}


int
fdc_close(int minor)
{
    DEBUG(9, "(%d/%d)\n", FDC_MAJOR, minor);

    down(&fdc_oc_mutex, WAIT_DEVICE_BUSY);

    /* It should never enter on the 'true' branch of this 'if'.
       Maybe this should be removed.  */

    if (!fdc_minor_info[minor].in_use)
    {
	printk("FDC: trying to close unused device %d\n", minor);
	up(&fdc_oc_mutex);
	return -EBUSY;
    }

    fdc_minor_info[minor].in_use = 0;

    up(&fdc_oc_mutex);

    return 0;
}


int
fdc_read(int minor, blk_request *br)
{
    int spt;                            /* Sectors per track.  */
    int spb;                            /* Sectors per file system block.  */
    int i, buf_no;
    unsigned address = 0, index = 0;
    int device = minor, ctrack, csector;
    int block, cblock, nblocks, cnblocks;
    int good_sectors, total_good_sectors = 0;


    down(&fdc_rw_mutex, WAIT_DEVICE_BUSY);

    current_fdd      = &fdd[device];
    current_fdd_type = &fdd_type[current_fdd->type];

    spb     = br->blksz / FDC_BLKSZ;
    block   = br->block   * spb;
    nblocks = br->nblocks * spb;


    /* Check the request.  */

    if (block   <  0 || block   >= current_fdd_type->total_sectors ||
	nblocks <= 0 || nblocks >  current_fdd_type->total_sectors ||
	block + nblocks         >  current_fdd_type->total_sectors)
	PANIC("%s (cmd=r dev=%d blk=%d nblk=%d buf=%d)\n",
	      ERR_BADREQ, device, br->block, br->nblocks, br->buf_no);

    spt = current_fdd_type->sectors << 1;
    buf_no = br->buf_no;

    DEBUG(9, "(cmd=r dev=%d blk=%d nblk=%d buf=%d)\n",
	  device, br->block, br->nblocks, br->buf_no);

    for (cblock = block; nblocks; cblock += cnblocks, nblocks -= cnblocks)
    {
	ctrack   = cblock / spt;
	csector  = cblock % spt;
	cnblocks = min(nblocks, spt - csector);

	if (fdc_command(FLOPPY_READ, device, ctrack, csector, cnblocks))
	    good_sectors = cnblocks;
	else
	    for (good_sectors = i = 0; i < cnblocks; good_sectors++, i++)
	    {
		printk("FDC: reading one sector at a time (%d/%z/%z)...\n",
		       device, ctrack, csector + i);
		if (fdc_command(FLOPPY_READ, device, ctrack, csector+i,1) == 0)
		    break;
	    }

	for (i = 0; i < cnblocks; index++, i++)
	{
	    if (i < good_sectors)
	    {
		if (index == 0)
		    address = (unsigned)buf_address(buf_no);
		memcpy((void *)(address + (index << 9)),
		       &fdc_buf[(csector + i) << 9], FDC_BLKSZ);
		buf_vect[buf_no].valid = 1;
	    }
	    else
		buf_vect[buf_no].valid = 0;

	    if (index == spb - 1)
	    {
		buf_no = buf_vect[buf_no].next;
		index = -1;
	    }
	}

	total_good_sectors += good_sectors;
	if (good_sectors != cnblocks)
	    break;
    }

    up(&fdc_rw_mutex);

    return total_good_sectors * FDC_BLKSZ;
}


int
fdc_write(int minor, blk_request *br)
{
    int spt;                            /* Sectors per track.  */
    int spb;                            /* Sectors per file system block.  */
    int i, buf_no;
    unsigned address = 0, index = 0;
    int device = minor, ctrack, csector;
    int block, cblock, nblocks, cnblocks;
    int good_sectors, total_good_sectors = 0;


    down(&fdc_rw_mutex, WAIT_DEVICE_BUSY);

    current_fdd      = &fdd[device];
    current_fdd_type = &fdd_type[current_fdd->type];

    spb     = br->blksz / FDC_BLKSZ;
    block   = br->block   * spb;
    nblocks = br->nblocks * spb;


    /* Check the request.  */

    if (block   <  0 || block   >= current_fdd_type->total_sectors ||
	nblocks <= 0 || nblocks >  current_fdd_type->total_sectors ||
	block + nblocks         >  current_fdd_type->total_sectors)
	PANIC("%s (cmd=w dev=%d blk=%d nblk=%d buf=%d)\n",
	      ERR_BADREQ, device, br->block, br->nblocks, br->buf_no);

    spt = current_fdd_type->sectors << 1;
    buf_no = br->buf_no;

    DEBUG(9, "(cmd=w dev=%d blk=%d nblk=%d buf=%d)\n",
	  device, br->block, br->nblocks, br->buf_no);

    for (cblock = block; nblocks; cblock += cnblocks, nblocks -= cnblocks)
    {
	ctrack   = cblock / spt;
	csector  = cblock % spt;
	cnblocks = min(nblocks, spt - csector);

	for (i = 0; i < cnblocks; index++, i++)
	{
	    if (index == 0)
		address = (unsigned)buf_address(buf_no);
	    memcpy(&fdc_buf[(csector + i) << 9],
		   (void *)(address + (index << 9)), FDC_BLKSZ);

	    if (index == spb - 1)
	    {
		buf_no = buf_vect[buf_no].next;
		index = -1;
	    }
	}

	if (fdc_command(FLOPPY_WRITE, device, ctrack, csector, cnblocks))
	    good_sectors = cnblocks;
	else
	    for (good_sectors = i = 0; i < cnblocks; good_sectors++, i++)
	    {
		printk("FDC: writing one sector at a time (%d/%z/%z)...\n",
		       device, ctrack, csector + i);
		if (fdc_command(FLOPPY_WRITE, device, ctrack,csector+i,1) == 0)
		    break;
	    }

	if (good_sectors != cnblocks)
	    break;
	total_good_sectors += good_sectors;
    }

    up(&fdc_rw_mutex);

    return total_good_sectors * FDC_BLKSZ;
}


int
fdc_ioctl(int minor, int cmd, void *argp)
{
    switch (cmd)
    {
	case IOCTL_GETBLKSZ:          /* Get device block size.  */
	    *(int *)argp = FDC_BLKSZ;
	    break;

	case IOCTL_GETDEVSZ:          /* Get device size (in no of blocks).  */
	    *(int *)argp =  fdd_type[fdd[minor].type].total_sectors;
	    break;

	default:
	    return -EINVAL;
    }

    return 0;
}


int
fdc_lseek(int minor, __off_t offset)
{
    if (minor < fdc_driver.minors)
	return offset;

    return -ESPIPE;
}


/*
 * Initialize the driver data structures.  Detect the number of floppies
 * and their type from the CMOS.  Also register the driver.
 */

int
fdc_init(void)
{
    int type, fdd_type_byte = read_cmos(0x10);

    type = fdd_type_byte >> 4;

    if ((fdd[0].present = (type == 2 || type == 4)))
	fdd[0].type = (type != 2);

    printk("FDC: first  floppy disk drive is %s\n", fdd_drive_name[type]);

    type = fdd_type_byte & 0x0F;

    if ((fdd[1].present = (type == 2 || type == 4)))
	fdd[1].type = (type != 2);

    printk("FDC: second floppy disk drive is %s\n", fdd_drive_name[type]);

    return register_driver((generic_driver *)&fdc_driver);
}
