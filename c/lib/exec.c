/* exec.c - execv: build a Variable-Length Parameter Block in long-lived
 * memory, which Chain keeps, point the ASCB at it, and Chain to the run file.
 * The block is laid out as start.c reads it: each argument is a parameter
 * with one subparameter, so an argument may hold spaces. */
#include <ctos.h>
#include <errno.h>
#include <process.h>
#include <string.h>
#include "libc.h"

#define PRIORITY 128    /* the middle of CTOS's 0 (highest) to 254 */

int execv(const char *path, char *const argv[])
{
    static char *const none[] = { "", NULL };
    unsigned n, i, j, len, size, ob;
    unsigned char __far *v;
    void __far *p;
    void __far *ascb;
    ErcType erc;

    if (argv == NULL || argv[0] == NULL)
        argv = none;
    for (n = 0, size = 6; argv[n]; n++) {
        len = strlen(argv[n]);
        if (len > 255)          /* the block holds each length in a byte */
            return -1;
        size += 4 + 1 + len;
    }
    if ((erc = GetpASCB(&ascb)) != 0 || (erc = AllocMemoryLL(size, &p)) != 0) {
        errno = erc;
        return -1;
    }
    v = p;
    ((unsigned __far *)v)[0] = size;            /* sVarParams */
    ((unsigned __far *)v)[1] = size;            /* ibFirstFree */
    ((unsigned __far *)v)[2] = n - 1;           /* cParams: parameter 0 isn't counted */
    ob = 6 + 4 * n;
    for (i = 0; i < n; i++) {
        len = strlen(argv[i]);
        ((unsigned __far *)v)[3 + 2 * i] = ob;
        ((unsigned __far *)v)[4 + 2 * i] = len + 1;
        v[ob] = (unsigned char)len;
        for (j = 0; j < len; j++)
            v[ob + 1 + j] = argv[i][j];
        ob += len + 1;
    }
    *(void __far * __far *)((char __far *)ascb + 2) = p;

    _closeall();
    erc = Chain(path, strlen(path), "", 0, PRIORITY, 0, 0);
    errno = erc;
    return -1;
}
