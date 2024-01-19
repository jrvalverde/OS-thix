#ifndef _THIX_STATBUF_H
#define _THIX_STATBUF_H


/* This should be defined if there is a `st_blksize' member.  */
#define _STATBUF_ST_BLKSIZE     1

/* This structure needs to be defined in accordance with the
   implementation of __stat, __fstat, and __lstat.  */

/* Structure describing file characteristics.  */
struct stat
{
    /* These are the members that POSIX.1 requires.  */

    __mode_t  st_mode;          /* File mode.  */
    __dev_t   st_dev;           /* Device containing the file.  */
    __dev_t   st_rdev;          /* Major and minor device numbers.  */
    __nlink_t st_nlink;         /* Link count.  */
    __ino_t   st_ino;           /* File serial number.  */

    __uid_t   st_uid;           /* User ID of the file's owner.  */
    __gid_t   st_gid;           /* Group ID of the file's group.  */
    __off_t   st_size;          /* Size of file, in bytes.  */

    __time_t  st_atime;         /* Time of last access.  */
    __time_t  st_mtime;         /* Time of last modification.  */
    __time_t  st_ctime;         /* Time of last status change.  */

    unsigned long st_atime_usec; /* Fractional part of st_atime.  */
    unsigned long st_mtime_usec; /* Fractional part of st_mtime.  */
    unsigned long st_ctime_usec; /* Fractional part of st_ctime.  */

    unsigned int  st_blocks;    /* Disk space used in 512-byte blocks.  */
    unsigned int  st_blksize;   /* Optimal block size for reading/writing.  */
};


/* Encoding of the file mode.  These are the standard Unix values,
   but POSIX.1 does not specify what values should be used.  */

#define __S_IFSOCK      0140000         /* socket */
#define __S_IFLNK       0120000         /* hard link */
#define __S_IFREG       0100000         /* regular file */
#define __S_IFBLK       0060000         /* block device */
#define __S_IFDIR       0040000         /* directory */
#define __S_IFCHR       0020000         /* character device */
#define __S_IFIFO       0010000         /* pipe */

/* Protection bits.  */

#define __S_ISUID       0004000         /* setuid executable */
#define __S_ISGID       0002000         /* set group id executable */
#define __S_ISVTX       0001000         /* sticky bit */

#define __S_IRWXU       0000700         /* read, write, execute by owner */
#define __S_IRUSR       0000400         /* read by owner */
#define __S_IWUSR       0000200         /* write by owner */
#define __S_IXUSR       0000100         /* execute by owner */

#define __S_IRWXG       0000070         /* read, write, execute by group */
#define __S_IRGRP       0000040         /* read by group */
#define __S_IWGRP       0000020         /* write by group */
#define __S_IXGRP       0000010         /* execute by group */

#define __S_IRWXO       0000007         /* read, write, execute by others */
#define __S_IROTH       0000004         /* read by others */
#define __S_IWOTH       0000002         /* write by others */
#define __S_IXOTH       0000001         /* execute by others */

#define __S_IREAD       __S_IRUSR       /* read by owner */
#define __S_IWRITE      __S_IWUSR       /* write by owner */
#define __S_IEXEC       __S_IXUSR       /* execute by owner */

#define __S_IFMT        0170000         /* These bits determine file type.  */


#endif  /* _THIX_STATBUF_H */
