/* null.c -- The null driver.  */

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
#include <thix/irq.h>
#include <thix/errno.h>
#include <thix/null.h>
#include <thix/gendrv.h>


static minor_info null_minor_info[1] =
{
    {0, 0, 1}
};


static character_driver null_driver =
{
    "null",
    null_open,
    null_close,
    null_read,
    null_write,
    NULL,
    NULL,
    NULL,
    null_lseek,
    NULL,
    NULL_MAJOR,
    NOIRQ,
    CHR_DRV,
    1,
    null_minor_info,
};


/*
 * Driver for the null device.
 */


int
null_open(int minor, int flags)
{
    DEBUG(9, "(%d/%d)\n", NULL_MAJOR, minor);

    return 0;
}


int
null_close(int minor)
{
    DEBUG(9, "(%d/%d)\n", NULL_MAJOR, minor);

    return 0;
}


int
null_read(int minor, chr_request *cr)
{
    return 0;
}


int
null_write(int minor, chr_request *cr)
{
    return cr->count;
}


int
null_lseek(int minor, __off_t offset)
{
    if (minor < null_driver.minors)
	return -EINVAL;

    return -ESPIPE;
}


int
null_init(void)
{
    return register_driver((generic_driver *)&null_driver);
}
