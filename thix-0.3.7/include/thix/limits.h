#ifndef _THIX_LIMITS_H
#define _THIX_LIMITS_H


/*
 * POSIX limits.
 */

/* argv + envp + misc.  */
#define ARG_MAX                65536

/* Maximum number processes having equal real user ID.  */
#define CHILD_MAX                 64

/* Maximum number of files that a process can have open simultaneously.  */
#define OPEN_MAX                 256

/* Maximum number of links  that a file can have.  */
#define LINK_MAX               65535

/* System limit for the amount of text in a line.  */
#define MAX_CANON                255

/* System limit for the total number of characters typed ahead as input.  */
#define MAX_INPUT                255

/* System limit for the length of a file name component.  */
#define NAME_MAX                 255

/* System limit for the length of an entire file name.  */
#define PATH_MAX                1024

/* System limit for the number of bytes that can be written atomically
   to a pipe.  */
#define PIPE_BUF                8192

/* Maximum number of streams that a process can have open simultaneously. */
#define STREAM_MAX               256

/* Maximum length of a time zone name.  */
/* #define TZNAME_MAX            256 */

/* Maximum number of supplememtary group IDs one process can have.  */
#define NGROUPS_MAX                0


#ifdef __KERNEL__

/*
 * Thix limits.
 */

/* Maximum number of processes.  */
#define SYSTEM_PROCESS_MAX        64

/* Maximum number of open files.  */
#define SYSTEM_OPEN_MAX         1024

/* Maximum number of mount points.  */
#define SYSTEM_MOUNT_MAX          32

/* Maximum number of swap devices.  */
#define SYSTEM_SWAP_MAX           16

/* Maximum number of blocks on a swap device.  */
#define SYSTEM_SWAPBLK_MAX     65536

/* Maximum size of a read/write request.  */
#define SYSTEM_REQUEST_MAX     65536

#endif  /* __KERNEL__ */


#endif  /* _THIX_LIMITS_H */
