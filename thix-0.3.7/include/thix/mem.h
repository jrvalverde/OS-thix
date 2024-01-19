#ifndef _THIX_MEM_H
#define _THIX_MEM_H


#ifndef _THIX_CHRDRV_H
#include <thix/chrdrv.h>
#endif


#define MEM_MAJOR       4


int mem_init(void);

int mem_open(int _minor, int _flags);
int mem_close(int _minor);
int mem_read(int _minor, chr_request *_cr);
int mem_write(int _minor, chr_request *_cr);
int mem_lseek(int _minor, __off_t _offset);


#endif  /* _THIX_MEM_H */
