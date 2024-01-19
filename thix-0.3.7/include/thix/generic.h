#ifndef _THIX_GENERIC_H
#define _THIX_GENERIC_H


#ifndef _THIX_LIMITS_H
#include <thix/limits.h>
#endif


#define check_ufd(ufd)                                                  \
{                                                                       \
    if ((ufd) > OPEN_MAX || (ufd) < 0 || this->ufdt[ufd] == 0)          \
	return -EBADF;                                                  \
}                                                                       \


/*
 * File access modes for the second argument to
 * `sys_access' and `permitted'.
 */
#define R_OK    4               /* Test for read permission.  */
#define W_OK    2               /* Test for write permission.  */
#define X_OK    1               /* Test for execute permission.  */
#define F_OK    0               /* Test for existence.  */


#define SEEK_SET        0
#define SEEK_CUR        1
#define SEEK_END        2


typedef struct
{
    unsigned short inode;
    unsigned short flags;               /* How much do we need for this?  */
    unsigned       count;               /* Count needs only one byte.  */
    unsigned       offset;
} fd_struct;


extern fd_struct fdt[];


int  get_fd(void);
void release_fd(int fd);
int  get_ufd(int first);
void release_ufd(int fd);

int io_blocks(int offset, int count, int fsize, int blksz);

int permitted(int i_no, int mode, int ruid_flag);

inline int is_close_on_exec(int ufd);
inline void   close_on_exec(int ufd, int status);

int path_bad(char *path);

int sys_close(int fildes);


#endif  /* _THIX_GENERIC_H */
