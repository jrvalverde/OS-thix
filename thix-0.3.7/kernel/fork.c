/* fork.c -- The fork() system call.  */

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
#include <thix/vmdev.h>
#include <thix/string.h>
#include <thix/sched.h>
#include <thix/kpid.h>
#include <thix/process.h>
#include <thix/inode.h>
#include <thix/limits.h>
#include <thix/inode.h>
#include <thix/generic.h>
#include <thix/proc/i386.h>


extern descriptor_t     ldt[SYSTEM_PROCESS_MAX][DESCRIPTORS_PER_LDT];
extern unsigned         work_set[SYSTEM_PROCESS_MAX][WORK_SET_SIZE];
extern unsigned         *work_set_ptr[];
extern swap_t           sdt[];
extern unsigned         user_mode_ESP[];
extern unsigned         kernel_mode_ESP[];

extern start_child();


/*
 * The fork system call.
 */

int
sys_fork(void)
{
    volatile tss_t *child;
    extpgtbl_t *extpgtbl;
    unsigned pgtbl_addr, brk, stk;
    pgtbl_t *pgtbl, **pgdir, **cpgdir;
    int child_kpid, requested_pages = 2, limit, ufd;
    int page_table, page, flag = 1, shifted_page_table, next_page_table;


    DEBUG(5, "\n");

    /* Check the kernel process table for an unused entry.  */
    if ((child_kpid = get_kpid()) == 0)
    {
	DEBUG(2, "no free process slot\n");
	return -EAGAIN;
    }

    pgdir = this->pgdir;

    /* Compute the number of page tables required.  */
    for (page_table = KERNEL_PAGE_TABLES; page_table < 1024; page_table++)
	if (pgdir[page_table])
	    requested_pages++;

    reserve_pages(requested_pages);

    /* Mark the parent as 'unswapable'; the child will inherit this
       temporary. */
    this->swapable = 0;

    /* Copy the parent u_area into the child u_area.  Fields that are
       different will be initialized later.  */
    *(child = &u_area[child_kpid]) = *this;

    /* Also copy the user_mode_ESP & kernel_mode_ESP values.  */
    user_mode_ESP[child_kpid]   = user_mode_ESP[current_kpid];
    kernel_mode_ESP[child_kpid] = kernel_mode_ESP[current_kpid];

    /* We have a new process.  */
    processes++;

    /* Allocate a new page for the process internal (kernel) stack and
       for the page directory.  Fill the page directory with zeros.  */
    child->istack  = get_page() << PAGE_SHIFT;
    child->pgdir   = (pgtbl_t **)(get_page() << PAGE_SHIFT);
    lmemset(cpgdir = child->pgdir, 0, PAGE_SIZE >> 2);

    brk = (this->brk + KERNEL_RESERVED_SPACE) >> PAGE_SHIFT;
    stk = (this->stk + KERNEL_RESERVED_SPACE) >> PAGE_SHIFT;

    /* We should use memcpy here!  */
    for (page_table = 0; page_table < KERNEL_PAGE_TABLES; page_table++)
	cpgdir[page_table] = pgdir[page_table];

    /* Loop through the parent page tables in order to  initialize  the
       corresponding child data structures.  This is the child VM setup
       code.  */
    for (next_page_table = KERNEL_PAGE_TABLES + 1;
	 page_table < 1024;
	 page_table = next_page_table++)
    {
	if (pgdir[page_table] == 0)
	    continue;

	pgtbl_addr = get_page() << PAGE_SHIFT;
	cpgdir[page_table] = (pgtbl_t *)(pgtbl_addr | PAGE_UWP);
	pgtbl = (pgtbl_t *)((unsigned)pgdir[page_table] & ~0xFFF);
	extpgtbl = (extpgtbl_t *)pgtbl;

	shifted_page_table = (page_table  + 1) << 10;
	limit = 1024;

	if (flag && shifted_page_table > brk)
	{
	    limit -= shifted_page_table - brk - 1;
	    next_page_table = stk >> 10;
	    flag = 0;
	}

	page = (page_table == (stk >> 10) && limit == 1024) ? stk & 0x3FF : 0;

	for (; page < limit; page++)
	    if (pgtbl[page].present)
	    {
	       pgtbl[page].rw = 0;
	       pgdata[pgtbl[page].address].count++;
	    }
	    else
	    {
	       if (extpgtbl[page].type == PAGE_SWAP && extpgtbl[page].block)
		  sdt[vmdev_aux(extpgtbl[page].vmdev)].
		      bitmap[extpgtbl[page].block]++;
	    }

	memcpy((void *)pgtbl_addr, pgtbl, PAGE_SIZE);

	invalidate_cache();
    }

    /* This process has a new child.  */
    this->children++;
    memcpy(ldt[child_kpid], ldt[current_kpid],
	   DESCRIPTORS_PER_LDT * sizeof(descriptor_t));

    /* Initialize the child specific u_area fields.  */
    child->eflags  |= 0x200;
    child->ss       = child->ss0 = (SYSTEM_PROCESS_MAX * 3 + child_kpid) << 3;
    child->ldt      = (SYSTEM_PROCESS_MAX + child_kpid) << 3;
    child->pid      = find_unused_pid();
    child->ppid     = this->pid;
    child->pkpid    = current_kpid;
    child->children = 0;

    child->real.it_value    = child->real.it_interval    = 0;
    child->virtual.it_value = child->virtual.it_interval = 0;
    child->prof.it_value    = child->prof.it_interval    = 0;

    child->usage.ru_utime.tv_sec  = child->usage.ru_utime.tv_usec  = 0;
    child->usage.ru_stime.tv_sec  = child->usage.ru_stime.tv_usec  = 0;
    child->cusage.ru_utime.tv_sec = child->cusage.ru_utime.tv_usec = 0;
    child->cusage.ru_stime.tv_sec = child->cusage.ru_stime.tv_usec = 0;

    child->start_time = seconds;
    child->sigpending = 0;
    child->exec_flag  = 0;
    child->leader     = 0;

    /* Notify the scheduler about the existence of this new process.  */
    sched_data[child_kpid].priority = sched_data[current_kpid].priority;
    new_schedulable_process(child_kpid, sched_data[child_kpid].priority);

    /* Force a complete search through the priority lists the next time
       the scheduler is started.  */
    sched_touched = 1;

    /* Set up the child work set.  */
    work_set_ptr[child_kpid] = work_set[child_kpid];

    /* Increment the current directory inode reference count.  */
    i_duplicate(this->cwd_inode);

    /* chroot() is not yet implemented... */
    /* i_duplicate(this->root); */

    /* Increment the executable image inode reference count.  */
    i_duplicate(this->a_out_inode);

    /* Loop through the open files descriptor table incrementing their
       counts.  */
    for (ufd = 0; ufd < OPEN_MAX; ufd++)
	if (this->ufdt[ufd])
	    fdt[this->ufdt[ufd]].count++;

    this->ff_ufd = 0;           /* Do we really need this ?  */
    this->kwreq  = 0;           /* Not sure about this either...  */

    /* Mark both parent and child as 'swapable'.  */
    this->swapable  = 1;
    child->swapable = 1;

    /* Set up the child registers in order to be able to start.  */
    child->ds = child->es = 0x10;
    child->esp = child->esp0 = child->istack + 0xFF0 - 16 * sizeof(unsigned);

    /* Initialize the startup stack of the new process.  We have to
    create a stack containing the values of the following registers:

    ss esp eflags cs eip ebp ds es eax ecx edx ebx esp ebp esi edi

    These registers can be found in precisely the same order on the
    parent internal stack.  kernel_mode_ESP[current_kpid] points to
    ebp so kernel_mode_ESP[current_kpid] - 40 points to edi.  Take a
    look at syscall.S for details.  */

    memcpy((void *)child->esp,
	   (void *)(kernel_mode_ESP[current_kpid] - 10 * sizeof(unsigned)),
	   16 * sizeof(unsigned));

    child->eip = (unsigned)start_child;

    /* This is the parent return point.  Return the child process id.  */
    return child->pid;
}
