/* sleep.c -- Sleep & wakeup routines.  */

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
#include <thix/klib.h>
#include <thix/sleep.h>
#include <thix/sched.h>
#include <thix/string.h>
#include <thix/process.h>
#include <thix/limits.h>
#include <thix/proc/i386.h>


#define ERR_BADWAKEUP   error[0]

static char *error[] =
{
    "can't wakeup process %d supossed to sleep on address %x",
};


#define SLEEP_BUCKETS 67

typedef struct
{
    void  *address;
    short next;
    short dummy;
} SLEEP;


short sleep_hash[SLEEP_BUCKETS + 1];
SLEEP sleep_vect[SYSTEM_OPEN_MAX];


/*
 * Sleeping processes keep the address they  are  sleeping  on  in  the
 * u_area's  'sleeping_address'  field.  This  way  wakeup_process  can
 * detect it when it wants to wake up a process.  If a process is waked
 * up using wakup_process, the 'sleeping_address' field is set to NULL.
 * sleep() use this to detect the wake up reason.
 *
 * sleep() returns the number of the signal that waked the  process  up
 * (when the process is sleeping at an interruptible priority level) or
 * 0 if the process has been waked up by a SIGCHLD signal and, before
 * going to sleep, it was running wait().
 *
 * sleep() will return a nonzero value when a signal has been delivered
 * to a process sleeping on an  interruptible priority level.  If  that
 * process was running a read/write system call, that system call  will
 * return -1 and set errno to EINTR.
 *
 * sleep() allways return 0 when the expected event occured (and not as
 * a result of a signal delivery). The only exception is when a SIGCHLD
 * signal is delivered while the current  process was in waitpid(), but
 * this signal was  the event that the process was waiting for.  Sounds
 * resonable to me.
 */

void
sleep_init(void)
{
    lmemset(sleep_hash, 0, (SLEEP_BUCKETS + 1) >> 1);
}


/*
 * A process calling sleep() will go to sleep on the  address  'address'
 * using the wakeup priority level specified in  'wakeup_priority' (this
 * is the priority that will be used when the process will be waked up).
 * This priority will be used just  one  time, in  order  to  give  that
 * process a chance to  eventually  make  a  new  request  to  a  device
 * driver and go back to sleep.  This is  usually  the  case  with  disk
 * drivers: sleeping on a high priority  level  (an uninterruptible one)
 * means that when the interrupt occur, the process will  be  waked  up,
 * put in one of the highest priority list and run immediately.  It will
 * then make a new request to the device driver, without waiting at all.
 * Doing otherwise will lead to poor performances.
 */

int
sleep(void *address, int wakeup_priority)
{
    int kpid, signum = 0, index = (unsigned)address % SLEEP_BUCKETS;

    disable();

    for (;;)
    {
	if (wakeup_priority < U_THRESHOLD_PRIORITY && signals())
	{
	    if ((signum = __issig()) == SIGCHLD)
	    {
		switch ((unsigned)this->sigactions[SIGCHLD].sa_handler)
		{
		    case SIG_IGN: while ((kpid = pick_zombie()))
				  {
				      DEBUG(2, "releasing zombie ...\n");
				      release_zombie(kpid);
				  }

		    case SIG_DFL: if (address == &sys_waitpid)
				      return 0;
				  else
				      break;
		    default:      return (address==&sys_waitpid) ? 0 : SIGCHLD;
		}
	    }
	    else
		return signum;
	}

	sleep_vect[current_kpid].next = sleep_hash[index];
	sleep_vect[current_kpid].address = address;
	sleep_hash[index] = current_kpid;
	this->sleep_address = address;
	this->status = PROCESS_ASLEEP;
	this->wakeup_priority = wakeup_priority;
	this->old_cpu_usage = delete_schedulable_process(current_kpid);

	reschedule();

	delete_schedulable_process(current_kpid);
	restore_schedulable_process(current_kpid, this->old_cpu_usage);

	if (this->sleep_address)
	    return 0;
    }
}


/*
 * This function is used to wake up all the processes sleeping on a given
 * address.  However, only one process will be able to actually enter the
 * critical region.  The other ones will find the critical region  locked
 * and will go back to sleep.  This can lead to undesirable results, like
 * keeping a process from entering the critical region a long  period  of
 * time.  In practice however, this never happens.  Maybe this should  be
 * replaced with a FIFO policy one day...
 */

void
wakeup(void *address)
{
    short *proc;
    int index = (unsigned)address % SLEEP_BUCKETS;

    disable();
    proc = &sleep_hash[index];

    while (*proc)
	if (sleep_vect[*proc].address == address)
	{
	    u_area[*proc].status = PROCESS_RUNNING;
	    new_schedulable_process(*proc, u_area[*proc].wakeup_priority);
	    *proc = sleep_vect[*proc].next;
	    sched_touched = 1;
	}
	else
	    proc = &sleep_vect[*proc].next;
}


/*
 * This function is used to wake up a specified process.  In order to do
 * this, the address on which the process sleeps is kept in the u_area's
 * 'sleep_address' field.  The hash table is parsed and when the process
 * is found, it is waked up.  This is called from  kill_kpid()  and  the
 * process is supposed to be found.  However, the  kernel  will  try  to
 * resume and only issue a warning.
 *
 * Note that the swapper *should not* be waked up using wakeup_process()
 * because  this  function  doesn't  initialize  the   'swapper_running'
 * variable used by the VM routines (get_page(), reserve_pages()) to get
 * its state.
 */

void
wakeup_process(int kpid)
{
    short *proc;
    int index = (unsigned)u_area[kpid].sleep_address % SLEEP_BUCKETS;

    disable();
    proc = &sleep_hash[index];

    while (*proc)
	if (*proc == kpid &&
	    sleep_vect[*proc].address == u_area[kpid].sleep_address)
	{
	    u_area[*proc].status = PROCESS_RUNNING;
	    u_area[*proc].sleep_address = NULL;
	    new_schedulable_process(*proc, u_area[*proc].wakeup_priority);
	    *proc = sleep_vect[*proc].next;
	    sched_touched = 1;
	    return;
	}
	else
	    proc = &sleep_vect[*proc].next;

    PANIC(ERR_BADWAKEUP, pid(kpid), u_area[kpid].sleep_address);
}


/*
 * This function wakes up only one process of those sleeping on  address
 * 'address'.  It is used in the semaphore  implementation.  It  returns
 * the number of waked up processes (0 or 1).  This is used to detect if
 * we should increment the semaphore's value or not.  Note that  we  are
 * dealing with the same FIFO problem as in wakeup().
 */

int
wakeup_one_process(void *address)
{
    short *proc;
    int index = (unsigned)address % SLEEP_BUCKETS;

    disable();
    proc = &sleep_hash[index];

    while (*proc)
	if (sleep_vect[*proc].address == address)
	{
	    u_area[*proc].status = PROCESS_RUNNING;
	    new_schedulable_process(*proc, u_area[*proc].wakeup_priority);
	    *proc = sleep_vect[*proc].next;
	    sched_touched = 1;
	    return 1;
	}
	else
	    proc = &sleep_vect[*proc].next;

    return 0;
}


/*
 * This function wakes up all the processes waiting on an address and
 * sends them the specified signal.  Used only when closing pipes.
 */

void
wakeup_and_kill(void *address, int signum)
{
    short *proc;
    int index = (unsigned)address % SLEEP_BUCKETS;

    disable();
    proc = &sleep_hash[index];

    while (*proc)
	if (sleep_vect[*proc].address == address)
	{
	    u_area[*proc].status = PROCESS_RUNNING;
	    new_schedulable_process(*proc, u_area[*proc].wakeup_priority);
	    *proc = sleep_vect[*proc].next;
	    sched_touched = 1;
	    kill_kpid(*proc, signum);
	}
	else
	    proc = &sleep_vect[*proc].next;
}
