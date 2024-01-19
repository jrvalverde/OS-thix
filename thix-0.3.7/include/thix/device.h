#ifndef _THIX_DEVICE_H
#define _THIX_DEVICE_H


#ifndef _THIX_TYPES_H
#include <thix/types.h>
#endif


int can_close_device(__dev_t device, int current_fd);


int blk_open(int fd);
int blk_close(int fd);

int blk_read(int fd, char *buf, size_t count);
int blk_write(int fd, char *buf, size_t count);
int blk_ioctl(int fd, int cmd, void *argp);
int blk_lseek(int fd, __off_t offset);


int chr_open(int fd);
int chr_close(int fd);

int chr_read(int fd, char *buf, size_t count);
int chr_write(int fd, char *buf, size_t count);
int chr_ioctl(int fd, int cmd, void *argp);
int chr_lseek(int fd, __off_t offset);


#endif  /* _THIX_DEVICE_H */
