/* faults.c -- Faults and exceptions handling routines.  */

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


#include <thix/klib.h>
#include <thix/vm.h>
#include <thix/kpid.h>
#include <thix/proc/i386.h>


/*
 * Exception 0: i386 divide error.
 */

void
divide_error(void)
{
    DEBUG(2, "\n");

    kill_kpid(current_kpid, SIGFPE);
}


/*
 * Exception 1: i386 single step.
 */

void
single_step(void)
{
    DEBUG(2, "\n");

    kill_kpid(current_kpid, SIGFPE);
}


/*
 * Exception 2: i386 non maskable interrupt.
 */

void
non_maskable_interrupt(void)
{
    DEBUG(2, "\n");

    kill_kpid(current_kpid, SIGFPE);
}


/*
 * Exception 3: i386 breakpoint.
 */

void
breakpoint(void)
{
    DEBUG(2, "\n");

    kill_kpid(current_kpid, SIGFPE);
}


/*
 * Exception 4: i386 overflow.
 */

void
overflow(void)
{
    DEBUG(2, "\n");

    kill_kpid(current_kpid, SIGFPE);
}


/*
 * Exception 5: i386 array bounds check.
 */

void
array_bounds_check(void)
{
    DEBUG(2, "\n");

    kill_kpid(current_kpid, SIGFPE);
}


/*
 * Exception 6: i386 invalid operation code.
 */

void
invalid_operation_code(void)
{
    DEBUG(2, "\n");

    kill_kpid(current_kpid, SIGILL);
}


/*
 * Exception 7: i386 coprocessor not available.
 */

void
coprocessor_not_available(void)
{
    DEBUG(2, "\n");

    kill_kpid(current_kpid, SIGILL);
}


/*
 * Exception 8: i386 double fault.
 */

void
double_fault(unsigned   es,
	     unsigned   ds, unsigned    edi, unsigned esi, unsigned _ebp,
	     unsigned _esp, unsigned    ebx, unsigned edx, unsigned ecx,
	     unsigned  eax, unsigned    ebp, unsigned err, unsigned eip,
	     unsigned   cs, unsigned eflags, unsigned esp, unsigned ss)
{
    display_exception_header("double fault");
    display_registers(cs, ds, es, ss,
		      eax, ebx, ecx, edx, esi, edi, ebp, esp, _esp,
		      eip, eflags);
    printk("ecode=%w", err);

    /* Double fault is bad enough.  Stop.  */
    PANIC("");
}


/*
 * Exception 9: i386 coprocessor segment overrun.
 */

void
coprocessor_segment_overrun(void)
{
    DEBUG(2, "\n");

    kill_kpid(current_kpid, SIGFPE);
}


/*
 * Exception A: i386 invalid TSS fault.
 */

void
invalid_TSS_fault(unsigned   es,
		  unsigned   ds, unsigned    edi, unsigned esi, unsigned _ebp,
		  unsigned _esp, unsigned    ebx, unsigned edx, unsigned ecx,
		  unsigned  eax, unsigned    ebp, unsigned err, unsigned eip,
		  unsigned   cs, unsigned eflags, unsigned esp, unsigned ss)
{
    display_exception_header("invalid tss fault");
    display_registers(cs, ds, es, ss,
		      eax, ebx, ecx, edx, esi, edi, ebp, esp, _esp,
		      eip, eflags);
    printk("ecode=%w", err);

    /* Invalid TSS means something really bad happened into the kernel.
       So all we can do is ... stop.  */
    PANIC("");
}


/*
 * Exception B: i386 segment not present fault.
 */

void
segment_not_present_fault(unsigned es,
		unsigned   ds, unsigned    edi, unsigned  esi, unsigned _ebp,
		unsigned _esp, unsigned    ebx, unsigned  edx, unsigned ecx,
		unsigned  eax, unsigned    ebp, unsigned  err, unsigned eip,
		unsigned   cs, unsigned eflags, unsigned  esp, unsigned ss)
{
    display_exception_header("segment not present fault");
    display_registers(cs, ds, es, ss,
		      eax, ebx, ecx, edx, esi, edi, ebp, esp, _esp,
		      eip, eflags);
    printk("ecode=%w", err);

    kill_kpid(current_kpid, SIGSEGV);
}


/*
 * Exception C: i386 stack fault.
 */

void
stack_fault(unsigned   es,
	    unsigned   ds, unsigned    edi, unsigned esi, unsigned _ebp,
	    unsigned _esp, unsigned    ebx, unsigned edx, unsigned ecx,
	    unsigned  eax, unsigned    ebp, unsigned err, unsigned eip,
	    unsigned   cs, unsigned eflags, unsigned esp, unsigned ss)
{
    display_exception_header("stack fault");
    display_registers(cs, ds, es, ss,
		      eax, ebx, ecx, edx, esi, edi, ebp, esp, _esp,
		      eip, eflags);
    printk("ecode=%w", err);

    /* We are going to send a SIGKILL signal here in order  to  avoid
       a new stack fault when trying to  execute a  singnal  handler.
       I simply hope that this is a user-level stack fault, otherwise
       we will loop forever, or just hang.
       BSD is smarter :-), it has alternate stacks for this.  */

    kill_kpid(current_kpid, SIGKILL);
}


/*
 * Exception D: i386 general protection fault.
 */

void
general_protection_fault(unsigned  es,
		unsigned   ds, unsigned    edi, unsigned esi, unsigned _ebp,
		unsigned _esp, unsigned    ebx, unsigned edx, unsigned ecx,
		unsigned  eax, unsigned    ebp, unsigned err, unsigned eip,
		unsigned   cs, unsigned eflags, unsigned esp, unsigned ss)
{
    display_exception_header("general protection fault");
    display_registers(cs, ds, es, ss,
		      eax, ebx, ecx, edx, esi, edi, ebp, esp, _esp,
		      eip, eflags);
    printk("ecode=%w", err);

    if ((unsigned short)cs == 8)
	PANIC("");

    /* Try to recover by killing the offending process.  Hope it works.  */
    kill_kpid(current_kpid, SIGSEGV);
}


/*
 * Exception 10: i386 coprocessor error.
 */

void
coprocessor_error(void)
{
    DEBUG(2, "\n");

    kill_kpid(current_kpid, SIGFPE);
}


/*
 * Display registers.  Does *not* writes a '\n' at the end, mostly
 * because in usual cases a PANIC() folows and does the job.  Also
 * new information can be added without wasting an  useful  screen
 * line :-).
 */

void
display_registers(unsigned cs,   unsigned ds,  unsigned es,
		  unsigned ss,   unsigned eax, unsigned ebx,
		  unsigned ecx,  unsigned edx, unsigned esi,
		  unsigned edi,  unsigned ebp, unsigned esp,
		  unsigned _esp, unsigned eip, unsigned eflags)
{
    int i, j;
    unsigned *sp = (unsigned *)((cs==8) ? _esp : esp + KERNEL_RESERVED_SPACE);

    printk("cs=%w\t\tds=%w\t\tes=%w\t\tss=%w\t\teip=%x\n",
	   cs, ds, es, ((cs == 8) ? get_ss() : ss), eip);

    printk("eax=%x\tebx=%x\tecx=%x\tedx=%x\n", eax, ebx, ecx, edx);

    printk("esi=%x\tedi=%x\tebp=%x\tesp=%x\n",
	   esi, edi, ebp, ((cs == 8) ? _esp : esp));

    printk("stack:   ");

    sp += 3 * 9 - 1;

    for (i = 0; i < 3; i++)
	for (j = 0; j < 9; j++)
	{
	    if (i == 0 && j == 0)
		continue;

	    printk ("%x%s", *--sp, ((j == 8) ? "" : " "));
	}


    printk("eflags=%B\t", eflags);

    /* A short delay (2 secs), just to have a chance to see those values.  */
    delay(2000);
}


/*
 * Display exception header.
 */

void
display_exception_header(char *exception_name)
{
    printk("\n[%s] (pid=%d, kpid=%d): <%s>\n",
	   this->args, pid(current_kpid), current_kpid, exception_name);
}
