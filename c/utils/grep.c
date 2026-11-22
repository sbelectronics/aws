/* grep - finds lines that match a pattern.
 *
 *   grep [-i] [-v] [-n] [-c] [-l] [-h] [-F] 'pattern' file... [>out]
 *
 * The pattern is a regular expression: . any character, [abc] [a-z] [^0-9]
 * a set, * + ? after one of those for any number, one or more, or none or
 * one, ^ and $ for the start and end of the line, \ before any of these
 * characters to mean itself. \s is a space or tab, \d a digit and \w a
 * letter, digit or _. -F takes the pattern as plain text instead.
 *
 * Put a pattern with a * or a space in single quotes: otherwise the
 * Executive takes * as a wildcard for file names, and splits at spaces.
 *
 * -i ignores case, -v shows the lines that don't match, -n numbers the
 * lines, -c only counts them, -l only lists the files with a match, -h
 * leaves out the file names when there are several files. */
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "util.h"

/* the command form, for tool_start and tools/mksub.py */
#define FORM "Find the lines of files that match a pattern|" \
    "Pattern|File list|[Ignore case?] -i|[Line numbers?] -n|" \
    "[Lines that do not match?] -v|[Count only?] -c|[File names only?] -l|" \
    "[Pattern is plain text?] -F|[Output file] >"

#define MAXLINE 1024

static int fold, invert, numbers, counts, names, nonames, fixed;
static char pat[256];
static int nfiles;
static long matches;

/* The character after the one at re: past a set or an escape. */
static const char *atom_end(const char *re)
{
    if (*re == '\\' && re[1])
        return re + 2;
    if (*re == '[') {
        re++;
        if (*re == '^')
            re++;
        if (*re == ']')
            re++;
        while (*re && *re != ']')
            re++;
        return *re ? re + 1 : re;
    }
    return re + 1;
}

static int atom_match(const char *re, int c)
{
    int neg, lo, hit = 0;

    if (*re == '.')
        return 1;
    if (*re == '\\') {
        switch (re[1]) {
        case 's': return c == ' ' || c == '\t';
        case 'd': return isdigit(c);
        case 'w': return isalnum(c) || c == '_';
        }
        return re[1] == c;
    }
    if (*re != '[')
        return *re == c;
    re++;
    if ((neg = *re == '^') != 0)
        re++;
    do {
        lo = (unsigned char)*re++;
        if (*re == '-' && re[1] && re[1] != ']') {
            if (c >= lo && c <= (unsigned char)re[1])
                hit = 1;
            re += 2;
        } else if (c == lo)
            hit = 1;
    } while (*re && *re != ']');
    return hit != neg;
}

/* Does re match at the start of s? */
static int here(const char *re, const char *s)
{
    const char *e, *t;
    char q;

    for (;;) {
        if (*re == 0)
            return 1;
        if (re[0] == '$' && re[1] == 0)
            return *s == 0;
        e = atom_end(re);
        q = *e;
        if (q == '?')
            return (*s && atom_match(re, (unsigned char)*s) && here(e + 1, s + 1)) ||
                   here(e + 1, s);
        if (q == '*' || q == '+') {
            /* as many as there are, then fewer until the rest matches */
            for (t = s; *t && atom_match(re, (unsigned char)*t); t++)
                ;
            for (; t >= s + (q == '+'); t--)
                if (here(e + 1, t))
                    return 1;
            return 0;
        }
        if (*s == 0 || !atom_match(re, (unsigned char)*s))
            return 0;
        re = e;
        s++;
    }
}

static int match(const char *s)
{
    if (fixed)
        return strstr(s, pat) != NULL;
    if (pat[0] == '^')
        return here(pat + 1, s);
    do
        if (here(pat, s))
            return 1;
    while (*s++);
    return 0;
}

static void lower(char *s)
{
    for (; *s; s++)
        *s = (char)tolower((unsigned char)*s);
}

static int search(const char *name, void *ctx)
{
    static char line[MAXLINE], folded[MAXLINE];
    FILE *f;
    long n = 0, hits = 0;

    (void)ctx;
    if ((f = fopen(name, "r")) == NULL) {
        warn(name, errno);
        return 0;
    }
    while (read_line(f, line, sizeof line) >= 0) {
        n++;
        if (fold) {
            strcpy(folded, line);
            lower(folded);
        }
        if (match(fold ? folded : line) == invert)
            continue;
        hits++;
        if (counts || names)
            continue;
        if (nfiles > 1 && !nonames)
            say("%s:", name);
        if (numbers)
            say("%ld:", n);
        say("%s\n", line);
    }
    fclose(f);
    if (counts) {
        if (nfiles > 1 && !nonames)
            say("%s:", name);
        say("%ld\n", hits);
    } else if (names && hits)
        say("%s\n", name);
    matches += hits;
    return 0;
}

int main(int argc, char **argv)
{
    int c, i;

    tool_start("grep", FORM, &argc, &argv);
    while ((c = getopt(argc, argv, "invclhF")) != -1) {
        switch (c) {
        case 'i': fold = 1; break;
        case 'n': numbers = 1; break;
        case 'v': invert = 1; break;
        case 'c': counts = 1; break;
        case 'l': names = 1; break;
        case 'h': nonames = 1; break;
        case 'F': fixed = 1; break;
        default: usage("[-invclhF] 'pattern' file...");
        }
    }
    if (argc - optind < 2)
        usage("[-invclhF] 'pattern' file...");
    if (strlen(argv[optind]) >= sizeof pat) {
        fprintf(stderr, "grep: the pattern is too long\n");
        return tool_end();
    }
    strcpy(pat, argv[optind++]);
    if (fold)
        lower(pat);
    /* file names go before the lines when there may be more than one file */
    nfiles = argc - optind;
    for (i = optind; i < argc; i++)
        if (has_wild(name_part(argv[i])))
            nfiles = 2;
    for (i = optind; i < argc; i++)
        each_file(argv[i], search, NULL);
    if (matches == 0)
        fprintf(stderr, "grep: no line matches\n");
    return tool_end();
}
