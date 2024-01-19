/* sched.c -- Time management & process scheduler routines.  */

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
#include <thix/sched.h>
#include <thix/gendrv.h>
#include <thix/string.h>
#include <thix/kpid.h>
#include <thix/process.h>
#include <thix/times.h>
#include <thix/sleep.h>
#include <thix/limits.h>
#include <thix/gendrv.h>
#include <thix/arch/pc/cmos.h>
#include <thix/proc/i386.h>


extern tss_t *this;
extern descriptor_t tss_descr[];
extern unsigned free_list_clear_time;


unsigned boot_time;
unsigned volatile seconds;
unsigned volatile ticks = 0;
int sched_touched       = 1;
int current_kpid        = 0;
int timer_functions     = 0;


alarm_struct alarms[SYSTEM_PROCESS_MAX];
alarm_struct *alarms_head = NULL;


void (*timer_map[MAX_DRIVERS])(void);


unsigned char sched_head[MAX_PRIORITY_LEVEL + 1];
unsigned char sched_tail[MAX_PRIORITY_LEVEL + 1];
sched_struct  sched_data[SYSTEM_OPEN_MAX];


char days_per_month[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };


inline static int
leap(int year)
{
    return ((year % 4 == 0) && (year % 100 != 0 || year % 400 == 0));
}


/*
 * This function updates the 'seconds' variable with the difference
 * (in seconds) between 00:00:00 January, 1, 1970 and the current CMOS time.
 */

void
get_CMOS_time(void)
{
    int m;
    int second  = bcd2i(read_cmos(0x00));
    int minute  = bcd2i(read_cmos(0x02));
    int hour    = bcd2i(read_cmos(0x04));

    int day     = bcd2i(read_cmos(0x07));
    int month   = bcd2i(read_cmos(0x08));
    int year    = bcd2i(read_cmos(0x32)) * 100 + bcd2i(read_cmos(0x09));

    int leap_years = ((year - 1972) >> 2) + ((year % 4) ? 1 : 0);

    if (year > 2000)
	leap_years--;

    seconds = ((year - 1970) * 365 + leap_years) * 24 * 60 * 60;

    if (leap(year))
	days_per_month[1]++;

    for (m = 0; m < month - 1; m++)
	seconds += days_per_month[m]*24*60*60;

    if (leap(year))
	days_per_month[1]--;

    seconds += (day - 1)*24*60*60 + hour*60*60 + minute*60 + second;

    DEBUG(1, "Current date %z-%z-%z\n", day,  month,  year);
    DEBUG(1, "Current time %z:%z:%z\n", hour, minute, second);
}


/*
 * Set the system / computer time.
 * It seems to me that there is no need to write into the CMOS the day of week
 * number (address 0x06) because Thix doesn't use it  and  the  BIOS  computes
 * it at boot time (at least my BIOS does). Anyway, this should be tested with
 * other computers too.
 */

int
sys_stime(__time_t *tptr)
{
    int year, month, day, hour, minute, second, sec, seconds_left;

    if (!ok_to_read_area(tptr, sizeof(__time_t)))
	return -EFAULT;

    if (!superuser())
	return -EPERM;

    seconds_left = seconds = *tptr;

    for (year = 1970; ; seconds_left -= sec, year++)
	if (seconds_left < (sec = (leap(year) ? 366 : 365) * 24 * 60 * 60))
	    break;

    if (leap(year))
	days_per_month[1]++;

    for (month = 1; seconds_left >= (sec = days_per_month[month-1]*24*60*60);
		    seconds_left -= sec, month++);

    if (leap(year))
	days_per_month[1]--;

    for (day = 1;
	 seconds_left >= 24 * 60 * 60;
	 seconds_left -= 24 * 60 * 60, day++);

    for (hour = 0;
	 seconds_left >= 60 * 60;
	 seconds_left -= 60 * 60, hour++);

    for (minute = 0;
	 seconds_left >= 60;
	 seconds_left -= 60, minute++);

    second = seconds_left;

    DEBUG(4, "\nkernel date: %z-%z-%z\n", day, month, year);
    DEBUG(4, "kernel time: %z:%z:%z\n\n", hour, minute, second);

    write_cmos(0x00, i2bcd(second));
    write_cmos(0x02, i2bcd(minute));
    write_cmos(0x04, i2bcd(hour));

    write_cmos(0x07, i2bcd(day));
    write_cmos(0x08, i2bcd(month));
    write_cmos(0x09, i2bcd(year % 100));
    write_cmos(0x32, i2bcd((year - (year % 100)) / 100));

    return 0;
}


/*
 * Return the time since 00:00:00 GMT, January 1, 1970, measured in
 * seconds.
 */

int
sys_time(__time_t *tptr)
{
    if (tptr == (long *)USER_NULL)
	return seconds;

    DEBUG(4, "(%x)\n", tptr);

    if (!ok_to_write_area(tptr, sizeof(__time_t)))
	return -EFAULT;

    SYSCALL_PROLOG();

    *tptr = seconds;

    SYSCALL_EPILOG();

    return seconds;
}


/*
 * Return in a tms structure the current process user and system times.
 * This function is obsolete.  sys_getrusage() should be used instead.
 */

int
sys_times(struct tms *tmsptr)
{
    DEBUG(0, "\x7Obsolete system call. Use sys_getrusage() instead.\n");

    if (!ok_to_write_area(tmsptr, sizeof(struct tms)))
	return -EFAULT;

    SYSCALL_PROLOG();

    tmsptr->tms_utime  = this->usage.ru_utime.tv_sec;
    tmsptr->tms_stime  = this->usage.ru_stime.tv_sec;
    tmsptr->tms_cutime = this->cusage.ru_utime.tv_sec;
    tmsptr->tms_cstime = this->cusage.ru_stime.tv_sec;

    SYSCALL_EPILOG();

    return ticks;
}


/*
 * Check an itimer for consistency.
 */

inline int
itimer_ok(struct itimerval *itimer)
{
    return (itimer->it_value.tv_sec     >= 0 &&
	    itimer->it_value.tv_usec    >= 0 &&
	    itimer->it_interval.tv_sec  >= 0 &&
	    itimer->it_interval.tv_usec >= 0 &&
	   (itimer->it_value.tv_sec     != 0 ||
	    itimer->it_value.tv_usec    != 0));
}


/*
 * Convert a timeval structure to an integer.
 */

unsigned
timeval2int(struct timeval *tv)
{
    return tv->tv_sec * HZ + tv->tv_usec / (1000000 / HZ);
}


/*
 * Convert an integer to a timeval structure.
 */

void
int2timeval(unsigned value, struct timeval *tv)
{
    tv->tv_sec  = (value / HZ);
    tv->tv_usec = (value % HZ) * (1000000 / HZ);
}


/*
 * Convert an itimerval structure to a _itimerval structure.  The
 * _itimerval structure is internally used by the kernel to speed
 * up things in the scheduler.
 */

void
itimerval2_itimerval(struct itimerval *itimer, _itimerval *_itimer)
{
    _itimer->it_value    = timeval2int(&itimer->it_value);
    _itimer->it_interval = timeval2int(&itimer->it_interval);
}


/*
 * Converts a _itimerval structure into an itimerval structure.
 */

void
_itimerval2itimerval(_itimerval *_itimer, struct itimerval *itimer)
{
    int2timeval(_itimer->it_value,    &itimer->it_value);
    int2timeval(_itimer->it_interval, &itimer->it_interval);
}


/*
 * Set an alarm for the current process.
 */

void
set_alarm(int kpid)
{
    unsigned alarm_time = ticks + u_area[kpid].real.it_value;
    alarm_struct *ap, *prev = alarms_head, *current = &alarms[kpid];


/*
    DEBUG(0, "setting alarm for kpid=%d to %d\n", kpid, alarm_time);
*/
    reset_alarm(kpid);

    current->alarm_time = alarm_time;

    if (alarms_head == NULL)
    {
	alarms_head = current;
	current->next = NULL;
	return;
    }

    if (alarm_time < alarms_head->alarm_time)
    {
	alarms_head = current;
	current->next = alarms_head->next;
	return;
    }

    for (ap = alarms_head->next;
	 ap && ap->alarm_time <= alarm_time;
	 ap = ap->next)
	prev = ap;

    current->next = ap;
    prev->next = current;
}


/*
 * Reset the current alarm of the kernel process kpid.
 */

void
reset_alarm(int kpid)
{
    alarm_struct *ap;
    alarm_struct *current = &alarms[kpid];


/*
    DEBUG(0, "resetting alarm for kpid=%d\n", kpid);
*/
    if (alarms_head == NULL)
	return;

    current->alarm_time = 0;

    if (alarms_head == current)
    {
	alarms_head = current->next;
	return;
    }

    for (ap = alarms_head; ap; ap = ap->next)
	if (ap->next == current)
	{
	    ap->next = current->next;
	    return;
	}
}


/*
 * Just another obsolete system call.
 */

int
sys_alarm(unsigned int sec)
{
    DEBUG(2, "\x7Obsolete system call. Use sys_setitimer() instead.\n");
    return -ENOSYS;
}


/*
 * Get the value of an interval timer.  Returns in 'old' its value.
 */

int
sys_getitimer(int which, struct itimerval *old)
{
    if (!ok_to_write_area(old, sizeof(struct itimerval)))
	return -EFAULT;

    SYSCALL_PROLOG();

    switch (which)
    {
	case ITIMER_REAL:

	    if (alarms[current_kpid].alarm_time > ticks)
		int2timeval(alarms[current_kpid].alarm_time - ticks,
			    &old->it_value);
	    else
		int2timeval(0, &old->it_value);

	    int2timeval(this->real.it_interval, &old->it_interval);
	    break;

	case ITIMER_VIRTUAL:

	    _itimerval2itimerval(&this->virtual, old);
	    break;

	case ITIMER_PROF:

	    _itimerval2itimerval(&this->prof, old);
	    break;

	default:
	    SYSCALL_EPILOG();
	    return -EINVAL;
    }

    SYSCALL_EPILOG();
    return 0;
}


/*
 * Set an interval timer.  Return its old value in 'old'.
 */

int
sys_setitimer(int which, struct itimerval *new, struct itimerval *old)
{
    DEBUG(4, "(%d,%x,%x)\n", which, old, new);

    if (!ok_to_read_area(new, sizeof(struct itimerval)))
	return -EFAULT;

    if (old != (struct itimerval *)USER_NULL)
	if (!ok_to_write_area(old, sizeof(struct itimerval)))
	    return -EFAULT;

    if (!itimer_ok(new))
	return -EINVAL;

    SYSCALL_PROLOG();

    switch (which)
    {
	case ITIMER_REAL:

	    if (old != (struct itimerval *)USER_NULL)
		_itimerval2itimerval(&this->real, old);

	    itimerval2_itimerval(new, &this->real);
	    set_alarm(current_kpid);
	    break;

	case ITIMER_VIRTUAL:

	    if (old != (struct itimerval *)USER_NULL)
		_itimerval2itimerval(&this->virtual, old);

	    itimerval2_itimerval(new, &this->virtual);
	    break;

	case ITIMER_PROF:

	    if (old != (struct itimerval *)USER_NULL)
		_itimerval2itimerval(&this->prof, old);

	    itimerval2_itimerval(new, &this->prof);
	    break;

	default:
	    SYSCALL_EPILOG();
	    return -EINVAL;
    }

    SYSCALL_EPILOG();
    return 0;
}


void
lost_ticks(void)
{
    DEBUG(2, "\nkernel process %x is losing ticks...\n", current_kpid);
}


/*
 * Initialize the timer at 100Hz.
 */

void
timer_init(void)
{
    outb_delay(0x43, 0x36);     /* timer mode = permanent. */
    outb_delay(0x40, 0x9c);     /* 1193182 / 100  (low word). */
    outb_delay(0x40, 0x2e);     /* 1193182 / 100  (high word). */
}


/*
 * Initialize the scheduler data.  Fill everything with zeros.
 */

void
scheduler_data_init(void)
{
    memset(sched_head, 0, sizeof(sched_head));
    memset(sched_tail, 0, sizeof(sched_tail));
    memset(sched_data, 0, sizeof(sched_data));
}


/*
 * Initialize the scheduler.  Resets all the alarms, set up the swapper
 * priority.  Also add it to the schedulable processes list.
 */

void
scheduler_init(void)
{
    get_CMOS_time();

    free_list_clear_time =
    u_area[0].start_time =
    u_area[1].start_time =
    u_area[2].start_time = boot_time = seconds;

    memset(alarms, 0, sizeof(alarms));

    scheduler_data_init();

    sched_data[SWAPPER].priority = WAIT_PAGE;

    new_schedulable_process(SWAPPER, WAIT_PAGE);
}


#ifdef __DEBUG__
void
scheduler_debug_info(void)
{
    int i;

    for (i = 0; i < SYSTEM_OPEN_MAX; i++)
    {
	if (sched_data[i].priority)
	    printk("[<%z,%z> %z,%z] ", pid(i), i,
		   sched_data[i].next, sched_data[i].cpu_usage);
    }

    printk("\n");

    for (i = 0; i < MAX_PRIORITY_LEVEL; i++)
    {
	if (sched_head[i])
	    printk("{%z,%z,%z} ", i, sched_head[i], sched_tail[i]);
    }
}
#endif /* __DEBUG__ */


/*
 * Add the kernel process 'kpid' to the list of schedulable processes
 * corresponding to the 'priority' priority.
 *
 * FIXME: should cpu_usage be inherited?
 */

inline void
new_schedulable_process(int kpid, int priority)
{
    sched_data[kpid].cpu_usage = priority;
    sched_data[kpid].next = 0;

    if (sched_head[priority])
	sched_data[sched_tail[priority]].next = kpid;
    else
	sched_head[priority] = kpid;

    sched_tail[priority] = kpid;
}


/*
 * Remove the kernel process 'kpid' from the scheduler list corresponding
 * to its priority.  Normally 'kpid' is in the head of the list.  Return
 * the CPU usage of the process.
 */

inline int
delete_schedulable_process(int kpid)
{
    int current_priority = sched_data[kpid].cpu_usage;

    if ((sched_head[current_priority] = sched_data[kpid].next) == 0)
	sched_tail[current_priority] = 0;

    return current_priority;
}


/*
 * Put a process back in its priority list.  Used only in sleep() to
 * put it back to the 'cpu_usage' priority list.
 */

inline void
restore_schedulable_process(int kpid, int priority)
{
    sched_data[kpid].cpu_usage = priority;
    sched_data[kpid].next = sched_head[priority];
    sched_head[priority] = kpid;

    if (sched_tail[priority] == 0)
	sched_tail[priority] = kpid;
}


/*
 * Register a new timer.  Used by device drivers, mostly for timeouts.
 */

int
register_timer(void (*timer_fn)(void))
{
    if (timer_fn == NULL)
	return 1;

    if (timer_functions < MAX_DRIVERS)
    {
	timer_map[timer_functions++] = timer_fn;
	return 1;
    }

    return 0;
}


/*
 * This is the clock interrupt handler.  Called 100 times per second.
 * Computes statistics, keeps track of time, dispatches system timers
 * and sends alarm signals as required.
 */

void
timer(unsigned short CS)
{
    int fn;
    _itimerval *prof    = &this->prof;
    _itimerval *virtual = &this->virtual;


    if (++ticks % HZ == 0)
	seconds++;

    /* I hate spending time with statistics here ... Maybe I should compute
       these only when requested (using ticks), but I don't feel like doing
       it now.  */

    if (CS == 8)
    {
	/* Kernel level statistics.  */
	this->usage.ru_stime.tv_usec += 1000000 / HZ;
	if (this->usage.ru_stime.tv_usec >= 1000000)
	{
	    this->usage.ru_stime.tv_sec++;
	    this->usage.ru_stime.tv_usec -= 1000000;
	}
    }
    else
    {
	/* User level statistics.  */
	this->usage.ru_utime.tv_usec += 1000000 / HZ;
	if (this->usage.ru_utime.tv_usec >= 1000000)
	{
	    this->usage.ru_utime.tv_sec++;
	    this->usage.ru_utime.tv_usec -= 1000000;
	}

	/* Virtual itimers.  */
	if (virtual->it_value && --virtual->it_value == 0)
	{
	    this->sigpending |= 1 << SIGVTALRM;

	    /* Statistics.  */
	    this->usage.ru_nsignals++;

	    if (virtual->it_interval)
		virtual->it_value = virtual->it_interval;
	}
    }

    /* Profile itimers.  */
    if (prof->it_value && --prof->it_value == 0)
    {
	this->sigpending |= 1 << SIGPROF;

	/* Statistics. */
	this->usage.ru_nsignals++;

	if (prof->it_interval)
	    prof->it_value = prof->it_interval;
    }

    for (fn = 0; fn < timer_functions; fn++)
	(*timer_map[fn])();

    /* Real itimers.  */
    if (alarms_head && alarms_head->alarm_time <= ticks)
    {
	int kpid = alarms_head - alarms;

#ifdef __PARANOIA__
	if (kpid < 0 || kpid >= SYSTEM_PROCESS_MAX ||
	    status(kpid) == PROCESS_ZOMBIE ||
	    status(kpid) == PROCESS_UNUSED)
	    PANIC("got kpid %x status=%d\n", kpid, status(kpid));
#endif

	reset_alarm(kpid);

	if (u_area[kpid].real.it_interval)
	{
	    u_area[kpid].real.it_value = u_area[kpid].real.it_interval;
	    set_alarm(kpid);
	}

	kill_kpid(kpid, SIGALRM);
    }
}


/* The process scheduler.  I really hope it is fast enough :-)  */

int
scheduler(void)
{
    int priority;
    int first_priority_level;
    int kpid, old_kpid = current_kpid;


#ifdef __PARANOIA__
    if (current_kpid < 0 || current_kpid > SYSTEM_PROCESS_MAX)
	PANIC("bad current_kpid (%x)\n", current_kpid);
#endif

    /* current_kpid == IDLE when the process calling scheduler() has
       just exited.  */
    if (current_kpid != IDLE)
    {
	first_priority_level = sched_touched ? MAX_PRIORITY_LEVEL :
			       sched_data[current_kpid].cpu_usage;

	/* If the process is asleep, don't try to delete it from the
	   scheduler list.  */
	if (this->status != PROCESS_ASLEEP)
	{
	    delete_schedulable_process(current_kpid);
	    new_schedulable_process(current_kpid,
				    --sched_data[current_kpid].cpu_usage);
	}
    }
    else
	first_priority_level = MAX_PRIORITY_LEVEL;      /* Really needed ?  */

    sched_touched = 0;

    /* Loop through the priority lists, trying to find an eligible process
       in their heads.  In most cases, we will find one at the very  first
       loop.  However, when a new process has been  waked up, we  have  to
       start searching from the highest priority list.  The process  waked
       up is normally inserted in the head of a high priority list so even
       in this situation there is a *big* chance to find it quickly.  */

    for (priority = first_priority_level; priority; priority--)
	if (sched_head[priority])
	{
	    kpid = sched_head[priority];
	    goto found;
	}

    /* No process has been found yet.  All the processes have exceeded
       their time quota.  So we have to reinsert them into their  base
       priority list.  */

    while ((kpid = sched_head[0]))
    {
	delete_schedulable_process(kpid);
	new_schedulable_process(kpid, sched_data[kpid].priority);
    }

    /* Rescan the priority lists.  Maybe this time we can find a
       process ready to run.  */

    for (priority = MAX_PRIORITY_LEVEL; priority; priority--)
	if (sched_head[priority])
	{
	    kpid = sched_head[priority];
	    goto found;
	}

    /* Bad luck.  Nothing to do.  Schedule IDLE.  */
    old_kpid = -1;
    kpid = IDLE;

   found:

    /* Ok, we have found an eligible process.  */
    this = &u_area[current_kpid = kpid];

#ifdef __PARANOIA__
    if (current_kpid < 0 || current_kpid > SYSTEM_PROCESS_MAX)
	PANIC("bad current_kpid (%x)\n", current_kpid);
#endif

    tss_descr[current_kpid].low_flags = VALID_TSS_ARB;

    /* Don't perform a context switch if the new selected process is
       the same as the old one.  It's faster - context switches are
       are expensive.  */
    if (current_kpid == old_kpid)
	return -1;

    /* Statistics. */
    if (old_kpid >= 0)
	u_area[old_kpid].usage.ru_nivcsw++;

    /* Returns the kernel process id of the process to switch to.  */
    return current_kpid;
}


/*
 * From the kernel point of view priorities are from 1 to 39 (39 levels) and
 * 39 is the higher priority.
 * From the user point of view priorities are from -19 to 19 (39 levels) and
 * -19 is the higher priority.
 */

/*
 * Get the priority of a process / set of processes.
 * The *real* priority is computed by the C library. We  can't  return  here
 * a negative value since it would be interpreted as an error. The C library
 * should return 20 - priority.
 */

int
sys_getpriority(int which, int who)
{
   int kpid;
   int max_priority = 0;


    switch (which)
    {
	case PRIO_PROCESS:

	    if (who == 0)
		return sched_data[current_kpid].priority;

	    for (kpid = SWAPPER; kpid; kpid = NEXT(kpid))
		if (pid(kpid) == who)
		    return sched_data[kpid].priority;

	    return -ESRCH;

	case PRIO_PGRP:

	    if (who == 0)
		who = u_area[current_kpid].pgrp;

	    for (kpid = SWAPPER; kpid; kpid = NEXT(kpid))
		if (u_area[kpid].pgrp == who)
		    max_priority = min(max_priority,sched_data[kpid].priority);

	    return max_priority ? max_priority : -ESRCH;

	case PRIO_USER:

	    if (who == 0)
		who = ruid(current_kpid);

	    for (kpid = SWAPPER; kpid; kpid = NEXT(kpid))
		if (ruid(kpid) == who)
		    max_priority = min(max_priority,sched_data[kpid].priority);

	    return max_priority ? max_priority : -ESRCH;
    }

    return -EINVAL;
}


/*
 * Set the priority of a process / set of processes.
 */

int
sys_setpriority(int which, int who, int priority)
{
    int kpid, found = 0;


    /* Only the superuser can set a negative priority.  */
    if (priority < 0 && !superuser())
	return -EACCES;

    /* Adjust bad priorities.  */
    if (priority < PRIO_MIN) priority = PRIO_MIN;
    if (priority > PRIO_MAX) priority = PRIO_MAX;

    switch (which)
    {
	case PRIO_PROCESS:

	    if (who == 0)
	    {
		sched_data[current_kpid].priority = 20 - priority;
		sched_touched = 1;
		return 0;
	    }

	    for (kpid = INIT; kpid; kpid = NEXT(kpid))
		if (pid(kpid) == who)
		{
		    sched_data[kpid].priority = 20 - priority;
		    sched_touched = 1;
		    return 0;
		}

	    return -ESRCH;

	case PRIO_PGRP:

	    if (who == 0)
		who = this->pgrp;

	    for (kpid = INIT; kpid; kpid = NEXT(kpid))
		if (pgrp(kpid) == who)
		{
		    sched_data[kpid].priority = 20 - priority;
		    sched_touched = found = 1;
		}

	    return found ? 0 : -ESRCH;

	case PRIO_USER:

	    if (who == 0)
		who = ruid(current_kpid);

	    for (kpid = INIT; kpid; kpid = NEXT(kpid))
		if (ruid(kpid) == who)
		{
		    sched_data[kpid].priority = 20 - priority;
		    sched_touched = found = 1;
		}

	    return found ? 0 : -ESRCH;
    }

    return -EINVAL;
}


/*
 * Let's start the *real* work!
 */

void
idle(void)
{
    for(;;);
}
