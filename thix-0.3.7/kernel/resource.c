/* resource.c -- Resources management routines.  */

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
#include <thix/process.h>
#include <thix/sched.h>
#include <thix/inode.h>
#include <thix/limits.h>
#include <thix/fdc.h>
#include <thix/buffer.h>
#include <thix/inode.h>
#include <thix/fs.h>
#include <thix/kpid.h>
#include <thix/resource.h>


/*
 * Get the current resource limits.
 */

int
sys_getrlimit(int resource, struct rlimit *rlp)
{
    DEBUG(4, "(%d,%x)\n", resource, rlp);

    if (!ok_to_write_area(rlp, sizeof(struct rlimit)))
	return -EFAULT;

    if (resource < 0 || resource >= RLIMIT_NLIMITS)
	return -EINVAL;

    SYSCALL_PROLOG();
    *rlp = ((struct rlimit *)(&this->limits))[resource];

    /* Only temporary, just to keep malloc() happy.  */
    if (resource == RLIMIT_DATA)
    {
	rlp->rlim_cur = this->brk;
	rlp->rlim_max = RLIMIT_INFINITY;
    }

    SYSCALL_EPILOG();
    return 0;
}


/*
 * Set the resource limits.  Check for appropriate privileges.
 */

int
sys_setrlimit(int resource, struct rlimit *rlp)
{
    struct rlimit *limit;

    DEBUG(4, "(%d,%x)\n", resource, rlp);

    if (!ok_to_read_area(rlp, sizeof(struct rlimit)))
	return -EFAULT;

    if (resource < 0 || resource >= RLIMIT_NLIMITS)
	return -EINVAL;

    if (rlp->rlim_cur < 0 || rlp->rlim_max < 0)
	return -EINVAL;

    if (rlp->rlim_cur > rlp->rlim_max)
	return -EINVAL;

    limit = &((struct rlimit *)(&this->limits))[resource];

    if (superuser())
	*limit = *rlp;
    else
    {
	if (rlp->rlim_max > limit->rlim_max ||
	    rlp->rlim_cur > limit->rlim_max)
	    return -EPERM;

	limit->rlim_cur = rlp->rlim_cur;
    }

    return 0;
}


/*
 * Get the resource usage statistics.  'who' specifies whether we want
 * the current process resource usage or the children ones.
 */

int
sys_getrusage(int who, struct rusage *usage)
{
    int kpid;


    DEBUG(4, "(%d,%x)\n", who, usage);

    if (!ok_to_write_area(usage, sizeof(struct rusage)))
	return -EFAULT;

    SYSCALL_PROLOG();

    switch (who)
    {
	case RUSAGE_SELF:
	    *usage = this->usage;
	    break;

	case RUSAGE_CHILDREN:
	    *usage = this->cusage;
	    break;

	default:
	    /* Return the resource usage information for the given pid (who),
	       as in the GNU system.  */
	    for (kpid = INIT; kpid; kpid = NEXT(kpid))
		if (status(kpid) != PROCESS_ZOMBIE && pid(kpid) == who)
		{
		    *usage = u_area[kpid].usage;
		    SYSCALL_EPILOG();
		    return 0;
		}

	    SYSCALL_EPILOG();
	    return -EINVAL;
    }

    SYSCALL_EPILOG()
    return 0;
}


/*
 * Add usage2 to usage1.  Perform the add operation depending on each
 * field type.  Used in release_zombie().
 */

void
add_rusage(struct rusage *usage1, struct rusage *usage2)
{
    usage1->ru_utime.tv_sec  += usage2->ru_utime.tv_sec;
    usage1->ru_utime.tv_usec += usage2->ru_utime.tv_usec;

    if (usage1->ru_utime.tv_usec >= 1000000)
    {
	usage1->ru_utime.tv_sec++;
	usage1->ru_utime.tv_usec -= 1000000;
    }

    usage1->ru_stime.tv_sec  += usage2->ru_stime.tv_sec;
    usage1->ru_stime.tv_usec += usage2->ru_stime.tv_usec;

    if (usage1->ru_stime.tv_usec >= 1000000)
    {
	usage1->ru_stime.tv_sec++;
	usage1->ru_stime.tv_usec -= 1000000;
    }

    usage1->ru_maxrss   += usage2->ru_maxrss;
    usage1->ru_ixrss    += usage2->ru_ixrss;
    usage1->ru_idrss    += usage2->ru_idrss;
    usage1->ru_isrss    += usage2->ru_isrss;
    usage1->ru_minflt   += usage2->ru_minflt;
    usage1->ru_majflt   += usage2->ru_majflt;
    usage1->ru_nswap    += usage2->ru_nswap;
    usage1->ru_inblock  += usage2->ru_inblock;
    usage1->ru_oublock  += usage2->ru_oublock;
    usage1->ru_msgsnd   += usage2->ru_msgsnd;
    usage1->ru_msgrcv   += usage2->ru_msgrcv;
    usage1->ru_nsignals += usage2->ru_nsignals;
    usage1->ru_nvcsw    += usage2->ru_nvcsw;
    usage1->ru_nivcsw   += usage2->ru_nivcsw;
}
