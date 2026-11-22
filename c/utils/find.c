/* find - finds files by name, in every directory of a volume.
 *
 *   find [-l] 'pattern' [volume...] [>out]
 *
 * The pattern is a file name with * and ?, such as '*.c', or a directory
 * and a file name, such as '<Sys>*.run' or '<C*>*', in single quotes so that
 * the Executive doesn't expand it itself. The volume is like [d0]; without
 * one, it's the current volume. -l shows each file's size and when it was
 * last changed. It reads the volume's file headers rather than each
 * directory, which is much quicker. */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "util.h"

/* the command form, for tool_start and tools/mksub.py */
#define FORM "Find files by name in every directory of a volume|" \
    "File name pattern|[Volume list]|[Sizes and dates?] -l|[Output file] >"

static char dirpat[16], namepat[52];
static int lng;
static List found;
static const char *vol;
static long matched;

static int check(VolFile *f, void *ctx)
{
    char line[120], date[20];

    (void)ctx;
    if (f->first != f->header || !wild(namepat, f->name) || !wild(dirpat, f->dir))
        return 0;
    matched++;
    /* "\1" keeps the details out of the way of sorting by name */
    sprintf(line, "%s<%s>%s\1%10lu  %s", vol, f->dir, f->name, f->size,
            date_str(f->modified, date));
    if (list_add(&found, line)) {
        fprintf(stderr, "find: too many files to list\n");
        return 1;
    }
    return 0;
}

static int by_name(const char __far *a, const char __far *b)
{
    return far_cmp(a, b, 1);
}

int main(int argc, char **argv)
{
    char line[120], volbuf[20], *pat, *p;
    unsigned i;
    int c, erc, failed = 0;

    tool_start("find", FORM, &argc, &argv);
    while ((c = getopt(argc, argv, "l")) != -1) {
        if (c == 'l')
            lng = 1;
        else
            usage("[-l] 'pattern' [volume...]");
    }
    if (optind >= argc)
        usage("[-l] 'pattern' [volume...]");
    pat = argv[optind++];
    strcpy(dirpat, "*");
    if (pat[0] == '<' && (p = strchr(pat, '>')) != NULL && p - pat <= 13) {
        memcpy(dirpat, pat + 1, p - pat - 1);
        dirpat[p - pat - 1] = 0;
        pat = p + 1;
    }
    strncpy(namepat, pat[0] ? pat : "*", sizeof namepat - 1);
    if (list_init(&found, 16000)) {
        fprintf(stderr, "find: not enough memory\n");
        return tool_end();
    }
    do {
        vol = vol_of(optind < argc ? argv[optind] : "", volbuf);
        if ((erc = each_header(vol, check, NULL)) != 0) {
            warn(vol, erc);
            failed = 1;
        }
    } while (++optind < argc);
    list_sort(&found, by_name);
    for (i = 0; i < found.n; i++) {
        far_copy(line, found.item[i], sizeof line);
        p = strchr(line, '\1');
        *p++ = 0;
        if (lng)
            say("%s  %s\n", p, line);
        else
            say("%s\n", line);
    }
    if (matched == 0 && !failed)
        fprintf(stderr, "find: no file matches\n");
    list_free(&found);
    return tool_end();
}
