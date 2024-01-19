#ifndef _THIX_CHRDRV_H
#define _THIX_CHRDRV_H


#ifndef _THIX_GENDRV_H
#include <thix/gendrv.h>
#endif


typedef struct
{
    char *buf;
    unsigned offset;
    int count;
} chr_request;


typedef struct
{
    char *name;
    int  (*open)(int, int);
    int  (*close)(int);
    int  (*read)(int, chr_request *);
    int  (*write)(int, chr_request *);
    void (*sync)(int);
    int  (*ioctl)(int, int, void *);
    int  (*fcntl)(int, int);
    int  (*lseek)(int, __off_t);
    void (*timer)(void);
    unsigned char major;
    unsigned char irq;
    unsigned char type;
    unsigned char minors;
    minor_info *minor;
} character_driver;


#endif  /* _THIX_CHRDRV_H */
