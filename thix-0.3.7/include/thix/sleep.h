#ifndef _THIX_SLEEP_H
#define _THIX_SLEEP_H


void sleep_init(void);

int  sleep(void *__address, int __priority);

void wakeup(void *__address);
void wakeup_process(int __process);
int  wakeup_one_process(void *__address);
void wakeup_and_kill(void *__address, int signum);


#endif  /* _THIX_SLEEP_H */
