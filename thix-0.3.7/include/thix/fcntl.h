#ifndef _THIX_FCNTL_H
#define _THIX_FCNTL_H


#ifndef _THIX_FCNTLBITS_H
#include <thix/fcntlbits.h>
#endif


/* File access modes for `open' and `fcntl'.  */
#define O_RDONLY        0x000   /* Open read-only.  */
#define O_WRONLY        0x001   /* Open write-only.  */
#define O_RDWR          0x002   /* Open read/write.  */

/* File status flags for `open' and `fcntl'.  */
#define O_APPEND        0x008   /* Writes append to the file.  */
#define O_NONBLOCK      0x080   /* Non-blocking I/O.  */
#define O_NDELAY        O_NONBLOCK

/* Bits OR'd into the second argument to open.  */
#define O_CREAT         0x100   /* Create file if it doesn't exist.  */
#define O_TRUNC         0x200   /* Truncate file to 0 length.  */
#define O_EXCL          0x400   /* Fail if file already exists.  */
#define O_NOCTTY        0x800   /* Don't assign a controlling terminal.  */

/* These are not implemented yet.  */
#define O_ASYNC         0x040   /* Send SIGIO to owner when data is ready.  */
#define O_FSYNC         0x010   /* Synchronous writes.  */
#define O_SYNC          O_FSYNC


#endif  /* _THIX_FCNTL_H */
