#ifndef _THIX_STAT_H
#define _THIX_STAT_H


#ifndef _THIX_TYPES_H
#include <thix/types.h>
#endif

#ifndef _THIX_STATBUF_H
#include <thix/statbuf.h>
#endif


/* Encoding of the file mode.  These are the standard Unix values,
   but POSIX.1 does not specify what values should be used.  */

#define S_IFSOCK __S_IFSOCK     /* socket */
#define S_IFLNK  __S_IFLNK      /* hard link */
#define S_IFREG  __S_IFREG      /* regular file */
#define S_IFBLK  __S_IFBLK      /* block device */
#define S_IFDIR  __S_IFDIR      /* directory */
#define S_IFCHR  __S_IFCHR      /* character device */
#define S_IFIFO  __S_IFIFO      /* pipe */

#define S_ISUID  __S_ISUID      /* setuid executable */
#define S_ISGID  __S_ISGID      /* set group id executable */
#define S_ISVTX  __S_ISVTX      /* sticky bit */

#define S_IRWXU  __S_IRWXU      /* read, write, execute by owner */
#define S_IRUSR  __S_IRUSR      /* read by owner */
#define S_IWUSR  __S_IWUSR      /* write by owner */
#define S_IXUSR  __S_IXUSR      /* execute by owner */

#define S_IRWXG  __S_IRWXG      /* read, write, execute by group */
#define S_IRGRP  __S_IRGRP      /* read by group */
#define S_IWGRP  __S_IWGRP      /* write by group */
#define S_IXGRP  __S_IXGRP      /* execute by group */

#define S_IRWXO  __S_IRWXO      /* read, write, execute by others */
#define S_IROTH  __S_IROTH      /* read by others */
#define S_IWOTH  __S_IWOTH      /* write by others */
#define S_IXOTH  __S_IXOTH      /* execute by others */

#define S_IREAD  __S_IRUSR      /* read by owner */
#define S_IWRITE __S_IWUSR      /* write by owner */
#define S_IEXEC  __S_IXUSR      /* execute by owner */

#define S_IFMT   __S_IFMT       /* mask to get file type from mode */

#define S_ISSOCK(mode)          (((mode) & S_IFMT) == S_IFSOCK)
#define S_ISLNK(mode)           (((mode) & S_IFMT) == S_IFLNK)
#define S_ISREG(mode)           (((mode) & S_IFMT) == S_IFREG)
#define S_ISBLK(mode)           (((mode) & S_IFMT) == S_IFBLK)
#define S_ISDIR(mode)           (((mode) & S_IFMT) == S_IFDIR)
#define S_ISCHR(mode)           (((mode) & S_IFMT) == S_IFCHR)
#define S_ISFIFO(mode)          (((mode) & S_IFMT) == S_IFIFO)


#endif  /* _THIX_STAT_H */
