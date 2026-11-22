#include <stdlib.h>
#include <string.h>

void *memcpy(void *d, const void *s, size_t n)
{
    char *dp = d;
    const char *sp = s;

    while (n--)
        *dp++ = *sp++;
    return d;
}

void *memmove(void *d, const void *s, size_t n)
{
    char *dp = d;
    const char *sp = s;

    if (dp < sp) {
        while (n--)
            *dp++ = *sp++;
    } else {
        dp += n;
        sp += n;
        while (n--)
            *--dp = *--sp;
    }
    return d;
}

void *memset(void *d, int c, size_t n)
{
    char *dp = d;

    while (n--)
        *dp++ = (char)c;
    return d;
}

int memcmp(const void *a, const void *b, size_t n)
{
    const unsigned char *p = a, *q = b;

    for (; n; n--, p++, q++)
        if (*p != *q)
            return *p - *q;
    return 0;
}

void *memchr(const void *s, int c, size_t n)
{
    const unsigned char *p = s;

    for (; n; n--, p++)
        if (*p == (unsigned char)c)
            return (void *)p;
    return NULL;
}

size_t strlen(const char *s)
{
    const char *p = s;

    while (*p)
        p++;
    return p - s;
}

char *strcpy(char *d, const char *s)
{
    char *r = d;

    while ((*d++ = *s++) != 0)
        ;
    return r;
}

char *strncpy(char *d, const char *s, size_t n)
{
    char *r = d;

    for (; n && *s; n--)
        *d++ = *s++;
    for (; n; n--)
        *d++ = 0;
    return r;
}

char *strcat(char *d, const char *s)
{
    strcpy(d + strlen(d), s);
    return d;
}

char *strncat(char *d, const char *s, size_t n)
{
    char *p = d + strlen(d);

    for (; n && *s; n--)
        *p++ = *s++;
    *p = 0;
    return d;
}

int strcmp(const char *a, const char *b)
{
    for (; *a && *a == *b; a++, b++)
        ;
    return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n)
{
    for (; n && *a && *a == *b; n--, a++, b++)
        ;
    return n ? (unsigned char)*a - (unsigned char)*b : 0;
}

char *strchr(const char *s, int c)
{
    for (; *s; s++)
        if (*s == (char)c)
            return (char *)s;
    return c ? NULL : (char *)s;
}

char *strrchr(const char *s, int c)
{
    const char *r = NULL;

    for (; *s; s++)
        if (*s == (char)c)
            r = s;
    return c ? (char *)r : (char *)s;
}

char *strstr(const char *s, const char *t)
{
    size_t n = strlen(t);

    for (; *s; s++)
        if (strncmp(s, t, n) == 0)
            return (char *)s;
    return n ? NULL : (char *)s;
}

char *strtok(char *s, const char *delims)
{
    static char *next;
    char *tok;

    if (s == NULL)
        s = next;
    if (s == NULL)
        return NULL;
    while (*s && strchr(delims, *s))
        s++;
    if (*s == 0) {
        next = NULL;
        return NULL;
    }
    tok = s;
    while (*s && !strchr(delims, *s))
        s++;
    if (*s)
        *s++ = 0;
    next = s;
    return tok;
}

char *strdup(const char *s)
{
    char *d = malloc(strlen(s) + 1);

    return d ? strcpy(d, s) : NULL;
}
