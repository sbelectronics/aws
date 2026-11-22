/* wc - counts the lines, words and bytes in files.
 *
 *   wc [-l] [-w] [-c] file... [>out]
 *
 * With no option it shows all three; with more than one file, the totals
 * too. A word is a run of characters other than spaces, tabs and ends of
 * line. */
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include "util.h"

/* the command form, for tool_start and tools/mksub.py */
#define FORM "Count the lines, words and bytes in files|" \
    "File list|[Lines only?] -l|[Words only?] -w|[Bytes only?] -c|" \
    "[Output file] >"

static int showLines, showWords, showBytes, nfiles;
static unsigned long tlines, twords, tbytes;

static void show(unsigned long l, unsigned long w, unsigned long b, const char *name)
{
    if (showLines)
        say("%8lu", l);
    if (showWords)
        say("%8lu", w);
    if (showBytes)
        say("%9lu", b);
    say("  %s\n", name);
}

static int count(const char *name, void *ctx)
{
    unsigned long l = 0, w = 0, b = 0;
    int c, inword = 0;
    FILE *f;

    (void)ctx;
    if ((f = fopen(name, "rb")) == NULL) {
        warn(name, errno);
        return 0;
    }
    while ((c = getc(f)) != EOF) {
        b++;
        if (c == '\n')
            l++;
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
            inword = 0;
        else if (!inword) {
            inword = 1;
            w++;
        }
    }
    fclose(f);
    show(l, w, b, name);
    tlines += l;
    twords += w;
    tbytes += b;
    nfiles++;
    return 0;
}

int main(int argc, char **argv)
{
    int c;

    tool_start("wc", FORM, &argc, &argv);
    while ((c = getopt(argc, argv, "lwc")) != -1) {
        switch (c) {
        case 'l': showLines = 1; break;
        case 'w': showWords = 1; break;
        case 'c': showBytes = 1; break;
        default: usage("[-lwc] file...");
        }
    }
    if (optind >= argc)
        usage("[-lwc] file...");
    if (!showLines && !showWords && !showBytes)
        showLines = showWords = showBytes = 1;
    for (; optind < argc; optind++)
        each_file(argv[optind], count, NULL);
    if (nfiles > 1)
        show(tlines, twords, tbytes, "total");
    return tool_end();
}
