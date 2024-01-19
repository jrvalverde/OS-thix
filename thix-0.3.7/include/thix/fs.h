#ifndef _THIX_FS_H
#define _THIX_FS_H


#ifdef __KERNEL__

#ifndef _THIX_LIMITS_H
#include <thix/limits.h>
#endif

#ifndef _THIX_INODE_H
#include <thix/inode.h>
#endif

#ifndef _THIX_VMDEV_H
#include <thix/vmdev.h>
#endif

#endif  /* __KERNEL__ */


#define TFS_MAGIC_V01           0x54544655
#define TFS_MAGIC_V02           0x424F4245
#define TFS_VERSION             0
#define TFS_PATCHLEVEL          2


#define TFS_BOOT_BLOCK          0
#define TFS_SUPERBLOCK          1

#define TFS_MAX_BAD_BLOCKS      1024

#define TFS_BAD                 0
#define TFS_OK                  1

#define TFS_INODES_BITMAP       1
#define TFS_BLOCKS_BITMAP       2


/*
 * (R) means redundant. Such fields have been inserted into the superblock
 * structure for faster computation and better error detection at startup.
 */

typedef struct
{
    unsigned       s_fsize;     /* Total number of blocks in the fs.  */
    unsigned       s_bblsz;     /* Bad blocks list size (in blocks).  */
    unsigned       s_clusters;  /* Clusters in the file system.  */
    unsigned       s_icluster;  /* Inodes in a cluster.  */
    unsigned       s_bcluster;  /* Data blocks in a cluster. (R)  */
    unsigned       s_lcflag;    /* Last cluster status flag.  */
    unsigned       s_blcluster; /* Blocks in the last cluster.  */
    unsigned       s_bpcluster; /* Blocks per cluster. (R)  */
    unsigned       s_ublocks;   /* Unused blocks.  */

    unsigned       s_ifree;     /* Total free inodes.  */
    unsigned       s_bfree;     /* Total free blocks.  */

    unsigned       s_flock;     /* Lock used during file manip.  */
    unsigned       s_ilock;     /* Lock used during inodes manip.  */

    unsigned       s_fmod;      /* Superblock modified flag.  */
    unsigned       s_ronly;     /* Mounted read only flag.  */

    unsigned       s_time;      /* Last superblock update.  */
    unsigned       s_state;     /* File system state.  */
    unsigned       s_magic;     /* File system magic number.  */

    unsigned       s_blksz;     /* File system block size.  */
    unsigned       s_direntsz;  /* Directory entry size (power of 2).  */

    unsigned char  s_ftype[64]; /* File system type.  */
    unsigned char  s_fname[64]; /* File system name.  */

    unsigned       s_f_in_demand; /* Block access required.  */
    unsigned       s_i_in_demand; /* Inode access required.  */

    unsigned       s_ssbldsz;   /* Second stage boot loader size.  */

    unsigned char  s_fill[288]; /* Adjust this to reach 512 bytes.  */

    unsigned       s_checksum;  /* Superblock checksum (0-507).  */
} superblock;


struct statfs
{
    long f_bsize;               /* Block size.  */
    long f_blocks;              /* Total data blocks in file system.  */
    long f_bfree;               /* Free data blocks in file system.  */
    long f_bavail;              /* Data blocks available to non superuser.  */
    long f_files;               /* Total inodes in file system.  */
    long f_ffree;               /* Free inodes in file system.  */
    long f_namelen;             /* Maximum length of file names.  */
};


#ifdef __KERNEL__


typedef struct
{
    __dev_t device;             /* Device (major/minor).  */
    int fd;                     /* mp file descriptor.  */
    int vmdev;                  /* VM device number.  */
    superblock *sb;             /* Device superblock.  */
    int dir_i_no;               /* Mount point parent directory
				   inode.  */
    int mp_i_no;                /* Mount point directory inode.  */
    int root_i_no;              /* Root inode of the mounted fs.  */
    int icluster;               /* Cluster used for op. on inodes.  */
    int bcluster;               /* Cluster used for op. on blocks.  */
    int icount;                 /* Free inodes in ibitmap.  */
    int bcount;                 /* Free blocks in bbitmap.  */
    unsigned *ibitmap;          /* Current inodes bitmap.  */
    unsigned *bbitmap;          /* Current blocks bitmap.  */
} mp_struct;


extern mp_struct mpt[];


int  sys_sync(void);

void fs_init(void);
int  fs_cleanup(void);

void mount_init(void);

int  mount_root(void);
void umount_root(void);

#endif  /* __KERNEL__ */


#endif  /* _THIX_FS_H */
