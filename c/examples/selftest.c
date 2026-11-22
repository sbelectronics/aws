/* selftest.c - checks the library on the machine itself: long arithmetic
 * (the helpers in i4.asm), printf formats, strtol, the string functions,
 * malloc and rand. Prints each failure, then a count; exits with the
 * number of failures. */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int checks, failures;

static void check(int ok, int line, const char *what)
{
    checks++;
    if (!ok) {
        failures++;
        printf("FAIL line %d: %s\n", line, what);
    }
}

#define CHECK(e) check((e) != 0, __LINE__, #e)

static void check_str(const char *got, const char *want, int line)
{
    char what[80];

    checks++;
    if (strcmp(got, want) != 0) {
        failures++;
        snprintf(what, sizeof what, "got \"%s\", want \"%s\"", got, want);
        printf("FAIL line %d: %s\n", line, what);
    }
}

#define FMT(want, ...) (sprintf(buf, __VA_ARGS__), check_str(buf, want, __LINE__))

/* volatile, so the compiler can't do the arithmetic itself */
static volatile long sa, sb;
static volatile unsigned long ua, ub;

static void arithmetic(void)
{
    sa = 123456L; sb = 789L;
    CHECK(sa * sb == 97406784L);
    sa = -1234L; sb = 5678L;
    CHECK(sa * sb == -7006652L);
    ua = 0xFFFFFFFFUL; ub = 3;
    CHECK(ua * ub == 0xFFFFFFFDUL);
    ua = 0x10000UL; ub = 0x10000UL;
    CHECK(ua * ub == 0);

    ua = 4000000000UL; ub = 7;
    CHECK(ua / ub == 571428571UL && ua % ub == 3);
    ua = 0xFFFFFFFFUL; ub = 0x10001UL;
    CHECK(ua / ub == 0xFFFF && ua % ub == 0);
    ua = 100000UL; ub = 100001UL;
    CHECK(ua / ub == 0 && ua % ub == 100000UL);

    sa = -1000000L; sb = 7;
    CHECK(sa / sb == -142857L && sa % sb == -1);
    sa = 1000000L; sb = -7;
    CHECK(sa / sb == -142857L && sa % sb == 1);
    sa = -1000000L; sb = -7;
    CHECK(sa / sb == 142857L && sa % sb == -1);
}

static void formats(void)
{
    char buf[64];

    FMT("-42", "%d", -42);
    FMT("65535", "%u", 65535u);
    FMT("beef BEEF", "%x %X", 0xBEEF, 0xBEEF);
    FMT("17", "%o", 15);
    FMT("-2147483648", "%ld", -2147483647L - 1);
    FMT("4294967295", "%lu", 4294967295UL);
    FMT("deadbeef", "%lx", 0xDEADBEEFUL);
    FMT("|    7|7    |00007|", "|%5d|%-5d|%05d|", 7, 7, 7);
    FMT("-0042", "%05d", -42);
    FMT("+5 5", "%+d %d", 5, 5);
    FMT(" 5", "% d", 5);
    FMT("  007", "%5.3d", 7);
    FMT("", "%.0d", 0);
    FMT("0x00beef", "%#08x", 0xBEEF);
    FMT("0", "%#x", 0);
    FMT("010 0", "%#o %#o", 8, 0);
    FMT("hi", "%c%c", 'h', 'i');
    FMT("ab    |", "%-6s|", "ab");
    FMT("ab", "%.2s", "abc");
    FMT("   9", "%*d", 4, 9);
    FMT("100%", "%d%%", 100);
    FMT("(null)", "%s", (char *)NULL);

    CHECK(snprintf(buf, 4, "hello") == 5 && strcmp(buf, "hel") == 0);
}

static void numbers(void)
{
    char *end;
    const char *s = "  -123abc", *bad = "xyz";

    CHECK(strtol(s, &end, 10) == -123 && *end == 'a');
    CHECK(strtol("0x1F", NULL, 0) == 31);
    CHECK(strtol("017", NULL, 0) == 15);
    CHECK(strtoul("4294967295", NULL, 10) == 4294967295UL);
    CHECK(strtol(bad, &end, 10) == 0 && end == bad);
    CHECK(atoi("  42") == 42 && atoi("-7") == -7);
    CHECK(abs(-5) == 5);
}

static void strings(void)
{
    char buf[32];

    CHECK(strlen("hello") == 5);
    CHECK(strcmp("abc", "abd") < 0 && strcmp("b", "a") > 0 && strcmp("x", "x") == 0);
    CHECK(strncmp("abcdef", "abcxyz", 3) == 0);
    strcpy(buf, "foo");
    strcat(buf, "bar");
    check_str(buf, "foobar", __LINE__);
    CHECK(strchr(buf, 'b') == buf + 3 && strrchr("abcabc", 'b') != NULL);
    CHECK(strstr(buf, "oba") == buf + 2 && strstr(buf, "xyz") == NULL);
    strcpy(buf, "123456789");
    memmove(buf + 2, buf, 5);
    check_str(buf, "121234589", __LINE__);
    memmove(buf, buf + 2, 5);
    check_str(buf, "123454589", __LINE__);
    CHECK(memcmp("abc", "abd", 3) < 0 && memchr("abc", 'c', 3) != NULL);
    CHECK(isdigit('5') && !isdigit('a') && isspace('\t') && toupper('a') == 'A');
}

static void memory(void)
{
    char *a, *b, *c;
    int i, ok;

    a = malloc(100);
    b = malloc(200);
    CHECK(a && b && (b >= a + 100 || a >= b + 200));
    memset(a, 'a', 100);
    memset(b, 'b', 200);
    free(a);
    c = malloc(50);
    CHECK(c == a);                      /* first fit reuses the freed block */
    for (ok = 1, i = 0; i < 200; i++)
        ok &= b[i] == 'b';
    CHECK(ok);
    c = realloc(c, 300);
    CHECK(c != NULL);
    free(b);
    free(c);
    a = calloc(10, 10);
    for (ok = a != NULL, i = 0; ok && i < 100; i++)
        ok &= a[i] == 0;
    CHECK(ok);
    free(a);
    CHECK(malloc(60000u) == NULL);
}

static void random(void)
{
    /* the C standard's sample generator gives these from seed 1 */
    srand(1);
    CHECK(rand() == 16838);
    CHECK(rand() == 5758);
}

int main(void)
{
    arithmetic();
    formats();
    numbers();
    strings();
    memory();
    random();
    printf("selftest: %d checks, %d failed\n", checks, failures);
    return failures;
}
