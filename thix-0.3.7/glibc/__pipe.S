/* Copyright (C) 1991, 1992 Free Software Foundation, Inc.
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

#include <sysdep.h>

SYSCALL__ (pipe, 1)
	ret

/*
 * Just temporary until I will figure out how to make this Makefile be
 * considered in the make process.
 */

SYSCALL__ (readdir, 3)
	ret

SYSCALL (umount, 1)
	ret

SYSCALL (mount, 3)
	ret

SYSCALL (setdomainname, 2)
	ret

SYSCALL (ustat, 2)
	ret

SYSCALL (swapoff, 1)
	ret

SYSCALL (getprocinfo, 2)
	ret

SYSCALL (getsysinfo, 1)
	ret

SYSCALL (sigend, 0)
	ret

SYSCALL (statfs, 2)
	ret

SYSCALL (fstatfs, 2)
	ret

SYSCALL (kctrl, 1)
	ret
