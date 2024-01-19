#ifndef _THIX_SYSTEM_H
#define _THIX_SYSTEM_H


#ifndef _THIX_LIMITS_H
#include <thix/limits.h>
#endif

#ifndef _THIX_RESOURCE_H
#include <thix/resource.h>
#endif


/*
 * Thix specific stuff ...
 */

/*
 * struct procinfo is used by the getprocinfo() system call.
 */

struct procinfo
{
    int                 status;         /* Process status.  */
    int                 pid;            /* Process id.  */
    int                 ppid;           /* Parent process id.  */
    int                 inode;          /* Executable inode.  */
    int                 ctty;           /* Controling terminal.  */
    int                 priority;       /* Process priority.  */
    unsigned            brk;            /* Process break value.  */
    unsigned            stk;            /* Process stack value.  */
    int                 umask;          /* File creation mask.  */
    int                 ruid;           /* Real user  id.  */
    int                 rgid;           /* Real group id.  */
    int                 euid;           /* Effective user  id.  */
    int                 egid;           /* Effective group id.  */
    struct rusage       usage;          /* Resource usage.  */
    struct rusage       cusage;         /* Dead children resource usage.  */
    char                args[132];      /* Command line arguments.  */
    int                 children;       /* Number of children.  */
    int                 start_time;     /* Process start time.  */
    int                 wakeup_priority;/* Process wakeup priority.  */
};


/*
 * struct sysinfo is used by the getsysinfo() system call.
 */

struct sysinfo
{
    int processes;
    int free_pages;
    int total_free_swap_blocks;
};


#endif /* _THIX_SYSTEM_H */
