#ifndef _THIX_KLOG_H
#define _THIX_KLOG_H


#ifndef _THIX_CHRDRV_H
#include <thix/chrdrv.h>
#endif


#define KLOG_MAJOR      9


int klog_init(void);

int klog_open(int _minor, int _flags);
int klog_close(int _minor);
int klog_read(int _minor, chr_request *_cr);
int klog_write(int _minor, chr_request *_cr);
int klog_fcntl(int _minor, int _flags);
int klog_lseek(int _minor, __off_t _offset);


#endif  /* _THIX_KLOG_H */
