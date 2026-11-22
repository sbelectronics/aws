/* dirent.h - reading CTOS directories, and making and removing them.
 * A directory is named like "[f0]<C>" or "<C>", or "" for the current one;
 * a file name is up to 50 characters. */
#ifndef DIRENT_H
#define DIRENT_H

struct dirent {
    char d_name[51];
};

typedef struct DIR DIR;

DIR *opendir(const char *dir);
struct dirent *readdir(DIR *d);
void rewinddir(DIR *d);
int closedir(DIR *d);

int mkdir(const char *dir);         /* 4 pages: room for about 100 files */
int rmdir(const char *dir);         /* only an empty directory */

#endif
