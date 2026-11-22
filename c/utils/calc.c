/* calc - a programmer's calculator, on 32-bit integers.
 *
 *   calc 'expression'     shows the value
 *   calc                  asks for one expression after another, until an
 *                         empty line
 *
 * An expression with a * needs single quotes, or the Executive takes it as a
 * wildcard for file names.
 *
 * Numbers are decimal, or hex as 0x1F, 1Fh or $1F, octal as 0o17, binary as
 * 0b101, or a character as 'A'. The operators are C's, with C's precedence:
 * unary - ~ !, then * / %, + -, << >>, &, ^, |. Shifts are logical (the
 * value is taken as unsigned), and / and % are signed. A letter names a
 * variable, set by "x = expression", and _ is the last value. Each value is
 * shown in decimal (and unsigned, if it's negative), hex, octal, binary and
 * as a character. Hex with a letter first needs a 0 in front, 0FFh, since
 * FFh would be a name. */
#include <ctype.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include "util.h"

/* the command form, for tools/mksub.py */
#define FORM "A programmer's calculator|[Expression]"

static const char *src, *start;
static jmp_buf oops;
static const char *why;
static long vars[26], last;

static void fail(const char *msg)
{
    why = msg;
    longjmp(oops, 1);
}

static void space(void)
{
    while (*src == ' ' || *src == '\t')
        src++;
}

static int digit(int c, int base)
{
    int d = isdigit(c) ? c - '0' : isalpha(c) ? tolower(c) - 'a' + 10 : 99;

    return d < base ? d : -1;
}

static long digits(int base)
{
    unsigned long v = 0;
    int d, any = 0;

    while ((d = digit((unsigned char)*src, base)) >= 0) {
        v = v * base + d;
        src++;
        any = 1;
    }
    if (!any)
        fail("a number is missing its digits");
    return (long)v;
}

static long number(void)
{
    const char *p;
    long v;

    if (src[0] == '0' && (src[1] == 'x' || src[1] == 'X')) {
        src += 2;
        return digits(16);
    }
    if (src[0] == '0' && (src[1] == 'b' || src[1] == 'B') && digit((unsigned char)src[2], 2) >= 0) {
        src += 2;
        return digits(2);
    }
    if (src[0] == '0' && (src[1] == 'o' || src[1] == 'O')) {
        src += 2;
        return digits(8);
    }
    if (src[0] == '$') {
        src++;
        return digits(16);
    }
    /* 1Fh: hex digits up to an h */
    for (p = src; digit((unsigned char)*p, 16) >= 0; p++)
        ;
    if ((*p == 'h' || *p == 'H') && !isalnum((unsigned char)p[1])) {
        v = digits(16);
        src++;
        return v;
    }
    return digits(10);
}

static long expr(void);

static long primary(void)
{
    long v;

    space();
    if (*src == '(') {
        src++;
        v = expr();
        space();
        if (*src != ')')
            fail("a ( has no )");
        src++;
        return v;
    }
    if (*src == '\'' && src[1] && src[2] == '\'') {
        v = (unsigned char)src[1];
        src += 3;
        return v;
    }
    if (*src == '_') {
        src++;
        return last;
    }
    if (isdigit((unsigned char)*src) || *src == '$')
        return number();
    if (isalpha((unsigned char)*src) && !isalnum((unsigned char)src[1])) {
        /* A-F alone is a hex digit to the 1Fh rule, but here it's a variable */
        return vars[tolower((unsigned char)*src++) - 'a'];
    }
    if (*src == 0)
        fail("the expression ends too soon");
    fail("this isn't a number or an operator");
    return 0;
}

static long unary(void)
{
    space();
    if (*src == '-') {
        src++;
        return -unary();
    }
    if (*src == '+') {
        src++;
        return unary();
    }
    if (*src == '~') {
        src++;
        return ~unary();
    }
    if (*src == '!') {
        src++;
        return !unary();
    }
    return primary();
}

static long product(void)
{
    long v = unary(), w;
    char op;

    for (;;) {
        space();
        op = *src;
        if (op != '*' && op != '/' && op != '%')
            return v;
        src++;
        w = unary();
        if (op == '*')
            v *= w;
        else if (w == 0)
            fail("division by zero");
        else
            v = op == '/' ? v / w : v % w;
    }
}

static long sum(void)
{
    long v = product();

    for (;;) {
        space();
        if (*src == '+') {
            src++;
            v += product();
        } else if (*src == '-') {
            src++;
            v -= product();
        } else
            return v;
    }
}

static long shift(void)
{
    long v = sum(), n;
    int left;

    for (;;) {
        space();
        if ((src[0] != '<' && src[0] != '>') || src[1] != src[0])
            return v;
        left = src[0] == '<';
        src += 2;
        n = sum();
        if (n < 0 || n > 31)
            v = 0;
        else
            v = (long)(left ? (unsigned long)v << n : (unsigned long)v >> n);
    }
}

static long band(void)
{
    long v = shift();

    for (space(); *src == '&'; space()) {
        src++;
        v &= shift();
    }
    return v;
}

static long bxor(void)
{
    long v = band();

    for (space(); *src == '^'; space()) {
        src++;
        v ^= band();
    }
    return v;
}

static long expr(void)
{
    long v = bxor();

    for (space(); *src == '|'; space()) {
        src++;
        v |= bxor();
    }
    return v;
}

static void show(long v)
{
    char bin[40], *b = bin;
    unsigned long u = (unsigned long)v;
    int bits = u > 0xFFFFUL ? 32 : u > 0xFFUL ? 16 : 8, i;

    say("  %ld", v);
    if (v < 0)
        say(" (%lu)", u);
    say("   hex %lX   oct %lo", u, u);
    if (u >= 32 && u < 127)
        say("   '%c'", (int)u);
    for (i = bits - 1; i >= 0; i--) {
        *b++ = (char)('0' + ((u >> i) & 1));
        if (i && i % 4 == 0)
            *b++ = ' ';
    }
    *b = 0;
    say("\n  bin %s\n", bin);
}

static void calc(const char *line)
{
    int var = -1;
    long v;

    src = start = line;
    space();
    if (isalpha((unsigned char)src[0])) {
        const char *p = src + 1;
        while (*p == ' ')
            p++;
        if (*p == '=' && p[1] != '=') {
            var = tolower((unsigned char)src[0]) - 'a';
            src = p + 1;
        }
    }
    if (setjmp(oops)) {
        say("  ");
        for (; start < src; start++)
            say(" ");
        say("^ %s\n", why);
        return;
    }
    v = expr();
    space();
    if (*src)
        fail("this is left over");
    if (var >= 0)
        vars[var] = v;
    last = v;
    show(v);
}

int main(int argc, char **argv)
{
    char line[200];
    int i;

    prog = "calc";      /* not tool_start: >> is an operator, not output */
    if (argc > 1) {
        line[0] = 0;
        for (i = 1; i < argc && strlen(line) + strlen(argv[i]) + 2 < sizeof line; i++) {
            if (i > 1)
                strcat(line, " ");
            strcat(line, argv[i]);
        }
        say("%s\n", line);
        calc(line);
        return 0;
    }
    for (;;) {
        printf("calc> ");
        if (fgets(line, sizeof line, stdin) == NULL)
            break;
        if (strchr(line, '\n'))
            *strchr(line, '\n') = 0;
        if (line[0] == 0)
            break;
        calc(line);
    }
    return 0;
}
