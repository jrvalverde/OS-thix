/* signal.c -- Signals management routines.  */

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
#include <thix/sleep.h>
#include <thix/kpid.h>
#include <thix/process.h>
#include <thix/signal.h>
#include <thix/limits.h>


/*
 * issig() is called to check for signals at every return from kernel mode
 * to user mode.  See startup.s for details.
 */


__sighandler_t
issig(int *signump)
{
    int kpid, signum;
    unsigned sigmask;
    __sighandler_t signalhandler;

    /* If there are no pending signals, just return.  */
    if (!signals())
	return SIG_DFL;

    for (signum = 1, sigmask = 2; signum < _NSIG; signum++, sigmask <<= 1)
	if (signals() & sigmask)
	{
	    if (signum == SIGCHLD)
	    {
		switch((unsigned)this->sigactions[signum].sa_handler)
		{
		    case SIG_IGN: while((kpid = pick_zombie()))
				  {
				      DEBUG(2, "releasing zombie %d\n",
					    pid(kpid));
				      release_zombie(kpid);
				  }
				  this->sigpending &= ~sigmask;
		    case SIG_DFL: return SIG_DFL;
		    default:      break;
		}
	    }

	    if ((unsigned)this->sigactions[signum].sa_handler)
	    {
		signalhandler = this->sigactions[signum].sa_handler;
		this->sigpending &= ~sigmask;

		/* POSIX.1:  Once an action is  installed  for  a  specific
		   signal, it remains installed  until  another  action  is
		   explicitly requested (by another call to the sigaction()
		   function), or until one of the exec functions is called.  */
		/*
		if (signum != SIGILL && signum != SIGTRAP)
		    this->sigactions[signum].sa_handler = SIG_DFL;
		*/

		/* Save the current mask.  */
		this->sigoldmask = this->sigblocked;

		/* Block the current signal and all the signals specified
		   in the sa_mask field.  */
		this->sigblocked |= sigmask;
		this->sigblocked |= this->sigactions[signum].sa_mask;

		*signump = signum;

#ifdef __PARANOIA__
		if((unsigned)signalhandler>=((1+this->text_pages)<<PAGE_SHIFT))
		{
		    printk("[%s]\n", this->args);
		    printk("text_pages=%x data_pages=%x bss_pages=%x\n",
			  this->text_pages, this->data_pages, this->bss_pages);
		    PANIC("invalid signal handler for signal %d: %x\n",
			  signum, signalhandler);
		}
#endif

		return signalhandler;
	    }
	    else
	    {
		DEBUG(2, "Killed by signal %d.\n", signum);
		/* Depending on the signal type, we should do a 'core dump'
		   here. But I first have to find out how the 'core' format
		   looks like :-).  */
		sys_exit(signum, 1);
	    }
	}

    return SIG_DFL;
}


/*
 * __issig() is actually called only when we are sure that a signal has been
 * received.  It forces the current process to exit if it doesn't  handle  a
 * received signal or returns a signal number otherwise.
 */

int
__issig(void)
{
    int signum;
    unsigned sigmask;

    /* Force the current process to exit if it has received a signal
       that it doesn't handle (and that signal is not SIGCHLD, of course).  */

    for (signum = 1, sigmask = 2; signum < _NSIG; signum++, sigmask <<= 1)
	if (signals() & sigmask &&
	    this->sigactions[signum].sa_handler == SIG_DFL &&
	    signum != SIGCHLD)
		sys_exit(signum, 1);

    /* Return a signal number.  */
    for (signum = 1, sigmask = 2; signum < _NSIG; signum++, sigmask <<= 1)
	if (signals() & sigmask)
	    return signum;

    printk("__issig() errorr !  Trying to recover...\n");
    return 0;
}


/*
 * sys_sigend()
 */

int
sys_sigend(void)
{
    this->sigblocked = this->sigoldmask;
    return 0;
}


/*
 * The obsolete signal() system call.
 */

__sighandler_t
sys_signal(int signum, __sighandler_t function)
{
    DEBUG(2, "\x7Obsolete system call.  Use sys_sigaction() instead.\n");

    return (__sighandler_t)-ENOSYS;
}


/*
 * Check if the current process has the appropriate permission to send
 * a signal to the process with the kernel process id 'kpid'.
 */

inline int
can_kill_kpid(int kpid)
{
    DEBUG(4, "(%x)\n", kpid);

    return superuser() || this->euid == euid(kpid) || this->ruid == ruid(kpid);
}


/*
 * Sends a signal to the process with the kernel process id 'kpid'.
 */

void
kill_kpid(int kpid, int signum)
{
    /* The swapper can't receive signals.  It it does, something really
       bad happened, so just PANIC!  */
    if (kpid == SWAPPER)
	PANIC("Swapper received signal %d\n", signum);

    /* Don't send ignored signals to processes, unless we are dealing
       with SIGCHLD.  */
    if (u_area[kpid].sigactions[signum].sa_handler == SIG_IGN &&
	signum != SIGCHLD)
	return;

    u_area[kpid].sigpending |= 1 << signum;

    /* Statistics.  */
    u_area[kpid].usage.ru_nsignals++;

    /* If the process is asleep, wake it up.  */
    if (status(kpid) == PROCESS_ASLEEP &&
	wakeup_priority(kpid) < U_THRESHOLD_PRIORITY)
	wakeup_process(kpid);
}


/*
 * Loop through the process table, trying to find the process  'pid'.
 * If found and the current process is allowed to send signals to it,
 * do so.  Otherwise, report an error.
 */

int
kill_pid(__pid_t pid, int signum)
{
    int kpid;

    for (kpid = INIT; kpid; kpid = NEXT(kpid))
	if (status(kpid) != PROCESS_ZOMBIE && pid(kpid) == pid)
	{
	    if (can_kill_kpid(kpid))
		kill_kpid(kpid, signum);
	    else
		return -EPERM;

	    return 0;
	}

    return -ESRCH;
}


/*
 * Try to send the 'signum' signal to all the processes in the process
 * group 'pgrp'.  Don't send the signal to the current process  unless
 * 'kill_the_sender' has a nonzero value.
 */

int
kill_pgrp(__pid_t pgrp, int signum, int kill_the_sender)
{
    int kpid, found = 0;

    for (kpid = FIRST; kpid; kpid = NEXT(kpid))
	if (status(kpid) != PROCESS_ZOMBIE && pgrp(kpid) == pgrp)
	{
	    if (kpid == current_kpid && !kill_the_sender)
		continue;

	    if (can_kill_kpid(kpid))
		kill_kpid(kpid, signum);
	    else
		return -EPERM;

	    found = 1;
	}

    return found ? 0 : -ESRCH;
}


/*
 * Send the 'signum' signal to all the processes the current process  is
 * allowed to.  Superuser processes have the right to send signal to any
 * process, while normal processes are only allowed to send  signals  to
 * processes with the same effective user id.
 */

int
kill_all(int signum)
{
    int kpid;

    if (superuser())
    {
	for (kpid = FIRST; kpid; kpid = NEXT(kpid))
	    if (status(kpid) != PROCESS_ZOMBIE)
	    {
		kill_kpid(kpid, signum);
	    }
    }
    else
    {
	for (kpid = FIRST; kpid; kpid = NEXT(kpid))
	    if (status(kpid) != PROCESS_ZOMBIE && euid(kpid) == this->euid)
	    {
		kill_kpid(kpid, signum);
	    }
    }

    return 0;
}


/*
 * Search a process with the id 'pid' into the kernel process table.  Used
 * in sys_kill to check the  presence  of  the  destination  process  when
 * delivering signals.
 */

int
search_pid(__pid_t pid)
{
    int kpid;

    for (kpid = INIT; kpid; kpid = NEXT(kpid))
	if (pid(kpid) == pid)
	    return 0;

    return -ESRCH;
}


/*
 * Try to send the  'signum'  signal to the process 'pid':
 *
 *  - pid > 0: try to send the given signal to that pid.
 *
 *  - pid == 0: send 'signum' to all the processes within the  same
 *    process group.  Don't send the signal to the current process.
 *
 *  - pid == -1: send the signal to all the processes  the  current
 *    process is allowed to.
 *
 *  - pid < -1: send the 'signum' signal to all  the  processes  in
 *    the -pid process group.  Also send the signal to the  current
 *    process.
 */

int
sys_kill(__pid_t pid, int signum)
{
    DEBUG(4, "(%d,%d)\n", pid, signum);

    if (signum == 0)
	return search_pid(pid);

    /* Check for invalid signals.  */
    if (signum < 0 || _NSIG <= signum)
	return -EINVAL;

    /* Don't send SIGKILL to init.  Original idea from a Linux doc.  */
    if (pid == 1 && signum == SIGKILL)
	return -EPERM;

    if (pid > 0)
	return kill_pid(pid, signum);

    if (pid == 0)
	return kill_pgrp(this->pgrp, signum, 0);

    if (pid == -1)
	return kill_all(signum);

    if (pid < -1)
	return kill_pgrp(-pid, signum, 1);

    /* Not reached.  */
    return 0;
}


/*
 * Force the current process to go to sleep until a signal is received.
 */

int
sys_pause(void)
{
    int signum;

    for (;;)
    {
	signum = sleep(&sys_pause, WAIT_SIGNAL);

	if (signum == 0)
	    signum = SIGCHLD;

	switch ((unsigned)this->sigactions[signum].sa_handler)
	{
	    case SIG_DFL: if (signum != SIGCHLD)
			      sys_exit(signum, 1);
	    case SIG_IGN: break;
	    default:      return -EINTR;
	}
    }
}


/*
 * Just another obsolete system call.
 */

int
sys_wait(int *wait_stat)
{
    DEBUG(2, "\x7Obsolete system call.  Use sys_waitpid() instead.\n");

    return -ENOSYS;
}


/*
 * Wait for a child to die.  See the waitpid() manual page for details.
 */

int
sys_waitpid(__pid_t pid, int *wait_stat, int options)
{
    int kpid, all;

    DEBUG(5, "(%d,%x,%d)\n", pid, wait_stat, options);

    if (options & ~(WNOHANG | WUNTRACED))
	return -EINVAL;

    if (wait_stat != (int *)USER_NULL)
	if (!ok_to_write_area(wait_stat, sizeof(int)))
	    return -EFAULT;

    all = this->sigactions[SIGCHLD].sa_handler == SIG_IGN;

    while (this->children)
    {
	if (!all && (kpid = pick_zombie()))
	{
	    if ((pid == pid(kpid))                              ||
		(pid == WAIT_MYGRP && pgrp(kpid) == this->pgrp) ||
		(pid == WAIT_ANY)                               ||
		(pid <  WAIT_ANY   && pgrp(kpid) == -pid))
	    {
		int zpid = pid(kpid);

		if (wait_stat != (int *)USER_NULL)
		{
		    SYSCALL_PROLOG();
		    *wait_stat = u_area[kpid].exit_code;
		    SYSCALL_EPILOG();
		}

		release_zombie(kpid);

		return zpid;
	    }

	    /* FIXME: there is a *BIG* problem here: if we don't reset the
	       SIGCHLD bit in the pending signals mask, this will lead  to
	       an endless loop (sleep() will return  without  waiting, the
	       pid/kpid/pgrp condition will still be false, etc...).
	       If we do reset it and later waitpid() is interrupted  by  a
	       signal, the next time waitpid() will be called it will  not
	       be able to figure out that one of its children had  exited.
	       This really neads to be fixed! */
	    this->sigpending &= ~(1 << SIGCHLD);
	}

	if (this->children == 0)
	    break;

	if (options & WNOHANG)
	    break;

	if (sleep(&sys_waitpid, WAIT_CHILD_EXIT))
	    return -EINTR;
    }

    return -ECHILD;
}


/*
 * Signals that are pending but blocked are member of the returned set.
 */

int
sys_sigpending(__sigset_t *set)
{
    if (!ok_to_write_area(set, sizeof(__sigset_t)))
	return -EFAULT;

    SYSCALL_PROLOG();
    *set = this->sigpending & this->sigblocked;
    SYSCALL_EPILOG();
    return 0;
}


/*
 * Examine or change the process signal mask.
 */

int
sys_sigprocmask(int how, __sigset_t *set, __sigset_t *old_set)
{
    DEBUG(4, "(%d,%x,%x)\n", how, set, old_set);

    if (old_set != (__sigset_t *)USER_NULL)
	if (ok_to_write_area(old_set, sizeof(__sigset_t)))
	{
	    SYSCALL_PROLOG();
	    *old_set = this->sigblocked;
	    SYSCALL_EPILOG();
	}
	else
	    return -EFAULT;

    if (set == (__sigset_t *)USER_NULL)
	return 0;

    if (!ok_to_read_area(set, sizeof(__sigset_t)))
	return -EFAULT;

    switch (how)
    {
	case SIG_BLOCK  : this->sigblocked |=   *set & SIG_BLOCKABLE;  break;
	case SIG_UNBLOCK: this->sigblocked &= ~(*set & SIG_BLOCKABLE); break;
	case SIG_SETMASK: this->sigblocked  =   *set & SIG_BLOCKABLE;  break;
	default         : return -EINVAL;
    }

    return 0;
}

/*
 * The current process is effectively suspended until one of the signals
 * that is not a member of *set arrives.
 * ?? should sys_sigsuspend() return an error when *set contains SIGKILL
 * or SIGSTOP?
 */

int
sys_sigsuspend(__sigset_t *set)
{
    __sigset_t old_mask;

    DEBUG(4, "(%x)\n", set);

    if (!ok_to_read_area(set, sizeof(__sigset_t)))
	return -EFAULT;

    old_mask = this->sigblocked;
    this->sigblocked = *set & SIG_BLOCKABLE;
    sys_pause();
    this->sigblocked = old_mask;
    return -EINTR;
}


/*
 * Specify how a signal should be handled.
 *
 * Notes:
 *   - I think SA_RESTORE should be implemented in the C library.
 *   - SA_ONSTACK and SA_NOCLDSTOP are not implemented.
 */

int
sys_sigaction(int signum, struct sigaction *action,
	      struct sigaction *old_action)
{
    int kpid, zombie = 0;


    DEBUG(5, "(%d,%x,%x)\n", signum, action, old_action);
    DEBUG(5, "(%d,%x)\n", signum, action->sa_handler);

    if (signum <= 0 || _NSIG <= signum)
	return -EINVAL;

    if ((action != (struct sigaction *)USER_NULL      &&
	 !ok_to_read_area(action, sizeof(struct sigaction))) ||
	(old_action != (struct sigaction *)USER_NULL  &&
	 !ok_to_write_area(old_action, sizeof(struct sigaction))))
	return -EFAULT;

    if (action != (struct sigaction *)USER_NULL)
    {
	if (((1 << signum) & ~SIG_BLOCKABLE) && action->sa_handler != SIG_DFL)
	    return -EINVAL;

	if (!ok_to_read_area(action->sa_handler + KERNEL_RESERVED_SPACE, 0))
	    return -EFAULT;

	this->sigactions[signum] = *action;
    }

    if (old_action != (struct sigaction *)USER_NULL)
    {
	SYSCALL_PROLOG();
	*old_action = this->sigactions[signum];
	SYSCALL_EPILOG();
    }

    if (signum == SIGCHLD)
	for (kpid = INIT; kpid; kpid = NEXT(kpid))
	    if (status(kpid) == PROCESS_ZOMBIE)
		zombie = 1;

    if (zombie)
	kill_kpid(current_kpid, SIGCHLD);

    return 0;
}
