/* device.c -- Special files management routines.  */

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
#include <thix/string.h>
#include <thix/buffer.h>
#include <thix/inode.h>
#include <thix/block.h>
#include <thix/dir.h>
#include <thix/fcntl.h>
#include <thix/stat.h>
#include <thix/limits.h>
#include <thix/blkdrv.h>
#include <thix/chrdrv.h>
#include <thix/process.h>
#include <thix/device.h>
#include <thix/generic.h>
#include <thix/ioctl.h>
#include <thix/indirect.h>


/*
 * Check if the given device can be closed.
 */

int
can_close_device(__dev_t device, int current_fd)
{
    int fd;

    for (fd = 1; fd < SYSTEM_OPEN_MAX; fd++)
	if (fd != current_fd &&
	    fdt[fd].count    &&
	    i_vect[fdt[fd].inode].i_rdev == device)
	    return 0;

    return 1;
}


/*
 * Open a block device.
 */

int
blk_open(int fd)
{
    int i_no = fdt[fd].inode;

    fdt[fd].offset = 0;
    return drv_open(i_vect[i_no].i_rdev, fdt[fd].flags);
}


/*
 * Close a block device.
 */

int
blk_close(int fd)
{
    int i_no = fdt[fd].inode;
    __dev_t device = i_vect[i_no].i_rdev;

    if (can_close_device(device, fd) && !mounted(device))
    {
	buf_sync(device, 1);
	drv_close(device);
    }

    return 0;
}


/*
 * Read from a block device.
 */

int
blk_read(int fd, char *buf, size_t count)
{
    int i_no;
    blk_request br;
    char *req_buf = NULL;
    int buf_no = 0, prev_buf_no;
    int block, device, no_request = 1;
    int offset, boffset, bcount, rcount = 0, blksz, iob, size;
    int req_count = 0, req_boffset = 0, req_bcount, req_rcount;

    DEBUG(6, "(%d,%x,%d)-offset=%d\n", fd, buf, count, fdt[fd].offset);


    i_no   = fdt[fd].inode;             /* Not locked.  */
    offset = fdt[fd].offset;
    device = i_vect[i_no].i_rdev;

    /* We will ask the device driver for the block size, but only  in order
       to find out the total device size.  We will *always*  use 1024 bytes
       blocks because we can't have a 512 bytes buffer together with a 1024
       bytes buffer for the same physical device block.  Such  a  situation
       could occur if we are reading/writing from/to a mounted file system.  */

    drv_ioctl(device, IOCTL_GETBLKSZ, &blksz);
    drv_ioctl(device, IOCTL_GETDEVSZ, &size);
    size *= blksz;

    /* Always use 1024 bytes buffers.  */
    br.blksz = blksz = 1024;

    for (iob = io_blocks(offset, count, size, blksz), block = offset / blksz;
	 iob;
	 iob--, block++, rcount += bcount, count -= bcount, offset += bcount)
    {
	boffset = offset % blksz;
	bcount  = min3(count, size - offset, blksz - boffset);

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
	    br.block    = block;
	    br.nblocks  = 1;
	    br.buf_no   = buf_no;
	    buf_vect[buf_no].next = 0;

	    req_buf     = buf + rcount;
	    req_boffset = boffset;
	    req_count   = bcount;

	    no_request  = 0;

	    if (iob > 1)
		continue;

	    goto do_request;
	}

	br.nblocks++;
	buf_vect[prev_buf_no].next = buf_no;
	buf_vect[buf_no].next = 0;
	req_count += bcount;

	if (iob > 1)
	    continue;

    do_request:

	drv_read(device, &br);
	req_rcount = 0;

	for (buf_no = br.buf_no; buf_no; buf_no = buf_vect[buf_no].next)
	{
	    req_bcount = min(req_count, blksz - req_boffset);
	    memcpy(req_buf + req_rcount, buf_address(buf_no) + req_boffset,
		   req_bcount);
	    buf_release(buf_no);
	    req_boffset = 0;
	    req_count -= req_bcount, req_rcount += req_bcount;
	}

	no_request = 1;
    }

    i_vect[i_no].modified = 1;

    fdt[fd].offset += rcount;
    return rcount;
}


/*
 * Write to a block device.
 */

int
blk_write(int fd, char *buf, size_t count)
{
    __dev_t device;
    int blksz, size;
    unsigned address;
    int i_no, buf_no = 0, offset, boffset, bcount, wcount = 0, buf_ok;

    DEBUG(6, "(%d,%x,%d)-offset=%d\n", fd, buf, count, fdt[fd].offset);


    i_no   = fdt[fd].inode;             /* not locked */
    offset = fdt[fd].offset;
    device = i_vect[i_no].i_rdev;
    drv_ioctl(device, IOCTL_GETBLKSZ, &blksz);
    drv_ioctl(device, IOCTL_GETDEVSZ, &size);
    size *= blksz;

    blksz = 1024;       /* Force usage of 1024 bytes buffers.  */

    while (count)
    {
	boffset = offset % blksz;
	bcount = min(count, size - offset);

	if ((bcount = min(bcount, blksz - boffset)) == 0)
	    break;

	buf_no = buf_get(device, offset / blksz, blksz);
	address = (unsigned)buf_address(buf_no);

	buf_ok = (bcount < blksz) ? _buf_read(buf_no) : 1;

	if (buf_ok)
	{
	    memcpy((void *)(address + boffset), buf + wcount, bcount);
	    buf_write(buf_no);
	}

	wcount += bcount, count -= bcount, offset += bcount;
    }


    i_vect[i_no].modified = 1;

    fdt[fd].offset += wcount;
    return wcount;
}


/*
 * Block devices ioctl().
 */

int
blk_ioctl(int fd, int cmd, void *argp)
{
    return drv_ioctl(i_vect[fdt[fd].inode].i_rdev, cmd, argp);
}


int
blk_lseek(int fd, __off_t offset)
{
    return drv_lseek(i_vect[fdt[fd].inode].i_rdev, offset);
}


/*
 * Open a character device.
 */

int
chr_open(int fd)
{
    int i_no  = fdt[fd].inode;
    int flags = fdt[fd].flags;

    /* Are we trying to open "/dev/tty" or "/dev/tty0", the indirect
       terminal? If so, release the old inode and use a new one.  */

    if (i_vect[i_no].i_rdev == makedev(ITTY_MAJOR, 0))
	if (this->ctty)
	{
	    __dev_t old_device = i_vect[i_no].device;
	    i_get_rid_of(i_no);
	    i_no = i_alloc(old_device);
	    /* FIXME: error test needed here!  */
	    i_vect[i_no].i_nlinks = 0;
	    i_vect[i_no].i_rdev = this->ctty;
	    i_vect[i_no].i_mode = S_IFCHR;
	    fdt[fd].inode = i_no;
	}
	else
	    return -ENXIO;

    fdt[fd].offset = 0;
    return drv_open(i_vect[i_no].i_rdev, flags);
}


/*
 * Close a character device.
 */

int
chr_close(int fd)
{
    int i_no = fdt[fd].inode;
    __dev_t device = i_vect[i_no].i_rdev;

    if (can_close_device(device, fd))
	drv_close(device);

    return 0;
}


/*
 * Read from a character device.
 */

int
chr_read(int fd, char *buf, size_t count)
{
    int i_no, rcount;
    chr_request cr = { buf, fdt[fd].offset, count };

    DEBUG(6, "(%d,%x,%d)-offset=%d\n", fd, buf, count, fdt[fd].offset);


    i_no = fdt[fd].inode;               /* Not locked.  */

    rcount = drv_read(i_vect[i_no].i_rdev, (drv_request *)&cr);

    i_vect[i_no].modified = 1;

    fdt[fd].offset += rcount;
    return rcount;
}


/*
 * Write to a character device.
 */

int
chr_write(int fd, char *buf, size_t count)
{
    int i_no, wcount;
    chr_request cr = { buf, fdt[fd].offset, count };

    DEBUG(6, "(%d,%x,%d)-offset=%d\n", fd, buf, count, fdt[fd].offset);


    i_no = fdt[fd].inode;               /* not locked */

    wcount = drv_write(i_vect[i_no].i_rdev, (drv_request *)&cr);

    i_vect[i_no].modified = 1;

    fdt[fd].offset += wcount;
    return wcount;
}


/*
 * Character devices ioctl().
 */

int
chr_ioctl(int fd, int cmd, void *argp)
{
    return drv_ioctl(i_vect[fdt[fd].inode].i_rdev, cmd, argp);
}


int
chr_lseek(int fd, __off_t offset)
{
    return drv_lseek(i_vect[fdt[fd].inode].i_rdev, offset);
}


/*
 * The mknod() system call.
 */

int
sys_mknod(char *newnodename, int mode, __dev_t device)
{
    superblock *sb;
    char *nodename;
    int i_no, dir_i_no, offset, empty_dir_slot, dir_offset;

    DEBUG(4, "(%s,%o,%x)\n", newnodename, mode, device);

    if (!(S_ISBLK(mode) || S_ISCHR(mode) || S_ISFIFO(mode)))
	return -EINVAL;

    if (!S_ISFIFO(mode))
	if (!superuser())
	    return -EPERM;

    i_no = __namei(newnodename, &empty_dir_slot, &dir_i_no,
				&dir_offset, &nodename);

    if (i_no)
    {
	if (dir_i_no)
	    i_get_rid_of(dir_i_no);

	if (i_no > 0)
	    i_get_rid_of(i_no);

	return (i_no > 0) ? -EEXIST : i_no;
    }

    if (dir_i_no == 0)
	return -EBUSY;

    if (!permitted(dir_i_no, W_OK | X_OK, 0))
    {
	i_get_rid_of(dir_i_no);
	return -EACCES;
    }

    if ((i_no = i_alloc(i_vect[dir_i_no].device)) == 0)
    {
	i_get_rid_of(dir_i_no);
	return -ENOSPC;
    }

    sb = sb(i_vect[dir_i_no].device);
    offset = find_empty_dir_slot(sb, dir_i_no,
				 (empty_dir_slot != -1) ? empty_dir_slot : 0);

    if (setup_directory_entry(sb, dir_i_no, offset, i_no, nodename) == 0)
    {
	i_vect[i_no].i_nlinks--;
	i_get_rid_of2(dir_i_no, i_no);
	return -ENOSPC;
    }

    i_vect[i_no].i_mode = (mode & ~this->umask & ~S_IFMT) | (mode & S_IFMT);

    i_set_mtime(dir_i_no);
    i_set_ctime(dir_i_no);

    i_vect[i_no].i_uid = this->euid;
    i_vect[i_no].i_gid = this->egid;
    i_vect[i_no].modified     =
    i_vect[dir_i_no].modified = 1;
    i_vect[i_no].i_rdev = S_ISFIFO(mode) ? 0 : device;

    i_get_rid_of2(dir_i_no, i_no);
    return 0;
}
