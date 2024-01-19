#ifndef _THIX_VTTY_H
#define _THIX_VTTY_H


#ifndef _THIX_LIMITS_H
#include <thix/limits.h>
#endif

#ifndef _THIX_CHRDRV_H
#include <thix/chrdrv.h>
#endif

#ifndef _THIX_TERMIOS_H
#include <thix/termios.h>
#endif


#define VTTY_MAJOR      3
#define VTTY_IRQ        1


int vtty_init(void);

int vtty_open(int _minor, int _flags);
int vtty_close(int _minor);
int vtty_read(int _minor, chr_request *_cr);
int vtty_write(int _minor, chr_request *_cr);
int vtty_ioctl(int _minor, int _cmd, void *_argp);
int vtty_fcntl(int _minor, int _flags);
int vtty_lseek(int _minor, __off_t _offset);
int vtty_discard_input(int _minor);
int vtty_getattr(int _minor, struct termios *_termios_p);
int vtty_setattr(int _minor, struct termios *_termios_p);


void vtty_beep(void);


#endif  /* _THIX_VTTY_H */
