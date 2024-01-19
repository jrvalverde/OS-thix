#ifndef _THIX_STDARG_H
#define _THIX_STDARG_H


typedef char *va_list;

#define va_start(ap, lastfix)  ap = (char *) &lastfix;

#define __va_rounded_size(TYPE)                                         \
  (((sizeof (TYPE) + sizeof (int) - 1) / sizeof (int)) * sizeof (int))

#define va_arg(AP, TYPE)                                                \
 (AP += __va_rounded_size (TYPE),                                       \
  *((TYPE *) AP))

#define va_end(ap)


#endif  /* _THIX_STDARG_H */
