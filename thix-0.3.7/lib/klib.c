/* klib.c -- Kernel library routines.  */

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
#include <thix/stdarg.h>
#include <thix/vtty.h>
#include <thix/string.h>
#include <thix/sched.h>
#include <thix/sleep.h>
#include <thix/kpid.h>
#include <thix/klib.h>
#include <thix/klog.h>
#include <thix/ctype.h>
#include <thix/buffer.h>
#include <thix/gendrv.h>
#include <thix/proc/i386.h>


char hex_digits[17] = "0123456789abcdef";


char *
utoa(unsigned int n, char *dest, int radix)
{
    int i, digits;

    switch (radix)
    {
	case  8: digits = 11; break;
	case 10: digits = 10; break;
	case 16: digits =  8; break;
	default: digits = 32; break;
    }

    dest[digits] = '\0';

    for (i = digits-1; i >= 0; i--)
    {
	dest[i] = hex_digits[n % radix];
	n /= radix;
    }

    return dest;
}


/*
unsigned atoi(const char *str)
{
    unsigned i, rez = 0;

    for (i = 0; i < strlen(str) && isdigit(str[i]); i++)
	rez = rez * 10 + str[i] - '0';

    return rez;
}
*/

char *
skip_zeros(char *s)
{
    while (*s++ == '0');
    return --s;
}


void
putch(char c)
{
    chr_request cr = { &c, 0, 1 };

    int saved_ctty = this->ctty;
    this->ctty = makedev(VTTY_MAJOR, 0);
    vtty_write(0, &cr);
    this->ctty = saved_ctty;
    klog_write(0, &cr);
}


void
puts(char *str)
{
    chr_request cr = { str, 0, strlen(str) };

    int saved_ctty = this->ctty;
    this->ctty = makedev(VTTY_MAJOR, 0);
    vtty_write(0, &cr);
    this->ctty = saved_ctty;
    klog_write(0, &cr);
}


void
printk(const char *format, ...)
{
    int n;
    char s[33];
    va_list argptr;
    const char *ptr = format;


    va_start(argptr, format);

    while(*ptr)
    {
	switch(*ptr++)
	{
	    case '%' : switch(*ptr++)
		       {
			   case '%' : putch('%');
				      break;
			   case 'c' : putch(va_arg(argptr, char));
				      break;
			   case 's' : puts(va_arg(argptr, char *));
				      break;
			   case 'D' : puts(utoa(va_arg(argptr, int), s, 10));
				      break;
			   case 'x' : puts(utoa(va_arg(argptr, int), s, 16));
				      break;
			   case 'o' : puts(utoa(va_arg(argptr, int), s, 8)+5);
				      break;
			   case 'O' : puts(utoa(va_arg(argptr, int), s, 8));
				      break;
			   case 'b' : puts(utoa(va_arg(argptr, char), s,2)+24);
				      break;
			   case 'w' : puts(utoa(va_arg(argptr, short),s,16)+4);
				      break;
			   case 'B' : puts(utoa(va_arg(argptr, int), s, 2));
				      break;
			   case 'z' : puts(utoa(va_arg(argptr, int), s, 10)+8);
				      break;
			   case 'd' : n = va_arg(argptr, int);

				      if (n == 0)
					  puts("0");
				      else
				      {
					  utoa(n, s, 10);
					  puts(skip_zeros(s));
				      }
				      break;
		       }
		       break;

	    case '\\': switch(*ptr++)
		       {
			   case 'n' : putch('\n'); break;
			   case 'r' : putch('\r'); break;
			   case 't' : putch('\t'); break;
			   case '\\': putch('\\'); break;
		       }
		       break;

	    default  : putch(*(ptr - 1));
       }
    }

    va_end(argptr);
}


char *
strcpy(char *dest, const char *src)
{
    while ((*dest++ = *src++));
    return dest;
}


char *
strchr(char *s, int c)
{
    for (; *s; s++)
	if (*s == (char)c)
	    return s;

    if (c == 0)
	return s;

    return NULL;
}


/* This functions makes a delay of the given number of miliseconds.
   Since the timer resolution is 100 Hz, the actual  resolution  is
   at the hundreds of seconds level.  It also activates interrupts,
   so be careful.  */

void
delay(int msec)
{
    int end_ticks = ticks + msec / 10;

    while (ticks < end_ticks)
	reschedule();
}


unsigned
checksum(unsigned char *data, int count)
{
    int i;
    unsigned chksum = 0;

    for (i = 0; i < count; i++)
	chksum += *data++;

    return chksum;
}


void
display_sys_info(void)
{
#ifdef __DEBUG__
    void vtty_debug_info();
    void process_debug_info();
    void scheduler_debug_info(void);

    process_debug_info();
#endif
}


void
panic_end(void)
{
    void vtty_setfocus0(void);

    printk("process %x(%d) did it\n", current_kpid, pid(current_kpid));
    disable();
    vtty_setfocus0();
    for(;;);
}


/*
 * We need this because reschedule is inline and we can't call it from
 * startup.s. It's ugly, I know ...
 */

void
_reschedule(void)
{
    reschedule();
}


void
display_number(unsigned x)
{
/*    printk("{%d:%d}", pid(current_kpid), x); */
}


/*
 * An invalid system call has been detected.
 */

void
bad_system_call(int sys_call_no)
{
    DEBUG(2, "(%d)\n", sys_call_no);

    kill_kpid(current_kpid, SIGSYS);
}


/*
 * An invalid interrupt has been detected.  Kill the offending process.
 */

void
bad_interrupt(int int_no)
{
    printk("\nkernel: Bad interrupt or exception %x!\n", int_no);
    printk("Exception occured while running kpid %x.\n", current_kpid);

    kill_kpid(current_kpid, SIGSEGV);
}
