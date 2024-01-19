#ifndef _THIX_SIGNAL_H
#define _THIX_SIGNAL_H


#define _NSIG   32
#define NSIG    _NSIG

/*
 * Some of these signals are defined only for compatibility ...
 */

#define SIGHUP          1       /* Hangup (POSIX).  */
#define SIGINT          2       /* Interrupt (ANSI).  */
#define SIGQUIT         3       /* Quit (POSIX).  */
#define SIGILL          4       /* Illegal instruction (ANSI).  */
#define SIGABRT         SIGIOT  /* Abort (ANSI).  */
#define SIGTRAP         5       /* Trace trap (POSIX).  */
#define SIGIOT          6       /* IOT trap.  */
#define SIGEMT          7       /* EMT trap.  */
#define SIGFPE          8       /* Floating-point exception (ANSI).  */
#define SIGKILL         9       /* Kill, unblockable (POSIX).  */
#define SIGBUS          10      /* Bus error.  */
#define SIGSEGV         11      /* Segmentation violation (ANSI).  */
#define SIGSYS          12      /* Bad argument to system call*/
#define SIGPIPE         13      /* Broken pipe (POSIX).  */
#define SIGALRM         14      /* Alarm clock (POSIX).  */
#define SIGTERM         15      /* Termination (ANSI).  */
#define SIGUSR1         16      /* User-defined signal 1 (POSIX).  */
#define SIGUSR2         17      /* User-defined signal 2 (POSIX).  */
#define SIGCHLD         18      /* Child status has changed (POSIX).  */
#define SIGCLD          SIGCHLD /* Same as SIGCHLD (System V).  */
#define SIGPWR          19      /* Power going down.  */
#define SIGWINCH        20      /* Window size change.  */
#define SIGURG          21      /* Urgent condition on socket.*/
#define SIGIO           SIGPOLL /* I/O now possible.  */
#define SIGPOLL         22      /* Same as SIGIO? (SVID).  */
#define SIGSTOP         23      /* Stop, unblockable (POSIX).  */
#define SIGTSTP         24      /* Keyboard stop (POSIX).  */
#define SIGCONT         25      /* Continue (POSIX).  */
#define SIGTTIN         26      /* Background read from tty (POSIX).  */
#define SIGTTOU         27      /* Background write to tty (POSIX).  */
#define SIGVTALRM       28      /* Virtual alarm clock.  */
#define SIGPROF         29      /* Profiling alarm clock.  */
#define SIGXCPU         30      /* CPU limit exceeded.  */
#define SIGXFSZ         31      /* File size limit exceeded.  */


#ifdef __KERNEL__

typedef unsigned int __sigset_t;        /* Blocked signals mask type.  */
typedef void (*__sighandler_t)(int);    /* Signal handler type.  */

#endif  /* __KERNEL__ */


/* Structure describing the action to be taken when a signal arrives.  */
struct sigaction
{
    __sighandler_t sa_handler;  /* Signal handler.  */
    __sigset_t     sa_mask;     /* Additional set of signals to be blocked.  */
    int            sa_flags;    /* Special flags.  */
};

/* Bits in `sa_flags'.  */
/*
#define SA_ONSTACK      0x1
#define SA_RESTART      0x2
*/
#define SA_NOCLDSTOP    0x8     /* Don't send SIGCHLD when children stop.  */


/* Values for the HOW argument to `sigprocmask'.  */
#define SIG_BLOCK       1       /* Block signals.  */
#define SIG_UNBLOCK     2       /* Unblock signals.  */
#define SIG_SETMASK     3       /* Set the set of blocked signals.  */


#define SIG_ERR         ((__sighandler_t)-1)
#define SIG_DFL         ((__sighandler_t)0)
#define SIG_IGN         ((__sighandler_t)1)


#define SIG_BLOCKABLE   (~((1 << SIGKILL) | (1 << SIGSTOP)))


#ifdef __KERNEL__

#define WNOHANG         1       /* Don't block waiting.  */
#define WUNTRACED       2       /* Report status of stopped children.  */

#define WAIT_ANY        (-1)    /* Wait any child process.  */
#define WAIT_MYGRP      0       /* Wait any child process in the same group
				   as the calling process.  */

#include <thix/types.h>

/* System internal definitions.  */
#define signals()       (this->sigpending & ~this->sigblocked)

/* System internal prototypes.  */
int __issig(void);
int sys_wait(int *wait_stat);
int sys_waitpid(__pid_t pid, int *wait_stat, int options);
void kill_kpid(int kpid, int signum);
int kill_pid(__pid_t pid, int signum);
int kill_pgrp(__pid_t pgrp, int signum, int kill_the_sender);
int kill_all(int signum);

#endif  /* __KERNEL__ */


#endif  /* _THIX_SIGNAL_H */
