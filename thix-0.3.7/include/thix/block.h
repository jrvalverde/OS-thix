#ifndef _THIX_BLOCK_H
#define _THIX_BLOCK_H


#ifndef _THIX_TYPES_H
#include <thix/types.h>
#endif


int  b_alloc(__dev_t device);
void b_free (__dev_t device, int b_no);


#endif  /* _THIX_BLOCK_H */
