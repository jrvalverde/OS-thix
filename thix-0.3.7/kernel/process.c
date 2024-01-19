/* process.c -- Process management routines.  */

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


int processes  = 3;
__pid_t latest_pid = 0;


/*
 * Find an unused process id.
 */

__pid_t
find_unused_pid()
{
    int kpid;

  retry:
    /* As far as I know, the only real reason why UNIX systems usually
       limit the process id to numbers <= 32767 is because it is
       considered a nuisance to type something like:
       		kill -9 1723129412
     */
    if (++latest_pid > 32767)
	latest_pid = 1 + 1;

    /* Make sure there is no other process currently using this number
       as either process id, process group id or session id.  POSIX.  */
    for (kpid = FIRST; kpid; kpid = NEXT(kpid))
    {
	if (latest_pid == pid(kpid)  ||
	    latest_pid == pgrp(kpid) ||
	    latest_pid == session(kpid))
	    goto retry;
    }

    return latest_pid;
}


#ifdef __DEBUG__
void
process_debug_info(void)
{
    int kpid;

    for (kpid = SWAPPER; kpid; kpid = NEXT(kpid))
	printk("pid=%w ppid=%w s=%d slp=%x c=%d sw=%d %s\n",
		u_area[kpid].pid, u_area[kpid].ppid,
		u_area[kpid].status, u_area[kpid].sleep_address,
		u_area[kpid].children, u_area[kpid].swapable,
		u_area[kpid].args);
}
#endif


/*
 * Return the process id of the current process.
 */

int
sys_getpid(void)
{
    return this->pid;
}


/*
 * Return the process id of the parent of the current process.
 */

int
sys_getppid(void)
{
    return this->ppid;
}


/*
 * This function sets both the real and effective user id of  the  current
 * process to 'uid', provided that the process has appropriate privileges.
 */

int
sys_setuid(int uid)
{
    if (superuser())
	this->euid = this->ruid = this->suid = uid;
    else
	if (this->ruid == uid || this->suid == uid)
	    this->euid = uid;
	else
	    return -EPERM;
    return 0;
}


/*
 * This function sets both the real and effective group id of the  current
 * process to 'gid', provided that the process has appropriate privileges.
 */

int
sys_setgid(int gid)
{
    if (superuser())
	this->egid = this->rgid = this->sgid = gid;
    else
	if (this->rgid == gid || this->sgid == gid)
	    this->egid = gid;
	else
	    return -EPERM;
    return 0;
}


/*
 * Return the real user id of the current process.
 */

int
sys_getuid(void)
{
    return this->ruid;
}


/*
 * Return the real group id of the current process.
 */

int
sys_getgid(void)
{
    return this->rgid;
}


/*
 * Return the effective user id of the current process.
 */

int
sys_geteuid(void)
{
    return this->euid;
}


/*
 * Return the effective group id of the current process.
 */

int
sys_getegid(void)
{
    return this->egid;
}


/*
 * Set the session id of the current process.  The current process becomes
 * the leader of the new session.
 */

int
sys_setsid(void)
{
    if (this->leader)
	return -EPERM;

    this->leader   = 1;
    this->session  = this->pid;
    this->pgrp     = this->pid;
    this->ctty     = 0;

    return this->pgrp;
}


/*
 * Set the process group id.  Not implemented yet.
 */

int
sys_setpgid(__pid_t pid, __pid_t pgid)
{
#if 0
    __pid_t pgrp;
    int kpid;


    if (pid == 0)
	pid = this->pid;
    else
    {
	for (kpid = INIT; kpid; kpid = NEXT(kpid))
	{
	}

	if (pid != this->pid && ppid(kpid) != this->pid)
	    return -EINVAL;
    }

    if (pgid == 0)
	pgrp = pid;
    else
    {
    }
#endif

    DEBUG(2, "sys_setpgid() is not implemented.\n");
    DEBUG(2, "In fact, job control is not supported at all :-(\n");
    return -ENOSYS;
}


/*
 * Return the process group of the current process.
 */

int
sys_getpgrp(void)
{
    return this->pgrp;
}
