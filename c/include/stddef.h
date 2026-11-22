#ifndef STDDEF_H
#define STDDEF_H

typedef unsigned size_t;
typedef int ptrdiff_t;
#define NULL ((void *)0)
#define offsetof(t, m) ((size_t)&(((t *)0)->m))

#endif
