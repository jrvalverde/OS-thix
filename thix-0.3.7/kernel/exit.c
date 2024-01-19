/* exit.c -- Process termination management routines.  */

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
#include <thix/kpid.h>
#include <thix/process.h>
#include <thix/sched.h>
#include <thix/inode.h>
#include <thix/limits.h>
#include <thix/fdc.h>
#include <thix/buffer.h>
#include <thix/inode.h>
#include <thix/generic.h>


/*
 * Remove a zombie process from the process table.  Free up all the
 * resources it is still using (the internal (kernel) stack and the
 * page directory, see sys_exit() for details) and add its resource
 * usage to the parent (the current process).
 */

void
release_zombie(int zombie_kpid)
{
    tss_t *zombie = &u_area[zombie_kpid];


    add_rusage(&this->cusage, &zombie->usage);

    release_page(zombie->istack >> PAGE_SHIFT);
    release_page((unsigned)zombie->pgdir >> PAGE_SHIFT);
    wakeup(&get_page);

    release_kpid(zombie_kpid);
    u_area[zombie_kpid].status = PROCESS_UNUSED;
    this->children--;

    DEBUG(4, "process %d released by process %d.\n",
	  u_area[zombie_kpid].pid, this->pid);
}


/*
 * Pick up the first zombie that is a child of the current process.
 * Return 0 if there is no such process.
 */

int
pick_zombie(void)
{
    int kpid;

    for (kpid = FIRST; kpid; kpid = NEXT(kpid))
    {
	if (status(kpid) == PROCESS_ZOMBIE && ppid(kpid) == this->pid)
	    return kpid;
    }

    this->sigpending &= ~(1 << SIGCHLD);
    return 0;
}


/*
 * Check for other instances of the current process (that share  the  same
 * binary).  Write permission to the image of a running process should not
 * be granted.
 */

int
currently_executing(int i_no)
{
    int kpid, saved_inode = this->a_out_inode;

    /* This test avoids executing the loop if 'this' is the only instance
       of the executable.  */
    if (i_vect[this->a_out_inode].nref == 1)
	return 0;

    /* This is ugly, but the code is simpler this way.  */
    this->a_out_inode = 0;

    for (kpid = INIT; kpid; kpid = NEXT(kpid))
	if (status(kpid) != PROCESS_ZOMBIE)
	    if (u_area[kpid].a_out_inode == i_no)
	    {
		this->a_out_inode = saved_inode;
		return 1;
	    }

    this->a_out_inode = saved_inode;
    return 0;
}


/*
 * Terminate the execution of the current process.  exit_code is the process
 * exit code and killed_by_signal is a flag specifying if the process called
 * sys_exit() as a result of receiving a signal.
 */

void
sys_exit(int exit_code, int killed_by_signal)
{
    extern int rebooting;
    int ufd, kpid, zombie_found = 0;

    if (rebooting)
	printk("[%d] ", this->pid);

    DEBUG(4, "(%d,%d)\n", exit_code, killed_by_signal);

    /* Mark the current process as 'unswapable'.  */
    this->swapable = 0;

    /* Add its children resource usage to its resource usage.  This is the
       value that the parent will get.  */
    add_rusage(&this->usage, &this->cusage);

    /* Remove all the memory used by this process  (data and stack segments)
       but don't remove its internal stack and page directory.  This is done
       just to be able to recover if something bad happens and this  process
       will try to return after becoming a zombie (see the infinite  loop at
       the end of this function).  In order to be able to call  reschedule()
       again we need at least a stack  (the internal one, of course)  and  a
       page directory (the trick is that the kernel page tables  are  shared
       by all the processes and the first page table pointers  in  the  page
       directory point to them, so we will be able to run in kernel mode).
       As a matter of fact, we will need those pages to run  reschedule() at
       the end of this function anyway (reschedule()  should  be  called  at
       least once).  */
    release_memory(0, this->brk, MEM_DATA);
    release_memory(this->stk, 0xFFFFFFFF - KERNEL_RESERVED_SPACE, MEM_STACK);

    /* Close all the file descriptors.  */
    for (ufd = 0; ufd < OPEN_MAX; ufd++)
	if (this->ufdt[ufd])
	    sys_close(ufd);

    /* Check if there is another instance of this process currently running.
       Only allow write operations on this inode if there is none.  */
    if (!currently_executing(this->a_out_inode))
	i_vect[this->a_out_inode].write_denied = 0;

    /* Release the current directory and binary image inode.  */
    i_release(this->cwd_inode);
    i_release(this->a_out_inode);

    /* The current process is now zombie.  Compute its exit code.  */
    this->status    = PROCESS_ZOMBIE;
    this->exit_code = (killed_by_signal) ? exit_code & 0xFF : exit_code << 8;

    /* If this process is a session leader, send a SIGHUP signal to all the
       processes belonging to the  same  session.  Don't  send  signals  to
       zombies.  */
    if (this->leader)
    {
	for (kpid = FIRST; kpid; kpid = NEXT(kpid))
	{
	    if (status(kpid)  != PROCESS_ZOMBIE &&
		session(kpid) == this->session)
	    {
		u_area[kpid].session = 0;
		u_area[kpid].pgrp    = 0;       /* ??? */
		u_area[kpid].ctty    = 0;

		kill_kpid(kpid, SIGHUP);
	    }
	}
    }

    /* Search for zombie children.  Connect all of them to the INIT process. */
    for (kpid = FIRST; kpid; kpid = NEXT(kpid))
	if (ppid(kpid) == this->pid)
	{
	    u_area[kpid].ppid  = 1;     /* INIT process id.  */
	    u_area[kpid].pkpid = INIT;  /* INIT kernel id.  */
	    u_area[INIT].children++;    /* INIT adopted a new child.  */

	    if (status(kpid) == PROCESS_ZOMBIE)
		zombie_found = 1;
	}

    /* Check the current process pid.  The IDLE and SWAPPER processes
       are not allowed to exit.  */
    if (this->pid == 0 || this->pid == 1)
	PANIC("kernel process is trying to exit !!!!");

    /* If at least one zombie process has been found, notify INIT, its
       current parent.  */
    if (zombie_found)
	kill_kpid(INIT, SIGCHLD);

    /* Notify the parent of the current process about the death of one of
       its children.  */
    kill_kpid(this->pkpid, SIGCHLD);

    /* Notify the scheduler about the death of the current process.  */
    delete_schedulable_process(current_kpid);

    /* A process has just died.  +  */
    processes--;

    /* Force a complete search through the priority lists the next time
       the scheduler is started.  */
    sched_touched = 1;
    reset_alarm(current_kpid);
    this = &u_area[IDLE];

    /* This just avoid testing the process status in the scheduler.
       It's faster and cleaner.  */
    current_kpid = IDLE;

    /* Try to recover if something bad happens and the just killed process
       try to return.  Just reschedule forever, but let me know about it...  */
    for (;;)
    {
	reschedule();
	printk("************* zombie process %x(%x) is back. ************\n",
		u_area[current_kpid].pid, current_kpid);
    }
}
