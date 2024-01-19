/* part.c -- Block device partitioning routines.  */

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
#include <thix/string.h>
#include <thix/buffer.h>
#include <thix/blkdrv.h>
#include <thix/device.h>
#include <thix/stat.h>
#include <thix/part.h>
#include <thix/ioctl.h>
#include <thix/gendrv.h>



typedef struct
{
    unsigned char  bootable;
    unsigned char  start_head;
    unsigned char  start_sect_cyl_hi;
    unsigned char  start_cyl_low;
    unsigned char  os_id;
    unsigned char  end_head;
    unsigned char  end_sect_cyl_hi;
    unsigned char  end_cyl_low;
    unsigned int   relative_sector;
    unsigned int   total_sectors;
} partition_table_entry;


int
partitions_setup(block_driver *bd, __dev_t device, partition *p,
		 int cylinders, int tracks, int sectors)
{
    blk_request br;
    unsigned char *address;
    partition_table_entry *pt;
    int blksz, part, bootable_partitions = 0;
    unsigned relative_sector, total_sectors;
    unsigned end_cylinder, end_track, end_sector;
    unsigned start_cylinder, start_track, start_sector;


    bd->ioctl(minor(device), IOCTL_GETBLKSZ, &blksz);

    blksz = 1024;       /* Always use 1024 bytes buffers.  */

    br.buf_no  = buf_get(device, 0, blksz);
    br.block   = 0;
    br.nblocks = 1;
    br.next    = NULL;
    br.blksz   = blksz;

    bd->read(minor(device), &br);

    address = buf_address(br.buf_no);

    pt = (partition_table_entry *)(address + 0x1be);

    if (*(unsigned short *)(address + 0x1fe) != 0xAA55)
	goto fail;

    for (part = 0; part < 4; part++)
    {
	start_cylinder       = ((pt[part].start_sect_cyl_hi & 0xc0) << 2) +
			       pt[part].start_cyl_low;
	start_track          = pt[part].start_head;
	start_sector         = pt[part].start_sect_cyl_hi & 0x3f;

	end_cylinder         = ((pt[part].end_sect_cyl_hi & 0xc0) << 2) +
			       pt[part].end_cyl_low;
	end_track            = pt[part].end_head;
	end_sector           = pt[part].end_sect_cyl_hi & 0x3f;

	relative_sector      = pt[part].relative_sector;
	total_sectors        = pt[part].total_sectors;

	bootable_partitions += (pt[part].bootable & 0x80) ? 1 : 0;

	if (start_cylinder >= cylinders ||
	    end_cylinder   >= cylinders ||
	    start_track    >= tracks    ||
	    end_track      >= tracks    ||
	    start_sector   >  sectors   ||
	    end_sector     >  sectors)
	    continue;

	if (relative_sector != start_cylinder * tracks * sectors +
			       start_track * sectors +
			       start_sector - 1)
	    continue;

	if (total_sectors != end_cylinder * tracks * sectors +
			     end_track * sectors +
			     end_sector - 1 -
			     relative_sector + 1)
	    continue;

	p[part].first_block  = relative_sector;
	p[part].total_blocks = total_sectors;
    }

    if (bootable_partitions > 1)
	printk("WARNING: There are %z active partitions on device %w !\n",
		device);

    buf_vect[br.buf_no].valid = 0;      /* Get rid of it. */
    buf_release(br.buf_no);
    return 1;

  fail:

    printk("Invalid partition table on device %w !\n", device);
    buf_vect[br.buf_no].valid = 0;      /* Get rid of it. */
    buf_release(br.buf_no);
    return 0;
}
