/* semaphore.c -- Semaphore routines.  */

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
#include <thix/sleep.h>
#include <thix/semaphore.h>
#include <thix/proc/i386.h>


/*
 * Semaphores:
 *
 *    1. The initial value of a semaphore represents the maximum number of
 *       processes that are allowed to use the resource at the  same time.
 *
 *    2. A positive semaphore's value indicates the number  of  additional
 *       processes that are allowed to enter the critical region, in order
 *       to use the resource.  It also indicates that there  is  currently
 *       no process waiting to enter the critical region.  The  difference
 *       between the initial and the current  semaphore  value  gives  the
 *       number of processes in the critical region.
 *
 *    3. If the semaphore's value  is 0, it  indicates  that  there  is  a
 *       number of processes sleeping on the semphore's address waiting to
 *       enter the critical region.  The initial  semaphore's value  gives
 *       the number of  processes  currently  running  into  the  critical
 *       region.
 *
 *    Note:
 *
 *       There is no way to detect the number  of  processes  waiting  to
 *       enter into the critical  region  using  this  implementation  of
 *       semaphores    as   positive    values.   A   signed   semaphores
 *       implementation would do the job.
 */

/*
 * down(): if the semaphore's value is positive (there is still  possible
 * for a number of processes to use the resource), it is decremented.  If
 * its value is 0 (new processes trying to enter the critical region  are
 * not allowed to do it), the process executing the down() operation goes
 * to sleep on the semaphore's address.
 * Basically, performing a down() operation means we want to  enter  into
 * the critical region and, depending on the number of processes  allowed
 * to use the resource at the same time and the number of processes  that
 * are actually using it, we are allowed or not to do it.
 */

void
down(semaphore *sem, int wakeup_priority)
{
    disable();

    if (*sem)
	(*sem)--;
    else
	sleep(sem, wakeup_priority);
}


/*
 * up(): if the semaphore's value is positive, there is no process waiting
 * to enter the critical region.  All the processes  that  have  requested
 * access to the critical region are already  there.  If  the  semaphore's
 * value is 0, we are trying to wakeup a sleeping process.  If there is no
 * process sleeping on the semaphore's address, the semaphore's  value  is
 * incremented.  Otherwise, the semaphore's value remains unchanged.
 * Basically, an up() operation on a semaphore means that one more process
 * can enter the critical region, therefore, if  there  is  at  least  one
 * process waiting to enter into the critical region, it is waked up.
 */

void
up(semaphore *sem)
{
    disable();

    if (*sem || !wakeup_one_process(sem))
	(*sem)++;
}
