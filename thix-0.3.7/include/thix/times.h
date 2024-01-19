#ifndef _THIX_TIMES_H
#define _THIX_TIMES_H


#ifndef _THIX_TYPES_H
#include <thix/types.h>
#endif


/* Structure describing CPU time used by a process and its children.  */
struct tms
{
    clock_t tms_utime;          /* User CPU time.  */
    clock_t tms_stime;          /* System CPU time.  */

    clock_t tms_cutime;         /* User CPU time of dead children.  */
    clock_t tms_cstime;         /* System CPU time of dead children.  */
};


#endif  /* _THIX_TIMES_H */
