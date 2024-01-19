#ifndef _SYS_SYSTEM_H
#define _SYS_SYSTEM_H


#include <thix/system.h>


int getprocinfo(int iprocess, struct procinfo* pi);
int getsysinfo(struct sysinfo *si);


#endif /* sys/system.h */
