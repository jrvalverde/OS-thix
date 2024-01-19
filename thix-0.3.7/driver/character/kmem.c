/* kmem.c -- The kmem driver.  */

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
#include <thix/string.h>
#include <thix/irq.h>
#include <thix/kmem.h>
#include <thix/gendrv.h>


static minor_info kmem_minor_info[1] =
{
    {0, 0, 1}
};


static character_driver kmem_driver =
{
    "kernel memory",
    kmem_open,
    kmem_close,
    kmem_read,
    kmem_write,
    NULL,
    NULL,
    NULL,
    kmem_lseek,
    NULL,
    KMEM_MAJOR,
    NOIRQ,
    CHR_DRV,
    1,
    kmem_minor_info,
};


/*
 * Driver for reading and writing KERNEL virtual memory.
 */


int
kmem_open(int minor, int flags)
{
    DEBUG(9, "(%d/%d)\n", KMEM_MAJOR, minor);

    return 0;
}


int
kmem_close(int minor)
{
    DEBUG(9, "(%d/%d)\n", KMEM_MAJOR, minor);

    return 0;
}


int
kmem_read(int minor, chr_request *cr)
{
    memcpy(cr->buf, (char *)cr->offset, cr->count);
    return cr->count;
}


int
kmem_write(int minor, chr_request *cr)
{
    memcpy(cr->buf, (char *)cr->offset, cr->count);
    return cr->count;
}


int
kmem_lseek(int minor, __off_t offset)
{
    if (minor < kmem_driver.minors)
	return offset;

    return -ESPIPE;
}


int
kmem_init(void)
{
    return register_driver((generic_driver *)&kmem_driver);
}
