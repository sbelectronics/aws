/* stdio.c - printf and friends, on the console in console.c */
#include <stdio.h>
#include <string.h>
#include "console.h"

struct FILE { int fd; };
static FILE fin = { 0 }, fout = { 1 }, ferr = { 2 };
FILE *stdin = &fin, *stdout = &fout, *stderr = &ferr;

int fputc(int c, FILE *f)
{
    (void)f;
    con_putc(c);
    if (c == '\n')
        con_flush();
    return (unsigned char)c;
}

int putchar(int c)
{
    return fputc(c, stdout);
}

int fputs(const char *s, FILE *f)
{
    (void)f;
    while (*s)
        con_putc(*s++);
    con_flush();
    return 0;
}

int puts(const char *s)
{
    fputs(s, stdout);
    return fputc('\n', stdout);
}

int fflush(FILE *f)
{
    (void)f;
    con_flush();
    return 0;
}

int fgetc(FILE *f)
{
    (void)f;
    return con_getc();
}

int getchar(void)
{
    return con_getc();
}

char *fgets(char *s, int n, FILE *f)
{
    int i = 0, c;

    while (i < n - 1) {
        c = fgetc(f);
        if (c == EOF)
            break;
        s[i++] = (char)c;
        if (c == '\n')
            break;
    }
    if (i == 0)
        return NULL;
    s[i] = 0;
    return s;
}

/* The formatter sends each character to o->put and counts them all, so
 * snprintf can return the length it would have written. */
struct out {
    void (*put)(int c, struct out *o);
    char *buf;
    size_t room;
    int count;
};

static void emit(struct out *o, int c)
{
    o->put(c, o);
    o->count++;
}

static void pad(struct out *o, int c, int n)
{
    while (n-- > 0)
        emit(o, c);
}

static void number(struct out *o, unsigned long u, int neg, int base, int upper,
                   int left, int zero, int plus, int space, int alt, int width, int prec)
{
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    char tmp[11], prefix[3];
    int len = 0, plen = 0, zeros, spaces, i;
    int nonzero = u != 0;

    while (u) {
        tmp[len++] = digits[(unsigned)(u % base)];
        u /= base;
    }
    if (len == 0 && prec != 0)
        tmp[len++] = '0';

    if (neg)
        prefix[plen++] = '-';
    else if (plus)
        prefix[plen++] = '+';
    else if (space)
        prefix[plen++] = ' ';
    if (alt && base == 16 && nonzero) {
        prefix[plen++] = '0';
        prefix[plen++] = upper ? 'X' : 'x';
    }
    if (alt && base == 8 && (len == 0 || tmp[len - 1] != '0') && prec <= len)
        prec = len + 1;

    zeros = prec > len ? prec - len : 0;
    if (prec < 0 && zero && !left && width > plen + len)
        zeros = width - plen - len;
    spaces = width - plen - zeros - len;

    if (!left)
        pad(o, ' ', spaces);
    for (i = 0; i < plen; i++)
        emit(o, prefix[i]);
    pad(o, '0', zeros);
    while (len > 0)
        emit(o, tmp[--len]);
    if (left)
        pad(o, ' ', spaces);
}

static int format(struct out *o, const char *fmt, va_list ap)
{
    const char *s;
    char c;
    unsigned long u;
    int left, zero, plus, space, alt, width, prec, longarg, n;

    for (; *fmt; fmt++) {
        if (*fmt != '%') {
            emit(o, *fmt);
            continue;
        }

        left = zero = plus = space = alt = 0;
        for (;; fmt++) {
            if (fmt[1] == '-')
                left = 1;
            else if (fmt[1] == '0')
                zero = 1;
            else if (fmt[1] == '+')
                plus = 1;
            else if (fmt[1] == ' ')
                space = 1;
            else if (fmt[1] == '#')
                alt = 1;
            else
                break;
        }
        fmt++;

        width = 0;
        if (*fmt == '*') {
            width = va_arg(ap, int);
            if (width < 0) {
                left = 1;
                width = -width;
            }
            fmt++;
        } else {
            while (*fmt >= '0' && *fmt <= '9')
                width = width * 10 + *fmt++ - '0';
        }

        prec = -1;
        if (*fmt == '.') {
            fmt++;
            prec = 0;
            if (*fmt == '*') {
                prec = va_arg(ap, int);
                fmt++;
            } else {
                while (*fmt >= '0' && *fmt <= '9')
                    prec = prec * 10 + *fmt++ - '0';
            }
        }

        longarg = *fmt == 'l';
        if (*fmt == 'l' || *fmt == 'h')
            fmt++;

        switch (*fmt) {
        case 'c':
            c = (char)va_arg(ap, int);
            s = &c;
            n = 1;
            goto string;
        case 's':
            s = va_arg(ap, const char *);
            if (s == NULL)
                s = "(null)";
            for (n = 0; s[n] && (prec < 0 || n < prec); n++)
                ;
        string:
            if (!left)
                pad(o, ' ', width - n);
            for (width -= n; n > 0; n--)
                emit(o, *s++);
            if (left)
                pad(o, ' ', width);
            break;
        case 'd':
        case 'i': {
            long v = longarg ? va_arg(ap, long) : va_arg(ap, int);
            number(o, v < 0 ? -(unsigned long)v : (unsigned long)v, v < 0, 10, 0,
                   left, zero, plus, space, alt, width, prec);
            break;
        }
        case 'u':
        case 'x':
        case 'X':
        case 'o':
            u = longarg ? va_arg(ap, unsigned long) : va_arg(ap, unsigned);
            number(o, u, 0, *fmt == 'u' ? 10 : *fmt == 'o' ? 8 : 16, *fmt == 'X',
                   left, zero, 0, 0, alt, width, prec);
            break;
        case 'p':
            number(o, (unsigned)va_arg(ap, void *), 0, 16, 1, left, 0, 0, 0, 0, width, 4);
            break;
        case '%':
            emit(o, '%');
            break;
        case 0:
            return o->count;
        default:
            emit(o, '%');
            emit(o, *fmt);
        }
    }
    return o->count;
}

static void put_console(int c, struct out *o)
{
    (void)o;
    con_putc(c);
}

static void put_buffer(int c, struct out *o)
{
    if (o->room) {
        *o->buf++ = (char)c;
        o->room--;
    }
}

int vfprintf(FILE *f, const char *fmt, va_list ap)
{
    struct out o;
    int n;

    (void)f;
    o.put = put_console;
    o.count = 0;
    n = format(&o, fmt, ap);
    con_flush();
    return n;
}

int vprintf(const char *fmt, va_list ap)
{
    return vfprintf(stdout, fmt, ap);
}

int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap)
{
    struct out o;
    int n;

    o.put = put_buffer;
    o.buf = buf;
    o.room = size ? size - 1 : 0;
    o.count = 0;
    n = format(&o, fmt, ap);
    if (size)
        *o.buf = 0;
    return n;
}

int vsprintf(char *buf, const char *fmt, va_list ap)
{
    return vsnprintf(buf, 0xFFFF, fmt, ap);
}

int printf(const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vfprintf(stdout, fmt, ap);
    va_end(ap);
    return n;
}

int fprintf(FILE *f, const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vfprintf(f, fmt, ap);
    va_end(ap);
    return n;
}

int sprintf(char *buf, const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsprintf(buf, fmt, ap);
    va_end(ap);
    return n;
}

int snprintf(char *buf, size_t size, const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return n;
}
