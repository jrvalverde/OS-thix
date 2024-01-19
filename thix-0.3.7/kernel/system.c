/* system.c -- System identification & some system dependent routines.  */

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
#include <thix/utsname.h>
#include <thix/string.h>
#include <thix/inode.h>
#include <thix/stat.h>
#include <thix/system.h>
#include <thix/limits.h>
#include <thix/process.h>
#include <thix/fs.h>
#include <thix/sched.h>
#include <thix/gendrv.h>


extern int free_pages;
extern int total_free_swap_blocks;


static char SystemName[]                  = "Thix";
static char SystemRelease[]               = VERSION;
static char SystemVersion[]               = __TIME__" "__DATE__;
static char NodeName[_UTSNAME_LENGTH+1]   = "chang";
static char DomainName[_UTSNAME_LENGTH+1] = "";
#ifdef __i386k__
static char Machine[]                     = "i386";
#else
static char Machine[]                     = "i486";
#endif


/*
 * Return system information in the struc utsname structure.
 */

int
sys_uname(struct utsname *name)
{
    if (!ok_to_write_area(name, sizeof(struct utsname)))
	return -EFAULT;

    DEBUG(4, "(%x)\n", name);

    SYSCALL_PROLOG();

    memcpy(name->sysname,  SystemName,
	   min(_UTSNAME_LENGTH, strlen(SystemName) + 1));

    memcpy(name->nodename, NodeName,
	   min(_UTSNAME_LENGTH, strlen(NodeName) + 1));

    memcpy(name->release, SystemRelease,
	   min(_UTSNAME_LENGTH, strlen(SystemRelease) + 1));

    memcpy(name->version, SystemVersion,
	   min(_UTSNAME_LENGTH, strlen(SystemVersion) + 1));

    memcpy(name->machine, Machine,
	   min(_UTSNAME_LENGTH, strlen(Machine) + 1));

    memcpy(name->domainname, DomainName,
	   min(_UTSNAME_LENGTH, strlen(DomainName) + 1));

    SYSCALL_EPILOG();

    return 0;
}


/*
 * Set the hostname.
 */

int
sys_sethostname(char *name, size_t length)
{
    if (!superuser())
	return -EPERM;

    if (!ok_to_read_area(name, length))
	return -EFAULT;

    DEBUG(4, "(%s, %d)\n", name, length);

    memcpy(NodeName, name, min(_UTSNAME_LENGTH, length));

    if (length < _UTSNAME_LENGTH)
	NodeName[length] = 0;

    return 0;
}


/*
 * Set the domainname.
 */

int
sys_setdomainname(char *name, size_t length)
{
    if (!superuser())
	return -EPERM;

    if (!ok_to_read_area(name, length))
	return -EFAULT;

    DEBUG(4, "(%s, %d)\n", name, length);

    memcpy(DomainName, name, min(_UTSNAME_LENGTH, length));

    if (length < _UTSNAME_LENGTH)
	DomainName[length] = 0;

    return 0;
}


/*
 * Display some kernel related information at boot time.
 */

void
show_sys_info(void)
{
    printk("%s OS %s Release %s [%s]\n",
	   SystemName, SystemRelease, SystemVersion, Machine);
    printk("Compiled with the GNU C compiler version %s\n", __VERSION__);

#ifdef __DEBUG__
    printk("Debug info enabled on level(s) %s.\n", thix_debug_levels);
#else
    printk("Distribution version: no debug info.\n");
#endif /* __DEBUG__ */

#ifdef __PARANOIA__
    printk("Consistency checkings enabled.\n");
#else
    printk("No consistency checkings.\n");
#endif /* __PARANOIA__ */
}


/*
 * Get process specific information.  Mainly called from ps(1).
 * Thix specific.
 */

int
sys_getprocinfo(int kpid, struct procinfo *pi)
{
    DEBUG(4, "(%d, %x)\n", kpid, pi);

    if (kpid < 0 || kpid >= SYSTEM_PROCESS_MAX)
	return -EINVAL;

    if (!ok_to_write_area(pi, sizeof(struct procinfo)))
	return -EFAULT;

    SYSCALL_PROLOG();

    pi->status          = u_area[kpid].status;
    pi->pid             = u_area[kpid].pid;
    pi->ppid            = u_area[kpid].ppid;
    pi->inode           = u_area[kpid].a_out_inode;
    pi->ctty            = u_area[kpid].ctty;
    pi->brk             = u_area[kpid].brk;
    pi->stk             = u_area[kpid].stk;
    pi->umask           = u_area[kpid].umask;
    pi->ruid            = u_area[kpid].ruid;
    pi->rgid            = u_area[kpid].rgid;
    pi->euid            = u_area[kpid].euid;
    pi->egid            = u_area[kpid].egid;
    pi->usage           = u_area[kpid].usage;
    pi->cusage          = u_area[kpid].cusage;
    pi->children        = u_area[kpid].children;
    pi->start_time      = u_area[kpid].start_time;
    pi->wakeup_priority = u_area[kpid].wakeup_priority;

    pi->priority        = 20 - sched_data[kpid].priority;

    memcpy(pi->args, u_area[kpid].args, sizeof(u_area[kpid].args));

    SYSCALL_EPILOG();
    return 0;
}


/*
 * Get system specific run time information.  Mainly called from
 * vmstat(1).  Thix specific.
 */

int
sys_getsysinfo(struct sysinfo *si)
{
    DEBUG(4, "(%x)\n", si);

    if (!ok_to_write_area(si, sizeof(struct sysinfo)))
	return -EFAULT;

    SYSCALL_PROLOG();

    si->processes               = processes;
    si->free_pages              = free_pages;
    si->total_free_swap_blocks  = total_free_swap_blocks;
/*
    si->free_buffers            = free_buffers;
*/

    SYSCALL_EPILOG();
    return 0;
}
