#ifndef _THIX_TYPES_H
#define _THIX_TYPES_H


#ifdef __KERNEL__

#define NULL (void *)0

typedef unsigned int size_t;
typedef long int clock_t;

typedef long int __time_t;
typedef unsigned short __dev_t;
typedef unsigned __ino_t;
typedef unsigned short __mode_t;
typedef unsigned short __nlink_t;
typedef long int __off_t;
typedef unsigned short __uid_t;
typedef unsigned short __gid_t;
typedef int __pid_t;

#endif  /* __KERNEL__ */


#endif  /* _THIX_TYPES_H */
