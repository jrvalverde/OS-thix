/* exec.c -- The exec() system call.  */

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
#include <thix/vmalloc.h>
#include <thix/klib.h>
#include <thix/string.h>
#include <thix/process.h>
#include <thix/sched.h>
#include <thix/limits.h>
#include <thix/a.out.h>
#include <thix/inode.h>
#include <thix/generic.h>
#include <thix/gendrv.h>
#include <thix/stat.h>
#include <thix/buffer.h>
#include <thix/limits.h>
#include <thix/regular.h>


extern unsigned         free_list_clear_time;
extern descriptor_t     ldt[SYSTEM_PROCESS_MAX][DESCRIPTORS_PER_LDT];


struct ae_data
{
    /* A vector of pages used to store the command line arguments and
       the environment of the new program.  */
    unsigned page_vect[ARG_MAX / PAGE_SIZE];

    /* The current page.  */
    int page;

    /* The offset into the current page.  */
    int offset;
};


/*
 * Add a new string to the process environment.
 */

void
add_ae_data(struct ae_data *ae, void *ptr, int size)
{
    char *cptr = (char *)ptr;

    if (ae->offset + size < PAGE_SIZE)
    {
	memcpy((char *)(ae->page_vect[ae->page]) + ae->offset, cptr, size);
	ae->offset += size;
    }
    else
    {
	memcpy((char *)(ae->page_vect[ae->page++]) + ae->offset,
	       cptr, PAGE_SIZE - ae->offset);

	for (size -= PAGE_SIZE - ae->offset; size > 0; size -= PAGE_SIZE)
	{
	    reserve_pages(1);

	    ae->page_vect[ae->page] = get_page() << PAGE_SHIFT;

	    memcpy((char *)(ae->page_vect[ae->page]), cptr,
		   ae->offset = min(size, PAGE_SIZE));

	    cptr += PAGE_SIZE;
	}
    }
}


/*
 * Build the process environment (argv + envp).
 */

int
ae_setup(struct ae_data *ae, char *argv[], char *envp[])
{
    unsigned value, _argv, _envp;
    char *dummy_envp[] = { NULL };
    int ae_pages, a_no = 1, e_no = 1;
    int i, ae_spc, a_spc = 0, e_spc = 0, zero = 0, len;

    /*
	Total required space for argv and envp is:
	    - 2 * sizeof(unsigned)              // _argv & _envp
	    - (a_no + e_no) * sizeof(unsigned)  // arrays of pointers
	    - a_spc + e_spc                     // strings

	Restriction:
	    - the space used by argv + envp + pinters + strings < ARG_MAX
    */

    if (argv == (char **)USER_NULL)
	return -EINVAL;
    else
	if (!ok_to_read_pointer_list(argv))
	    return -EFAULT;

    if (envp == (char **)USER_NULL)
	envp = dummy_envp;
    else
	if (!ok_to_read_pointer_list(envp))
	    return -EFAULT;

    for (i = 0; argv[i]; a_no++, i++)
    {
	if (!ok_to_read_string(argv[i] + KERNEL_RESERVED_SPACE))
	    return -EFAULT;
	a_spc += strlen(argv[i] + KERNEL_RESERVED_SPACE) + 1;
    }

    for (i = 0; envp[i]; e_no++, i++)
    {
	if (!ok_to_read_string(envp[i] + KERNEL_RESERVED_SPACE))
	    return -EFAULT;
	e_spc += strlen(envp[i] + KERNEL_RESERVED_SPACE) + 1;
    }

    /* This loop is used in order to get the argv/envp of another process.
       It will probably disapear when the kernel will be able to read
       (on behalf of the current process) the memory of another process.  */
    {
	int cnt = 0;
	char *args = this->args;

	for (i = 0; argv[i]; args += len + 1, cnt += len + 1, i++)
	{
	    len = strlen(argv[i] + KERNEL_RESERVED_SPACE);
	    if (cnt + len < 131)
		memcpy(args, argv[i] + KERNEL_RESERVED_SPACE, len);
	    else
		break;
	    args[len] = ' ';
	}
	args[0] = 0;
    }

    ae_spc = (2 + a_no + e_no) * sizeof(unsigned) + a_spc + e_spc;

    /* Check if the total amount of arg/env is less than ARG_MAX.  If it is,
       return with error.  */
    if (ae_spc > ARG_MAX)
	return 0;

    /* Initialize the 'ae' structure.  */
    ae->offset = 0;
    ae->page   = 0;
    reserve_pages(1);
    ae->page_vect[0] = get_page() << PAGE_SHIFT;

    /* Compute the total number of pages that will be used for storing
       the command line argumments and the environment of the program.  */
    ae_pages = (ae_spc >> PAGE_SHIFT) + ((ae_spc & (PAGE_SIZE - 1)) ? 1 : 0);

    _argv = (0xFFFFFFFF - KERNEL_RESERVED_SPACE) + 1 - ARG_MAX +
	    2 * sizeof(unsigned);
    this->argv = (char **)_argv;
    add_ae_data(ae, &_argv, sizeof(unsigned));

    _envp = _argv + a_no * sizeof(unsigned);
    this->envp = (char **)_envp;
    add_ae_data(ae, &_envp, sizeof(unsigned));

    value = _argv + (a_no + e_no) * sizeof(unsigned);

    for (i = 0; i < a_no - 1; i++)
    {
	add_ae_data(ae, &value, sizeof(unsigned));
	value += strlen(argv[i] + KERNEL_RESERVED_SPACE) + 1;
    }
    add_ae_data(ae, &zero, sizeof(unsigned));

    for (i = 0; i < e_no - 1; i++)
    {
	add_ae_data(ae, &value, sizeof(unsigned));
	value += strlen(envp[i] + KERNEL_RESERVED_SPACE) + 1;
    }
    add_ae_data(ae, &zero, sizeof(unsigned));

    for (i = 0; i < a_no - 1; i++)
	add_ae_data(ae, argv[i] + KERNEL_RESERVED_SPACE,
		    strlen(argv[i] + KERNEL_RESERVED_SPACE) + 1);

    for (i = 0; i < e_no - 1; i++)
    {
	add_ae_data(ae, envp[i] + KERNEL_RESERVED_SPACE,
		    strlen(envp[i] + KERNEL_RESERVED_SPACE) + 1);
    }

    return ae_pages;
}


/*
 * Read a binary image header.
 */

int
read_header(int i_no, superblock *sb, int *text_size, int *data_size,
	    int *bss_size,  int *text_start)
{
    struct exec *header;

    /* Yes, I know, is ugly.  I'll change this.  */
    while ((header = (struct exec *)vm_xalloc(sb->s_blksz)) == NULL);

    i_unlock(i_no);
    a_out_read(i_no, (char *)header, 0, sb->s_blksz);
    i_lock(i_no);

    /* FIXME: more checks here.  */
    if (N_BADMAG(*header)            ||
	N_MACHTYPE(*header) != M_386/* ||
	header->a_entry != 0x1020*/)
    {
	DEBUG(4, "not an executable file (a_info=%x).\n", header->a_info);
	vm_xfree(header);
	return -ENOEXEC;
    }

    *text_size  = header->a_text;
    *data_size  = header->a_data;
    *bss_size   = header->a_bss;
    *text_start = header->a_entry;

    vm_xfree(header);

    DEBUG(4, "text_size  = %x\n", *text_size);
    DEBUG(4, "data_size  = %x\n", *data_size);
    DEBUG(4, "bss_size   = %x\n", *bss_size);
    DEBUG(4, "text_start = %x\n", *text_start);

    return 0;
}


/*
 * The exec system call.
 */

int
sys_exec(char *filename, char *argv[], char *envp[])
{
    superblock *sb;
    struct ae_data ae;
    extpgtbl_t *extpgtbl;
    unsigned new_page_addr;
    int i_no, ae_pages, ufd;
    unsigned *block_map = NULL;
    int i, j, result, mp, vmdev;
    iblk_data ibd = { 0, 0, 0 };
    unsigned i_data[DIRECT_BLOCKS];
    unsigned text_size, data_size, bss_size, text_start;


    DEBUG(4, "(%s, %x, %x)\n", filename, argv, envp);

    i_no = namei(filename);

    if (i_no < 0)
	return i_no;

    if (!S_ISREG(i_vect[i_no].i_mode) || !permitted(i_no, X_OK, 0))
    {
	DEBUG(4, "cannot execute: not a regular file or no X_OK\n");
	i_get_rid_of(i_no);
	return -EACCES;
    }

    if (i_vect[i_no].exec_denied)
    {
	DEBUG(4, "cannot execute: text file busy\n");
	i_get_rid_of(i_no);
	return -ETXTBSY;
    }

    /* Don't allow writings to this file while we are running it.  */
    i_vect[i_no].write_denied = 1;

    /* Mark the current process as 'unswapable'.  */
    this->swapable = 0;

    mp = mp(i_vect[i_no].device);
    sb = mpt[mp].sb;

    vmdev = mpt[mp].vmdev;

    result = read_header(i_no, sb, &text_size, &data_size,
				   &bss_size,  &text_start);

    /* Only temporary.  */
    if (result < 0)
    {
	i_get_rid_of(i_no);
	i_vect[i_no].write_denied = 0;
	this->swapable = 1;
	return result;
    }

    /* Just in case sh calls sh ... :-)  */
    if (this->a_out_inode != i_no)
    {
	i_release(this->a_out_inode);
	this->a_out_inode = i_no;
	i_duplicate(i_no);
    }

    if ((ae_pages = ae_setup(&ae, argv, envp)) < 0)
    {
	DEBUG(4, "cannot execute: bad argv or envp\n");
	i_get_rid_of(i_no);
	i_vect[i_no].write_denied = 0;
	this->swapable = 1;
	return ae_pages;
    }

    if ((i_vect[i_no].i_mode & S_ISUID) == S_ISUID)
	this->euid = i_vect[i_no].i_uid;

    if ((i_vect[i_no].i_mode & S_ISGID) == S_ISGID)
	this->egid = i_vect[i_no].i_gid;

    /* The sticky bit is ignored.  */

    release_memory(0, this->brk, MEM_DATA);
    release_memory(this->stk, 0xFFFFFFFF - KERNEL_RESERVED_SPACE, MEM_STACK);

    this->text_pages = text_size >> PAGE_SHIFT;
    this->data_pages = data_size >> PAGE_SHIFT;
    this->bss_pages  = (bss_size >> PAGE_SHIFT) +
			((bss_size & (PAGE_SIZE - 1)) ? 1 : 0);

    this->brk = ((1 + this->text_pages +
		      this->data_pages +
		      this->bss_pages) << PAGE_SHIFT) - 1;
    this->stk = (0xFFFFFFFF - KERNEL_RESERVED_SPACE) + 1 - ARG_MAX;
    this->max_brk   = i386VM_PAGES; /* Unlimited.  */
    this->max_stack = i386VM_PAGES; /* Unlimited.  */

    /* Maybe it will be a good idea to delete the inherited page tables.  */

    /* Right now, a process cannot have an image bigger than 4 Mb.  It
       is a easy task to change this.  When we will, we should also
       modify reserve_pages().  */

    /* The first user level page table.  */
    reserve_pages(1);

    /* Allocate the page table.  */
    this->pgdir[KERNEL_PAGE_TABLES] = (pgtbl_t *)((get_page()<<PAGE_SHIFT) |
						  PAGE_UWP);

    /* Clear the page table.  */
    lmemset((void *)((unsigned)this->pgdir[KERNEL_PAGE_TABLES] & ~0xFFF), 0,
	    1024);

    extpgtbl = (extpgtbl_t*)((unsigned)this->pgdir[KERNEL_PAGE_TABLES]&~0xFFF);

    /* Don't map page 0 in order to catch NULL pointer references.  */
    extpgtbl->vmdev = 0;
    extpgtbl->block = 0;

    /* Set up the extended page table entries.  */
    for (i = 1; i < 1 + this->text_pages + this->data_pages; i++)
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

    ldt[current_kpid][1].low_limit  =  1 + this->text_pages;
    ldt[current_kpid][1].high_limit = (1 + this->text_pages) >> 16;

    this->esp0 = this->istack + PAGE_SIZE - 4;

    this->sleep_address = NULL;

    for (i = 0; i < _NSIG; i++)
	if (this->sigactions[i].sa_handler > SIG_IGN)
	    this->sigactions[i].sa_handler = SIG_DFL;

    /* The stack page table.  */
    reserve_pages(1);

    new_page_addr = get_page() << PAGE_SHIFT;
    this->pgdir[0x3FF] = (pgtbl_t *)(new_page_addr | PAGE_UWP);
    lmemset((void *)new_page_addr, 0, 1024);

    for (i = 1024 - ARG_MAX / PAGE_SIZE, j = 0; j < ae_pages; i++, j++)
    {
	((unsigned *)new_page_addr)[i] = ae.page_vect[j] |
					 PAGE_ACCESSED | PAGE_UWP;
	pgdata[ae.page_vect[j] >> PAGE_SHIFT].type = PAGE_SWAP;
    }

    for (ufd = 0; ufd < OPEN_MAX; ufd++)
	if (this->ufdt[ufd] && is_close_on_exec(ufd))
	    sys_close(ufd);

    i_set_atime(i_no);

    if (i_vect[i_no].i_ktime + 10 > free_list_clear_time)
	clear_free_list();

    i_get_rid_of(i_no);
    this->swapable  = 1;
    this->exec_flag = 1;
    return text_start;
}
