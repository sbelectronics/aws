/* util.h - what the tools in this directory share: output, file names with
 * wildcards, lists of strings in far memory, dates, and reading a volume's
 * file headers. */
#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>

/* The tools end with 0 even when they find nothing or fail: the Executive
 * shows any other ending as an error, with an unrelated CTOS message. They
 * say what went wrong themselves.
 *
 * Output. A ">name" argument sends it to that file instead of the screen;
 * tool_start takes it off the argument list. The file is made when the
 * first line is written, so a tool can read a file and write it back. On the
 * screen, say() stops when the screen is full, until a key: CANCEL ends the
 * tool.
 *
 * The form. Each tool has a FORM string: what it does, then its command
 * form's fields, separated by |. A field's last word says what it is: "-x"
 * for a yes-or-no field that gives the option -x, "-x=" for one whose value
 * goes with -x, ">" for the output file; otherwise it holds arguments. Started
 * by its own command (made by Install.sub, which tools/mksub.py writes from
 * the FORM strings), tool_start turns the fields into the options and
 * arguments it gets with Run, so the rest of the tool sees only those. */
void tool_start(const char *name, const char *form, int *argc, char ***argv);
int tool_end(void);     /* closes the output; returns 0, for main to end with */
void say(const char *fmt, ...);
void warn(const char *what, int erc);   /* "name: what: the error's text" */
void usage(const char *text);           /* prints "usage: name text", ends the tool */
extern const char *prog;

/* File names, "[vol]<dir>name". Wildcards are * (any run of characters) and
 * ? (any one), and case doesn't matter, as in CTOS names. */
int wild(const char *pat, const char *name);
int has_wild(const char *s);
char *name_part(const char *spec);      /* where the file name starts */

/* Calls fn with each file the spec names: with wildcards in its last part,
 * every match in that directory, in alphabetical order. fn returns nonzero
 * to stop. Returns the number of files, after a message if there were none. */
int each_file(const char *spec, int (*fn)(const char *name, void *ctx), void *ctx);

/* Reads a line without its end of line, keeping at most size - 1
 * characters of it. Returns the length, or -1 at the end of the file. */
int read_line(FILE *f, char *buf, int size);

/* A list of strings in far memory, for lists bigger than the 64 KB segment
 * the program lives in. */
typedef struct {
    char __far * __far *item;
    unsigned n, max;
    char __far *pool;           /* the block the next string goes in */
    unsigned used, room;
    void __far *blocks[24];
    int nblocks;
} List;

int list_init(List *l, unsigned max);   /* 0, or -1 if there's no memory */
int list_add(List *l, const char *s);   /* 0, or -1 if the list is full */
void list_sort(List *l, int (*cmp)(const char __far *a, const char __far *b));
void list_free(List *l);
void far_copy(char *to, const char __far *s, unsigned size);    /* to a near string */
int far_cmp(const char __far *a, const char __far *b, int fold);

/* A CTOS date and time as "1987-11-05 13:00" */
char *date_str(unsigned long dt, char *buf);

/* The volume a spec names, as "[d0]": its own, or the current one. */
char *vol_of(const char *spec, char *buf);

/* A volume's home block, from GetVHB, with the fields the tools use. */
typedef struct {
    char name[13];
    unsigned long created, modified;
    unsigned long freePages, totalPages;    /* totalPages 0 if unknown */
    unsigned headerPages, freeHeaders, altOffset, mfdPages;
    unsigned sysImagePages, crashPages, logPages;
} Volume;
int get_volume(const char *vol, Volume *v);     /* 0 or an error code */
unsigned headers_used(const Volume *v);         /* the file headers in use */

/* Calls fn with the name of each directory on the volume, from its Master
 * File Directory, in no order. Returns 0 or an error code. */
int each_dir(const char *vol, int (*fn)(const char *name, void *ctx), void *ctx);

/* A file, from its file header */
typedef struct {
    char dir[13], name[51];
    unsigned long size;         /* the end of file */
    unsigned long alloc;        /* bytes allocated to it in this header */
    unsigned long created, modified;
    unsigned header, first;     /* this header, and the file's first one */
} VolFile;

/* Calls fn with every file header in use on the volume ("[d0]"): a file's
 * first header, or, when first != header, one carrying more of an earlier
 * file's extents, with no name. Returns 0 or an error code. */
int each_header(const char *vol, int (*fn)(VolFile *f, void *ctx), void *ctx);

#endif
