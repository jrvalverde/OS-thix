/* main.c -- Main kernel routine.  */

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
#include <thix/ctype.h>
#include <thix/vtty.h>
#include <thix/kpid.h>
#include <thix/buffer.h>
#include <thix/inode.h>
#include <thix/irq.h>
#include <thix/gendrv.h>
#include <thix/device.h>
#include <thix/process.h>
#include <thix/proc/i386.h>


/* The kernel debug levels.  Used by many routines to display debug
   info at run-time when the kernel is compiled with __DEBUG__.  This
   string should contain at most 10 digits - the debug levels.  */
char thix_debug_levels[11] = "0";


int
sys_kctrl(struct kctrl *kc)
{
    int i;

    if (!superuser())
	return -EPERM;

    /* Check the information in the kernel cotrol structure pointed by
       `kctrl'.  */

    /* 1. check the debug levels.  */
    for (i = 0; i < 10; i++)
    {
	if (kc->debug_levels[i] == '\0')
	    break;

	if (!isdigit(kc->debug_levels[i]))
	    return -EINVAL;
    }

    /* The kernel control structure is ok.  Fill the appropriate
       kernel data structures from it.  */

    /* 1. Set the debug levels.  */
    memcpy(thix_debug_levels, kc->debug_levels, 10);
    thix_debug_levels[10] = '\0';

    DEBUG(4, "(%s)\n", thix_debug_levels);

    return 0;
}


/*
 * System startup.  Called from startup.s after initializing the hardware.
 */

void
main(void)
{
    /* Clear the kernel bss.  */
    bss_init();


    /* Initialize the timer at 100 Hz.  */
    timer_init();


    /* Initialize the IRQ management.  */
    irq_init();


    /* Initialize the block & character device handling.  */
    drv_init();


    /* Register the virtual terminal device driver. We need this here
       in order to be able to write error/warning messages on the screen.  */
    vtty_init();


    /* Initialize miscelaneous kernel data structures.  */
    kpid_init();                /* The kernel process ids management.  */
    sleep_init();               /* The sleep()/wakeup() stuff.  */
    vm_init();                  /* The VM stuff.  */
    buf_init();                 /* The file system buffer management.  */
    i_init();                   /* The file system inode management.  */
    scheduler_init();           /* The scheduler.  */

    /* Enable interrupts.  */
    enable();


    /* Enter in idle state.  The swapper will be started at the next
       timer interrupt.  */
    idle();
}
