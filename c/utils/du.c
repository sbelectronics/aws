/* du - how much of a volume each directory uses.
 *
 *   du [volume] [>out]          each directory, then the volume's totals
 *   du [vol]<dir> [>out]        each file in the directory, biggest first
 *
 * Without a volume it's the current one. Sizes are in KB of disk allocated,
 * which is in whole pages and often more than the file holds; the files'
 * own sizes are shown in bytes. Like find, it reads the volume's file
 * headers, and its Master File Directory for the directories with nothing
 * in them. */
#include <farmem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"

/* the command form, for tool_start and tools/mksub.py */
#define FORM "Show the space each directory of a volume uses|" \
    "[Volume or directory]|[Output file] >"

#define MAXDIRS  250
#define MAXFILES 1000
#define MAXEXT   200
#define KB(n) (((n) + 1023) / 1024)

static struct {
    char name[13];
    unsigned files;
    unsigned long bytes, alloc;
} dirs[MAXDIRS];
static int ndirs;

/* Headers after a file's first, which add to its space: they can come before
 * the first header in the file, so they wait until the end. */
static struct {
    unsigned first;
    unsigned long alloc;
} ext[MAXEXT];
static int next_ext;

typedef struct {
    unsigned header;
    unsigned long size, alloc;
    char name[51];
} File;
static File __far *files;
static unsigned nfiles;
static unsigned order[MAXFILES];

static char onedir[13];         /* the directory to list, or "" for all */
static unsigned char __far *dirOf;     /* by header number: index in dirs + 1 */
static unsigned maxHeader;

/* The directory's place in dirs, adding it if it's new. Names are compared
 * ignoring case, as CTOS does: system files give their directory as <sys>. */
static int find_dir(const char *name)
{
    int i;

    for (i = 0; i < ndirs; i++)
        if (far_cmp(dirs[i].name, name, 1) == 0)
            return i;
    if (ndirs == MAXDIRS)
        return -1;
    strcpy(dirs[ndirs].name, name);
    return ndirs++;
}

static int add_dir(const char *name, void *ctx)
{
    (void)ctx;
    find_dir(name);
    return 0;
}

static int add(VolFile *f, void *ctx)
{
    int i;

    (void)ctx;
    if (f->first != f->header) {
        if (next_ext < MAXEXT) {
            ext[next_ext].first = f->first;
            ext[next_ext++].alloc = f->alloc;
        }
        return 0;
    }
    if (onedir[0]) {
        if (far_cmp(f->dir, onedir, 1) != 0)
            return 0;
        if (nfiles == MAXFILES)
            return 0;
        files[nfiles].header = f->header;
        files[nfiles].size = f->size;
        files[nfiles].alloc = f->alloc;
        _fmemcpy(files[nfiles].name, f->name, sizeof f->name);
        nfiles++;
        return 0;
    }
    if ((i = find_dir(f->dir)) < 0)
        return 0;
    dirs[i].files++;
    dirs[i].bytes += f->size;
    dirs[i].alloc += f->alloc;
    if (f->header < maxHeader)
        dirOf[f->header] = (unsigned char)(i + 1);
    return 0;
}

static int by_name(const void *a, const void *b)
{
    return far_cmp(a, b, 1);
}

static int by_alloc(const void *a, const void *b)
{
    const File __far *x = &files[*(const unsigned *)a], __far *y = &files[*(const unsigned *)b];

    return x->alloc < y->alloc ? 1 : x->alloc > y->alloc ? -1 : 0;
}

int main(int argc, char **argv)
{
    char vol[20], name[51];
    const char *arg, *d, *e;
    unsigned long bytes = 0, alloc = 0;
    unsigned i;
    int k, erc;
    Volume v;

    tool_start("du", FORM, &argc, &argv);
    arg = argc > 1 ? argv[1] : "";
    if (argc > 2)
        usage("[volume or [vol]<dir>]");
    vol_of(arg, vol);
    if ((d = strchr(arg, '<')) != NULL && (e = strchr(d, '>')) != NULL && e - d <= 13) {
        memcpy(onedir, d + 1, e - d - 1);
        onedir[e - d - 1] = 0;
    }
    if ((erc = get_volume(vol, &v)) != 0) {
        warn(vol, erc);
        return tool_end();
    }
    maxHeader = v.headerPages;
    files = farmalloc(MAXFILES * sizeof(File));
    dirOf = farmalloc(maxHeader ? maxHeader : 1);
    if (!files || !dirOf) {
        fprintf(stderr, "du: not enough memory\n");
        return tool_end();
    }
    _fmemset(dirOf, 0, maxHeader);
    each_dir(vol, add_dir, NULL);       /* first, for the names as made, and the empty ones */
    if ((erc = each_header(vol, add, NULL)) != 0) {
        warn(vol, erc);
        return tool_end();
    }

    if (onedir[0]) {
        for (k = 0; k < next_ext; k++)
            for (i = 0; i < nfiles; i++)
                if (files[i].header == ext[k].first)
                    files[i].alloc += ext[k].alloc;
        for (i = 0; i < nfiles; i++)
            order[i] = i;
        qsort(order, nfiles, sizeof order[0], by_alloc);
        say("%8s %10s  %s<%s>\n", "KB", "bytes", vol, onedir);
        for (i = 0; i < nfiles; i++) {
            File __far *f = &files[order[i]];
            far_copy(name, f->name, sizeof name);
            say("%8lu %10lu  %s\n", KB(f->alloc), f->size, name);
            bytes += f->size;
            alloc += f->alloc;
        }
        say("%8lu %10lu  %u files\n", KB(alloc), bytes, nfiles);
        return tool_end();
    }

    for (k = 0; k < next_ext; k++)
        if (ext[k].first < maxHeader && dirOf[ext[k].first])
            dirs[dirOf[ext[k].first] - 1].alloc += ext[k].alloc;
    qsort(dirs, ndirs, sizeof dirs[0], by_name);
    say("%8s %10s %6s  %s\n", "KB", "bytes", "files", "directory");
    for (k = 0; k < ndirs; k++) {
        say("%8lu %10lu %6u  <%s>\n", KB(dirs[k].alloc), dirs[k].bytes, dirs[k].files,
            dirs[k].name);
        bytes += dirs[k].bytes;
        alloc += dirs[k].alloc;
        nfiles += dirs[k].files;
    }
    say("%8lu %10lu %6u  in %d directories on %s (%s)\n", KB(alloc), bytes, nfiles, ndirs, vol,
        v.name);
    if (v.totalPages)
        say("%8lu KB free of %lu KB\n", v.freePages / 2, v.totalPages / 2);
    else
        say("%8lu KB free\n", v.freePages / 2);
    return tool_end();
}
