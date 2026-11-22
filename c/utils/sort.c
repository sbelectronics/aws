/* sort - sorts the lines of text files.
 *
 *   sort [-r] [-n] [-f] [-u] [-k field] [-t c] file... [>out]
 *
 * -r reverses the order, -n compares numbers (a sign and digits at the start
 * of the key), -f ignores case, -u keeps one of each run of equal lines.
 * -k 2 sorts on the second field on, fields being separated by spaces and
 * tabs, or by the character given with -t. Lines with equal keys are put in
 * order by the whole line. The lines are kept in far memory, up to 16000 of
 * them, of up to 1023 characters. Since ">out" is only made after the
 * input is read, a file can be sorted into itself. */
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "util.h"

/* the command form, for tool_start and tools/mksub.py */
#define FORM "Sort the lines of files|" \
    "File list|[Reverse?] -r|[Numbers?] -n|[Ignore case?] -f|" \
    "[Drop repeats?] -u|[Sort on field] -k=|[Field separator] -t=|" \
    "[Output file] >"

#define MAXLINE 1024

static int reverse, numeric, fold, unique, field = 1, sep;
static List lines;

static int add(const char *name, void *ctx)
{
    static char line[MAXLINE];
    FILE *f;

    (void)ctx;
    if ((f = fopen(name, "r")) == NULL) {
        warn(name, errno);
        return 0;
    }
    while (read_line(f, line, sizeof line) >= 0) {
        if (list_add(&lines, line)) {
            fprintf(stderr, "sort: too many lines, or not enough memory\n");
            exit(tool_end());
        }
    }
    fclose(f);
    return 0;
}

static int blank(int c)
{
    return c == ' ' || c == '\t';
}

/* the start of the key: field N on */
static const char __far *key(const char __far *s)
{
    int n;

    for (n = 1; n < field && *s; n++) {
        if (sep) {
            while (*s && *s != sep)
                s++;
            if (*s)
                s++;
        } else {
            while (blank(*s))
                s++;
            while (*s && !blank(*s))
                s++;
        }
    }
    return s;
}

static long number(const char __far *s)
{
    long v = 0;
    int neg = 0;

    while (blank(*s))
        s++;
    if (*s == '-' || *s == '+')
        neg = *s++ == '-';
    while (isdigit((unsigned char)*s))
        v = v * 10 + (*s++ - '0');
    return neg ? -v : v;
}

static int key_cmp(const char __far *a, const char __far *b)
{
    long x, y;
    int r;

    a = key(a);
    b = key(b);
    if (numeric) {
        x = number(a);
        y = number(b);
        r = x < y ? -1 : x > y;
    } else
        r = far_cmp(a, b, fold);
    return r;
}

static int line_cmp(const char __far *a, const char __far *b)
{
    int r = key_cmp(a, b);

    if (r == 0)
        r = far_cmp(a, b, 0);
    return reverse ? -r : r;
}

int main(int argc, char **argv)
{
    static char line[MAXLINE];
    unsigned i;
    int c;

    tool_start("sort", FORM, &argc, &argv);
    while ((c = getopt(argc, argv, "rnfuk:t:")) != -1) {
        switch (c) {
        case 'r': reverse = 1; break;
        case 'n': numeric = 1; break;
        case 'f': fold = 1; break;
        case 'u': unique = 1; break;
        case 'k': field = atoi(optarg); break;
        case 't': sep = optarg[0]; break;
        default: usage("[-rnfu] [-k field] [-t c] file...");
        }
    }
    if (optind >= argc || field < 1)
        usage("[-rnfu] [-k field] [-t c] file...");
    if (list_init(&lines, 16000)) {
        fprintf(stderr, "sort: not enough memory\n");
        return tool_end();
    }
    for (; optind < argc; optind++)
        each_file(argv[optind], add, NULL);
    list_sort(&lines, line_cmp);
    for (i = 0; i < lines.n; i++) {
        if (unique && i > 0 && key_cmp(lines.item[i - 1], lines.item[i]) == 0)
            continue;
        far_copy(line, lines.item[i], sizeof line);
        say("%s\n", line);
    }
    list_free(&lines);
    return tool_end();
}
