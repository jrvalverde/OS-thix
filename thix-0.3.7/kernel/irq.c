/* irq.c -- IRQ management routines.  */

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
#include <thix/sched.h>
#include <thix/irq.h>


#define ERR_BADIRQ      error[0]
#define ERR_BADUNREG    error[1]

static char *error[] =
{
    "trying to unregister irq",
    "trying to unregister unregistered irq",
};


#define MAX_IRQ         16


static unsigned irq_map = (1 << TIMER_IRQ) | (1 << CASCADE_IRQ);


/*
 * Register a new irq.  Return 1 when no conflict occured, 0 otherwise.
 */

int
irq_register(unsigned char irq)
{
    if (irq == NOIRQ)
	return 1;

    if (irq >= MAX_IRQ || ((1 << irq) & irq_map))
	return 0;

    irq_map |= 1 << irq;

    return 1;
}


/*
 * Unregister an irq.  Not used.
 */

void
irq_unregister(unsigned char irq)
{
    if (irq == NOIRQ || irq >= MAX_IRQ)
	PANIC("(%x): %s\n", irq, ERR_BADIRQ);

    if ((1 << irq) & irq_map)
	irq_map &= ~(1 << irq);
    else
	PANIC("(%x): %s\n", irq, ERR_BADUNREG);
}


int
irq_init(void)
{
    return 1;
}
