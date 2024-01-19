#ifndef _SYS_SYSMACROS_H
#define _SYS_SYSMACROS_H


#define major(device) (((unsigned short) (device)) >>    8)
#define minor(device) (((unsigned short) (device))  & 0xFF)

#define makedev(major, minor) (((major) << 8) + (minor))


#endif	/* sys/sysmacros.h */
