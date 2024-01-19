#ifndef _JMP_BUF_H
#define _JMP_BUF_H


/* Define the machine-dependent type `jmp_buf'.  Intel 386 version.  */

typedef struct
  {
    long int __bx, __si, __di;
    __ptr_t __bp;
    __ptr_t __sp;
    __ptr_t __pc;
  } __jmp_buf[1];


#endif /* jmp_buf.h */
