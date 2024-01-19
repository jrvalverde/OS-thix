/* pipe.c -- Pipes management routines.  */

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
#include <thix/sleep.h>
#include <thix/buffer.h>
#include <thix/inode.h>
#include <thix/errno.h>
#include <thix/fcntl.h>
#include <thix/stat.h>
#include <thix/signal.h>
#include <thix/limits.h>
#include <thix/gendrv.h>
#include <thix/generic.h>


extern int root_device;


#define pipe_check_readers()                    \
{                                               \
    if (pipe_readers == 0)                      \
    {                                           \
	kill_kpid(current_kpid, SIGPIPE);       \
	pipe_unlock();                          \
	return -EPIPE;                          \
    }                                           \
}


#define pipe_check_writers()                    \
{                                               \
    if (pipe_writers == 0)                      \
	goto pipe_read_exit;                    \
}


#define pipe_lock()     i_lock(i_no)
#define pipe_unlock()   i_unlock(i_no)


#define pipe_size       (i_vect[i_no].i_pipesize)

#define pipe_readers    (i_vect[i_no].i_pipercnt)
#define pipe_writers    (i_vect[i_no].i_pipewcnt)

#define pipe_roff       (i_vect[i_no].i_piperoff)
#define pipe_woff       (i_vect[i_no].i_pipewoff)


/*
 * Open a pipe.
 */

int
pipe_open(int fd)
{
    int i_no  = fdt[fd].inode;
    int flags = fdt[fd].flags;

    DEBUG(4, "(%d)\n", fd);

    pipe_unlock();

    if ((flags & 3) == O_RDONLY)
    {
	pipe_readers++;
	wakeup(&pipe_readers);

	if ((flags & O_NONBLOCK) == 0)
	    while (pipe_writers == 0)
		if (sleep(&pipe_writers, WAIT_PIPE))
		    return -EINTR;
    }
    else
	if ((flags & 3) == O_WRONLY)
	{
	    pipe_writers++;
	    wakeup(&pipe_writers);

	    if ((flags & O_NONBLOCK) == 0)
		while (pipe_readers == 0)
		    if (sleep(&pipe_readers, WAIT_PIPE))
			return -EINTR;
	}
	else
	    return -EPERM;

    return 0;
}


/*
 * Close a pipe.
 */

int
pipe_close(int fd)
{
    int i_no  = fdt[fd].inode;
    int flags = fdt[fd].flags;


    DEBUG(5, "(%d)\n", fd);

    if ((flags & 3) == O_RDONLY)
    {
	if (--pipe_readers == 0)
	    wakeup_and_kill(&pipe_woff, SIGPIPE);
    }
    else
	if ((flags & 3) == O_WRONLY)
	{
	    if (--pipe_writers == 0)
		wakeup(&pipe_roff);
	}
	else
	    PANIC("pipe close error: invalid flags %x\n", flags);

    return 0;
}


/*
 * Read from a pipe.
 */

int
pipe_read(int fd, char *buf, size_t count)
{
    int i_no = fdt[fd].inode, buf_no, boffset, bcount, rcount = 0, blksz;


    DEBUG(5, "(%d,%x,%d) offset=%x\n", fd, buf, count, pipe_roff);

    pipe_lock();
    blksz = sb(i_vect[i_no].device)->s_blksz;

    while (count)
    {
	if (pipe_size == 0)
	{
	    if (rcount || fdt[fd].flags & O_NONBLOCK)
		break;

	    while (pipe_size == 0)
	    {
		pipe_check_writers();
		pipe_unlock();
		wakeup(&pipe_woff);

		if (sleep(&pipe_roff, WAIT_PIPE))
		    return -EINTR;

		pipe_lock();
	    }

	    pipe_check_writers();
	}

	if (pipe_roff >= PIPE_BUF)
	    pipe_roff = 0;

	boffset = pipe_roff % blksz;
	bcount  = min3(count, blksz - boffset, pipe_size);

	if (bcount == 0)
	    break;

	if ((buf_no = rbmap(i_no, pipe_roff)))
	{
	    memcpy(buf + rcount, (void *)(buf_address(buf_no) + boffset),
		   bcount);
	    buf_release(buf_no);
	}
	else
	    PANIC("pipe error\n");

	rcount += bcount, pipe_roff += bcount;
	count  -= bcount, pipe_size -= bcount;
    }

  pipe_read_exit:

    pipe_unlock();
    wakeup(&pipe_woff);
    return rcount;
}


/*
 * Write to a pipe.
 */

int
pipe_write(int fd, char *buf, size_t count)
{
    unsigned address;
    int i_no = fdt[fd].inode, blksz;
    int buf_ok, buf_no = 0, boffset, bcount, wcount = 0;


    DEBUG(5, "(%d,%x,%d) offset=%x\n", fd, buf, count, pipe_woff);

    pipe_lock();
    blksz = sb(i_vect[i_no].device)->s_blksz;

    if (count <= PIPE_BUF)
	while (count > PIPE_BUF - pipe_size)
	{
	    pipe_check_readers();
	    pipe_unlock();
	    if (sleep(&pipe_woff, WAIT_PIPE))
		return (signals() & SIGPIPE) ? -EPIPE : -EINTR;
	    pipe_lock();
	}

    while (count)
    {
	while (pipe_size == PIPE_BUF)
	{
	    pipe_check_readers();
	    pipe_unlock();
	    wakeup(&pipe_roff);
	    if (sleep(&pipe_woff, WAIT_PIPE))
		return (signals() & SIGPIPE) ? -EPIPE : -EINTR;
	    pipe_lock();
	}

	pipe_check_readers();

	if (pipe_woff >= PIPE_BUF)
	    pipe_woff = 0;

	boffset = pipe_woff % blksz;
	bcount = min3(count, blksz - boffset, PIPE_BUF - pipe_size);

	if ((buf_no = wbmap(i_no, pipe_woff)) == 0)
	    break;

	address = (unsigned)buf_address(buf_no);

	if ((bcount < blksz) && ((pipe_woff & ~(blksz - 1)) < pipe_roff))
	    buf_ok = _buf_read(buf_no);
	else
	    buf_ok = 1;

	if (buf_ok)
	{
	    memcpy((void *)(address + boffset), buf + wcount, bcount);
	    buf_write(buf_no);
	}

	wcount += bcount, pipe_woff += bcount;
	count  -= bcount, pipe_size += bcount;
    }

    if (wcount)
	i_vect[i_no].modified = 1;

    pipe_unlock();
    wakeup(&pipe_roff);
    return wcount ? wcount : -ENOSPC;
}


/*
 * The pipe() system call.
 */

int
sys_pipe(int *ufdp)
{
    int i_no;
    int fd0, fd1, ufd0, ufd1;


    DEBUG(4, "(%x)\n", ufdp);

    if (!ok_to_write_area(ufdp, 2 * sizeof(int)))
	return -EFAULT;

    if ((i_no = i_alloc(root_device)) == 0)
	return -ENOSPC;

    if ((fd0 = get_fd()) == -1)
	return -ENFILE;

    if ((fd1 = get_fd()) == -1)
    {
	release_fd(fd0);
	return -ENFILE;
    }

    if ((ufd0 = get_ufd(0)) == -1)
    {
	release_fd(fd0);
	release_fd(fd1);
	return -EMFILE;
    }

    if ((ufd1 = get_ufd(0)) == -1)
    {
	release_fd(fd0);
	release_fd(fd1);
	release_ufd(ufd0);
	return -EMFILE;
    }

    fdt[fd0].flags = O_RDONLY;
    fdt[fd1].flags = O_WRONLY;

    fdt[fd0].inode = i_no;
    fdt[fd1].inode = i_no;

    this->ufdt[ufd0] = fd0;
    this->ufdt[ufd1] = fd1;

    SYSCALL_PROLOG();
    ufdp[0] = ufd0;
    ufdp[1] = ufd1;
    SYSCALL_EPILOG();

    /* FIXME: times should be updated.  */

    pipe_size    = 0;
    pipe_roff    = 0;
    pipe_woff    = 0;
    pipe_readers = 1;
    pipe_writers = 1;

    i_vect[i_no].i_mode   = S_IFIFO;
    i_vect[i_no].i_nlinks = 0;
    i_vect[i_no].nref     = 2;

    DEBUG(6, "fd0  = %x,  fd1 = %x\n", fd0, fd1);
    DEBUG(6, "ufd0 = %x, ufd1 = %x\n", ufd0, ufd1);
    DEBUG(6, "flg0 = %x, flg1 = %x\n", fdt[fd0].flags, fdt[fd1].flags);

    pipe_unlock();

    return 0;
}
