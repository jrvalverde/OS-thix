/* regular.c -- Regular files management routines.  */

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
#include <thix/string.h>
#include <thix/buffer.h>
#include <thix/inode.h>
#include <thix/fs.h>
#include <thix/fcntl.h>
#include <thix/stat.h>
#include <thix/limits.h>
#include <thix/blkdrv.h>
#include <thix/gendrv.h>
#include <thix/generic.h>


/*
 * Check if the file is opened for write.  Don't allow exec system calls
 * using this file if someone is currently writting in it.
 */

int
currently_opened_for_write(int i_no)
{
    int fd;

    /* This test avoids executing the loop if we are trying to check
       the only instance of the file.  */
    if (i_vect[i_no].nref == 1)
	return 0;

    for (fd = 0; fd < SYSTEM_OPEN_MAX; fd++)
	if (fdt[fd].count         &&
	    fdt[fd].inode == i_no &&
	    fdt[fd].flags & (O_RDWR | O_WRONLY | O_TRUNC))
	    return 1;

    return 0;
}


/*
 * Open a regular file.  Should we truncate the file only for O_RDWR or
 * O_WRONLY !?
 */

int
regular_open(int fd)
{
    int i_no = fdt[fd].inode;

    if ((fdt[fd].flags & O_TRUNC) && i_vect[i_no].i_size)
	i_truncate(i_no, 0);

    fdt[fd].offset = 0;

    return 0;
}


/*
 * Close a regular file.
 */

int
regular_close(int fd)
{
    int i_no = fdt[fd].inode;

    if ((fdt[fd].flags & (O_RDWR | O_WRONLY | O_TRUNC)) &&
	!currently_opened_for_write(i_no))
	i_vect[i_no].exec_denied = 0;

    return 0;
}


/*
 * Read from a regular file.
 */

int
regular_read(int fd, char *buf, size_t count)
{
    int i_no;
    iblk_data ibd;
    blk_request br;
    char *req_buf = NULL;
    unsigned i_data[DIRECT_BLOCKS], *block_map;
    int buf_no = 0, prev_buf_no, next_buf_no = 0;
    int block, prev_block = 0, device, lastblk, no_request = 1;
    int req_count = 0, req_boffset = 0, req_bcount, req_rcount;
    int offset, boffset, bcount, rcount = 0, blksz, iob, size, i;


    DEBUG(6, "(%d,%x,%d) offset=%d\n", fd, buf, count, fdt[fd].offset);

    i_lock(i_no = fdt[fd].inode);

    offset   = fdt[fd].offset;
    size     = i_vect[i_no].i_size;
    br.blksz = blksz = sb(device = i_vect[i_no].device)->s_blksz;

    for (iob = io_blocks(offset, count, size, blksz); iob;)
    {
	block_map = bmap(i_no, offset, &ibd, i_data);

	for (i = ibd.index, lastblk = ibd.index + ibd.nblocks;
	     iob && i < lastblk;
	     i++, iob--, rcount += bcount, count -= bcount, offset += bcount)
	{
	    boffset = offset % blksz;
	    bcount  = min3(count, size - offset, blksz - boffset);

	    if ((block = block_map[i]) == 0)
	    {
		memset(buf + rcount, 0, bcount);

		if (no_request)
		    continue;

		goto do_request;
	    }

	    prev_buf_no = buf_no;
	    buf_no = buf_get(device, block, blksz);

	    if (buf_vect[buf_no].valid)
	    {
		memcpy(buf + rcount, buf_address(buf_no) + boffset, bcount);
		buf_release(buf_no);

		if (no_request)
		    continue;

		goto do_request;
	    }

	    if (no_request)
	    {
	      new_request:
		br.block    = block;
		br.nblocks  = 1;
		br.buf_no   = buf_no;
		buf_vect[buf_no].next = 0;
		prev_block  = block;

		req_buf     = buf + rcount;
		req_boffset = boffset;
		req_count   = bcount;

		no_request  = 0;
		next_buf_no = 0;

		if (iob > 1 && i < lastblk - 1)
		    continue;

		goto do_request;
	    }

	    if (block == prev_block + 1)
	    {
		br.nblocks++;
		buf_vect[prev_buf_no].next = buf_no;
		buf_vect[buf_no].next = 0;
		prev_block = block;
		req_count += bcount;

		if (iob > 1 && i < lastblk - 1)
		    continue;

		goto do_request;
	    }

	    next_buf_no = buf_no;

	do_request:

	    drv_read(device, &br);
	    req_rcount = 0;

	    for (buf_no = br.buf_no; buf_no; buf_no = buf_vect[buf_no].next)
	    {
		req_bcount = min(req_count, blksz - req_boffset);
		memcpy(req_buf + req_rcount,
		       buf_address(buf_no) + req_boffset, req_bcount);
		buf_release(buf_no);
		req_boffset = 0;
		req_count -= req_bcount, req_rcount += req_bcount;
	    }

	    no_request = 1;

	    if (next_buf_no)
	    {
		buf_no = next_buf_no;
		goto new_request;
	    }
	}

	if (ibd.buf_no)
	    buf_release(ibd.buf_no);
    }

    fdt[fd].offset += rcount;
    i_unlock(i_no);
    return rcount;
}


/*
 * Write to a regular file.
 */

int
regular_write(int fd, char *buf, size_t count)
{
    unsigned address;
    int i_no, buf_no = 0, offset, boffset, bcount, wcount = 0, blksz, buf_ok;

    DEBUG(6, "(%d,%x,%d) offset=%d\n", fd, buf, count, fdt[fd].offset);

    i_lock(i_no = fdt[fd].inode);

    if (fdt[fd].flags & O_APPEND)
	fdt[fd].offset = i_vect[i_no].i_size;

    offset = fdt[fd].offset;

    blksz = sb(i_vect[i_no].device)->s_blksz;

    while (count)
    {
	boffset = offset % blksz;
	bcount = min(count, blksz - boffset);

	if ((buf_no = wbmap(i_no, offset)) == 0)
	    break;

	address = (unsigned)buf_address(buf_no);

	buf_ok = 1;

	if (bcount < blksz)
	    if ((offset & ~(blksz - 1)) < i_vect[i_no].i_size)
		buf_ok = _buf_read(buf_no);
	    else
		lmemset((void *)address, 0, blksz >> 2);

	if (buf_ok)
	{
	    memcpy((void *)(address + boffset), buf + wcount, bcount);
	    buf_write(buf_no);
	}

	wcount += bcount, count -= bcount, offset += bcount;

	if (i_vect[i_no].i_size < offset)
	    i_vect[i_no].i_size = offset;
    }

    if (wcount)
	i_vect[i_no].modified = 1;

    i_unlock(i_no);
    fdt[fd].offset += wcount;
    return wcount ? wcount : -ENOSPC;
}


/*
 * Read from a binary image.  Called on page faults.  This is a simplified
 * version of regular_read().
 */

int
a_out_read(int i_no, char *buf, __off_t offset, size_t count)
{
    iblk_data ibd;
    blk_request br;
    char *req_buf = NULL;
    unsigned i_data[DIRECT_BLOCKS], *block_map;
    int buf_no = 0, prev_buf_no, next_buf_no = 0;
    int rcount = 0, blksz, iob, i, req_count = 0, req_rcount;
    int block, prev_block = 0, device, lastblk, no_request = 1;


    DEBUG(6, "(%w,%x,%x,%d,%d)\n", i_no, i_vect[i_no].dinode, buf,
	  offset, count);

    br.blksz = blksz = sb(device = i_vect[i_no].device)->s_blksz;

    if (offset >= i_vect[i_no].i_size)
	PANIC("invalid offset\n");

    i_lock(i_no);

    for (iob = count / blksz; iob;)
    {
	block_map = bmap(i_no, offset, &ibd, i_data);

	for (i = ibd.index, lastblk = ibd.index + ibd.nblocks;
	     iob && i < lastblk;
	     i++, iob--, rcount += blksz, offset += blksz)
	{
	    if ((block = block_map[i]) == 0)
	    {
		memset(buf + rcount, 0, blksz);

		if (no_request)
		    continue;

		goto do_request;
	    }

	    prev_buf_no = buf_no;
	    buf_no = buf_get(device, block, blksz);

	    if (buf_vect[buf_no].valid)
	    {
		memcpy(buf + rcount, buf_address(buf_no), blksz);
		buf_release(buf_no);

		if (no_request)
		    continue;

		goto do_request;
	    }

	    if (no_request)
	    {
	      new_request:
		br.block    = block;
		br.nblocks  = 1;
		br.buf_no   = buf_no;
		buf_vect[buf_no].next = 0;
		prev_block  = block;

		req_buf     = buf + rcount;
		req_count   = blksz;

		next_buf_no = 0;
		no_request  = 0;

		if (iob > 1 && i < lastblk - 1)
		    continue;

		goto do_request;
	    }

	    if (block == prev_block + 1)
	    {
		br.nblocks++;
		buf_vect[prev_buf_no].next = buf_no;
		buf_vect[buf_no].next = 0;
		prev_block = block;
		req_count += blksz;

		if (iob > 1 && i < lastblk - 1)
		    continue;

		goto do_request;
	    }

	    next_buf_no = buf_no;

	do_request:

	    drv_read(device, &br);
	    req_rcount = 0;

	    for (buf_no = br.buf_no; buf_no; buf_no = buf_vect[buf_no].next)
	    {
		memcpy(req_buf + req_rcount,
		       buf_address(buf_no) + 0, blksz);
		buf_release(buf_no);
		req_count -= blksz, req_rcount += blksz;
	    }

	    no_request = 1;

	    if (next_buf_no)
	    {
		buf_no = next_buf_no;
		goto new_request;
	    }
	}

	if (ibd.buf_no)
	    buf_release(ibd.buf_no);
    }

    i_unlock(i_no);

    return rcount;
}
