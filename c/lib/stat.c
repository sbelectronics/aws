#include <ctos.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include "libc.h"

#define statusCreationDT 4
#define statusLastModDT  5

int stat(const char *name, struct stat *st)
{
    unsigned fh;
    unsigned long created, changed;
    ErcType erc;

    if ((erc = OpenFile(&fh, name, strlen(name), "", 0, modeRead)) != 0) {
        errno = erc;
        return -1;
    }
    if ((erc = GetFileStatus(fh, statusEOF, &st->st_size, 4)) == 0 &&
        (erc = GetFileStatus(fh, statusFileLength, &st->st_alloc, 4)) == 0 &&
        (erc = GetFileStatus(fh, statusCreationDT, &created, 4)) == 0 &&
        (erc = GetFileStatus(fh, statusLastModDT, &changed, 4)) == 0) {
        st->st_ctime = _ctos_time(created);
        st->st_mtime = _ctos_time(changed);
    }
    CloseFile(fh);
    if (erc) {
        errno = erc;
        return -1;
    }
    return 0;
}
