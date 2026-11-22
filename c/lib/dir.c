/* dir.c - directories. ReadDirSector gives a directory one 512-byte page at a
 * time. Byte 0 of a page is a flag (the directory's hashing overflowed past
 * this page); then come entries, each a length byte, the file name and a
 * 2-byte file header number, until a length of 0. */
#include <ctos.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PAGE 512

struct DIR {
    unsigned char buf[PAGE];    /* first: CTOS needs it at an even address */
    char spec[64];
    unsigned page;              /* the page in buf */
    unsigned off;               /* the next entry in it */
    int loaded;
    struct dirent ent;
};

static int load(DIR *d)
{
    ErcType erc = ReadDirSector(d->spec, strlen(d->spec), "", 0, d->page, d->buf);

    if (erc) {
        if (erc != ercEOF)
            errno = erc;
        return -1;
    }
    d->off = 1;
    d->loaded = 1;
    return 0;
}

DIR *opendir(const char *dir)
{
    DIR *d;

    if (strlen(dir) >= sizeof d->spec || (d = malloc(sizeof *d)) == NULL)
        return NULL;
    strcpy(d->spec, dir);
    if (dir[0] == 0 && getcwd(d->spec, sizeof d->spec) == NULL) {    /* "" is the current one */
        free(d);
        return NULL;
    }
    d->page = 0;
    if (load(d)) {
        free(d);
        return NULL;
    }
    return d;
}

struct dirent *readdir(DIR *d)
{
    unsigned n;

    for (;;) {
        if (!d->loaded)
            return NULL;
        n = d->off < PAGE ? d->buf[d->off] : 0;
        if (n == 0 || d->off + 3 + n > PAGE) {
            d->page++;
            d->loaded = 0;
            load(d);
            continue;
        }
        memcpy(d->ent.d_name, d->buf + d->off + 1, n);
        d->ent.d_name[n] = 0;
        d->off += 3 + n;
        return &d->ent;
    }
}

void rewinddir(DIR *d)
{
    d->page = 0;
    d->loaded = 0;
    load(d);
}

int closedir(DIR *d)
{
    free(d);
    return 0;
}

int mkdir(const char *dir)
{
    /* protection level 15: files in it are unprotected */
    ErcType erc = CreateDir(dir, strlen(dir), "", 0, "", 0, 4, 15);

    if (erc) {
        errno = erc;
        return -1;
    }
    return 0;
}

int rmdir(const char *dir)
{
    ErcType erc = DeleteDir(dir, strlen(dir), "", 0);

    if (erc) {
        errno = erc;
        return -1;
    }
    return 0;
}
