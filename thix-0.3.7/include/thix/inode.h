#ifndef _THIX_INODE_H
#define _THIX_INODE_H


#ifdef __KERNEL__

#ifndef _THIX_TYPES_H
#include <thix/types.h>
#endif

#ifndef _THIX_SLEEP_H
#include <thix/sleep.h>
#endif

#ifndef _THIX_SCHED_H
#include <thix/sched.h>
#endif

/* This should be bigger, or inodes should be dinamically allocated, as
   buffers are.  */
#define MAX_INODES		128

/* This should be moved in inode.c when the code will be cleaned and
   there will be no references to it from outside.  */
#define INODES_BUCKETS_NO	127

#define NO_INODE                 0
#define ROOT_INODE               1

#endif  /* __KERNEL__ */


#define DIRECT_BLOCKS           20
#define SINGLE_INDIRECT_BLOCK   20
#define DOUBLE_INDIRECT_BLOCK   21
#define TRIPLE_INDIRECT_BLOCK   22


typedef struct
{
    __mode_t       di_mode;     /* File mode.  */
    __nlink_t      di_nlinks;   /* Inode number of links.  */

    __uid_t        di_uid;      /* File owner id.  */
    __gid_t        di_gid;      /* File owner group.  */

    size_t         di_size;     /* File size.  */
    size_t         di_blocks;   /* Real number of blocks.  */

    unsigned char  di_addr[70]; /* Direct & indirect blocks map.  */

    unsigned short di_pipesize; /* Pipe size.  */

    __time_t       di_atime;    /* Last access time.  */
    __time_t       di_mtime;    /* Last modification time.  */
    __time_t       di_ctime;    /* Last status change time.  */

    __dev_t        di_rdev;     /* Special file major/minor numbers.  */

    unsigned short di_pipercnt; /* Pipe readers count.  */
    unsigned short di_pipewcnt; /* Pipe writers count.  */
    unsigned short di_piperoff; /* Pipe read  offset.  */
    unsigned short di_pipewoff; /* Pipe write offset.  */

    unsigned long  di_atime_usec;       /* Fractional part of di_atime.  */
    unsigned long  di_mtime_usec;       /* Fractional part of di_mtime.  */
    unsigned long  di_ctime_usec;       /* Fractional part of di_ctime.  */

    __time_t       di_ktime;    /* Unmodifiable last modified time.  */
} dinode_t;


#ifdef __KERNEL__

typedef struct
{
    __mode_t       i_mode;      /* File mode.  */
    __nlink_t      i_nlinks;    /* Inode number of links.  */

    __uid_t        i_uid;       /* File owner id.  */
    __gid_t        i_gid;       /* File owner group.  */

    size_t         i_size;      /* File size.  */
    size_t         i_blocks;    /* Real number of blocks.  */

    unsigned char  i_addr[70];  /* Direct & indirect blocks map.  */

    unsigned short i_pipesize;  /* Pipe size.  */

    __time_t       i_atime;     /* Last access time.  */
    __time_t       i_mtime;     /* Last modification time.  */
    __time_t       i_ctime;     /* Last status change time.  */

    __dev_t        i_rdev;      /* Special file major/minor numbers.  */

    unsigned short i_pipercnt;  /* Pipe readers count.  */
    unsigned short i_pipewcnt;  /* Pipe writers count.  */
    unsigned short i_piperoff;  /* Pipe read  offset.  */
    unsigned short i_pipewoff;  /* Pipe write offset.  */

    unsigned long  i_atime_usec;/* Fractional part of i_atime.  */
    unsigned long  i_mtime_usec;/* Fractional part of i_mtime.  */
    unsigned long  i_ctime_usec;/* Fractional part of i_ctime.  */

    __time_t       i_ktime;     /* Unmodifiable last modified time.  */

    unsigned char  busy         : 1,    /* Busy flag.  */
		   in_demand    : 1,    /* Access to the inode requested.  */
		   modified     : 1,    /* Modified flag.  */
		   mount_point  : 1,    /* The file is a mount point.  */
		   write_denied : 1,    /* Set when the file is currently
					   beeing executed.  */
		   exec_denied  : 1;    /* Set when the file is currently
					   open for writing.  */
    unsigned char  __fill;

    __dev_t        device;
    __nlink_t        nref;      /* Inode reference count.  */
    __ino_t        dinode;      /* Disk inode number.  */

    unsigned short hprev;
    unsigned short hnext;
    unsigned short lprev;
    unsigned short lnext;
} inode_t;


/*
 * This structure is named iblk_data (indirect block data) because we might
 * think to an inode as to an indirect block.
 */

typedef struct
{
    int buf_no;         /* If > 1 => the last indirect block buffer  */
			/* If 0   => use the inode instead */
			/* If 1   => an indirect block is missing */

    int index;          /* Index of the first block in the buffer */

    int nblocks;        /* The maximum number of blocks available via the
			   the indirect block or inode, starting  at  the
			   specified index.  */
} iblk_data;


extern inode_t *i_vect;


int  i_get(__dev_t device, __ino_t dinode);
void i_release(int i_no);
int  i_remove(int i_no);

int  i_alloc(__dev_t device);
void i_free(__dev_t device, __ino_t dinode);

void i_lock(int i_no);
void i_unlock(int i_no);


static inline void
i_set_atime(int i_no)
{
    i_vect[i_no].i_atime      = seconds;
    i_vect[i_no].i_atime_usec = (ticks % HZ) * (1000000 / HZ);
}


static inline void
i_set_mtime(int i_no)
{
    i_vect[i_no].i_mtime      = seconds;
    i_vect[i_no].i_mtime_usec = (ticks % HZ) * (1000000 / HZ);
}


static inline void
i_set_ctime(int i_no)
{
    i_vect[i_no].i_ctime      = seconds;
    i_vect[i_no].i_ctime_usec = (ticks % HZ) * (1000000 / HZ);
}


static inline void
i_set_time(int i_no)
{
    i_vect[i_no].i_atime =
    i_vect[i_no].i_mtime =
    i_vect[i_no].i_ctime = seconds;

    i_vect[i_no].i_atime_usec =
    i_vect[i_no].i_mtime_usec =
    i_vect[i_no].i_ctime_usec = (ticks % HZ) * (1000000 / HZ);
}


static inline void
i_duplicate(int i_no)
{
    i_vect[i_no].nref++;
}


static inline void
i_get_rid_of(int i_no)
{
    i_unlock(i_no);
    i_release(i_no);
}


static inline void
i_get_rid_of2(int dir_i_no, int i_no)
{
    i_unlock(dir_i_no);
    i_release(dir_i_no);
    i_unlock(i_no);
    i_release(i_no);
}


inline unsigned get_block_from_inode(int i_no, int index);
inline void put_block_to_inode(int i_no, int index, unsigned value);


void i_truncate(int i_no, unsigned offset);

unsigned *bmap(int i_no, unsigned file_offset,
	       iblk_data *ibd, unsigned *i_data);

int rbmap(int i_no, unsigned file_offset);
int wbmap(int i_no, unsigned file_offset);

int __namei(char *path, int *empty_dir_slot, int *dir_inode,
	   int *f_offset, char **f_name);
int namei(char *path);
int namei_with_dir(char *path, int *dir_i_no);

void i_init(void);
int  i_cleanup(void);


#endif  /* __KERNEL__ */


#endif  /* _THIX_INODE_H */
