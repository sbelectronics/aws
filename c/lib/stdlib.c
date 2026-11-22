#include <ctos.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "libc.h"

void exit(int status)
{
    _closeall();
    if (status)
        ErrorExit(status);
    Exit();
}

void abort(void)
{
    exit(EXIT_FAILURE);
}

int abs(int n)
{
    return n < 0 ? -n : n;
}

long labs(long n)
{
    return n < 0 ? -n : n;
}

div_t div(int num, int den)
{
    div_t r;

    r.quot = num / den;
    r.rem = num % den;
    return r;
}

ldiv_t ldiv(long num, long den)
{
    ldiv_t r;

    r.quot = num / den;
    r.rem = num % den;
    return r;
}

unsigned long strtoul(const char *s, char **end, int base)
{
    unsigned long v = 0;
    int neg = 0, d;
    const char *orig = s, *start;

    while (isspace(*s))
        s++;
    if (*s == '-' || *s == '+')
        neg = *s++ == '-';
    if ((base == 0 || base == 16) && s[0] == '0' && (s[1] | 0x20) == 'x') {
        s += 2;
        base = 16;
    } else if (base == 0) {
        base = *s == '0' ? 8 : 10;
    }
    start = s;
    for (;; s++) {
        if (isdigit(*s))
            d = *s - '0';
        else if (isalpha(*s))
            d = (*s | 0x20) - 'a' + 10;
        else
            break;
        if (d >= base)
            break;
        v = v * base + d;
    }
    if (end)
        *end = (char *)(s == start ? orig : s);
    return neg ? -v : v;
}

long strtol(const char *s, char **end, int base)
{
    return (long)strtoul(s, end, base);
}

int atoi(const char *s)
{
    return (int)strtol(s, NULL, 10);
}

long atol(const char *s)
{
    return strtol(s, NULL, 10);
}

/* rand: a 32-bit linear congruential generator; the top 15 bits are returned */
static unsigned long seed = 1;

int rand(void)
{
    seed = seed * 1103515245UL + 12345;
    return (int)(seed >> 16) & RAND_MAX;
}

void srand(unsigned s)
{
    seed = s;
}

/* malloc: first fit in a fixed heap. Each block starts with a header giving
 * its size in bytes (header included); the low bit marks it as in use. */
#ifndef HEAP_SIZE
#define HEAP_SIZE 16384
#endif

static unsigned heap[HEAP_SIZE / sizeof(unsigned)];
static int heap_ready;

#define HDR       sizeof(unsigned)
#define INUSE     1u
#define SIZE(p)   (*(p) & ~INUSE)
#define NEXT(p)   ((unsigned *)((char *)(p) + SIZE(p)))
#define HEAP_END  ((unsigned *)((char *)heap + sizeof heap))

void *malloc(size_t n)
{
    unsigned *p, need;

    if (!heap_ready) {
        heap[0] = sizeof heap;
        heap_ready = 1;
    }
    if (n == 0 || n > sizeof heap - HDR)
        return NULL;
    need = (n + HDR + 1) & ~1u;
    for (p = heap; p < HEAP_END; p = NEXT(p)) {
        if (*p & INUSE)
            continue;
        /* join the free blocks that follow */
        while (NEXT(p) < HEAP_END && !(*NEXT(p) & INUSE))
            *p += SIZE(NEXT(p));
        if (SIZE(p) >= need) {
            if (SIZE(p) - need >= HDR + 2) {
                unsigned *rest = (unsigned *)((char *)p + need);
                *rest = SIZE(p) - need;
                *p = need;
            }
            *p |= INUSE;
            return p + 1;
        }
    }
    return NULL;
}

void free(void *ptr)
{
    if (ptr)
        ((unsigned *)ptr)[-1] &= ~INUSE;
}

void *calloc(size_t n, size_t size)
{
    unsigned long total = (unsigned long)n * size;
    void *p;

    if (total > 0xFFFF)
        return NULL;
    p = malloc((size_t)total);
    if (p)
        memset(p, 0, (size_t)total);
    return p;
}

void *realloc(void *ptr, size_t n)
{
    void *q;
    size_t old;

    if (!ptr)
        return malloc(n);
    old = SIZE((unsigned *)ptr - 1) - HDR;
    if (n <= old)
        return ptr;
    q = malloc(n);
    if (q) {
        memcpy(q, ptr, old);
        free(ptr);
    }
    return q;
}
