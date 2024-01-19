#ifndef _THIX_PROC_I386_H
#define _THIX_PROC_I386_H


/*
 * Enable interrupts.
 */

static inline void
enable(void)
{
    __asm__ __volatile__("sti");
}


/*
 * Disable interrupts.
 */

static inline void
disable(void)
{
    __asm__ __volatile__("cli");
}


/*
 * Read a byte from a given port.
 */

static inline unsigned char
inb(unsigned short __port)
{
    unsigned char data;

    __asm__ __volatile__("inb %1,%0"            :
			 "=a" (data)            :
			 "d" (__port)           :
			 "eax","edx");

    return data;
}


/*
 * Output a byte to a given port.
 */

static inline void
outb(unsigned short __port, unsigned char __value)
{
    __asm__ __volatile__("outb %0,%1"           :
			 /* no output */        :
			 "a" (__value),
			 "d" (__port)           :
			 "eax","edx");
}


/*
 * Read a 16 bit word from a given port.
 */

static inline unsigned short
inw(unsigned short __port)
{
    unsigned short data;

    __asm__ __volatile__("inw %1,%0"            :
			 "=a" (data)            :
			 "d" (__port)           :
			 "eax","edx");

    return data;
}


/*
 * Read a 16 bit word from a given port and use outb to an unused port
 * (0xE0) as a delay.
 */

static inline unsigned short
inw_delay(unsigned short __port)
{
    unsigned short data;

    __asm__ __volatile__("inw %1,%0
			  outb %%al,$0xE0"      :
			 "=a" (data)            :
			 "d" (__port)           :
			 "eax","edx");

    return data;
}


/*
 * Read a 32 bit word from a given port.
 */

static inline unsigned long
inl(unsigned short __port)
{
    unsigned long data;

    __asm__ __volatile__("inl %1,%0"            :
			 "=a" (data)            :
			 "d" (__port)           :
			 "eax","edx");

    return data;
}


/*
 * Read a 32 bit word from a given port and use outb to an unused port
 * (0xE0) as a delay.
 */

static inline unsigned long
inl_delay(unsigned short __port)
{
    unsigned long data;

    __asm__ __volatile__("inl %1,%0
			  outb %%al,$0xE0"      :
			 "=a" (data)            :
			 "d" (__port)           :
			 "eax","edx");

    return data;
}


/*
 * Output a 16 bit word to a given port.
 */

static inline void
outw(unsigned short __port, unsigned short __value)
{
    __asm__ __volatile__("outw %0,%1"           :
			 /* no output */        :
			 "a" (__value),
			 "d" (__port)           :
			 "eax","edx");
}


/*
 * Output a 16 bit word to a given port and use outb to an unused port
 * (0xE0) as a delay.
 */

static inline void
outw_delay(unsigned short __port, unsigned short __value)
{
    __asm__ __volatile__("outw %0,%1
			  outb %%al,$0xE0"      :
			 /* no output */        :
			 "a" (__value),
			 "d" (__port)           :
			 "eax","edx");
}


/*
 * Output a 32 bit word to a given port.
 */

static inline void
outl(unsigned short __port, unsigned long __value)
{
    __asm__ __volatile__("outl %0,%1"           :
			 /* no output */        :
			 "a" (__value),
			 "d" (__port)           :
			 "eax","edx");
}


/*
 * Output a 32 bit word to a given port and use outb to an unused port
 * (0xE0) as a delay.
 */

static inline void
outl_delay(unsigned short __port, unsigned long __value)
{
    __asm__ __volatile__("outl %0,%1
			  outb %%al,$0xE0"      :
			 /* no output */        :
			 "a" (__value),
			 "d" (__port)           :
			 "eax","edx");
}


/*
 * Read a byte from a given port and use outb to an unused port
 * (0xE0) as a delay.
 */

static inline unsigned char
inb_delay(unsigned short __port)
{
    unsigned char data;

    __asm__ __volatile__("inb %1,%0
			  outb %%al,$0xE0"      :
			 "=a" (data)            :
			 "d" (__port)           :
			 "eax","edx");

    return data;
}


/*
 * Output a byte to a given port and use outb to an unused port
 * (0xE0) as a delay.
 */

static inline void
outb_delay(unsigned short __port, unsigned char __value)
{
    __asm__ __volatile__("outb %0,%1
			  outb %%al,$0xE0"      :
			 /* no output */        :
			 "a" (__value),
			 "d" (__port)           :
			 "eax","edx");
}


/*
 * Read a number of 16 bit words from a given port.
 */

static inline void
insw(unsigned short __port, unsigned short *__address,
			int __count)
{
    __asm__ __volatile__("cld
			  rep
			  insw"                 :
			 /* no output */        :
			 "c" (__count),
			 "D" (__address),
			 "d" (__port)           :
			 "ecx","edi","edx");
}


/*
 * Output a number of 16 bit words to a given port.
 */

static inline void
outsw(unsigned short __port, unsigned short *__address,
			 int __count)
{
    __asm__ __volatile__("cld
			  rep
			  outsw"                :
			 /* no output */        :
			 "c" (__count),
			 "S" (__address),
			 "d" (__port)           :
			 "ecx","esi","edx");
}


/*
 * Bit scan forward.  Returns the position of the first bit set in `data'.
 */

static inline int
bsf(unsigned data)
{
    int bit_no;

    __asm__ __volatile__("bsf %1,%0"   :
			 "=q" (bit_no) :
			 "rm" (data));

    return bit_no;
}


/*
 * Convert a BCD value to an int.
 */

static inline unsigned char
bcd2i(unsigned char bcd_value)
{
    return (bcd_value >> 4) * 10 + (bcd_value & 0x0F);
}


/*
 * Convert an int to a BCD value.
 */

static inline unsigned char
i2bcd(unsigned char int_value)
{
    return (((int_value - (int_value % 10)) / 10) << 4) + (int_value % 10);
}


void halt(void);

unsigned get_eip(void);
unsigned get_esp(void);
unsigned get_ebp(void);
unsigned get_ss(void);


void display_registers(unsigned cs,   unsigned ds,  unsigned es,
		       unsigned ss,   unsigned eax, unsigned ebx,
		       unsigned ecx,  unsigned edx, unsigned esi,
		       unsigned edi,  unsigned ebp, unsigned esp,
		       unsigned _esp, unsigned eip, unsigned eflags);

void display_exception_header(char *exception_name);


#endif /* _THIX_PROC_I386_H */
