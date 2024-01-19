/* Copyright (C) 1992, 1993 Free Software Foundation, Inc.
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

#include <sysdeps/unix/sysdep.h>

#define ENTRY(name)                                                           \
  .globl _##name;                                                             \
  .align 2;                                                                   \
  _##name##:

#define PSEUDO(name, syscall_name, args)                                      \
  .text;                                                                      \
  .globl syscall_error;                                                       \
  ENTRY (name)                                                                \
    movl $SYS_##syscall_name, %eax;                                           \
    int $0x30;                                                                \
    test %eax, %eax;                                                          \
    jl syscall_error;

#define r0              %eax    /* Normal return-value register.  */
#define r1              %edx    /* Secondary return-value register.  */
#define scratch         %ecx    /* Call-clobbered register for random use.  */
#define MOVE(x,y)       movl x, y
