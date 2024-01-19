#ifndef _THIX_PIPE_H
#define _THIX_PIPE_H


#ifndef _THIX_TYPES_H
#include <thix/types.h>
#endif


int pipe_open(int fd);
int pipe_close(int fd);

int pipe_read(int fd, char *buf, size_t count);
int pipe_write(int fd, char *buf, size_t count);


#endif  /* _THIX_PIPE_H */
