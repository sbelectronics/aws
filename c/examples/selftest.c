/* selftest.c - checks the library on the machine itself: long arithmetic
 * (the helpers in i4.asm), printf and scanf, strtol, the string functions,
 * malloc, qsort and bsearch, setjmp and longjmp, the date conversions, and
 * rand. Prints each failure, then a count; exits with the number of
 * failures. */
#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
    CHECK(atol("-100000") == -100000L);
    CHECK(abs(-5) == 5 && labs(-70000L) == 70000L);
    CHECK(div(-7, 2).quot == -3 && div(-7, 2).rem == -1);
    CHECK(ldiv(100000L, 7).quot == 14285L && ldiv(100000L, 7).rem == 5);
    CHECK(INT_MAX == 32767 && UINT_MAX == 65535U && LONG_MIN == -2147483647L - 1);
    CHECK((char)-1 == CHAR_MAX);
}

static void scanning(void)
{
    char s[16], t[16];
    int a = 0, b = 0, c = 0, d = 0, n = 0;
    long l = 0;
    unsigned u = 0;

    CHECK(sscanf("  42 -17 0x1f abc 077", "%d %i %i %s %o", &a, &b, &c, s, &d) == 5);
    CHECK(a == 42 && b == -17 && c == 31 && strcmp(s, "abc") == 0 && d == 63);
    CHECK(sscanf("12345", "%3d%d", &a, &b) == 2 && a == 123 && b == 45);
    CHECK(sscanf("abc123def", "%[a-z]%d%s", s, &a, t) == 3 && strcmp(s, "abc") == 0 &&
          a == 123 && strcmp(t, "def") == 0);
    CHECK(sscanf("x=10, y=20", "x=%d, y=%d%n", &a, &b, &n) == 2 && a == 10 && b == 20 && n == 10);
    CHECK(sscanf("1 2 3", "%*d %d", &a) == 1 && a == 2);
    CHECK(sscanf("-2147483648 ff", "%ld %x", &l, &u) == 2 && l == -2147483647L - 1 && u == 255);
    CHECK(sscanf("12 abc", "%d %d", &a, &b) == 1);
    CHECK(sscanf("", "%d", &a) == EOF);
    CHECK(sscanf("hi there", "%[^ ] %c", s, &t[0]) == 2 && strcmp(s, "hi") == 0 && t[0] == 't');
}

static int cmp_int(const void *a, const void *b)
{
    return *(const int *)a - *(const int *)b;
}

static int cmp_str(const void *a, const void *b)
{
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static void sorting(void)
{
    int v[50], i, ok, key;
    const char *w[] = { "pear", "apple", "fig", "kiwi", "banana" };

    for (i = 0; i < 50; i++)
        v[i] = (i * 37) % 50;               /* 0 to 49, shuffled */
    qsort(v, 50, sizeof v[0], cmp_int);
    for (ok = 1, i = 0; i < 50; i++)
        ok &= v[i] == i;
    CHECK(ok);
    key = 37;
    CHECK(bsearch(&key, v, 50, sizeof v[0], cmp_int) == &v[37]);
    key = 99;
    CHECK(bsearch(&key, v, 50, sizeof v[0], cmp_int) == NULL);
    qsort(w, 5, sizeof w[0], cmp_str);
    CHECK(strcmp(w[0], "apple") == 0 && strcmp(w[4], "pear") == 0);
}

static void tokens(void)
{
    char buf[32], *p;

    strcpy(buf, "  one, two;;three ");
    CHECK((p = strtok(buf, " ,;")) && strcmp(p, "one") == 0);
    CHECK((p = strtok(NULL, " ,;")) && strcmp(p, "two") == 0);
    CHECK((p = strtok(NULL, " ,;")) && strcmp(p, "three") == 0);
    CHECK(strtok(NULL, " ,;") == NULL);
    p = strdup("copy");
    CHECK(p && strcmp(p, "copy") == 0);
    free(p);
}

static jmp_buf jb;
static int unwound;

static void dive(int n)
{
    if (n == 0)
        longjmp(jb, unwound ? 0 : 42);
    dive(n - 1);
}

static void jumps(void)
{
    int r;

    unwound = 0;
    r = setjmp(jb);
    if (r == 0) {
        dive(5);
        CHECK(0);                           /* not reached */
    }
    CHECK(r == 42);
    unwound = 1;
    if ((r = setjmp(jb)) == 0)
        dive(3);
    CHECK(r == 1);                          /* longjmp with 0 gives 1 */
}

static void dates(void)
{
    struct tm tm, *g;
    time_t t = 1795257552L, z = 0;

    g = gmtime(&t);
    CHECK(g->tm_year == 126 && g->tm_mon == 10 && g->tm_mday == 21 && g->tm_hour == 10 &&
          g->tm_min == 39 && g->tm_sec == 12 && g->tm_wday == 6);
    check_str(asctime(g), "Sat Nov 21 10:39:12 2026\n", __LINE__);
    check_str(ctime(&z), "Thu Jan  1 00:00:00 1970\n", __LINE__);
    memset(&tm, 0, sizeof tm);
    tm.tm_year = 124;
    tm.tm_mon = 2;
    tm.tm_mday = 0;                         /* the day before 1 March 2024 */
    CHECK(mktime(&tm) == 1709251200L - 86400L && tm.tm_mon == 1 && tm.tm_mday == 29 &&
          tm.tm_yday == 59 && tm.tm_wday == 4);
    tm = *gmtime(&t);
    CHECK(mktime(&tm) == t);
    CHECK(time(NULL) > 0);
    assert(t > 0);
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
    scanning();
    strings();
    sorting();
    tokens();
    jumps();
    memory();
    dates();
    random();
    printf("selftest: %d checks, %d failed\n", checks, failures);
    return failures;
}
