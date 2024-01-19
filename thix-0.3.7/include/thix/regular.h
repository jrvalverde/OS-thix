#ifndef _THIX_REGULAR_H
#define _THIX_REGULAR_H


#ifndef _THIX_TYPES_H
#include <thix/types.h>
#endif


int regular_open(int fd);
int regular_close(int fd);

int regular_read(int fd, char *buf, size_t count);
int regular_write(int fd, char *buf, size_t count);


int a_out_read(int i_no, char *buf, __off_t offset, size_t size);


#endif  /* _THIX_REGULAR_H */
