#ifndef _THIX_PROCESS_H
#define _THIX_PROCESS_H


#include <thix/types.h>


#define superuser()     (this->euid == 0)


extern int processes;


__pid_t find_unused_pid();

int startinit(void);
int pick_zombie(void);
void release_zombie(int __zombie_kpid);
void sys_exit(int __exit_code, int __killed_by_signal);


#endif  /* _THIX_PROCESS_H */
