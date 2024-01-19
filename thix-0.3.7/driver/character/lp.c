/* lp.c -- The line printer driver.  */

/*
 * Thix Operating System
 * Copyright (C) 1996 Tudor Hulubei
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
#include <thix/sched.h>
#include <thix/signal.h>
#include <thix/irq.h>
#include <thix/lp.h>
#include <thix/gendrv.h>
#include <thix/proc/i386.h>
#include <thix/arch/pc/port.h>


static minor_info lp_minor_info[4] =
{
    {0, 0, 1},
    {0, 0, 1},
    {0, 0, 1},
    {0, 0, 1},
};


static character_driver lp_driver =
{
    "line printer",
    lp_open,
    lp_close,
    lp_read,
    lp_write,
    NULL,
    NULL,
    NULL,
    lp_lseek,
    NULL,
    LP_MAJOR,
    NOIRQ,
    CHR_DRV,
    2,
    lp_minor_info,
};


typedef struct
{
    char in_use;
    char present;
    char blocking;
} lpflags_t;

static lpflags_t lp_flags[4] = { {0, 0, 0}, {0, 0, 0}, {0, 0, 0} };

/*
 * Note:
 *
 * The LP driver is based on the information found in "The Indispensable
 * PC Hardware Book" Second Edition by Hans-Peter Messmer.
 *
 * Addison-Wesley 1995
 * ISBN 0-201-87697-3
 */

#define LPSTAT_BSY	(1 << 7) /* 0 = Printer busy, offline or error.  */
#define LPSTAT_ACK	(1 << 6) /* 0 = Data transfer complete.  */
#define LPSTAT_PAP	(1 << 5) /* 1 = No more paper.  */
#define LPSTAT_OFON	(1 << 4) /* 1 = Printer online.  */
#define LPSTAT_ERR	(1 << 3) /* 0 = Printer error.  */

#define LPCTRL_IRQ	(1 << 4) /* 1 = IRQ 7 & IRQ 5 enabled.  */
#define LPCTRL_DSL	(1 << 3) /* 1 = Printer selected.  */
#define LPCTRL_INI	(1 << 2) /* 0 = Printer is initializing.  */
#define LPCTRL_ALF	(1 << 1) /* 1 = Automatic line feed.  */
#define LPCTRL_STR	(1 << 0) /* 1 = Transfer data to printer.  */

#define LP_TIMEOUT	0x4000

#define MAX_LPS		4

/* Line printer base ports.  */
#define LP_BPORT0	0x3bc
#define LP_BPORT1	0x378
#define LP_BPORT2	0x278
#define LP_BPORT3	0x2bc

int lp_data[4] = {LP_BPORT0 + 0, LP_BPORT1 + 0, LP_BPORT2 + 0, LP_BPORT3 + 0};
int lp_stat[4] = {LP_BPORT0 + 1, LP_BPORT1 + 1, LP_BPORT2 + 1, LP_BPORT3 + 1};
int lp_ctrl[4] = {LP_BPORT0 + 2, LP_BPORT1 + 2, LP_BPORT2 + 2, LP_BPORT3 + 2};


/*
 * Driver for the line printer.  Not implemented yet.
 */


int
lp_open(int minor, int flags)
{
    DEBUG(9, "(%d/%d)\n", LP_MAJOR, minor);

    if (minor < 0 || minor >= MAX_LPS || !lp_flags[minor].present)
    {
	DEBUG(9, "no such device\n");
	return -ENXIO;
    }

    if ((flags & 3) != O_WRONLY)
	return -EINVAL;

    /* Only one process can access the line printer.  */
    if (lp_flags[minor].in_use)
    {
	DEBUG(9, "device busy\n");
	return -EBUSY;
    }

    lp_flags[minor].blocking = !(flags & O_NONBLOCK);
    lp_flags[minor].in_use = 1;
    return 0;
}


int
lp_close(int minor)
{
    DEBUG(9, "(%d/%d)\n", LP_MAJOR, minor);

    lp_flags[minor].in_use = 0;
    return 0;
}


int
lp_read(int minor, chr_request *cr)
{
    DEBUG(9, "(%d,%x,%d)\n", minor, cr->buf, cr->count);

    /* Not reached. */
    return -EPERM;
}


static int
lp_write_data(int minor, unsigned char c)
{
    int i;
    unsigned char code;

    for (i = 0; i < LP_TIMEOUT; i++)
    {
	code = inb(lp_stat[minor]);

	if ((code & LPSTAT_BSY) != 0)
	    break;
    }

    if (i == LP_TIMEOUT)
	return 0;

    /* Set the data register.  */
    outb(lp_data[minor], c);

    /* Set the STR (strobe) bit.  */
    outb(lp_ctrl[minor], inb(lp_ctrl[minor]) | LPCTRL_STR);

    /* A small delay.  */
    io_delay(2);

    /* Clear the STR (strobe) bit.  */
    outb(lp_ctrl[minor], inb(lp_ctrl[minor]) & ~LPCTRL_STR);

    for (i = 0; i < LP_TIMEOUT; i++)
    {
	code = inb(lp_stat[minor]);

	if ((code & LPSTAT_ACK) == 0)
	    break;
    }

    if (i == LP_TIMEOUT)
	return 0;

    return 1;
}


int
lp_write(int minor, chr_request *cr)
{
    int index;
    unsigned char status;

    DEBUG(9, "(%d,%x,%d)\n", minor, cr->buf, cr->count);


    for (index = 0; index < cr->count;)
    {
	if (lp_write_data(minor, cr->buf[index]))
	    index++;
	else
	{
	    status = inb(lp_stat[minor]);

	    if (status & LPSTAT_PAP)
		printk("LP: lp%d out of paper\n", minor);
	    else if (status & LPSTAT_OFON)
		printk("LP: lp%d off line\n", minor);
	    else
		printk("LP: lp%d printer error (status=%b)\n", minor, status);

	    if (!lp_flags[minor].blocking)
		return -EAGAIN;
	    else
	    {
		/* 5 seconds delay.  */
		int end_ticks = ticks + 5000 / 10;

		/* FIXME: We should not reschedule() directly here, as
		   it is not very efficient.  We should be able to set
		   a itimer timeout, but that feature is not yet
		   implemented.  Using normal timers will be even more
		   inefficient.  */

		while (ticks < end_ticks)
		{
		    /* Check for signals.  We are in an interruptible
		       state.  */
		    if (signals())
			return -EINTR;

		    reschedule();
		}
	    }
	}
    }

    return index;
}


int
lp_lseek(int minor, __off_t offset)
{
    return -ESPIPE;
}


static void
lp_minor_init(int minor)
{
    unsigned char code;


    /* Check the presence of the interface.  */
    if (!port_exists(lp_data[minor]))
	return;

    /* Reset the interface/printer.  */
    outb(lp_ctrl[minor], 0);

    io_delay(10);

    /*
     * Well, the printer has been initialized.  Set the control
     * register to a valid state, that is:
     *
     *  - set the INI bit to specify that the printer is operating normal.
     *  - set DSL bits to specify that the printer is selected.
     *  - reset the IRQ bit, as we cannot handle IRQs.
     *  - reset the ALF bit, as line feed is not automatic.
     */
    code = inb(lp_ctrl[minor]);
    code |=  LPCTRL_INI;
    code |=  LPCTRL_DSL;
    code &= ~LPCTRL_IRQ;
    code &= ~LPCTRL_ALF;
    outb(lp_ctrl[minor], code);

    lp_flags[minor].present = 1;
}


int
lp_init(void)
{
    int minor, registered;

    for (minor = 0; minor < MAX_LPS; minor++)
    {
	lp_minor_init(minor);

	if (lp_flags[minor].present)
	    printk("LP: lp%d detected at 0x%w\n", minor, lp_data[minor]);
    }

    return registered = register_driver((generic_driver *)&lp_driver);;
}
