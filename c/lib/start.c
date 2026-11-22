/* start.c - builds argc and argv from the command form, then runs main.
 *
 * The Executive leaves the form's fields in the Variable-Length Parameter
 * Block, which the ASCB points to (offset 2):
 *   4  cParams, the number of fields (parameter 0, the command, isn't counted)
 *   6  for parameters 0 to cParams, the (offset, length) in the VLPB of its
 *      subparameters, each of which is a length byte and its characters
 *
 * argv[0] is the command name, parameter 0's first subparameter. (The rest of
 * parameter 0 is the Executive's own.) Then come the subparameters of each
 * field in order, so a field holding "a b" gives two arguments and an empty
 * field gives none.
 */
#include <ctos.h>
#include <process.h>
#include <stdlib.h>
#include "libc.h"

int main(int argc, char **argv);

/* Watcom makes a main that takes arguments refer to __argc, by that exact
 * name (data names normally get a leading underscore) */
#pragma aux __argc "*";
#pragma aux __argv "*";
int __argc;
char **__argv;
static unsigned char *fields;   /* the field each argument came from */

/* Walks the VLPB. With argv NULL it only counts the arguments and the bytes
 * their strings need; otherwise it fills in argv, the strings and fields. */
static unsigned collect(const unsigned char __far *vlpb, char **argv, char *str,
                        unsigned *bytes)
{
    unsigned cparams = *(const unsigned __far *)(vlpb + 4), i, j, n, argc = 0;
    const unsigned char __far *s, __far *end;

    *bytes = 0;
    for (i = 0; i <= cparams; i++) {
        s = vlpb + *(const unsigned __far *)(vlpb + 6 + 4 * i);
        end = s + *(const unsigned __far *)(vlpb + 8 + 4 * i);
        while (s < end) {
            n = *s++;
            if (n > (unsigned)(end - s))
                break;
            if (argv) {
                fields[argc] = (unsigned char)i;
                argv[argc] = str;
                for (j = 0; j < n; j++)
                    *str++ = s[j];
                *str++ = 0;
            }
            s += n;
            *bytes += n + 1;
            argc++;
            if (i == 0)
                break;
        }
    }
    return argc;
}

int arg_field(int i)
{
    return fields && i >= 0 && i < __argc ? fields[i] : 0;
}

void _cmain(void)
{
    static char *noargs[] = { "", NULL };
    void __far *ascb;
    const unsigned char __far *vlpb = NULL;
    char **argv = noargs, *str;
    unsigned argc = 1, n, bytes;

    if (GetpASCB(&ascb) == 0 && ascb != NULL)
        vlpb = *(const unsigned char __far * __far *)((const char __far *)ascb + 2);
    if (vlpb != NULL && (n = collect(vlpb, NULL, NULL, &bytes)) != 0) {
        argv = malloc((n + 1) * sizeof *argv);
        str = malloc(bytes);
        fields = malloc(n);
        if (argv && str && fields) {
            argc = collect(vlpb, argv, str, &bytes);
            argv[argc] = NULL;
        } else {
            argv = noargs;
            fields = NULL;
        }
    }
    __argc = argc;
    __argv = argv;
    exit(main(argc, argv));
}
