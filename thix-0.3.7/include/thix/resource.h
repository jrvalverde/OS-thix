#ifndef _THIX_RESOURCE_H
#define _THIX_RESOURCE_H


#ifndef _THIX_SCHED_H
#include <thix/sched.h>
#endif


#define RLIMIT_INFINITY 0x7FFFFFFF      /* No limit.  */
#define RLIM_INFINITY   RLIMIT_INFINITY /* The standard name :-(.  */


#define RLIMIT_CPU      0       /* Per-process CPU limit, in seconds.  */
#define RLIMIT_FSIZE    1       /* Largest file that can be created,
				   in bytes.  */
#define RLIMIT_DATA     2       /* Maximum size of data segment, in bytes.  */
#define RLIMIT_STACK    3       /* Maximum size of stack segment, in bytes.  */
#define RLIMIT_CORE     4       /* Largest core file that can be created,
				   in bytes.  */
#define RLIMIT_RSS      5       /* Largest resident set size, in bytes.  */
#define RLIMIT_MEMLOCK  6       /* Locked-in-memory address space.  */
#define RLIMIT_NPROC    7       /* Number of processes.  */
#define RLIMIT_OFILE    8       /* Number of open files. */
#define RLIMIT_NLIMITS  9       /* Number of limits.  */
#define RLIM_NLIMITS    RLIMIT_NLIMITS



#define RUSAGE_SELF       0     /* The calling process.  */
#define RUSAGE_CHILDREN (-1)    /* All of its terminated child processes.  */


struct rlimit
{
    int rlim_cur;               /* The current value of the limit.  */
    int rlim_max;               /* The maximum permissible limit value.  */
};


struct rusage
{
    struct timeval ru_utime;    /* Total amount of user time used.  */
    struct timeval ru_stime;    /* Total amount of system time used.  */
    long ru_maxrss;             /* Maximum resident set size (in Kb).  */
    long ru_ixrss;              /* Amount of sharing of text segment memory
				   with other processes (kilobyte-seconds).  */
    long ru_idrss;              /* Amount of data segment memory used
				   (kilobyte-seconds).  */
    long ru_isrss;              /* Amount of stack memory used
				   (kilobyte-seconds).  */
    long ru_minflt;             /* Number of soft page faults (i.e. those
				   serviced by reclaiming a page from the
				   list of pages awaiting reallocation.  */
    long ru_majflt;             /* Number of hard page faults (i.e. those that
				   required I/O).  */
    long ru_nswap;              /* Number of times a process was swapped out of
				   physical memory.  */
    long ru_inblock;            /* Number of input operations via the file
				   system.  Note: This and `ru_oublock' do not
				   include operations with the cache.  */
    long ru_oublock;            /* Number of output operations via the file
				   system.  */
    long ru_msgsnd;             /* Number of IPC messages sent.  */
    long ru_msgrcv;             /* Number of IPC messages received.  */
    long ru_nsignals;           /* Number of signals received.  */
    long ru_nvcsw;              /* Number of voluntary context switches, i.e.
				   because the process gave up the process
				   before it had to (usually to wait for some
				   resource to be available).  */
    long ru_nivcsw;             /* Number of involuntary context switches, i.e.
				   a higher priority process became runnable
				   or the current process used up its time
				   slice.  */
};


#ifdef __KERNEL__

void add_rusage(struct rusage *, struct rusage *);

#endif  /* __KERNEL__ */


#endif /* _THIX_RESOURCE_H  */
