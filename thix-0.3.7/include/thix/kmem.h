#ifndef _THIX_KMEM_H
#define _THIX_KMEM_H


#ifndef _THIX_CHRDRV_H
#include <thix/chrdrv.h>
#endif


#define KMEM_MAJOR      5


int kmem_init(void);

int kmem_open(int _minor, int _flags);
int kmem_close(int _minor);
int kmem_read(int _minor, chr_request *_cr);
int kmem_write(int _minor, chr_request *_cr);
int kmem_lseek(int _minor, __off_t _offset);


#endif  /* _THIX_KMEM_H */
