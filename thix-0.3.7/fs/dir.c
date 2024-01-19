/* dir.c -- Directories management routines.  */

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
#include <thix/block.h>
#include <thix/sched.h>
#include <thix/fs.h>
#include <thix/gendrv.h>
#include <thix/fcntl.h>
#include <thix/stat.h>
#include <thix/limits.h>
#include <thix/dirent.h>
#include <thix/process.h>
#include <thix/generic.h>
#include <thix/regular.h>


/*
 * Open a directory.  Nothing special :-).
 */

int
dir_open(int fd)
{
    fdt[fd].offset = 0;
    return 0;
}


/*
 * Close a directory.
 */

int
dir_close(int fd)
{
    return 0;
}


/*
 * Reads from a directory.  Directories can be read as any regular file.
 */

int
dir_read(int fd, char *buf, size_t count)
{
    return regular_read(fd, buf, count);
}


/*
 * Writting to a directory is not allowed.
 */

int
dir_write(int fd, char *buf, size_t count)
{
    return -EBADF;
}


/*
 * Find an empty slot in a directory.
 */

int
find_empty_dir_slot(superblock *sb, int dir_i_no, unsigned dir_offset)
{
    unsigned address = 0;
    int buf_no = 0, offset, first_time = 1;


    for (offset = dir_offset;
	 offset < i_vect[dir_i_no].i_size;
	 offset += sb->s_direntsz)
    {
	if (offset % sb->s_blksz == 0 || first_time)
	{
	    if ((buf_no = rbmap(dir_i_no, offset)) == 0)
		PANIC("bad directory");
	    address = (unsigned)buf_address(buf_no);
	    first_time = 0;
	}

	if (*(__ino_t *)(address + offset % sb->s_blksz) == 0)
	{
	    buf_release(buf_no);
	    return offset;
	}

	if (offset % sb->s_blksz == sb->s_blksz - sb->s_direntsz)
	    buf_release(buf_no), buf_no = 0;
    }

    if (buf_no)
	buf_release(buf_no);

    return offset;
}


/*
 * Set up a directory entry.
 */

int
setup_directory_entry(superblock *sb, int dir_i_no, int offset,
		      int i_no, char *entry_name)
{
    int buf_no;
    unsigned address;


    if ((buf_no = wbmap(dir_i_no, offset)) == 0)
	return 0;

    if (offset == i_vect[dir_i_no].i_size)
    {
	i_vect[dir_i_no].i_size += sb->s_direntsz;
	i_vect[dir_i_no].modified = 1;
    }

    address = (unsigned)buf_address(buf_no) + offset % sb->s_blksz;

    *(__ino_t *)address = i_no ? i_vect[i_no].dinode : 0;

    if (entry_name)
	memcpy((char *)(address + sizeof(__ino_t)), entry_name,
	       sb->s_direntsz - sizeof(__ino_t));

    buf_write(buf_no);
    return 1;
}


/*
 * Check if the directory is empty.  Called from rmdir when removing a
 * directory.
 */

int
is_dir_empty(superblock *sb, int dir_i_no)
{
    unsigned address = 0;
    int buf_no = 0, offset, first_time = 1;


    for (offset = sb->s_direntsz * 2;
	 offset < i_vect[dir_i_no].i_size;
	 offset += sb->s_direntsz)
    {
	if (offset % sb->s_blksz == 0 || first_time)
	{
	    if ((buf_no = rbmap(dir_i_no, offset)) == 0)
		PANIC("DIR: BAD DIRECTORY (is_dir_empty())");
	    address = (unsigned)buf_address(buf_no);
	    first_time = 0;
	}

	if (*(__ino_t *)(address + offset % sb->s_blksz))
	{
	    buf_release(buf_no);
	    return 0;
	}

	if (offset % sb->s_blksz == sb->s_blksz - sb->s_direntsz)
	    buf_release(buf_no), buf_no = 0;
    }

    if (buf_no)
	buf_release(buf_no);

    return 1;
}


/*
 * The mkdir() system call.
 */

int
sys_mkdir(char *newdirname, __mode_t mode)
{
    char *dirname;
    superblock *sb;
    int i_no, dir_i_no, offset, empty_dir_slot, dir_offset;


    DEBUG(4, "(%s,%o)\n", newdirname, mode);

    i_no = __namei(newdirname, &empty_dir_slot, &dir_i_no,
			       &dir_offset, &dirname);

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

    if (setup_directory_entry(sb, i_no, 0,                  i_no, "." ) == 0 ||
	setup_directory_entry(sb, i_no, sb->s_direntsz, dir_i_no, "..") == 0 ||
	setup_directory_entry(sb, dir_i_no, offset, i_no, dirname)      == 0)
    {
	i_vect[i_no].i_nlinks--;
	i_get_rid_of2(dir_i_no, i_no);
	return -ENOSPC;
    }

    i_set_mtime(dir_i_no);
    i_set_ctime(dir_i_no);

    i_vect[i_no].i_mode = (mode & ~this->umask & ~S_IFMT) | S_IFDIR;
    i_vect[i_no].i_nlinks++;
    i_vect[dir_i_no].i_nlinks++;
    i_vect[i_no].i_uid = this->euid;
    i_vect[i_no].i_gid = this->egid;
    i_vect[i_no].modified     =
    i_vect[dir_i_no].modified = 1;

    i_get_rid_of2(dir_i_no, i_no);
    return 0;
}


/*
 * The rmdir() system call.
 */

int
sys_rmdir(char *dir)
{
    char *dirname;
    superblock *sb;
    int i_no, empty_dir_slot, dir_i_no, dir_offset;


    DEBUG(4, "(%s)\n", dir);

    i_no = __namei(dir, &empty_dir_slot, &dir_i_no, &dir_offset, &dirname);

    if (i_no <= 0)
    {
	if (dir_i_no)
	    i_get_rid_of(dir_i_no);
	return (i_no == 0) ? -ENOENT : i_no;
    }

    if (dir_i_no == 0)
    {
	i_get_rid_of(i_no);
	return -EBUSY;
    }

    if (!S_ISDIR(i_vect[i_no].i_mode))
    {
	i_get_rid_of2(dir_i_no, i_no);
	return -ENOTDIR;
    }

    if (!permitted(dir_i_no, W_OK | X_OK, 0))
    {
	i_get_rid_of2(dir_i_no, i_no);
	return -EACCES;
    }

    if ((i_vect[dir_i_no].i_mode & S_ISVTX) == S_ISVTX)
	if (this->euid != i_vect[i_no].i_uid)
	{
	    i_get_rid_of2(dir_i_no, i_no);
	    return -EACCES;
	}

    sb = sb(i_vect[dir_i_no].device);

    if (dir_offset == 0 || dir_offset == sb->s_direntsz)
    {
	/* we can't delete "." or ".." */
	i_get_rid_of2(dir_i_no, i_no);
	return -EPERM;
    }

    if (!is_dir_empty(sb, i_no))
    {
	i_get_rid_of2(dir_i_no, i_no);
	return -EEXIST;
    }

#if 0
    if (i_no == this->cwd_inode)
    {
	i_vect[i_no].nref++;
	DEBUG(0, "Deleting current directory <%x>!\n", i_vect[i_no].nref);
    }
#endif

    setup_directory_entry(sb, dir_i_no, dir_offset, 0, NULL);

    i_vect[i_no].i_nlinks -= 2;
    i_vect[dir_i_no].i_nlinks--;

    i_set_ctime(i_no);
    i_set_mtime(dir_i_no);
    i_set_ctime(dir_i_no);

    i_vect[i_no].modified     =
    i_vect[dir_i_no].modified = 1;

    i_get_rid_of2(dir_i_no, i_no);
    return 0;
}


int
sys_chdir(char *dirname)
{
    int i_no;


    DEBUG(4, "(%s)\n", dirname);

    i_no = namei(dirname);

    if (i_no < 0)
	return i_no;

#if 0
    /* I don't think this is really needed.  The permission test is
       performed by namei().  */

    if (!permitted(i_no, X_OK, 0))
    {
	i_get_rid_of(i_no);
	return -EACCES;
    }
#endif

    if (!S_ISDIR(i_vect[i_no].i_mode))
    {
	i_get_rid_of(i_no);
	return -ENOTDIR;
    }

    i_unlock(i_no);
    i_release(this->cwd_inode);
    this->cwd_inode = i_no;
    return 0;
}


/*
 * The readdir() system call.  'count' is unused right now.
 */

int
sys_readdir(int ufd, struct dirent *dirent, size_t count)
{
    superblock *sb;
    unsigned address;
    int fd, i_no, buf_no, offset;


    DEBUG(5, "(%d,%x,%d)\n", ufd, dirent, count);

    if (!ok_to_write_area(dirent, sizeof(struct dirent)))
	return -EFAULT;

    check_ufd(ufd);
    i_no = fdt[fd = this->ufdt[ufd]].inode;

    if (!S_ISDIR(i_vect[i_no].i_mode))
	return -ENOTDIR;

    i_lock(i_no);
    offset = fdt[fd].offset;

    /* buf_no == 0  =>  *big* trouble.  */

    if (offset >= i_vect[i_no].i_size || ((buf_no = rbmap(i_no, offset)) == 0))
    {
	i_unlock(i_no);
	return 0;
    }

    sb = sb(i_vect[i_no].device);
    address = (unsigned)buf_address(buf_no);
    SYSCALL_PROLOG();

    dirent->d_fileno = *(__ino_t *)(address + offset % sb->s_blksz);

    memcpy(dirent->d_name,
	   (char *)(address + offset % sb->s_blksz + sizeof(__ino_t)),
	   sb->s_direntsz - sizeof(__ino_t));

    dirent->d_name[sb->s_direntsz - sizeof(__ino_t)] = 0;

    dirent->d_namlen = strlen(dirent->d_name);

    fdt[fd].offset += sb->s_direntsz;
    i_unlock(i_no);
    buf_release(buf_no);
    SYSCALL_EPILOG();
    return 1;
}
