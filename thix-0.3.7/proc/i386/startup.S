/* startup.S -- Thix/i386 startup, LDTs, GDT, ...  */

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


DATA_ARB  = 0x92
STACK_ARB = 0x96
CODE_ARB  = 0x9A


.text

thix_init:
		movw    $0x10, %bx
		movw    %bx, %ds
		movw    %bx, %es

		xorl    %ebx, %ebx
		movl    0xC04(%ebx), %eax
		addl    $0x1000, %eax
		movl    %eax, _KERNEL_TEXT_SIZE
		movl    0xC08(%ebx), %eax
		movl    %eax, _KERNEL_DATA_SIZE
		movl    0xC0C(%ebx), %eax
		movl    %eax, _KERNEL_BSS_SIZE
		movl    0xC10(%ebx), %eax
		movl    %eax, _EXTENDED_MEMORY_SIZE

		lgdt    GDT_DESCRIPTOR
		lidt    IDT_DESCRIPTOR

		movw    $0x18, %ax      /* init stack */
		movw    %ax, %ss
		movl    $0x9FFFC, %esp

		movw    $0x10, %ax
		movw    %ax, %ds
		movw    %ax, %es
		movw    %ax, %fs
		movw    %ax, %gs

		movb    $0x11, %al      /* initialization sequence */
		outb    %al, $0x20      /* send it to 8259A-1 */
		.word   0x00eb, 0x00eb
		outb    %al, $0xA0      /* ... and to 8259A-2 */
		.word   0x00eb, 0x00eb

		movb    $0x20, %al      /* start of hardware */
					/* int's (0x20) */
		outb    %al, $0x21
		.word   0x00eb, 0x00eb

		movb    $0x28, %al      /* start of hardware */
					/* int's 2 (0x28) */
		outb    %al, $0xA1
		.word   0x00eb, 0x00eb

		movb    $0x04, %al      /* 8259-1 is master */
		outb    %al, $0x21
		.word   0x00eb, 0x00eb

		movb    $0x02, %al      /* 8259-2 is slave */
		outb    %al, $0xA1
		.word   0x00eb, 0x00eb

		movb    $0x01, %al      /* 8086 mode for both */
		outb    %al, $0x21
		.word   0x00eb, 0x00eb
		outb    %al, $0xA1
		.word   0x00eb, 0x00eb

		movb    $0xBF, %al
		outb    %al, $0xA1      /* unmask only irq 14 */
		.word   0x00eb, 0x00eb

		movb    $0xB8, %al
		outb    %al, $0x21      /* unmask irq 0, 1, 2, 6 */
		.word   0x00eb, 0x00eb

		inb     $0x60, %al      /* read a key */

		inb     $0x61, %al      /* this is needed to satisfy */
		movb    %al, %ah        /* the hardware.             */
		orb     $0x80, %al
		outb    %al, $0x61
		xchgb   %al, %ah
		outb    %al, $0x61

		cld
		call    _main

infinite:       jmp     infinite


.data

.align  4
		.word   0
GDT_DESCRIPTOR: .word   SYSTEM_PROCESS_MAX * 4 * 8 - 1
		.long   _gdt

.align  4
		.word   0
IDT_DESCRIPTOR: .word   (3 * 16 + 2) * 8 - 1
		.long   _idt


_KERNEL_TEXT_SIZE:
		.long   0

_KERNEL_DATA_SIZE:
		.long   0

_KERNEL_BSS_SIZE:
		.long   0

_EXTENDED_MEMORY_SIZE:
		.long   0

_user_mode_ESP:
		.fill   SYSTEM_PROCESS_MAX, 4, 0

_kernel_mode_ESP:
		.fill   SYSTEM_PROCESS_MAX, 4, 0

.align  4
_gdt:
		.long   0, 0                     /*    0: dummy  descriptor */

		.word   0x100, 0                 /*    8: text   descriptor */
		.byte   0, CODE_ARB, 0xC0, 0

		.word   0xFFFF, 0                /* 0x10: data   descriptor */
		.byte   0, DATA_ARB, 0xCF, 0

		.word   0, 0                     /* 0x18: stack  descriptor */
		.byte   0, STACK_ARB, 0x40, 0

		.fill   60, 8, 0                 /* EMPTY descriptors  */

_ldt_descr:
		.fill   SYSTEM_PROCESS_MAX, 8, 0 /* ldt descriptors */

_tss_descr:
		.fill   SYSTEM_PROCESS_MAX, 8, 0 /* tss descriptors */

_internal_stack_descr:
		.fill   SYSTEM_PROCESS_MAX, 8, 0 /* stack descriptors */


.globl          _gdt
.globl          _ldt_descr
.globl          _tss_descr
.globl          _internal_stack_descr
.globl          _KERNEL_TEXT_SIZE
.globl          _KERNEL_DATA_SIZE
.globl          _KERNEL_BSS_SIZE
.globl          _EXTENDED_MEMORY_SIZE
.globl          _user_mode_ESP
.globl          _kernel_mode_ESP
