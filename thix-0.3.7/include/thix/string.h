#ifndef _THIX_STRING_H
#define _THIX_STRING_H


void *memcpy(void *__dest, const void *__src, unsigned int __size);


static inline void
lmemcpy(void *__dest, const void *__src, unsigned int __size)
{
    __asm__ __volatile__ ("movl %0,%%ecx
			   movl %1,%%esi
			   movl %2,%%edi
			   cld
			   rep
			   movsl"               :
			   /* no outputs */     :
			   "c" (__size),
			   "S" (__src),
			   "D" (__dest) :
			   "ecx","esi","edi","memory");
}


static inline void
memset(void *__dest, unsigned int __fill, unsigned int __size)
{
    __asm__ __volatile__ ("cld
			   rep
			   stosb"               :
			   /* no outputs */     :
			   "c" (__size),
			   "a" (__fill),
			   "D" (__dest) :
			   "ecx","eax","edi","memory");
}


static inline void
lmemset(void *__dest, unsigned int __fill, unsigned int __size)
{
    __asm__ __volatile__ ("cld
			   rep
			   stosl"               :
			   /* no outputs */     :
			   "c" (__size),
			   "a" (__fill),
			   "D" (__dest) :
			   "ecx","eax","edi","memory");
}


int memcmp(const void *__s1, const void *__s2, unsigned int __size);


unsigned int strlen(const char *__str);
char *strcpy(char *__dest, const char *__src);
char *strchr(char *__s, int __c);


#endif  /* _THIX_STRING_H */
