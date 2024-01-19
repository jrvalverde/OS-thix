#ifndef _THIX_VM_H
#define _THIX_VM_H


#ifndef _THIX_SIGNAL_H
#include <thix/signal.h>
#endif

#ifndef _THIX_ERRNO_H
#include <thix/errno.h>
#endif

#ifndef _THIX_TYPES_H
#include <thix/types.h>
#endif

#ifndef _THIX_SCHED_H
#include <thix/sched.h>
#endif

#ifndef _THIX_RESOURCE_H
#include <thix/resource.h>
#endif

#ifndef _THIX_FS_H
#include <thix/fs.h>
#endif

#ifndef _THIX_SEMAPHORE_H
#include <thix/semaphore.h>
#endif


#define GDT_DESCRIPTORS         0x200

#define DATA_ARB                0x92    /* Data  segment access right byte.  */
#define STACK_ARB               0x96    /* Stack segment access right byte.  */
#define CODE_ARB                0x9A
#define LDT_ARB                 0x82
#define VALID_TSS_ARB           0x89
#define INVALID_TSS_ARB         0x8B

#define DESCRIPTORS_PER_LDT     0x03    /* NULL descriptor,
					   text descriptor,
					   data + bss + stack descriptor.  */

/* i386/i486 page size.  */
#define PAGE_SIZE               0x1000

/* i386/i486 PAGE_SIZE bits.  */
#define PAGE_SHIFT              0x0C

/* The 'Present' page flag.  */
#define PAGE_PRESENT            0x01

/* The 'Read/Write' page flag.  */
#define PAGE_RW                 0x02

/* The 'User/Supervisor' page flag.  */
#define PAGE_US                 0x04

/* The 'Supervisor/Writable/Present' page table entry mask.  */
#define PAGE_SWP                (PAGE_RW | PAGE_PRESENT)

/* The 'User/Writable/Present' page table entry mask.  */
#define PAGE_UWP                (PAGE_US | PAGE_RW | PAGE_PRESENT)

/* The 'User/ReadOnly/Present' page table entry mask.  */
#define PAGE_URP                (PAGE_US | PAGE_PRESENT)

/* The 'Accessed' page flag.  */
#define PAGE_ACCESSED           0x40

/* The number of pages used for the BIOS & video memory on PCs.  */
#define HARDWARE_RESERVED_PAGES 0x60

/* The size of the i386/i486 virtual address space in number of pages.  */
#define i386VM_PAGES            0x100000

/* When the number of free pages in the system decreases below this limit,
  the swapper is started.  */
#define LOW_PAGES_LIMIT         0x20

/* When the number of free pages in the system increases over this limit,
   the swapper is stopped.  */
#define HIGH_PAGES_LIMIT        0x40

/* This is the size of the vector where we keep track of the last faulted
   in pages, in order to avoid trashing.  */
#define WORK_SET_SIZE           0x10

/* When a page is that old, it is eligible for swapping.  */
#define MAX_PAGE_AGE            0x03

/* The safe number of pages, that is, the minimum number of pages between
   the end of the data segment and the current stack pointer.  */
#define SAFE_PAGES              0x04


typedef struct
{
    unsigned low_limit   : 16,
	     low_address : 16,
	     med_address : 8,
	     low_flags   : 8,
	     high_limit  : 4,
	     high_flags  : 4,
	     high_address: 8;
} descriptor_t;


typedef struct
{
    unsigned page_index  : 12,
	     pgtbl_index : 10,
	     pgdir_index : 10;
} vmaddr_t;


typedef struct
{
    unsigned reserved : 1,
	     type     : 2,
	     vmdev    : 5,
	     block    : 24;

    unsigned char busy;
    unsigned char dedicated;
    unsigned char count;

   /* There are 48 unused bits here. Sorry. Busy, dedicated & count can be
      restricted to one  byte.  Maybe  I  should  expand  type/vmdev/block
      instead of busy/dedicated/count.  */

    union
    {
	unsigned age;	/* Used when the page is part of the address space. */
	unsigned map;	/* Used for dynamic data allocation. */
    } aux;

    unsigned short hprev;
    unsigned short hnext;
    unsigned short lprev;
    unsigned short lnext;
} pgdata_t;


typedef struct
{
    unsigned present      : 1,
	     rw           : 1,
	     us           : 1,
	     reserved0    : 2,
	     accessed     : 1,
	     dirty        : 1,
	     reserved1    : 2,
	     busy         : 1,
	     reservedOS   : 2,
	     address      : 20;
} pgtbl_t;


#define PAGE_DZ         0
#define PAGE_DF         1
#define PAGE_FILE       2
#define PAGE_SWAP       3


typedef struct
{
    unsigned present    : 1,
	     type       : 2,  /* Should the 'type' field be the first one!?  */
	     vmdev      : 5,
	     block      : 24;
} extpgtbl_t;


typedef struct
{
    int            vmdev;  /* VM device numbers.  */
    size_t         total;  /* Total number of pages.  */
    size_t         free;   /* Free number of pages.  */
    int            latest; /* Latest swapped block.  */
    semaphore      mutex;  /* Semaphore used for mutual exclusion.  */
    unsigned char *bitmap; /* Bitmap for keeping track of used pages. */
} swap_t;


typedef struct
{
    struct rlimit cpu;     /* Per-process CPU limit, in seconds.  */
    struct rlimit fsize;   /* Largest file that can be created, in bytes.  */
    struct rlimit data;    /* Maximum size of data segment, in bytes.  */
    struct rlimit stack;   /* Maximum size of stack segment, in bytes.  */
    struct rlimit core;    /* Largest core file, in bytes.  */
    struct rlimit rss;     /* Largest resident set size, in bytes.  */
    struct rlimit memlock; /* Locked-in-memory address space.  */
    struct rlimit nproc;   /* Number of processes.  */
    struct rlimit ofile;   /* Number of open files.  */
} rlimits;


typedef struct
{
    unsigned vmdev : 5,    /* vmdev currently used to swap VM pages in.  */
	     block : 24;   /* The device block from which we are currently
			      swapping in.  */
} busyblock_t;


typedef struct
{
    /* i80386 TSS */

    unsigned backlink;
    unsigned esp0;
    unsigned ss0;
    unsigned esp1;
    unsigned ss1;
    unsigned esp2;
    unsigned ss2;
    pgtbl_t  **pgdir;
    unsigned eip;
    unsigned eflags;
    unsigned eax;
    unsigned ecx;
    unsigned edx;
    unsigned ebx;
    unsigned esp;
    unsigned ebp;
    unsigned esi;
    unsigned edi;
    unsigned es;
    unsigned cs;
    unsigned ss;
    unsigned ds;
    unsigned fs;
    unsigned gs;
    unsigned ldt;
    unsigned io_bitmap_offset;

    /* End of i386 TSS.  */

    /* i80386 TSS auxiliary data (process data).  */
    __pid_t pid;             /* Process id.  */
    __pid_t ppid;            /* Parent process id.  */
    __pid_t session;         /* Session id.  */
    __pid_t pgrp;            /* Process group id.  */

    int pkpid;               /* Parent kernel process id.  */

    unsigned istack;         /* Process internal stack (kernel mode stack).  */

    int text_pages;          /* Number of text pages in the executable.  */
    int data_pages;          /* Number of data pages in the executable.  */
    int bss_pages;           /* Number of bss  pages in the executable . */

    unsigned brk;            /* Break value.  */
    unsigned stk;            /* Stack value.  */
    int max_brk;             /* Maximum break value in number of pages.  */
    int max_stack;           /* Maximum stack value in number of pages.  */

    volatile
    void *sleep_address;     /* Process' current sleeping address.  */
    int wakeup_priority;     /* Process current wakeup priority.  */
    int old_cpu_usage;       /* CPU usage before going to sleep.  */

    _itimerval real;         /* Real interval timer.  */
    _itimerval virtual;      /* Virtual interval timer.  */
    _itimerval prof;         /* Profile interval timer.  */

    rlimits limits;          /* Resource limits.  */

    struct rusage usage;     /* Resource usage.  */
    struct rusage cusage;    /* Dead children resource usage.  */

    int cwd_inode;           /* Current directory inode.  */
    unsigned short ufdt[256]; /* User file descriptors table.  */

    unsigned close_on_exec[8]; /* Close on exec bits.  */

    int a_out_inode;         /* Executable inode.  */

    int ff_ufd;              /* First free user file descriptor.  */

    int umask;               /* Process umask.  */

    __uid_t ruid, euid, suid; /* Real, effective and saved user  ids. POSIX. */
    __gid_t rgid, egid, sgid; /* Real, effective and saved group ids. POSIX. */

    char access_flag;        /* Flag used by sys_access() to force namei()
				to use ruid/rgid instead of euid/egid.  */

    char kwreq;              /* Kernel write request flag. Set when a system
				call tries to write data in the user address
				space because  page protection faults  don't
				occur when running in kernel mode.  The page
				protection fault routine is called "by hand"
				from the page fault routine.  This  is  only
				useful when running on i386.  */

    char children;           /* Number of children.  */
    char swapable;           /* Swapable flag. Used by swapper to  determine
				if the process is currently swapable.  */

    char status;             /* Process status.  */
    char exec_flag;          /* The process has executed an 'exec' system call
				since it was forked.  It will be used by the
				setpgid() system call.  */

    char leader;             /* The process is a session leader.  */

    unsigned short exit_code;/* Process exit code.  */

    volatile
    __sigset_t sigpending;   /* Mask of pending signals.  */
    __sigset_t sigblocked;   /* Mask of blocked signals.  */
    __sigset_t sigoldmask;   /* Mask of blocked signals to be restored when
				the current signal handler will return.  */
    struct sigaction sigactions[_NSIG]; /* Sigaction structures.  */

    __time_t start_time;     /* Process start time.  */

    __dev_t ctty;            /* Controlling terminal major/minor numbers.  */

    busyblock_t busyblock;   /* The block currently used to swap VM pages
				in.  */

    char **argv;             /* Command line arguments pointer.  */
    char **envp;             /* Environment pointer.  */

    char args[132];          /* Command line string. I hope I will get rid of
				this one day...  */
} tss_t;


#ifdef __i386k__
#define SYSCALL_PROLOG()        { this->kwreq = 1; }
#define SYSCALL_EPILOG()        { this->kwreq = 0; }
#else
#define SYSCALL_PROLOG()
#define SYSCALL_EPILOG()
#endif


static inline void
invalidate_cache()
{
    __asm__ __volatile__ ("mov %%cr3,%%eax
			   mov %%eax,%%cr3"     :
			  /* no output */       :
						:
			  "eax");
}


#define USER_NULL               KERNEL_RESERVED_SPACE

#define pid(kpid)               (u_area[kpid].pid)
#define ppid(kpid)              (u_area[kpid].ppid)
#define pgrp(kpid)              (u_area[kpid].pgrp)
#define session(kpid)		(u_area[kpid].session)

#define ruid(kpid)              (u_area[kpid].ruid)
#define euid(kpid)              (u_area[kpid].euid)
#define suid(kpid)              (u_area[kpid].suid)

#define rgid(kpid)              (u_area[kpid].rgid)
#define egid(kpid)              (u_area[kpid].egid)
#define sgid(kpid)              (u_area[kpid].sgid)

#define status(kpid)            (u_area[kpid].status)
#define session(kpid)           (u_area[kpid].session)
#define wakeup_priority(kpid)   (u_area[kpid].wakeup_priority)


extern tss_t *this;
extern tss_t u_area[];
extern pgdata_t *pgdata;
extern volatile int free_pages;
extern int total_free_swap_blocks;
extern unsigned KERNEL_PAGE_TABLES;
extern unsigned KERNEL_RESERVED_SPACE;
extern unsigned *kernel_page_table;


#define map_page(linear_kernel_address, physical_address)                   \
{                                                                           \
    kernel_page_table[(unsigned)(linear_kernel_address) >> PAGE_SHIFT] =    \
      (physical_address) + 7;                                               \
    invalidate_cache();                                                     \
}                                                                           \


#define is_mapped(linear_kernel_address)                     		    \
    (kernel_page_table[(unsigned)(linear_kernel_address) >> PAGE_SHIFT] & 1)


int read_header(int kpid, superblock *sb, int *text_size, int *data_size,
					  int *bss_size,  int *text_start);


void ldt0tr_init(void);
void paging_init(void);
void vm_init(void);
void swap_init(void);
void bss_init(void);

int  get_page(void);
int  release_page(int);
void reserve_pages(int);
void clear_free_list(void);

int  get_swap_block(int __swap_dev, int __swap_block);
void release_swap_block(int __swap_dev, int __swap_block);

void work_set_insert(int __process, unsigned __page);
int page_in_work_set(int __process, unsigned __page);

#define pgd_address(pgd)        ((void *)((pgd - pgdata) << PAGE_SHIFT))

void swap_page(pgdata_t *__pgd);
int  load_page(pgdata_t *__pgd);

void *vm_static_alloc(int __size, int __max_size, int __alignment);

/* A bunch of functions used to check system calls parameters.  */
int ok_to_read_area(void *__pointer, unsigned __size);
int ok_to_write_area(void *__pointer, unsigned __size);
int ok_to_read_string(char *__pointer);
int ok_to_read_pointer_list(char **__pointer);


#define MEM_DATA        0
#define MEM_STACK       1

void release_memory(unsigned __first_address,
		    unsigned __last_address,
		    int __type);


static inline void
reschedule(void)
{
    /* Statistics. */
    this->usage.ru_nvcsw++;

    __asm__ __volatile__("sti
			  int $0x31
			  cli");
}


#endif  /* _THIX_VM_H */
