#ifndef _THIX_NULL_H
#define _THIX_NULL_H


#ifndef _THIX_CHRDRV_H
#include <thix/chrdrv.h>
#endif


#define NULL_MAJOR      6


int null_init(void);

int null_open(int _minor, int _flags);
int null_close(int _minor);
int null_read(int _minor, chr_request *_cr);
int null_write(int _minor, chr_request *_cr);
int null_lseek(int _minor, __off_t _offset);


#endif  /* _THIX_NULL_H */
