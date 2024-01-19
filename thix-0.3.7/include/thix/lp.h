#ifndef _THIX_LP_H
#define _THIX_LP_H


#ifndef _THIX_CHRDRV_H
#include <thix/chrdrv.h>
#endif


#define LP_MAJOR        8


int lp_init(void);

int lp_open(int _minor, int _flags);
int lp_close(int _minor);
int lp_read(int _minor, chr_request *_cr);
int lp_write(int _minor, chr_request *_cr);
int lp_ioctl(int _minor, int _cmd, void *_argp);
int lp_lseek(int _minor, __off_t _offset);


#endif  /* _THIX_LP_H */
