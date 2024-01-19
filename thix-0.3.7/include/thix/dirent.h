#ifndef _THIX_DIRENT_H
#define _THIX_DIRENT_H


#ifndef _THIX_TYPES_H
#include <thix/types.h>
#endif

#ifndef _THIX_LIMITS_H
#include <thix/limits.h>
#endif


/* Directory entry structure.  */

struct dirent
{
    __ino_t d_fileno;           /* File inode number.  */

    size_t d_namlen;            /* Length of the file name.  */

    /* Only this member is in the POSIX standard.  */
    char d_name[NAME_MAX + 1];  /* File name.  */
};


#endif  /* _THIX_DIRENT_H */
