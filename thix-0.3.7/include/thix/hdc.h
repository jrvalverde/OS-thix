#ifndef _THIX_HDC_H
#define _THIX_HDC_H


#ifndef _THIX_BLKDRV_H
#include <thix/blkdrv.h>
#endif


/*
 * According to Tech Help 4.0...
 *
 * Note that Tech Help 4.0 contains a typo: 0x1F4 is for cylinder low
 * and 0x1F5 is for cylinder high.
 */


#define HDC_MAJOR        2
#define HDC_IRQ         14


#define HDC_CONTROL     0x3F6


int  hdc_init(void);

int  hdc_open(int _minor, int _flags);
int  hdc_close(int _minor);
int  hdc_read(int _minor, blk_request *_br);
int  hdc_write(int _minor, blk_request *_br);
int  hdc_ioctl(int _minor, int _cmd, void *_arg);
int  hdc_lseek(int _minor, __off_t _offset);
void hdc_timer(void);


#endif /* _THIX_HDC_H */
