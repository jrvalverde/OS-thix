#ifndef _THIX_ARCH_PC_CMOS_H
#define _THIX_ARCH_PC_CMOS_H


unsigned char read_cmos(unsigned char address);
void write_cmos(unsigned char address, unsigned char data);


#endif /* thix/arch/pc/cmos.h */
