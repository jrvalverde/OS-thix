/* generic.c -- Generic files management routines.  */

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
#include <thix/sched.h>
#include <thix/buffer.h>
#include <thix/inode.h>
#include <thix/fs.h>
#include <thix/ustat.h>
#include <thix/fcntl.h>
#include <thix/stat.h>
#include <thix/limits.h>
#include <thix/fdc.h>
#include <thix/hdc.h>
#include <thix/mem.h>
#include <thix/kmem.h>
#include <thix/null.h>
#include <thix/klog.h>
#include <thix/lp.h>
#include <thix/gendrv.h>
#include <thix/utime.h>
#include <thix/ioctl.h>
#include <thix/process.h>
#include <thix/device.h>
#include <thix/pipe.h>
#include <thix/dir.h>
#include <thix/regular.h>
#include <thix/generic.h>


fd_struct fdt[SYSTEM_OPEN_MAX];
int ff_fd = 1;


/*
 * System file descriptor entries allocation.
 * A fdt entry is free if it's count is 0.
 */

int
get_fd(void)
{
    int fd;

    for (fd = ff_fd; fd < SYSTEM_OPEN_MAX; fd++)
	if (fdt[fd].count == 0)
	{
	    fdt[fd].count = 1;
	    return ff_fd = fd;
	}

    return -1;
}


/*
 * Release a system file descriptor.
 */

inline void
release_fd(int fd)
{
#ifdef __PARANOIA__
    if (fd < 0 || fd > SYSTEM_OPEN_MAX)
	PANIC("releasing invalid file descriptor (%d)", fd);
#endif
    fdt[fd].count = 0;

    if (fd < ff_fd)
	ff_fd = fd;
}


/*
 * Allocate an user file descriptor.
 */

int
get_ufd(int first)
{
    int ufd;

    for (ufd = max(first, this->ff_ufd); ufd < OPEN_MAX; ufd++)
	if (this->ufdt[ufd] == 0)
	{
	    /* Prevent reallocation.  */
	    this->ufdt[ufd] = 0xFFFF;
	    return this->ff_ufd = ufd;
	}

    return -1;
}


/*
 * Release an user file descriptor.
 */

inline void
release_ufd(int ufd)
{
    this->ufdt[ufd] = 0;

    if (ufd < this->ff_ufd)
	this->ff_ufd = ufd;
}


/*
 * Returns 1 if an user file descriptor is marked 'close on exec'.
 */

inline int
is_close_on_exec(int ufd)
{
    return this->close_on_exec[ufd >> 5] & (1 << (ufd & 31));
}


/*
 * Manipulate the user file descriptors 'close on exec' flag.
 */

inline void
close_on_exec(int ufd, int status)
{
    if (status)
	this->close_on_exec[ufd >> 5] |= (1 << (ufd & 31));
    else
	this->close_on_exec[ufd >> 5] &= ~(1 << (ufd & 31));
}


int
io_blocks(int offset, int count, int fsize, int blksz)
{
    int blocks;

    count = min(count, fsize - offset);

    if (count == 0 || offset >= fsize)
	return 0;

    if (offset / blksz == (offset + count - 1) / blksz)
	return 1;

    count -= blksz - (offset % blksz);
    blocks = 1 + (count / blksz) + ((count % blksz) ? 1 : 0);

    return blocks;
}


/*
 * Check if 'path' is a valid string.  Used in checking system calls
 * arguments.
 */

int
path_bad(char *path)
{
    if (!ok_to_read_string(path))
	return -EFAULT;

    if (!(path && *path))
	return -ENOENT;

    if (strlen(path) + 1 > PATH_MAX)
	return -ENAMETOOLONG;

    return 0;
}


/*
 * Check file permissions.
 */

int
permitted(int i_no, int requested_mode, int ruid_flag)
{
    __uid_t uid;
    __gid_t gid;
    int mode, rights;

    if (ruid_flag)
	uid = this->ruid, gid = this->rgid;
    else
	uid = this->euid, gid = this->egid;

    mode = i_vect[i_no].i_mode;

    if (uid == 0)
	return !(S_ISREG(mode)           &&
		 (requested_mode & X_OK) &&
		 ((mode & 0111) == 0));
    else
    {
	if (uid == i_vect[i_no].i_uid)
	    rights = (mode >> 6) & 7;
	else
	    if (gid == i_vect[i_no].i_gid)
		rights = (mode >> 3) & 7;
	    else
		rights = mode & 7;

	return ((rights & requested_mode) == requested_mode);
    }
}


/*
 * The open() system call.
 */

int
sys_open(char *filename, int flags, __mode_t mode)
{
    char *name;
    superblock *sb;
    int previous_exec_denied_status = 0;
    int fd, ufd, device, result = -1, offset, open_error;
    int i_no, dir_i_no, empty_dir_slot, dir_offset, required_perm = 0;


    DEBUG(4, "(%s,%o,%o)\n", filename, flags, mode);

    if ((flags & 3) == (O_WRONLY | O_RDWR))
	return -EINVAL;

    if ((fd = get_fd()) == -1)
	return -ENFILE;

    if ((ufd = get_ufd(0)) == -1)
    {
	release_fd(fd);
	return -EMFILE;
    }

    i_no = __namei(filename, &empty_dir_slot, &dir_i_no, &dir_offset, &name);

    if (i_no < 0)
    {
	release_fd(fd);
	release_ufd(ufd);

	if (dir_i_no)
	    i_get_rid_of(dir_i_no);

	return i_no;
    }

    if (i_no == 0)
    {
	if (flags & O_CREAT)
	{
	    if (dir_i_no == 0)
	    {
		release_fd(fd);
		release_ufd(ufd);
		return -EBUSY;
	    }

	    if (!permitted(dir_i_no, W_OK | X_OK, 0))
	    {
		release_fd(fd);
		release_ufd(ufd);

		if (dir_i_no)
		    i_get_rid_of(dir_i_no);

		return -EPERM;
	    }

	    if ((i_no = i_alloc(device = i_vect[dir_i_no].device)) == 0)
	    {
		release_fd(fd);
		release_ufd(ufd);
		i_get_rid_of(dir_i_no);
		return -ENOSPC;
	    }

	    sb = sb(device);
	    offset = find_empty_dir_slot(sb, dir_i_no,
					(empty_dir_slot!=-1)?empty_dir_slot:0);

	    if (setup_directory_entry(sb, dir_i_no, offset, i_no, name) == 0)
	    {
		release_fd(fd);
		release_ufd(ufd);
		i_vect[i_no].i_nlinks--;
		i_get_rid_of2(dir_i_no, i_no);
		return -ENOSPC;
	    }

	    i_vect[i_no].i_mode = (mode & ~this->umask & ~S_IFMT) | S_IFREG;

	    i_set_mtime(dir_i_no);
	    i_set_ctime(dir_i_no);

	    i_vect[dir_i_no].modified = 1;
	    i_vect[i_no].i_uid = this->euid;
	    i_vect[i_no].i_gid = this->egid;
	    fdt[fd].offset = 0;
	}
	else
	{
	    release_fd(fd);
	    release_ufd(ufd);

	    if (dir_i_no)
		i_get_rid_of(dir_i_no);

	    return -ENOENT;
	}
    }
    else
    {
	if ((flags & 3) == O_RDONLY)
	    required_perm = R_OK;
	else
	    if ((flags & 3) == O_WRONLY)
		required_perm = W_OK;
	    else
		required_perm = R_OK | W_OK;

	open_error = 0;

	previous_exec_denied_status = i_vect[i_no].exec_denied;

	/* It's ugly to put it here, but POSIX specifies it this way.
	   Trying to open a directory with write permission will fail
	   with EISDIR.  No other mode  checking  error  should  take
	   precedence.  */

	if (S_ISDIR(i_vect[i_no].i_mode) &&
	    (flags & (O_RDWR | O_WRONLY | O_TRUNC)))
	    return -EISDIR;

	if (!permitted(i_no, required_perm, 0))
	    open_error = -EACCES;
	else
	{
	    if ((flags & O_CREAT) && (flags & O_EXCL))
		open_error = -EEXIST;

	    if (flags & (O_RDWR | O_WRONLY | O_TRUNC))
		if (i_vect[i_no].write_denied)
		    open_error = -ETXTBSY;
		else
		    i_vect[i_no].exec_denied = 1;
	}

	if (open_error)
	{
	    release_fd(fd);
	    release_ufd(ufd);

	    if (dir_i_no)
		i_get_rid_of(dir_i_no);

	    /* There is no need to restore the exec_denied flag. It hasn't
	       been changed. */
	    i_get_rid_of(i_no);

	    return open_error;
	}
    }

    if (dir_i_no)
	i_get_rid_of(dir_i_no);

    fdt[fd].inode = i_no;
    fdt[fd].flags = flags;

    switch (i_vect[i_no].i_mode & S_IFMT)
    {
	case S_IFDIR: result = dir_open(fd);     break;
	case S_IFREG: result = regular_open(fd); break;
	case S_IFBLK: result = blk_open(fd);     break;
	case S_IFCHR: result = chr_open(fd);     break;
	case S_IFIFO: result = pipe_open(fd);    break;

	default:      PANIC("invalid mode %o", i_vect[i_no].i_mode);
		      break;
    }

    /* We need this because chr_open() could change the inode.  */
    i_no = fdt[fd].inode;

    if (result < 0)
    {
	release_fd(fd);
	release_ufd(ufd);
	i_vect[i_no].exec_denied = previous_exec_denied_status;
	i_get_rid_of(i_no);
	return result;
    }

    i_set_atime(i_no);

    i_vect[i_no].modified = 1;
    this->ufdt[ufd] = fd;
    i_unlock(i_no);
    return ufd;
}


/*
 * The close() system call.
 */

int
sys_close(int ufd)
{
    int fd, i_no;


    DEBUG(5, "(%d,%d) cnt=%d\n", ufd, this->ufdt[ufd],
	  fdt[this->ufdt[ufd]].count);

    check_ufd(ufd);
    fd = this->ufdt[ufd];
    release_ufd(ufd);

    if (--fdt[fd].count)
	return 0;

    i_lock(i_no = fdt[fd].inode);

    switch (i_vect[i_no].i_mode & S_IFMT)
    {
	case S_IFDIR: dir_close(fd);     break;
	case S_IFREG: regular_close(fd); break;
	case S_IFBLK: blk_close(fd);     break;
	case S_IFCHR: chr_close(fd);     break;
	case S_IFIFO: pipe_close(fd);    break;

	default:      PANIC("invalid mode %o", i_vect[i_no].i_mode);
		      break;
    }

    release_fd(fd);
    i_get_rid_of(i_no);
    return 0;
}


/*
 * The read() system call.
 */

int
sys_read(int ufd, char *buf, size_t count)
{
    int fd, i_no, result = 0;
    size_t n, requested = count;


    DEBUG(5, "(%d,%x,%d)\n", ufd, buf, count);

    check_ufd(ufd);

    fd   = this->ufdt[ufd];
    i_no = fdt[fd].inode;

    if ((fdt[fd].flags & 3) == O_WRONLY)
	return -EBADF;

    if (count == 0)
	return 0;

    /* The POSIX tests expect this to be detected before the invalid
       buffer.  */
    if (!ok_to_write_area(buf, count))
	return -EFAULT;

    SYSCALL_PROLOG();

    /* Split the request into smaller pieces, just to make sure we are
       not using up all the buffers (for block devices) or remain with
       interrupts disabled for a very long time (vtty).  Make sure the
       request is not smaller than PIPE_BUF - we want pipe read/writes
       to be atomic if <= PIPE_BUF.  */

    while (count)
    {
	/* Remember the time when this request has been started.  */
	unsigned start_ticks = ticks;

	n = min(SYSTEM_REQUEST_MAX, count);

	switch (i_vect[i_no].i_mode & S_IFMT)
	{
	    case S_IFDIR: result = dir_read(fd, buf, n);     break;
	    case S_IFREG: result = regular_read(fd, buf, n); break;
	    case S_IFBLK: result = blk_read(fd, buf, n);     break;
	    case S_IFCHR: result = chr_read(fd, buf, n);     break;
	    case S_IFIFO: result = pipe_read(fd, buf, n);    break;

	    default:      PANIC("invalid file type (mode) %o",
				i_vect[i_no].i_mode);
	    		  break;
	}

	if (result != n)
	{
	    if (result >= 0)
	    {
		count -= result;
		break;
	    }

	    goto failed;
	}

	count -= n;
	buf   += n;

	/* If the request is a big one (i.e. > SYSTEM_REQUEST_MAX) and
	   the interrupts have not been enabled during the first
	   chunk, it means that the driver works with interrupts
	   disabled (e.g. vtty).  Reschedule in order to give other
	   processes a chance to run.  */

	if (count && ticks == start_ticks)
	    reschedule();
    }

    result = requested - count;

  failed:

    SYSCALL_EPILOG();

    i_set_atime(i_no);
    i_vect[i_no].modified = 1;

    return result;
}


/*
 * The write system call.
 */

int
sys_write(int ufd, char *buf, size_t count)
{
    int fd, i_no, result = 0;
    size_t n, requested = count;


    DEBUG(5, "(%d,%x,%d)\n", ufd, buf, count);

    check_ufd(ufd);

    fd   = this->ufdt[ufd];
    i_no = fdt[fd].inode;

    /* The POSIX tests expect this to be detected before the invalid
       buffer.  */
    if ((fdt[fd].flags & 3) == O_RDONLY)
	return -EBADF;

    if (count == 0)
	return 0;

    if (!ok_to_read_area(buf, count))
	return -EFAULT;

    /* Split the request into smaller pieces.  See the comment in
       sys_read().  */

    while (count)
    {
	/* Remember the time when this request has been started.  */
	unsigned start_ticks = ticks;

	n = min(SYSTEM_REQUEST_MAX, count);

	switch (i_vect[i_no].i_mode & S_IFMT)
	{
	    case S_IFDIR: result = dir_write(fd, buf, n);     break;
	    case S_IFREG: result = regular_write(fd, buf, n); break;
	    case S_IFBLK: result = blk_write(fd, buf, n);     break;
	    case S_IFCHR: result = chr_write(fd, buf, n);     break;
	    case S_IFIFO: result = pipe_write(fd, buf, n);    break;

	    default:      PANIC("invalid file type (mode) %o",
				i_vect[i_no].i_mode);
			  break;
	}

	if (result != n)
	{
	    if (result >= 0)
	    {
		count -= result;
		break;
	    }

	    goto failed;
	}

	count -= n;
	buf   += n;

	/* See the comment in sys_read().  */
	if (count && ticks == start_ticks)
	    reschedule();
    }

    result = requested - count;

  failed:

    if (result > 0)
    {
	i_set_mtime(i_no);
	i_vect[i_no].i_ktime = seconds;
	i_vect[i_no].modified = 1;
    }

    return result;
}


/*
 * The lseek() system call.
 */

int
sys_lseek(int ufd, __off_t offset, int whence)
{
    int fd, type, i_no, size, new_offset;


    DEBUG(5, "(%d,%d,%d)\n", ufd, offset, whence);

    check_ufd(ufd);
    fd = this->ufdt[ufd];

    type = i_vect[i_no = fdt[fd].inode].i_mode & S_IFMT;

    if (type != S_IFREG && whence == SEEK_END)
	return -ESPIPE;

    size = i_vect[i_no].i_size;

    switch (whence)
    {
	case SEEK_SET: new_offset = offset; break;
	case SEEK_CUR: new_offset = fdt[fd].offset + offset; break;
	case SEEK_END: new_offset = size + offset; break;
	default      : return -EINVAL;
    }

    if (new_offset < 0)
	return -EINVAL;

    switch (type)
    {
	case S_IFDIR: return -EINVAL;
	case S_IFREG: break;
	case S_IFBLK: if (blk_lseek(fd, new_offset) < 0) return -ESPIPE;
		      break;
	case S_IFCHR: if (chr_lseek(fd, new_offset) < 0) return -ESPIPE;
		      break;
	case S_IFIFO: return -ESPIPE;

	default:      PANIC("invalid file type (mode) %o",
			    i_vect[i_no].i_mode);
		      break;
    }

    return fdt[fd].offset = new_offset;
}


/*
 * The dup() system call.
 */

int
sys_dup(int ufd)
{
    int fd, new_ufd;


    DEBUG(5, "(%d)\n", ufd);

    check_ufd(ufd);

    if ((new_ufd = get_ufd(0)) == -1)
	return -EMFILE;

    fd = this->ufdt[new_ufd] = this->ufdt[ufd];
    fdt[fd].count++;

    return new_ufd;
}


/*
 * The dup2() system call.
 */

int
sys_dup2(int ufd, int ufd2)
{
    int fd;

    DEBUG(5, "(%d,%d)\n", ufd, ufd2);

    check_ufd(ufd);

    if (ufd2 < 0 || ufd2 >= OPEN_MAX)
	return -EINVAL;

    if (this->ufdt[ufd2])
	sys_close(ufd2);

    fd = this->ufdt[ufd2] = this->ufdt[ufd];
    fdt[fd].count++;

    return ufd2;
}


/*
 * The link() system call.
 */

int
sys_link(char *from, char *to)
{
    char *name;
    superblock *sb;
    int f_i_no, t_i_no, dir_i_no, empty_dir_slot, dir_offset, offset;


    DEBUG(4, "(%s,%s)\n", from, to);

    f_i_no = namei(from);

    if (f_i_no < 0)
	return f_i_no;

    if (S_ISDIR(i_vect[f_i_no].i_mode))
    {
	i_get_rid_of(f_i_no);
	return -EPERM;
    }

    if (i_vect[f_i_no].i_nlinks == LINK_MAX)
    {
	i_get_rid_of(f_i_no);
	return -EMLINK;
    }

    i_vect[f_i_no].i_nlinks++;

    /* Avoid deadlocks.  */
    i_unlock(f_i_no);

    t_i_no = __namei(to, &empty_dir_slot, &dir_i_no, &dir_offset, &name);

    if (t_i_no)
    {
	i_vect[f_i_no].i_nlinks--;
	i_get_rid_of(f_i_no);

	if (dir_i_no)
	    i_get_rid_of(dir_i_no);

	if (t_i_no > 0)
	{
	    i_get_rid_of(t_i_no);
	    return -EEXIST;
	}
	else
	    return t_i_no;
    }

    if (dir_i_no == 0)
    {
	i_vect[f_i_no].i_nlinks--;
	i_get_rid_of(f_i_no);
	return -EBUSY;
    }

    if (!permitted(dir_i_no, W_OK | X_OK, 0))
    {
	i_vect[f_i_no].i_nlinks--;
	i_get_rid_of2(f_i_no, dir_i_no);
	return -EACCES;
    }

    if (i_vect[f_i_no].device != i_vect[dir_i_no].device)
    {
	i_vect[f_i_no].i_nlinks--;
	i_get_rid_of2(f_i_no, dir_i_no);
	return -EXDEV;
    }

    sb = sb(i_vect[dir_i_no].device);
    offset = find_empty_dir_slot(sb, dir_i_no,
				 (empty_dir_slot != -1) ? empty_dir_slot : 0);

    if (setup_directory_entry(sb, dir_i_no, offset, f_i_no, name) == 0)
    {
	i_vect[f_i_no].i_nlinks--;
	i_get_rid_of2(f_i_no, dir_i_no);
	return -ENOSPC;
    }

    i_set_ctime(f_i_no);
    i_set_mtime(dir_i_no);
    i_set_ctime(dir_i_no);

    i_vect[f_i_no].modified = 1;
    i_vect[dir_i_no].modified = 1;

    i_get_rid_of2(f_i_no, dir_i_no);
    return 0;
}


/*
 * The unlink() system call.
 */

int
sys_unlink(char *filename)
{
    char *name;
    int i_no, empty_dir_slot, dir_i_no, dir_offset;


    DEBUG(4, "(%s)\n", filename);

    i_no = __namei(filename, &empty_dir_slot, &dir_i_no, &dir_offset, &name);

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

    if (S_ISDIR(i_vect[i_no].i_mode))
    {
	i_get_rid_of2(dir_i_no, i_no);
	return -EPERM;
    }

    if (!permitted(dir_i_no, W_OK | X_OK, 0))
    {
	i_get_rid_of2(dir_i_no, i_no);
	return -EACCES;
    }

    if ((i_vect[dir_i_no].i_mode & S_ISVTX) == S_ISVTX)
	if (!superuser() && this->euid != i_vect[i_no].i_uid)
	{
	    i_get_rid_of2(dir_i_no, i_no);
	    return -EACCES;
	}

    setup_directory_entry(sb(i_vect[dir_i_no].device),
			  dir_i_no, dir_offset, 0, NULL);

    i_vect[i_no].i_nlinks--;

    i_set_ctime(i_no);
    i_set_mtime(dir_i_no);
    i_set_ctime(dir_i_no);

    i_vect[i_no].modified     =
    i_vect[dir_i_no].modified = 1;

    i_get_rid_of2(dir_i_no, i_no);
    return 0;
}


/*
 * Copy the inode structure into 'statbuf'.  Used by both stat & fstat.
 */

void
inode2stat(int i_no, struct stat *statbuf)
{
    superblock *sb = sb(i_vect[i_no].device);

    statbuf->st_mode  = i_vect[i_no].i_mode;
    statbuf->st_nlink = i_vect[i_no].i_nlinks;

    statbuf->st_uid   = i_vect[i_no].i_uid;
    statbuf->st_gid   = i_vect[i_no].i_gid;

    statbuf->st_rdev  = i_vect[i_no].i_rdev;

    statbuf->st_atime = i_vect[i_no].i_atime;
    statbuf->st_mtime = i_vect[i_no].i_mtime;
    statbuf->st_ctime = i_vect[i_no].i_ctime;

    statbuf->st_atime_usec = i_vect[i_no].i_atime_usec;
    statbuf->st_mtime_usec = i_vect[i_no].i_mtime_usec;
    statbuf->st_ctime_usec = i_vect[i_no].i_ctime_usec;

    statbuf->st_ino = i_vect[i_no].dinode;
    statbuf->st_dev = i_vect[i_no].device;

    statbuf->st_size    = i_vect[i_no].i_size;
    statbuf->st_blocks  = i_vect[i_no].i_blocks;
    statbuf->st_blksize = sb->s_blksz;
}


/*
 * The stat() system call.
 */

int
sys_stat(char *filename, struct stat *statbuf)
{
    int i_no;


    if (!ok_to_write_area(statbuf, sizeof(struct stat)))
	return -EFAULT;

    DEBUG(5, "(%s,%x)\n", filename, statbuf);

    i_no = namei(filename);

    if (i_no < 0)
	return i_no;

    SYSCALL_PROLOG();
    inode2stat(i_no, statbuf);
    SYSCALL_EPILOG();

    i_get_rid_of(i_no);
    return 0;
}


/*
 * The fstat() system call.
 */

int
sys_fstat(int ufd, struct stat *statbuf)
{
    check_ufd(ufd);

    DEBUG(5, "(%d,%x)\n", ufd, statbuf);

    if (!ok_to_write_area(statbuf, sizeof(struct stat)))
	return -EFAULT;

    SYSCALL_PROLOG();
    inode2stat(fdt[this->ufdt[ufd]].inode, statbuf);
    SYSCALL_EPILOG();

    return 0;
}


/*
 * The lstat() system call.
 */

int
sys_lstat(char *filename, struct stat *statbuf)
{
    return sys_stat(filename, statbuf);
}


/*
 * We should check 'device' to make sure is a valid file system.  Use the
 * magic number for this.
 */

int
sys_ustat(__dev_t device, struct ustat *ubuf)
{
    int retval;
    superblock sb;

    DEBUG(2, "not implemented\n");
    return -ENOSYS;

    if (!ok_to_write_area(ubuf, sizeof(struct ustat)))
	return -EFAULT;

    /* FIXME: drv_open() should be called!  */
/*
    drv_read(device, 0, (char *)&sb);
*/
    if (sb.s_magic == TFS_MAGIC_V02)
    {
	SYSCALL_PROLOG();
	ubuf->f_tfree  = sb.s_bfree;
	ubuf->f_tinode = sb.s_ifree;
	memcpy(ubuf->f_fname, sb.s_fname, sizeof(sb.s_fname));
	SYSCALL_EPILOG();
	retval = 0;
    }
    else
	retval = -EINVAL;

    if (can_close_device(device, -1) && !mounted(device))
    {
	buf_sync(device, 1);
	drv_close(device);
    }

    return retval;
}


void
sb2statfs(superblock *sb, struct statfs *statfsbuf)
{
    int total_blocks = (sb->s_clusters - 1) * sb->s_bcluster +
		       (sb->s_lcflag ? sb->s_blcluster : sb->s_bcluster);

    statfsbuf->f_bsize   = sb->s_blksz;
    statfsbuf->f_blocks  = total_blocks;
    statfsbuf->f_bfree   = sb->s_bfree;
    statfsbuf->f_bavail  = total_blocks;
    statfsbuf->f_files   = sb->s_clusters * sb->s_icluster;
    statfsbuf->f_ffree   = sb->s_ifree;
    statfsbuf->f_namelen = sb->s_direntsz - sizeof(__ino_t);
}


/*
 * The statfs system call.  Retreive information about file systems.
 */

int
sys_statfs(char *path, struct statfs *statfsbuf)
{
    int i_no;

    if (!ok_to_write_area(statfsbuf, sizeof(struct statfs)))
	return -EFAULT;

    DEBUG(5, "(%s,%x)\n", path, statfsbuf);

    i_no = namei(path);

    if (i_no < 0)
	return i_no;

    SYSCALL_PROLOG();
    sb2statfs(sb(i_vect[i_no].device), statfsbuf);
    SYSCALL_EPILOG();

    i_get_rid_of(i_no);
    return 0;
}


/*
 * The fstatfs system call.  Retreive information about file systems.
 */

int
sys_fstatfs(int ufd, struct statfs *statfsbuf)
{
    check_ufd(ufd);

    if (!ok_to_write_area(statfsbuf, sizeof(struct statfs)))
	return -EFAULT;

    DEBUG(5, "(%d,%x)\n", ufd, statfsbuf);

    SYSCALL_PROLOG();
    sb2statfs(sb(i_vect[fdt[this->ufdt[ufd]].inode].device), statfsbuf);
    SYSCALL_EPILOG();

    return 0;
}


/*
 * The utime() system call.
 */

int
sys_utime(char *filename, struct utimbuf *times)
{
    int i_no;


    if (!ok_to_read_area(times, sizeof(struct utimbuf)))
	return -EFAULT;

    DEBUG(4, "(%s,%x)\n", filename, times);

    i_no = namei(filename);

    if (i_no < 0)
	return i_no;

    if (times == (struct utimbuf *)USER_NULL)
	if (permitted(i_no, W_OK, 0) || i_vect[i_no].i_uid == this->euid)
	{
	    i_set_atime(i_no);
	    i_set_mtime(i_no);
	}
	else
	{
	    i_get_rid_of(i_no);
	    return -EACCES;
	}
    else
	if (superuser() || i_vect[i_no].i_uid == this->euid)
	{
	    i_vect[i_no].i_atime = times->actime;
	    i_vect[i_no].i_mtime = times->modtime;
	    i_vect[i_no].i_atime_usec = 0;
	    i_vect[i_no].i_mtime_usec = 0;
	}
	else
	{
	    i_get_rid_of(i_no);
	    return -EPERM;
	}

    i_set_ctime(i_no);
    i_vect[i_no].modified = 1;
    i_get_rid_of(i_no);
    return 0;
}


/*
 * The umask() system call.
 */

int
sys_umask(__mode_t mask)
{
    int old_mask = this->umask;


    DEBUG(4, "(%o)\n", mask);

    this->umask = mask & 0777;
    return old_mask;
}


/*
 * The fcntl() system call.
 */

int
sys_fcntl(int ufd, int cmd, int arg)
{
    int i_no, fd, new_ufd;


    DEBUG(4, "(%d,%d,%d)\n", ufd, cmd, arg);

    check_ufd(ufd);

    switch (cmd)
    {
	case F_DUPFD:   if (arg < 0 || arg >= OPEN_MAX)
			    return -EINVAL;

			if ((new_ufd = get_ufd(arg)) == -1)
			    return -EMFILE;

			fd = this->ufdt[new_ufd] = this->ufdt[ufd];
			fdt[fd].count++;
			return new_ufd;

	case F_GETFD:   return is_close_on_exec(ufd);

	case F_SETFD:   close_on_exec(ufd, arg & 1);
			return 0;

	case F_GETFL:   return fdt[this->ufdt[ufd]].flags;

	case F_SETFL:   arg &= (O_APPEND | O_NONBLOCK);
			fdt[this->ufdt[ufd]].flags = arg;
			i_no = fdt[this->ufdt[ufd]].inode;

			if (i_vect[i_no].i_rdev)
			    drv_fcntl(i_vect[i_no].i_rdev, arg);

			return 0;

	case F_GETLK:
	case F_SETLK:
	case F_SETLKW:
			DEBUG(2, "F_GETLK, F_SETLK & F_SETLKW"
			      "are not implemented!\n");
			return -EINVAL;

/*
	case F_GETOWN:
	case F_SETOWN:
*/
	default:
			DEBUG(2, "invalid cmd %x\n", cmd);
			return -EINVAL;
    }

    /* Not reached.  */
    return 0;
}


/*
 * The access() system call.
 */

int
sys_access(char *filename, int mode)
{
    int i_no;


    DEBUG(4, "(%s,%o)\n", filename, mode);

    /* Force namei() to use ruid/rgid.  */
    this->access_flag = 1;
    i_no = namei(filename);
    this->access_flag = 0;

    if (i_no < 0)
	return i_no;

    if (mode > (R_OK | W_OK | X_OK) || mode < 0)
    {
	i_get_rid_of(i_no);
	return -EINVAL;
    }

    if (!permitted(i_no, mode, 1))
    {
	i_get_rid_of(i_no);
	return -EACCES;
    }

    i_get_rid_of(i_no);
    return 0;
}


int
do_chmod(int i_no, __mode_t mode)
{
    if (superuser() || i_vect[i_no].i_uid == this->euid)
    {
	i_set_ctime(i_no);
	i_vect[i_no].i_mode   = (i_vect[i_no].i_mode & S_IFMT) |
				(mode & ~S_IFMT);
	i_vect[i_no].modified = 1;
	return 0;
    }

    return -EPERM;
}


/*
 * The chmod() system call.
 */

int
sys_chmod(char *filename, __mode_t mode)
{
    int i_no, result;


    DEBUG(4, "(%s,%o)\n", filename, mode);

    i_no = namei(filename);

    if (i_no < 0)
	return i_no;

    result = do_chmod(i_no, mode);

    i_get_rid_of(i_no);

    return result;
}


/*
 * The fchmod() system call.
 */

int
sys_fchmod(int ufd, __mode_t mode)
{
    DEBUG(4, "(%d,%o)\n", ufd, mode);

    check_ufd(ufd);

    return do_chmod(fdt[this->ufdt[ufd]].inode, mode);
}


int
do_chown(int i_no, __uid_t owner, __gid_t group)
{
    if (owner == (__uid_t)-1) owner = i_vect[i_no].i_uid;
    if (group == (__uid_t)-1) group = i_vect[i_no].i_gid;

    if (superuser() ||
	(i_vect[i_no].i_uid == this->euid &&
	 i_vect[i_no].i_uid == owner      &&
	 i_vect[i_no].i_gid == group))
    {
	i_set_ctime(i_no);
	i_vect[i_no].i_mode  &= ~(S_ISUID | S_ISGID);
	i_vect[i_no].i_uid    = owner;
	i_vect[i_no].i_gid    = group;
	i_vect[i_no].modified = 1;
	return 0;
    }

    return -EPERM;
}


/*
 * The chown() system call.
 */

int
sys_chown(char *filename, __uid_t owner, __gid_t group)
{
    int i_no, result;


    DEBUG(4, "(%s,%d,%d)\n", filename, owner, group);

    i_no = namei(filename);

    if (i_no < 0)
	return i_no;

    result = do_chown(i_no, owner, group);

    i_get_rid_of(i_no);

    return result;
}


/*
 * The fchown() system call.
 */

int
sys_fchown(int ufd, __uid_t owner, __gid_t group)
{
    DEBUG(4, "(%d,%d,%d)\n", ufd, owner, group);

    check_ufd(ufd);

    return do_chown(fdt[this->ufdt[ufd]].inode, owner, group);
}


/*
 * The truncate() system call.
 */

int
sys_truncate(char *filename, size_t length)
{
    int i_no;


    DEBUG(4, "(%s,%d)\n", filename, length);

    i_no = namei(filename);

    if (i_no < 0)
	return i_no;

    if (!S_ISREG(i_vect[i_no].i_mode))
    {
	i_get_rid_of(i_no);
	return -EPERM;
    }

    if (i_vect[i_no].write_denied)
    {
	i_get_rid_of(i_no);
	return -ETXTBSY;
    }

    i_truncate(i_no, length);

    i_get_rid_of(i_no);
    return 0;
}


/*
 * The ftruncate() system call.
 */

int
sys_ftruncate(int ufd, size_t length)
{
    int i_no;

    check_ufd(ufd);

    DEBUG(4, "(%d,%d)\n", ufd, length);

    i_no = fdt[this->ufdt[ufd]].inode;

    if (!S_ISREG(i_vect[i_no].i_mode))
	return -EPERM;

    if (i_vect[i_no].write_denied)
	return -ETXTBSY;

    i_truncate(i_no, length);
    return 0;
}


/*
 * The symlink() system call.  Not implemented yet.
 */

int
sys_symlink(char *oldname, char *newname)
{
    DEBUG(2, "Symbolic links are not implemented yet.\n");
    return -ENOSYS;
}


/*
 * The readlink() system call.
 */

int
sys_readlink(char *filename, char *buffer, size_t size)
{
    DEBUG(2, "Symbolic links are not implemented yet.\n");
    return -ENOSYS;
}


/*
 * The chroot() system call.  Not implemented yet.
 */

int
sys_chroot(char *path)
{
    DEBUG(2, "sys_chroot() is not implemented yet.\n");
    return -ENOSYS;
}


/*
 * The ioctl() system call.
 */

int
sys_ioctl(int ufd, int cmd, char *argp)
{
    int fd, i_no, mode, result = 0;


    DEBUG(4, "(%d,%d,%x)\n", ufd, cmd, argp);

    check_ufd(ufd);

    fd   = this->ufdt[ufd];
    i_no = fdt[fd].inode;
    mode = i_vect[i_no].i_mode;

    if (!S_ISBLK(mode) && !S_ISCHR(mode))
	return -EPERM;

    switch (cmd)
    {
	case TIOCGWINSZ:
	    if (!ok_to_write_area(argp, sizeof(struct winsize)))
		return -EFAULT;
	    SYSCALL_PROLOG();
	    break;

	case TIOCSWINSZ:
	    if (!ok_to_read_area(argp, sizeof(struct winsize)))
		return -EFAULT;
	    break;

	default:
	    return -EINVAL;
    }

    switch (mode & S_IFMT)
    {
	case S_IFBLK: result = blk_ioctl(fd, cmd, argp); break;
	case S_IFCHR: result = chr_ioctl(fd, cmd, argp); break;
    }

    SYSCALL_EPILOG();
    return result;
}


/*
 * The select() system call.  Not implemented yet.
 */

int
sys_select(void)
{
    DEBUG(2, "sys_select() is not implemented yet.\n");
    return -ENOSYS;
}


/*
 * Clean up all the fs-related data structures before rebooting.  Return
 * the number of errors encountered.
 */

int
fs_cleanup(void)
{
    int fd, ufd;
    int errors = 0;

    /* Close "by hand" all the system open file  descriptors.  Processes
       that have been stopped by stop_all() might still have open files.
       In order to be able to close them all, we have to map  them  into
       the current process file descriptor table.  Well, it's  not  that
       hard :-) */
    for (fd = 2; fd < SYSTEM_OPEN_MAX; fd++)
	if (fdt[fd].count)
	{
	    printk("WARNING: closing internal file descriptor %d.\n", fd);
	    errors++;

	    /* Ignore dups.  */
	    fdt[fd].count = 1;
	    ufd = get_ufd(0);

	    if (ufd)
	    {
		printk("WARNING: bad ufd (%x).\n", ufd);
		errors++;
	    }

	    this->ufdt[ufd] = fd;
	    printk("inode %x -> nref = %x\n", fdt[fd].inode,
					      i_vect[fdt[fd].inode].nref);
	    i_unlock(fdt[fd].inode);
	    sys_close(ufd);
	}

    /* Sync the file systems.  */
    sys_sync();

    /* Unmount the root file system.  The other file systems should  be
       unmounted from user level.  Don't want to bother the kernel with
       this.  */
    umount_root();

    return errors;
}

/*
 * Initialize the file system stuff.  Initialize the mount tables and
 * then mount the root file system.
 */

void
fs_init(void)
{
    /* The swapper is initially used to initialize some  device  drivers
       and the file system.  This is the first process that can actually
       read from the disk, since IDLE cannot sleep.  */

    /* Register the block device drivers.  */
    fdc_init();
    hdc_init();

    /* Register the character device drivers.  */
    null_init();
    mem_init();
    kmem_init();
    klog_init();
    lp_init();

    memset(fdt, 0, sizeof(fdt));

    mount_init();

    mount_root();
}
