/* Copyright (C) 1991, 1992, 1993 Free Software Foundation, Inc.
This file is part of the GNU C Library.

The GNU C Library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The GNU C Library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with the GNU C Library; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 675 Mass Ave,
Cambridge, MA 02139, USA.  */

#include <ansidecl.h>
#include <stddef.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <syscall.h>

#include "direct.h"

int
DEFUN(__getdirentries, (fd, buf, nbytes, basep),
      int fd AND char *buf AND size_t nbytes AND off_t basep)
{
    int nbytes_read = 0;

    while (nbytes >= sizeof(struct direct))
    {
	if (__readdir(fd, buf, 1) <= 0)
	    return nbytes_read;

	if (((struct direct *)buf)->d_fileno)
	{
	    nbytes      -= sizeof(struct direct);
	    nbytes_read += sizeof(struct direct);
	    buf         += sizeof(struct direct);
	}
    }

    return nbytes_read;
}
