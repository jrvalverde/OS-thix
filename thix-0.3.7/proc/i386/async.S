/* async.S -- Thix asynchronous events (interrupts and exceptions).  */

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


FIRST_TSS = SYSTEM_PROCESS_MAX * 8 * 2


/* No error code (exception 0 to 7).  */

.align  4
except0:                                /* Divide error, no error code.  */
		pushl   %ebp
		movl    %esp, %ebp

		pushal
		pushl   %ds
		pushl   %es

		movw    $0x10, %ax
		movw    %ax, %ds
		movw    %ax, %es

		call    _divide_error
		jmp     end_except_0_7

.align  4
except1:                                /* Single step, no error code.  */
		pushl   %ebp
		movl    %esp, %ebp

		pushal
		pushl   %ds
		pushl   %es

		movw    $0x10, %ax
		movw    %ax, %ds
		movw    %ax, %es

		call    _single_step
		jmp     end_except_0_7

.align  4
except2:                                /* NMI, no error code.  */
		pushl   %ebp
		movl    %esp, %ebp

		pushal
		pushl   %ds
		pushl   %es

		movw    $0x10, %ax
		movw    %ax, %ds
		movw    %ax, %es

		call    _non_maskable_interrupt
		jmp     end_except_0_7

.align  4
except3:                                /* Breakpoint, no error code.  */
		push    %ebp
		movl    %esp, %ebp

		pushal
		pushl   %ds
		pushl   %es

		movw    $0x10, %ax
		movw    %ax, %ds
		movw    %ax, %es

		call    _breakpoint
		jmp     end_except_0_7

.align  4
except4:                                /* Overflow, no error code.  */
		pushl   %ebp
		movl    %esp, %ebp

		pushal
		pushl   %ds
		pushl   %es

		movw    $0x10, %ax
		movw    %ax, %ds
		movw    %ax, %es

		call    _overflow
		jmp     end_except_0_7

.align  4
except5:                                /* Bound check, no error code.  */
		pushl   %ebp
		movl    %esp, %ebp

		pushal
		pushl   %ds
		pushl   %es

		movw    $0x10, %ax
		movw    %ax, %ds
		movw    %ax, %es

		call    _array_bounds_check
		jmp     end_except_0_7

.align  4
except6:                                /* Invalid op. code, no error code.  */
		pushl   %ebp
		movl    %esp, %ebp

		pushal
		pushl   %ds
		pushl   %es

		movw    $0x10, %ax
		movw    %ax, %ds
		movw    %ax, %es

		call    _invalid_operation_code
		jmp     end_except_0_7

.align  4
except7:                                /* FPU n/a, no error code.  */
		pushl   %ebp
		movl    %esp, %ebp

		pushal
		pushl   %ds
		pushl   %es

		movw    $0x10, %ax
		movw    %ax, %ds
		movw    %ax, %es

		call    _coprocessor_not_available
		jmp     end_except_0_7


end_except_0_7: movl    8(%ebp), %eax   /* Exception 9 may come here      */
		cmpw    $8, %ax         /* because it has no error code.  */
		je      nosigchk_e0_7

		cli
		pushl   $tmp_signum
		call    _issig          /* check for signals */
		addl    $4, %esp
		orl     %eax, %eax
		jz      nosigchk_e0_7

		movl    $0x1022, %edx   /* 0x1022 in user mode (see crt0.s) */
		xchgl   4(%ebp), %edx   /* %edx=%eip,
					   %eax=sigfn,
					   4(%ebp)=0x1022 */
		subl    $12, 16(%ebp)
		movl    tmp_signum, %ecx/* %ecx=sig */
		lds     16(%ebp), %ebx
		movl    %eax, 8(%ebx)   /* SIGNAL function */
		movl    %ecx, 4(%ebx)   /* SIGNAL number */
		movl    %edx, (%ebx)    /* %eip */

nosigchk_e0_7:  popl    %es
		popl    %ds
		popal

		popl    %ebp
		iret

.align  4
except8:                                /* Double fault.  */
		pushl   %ebp
		movl    %esp, %ebp

		pushal
		pushl   %ds
		pushl   %es

		movw    $0x10, %ax
		movw    %ax, %ds
		movw    %ax, %es

		call    _double_fault
		jmp     end_except_8_F

end_except_8_F: movl    12(%ebp), %eax  /* Exception 9 doesn't come here  */
		cmpw    $8, %ax         /* because it has no error code.  */
		je      nosigchk_e8_F

		cli
		pushl   $tmp_signum
		call    _issig          /* check for signals */
		addl    $4, %esp
		orl     %eax, %eax
		jz      nosigchk_e8_F

		movl    $0x1022, %edx   /* 0x1022 in user mode (see crt0.s) */
		xchgl   8(%ebp), %edx   /* %edx=%eip
					   %eax=sigfn
					   4(%ebp)=0x1022 */
		subl    $12, 20(%ebp)
		movl    tmp_signum, %ecx/* %ecx=sig */
		lds     20(%ebp), %ebx
		movl    %eax, 8(%ebx)   /* SIGNAL function */
		movl    %ecx, 4(%ebx)   /* SIGNAL number */
		movl    %edx, (%ebx)    /* %eip */

nosigchk_e8_F:  popl    %es
		popl    %ds
		popal

		popl    %ebp
		addl    $4, %esp        /* discard error code */
		iret

.align  4
except9:        pushl   $0x09           /* FPU segment overrun,
					   no error code. */
		pushl   %ebp
		movl    %esp, %ebp

		pushal
		pushl   %ds
		pushl   %es

		movw    $0x10, %ax
		movw    %ax, %ds
		movw    %ax, %es

		call    _coprocessor_segment_overrun
		jmp     end_except_0_7

.align  4
exceptA:                                /* Invalid TSS.  */
		pushl   %ebp
		movl    %esp, %ebp

		pushal
		pushl   %ds
		pushl   %es

		movw    $0x10, %ax
		movw    %ax, %ds
		movw    %ax, %es

		call    _invalid_TSS_fault
		jmp     end_except_8_F

.align  4
exceptB:                                /* Segment not present.  */
		pushl   %ebp
		movl    %esp, %ebp

		pushal
		pushl   %ds
		pushl   %es

		movw    $0x10, %ax
		movw    %ax, %ds
		movw    %ax, %es

		call    _segment_not_present_fault
		jmp     end_except_8_F

.align  4
exceptC:                                /* Stack fault.  */
		pushl   %ebp
		movl    %esp, %ebp

		pushal
		pushl   %ds
		pushl   %es

		movw    $0x10, %ax
		movw    %ax, %ds
		movw    %ax, %es

		call    _stack_fault
		jmp     end_except_8_F

.align  4
exceptD:                                /* General protection fault.  */
		pushl   %ebp
		movl    %esp, %ebp

		pushal
		pushl   %ds
		pushl   %es

		movw    $0x10, %ax
		movw    %ax, %ds
		movw    %ax, %es

		call    _general_protection_fault
		jmp     end_except_8_F

.align  4
exceptE:                                /* Page fault.  */
		pushl   %ebp
		movl    %esp, %ebp

		pushal
		pushl   %ds
		pushl   %es

		movl    %cr2, %eax
		pushl   %eax
		movw    $0x10, %ax
		movw    %ax, %ds
		movw    %ax, %es

		call    _page_fault
		popl    %eax
		jmp     end_except_8_F

.align  4
exceptF:
except11:
except12:
except13:
except14:
except15:
except16:
except17:
except18:
except19:
except1A:
except1B:
except1C:
except1D:
except1E:
except1F:
		iret                    /* reserved for future use */

.align  4
except10:                               /* FPU error, no error code.  */
		pushl   %ebp
		movl    %esp, %ebp

		pushal
		pushl   %ds
		pushl   %es

		movw    $0x10, %ax
		movw    %ax, %ds
		movw    %ax, %es

		call    _coprocessor_error
		jmp     end_except_0_7


.align  4
int20:                                  /* IRQ 0 -> Timer.  */
		pushl   %ebp
		movl    %esp, %ebp

		pushal
		pushl   %ds
		pushl   %es

		movw    $0x10, %ax
		movw    %ax, %ds
		movw    %ax, %es

		movb    $(1 << 0), %bl
		call    mask_i8259A_irq

		pushl   8(%ebp)
		call    _timer
		popl    %eax            /* _timer() returns void :-) */

		movb    $~(1 << 0), %bl
		call    unmask_i8259A_irq

		jmp     do_sched        /* always ! */


.align  4
int21:                                  /* IRQ 1 -> the keyboard.  */
		pushl   %ebp
		movl    %esp, %ebp

		pushal
		pushl   %ds
		pushl   %es

		movw    $0x10, %ax
		movw    %ax, %ds
		movw    %ax, %es

		movb    $(1 << 1), %bl
		call    mask_i8259A_irq

		call    _keyboard_interrupt

#if 0
		/* This used to re-enable the keyboard.  I don't think it is
		   still necessary, but I am keeping it here just in case.  */

		inb     $0x61, %al
		movb    %al, %ah
		orb     $0x80, %al
		outb    %al, $0x61
		xchgb   %al, %ah
		outb    %al, $0x61
#endif

		movb    $~(1 << 1), %bl
		call    unmask_i8259A_irq

		/*
		 * If a process with a higher priority waked up,
		 * reschedule.
		 */
		cmpl    $0, _sched_touched
		jne     do_sched

		jmp     end_IRQ

.align  4
int22:                                  /* IRQ 2 -> Cascade.  */
		pushl   $0x22
		jmp     bad_intr

.align  4
int23:          pushl   $0x23           /* IRQ 3 ->.  */
		jmp     bad_intr

.align  4
int24:          pushl   $0x24           /* IRQ 4 ->.  */
		jmp     bad_intr

.align  4
int25:          pushl   $0x25           /* IRQ 5 ->.  */
		jmp     bad_intr

.align  4
int26:                                  /* IRQ 6 -> The floppy disk.  */
		pushl   %ebp
		movl    %esp, %ebp

		pushal
		pushl   %ds
		pushl   %es

		movw    $0x10, %ax
		movw    %ax, %ds
		movw    %ax, %es

		movb    $(1 << 6), %bl
		call    mask_i8259A_irq

		call    _fdc_interrupt

		movb    $~(1 << 6), %bl
		call    unmask_i8259A_irq

		/*
		 * If a process with a higher priority waked up,
		 * reschedule.
		 */
		cmpl    $0, _sched_touched
		jne     do_sched

		jmp     end_IRQ

.align  4
int27:          pushl   %ebp
		movl    %esp, %ebp

		pushal
		pushl   %ds
		pushl   %es

		movw    $0x10, %ax
		movw    %ax, %ds
		movw    %ax, %es

		call    _lost_ticks

		jmp     end_IRQ

.align  4
int28:          pushl   $0x28
		jmp     bad_intr

.align  4
int29:          pushl   $0x29
		jmp     bad_intr

.align  4
int2A:          pushl   $0x2a
		jmp     bad_intr

.align  4
int2B:          pushl   $0x2b
		jmp     bad_intr

.align  4
int2C:          pushl   $0x2c
		jmp     bad_intr

.align  4
int2D:          pushl   $0x2d
		jmp     bad_intr

.align  4
int2E:                                  /* IRQ 14 -> The hard disk.  */
		pushl   %ebp
		movl    %esp, %ebp

		pushal
		pushl   %ds
		pushl   %es

		movw    $0x10, %ax
		movw    %ax, %ds
		movw    %ax, %es

		movb    $(1 << 6), %bl
		call    mask_i8259B_irq

		call    _hdc_interrupt

		movb    $~(1 << 6), %bl
		call    unmask_i8259B_irq

		/*
		 * If a process with a higher priority waked up,
		 * reschedule.
		 */
		cmpl    $0, _sched_touched
		jne     do_sched

		jmp     end_IRQ

.align  4
int2F:          pushl   $0x2f
		jmp     bad_intr


bad_intr:       pushl   %eax
		pushl   %ds

		movw    $0x10, %ax
		movw    %ax, %ds

		movl    12(%esp), %eax          /* copy the error code into */
		movl    %eax, bad_intr_no       /* bad_intr_no.             */

		popl    %ds
		popl    %eax

		addl    $4, %esp        /* discard the error code (the error */
					/* code passed by every interrupt    */
					/* that called bad_intr).            */
		pushl   %ebp
		movl    %esp, %ebp

		pushal
		pushl   %ds
		pushl   %es

		movw    $0x10, %ax
		movw    %ax, %ds
		movw    %ax, %es

		pushl   bad_intr_no
		call    _bad_interrupt
		addl    $4, %ebp

		jmp     end_IRQ

bad_intr_no:    .long   0


.align  4
sys_sched:                              /* The process scheduler.  */
		pushl   %ebp
		movl    %esp, %ebp

		pushal
		pushl   %ds
		pushl   %es

		movw    $0x10, %ax
		movw    %ax, %ds
		movw    %ax, %es

do_sched:       call    _scheduler

		/* Do *NOT* perform the task switch unless we are really
		   switching to another task.  */
		cmpl    $0xFFFFFFFF, %eax
		je      end_IRQ

		shl     $3, %eax
		addl    $FIRST_TSS, %eax
		movw    %ax, do_task_switch + 5

		/*
		 * Flush the instruction queue.  This should be done
		 * smarter :-)
		 */
	
		jmp     do_task_switch

do_task_switch: ljmp    $0, $0          /* switch to the next task */

		jmp     end_IRQ


.globl          ___main
___main:        ret



end_IRQ:
		/*
		 * Check if the interrupt occured while running in kernel mode.
		 * If so, we will remain in kernel mode after the interrupt
		 * handler completes its job, so there is no need to check
		 * for signals.
		 */

		movl    8(%ebp), %eax
		cmpw    $8, %ax
		je      nosigchk


		cli
		pushl   $tmp_signum
		call    _issig          /* check for signals */
		addl    $4, %esp
		orl     %eax, %eax
		jz      nosigchk

		movl    $0x1022, %edx   /* 0x1022 in user mode (see crt0.s) */
		xchgl   4(%ebp), %edx   /* %edx=%eip
					   %eax=sigfn
					   4(%ebp)=0x1022 */
		subl    $12, 16(%ebp)
		movl    tmp_signum, %ecx/* %ecx=sig */
		lds     16(%ebp), %ebx
		movl    %eax, 8(%ebx)   /* SIGNAL function */
		movl    %ecx, 4(%ebx)   /* SIGNAL number */
		movl    %edx, (%ebx)    /* %eip */

nosigchk:       popl    %es
		popl    %ds
		popal

		popl    %ebp
		iret


/*
 * i8259 A/B interrupt controllers handling.
 */

mask_i8259A_irq:
		inb     $0x21, %al      /* get the current 8259A mask */
		.word   0x00eb, 0x00eb  /* io delay */
		orb     %bl, %al        /* set up the new 8259A mask */
		outb    %al, $0x21      /* mask the irq */
		.word   0x00eb, 0x00eb  /* io delay */
		movb    $0x20, %al
		outb    %al, $0x20      /* end of interrupt for 8259A */
		ret

mask_i8259B_irq:
		inb     $0xA1, %al      /* get the current 8259B mask */
		.word   0x00eb, 0x00eb  /* io delay */
		orb     %bl, %al        /* set up the new 8259B mask */
		outb    %al, $0xA1      /* mask the irq */
		.word   0x00eb, 0x00eb  /* io delay */
		movb    $0x20, %al
		outb    %al, $0x20      /* end of interrupt for 8259A */
		outb    %al, $0xA0      /* end of interrupt for 8259B */
		ret

unmask_i8259A_irq:
		inb     $0x21, %al      /* get the current 8259A mask */
		.word   0x00eb, 0x00eb  /* io delay */
		andb    %bl, %al        /* set up the new 8259A mask */
		outb    %al, $0x21      /* unmask the irq */
		ret

unmask_i8259B_irq:
		inb     $0xA1, %al      /* get the current 8259B mask */
		.word   0x00eb, 0x00eb  /* io delay */
		andb    %bl, %al        /* set up the new 8259B mask */
		outb    %al, $0xA1      /* unmask the irq */
		ret

.data

.align 4
_idt:

/* Intel reserved interrupts (exceptions):  */


	.word   except0,  8, 0xEE00, 0 /* divide error */
	.word   except1,  8, 0xEE00, 0 /* single step */
	.word   except2,  8, 0xEE00, 0 /* non maskable interrupt */
	.word   except3,  8, 0xEE00, 0 /* breakpoint */
	.word   except4,  8, 0xEE00, 0 /* overflow */
	.word   except5,  8, 0xEE00, 0 /* array bounds check */
	.word   except6,  8, 0xEE00, 0 /* invalid operation code */
	.word   except7,  8, 0xEE00, 0 /* coprocessor not available */
	.word   except8,  8, 0xEE00, 0 /* double fault */
	.word   except9,  8, 0xEE00, 0 /* coprocessor segment overrun */
	.word   exceptA,  8, 0xEE00, 0 /* invalid TSS */
	.word   exceptB,  8, 0xEE00, 0 /* segment not present */
	.word   exceptC,  8, 0xEE00, 0 /* stack fault */
	.word   exceptD,  8, 0xEE00, 0 /* general protection fault */
	.word   exceptE,  8, 0xEE00, 0 /* page fault */
	.word   exceptF,  8, 0xEE00, 0 /* reserved for future use */
	.word   except10, 8, 0xEE00, 0 /* coprocessor error */

/* We should also check for i486 here...
  (checked, nothing important right now :-)  */

	.word   except11,  8, 0xEE00, 0 /* reserved for future use */
	.word   except12,  8, 0xEE00, 0 /* reserved for future use */
	.word   except13,  8, 0xEE00, 0 /* reserved for future use */
	.word   except14,  8, 0xEE00, 0 /* reserved for future use */
	.word   except15,  8, 0xEE00, 0 /* reserved for future use */
	.word   except16,  8, 0xEE00, 0 /* reserved for future use */
	.word   except17,  8, 0xEE00, 0 /* reserved for future use */
	.word   except18,  8, 0xEE00, 0 /* reserved for future use */
	.word   except19,  8, 0xEE00, 0 /* reserved for future use */
	.word   except1A,  8, 0xEE00, 0 /* reserved for future use */
	.word   except1B,  8, 0xEE00, 0 /* reserved for future use */
	.word   except1C,  8, 0xEE00, 0 /* reserved for future use */
	.word   except1D,  8, 0xEE00, 0 /* reserved for future use */
	.word   except1E,  8, 0xEE00, 0 /* reserved for future use */
	.word   except1F,  8, 0xEE00, 0 /* reserved for future use */


/* Hardware interrupts:  */

	.word   int20, 8, 0xEE00, 0     /* IRQ  0       Timer */
	.word   int21, 8, 0xEE00, 0     /* IRQ  1       Keyboard */
	.word   int22, 8, 0xEE00, 0     /* IRQ  2       Cascade */
	.word   int23, 8, 0xEE00, 0     /* IRQ  3 */
	.word   int24, 8, 0xEE00, 0     /* IRQ  4 */
	.word   int25, 8, 0xEE00, 0     /* IRQ  5 */
	.word   int26, 8, 0xEE00, 0     /* IRQ  6       Floppy disk */
	.word   int27, 8, 0xEE00, 0     /* IRQ  7 */
	.word   int28, 8, 0xEE00, 0     /* IRQ  8 */
	.word   int29, 8, 0xEE00, 0     /* IRQ  9 */
	.word   int2A, 8, 0xEE00, 0     /* IRQ 10 */
	.word   int2B, 8, 0xEE00, 0     /* IRQ 11 */
	.word   int2C, 8, 0xEE00, 0     /* IRQ 12 */
	.word   int2D, 8, 0xEE00, 0     /* IRQ 13 */
	.word   int2E, 8, 0xEE00, 0     /* IRQ 14       Hard disk */
	.word   int2F, 8, 0xEE00, 0     /* IRQ 15 */


	.word   sys_call , 8, 0xEE00, 0 /* system calls entry point */
	.word   sys_sched, 8, 0xEE00, 0 /* scheduler    entry point */

.globl _idt
