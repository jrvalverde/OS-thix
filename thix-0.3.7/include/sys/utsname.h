#ifndef _SYS_UTSNAME_H
#define _SYS_UTSNAME_H


#ifndef _THIX_UTSNAME_H
#include <thix/utsname.h>
#endif


/* Put information about the system in NAME.  */
extern int uname __P ((struct utsname * __name));


#endif	/* sys/utsname.h */
