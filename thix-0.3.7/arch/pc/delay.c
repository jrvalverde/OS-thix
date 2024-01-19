/* delay.c -- PC architecture: very precise I/O delay.  */

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


#include <thix/arch/pc/cmos.h>
#include <thix/proc/i386.h>


/*
 * PC specific I/O delay.
 */

void
io_delay(int cnt)
{
    do
    {
	while ((inb(0x61) & 0x10) == 0);

	if (--cnt == 0)
	    break;

	while ((inb(0x61) & 0x10));
    }
    while (--cnt);
}
