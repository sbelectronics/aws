/* exec.c - execv and execf: build a Variable-Length Parameter Block in
 * long-lived memory, which Chain keeps, point the ASCB at it, and Chain to
 * the run file. The block is laid out as start.c reads it: after the sizes
 * and the count, an (offset, length) pair for each parameter, then the
 * parameters, each a run of subparameters, each a length byte and its
 * characters. */
#include <ctos.h>
#include <errno.h>
#include <process.h>
#include <string.h>
#include "libc.h"

#define PRIORITY 128    /* the middle of CTOS's 0 (highest) to 254 */

int execf(const char *path, char *const argv[], const int field[])
{
    static char *const none[] = { "", NULL };
    unsigned n, i, j, k, len, size, ob, start, nparams;
    unsigned char __far *v;
    void __far *p;
    void __far *ascb;
    ErcType erc;

    if (argv == NULL || argv[0] == NULL)
        argv = none, field = NULL;
    for (n = 0, size = 0; argv[n]; n++) {
        len = strlen(argv[n]);
        /* the block holds each length in a byte, and fields go up */
        if (len > 255 || (field && n > 0 && field[n] < field[n - 1]))
            return -1;
        size += 1 + len;
    }
    nparams = (field ? field[n - 1] : n - 1) + 1;
    size += 6 + 4 * nparams;
    if ((erc = GetpASCB(&ascb)) != 0 || (erc = AllocMemoryLL(size, &p)) != 0) {
        errno = erc;
        return -1;
    }
    v = p;
    ((unsigned __far *)v)[0] = size;            /* sVarParams */
    ((unsigned __far *)v)[1] = size;            /* ibFirstFree */
    ((unsigned __far *)v)[2] = nparams - 1;     /* cParams: parameter 0 isn't counted */
    ob = 6 + 4 * nparams;
    for (k = 0, i = 0; k < nparams; k++) {
        ((unsigned __far *)v)[3 + 2 * k] = ob;
        start = ob;
        for (; argv[i] && (field ? field[i] : (int)i) == (int)k; i++) {
            v[ob++] = (unsigned char)strlen(argv[i]);
            for (j = 0; argv[i][j]; j++)
                v[ob++] = argv[i][j];
        }
        ((unsigned __far *)v)[4 + 2 * k] = ob - start;
    }
    *(void __far * __far *)((char __far *)ascb + 2) = p;

    _closeall();
    erc = Chain(path, strlen(path), "", 0, PRIORITY, 0, 0);
    errno = erc;
    return -1;
}

int execv(const char *path, char *const argv[])
{
    return execf(path, argv, NULL);
}
