/* mem.c -- The mem driver.  */

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
#include <thix/mem.h>
#include <thix/gendrv.h>


extern int memory_pages;


static minor_info mem_minor_info[1] =
{
    {0, 0, 1}
};


character_driver mem_driver =
{
    "system memory",
    mem_open,
    mem_close,
    mem_read,
    mem_write,
    NULL,
    NULL,
    NULL,
    mem_lseek,
    NULL,
    MEM_MAJOR,
    NOIRQ,
    CHR_DRV,
    1,
    mem_minor_info,
};


/*
 * Driver for reading and writing PHYSICAL memory.
 */


int
mem_open(int minor, int flags)
{
    DEBUG(9, "(%d/%d)\n", MEM_MAJOR, minor);

    return 0;
}


int
mem_close(int minor)
{
    DEBUG(9, "(%d/%d)\n", MEM_MAJOR, minor);

    return 0;
}


int
mem_read(int minor, chr_request *cr)
{
    int count;

    if (cr->offset >= memory_pages * PAGE_SIZE)
	return 0;

    count = min(cr->count, memory_pages * PAGE_SIZE - cr->offset);

    memcpy(cr->buf, (char *)cr->offset, count);
    return count;
}


int
mem_write(int minor, chr_request *cr)
{
    int count;

    if (cr->offset >= memory_pages * PAGE_SIZE)
	return 0;

    count = min(cr->count, memory_pages * PAGE_SIZE - cr->offset);

    memcpy((char *)cr->offset, cr->buf, count);
    return count;
}


int
mem_lseek(int minor, __off_t offset)
{
    if (minor < mem_driver.minors)
	return offset;

    return -ESPIPE;
}


int
mem_init(void)
{
    return register_driver((generic_driver *)&mem_driver);
}
