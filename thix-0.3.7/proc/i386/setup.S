/* setup.S -- i386 specific initializations.  */

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


#include <thix/limits.h>


/*
 * Initialize the i386 paging mechanism.
 */

.align  4
_paging_init:   movl    _kernel_page_dir, %eax
		movl    %eax, %cr3
		movl    %cr0, %eax

		/*
		 * bit 31 - PG:	1 = paging enabled
		 * bit 18 - AM:	1 = alignment mask for CPL=3
		 * bit 16 - WP:	1 = generate protection faults on level 0
		 * bit  5 - NE:	1 = use int 16 for numeric exceptions.
		 * bit  4 - ET:	1 = use 32 bit protocol to talk to the 80387
		 * bit  3 - TS:	1 = 387 context invalid due to a task switch
		 * bit  2 - EM:	1 = 387 emulation on
		 * bit  1 - MP:	1 = math present
		 * bit  0 - PE:	1 = protection enabled
		 */

		andl	$0x00000011, %eax	/* preserve ET & PE.  */
		orl	$0x80050022, %eax	/* set PG, AM, WP, NE & MP.  */

		movl    %eax, %cr0
		ret

/*
 * Set up LDTR and TR in order to be able to perform the first context switch.
 * Also reset the NT flag.
 */

.align  4
_ldt0tr_init:   pushfl                          /* NT = 0 */
		andl    $0xffffbfff, (%esp)
		popfl
		movw    $SYSTEM_PROCESS_MAX * 8, %ax
		lldt    %ax
		movw    $SYSTEM_PROCESS_MAX * 8 * 2, %ax
		ltr     %ax
		ret


_kbd_wait:	.word   0x00eb, 0x00eb
		in      $0x64, %al
		test    $2, %al
		jnz     _kbd_wait
		ret


real_mode_idt:  .long   0, 0

_halt:          cli
		lidt    real_mode_idt

		mov     $0x40000, %ecx
retry:		call    _kbd_wait
		mov     $0xfe, %al
		outb    %al, $0x64
		loopl   retry

		ljmp    $0xFFF0, $0xF000

		/* Something bad happened :-(  */
		jmp	_halt


.globl          _paging_init
.globl          _ldt0tr_init
.globl          _kbd_wait
.globl          _halt
