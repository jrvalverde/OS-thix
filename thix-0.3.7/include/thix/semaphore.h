#ifndef _THIX_SEMAPHORE_H
#define _THIX_SEMAPHORE_H


typedef int semaphore;


void down(semaphore *__sem, int __wakeup_priority);
void up(semaphore *__sem);


#endif /* _THIX_SEMAPHORE_H */
