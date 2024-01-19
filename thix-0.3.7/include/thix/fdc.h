#ifndef _THIX_FDC_H
#define _THIX_FDC_H


#ifndef _THIX_BLKDRV_H
#include <thix/blkdrv.h>
#endif


#define FDC_MAJOR       1
#define FDC_IRQ         6


int  fdc_init(void);

int  fdc_open(int _minor, int _flags);
int  fdc_close(int _minor);
int  fdc_read(int _minor, blk_request *_br);
int  fdc_write(int _minor, blk_request *_br);
int  fdc_ioctl(int _minor, int _cmd, void *_arg);
int  fdc_lseek(int _minor, __off_t _offset);
void fdc_timer(void);


#endif  /* _THIX_FDC_H */
