/* scanf.c - the scanf family: %d %i %u %o %x %X %c %s %[...] %n %%, with
 * widths, * to skip an assignment, and h and l. No floating point. */
#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* Where the characters come from: a string or a FILE. scanf needs to put back
 * at most one character. */
struct in {
    const char *s;
    FILE *f;
    int count;          /* characters consumed, for %n */
};

static int get(struct in *in)
{
    int c;

    if (in->f)
        c = fgetc(in->f);
    else
        c = *in->s ? (unsigned char)*in->s++ : EOF;
    if (c != EOF)
        in->count++;
    return c;
}

static void unget(struct in *in, int c)
{
    if (c == EOF)
        return;
    in->count--;
    if (in->f)
        ungetc(c, in->f);
    else
        in->s--;
}

static int skip_space(struct in *in)
{
    int c;

    while ((c = get(in)) != EOF && isspace(c))
        ;
    unget(in, c);
    return c;
}

/* Is c in the scanset between set and end? "a-z" is a range; a '-' first or
 * last is itself. */
static int in_set(const char *set, const char *end, int c)
{
    for (; set < end; set++) {
        if (set + 2 < end && set[1] == '-') {
            if (c >= (unsigned char)set[0] && c <= (unsigned char)set[2])
                return 1;
            set += 2;
        } else if ((unsigned char)*set == c) {
            return 1;
        }
    }
    return 0;
}

static int digit(int c, int base)
{
    int d = isdigit(c) ? c - '0' : isalpha(c) ? (c | 0x20) - 'a' + 10 : 99;

    return d < base ? d : -1;
}

/* Reads a number of at most width characters. Returns 0 if there was none. */
static int number(struct in *in, int base, unsigned width, unsigned long *v, int *neg)
{
    int c, d, any = 0;

    *v = 0;
    *neg = 0;
    c = get(in);
    if ((c == '-' || c == '+') && width > 1) {
        *neg = c == '-';
        width--;
        c = get(in);
    }
    if (c == '0' && (base == 0 || base == 16) && width > 1) {
        any = 1;
        width--;
        c = get(in);
        if ((c | 0x20) == 'x' && width > 1) {
            base = 16;
            width--;
            c = get(in);
        } else if (base == 0) {
            base = 8;
        }
    }
    if (base == 0)
        base = 10;
    while (width > 0 && (d = digit(c, base)) >= 0) {
        *v = *v * base + d;
        any = 1;
        width--;
        c = get(in);
    }
    unget(in, c);
    return any;
}

static int scan(struct in *in, const char *fmt, va_list ap)
{
    int assigned = 0, suppress, size, c, neg, base, invert;
    unsigned width;
    unsigned long v;
    char *p;
    const char *set, *end;

    for (; *fmt; fmt++) {
        if (isspace(*fmt)) {
            skip_space(in);
            continue;
        }
        if (*fmt != '%' || fmt[1] == '%') {
            if (*fmt == '%')
                fmt++;
            c = get(in);
            if (c != (unsigned char)*fmt) {
                unget(in, c);
                return c == EOF && assigned == 0 ? EOF : assigned;
            }
            continue;
        }

        fmt++;
        suppress = *fmt == '*';
        if (suppress)
            fmt++;
        width = 0;
        while (isdigit(*fmt))
            width = width * 10 + *fmt++ - '0';
        size = 0;
        if (*fmt == 'h' || *fmt == 'l')
            size = *fmt++;

        switch (*fmt) {
        case 'n':
            if (!suppress)
                *va_arg(ap, int *) = in->count;
            continue;
        case 'c':
            if (width == 0)
                width = 1;
            p = suppress ? NULL : va_arg(ap, char *);
            while (width--) {
                if ((c = get(in)) == EOF)
                    return assigned ? assigned : EOF;
                if (p)
                    *p++ = (char)c;
            }
            break;
        case 's':
            if (skip_space(in) == EOF)
                return assigned ? assigned : EOF;
            p = suppress ? NULL : va_arg(ap, char *);
            if (width == 0)
                width = 0xFFFF;
            while (width-- && (c = get(in)) != EOF && !isspace(c))
                if (p)
                    *p++ = (char)c;
            if (c != EOF && isspace(c))
                unget(in, c);
            if (p)
                *p = 0;
            break;
        case '[':
            invert = *++fmt == '^';
            if (invert)
                fmt++;
            set = fmt;
            end = strchr(*fmt == ']' ? fmt + 1 : fmt, ']');
            if (end == NULL)
                return assigned;
            fmt = end;
            p = suppress ? NULL : va_arg(ap, char *);
            if (width == 0)
                width = 0xFFFF;
            {
                unsigned n = 0;

                while (width && (c = get(in)) != EOF && in_set(set, end, c) != invert) {
                    if (p)
                        p[n] = (char)c;
                    n++;
                    width--;
                }
                if (c != EOF && width)
                    unget(in, c);
                if (n == 0)
                    return c == EOF && assigned == 0 ? EOF : assigned;
                if (p)
                    p[n] = 0;
            }
            break;
        case 'd':
        case 'i':
        case 'u':
        case 'o':
        case 'x':
        case 'X':
            base = *fmt == 'o' ? 8 : *fmt == 'x' || *fmt == 'X' ? 16 : *fmt == 'i' ? 0 : 10;
            if (skip_space(in) == EOF)
                return assigned ? assigned : EOF;
            if (!number(in, base, width ? width : 0xFFFF, &v, &neg))
                return assigned;
            if (neg)
                v = -v;
            if (suppress)
                continue;
            if (size == 'l')
                *va_arg(ap, unsigned long *) = v;
            else if (size == 'h')
                *va_arg(ap, unsigned short *) = (unsigned short)v;
            else
                *va_arg(ap, unsigned *) = (unsigned)v;
            break;
        default:
            return assigned;
        }
        if (!suppress)
            assigned++;
    }
    return assigned;
}

int vsscanf(const char *s, const char *fmt, va_list ap)
{
    struct in in;

    in.s = s;
    in.f = NULL;
    in.count = 0;
    return scan(&in, fmt, ap);
}

int vfscanf(FILE *f, const char *fmt, va_list ap)
{
    struct in in;

    in.s = NULL;
    in.f = f;
    in.count = 0;
    return scan(&in, fmt, ap);
}

int sscanf(const char *s, const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsscanf(s, fmt, ap);
    va_end(ap);
    return n;
}

int fscanf(FILE *f, const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vfscanf(f, fmt, ap);
    va_end(ap);
    return n;
}

int scanf(const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vfscanf(stdin, fmt, ap);
    va_end(ap);
    return n;
}
