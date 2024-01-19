#ifndef _THIX_BLKDRV_H
#define _THIX_BLKDRV_H


#ifndef _THIX_GENDRV_H
#include <thix/gendrv.h>
#endif


typedef struct tag_br
{
/*  int kid;    */              /* Kernel ID of the process.  */
    int blksz;                  /* File system block size.  */
    int block;                  /* First block. -1 means unused request.  */
    int nblocks;                /* Number of blocks.  */
    int buf_no;                 /* A list of buffers to receive data.  */
    struct tag_br *next;        /* Next request in the request list.  */
} blk_request;


typedef struct
{
    char *name;
    int  (*open)(int, int);
    int  (*close)(int);
    int  (*read)(int, blk_request *);
    int  (*write)(int, blk_request *);
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
} block_driver;


#endif  /* _THIX_BLKDRV_H */
