#ifndef _THIX_KLIB_H
#define _THIX_KLIB_H


#ifndef _THIX_DEBUG_H
#include <thix/debug.h>
#endif


#define min(a,b) ((a) <= (b) ? (a) : (b))
#define max(a,b) ((a) >= (b) ? (a) : (b))

#define min3(a,b,c) (min((a),min((b),(c))))
#define max3(a,b,c) (max((a),max((b),(c))))


char *utoa(unsigned __n, char *__dest, int __radix);
/*unsigned atoi(const char *__str);*/

void cursor_move(int __xy);

void kbd_wait(void);

void putch(char __c);
void puts(char *__string);
void printk(const char *__format, ...);
void panic_end(void);

#define PANIC(__format, __args...)                              \
{                                                               \
    printk("\nPANIC:%d:%s: ", __LINE__, __FUNCTION__);          \
    printk(__format, ## __args);                                \
    panic_end();                                                \
}

void io_delay(int __cnt);
void delay(int __msec);

/* This function makes a delay of the given number of miliseconds.
   It is precise and does *not* activates interrupts.  For an even
   preciser timing, use io_delay(), it gives an  aproximation of
   1/33000 seconds.  */
#define msec_delay(msec) io_delay(msec * 33)

/* This function makes a delay of the given number of seconds.  */
#define sec_delay(sec) io_delay(sec * 32813)


unsigned checksum(unsigned char *__data, int __count);


#endif  /* _THIX_KLIB_H */
