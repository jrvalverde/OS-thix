#ifndef _DIRECT_H
#define _DIRECT_H


#define NAME_MAX        255


/* Directory entry structure.  */

struct direct
{
    __ino_t d_fileno;           /* File inode number.  */

    size_t d_namlen;            /* Length of the file name.  */

    /* Only this member is in the POSIX standard.  */
    char d_name[NAME_MAX + 1];  /* File name */
};


#undef  D_RECLEN
#define D_RECLEN(dp) sizeof(struct direct)

#undef  D_NAMLEN
#define D_NAMLEN(dp) ((dp)->d_namlen)


#endif  /* direct.h */
