/* file.c - FILE: the console streams and CTOS files.
 *
 * CTOS reads and writes files only in whole 512-byte pages, so each open file
 * has a one-page buffer. Writing past the allocated size first grows it with
 * ChangeFileLength (GROW bytes at a time); closing gives back what's past the
 * end of file and records the exact length as the file's end of file.
 */
#include <ctos.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libc.h"

#define PAGE    512u
#define GROW    4096u
#define NOPAGE  0xFFFFFFFFUL
#define PAGEOF(pos) ((pos) & ~(unsigned long)(PAGE - 1))

enum { CON_IN, CON_OUT, DISK };

#define F_READ    0x01
#define F_WRITE   0x02
#define F_APPEND  0x04
#define F_EOF     0x08
#define F_ERR     0x10
#define F_DIRTY   0x20      /* the buffer differs from the page on disk */
#define F_LENGTH  0x40      /* the end of file on disk needs setting; also set after
                               ChangeFileLength, which can move it */

struct FILE {
    unsigned char kind, flags;
    unsigned fh;
    int unget;
    unsigned long pos;      /* where the next read or write goes */
    unsigned long len;      /* end of file */
    unsigned long alloc;    /* allocated size on disk */
    unsigned long page;     /* file address of the page in buf */
    char *buf;
};

int errno;

static FILE fin = { CON_IN, F_READ, 0, EOF };
static FILE fout = { CON_OUT, F_WRITE, 0, EOF };
static FILE ferr = { CON_OUT, F_WRITE, 0, EOF };
FILE *stdin = &fin, *stdout = &fout, *stderr = &ferr;

static FILE *open_files[FOPEN_MAX];

static int fail(FILE *f, ErcType erc)
{
    if (erc)
        errno = erc;
    f->flags |= F_ERR;
    return EOF;
}

static int flush_page(FILE *f)
{
    unsigned cb;
    unsigned long want;
    ErcType erc;

    if (!(f->flags & F_DIRTY))
        return 0;
    if (f->page + PAGE > f->alloc) {
        want = (f->page + PAGE + GROW - 1) & ~(unsigned long)(GROW - 1);
        if ((erc = ChangeFileLength(f->fh, want)) != 0)
            return fail(f, erc);
        f->alloc = want;
        f->flags |= F_LENGTH;
    }
    if ((erc = Write(f->fh, f->buf, PAGE, f->page, &cb)) != 0)
        return fail(f, erc);
    f->flags &= ~F_DIRTY;
    return 0;
}

static int load_page(FILE *f, unsigned long page)
{
    unsigned cb = 0;
    ErcType erc;

    if (f->page == page)
        return 0;
    if (flush_page(f))
        return EOF;
    if (page < f->len) {
        erc = Read(f->fh, f->buf, PAGE, page, &cb);
        if (erc != 0 && erc != ercEOF) {
            f->page = NOPAGE;
            return fail(f, erc);
        }
    }
    memset(f->buf + cb, 0, PAGE - cb);
    f->page = page;
    return 0;
}

/* Write the buffer, then the end of file if it has changed. */
static int sync(FILE *f)
{
    ErcType erc;

    if (flush_page(f))
        return EOF;
    if (f->flags & F_LENGTH) {
        if ((erc = SetFileStatus(f->fh, statusEOF, &f->len, sizeof f->len)) != 0)
            return fail(f, erc);
        f->flags &= ~F_LENGTH;
    }
    return 0;
}

FILE *fopen(const char *name, const char *mode)
{
    FILE *f;
    unsigned fh, cb = strlen(name), slot;
    unsigned char flags;
    int plus = strchr(mode, '+') != NULL;
    ErcType erc;

    for (slot = 0; slot < FOPEN_MAX && open_files[slot]; slot++)
        ;
    if (slot == FOPEN_MAX)
        return NULL;
    switch (mode[0]) {
    case 'r':
        flags = F_READ | (plus ? F_WRITE : 0);
        break;
    case 'w':
        flags = F_WRITE | (plus ? F_READ : 0);
        break;
    case 'a':
        flags = F_WRITE | F_APPEND | (plus ? F_READ : 0);
        break;
    default:
        return NULL;
    }

    erc = OpenFile(&fh, name, cb, "", 0, (flags & F_WRITE) ? modeModify : modeRead);
    if (erc == ercNoSuchFile && mode[0] != 'r') {
        erc = CreateFile(name, cb, "", 0, 0);
        if (erc == 0)
            erc = OpenFile(&fh, name, cb, "", 0, modeModify);
    }
    if (erc) {
        errno = erc;
        return NULL;
    }

    f = malloc(sizeof *f);
    if (f && (f->buf = malloc(PAGE)) == NULL) {
        free(f);
        f = NULL;
    }
    if (f == NULL) {
        CloseFile(fh);
        return NULL;
    }
    f->kind = DISK;
    f->flags = flags;
    f->fh = fh;
    f->unget = EOF;
    f->pos = 0;
    f->page = NOPAGE;
    if ((erc = GetFileStatus(fh, statusFileLength, &f->alloc, sizeof f->alloc)) != 0 ||
        (erc = GetFileStatus(fh, statusEOF, &f->len, sizeof f->len)) != 0 ||
        (mode[0] == 'w' && f->alloc && (erc = ChangeFileLength(fh, 0)) != 0)) {
        errno = erc;
        CloseFile(fh);
        free(f->buf);
        free(f);
        return NULL;
    }
    if (mode[0] == 'w') {
        f->alloc = f->len = 0;
        f->flags |= F_LENGTH;
    }
    if (flags & F_APPEND)
        f->pos = f->len;
    open_files[slot] = f;
    return f;
}

int fclose(FILE *f)
{
    unsigned long want;
    ErcType erc;
    int r = 0, i;

    if (f->kind != DISK)
        return fflush(f);
    if (flush_page(f))
        r = EOF;
    want = (f->len + PAGE - 1) & ~(unsigned long)(PAGE - 1);
    if (want < f->alloc) {
        if ((erc = ChangeFileLength(f->fh, want)) != 0)
            r = fail(f, erc);
        f->flags |= F_LENGTH;
    }
    if (sync(f))
        r = EOF;
    if ((erc = CloseFile(f->fh)) != 0)
        r = fail(f, erc);
    for (i = 0; i < FOPEN_MAX; i++)
        if (open_files[i] == f)
            open_files[i] = NULL;
    free(f->buf);
    free(f);
    return r;
}

void _closeall(void)
{
    int i;

    for (i = 0; i < FOPEN_MAX; i++)
        if (open_files[i])
            fclose(open_files[i]);
    con_flush();
}

int fflush(FILE *f)
{
    int i, r = 0;

    if (f == NULL) {
        for (i = 0; i < FOPEN_MAX; i++)
            if (open_files[i] && fflush(open_files[i]))
                r = EOF;
        con_flush();
        return r;
    }
    if (f->kind != DISK) {
        con_flush();
        return 0;
    }
    return sync(f);
}

int fgetc(FILE *f)
{
    int c;

    if (f->unget != EOF) {
        c = f->unget;
        f->unget = EOF;
        if (f->kind == DISK)
            f->pos++;
        return c;
    }
    if (f->kind == CON_IN)
        return con_getc();
    if (!(f->flags & F_READ))
        return fail(f, 0);
    if (f->pos >= f->len) {
        f->flags |= F_EOF;
        return EOF;
    }
    if (load_page(f, PAGEOF(f->pos)))
        return EOF;
    c = (unsigned char)f->buf[(unsigned)f->pos & (PAGE - 1)];
    f->pos++;
    return c;
}

int ungetc(int c, FILE *f)
{
    if (c == EOF || f->unget != EOF)
        return EOF;
    f->unget = (unsigned char)c;
    if (f->kind == DISK && f->pos)
        f->pos--;
    f->flags &= ~F_EOF;
    return f->unget;
}

/* Copies n bytes into the file at pos; returns how many were written. */
static size_t put_bytes(FILE *f, const char *s, size_t n)
{
    size_t done = 0;
    unsigned off, chunk;

    while (done < n) {
        if (load_page(f, PAGEOF(f->pos)))
            break;
        off = (unsigned)f->pos & (PAGE - 1);
        chunk = PAGE - off;
        if (chunk > n - done)
            chunk = n - done;
        memcpy(f->buf + off, s + done, chunk);
        f->flags |= F_DIRTY;
        done += chunk;
        f->pos += chunk;
        if (f->pos > f->len) {
            f->len = f->pos;
            f->flags |= F_LENGTH;
        }
    }
    return done;
}

/* Before a write: go to the end if appending, and if pos is past the end of
 * file, fill the gap with zeros (otherwise it would hold old disk contents). */
static int start_write(FILE *f)
{
    static const char zeros[64];
    unsigned long target;
    unsigned n;

    if (!(f->flags & F_WRITE))
        return fail(f, 0);
    if (f->flags & F_APPEND)
        f->pos = f->len;
    target = f->pos;
    while (f->len < target) {
        n = target - f->len > sizeof zeros ? sizeof zeros : (unsigned)(target - f->len);
        f->pos = f->len;
        if (put_bytes(f, zeros, n) != n) {
            f->pos = target;
            return EOF;
        }
    }
    f->pos = target;
    return 0;
}

int fputc(int c, FILE *f)
{
    char ch = (char)c;

    if (f->kind != DISK) {
        if (f->kind == CON_IN)
            return fail(f, 0);
        con_putc(c);
        if (c == '\n')
            con_flush();
        return (unsigned char)c;
    }
    if (start_write(f) || put_bytes(f, &ch, 1) != 1)
        return EOF;
    return (unsigned char)c;
}

size_t fread(void *p, size_t size, size_t n, FILE *f)
{
    char *d = p;
    size_t want = size * n, got = 0;
    unsigned off, chunk;
    int c;

    if (want == 0)
        return 0;
    if (f->kind != DISK) {
        while (got < want && (c = fgetc(f)) != EOF)
            d[got++] = (char)c;
        return got / size;
    }
    if (!(f->flags & F_READ)) {
        fail(f, 0);
        return 0;
    }
    if (f->unget != EOF)
        d[got++] = (char)fgetc(f);
    while (got < want) {
        if (f->pos >= f->len) {
            f->flags |= F_EOF;
            break;
        }
        if (load_page(f, PAGEOF(f->pos)))
            break;
        off = (unsigned)f->pos & (PAGE - 1);
        chunk = PAGE - off;
        if (chunk > want - got)
            chunk = want - got;
        if (chunk > f->len - f->pos)
            chunk = (unsigned)(f->len - f->pos);
        memcpy(d + got, f->buf + off, chunk);
        got += chunk;
        f->pos += chunk;
    }
    return got / size;
}

size_t fwrite(const void *p, size_t size, size_t n, FILE *f)
{
    const char *s = p;
    size_t want = size * n, done = 0;

    if (want == 0)
        return 0;
    if (f->kind != DISK) {
        while (done < want && fputc(s[done], f) != EOF)
            done++;
        return done / size;
    }
    if (start_write(f))
        return 0;
    return put_bytes(f, s, want) / size;
}

int fseek(FILE *f, long offset, int whence)
{
    long base;

    if (f->kind != DISK)
        return -1;
    if (whence == SEEK_SET)
        base = 0;
    else if (whence == SEEK_CUR)
        base = (long)f->pos;
    else if (whence == SEEK_END)
        base = (long)f->len;
    else
        return -1;
    if (base + offset < 0)
        return -1;
    f->pos = base + offset;
    f->unget = EOF;
    f->flags &= ~F_EOF;
    return 0;
}

long ftell(FILE *f)
{
    return f->kind == DISK ? (long)f->pos : -1L;
}

void rewind(FILE *f)
{
    fseek(f, 0L, SEEK_SET);
    f->flags &= ~F_ERR;
}

int feof(FILE *f)
{
    return (f->flags & F_EOF) != 0;
}

int ferror(FILE *f)
{
    return (f->flags & F_ERR) != 0;
}

void clearerr(FILE *f)
{
    f->flags &= ~(F_EOF | F_ERR);
}

int remove(const char *name)
{
    unsigned fh;
    ErcType erc;

    erc = OpenFile(&fh, name, strlen(name), "", 0, modeModify);
    if (erc == 0 && (erc = DeleteFile(fh)) != 0)    /* DeleteFile frees fh itself */
        CloseFile(fh);
    if (erc) {
        errno = erc;
        return -1;
    }
    return 0;
}

int rename(const char *from, const char *to)
{
    unsigned fh;
    ErcType erc, erc2;

    erc = OpenFile(&fh, from, strlen(from), "", 0, modeModify);
    if (erc == 0) {
        erc = RenameFile(fh, to, strlen(to), "", 0);
        erc2 = CloseFile(fh);
        if (erc == 0)
            erc = erc2;
    }
    if (erc) {
        errno = erc;
        return -1;
    }
    return 0;
}

int putchar(int c)
{
    return fputc(c, stdout);
}

int getchar(void)
{
    return fgetc(stdin);
}

int fputs(const char *s, FILE *f)
{
    for (; *s; s++)
        if (fputc(*s, f) == EOF)
            return EOF;
    if (f->kind != DISK)
        con_flush();
    return 0;
}

int puts(const char *s)
{
    if (fputs(s, stdout) == EOF)
        return EOF;
    return fputc('\n', stdout);
}

char *fgets(char *s, int n, FILE *f)
{
    int i = 0, c;

    while (i < n - 1) {
        c = fgetc(f);
        if (c == EOF)
            break;
        s[i++] = (char)c;
        if (c == '\n')
            break;
    }
    if (i == 0)
        return NULL;
    s[i] = 0;
    return s;
}
