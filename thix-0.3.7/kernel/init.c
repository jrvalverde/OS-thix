/* init.c -- Loads /bin/init from the file system.  */

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
#include <thix/string.h>
#include <thix/sched.h>
#include <thix/kpid.h>
#include <thix/process.h>
#include <thix/signal.h>
#include <thix/limits.h>
#include <thix/a.out.h>
#include <thix/inode.h>
#include <thix/buffer.h>
#include <thix/fs.h>
#include <thix/gendrv.h>


extern descriptor_t	ldt[SYSTEM_OPEN_MAX][DESCRIPTORS_PER_LDT];
extern __pid_t		latest_pid;
extern unsigned         work_set[SYSTEM_OPEN_MAX][WORK_SET_SIZE];
extern unsigned         *work_set_ptr[];
extern int              root_inode;


tss_t *initp = &u_area[INIT];


/*
 * Starts the /bin/init process.
 */

int
startinit(void)
{
    int mp, vmdev;
    superblock *sb;
    extpgtbl_t *extpgtbl;
    int i_no = 0, i, result;
    unsigned *block_map = NULL;
    iblk_data ibd = { 0, 0, 0 };
    unsigned i_data[DIRECT_BLOCKS];
    unsigned text_size, data_size, bss_size, text_start, limit;


    initp->cwd_inode = root_inode;

    /*
     * UGLY!: without this "/bin/init" will not pass the namei test because
     * it is not located into the user level address space.
     */

    {
	unsigned KERNEL_RESERVED_SPACE_safe = KERNEL_RESERVED_SPACE;

	KERNEL_RESERVED_SPACE = 0;
	i_no = namei("/bin/init");
	KERNEL_RESERVED_SPACE = KERNEL_RESERVED_SPACE_safe;
    }

    /* Have we found the /bin/init binary ?  */
    if (i_no < 0)
	PANIC("can't find /bin/init\n");

    initp->a_out_inode = i_no;
    i_vect[i_no].nref++;
    i_vect[i_no].write_denied = 1;

    /* Get the root file system mount point and the associated
       superblock.  */
    mp = mp(i_vect[i_no].device);
    sb = mpt[mp].sb;

    vmdev = mpt[mp].vmdev;

    /* Read the image header.  */
    result = read_header(i_no, sb, &text_size, &data_size,
				   &bss_size,  &text_start);

    if (result < 0)
	PANIC("bad magic number or machine type\n");

    initp->text_pages = text_size >> PAGE_SHIFT;
    initp->data_pages = data_size >> PAGE_SHIFT;
    initp->bss_pages  = (bss_size >> PAGE_SHIFT) +
			((bss_size & (PAGE_SIZE - 1)) ? 1 : 0);

    initp->brk = ((1 + initp->text_pages +
		       initp->data_pages +
		       initp->bss_pages) << PAGE_SHIFT) - 1;

    initp->stk       = (0xFFFFFFFF - ARG_MAX) + 1 - KERNEL_RESERVED_SPACE;
    initp->max_brk   = i386VM_PAGES; /* unlimited.  */
    initp->max_stack = i386VM_PAGES; /* unlimited.  */

    initp->istack   = get_page() << PAGE_SHIFT;
    initp->pgdir    = (pgtbl_t **)(get_page() << PAGE_SHIFT);

    /* The first page tables of the virtual address space belong to the
       kernel.  */
    for (i = 0; i < KERNEL_PAGE_TABLES; i++)
	initp->pgdir[i] = (pgtbl_t *)((unsigned)&kernel_page_table[i*1024] |
				      PAGE_SWP);

    /* The next page table belongs to the process.  */
    initp->pgdir[i++] = (pgtbl_t *)((get_page() << PAGE_SHIFT) | PAGE_UWP);

    /* Just delete the rest of them.  */
    lmemset(initp->pgdir + i, 0, 1024 - i);

    /* Process number 2 (INIT (pid = 1)) will not have an image bigger than
       4 Mb. */

    /* Clear the page table.  */
    lmemset((void *)((unsigned)initp->pgdir[KERNEL_PAGE_TABLES] & ~0xFFF), 0,
	    1024);

    extpgtbl=(extpgtbl_t*)((unsigned)initp->pgdir[KERNEL_PAGE_TABLES]&~0xFFF);

    /* Don't map page 0 in order to catch NULL pointer references.  */
    extpgtbl->vmdev = 0;
    extpgtbl->block = 0;

    /* Set up the extended page table entries.  */
    for (i = 1; i < 1 + initp->text_pages + initp->data_pages; i++)
    {
	if (ibd.nblocks <= 0)
	{
	    if (ibd.buf_no)
		buf_release(ibd.buf_no);

	    block_map = bmap(i_no, (i - 1) * PAGE_SIZE, &ibd, i_data);
	}

	(extpgtbl + i)->vmdev = vmdev;
	(extpgtbl + i)->block = block_map[ibd.index];
	(extpgtbl + i)->type  = PAGE_FILE;

	ibd.index   += PAGE_SIZE / sb->s_blksz;
	ibd.nblocks -= PAGE_SIZE / sb->s_blksz;
    }

    if (ibd.buf_no)
	buf_release(ibd.buf_no);

    /* Set up the local descriptor table.  */
    ldt[INIT][1].low_limit  =  1 + initp->text_pages;
    ldt[INIT][1].high_limit = (1 + initp->text_pages) >> 16;

    limit = ((0xFFFFFFFF - KERNEL_RESERVED_SPACE) + 1) >> PAGE_SHIFT;
    ldt[INIT][2].low_limit  = (unsigned short)(limit);
    ldt[INIT][2].high_limit = (unsigned short)(limit >> 16);

    /* Set up INIT's u_area.  */
    initp->esp0   = initp->istack + PAGE_SIZE - 4;
    initp->eip    = text_start;
    initp->eflags = 0x200;
    initp->eax    = initp->ecx = initp->edx = 0;
    initp->ebx    = initp->esi = initp->edi = 0;
    initp->esp    = (0xFFFFFFFF - ARG_MAX) + 1 - KERNEL_RESERVED_SPACE;
    initp->ebp    = 0;
    initp->cs     = 0x8 + 7;
    initp->ds     = initp->es = 0x10 + 7;
    initp->fs     = initp->gs = 0x10 + 7;
    initp->ss     = 0x10 + 7;

    initp->sleep_address = NULL;
    initp->sigpending    = 0;
    initp->sigblocked    = 0;
    initp->children      = 0;
    initp->ppid          = 0;
    initp->pid           = ++latest_pid;

    /* There are no pending signals.  */
    lmemset(initp->sigactions, 0, sizeof(initp->sigactions) >> 2);

    /* No open file descriptors.  */
    lmemset(initp->ufdt, 0, sizeof(initp->ufdt) >> 2);
    initp->ff_ufd = 0;

    /* No close_on_exec file descriptors.  */
    lmemset(initp->close_on_exec, 0, sizeof(initp->close_on_exec) >> 2);

    memcpy(initp->args, "init", 5);

    initp->usage.ru_utime.tv_sec  = initp->usage.ru_utime.tv_usec  = 0;
    initp->usage.ru_stime.tv_sec  = initp->usage.ru_stime.tv_usec  = 0;
    initp->cusage.ru_utime.tv_sec = initp->cusage.ru_utime.tv_usec = 0;
    initp->cusage.ru_stime.tv_sec = initp->cusage.ru_stime.tv_usec = 0;

    initp->ruid = initp->euid = initp->suid = 0;
    initp->rgid = initp->egid = initp->sgid = 0;

    initp->wakeup_priority = 0;

    work_set_ptr[INIT] = work_set[INIT];

    /* Mark INIT as 'swapable' and set its 'exec_flag' (sys_setpgid() will
       need this one day...  */
    initp->swapable    = 1;
    initp->exec_flag   = 1;

    /* INIT is not a session leader right now.  */
    initp->leader  = 0;
    initp->session = 0;

    /* Set up the INIT status.  Also notify the scheduler.  */
    initp->status = PROCESS_RUNNING;
    sched_data[INIT].priority   = I_THRESHOLD_PRIORITY >> 1;
    new_schedulable_process(INIT, I_THRESHOLD_PRIORITY >> 1);

    /* Release the image inode.  */
    i_get_rid_of(i_no);

    return 1;
}
