/* util.c - shared by the tools: see util.h */
#include <ctos.h>
#include <ctosmsg.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <farmem.h>
#include <process.h>
#include <screen.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "util.h"

const char *prog = "";
static const char *outname;     /* the file after ">", or NULL for the screen */
static FILE *out;
static int lines, col;          /* on the screen since the last pause */

/* The role of field f (1 for the first) in the form: "" for an argument,
 * "-x" for a yes-or-no field, "-x=" for an option's value, ">" for the
 * output file. */
static const char *field_role(const char *form, int f, char *buf)
{
    char field[60], *w;
    const char *p = form;
    int n;

    for (n = 0; n < f && p; n++) {     /* the description comes first */
        p = strchr(p, '|');
        if (p)
            p++;
    }
    buf[0] = 0;
    if (p == NULL)
        return buf;
    for (n = 0; n < (int)sizeof field - 1 && p[n] && p[n] != '|'; n++)
        field[n] = p[n];
    field[n] = 0;
    /* the role is the field's last word, if it starts with - or > */
    w = strrchr(field, ' ');
    if (w && (w[1] == '-' || w[1] == '>') && strlen(w + 1) < 8)
        strcpy(buf, w + 1);
    return buf;
}

/* Started by its own command, the tool gets the form's fields: this turns
 * them into the options and arguments it takes from Run. */
static void from_form(const char *form, int *argc, char ***argvp)
{
    char **argv = *argvp, **args, role[8];
    int i, n = 1;

    args = malloc((2 * *argc + 1) * sizeof *args);
    if (args == NULL)
        return;
    args[0] = argv[0];
    for (i = 1; i < *argc; i++) {     /* options first, for getopt */
        field_role(form, arg_field(i), role);
        if (role[0] == '-' && role[2] == '=') {
            args[n] = strdup(role);
            args[n++][2] = 0;
            args[n++] = argv[i];
        } else if (role[0] == '-' && (argv[i][0] == 'y' || argv[i][0] == 'Y'))
            args[n++] = strdup(role);
    }
    for (i = 1; i < *argc; i++) {
        field_role(form, arg_field(i), role);
        if (role[0] == '>')
            outname = argv[i];
        else if (role[0] == 0)
            args[n++] = argv[i];
    }
    args[n] = NULL;
    *argc = n;
    *argvp = args;
}

void tool_start(const char *name, const char *form, int *argc, char ***argvp)
{
    char **argv;
    int i, j;

    prog = name;
    if ((*argvp)[0][0] && form)
        from_form(form, argc, argvp);
    argv = *argvp;
    for (i = j = 1; i < *argc; i++) {
        if (argv[i][0] == '>' && argv[i][1])
            outname = argv[i] + 1;
        else if (argv[i][0] == '>' && i + 1 < *argc)
            outname = argv[++i];
        else
            argv[j++] = argv[i];
    }
    *argc = j;
    argv[j] = NULL;
}

int tool_end(void)
{
    if (out && fclose(out) != 0)
        warn(outname, errno);
    out = NULL;
    return 0;
}

/* Waits for a key when the screen has filled since the last wait. */
static void wait_if_full(void)
{
    static int rows;
    int k;

    if (rows == 0)
        rows = scr_rows();
    if (lines < rows - 1)
        return;
    printf("-- more: RETURN goes on, CANCEL stops --");
    k = scr_getkey();
    printf("\r%40s\r", "");
    if (k == KEY_CANCEL)
        exit(tool_end());
    lines = 0;
}

void say(const char *fmt, ...)
{
    static char buf[1200];
    va_list ap;
    char *s;

    va_start(ap, fmt);
    if (outname) {
        if (out == NULL && (out = fopen(outname, "w")) == NULL) {
            warn(outname, errno);
            exit(0);
        }
        vfprintf(out, fmt, ap);
    } else {
        vsnprintf(buf, sizeof buf, fmt, ap);
        for (s = buf; *s; s++) {
            putchar(*s);
            if (*s == '\n' || ++col == SCR_COLS) {
                col = 0;
                lines++;
                wait_if_full();
            }
        }
    }
    va_end(ap);
}

void warn(const char *what, int erc)
{
    fprintf(stderr, "%s: %s: %s\n", prog, what, strerror(erc));
}

void usage(const char *text)
{
    fprintf(stderr, "usage: %s %s\n", prog, text);
    exit(0);
}

int wild(const char *pat, const char *s)
{
    for (; *pat; pat++, s++) {
        if (*pat == '*') {
            for (pat++; ; s++) {
                if (wild(pat, s))
                    return 1;
                if (*s == 0)
                    return 0;
            }
        }
        if (*s == 0 || (*pat != '?' && tolower((unsigned char)*pat) != tolower((unsigned char)*s)))
            return 0;
    }
    return *s == 0;
}

int has_wild(const char *s)
{
    return strchr(s, '*') != NULL || strchr(s, '?') != NULL;
}

char *name_part(const char *spec)
{
    const char *p = strrchr(spec, '>');

    if (p == NULL)
        p = strrchr(spec, ']');
    return (char *)(p ? p + 1 : spec);
}

static int name_cmp(const char __far *a, const char __far *b)
{
    return far_cmp(a, b, 1);
}

int each_file(const char *spec, int (*fn)(const char *name, void *ctx), void *ctx)
{
    char dir[80], name[51], path[132];
    const char *pat = name_part(spec);
    struct dirent *e;
    unsigned i;
    DIR *d;
    List l;

    if (!has_wild(pat)) {
        fn(spec, ctx);
        return 1;
    }
    if ((unsigned)(pat - spec) >= sizeof dir) {
        warn(spec, ercNoSuchFile);
        return 0;
    }
    memcpy(dir, spec, pat - spec);
    dir[pat - spec] = 0;
    if ((d = opendir(dir)) == NULL) {
        warn(dir[0] ? dir : "the current directory", errno);
        return 0;
    }
    if (list_init(&l, 2000)) {
        warn(spec, errno);
        closedir(d);
        return 0;
    }
    while ((e = readdir(d)) != NULL)
        if (wild(pat, e->d_name) && list_add(&l, e->d_name))
            break;
    closedir(d);
    list_sort(&l, name_cmp);
    for (i = 0; i < l.n; i++) {
        far_copy(name, l.item[i], sizeof name);
        sprintf(path, "%s%s", dir, name);
        if (fn(path, ctx))
            break;
    }
    if (l.n == 0)
        fprintf(stderr, "%s: no file matches %s\n", prog, spec);
    i = l.n;
    list_free(&l);
    return i;
}

int read_line(FILE *f, char *buf, int size)
{
    int c, n = 0;

    while ((c = getc(f)) != EOF && c != '\n') {
        if (c != '\r' && n < size - 1)
            buf[n++] = (char)c;
    }
    buf[n] = 0;
    return c == EOF && n == 0 ? -1 : n;
}

/* Lists */

#define POOL 16000u

int list_init(List *l, unsigned max)
{
    memset(l, 0, sizeof *l);
    if (max > 16000)
        max = 16000;
    l->item = farmalloc(max * sizeof(char __far *));
    if (l->item == 0)
        return -1;
    l->max = max;
    return 0;
}

int list_add(List *l, const char *s)
{
    unsigned len = strlen(s) + 1;

    if (l->n == l->max)
        return -1;
    if (l->room - l->used < len) {
        if (l->nblocks == sizeof l->blocks / sizeof l->blocks[0] ||
            (l->pool = farmalloc(POOL)) == 0)
            return -1;
        l->blocks[l->nblocks++] = l->pool;
        l->used = 0;
        l->room = POOL;
    }
    _fmemcpy(l->pool + l->used, s, len);
    l->item[l->n++] = l->pool + l->used;
    l->used += len;
    return 0;
}

/* Heapsort: it needs no memory beyond the list, and is never slow. */
static void sift(char __far * __far *a, unsigned i, unsigned n,
                 int (*cmp)(const char __far *, const char __far *))
{
    char __far *t = a[i];
    unsigned c;

    while ((c = 2 * i + 1) < n) {
        if (c + 1 < n && cmp(a[c + 1], a[c]) > 0)
            c++;
        if (cmp(a[c], t) <= 0)
            break;
        a[i] = a[c];
        i = c;
    }
    a[i] = t;
}

void list_sort(List *l, int (*cmp)(const char __far *a, const char __far *b))
{
    char __far * __far *a = l->item;
    char __far *t;
    unsigned i;

    for (i = l->n / 2; i-- > 0;)
        sift(a, i, l->n, cmp);
    for (i = l->n; i-- > 1;) {
        t = a[0];
        a[0] = a[i];
        a[i] = t;
        sift(a, 0, i, cmp);
    }
}

void list_free(List *l)
{
    while (l->nblocks > 0)
        farfree(l->blocks[--l->nblocks]);
    if (l->item)
        farfree(l->item);
    l->item = 0;
    l->n = 0;
}

void far_copy(char *to, const char __far *s, unsigned size)
{
    while (--size && *s)
        *to++ = *s++;
    *to = 0;
}

int far_cmp(const char __far *a, const char __far *b, int fold)
{
    int c, d;

    for (;; a++, b++) {
        c = (unsigned char)*a;
        d = (unsigned char)*b;
        if (fold) {
            c = tolower(c);
            d = tolower(d);
        }
        if (c != d || c == 0)
            return c - d;
    }
}

char *date_str(unsigned long dt, char *buf)
{
    time_t t = ctos_time(dt);
    struct tm *tm = gmtime(&t);

    if (dt == 0)
        strcpy(buf, "-");
    else
        sprintf(buf, "%04d-%02d-%02d %02d:%02d", tm->tm_year + 1900, tm->tm_mon + 1,
                tm->tm_mday, tm->tm_hour, tm->tm_min);
    return buf;
}

char *vol_of(const char *spec, char *buf)
{
    const char *e;
    char cwd[40];

    if (spec[0] != '[' || (e = strchr(spec, ']')) == NULL || e - spec > 14) {
        if (getcwd(cwd, sizeof cwd) == NULL || (e = strchr(cwd, ']')) == NULL)
            return strcpy(buf, "[Sys]");
        spec = cwd;
    }
    memcpy(buf, spec, e - spec + 1);
    buf[e - spec + 1] = 0;
    return buf;
}

/* Volumes and file headers. The layouts are CTOS's own, as ctostool reads
 * them: see ctosdisk.py. */

#define rqGetVHB 15

static unsigned w16(const unsigned char __far *p, unsigned off)
{
    return p[off] | p[off + 1] << 8;
}

static unsigned long w32(const unsigned char __far *p, unsigned off)
{
    return w16(p, off) | (unsigned long)w16(p, off + 2) << 16;
}

static void sb_copy(char *to, const unsigned char __far *sb, unsigned max)
{
    unsigned n = sb[0] < max ? sb[0] : max;

    _fmemcpy(to, sb + 1, n);
    to[n] = 0;
}

int get_volume(const char *vol, Volume *v)
{
    static unsigned vhb[128];
    const unsigned char *b = (const unsigned char *)vhb;
    char dev[16];
    unsigned long pages;
#pragma pack(push, 1)
    struct {
        RqHeader h;
        unsigned char reserved[6];
        const char __far *pbDevSpec;
        unsigned cbDevSpec;
        void __far *pVhbRet;
        unsigned sVhbMax;
    } rq;
#pragma pack(pop)
    ErcType erc;

    strncpy(dev, vol, sizeof dev - 1);
    dev[sizeof dev - 1] = 0;
    memset(&rq, 0, sizeof rq);
    rq.h.sCntInfo = 6;
    rq.h.nReqPbCb = 1;
    rq.h.nRespPbCb = 1;
    rq.h.rqCode = rqGetVHB;
    rq.pbDevSpec = dev;
    rq.cbDevSpec = strlen(dev);
    rq.pVhbRet = vhb;
    rq.sVhbMax = sizeof vhb;
    memset(vhb, 0, sizeof vhb);
    if ((erc = DoRequest(&rq)) != 0)
        return erc;
    sb_copy(v->name, b + 20, 12);
    v->created = w32(b, 54);
    v->modified = w32(b, 58);
    v->mfdPages = w16(b, 66);
    v->logPages = w16(b, 72);
    v->headerPages = w16(b, 82);
    v->altOffset = w16(b, 84);
    v->freeHeaders = w16(b, 88);
    v->freePages = w32(b, 108);
    v->sysImagePages = w16(b, 6);
    v->crashPages = w16(b, 18);
    /* the geometry is the boot ROM's, and on floppies it isn't the whole disk */
    pages = (unsigned long)w16(b, 239) * w16(b, 241) * w16(b, 243) * w16(b, 245) / 512;
    v->totalPages = pages > v->freePages ? pages : 0;
    return 0;
}

/* The headers come in groups of altOffset pages, a group of first copies
 * then a group of their alternates. */
static int primary(const Volume *v, unsigned page)
{
    return v->altOffset == 0 ||
           ((page / v->altOffset) % 2 == 0 && page + v->altOffset < v->headerPages);
}

unsigned headers_used(const Volume *v)
{
    unsigned firsts = v->altOffset ? v->headerPages / 2 : v->headerPages;

    /* page 0 is never a header */
    return firsts > v->freeHeaders ? firsts - v->freeHeaders - 1 : 0;
}

int each_header(const char *vol, int (*fn)(VolFile *f, void *ctx), void *ctx)
{
    char spec[40];
    unsigned char __far *buf, __far *h;
    unsigned fh, cb, page, k, n, i, runs, left;
    VolFile f;
    Volume v;
    ErcType erc;

    if ((erc = get_volume(vol, &v)) != 0)
        return erc;
    sprintf(spec, "%s<Sys>FileHeaders.Sys", vol);
    if ((erc = OpenFile(&fh, spec, strlen(spec), "", 0, modeRead)) != 0)
        return erc;
    if ((buf = farmalloc(8 * 512)) == 0) {
        CloseFile(fh);
        return (ErcType)errno;
    }
    /* CTOS hands out headers from the front, so the ones in use are mostly
     * early in the file: stop when all of them have been seen */
    left = headers_used(&v);
    for (page = 0; page < v.headerPages && left > 0 && erc == 0; page += 8) {
        n = v.headerPages - page < 8 ? v.headerPages - page : 8;
        if ((erc = Read(fh, buf, n * 512, page * 512UL, &cb)) != 0)
            break;
        for (k = 0; k < n && left > 0; k++) {
            h = buf + k * 512;
            if (!primary(&v, page + k) || w16(h, 81) == 0)  /* 0: free */
                continue;
            left--;
            memset(&f, 0, sizeof f);
            f.header = page + k;
            f.first = w16(h, 81);
            if (f.first == f.header && h[85] == 0) {
                sb_copy(f.name, h + 4, 50);
                sb_copy(f.dir, h + 68, 12);
                f.size = w32(h, 111);
                f.created = w32(h, 92);
                f.modified = w32(h, 96);
            }
            runs = w16(h, 119);
            for (i = 0; i < runs && i < 32; i++)
                f.alloc += w32(h, 249 + 4 * i);
            if (fn(&f, ctx)) {
                left = 0;
                break;
            }
        }
    }
    farfree(buf);
    CloseFile(fh);
    return erc;
}

int each_dir(const char *vol, int (*fn)(const char *name, void *ctx), void *ctx)
{
    static unsigned buf[256];   /* a word array: CTOS reads to even addresses */
    const unsigned char *b = (const unsigned char *)buf;
    char spec[40], name[13];
    unsigned fh, cb, page, k;
    Volume v;
    ErcType erc;

    if ((erc = get_volume(vol, &v)) != 0)
        return erc;
    sprintf(spec, "%s<Sys>Mfd.Sys", vol);
    if ((erc = OpenFile(&fh, spec, strlen(spec), "", 0, modeRead)) != 0)
        return erc;
    /* each page: a byte, then 14 entries of 35 bytes, each starting with the
     * directory's name; an empty name is an empty slot */
    for (page = 0; page < v.mfdPages; page++) {
        if ((erc = Read(fh, buf, 512, page * 512UL, &cb)) != 0)
            break;
        for (k = 0; k < 14; k++) {
            if (b[1 + 35 * k] == 0)
                continue;
            sb_copy(name, b + 1 + 35 * k, 12);
            if (fn(name, ctx)) {
                page = v.mfdPages;
                break;
            }
        }
    }
    CloseFile(fh);
    return erc;
}
