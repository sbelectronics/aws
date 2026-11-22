/* diff - shows how two text files differ, or where two binary files do.
 *
 *   diff [-u] [-i] [-w] [-q] [-b] file1 file2 [>out]
 *
 * The output lists what changes file1 into file2: "3c3" (change), "5d4"
 * (delete), "7a8" (add), with "<" before file1's lines and ">" before
 * file2's. -u gives unified diffs instead, with three lines of context.
 * -i ignores case, -w ignores spaces and tabs, -q only says whether the files
 * differ. A file with a zero byte in its first 512 is compared as binary (-b
 * forces it): the offsets of the differing bytes, up to 20 of them.
 *
 * It finds the fewest changes with Myers' O(ND) algorithm, in its linear
 * space form, comparing lines by a 32-bit hash. The files may have 16000
 * lines between them. */
#include <ctype.h>
#include <errno.h>
#include <farmem.h>
#include <stdio.h>
#include <unistd.h>
#include "util.h"

/* the command form, for tool_start and tools/mksub.py */
#define FORM "Show how two files differ|" \
    "File 1|File 2|[Unified diff?] -u|[Ignore case?] -i|[Ignore spaces?] -w|" \
    "[Only say if they differ?] -q|[Compare as binary?] -b|[Output file] >"

#define MAXLINES 16000
#define MAXLINE  1024
#define CONTEXT  3

static int unified, fold, nospace, quiet, binary;
static const char *name1, *name2;

static unsigned long __far *hash1, __far *hash2;   /* each line's hash */
static unsigned char __far *del, __far *add;       /* the lines that change */
static int n1, n2;
static int __far *vf, __far *vb;                    /* the forward and backward paths */
static int vsize;

static unsigned long line_hash(const char *s)
{
    unsigned long h = 2166136261UL;     /* FNV-1a */
    int c;

    for (; *s; s++) {
        c = (unsigned char)*s;
        if (nospace && (c == ' ' || c == '\t'))
            continue;
        if (fold)
            c = tolower(c);
        h = (h ^ c) * 16777619UL;
    }
    return h;
}

/* Reads a file's lines into a new array of hashes. Returns the count, or -1. */
static int hash_file(const char *name, unsigned long __far **out)
{
    static char line[MAXLINE];
    unsigned long __far *h;
    FILE *f;
    int n = 0;

    if ((f = fopen(name, "r")) == NULL) {
        warn(name, errno);
        return -1;
    }
    if ((h = farmalloc(MAXLINES * sizeof *h)) == 0) {
        fprintf(stderr, "diff: not enough memory\n");
        fclose(f);
        return -1;
    }
    while (read_line(f, line, sizeof line) >= 0) {
        if (n == MAXLINES) {
            fprintf(stderr, "diff: %s has too many lines\n", name);
            fclose(f);
            return -1;
        }
        h[n++] = line_hash(line);
    }
    fclose(f);
    *out = h;
    return n;
}

static int wrap(int k)
{
    k %= vsize;
    return k < 0 ? k + vsize : k;
}

/* Marks the changes between lines a0.. (n of them) of file1 and b0.. (m)
 * of file2. Each call finds the middle snake, the run of equal lines in the
 * middle of a shortest edit path, and recurses on each side of it. */
static void compare(int a0, int n, int b0, int m)
{
    int h, r, k, x, y, s, t, z, d, L, w, Z, lo, hi;
    int __far *c, __far *o;

    while (n > 0 && m > 0 && hash1[a0] == hash2[b0]) {
        a0++, b0++;
        n--, m--;
    }
    while (n > 0 && m > 0 && hash1[a0 + n - 1] == hash2[b0 + m - 1])
        n--, m--;
    if (n == 0 || m == 0) {
        while (n > 0)
            del[a0 + --n] = 1;
        while (m > 0)
            add[b0 + --m] = 1;
        return;
    }
    L = n + m;
    w = n - m;
    Z = 2 * (n < m ? n : m) + 2;
    vsize = Z;
    for (k = 0; k < Z; k++)
        vf[k] = vb[k] = 0;
    for (h = 0; h <= L / 2 + L % 2; h++) {
        for (r = 0; r < 2; r++) {
            c = r == 0 ? vf : vb;
            o = r == 0 ? vb : vf;
            lo = -(h - 2 * (h > m ? h - m : 0));
            hi = h - 2 * (h > n ? h - n : 0);
            for (k = lo; k <= hi; k += 2) {
                if (k == -h || (k != h && c[wrap(k - 1)] < c[wrap(k + 1)]))
                    x = c[wrap(k + 1)];
                else
                    x = c[wrap(k - 1)] + 1;
                y = x - k;
                s = x;
                t = y;
                if (r == 0)
                    while (x < n && y < m && hash1[a0 + x] == hash2[b0 + y])
                        x++, y++;
                else
                    while (x < n && y < m && hash1[a0 + n - x - 1] == hash2[b0 + m - y - 1])
                        x++, y++;
                c[wrap(k)] = x;
                z = -(k - w);
                d = r == 0;     /* the paths can meet: forwards if L is odd */
                if (L % 2 == d && z >= -(h - d) && z <= h - d && x + o[wrap(z)] >= n) {
                    /* The snake runs from (s,t) to (x,y), going forwards; going
                     * backwards, those count from the far end. Both sides of it
                     * are done the same way. */
                    if (r == 0) {
                        compare(a0, s, b0, t);
                        compare(a0 + x, n - x, b0 + y, m - y);
                    } else {
                        compare(a0, n - x, b0, m - y);
                        compare(a0 + n - s, s, b0 + m - t, t);
                    }
                    return;
                }
            }
        }
    }
}

/* Reads a file's lines in order, for showing them. */
typedef struct {
    FILE *f;
    int next;               /* the number of the line in buf, plus 1 */
    char buf[MAXLINE];
} Reader;

static Reader r1, r2;

static const char *line_at(Reader *r, int n)
{
    while (r->next <= n) {
        if (read_line(r->f, r->buf, sizeof r->buf) < 0)
            r->buf[0] = 0;
        r->next++;
    }
    return r->buf;
}

static void show(Reader *r, int from, int to, const char *mark)
{
    for (; from < to; from++)
        say("%s%s\n", mark, line_at(r, from));
}

static void range(int from, int to)
{
    if (to - from > 1)
        say("%d,%d", from + 1, to);
    else
        say("%d", to > from ? from + 1 : from);
}

static void normal(void)
{
    int i = 0, j = 0, i2, j2;

    while (i < n1 || j < n2) {
        for (i2 = i; i2 < n1 && del[i2]; i2++)
            ;
        for (j2 = j; j2 < n2 && add[j2]; j2++)
            ;
        if (i2 == i && j2 == j) {
            i++, j++;
            continue;
        }
        range(i, i2);
        say("%c", i2 == i ? 'a' : j2 == j ? 'd' : 'c');
        range(j, j2);
        say("\n");
        show(&r1, i, i2, "< ");
        if (i2 > i && j2 > j)
            say("---\n");
        show(&r2, j, j2, "> ");
        i = i2;
        j = j2;
    }
}

/* Unified: changes closer than twice the context share a hunk. */
static void unified_diff(void)
{
    int i = 0, j = 0, i2, j2, hi, hj, ei, ej, gap;

    say("--- %s\n+++ %s\n", name1, name2);
    while (i < n1 || j < n2) {
        if (!(i < n1 && del[i]) && !(j < n2 && add[j])) {
            i++, j++;
            continue;
        }
        /* a hunk starts here: find where it ends */
        hi = i > CONTEXT ? i - CONTEXT : 0;
        hj = j - (i - hi);
        ei = i, ej = j;
        for (;;) {
            for (i2 = ei; i2 < n1 && del[i2]; i2++)
                ;
            for (j2 = ej; j2 < n2 && add[j2]; j2++)
                ;
            /* equal lines after this change */
            for (gap = 0; i2 + gap < n1 && j2 + gap < n2 && !del[i2 + gap] && !add[j2 + gap]; gap++)
                ;
            ei = i2, ej = j2;
            if (gap > 2 * CONTEXT || (i2 + gap >= n1 && j2 + gap >= n2))
                break;
            ei += gap, ej += gap;
        }
        gap = n1 - ei < CONTEXT ? n1 - ei : CONTEXT;
        if (n2 - ej < gap)
            gap = n2 - ej;
        say("@@ -%d,%d +%d,%d @@\n", hi + (ei + gap > hi), ei + gap - hi,
            hj + (ej + gap > hj), ej + gap - hj);
        /* walk the hunk */
        while (hi < ei || hj < ej) {
            if (hi < n1 && del[hi])
                show(&r1, hi, hi + 1, "-"), hi++;
            else if (hj < n2 && add[hj])
                show(&r2, hj, hj + 1, "+"), hj++;
            else {
                show(&r1, hi, hi + 1, " ");
                line_at(&r2, hj);
                hi++, hj++;
            }
        }
        show(&r1, ei, ei + gap, " ");
        i = ei + gap;
        j = ej + gap;
        line_at(&r2, j - 1);
    }
}

static int is_binary(const char *name)
{
    char buf[512];
    size_t n, i;
    FILE *f = fopen(name, "rb");

    if (f == NULL)
        return 0;
    n = fread(buf, 1, sizeof buf, f);
    fclose(f);
    for (i = 0; i < n; i++)
        if (buf[i] == 0)
            return 1;
    return 0;
}

static void binary_diff(void)
{
    FILE *a = fopen(name1, "rb"), *b = fopen(name2, "rb");
    long at = 0, count = 0;
    int c, d;

    if (a == NULL || b == NULL) {
        warn(a == NULL ? name1 : name2, errno);
        if (a)
            fclose(a);
        if (b)
            fclose(b);
        return;
    }
    for (;; at++) {
        c = getc(a);
        d = getc(b);
        if (c == EOF || d == EOF)
            break;
        if (c != d && ++count <= 20 && !quiet)
            say("%08lX: %02X %02X\n", at, c, d);
    }
    if (c != d && !quiet)
        say("%s is shorter\n", c == EOF ? name1 : name2);
    if (count > 20 && !quiet)
        say("%ld bytes differ\n", count);
    if ((count || c != d) && quiet)
        say("%s and %s differ\n", name1, name2);
    if (count == 0 && c == d)
        fprintf(stderr, "diff: the files are the same\n");
    fclose(a);
    fclose(b);
}

int main(int argc, char **argv)
{
    int c, i, changed = 0;

    tool_start("diff", FORM, &argc, &argv);
    while ((c = getopt(argc, argv, "uiwqb")) != -1) {
        switch (c) {
        case 'u': unified = 1; break;
        case 'i': fold = 1; break;
        case 'w': nospace = 1; break;
        case 'q': quiet = 1; break;
        case 'b': binary = 1; break;
        default: usage("[-uiwqb] file1 file2");
        }
    }
    if (argc - optind != 2)
        usage("[-uiwqb] file1 file2");
    name1 = argv[optind];
    name2 = argv[optind + 1];
    if (binary || is_binary(name1) || is_binary(name2)) {
        binary_diff();
        return tool_end();
    }
    if ((n1 = hash_file(name1, &hash1)) < 0 || (n2 = hash_file(name2, &hash2)) < 0)
        return tool_end();
    if (n1 + n2 > MAXLINES) {
        fprintf(stderr, "diff: the files have more than %d lines between them\n", MAXLINES);
        return tool_end();
    }
    del = farmalloc(n1 + 1);
    add = farmalloc(n2 + 1);
    vsize = 2 * (n1 < n2 ? n1 : n2) + 2;
    vf = farmalloc(vsize * sizeof *vf);
    vb = farmalloc(vsize * sizeof *vb);
    if (!del || !add || !vf || !vb) {
        fprintf(stderr, "diff: not enough memory\n");
        return tool_end();
    }
    _fmemset(del, 0, n1 + 1);
    _fmemset(add, 0, n2 + 1);
    compare(0, n1, 0, n2);
    for (i = 0; i < n1 && !changed; i++)
        changed = del[i];
    for (i = 0; i < n2 && !changed; i++)
        changed = add[i];
    if (!changed) {
        fprintf(stderr, "diff: the files are the same\n");
        return tool_end();
    }
    if (quiet) {
        say("%s and %s differ\n", name1, name2);
        return tool_end();
    }
    r1.f = fopen(name1, "r");
    r2.f = fopen(name2, "r");
    if (r1.f == NULL || r2.f == NULL) {
        warn(r1.f == NULL ? name1 : name2, errno);
        return tool_end();
    }
    if (unified)
        unified_diff();
    else
        normal();
    fclose(r1.f);
    fclose(r2.f);
    return tool_end();
}
