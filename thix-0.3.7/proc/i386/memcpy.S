/* memcpy.S -- Simple i386 memcpy implementation.  */

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


/*
 * gcc has a builtin version of memcpy. This memcpy seems to be only called
 * when assigning structures or when calling memcpy with size not  being  a
 * constant. If size is not a constant, use lmemcpy.
 */

_memcpy:        pushl   %ebp
		movl    %esp, %ebp
		pushl   %edi
		pushl   %esi
		pushl   %ecx

		movl     8(%ebp), %edi          /* destination pointer */
		movl    12(%ebp), %esi          /* source pointer */
		movl    16(%ebp), %ecx          /* size */

		cld
		rep
		movsb

		popl    %ecx
		popl    %esi
		popl    %edi
		popl    %ebp
		ret


.globl          _memcpy
