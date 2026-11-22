/* sys/stat.h - a CTOS file's size and dates */
#ifndef SYS_STAT_H
#define SYS_STAT_H

#include <time.h>

struct stat {
    long st_size;           /* the end of file: the bytes in it */
    long st_alloc;          /* the space allocated, a multiple of 512 */
    time_t st_ctime;        /* when it was created */
    time_t st_mtime;        /* when it was last changed */
};

int stat(const char *name, struct stat *st);

#endif
