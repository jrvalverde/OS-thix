#ifndef _THIX_UTIME_H
#define _THIX_UTIME_H


#ifndef _THIX_TYPES_H
#include <thix/types.h>
#endif


/* Structure describing file times.  */
struct utimbuf
{
    __time_t actime;    /* Access time.  */
    __time_t modtime;   /* Modification time.  */
};


#endif  /* _THIX_UTIME_H */
