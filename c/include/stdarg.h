/* stdarg.h - variable arguments. Open Watcom passes all the arguments of a
 * variadic function on the stack, each a whole number of words. */
#ifndef STDARG_H
#define STDARG_H

typedef char *va_list;
#define va_start(ap, last) ((ap) = (char *)&(last) + ((sizeof(last) + 1) & ~1))
#define va_arg(ap, t)      ((ap) += (sizeof(t) + 1) & ~1, *(t *)((ap) - ((sizeof(t) + 1) & ~1)))
#define va_end(ap)         ((void)0)

#endif
