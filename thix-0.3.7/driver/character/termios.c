/* termios.c -- The standard terminal interface.  POSIX.1.  */

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
#include <thix/sched.h>
#include <thix/vtty.h>
#include <thix/errno.h>
#include <thix/fs.h>
#include <thix/stat.h>
#include <thix/gendrv.h>
#include <thix/generic.h>


/*
 * This functions checks if a *valid* user file descriptor (ufd) points  to
 * a terminal device. It returns the minor vtty number if it does, -1 if it
 * doesn't. Please check the user file descriptor *before* calling this.
 *
 * Note:
 * Unfortunately, this is *very* vtty dependant. It should  be  changed  as
 * soon as possible.
 */

static int
isatty(int ufd)
{
    int fd, i_no;

    fd   = this->ufdt[ufd];
    i_no = fdt[fd].inode;

    if (S_ISCHR(i_vect[i_no].i_mode) &&
	major(i_vect[i_no].i_rdev) == VTTY_MAJOR)
	return minor(i_vect[i_no].i_rdev);
    else
	return -1;
}


int
sys_tcgetattr(int ufd, struct termios *termios_p)
{
    int vt, retval;

    DEBUG(5, "(%d,%x)\n", ufd, termios_p);

    if (!ok_to_write_area(termios_p, sizeof(struct termios)))
	return -EFAULT;

    check_ufd(ufd);

    if ((vt = isatty(ufd)) == -1)
	return -ENOTTY;

    SYSCALL_PROLOG();
    retval = vtty_getattr(vt, termios_p);
    SYSCALL_EPILOG();

    return retval;
}


int
sys_tcsetattr(int ufd, int when, struct termios *termios_p)
{
    int vt;

    DEBUG(5, "(%d,%x)\n", ufd, termios_p);

    if (!ok_to_read_area(termios_p, sizeof(struct termios)))
	return -EFAULT;

    check_ufd(ufd);

    if ((vt = isatty(ufd)) == -1)
	return -ENOTTY;

    /* These are the flags that we are *really* using.  */
    when &= (TCSANOW | TCSADRAIN | TCSAFLUSH);

    switch (when)
    {
	case TCSANOW:

	    /* Set the attributes NOW.  */
	    return vtty_setattr(vt, termios_p);

	case TCSADRAIN:

	    /* If we'll ever queue some output, this have to be changed.
	       Until then, we just set it through TCSAFLUSH.  */

	    /* Wait until all queued output has been written.  */

	case TCSAFLUSH:

	    /* Like TCSADRAIN, but also discards any queued input.  */
	    vtty_discard_input(vt);
	    return vtty_setattr(vt, termios_p);

	default:

	    return -EINVAL;
    }

    return 0;
}


int
sys_tcsendbreak(int ufd, int duration)
{
    int vt;

    check_ufd(ufd);

    if ((vt = isatty(ufd)) == -1)
	return -ENOTTY;

    return 0;
}


int
sys_tcdrain(int ufd)
{
    int vt;

    check_ufd(ufd);

    if ((vt = isatty(ufd)) == -1)
	return -ENOTTY;

    return 0;
}


int
sys_tcflush(int ufd, int queue)
{
    int vt;

    check_ufd(ufd);

    if ((vt = isatty(ufd)) == -1)
	return -ENOTTY;

    switch(queue)
    {
	case TCIFLUSH:
	    return vtty_discard_input(vt);

	case TCOFLUSH:
	    /* If we'll ever queue some output, it must be discarded here.  */
	    break;

	case TCIOFLUSH:
	    /* If we'll ever queue some output, it must be discarded here.  */
	    return vtty_discard_input(vt);

	default:
	    return -EINVAL;
    }

    return 0;
}


int
sys_tcflow(int ufd, int action)
{
    int vt;

    check_ufd(ufd);

    if ((vt = isatty(ufd)) == -1)
	return -ENOTTY;

    return 0;
}
