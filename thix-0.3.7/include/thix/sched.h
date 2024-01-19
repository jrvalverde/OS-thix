#ifndef _THIX_SCHED_H
#define _THIX_SCHED_H


#ifdef __KERNEL__

#define HZ                            100
#define TIMER_IRQ                       0

#endif  /* __KERNEL__ */


#define ITIMER_REAL     0
#define ITIMER_VIRTUAL  1
#define ITIMER_PROF     2


#define PRIO_MIN        (-19)   /* The minimum priority a process can have.  */
#define PRIO_MAX          19    /* The maximum priority a process can have.  */

#define PRIO_PROCESS    0       /* WHO is a process ID.  */
#define PRIO_PGRP       1       /* WHO is a process group ID.  */
#define PRIO_USER       2       /* WHO is a user ID.  */


#ifdef __KERNEL__

/*
 * There are 56 priority levels (from the kernel point of view):
 * - priority level 0 is used internally by the kernel
 * - user mode priorities are from 1 to 39
 * - interruptible kernel mode priorities are from 40 to 47
 * - uninterruptible kernel mode priorities are from 48 to 55
 */

#define I_THRESHOLD_PRIORITY    40 /* Interuptible threshold.  */
#define U_THRESHOLD_PRIORITY    48 /* Uninterruptible threshold.  */
#define MAX_PRIORITY_LEVEL      55 /* The maximum priority level.  */


/* Interruptible priority levels (40-47):  */

#define WAIT_PIPE               40
#define WAIT_SIGNAL             42
#define WAIT_CHILD_EXIT         43
#define WAIT_TTY_OUTPUT         45
#define WAIT_KLOG_INPUT         47
#define WAIT_TTY_INPUT          47


/* Uninterruptible priority levels (48-55):  */

#define WAIT_PAGE               48
#define WAIT_INODE              49
#define WAIT_SUPERBLOCK         50
#define WAIT_SYNC               51
#define WAIT_BUFFER             52
#define WAIT_DEVICE_BUSY        53
#define WAIT_DEVICE_IO          54
#define WAIT_SWAPPER            55


/* Process status.  */

#define PROCESS_UNUSED          0
#define PROCESS_RUNNING         1
#define PROCESS_STOPPED         2
#define PROCESS_ZOMBIE          3
#define PROCESS_ASLEEP          4


typedef struct
{
    unsigned char prev;
    unsigned char next;
    unsigned char priority;             /* The process's base priority.  */
    unsigned char cpu_usage;            /* Current process's ticks left.  */
} sched_struct;


extern sched_struct sched_data[];


typedef struct
{
    unsigned it_value;
    unsigned it_interval;
} _itimerval;


typedef struct tag_alarm_struct
{
    unsigned alarm_time;
    struct tag_alarm_struct *next;
} alarm_struct;


extern alarm_struct alarms[];


#endif  /* __KERNEL__ */


struct timeval
{
    long int tv_sec;
    long int tv_usec;
};


struct itimerval
{
    struct timeval it_interval;
    struct timeval it_value;
};


#ifdef __KERNEL__

extern unsigned boot_time;
extern unsigned volatile seconds;
extern unsigned volatile ticks;

extern int current_kpid;
extern int sched_touched;

inline void new_schedulable_process(int process, int priority);
inline int delete_schedulable_process(int process);
inline void restore_schedulable_process(int process, int priority);

void reset_alarm(int kpid);

int  scheduler(void);
void scheduler_init(void);
void scheduler_data_init(void);
void timer_init(void);
int  register_timer(void (*timer_fn)(void));
void idle(void);

#endif  /* __KERNEL__ */


#endif  /* _THIX_SCHED_H */
