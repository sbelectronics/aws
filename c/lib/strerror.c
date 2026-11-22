/* strerror.c - text for the CTOS error codes a program is likely to meet,
 * and perror. The numbers are from CTOS's Erc.Edf. */
#include <errno.h>
#include <stdio.h>
#include <string.h>

static const struct {
    unsigned erc;
    const char *text;
} msgs[] = {
    { 0, "no error" },
    { 1, "end of file" },
    { 2, "end of medium" },
    { 4, "operator intervention" },
    { 7, "not implemented" },
    { 33, "service not available" },
    { 60, "invalid communications line" },
    { 62, "invalid communications line handle" },
    { 201, "no free volume structure" },
    { 202, "directory full" },
    { 203, "no such file" },
    { 204, "no such directory" },
    { 205, "invalid file specification" },
    { 207, "invalid request code" },
    { 209, "file is read-only" },
    { 210, "invalid file handle" },
    { 211, "invalid buffer size" },
    { 212, "invalid file address" },
    { 214, "too many open files" },
    { 215, "no such volume" },
    { 216, "volume not mounted" },
    { 217, "wrong password" },
    { 218, "invalid mode" },
    { 219, "access denied" },
    { 220, "file in use" },
    { 224, "file already exists" },
    { 225, "no free file headers" },
    { 227, "device in use" },
    { 229, "device not mounted" },
    { 230, "disk full" },
    { 235, "wrong volume mounted" },
    { 236, "invalid device specification" },
    { 239, "can't rename to another volume" },
    { 240, "directory already exists" },
    { 241, "directory not empty" },
    { 242, "volume's directory list full" },
    { 244, "system device not ready" },
    { 246, "invalid run file" },
    { 247, "old format run file" },
    { 267, "file locked" },
    { 270, "not a directory" },
    { 276, "not a file" },
    { 400, "not enough memory" },
    { 402, "invalid deallocation" },
    { 410, "no more exchanges" },
    { 602, "no character available" },
    { 610, "keyboard buffer overflow" },
};

char *strerror(int erc)
{
    static char buf[24];
    unsigned i;

    for (i = 0; i < sizeof msgs / sizeof msgs[0]; i++)
        if (msgs[i].erc == (unsigned)erc)
            return (char *)msgs[i].text;
    sprintf(buf, "CTOS error %u", (unsigned)erc);
    return buf;
}

void perror(const char *s)
{
    if (s && *s)
        fprintf(stderr, "%s: ", s);
    fprintf(stderr, "%s (error %d)\n", strerror(errno), errno);
}
