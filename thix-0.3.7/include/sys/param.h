#ifndef _GNU_SYS_PARAM_H
#define _GNU_SYS_PARAM_H 1
#include <endian.h>
#ifndef _SYS_PARAM_H
#define _SYS_PARAM_H


#include <limits.h>
#include <thix/param.h>


#define MAXPATHLEN      PATH_MAX
#define NOFILE          OPEN_MAX


#endif /* sys/param.h  */
#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 64 /* XXX */
#endif /* No MAXHOSTNAMELEN.  */
#endif	/* sys/param.h */
