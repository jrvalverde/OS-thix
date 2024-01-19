#ifndef _THIX_GENDRV_H
#define _THIX_GENDRV_H


#ifndef _THIX_FS_H
#include <thix/fs.h>
#endif

#ifndef _THIX_SEMAPHORE_H
#include <thix/semaphore.h>
#endif


#define MAX_DRIVERS     32      /* The maximum number of drivers in the
				   system.  */


#define BLK_DRV          1      /* Block driver.  */
#define CHR_DRV          2      /* Character driver.  */


typedef struct
{
    int in_use;                 /* In use flag.  */
    int mp;                     /* Index in mpt[].  */
    semaphore sync_mutex;       /* Semaphore used at sync time.  */
} minor_info;


#define major(device) (((unsigned short) (device)) >>    8)
#define minor(device) (((unsigned short) (device))  & 0xFF)
#define makedev(major, minor) (((major) << 8) + (minor))


minor_info *get_minor_info(__dev_t device);
void set_minor_info(__dev_t device, int mp);
int is_block_device(int major);
int get_minors(int major);
int check_device(__dev_t device);
int find_mp(int mp_i_no, int root_i_no);


#define mp(device)              (get_minor_info(device)->mp)
#define sb(device)              (mpt[mp(device)].sb)
#define device_in_use(device)   (get_minor_info(device)->in_use)

static inline int mounted(__dev_t device)
{
    minor_info *mi = get_minor_info(device);

    return mi->in_use && mpt[mi->mp].sb;
}


#define down_sync_mutex(device)                                         \
    (down(&(get_minor_info(device)->sync_mutex), WAIT_SYNC))
#define up_sync_mutex(device)                                           \
    (up(&(get_minor_info(device)->sync_mutex)))


typedef void *drv_request;


typedef struct
{
    char *name;
    int  (*open)(int, int);
    int  (*close)(int);
    int  (*read)(int, drv_request *);
    int  (*write)(int, drv_request *);
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
} generic_driver;


int  register_driver(generic_driver *gd);


int  drv_init(void);

int  drv_open(__dev_t _device, int _flags);
int  drv_close(__dev_t _device);

/* _dr = (blk_request*) or (chr_request*) */
int  drv_read(__dev_t _device, void *_dr);

/* _dr = (blk_request*) or (chr_request*) */
int  drv_write(__dev_t _device, void *_dr);

void drv_sync(__dev_t _device);
int  drv_ioctl(__dev_t _device, int _cmd, void *_argp);
int  drv_fcntl(__dev_t _device, int _flags);
int  drv_lseek(__dev_t _device, __off_t _offset);


#endif  /* _THIX_GENDRV_H */
