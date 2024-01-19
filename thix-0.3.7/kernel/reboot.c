/* reboot.c -- Process management routines.  */

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
#include <thix/proc/i386.h>


int rebooting = 0;

extern int
currently_executing(int i_no);


/*
 * stop_process() stops the specified processes unconditionally.
 */

void
stop_process(int kpid)
{
    if (!currently_executing(u_area[kpid].a_out_inode))
	i_vect[u_area[kpid].a_out_inode].write_denied = 0;

    i_release(u_area[kpid].cwd_inode);
    i_release(u_area[kpid].a_out_inode);

    release_kpid(kpid);
    u_area[kpid].status = PROCESS_UNUSED;
    processes--;
}


/*
 * stop_all() stops all the processes unconditionally.  Use stop_process()
 * for this.  Only the current process is allowed to resume its execution.
 */

void
stop_all(void)
{
    int kpid;
    extern void scheduler_data_init();


    for (kpid = FIRST; kpid; kpid = NEXT(kpid))
    {
	if (kpid == current_kpid)
	    continue;

	if (status(kpid) == PROCESS_RUNNING ||
	    status(kpid) == PROCESS_STOPPED ||
	    status(kpid) == PROCESS_ASLEEP)
	{
		stop_process(kpid);
	}
    }

    /* Re-initialize the scheduler data.  Now all the priority lists are
       empty.  No process will be scheduled (including the current one),
       so we have to add it using new_schedulable_process().  */
    scheduler_data_init();
    new_schedulable_process(current_kpid, 20);
}


/*
 * Send a signal to all the processes except the current one.  Called from
 * sys_reboot() to send SIGTERM and SIGKILL to all the running processes.
 */

void
destroy_all(int signum)
{
    int kpid;

    for (kpid = FIRST; kpid; kpid = NEXT(kpid))
    {
	if (kpid == current_kpid)
	    continue;

	if (status(kpid) != PROCESS_ZOMBIE)
	    kill_kpid(kpid, signum);
    }
}


/*
 * Well, here we are.  sys_reboot() will shut  the  system  down.  It  will
 * stop INIT just to make sure it doesn't try to respawn new children, then
 * try to be nice sending a SIGTERM signal  to  all the running  processes.
 * The next step is to send them a  SIGKILL  signal and, finally, to  close
 * all the open  file descriptors and stop the system. It will also perform
 * consistency checkings.
 */

int
sys_reboot(int options)
{
    int i, ufd, errors = 0;
    void vtty_setfocus0(void);

    DEBUG(4, "\n");

    /* Only the super user can shut the system down.  */
    if (!superuser())
	return -EPERM;

    /* If already rebooting, return an error.  */
    if (rebooting)
	return -EINVAL;

    /* Everything seems ok, so let's do it...  */
    rebooting = 1;

    /* Switch to the first virtual console.  */
    vtty_setfocus0();

    printk("\nThe system is going down...\n");

    /* Stop the init process.  Needed because init will try to respawn
       its child when notified about their death.  It this is the init
       process, forget about respawn, it will never reach  user  level
       again.  */
    if (this->pid != 1)
	stop_process(INIT);

    /* Be nice, send all processes the SIGTERM signal first.  */
    destroy_all(SIGTERM);

    /* Give them a chance to handle it.  */
    enable();

    for (i = 0; i < 10 && processes > 3; i++)
	delay(200);

    disable();

    /* Only 3 processes should remain active.  These are the IDLE, SWAPPER
       and the current process.  If there are still other processes alive,
       send them the SIGKILL signal.  */
    if (processes > 3)
    {
	/* Send all processes the SIGKILL signal.  */
	destroy_all(SIGKILL);

	/* Give them a chance to handle it.  */
	enable();

	for (i = 0; i < 10 && processes > 3; i++)
	    delay(200);

	disable();
    }

    printk("\n");

    /* Close all the open file descriptors of the current process.  All the
       stuff here actually belongs to the sys_exit() call, but the  current
       process will never be able to call it.  So  we  want  to  make  sure
       everything is synced properly.  */
    for (ufd = 0; ufd < OPEN_MAX; ufd++)
	if (this->ufdt[ufd])
	    sys_close(ufd);

    /* Well, just to be sure...  I don't think this is really needed.  */
    if (!currently_executing(this->a_out_inode))
	i_vect[this->a_out_inode].write_denied = 0;

    /* Release the current directory and binary image inodes of the
       current process.  */
    i_release(this->cwd_inode);
    i_release(this->a_out_inode);

    /* We can even handle this :-).  Output a warning message.  */
    if (processes > 3)
    {
      printk("WARNING: SIGKILL sent but %d user processes are still alive!\n",
	     processes - 3);
      errors++;
    }

    /* Stops all the processes unconditionally.  */
    stop_all();

    /* Clean up the fs-related data structures.  */
    errors += fs_cleanup();

    /* Clean up the inode-related data structures.  */
    errors += i_cleanup();

    /* Clean up the buffer cache-related data structures.  */
    errors += buf_cleanup();

    /* If there have been errors, 30 seconds delay to see the messages.  */
    if (errors)
	sec_delay(30);

    printk("\n");
    printk("*******************\n");
    printk("The system is down.\n");
    printk("*******************\n");

    sec_delay(5);

    /*
    fdc_stop_motor();
    */

    halt();

    /* Well, we're down, so this is the point of no return... :-)  */
    return 0;
}
