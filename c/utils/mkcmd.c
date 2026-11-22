/* mkcmd - installs a tool as an Executive command, for Install.sub.
 *
 *   mkcmd 'command name' tool 'description' 'field name'...
 *
 * It copies tool.run from the current path to [Sys]<Tools>, making that
 * directory if it isn't there: a directory of its own, so that no file of
 * CTOS's is replaced (the NGEN has a sort.run of its own). Then it makes the
 * command with New Command's own program, as though its form had been
 * filled in: the command's name, the run file, the field names, the
 * description, and "no" to replacing a command already there. Its form
 * takes only what fits on the screen line, too little for the field names.
 *
 * New Command is [Sys]<Sys>Command.run on the AWS and some NGENs, and
 * CmdFileEditor.run on others, which also does Remove Command and Merge
 * Command Files. Which one it's to do is the command's case, the second word
 * of parameter 0 after the command's name: AC for New Command. */
#include <dirent.h>
#include <errno.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DIR "[Sys]<Tools>"

static const struct {
    const char *run, *casecode;
} editors[] = {
    { "[Sys]<Sys>Command.run", NULL },
    { "[Sys]<Sys>CmdFileEditor.run", "AC" },
};

static int copy(const char *from, const char *to)
{
    static char buf[512];
    FILE *in, *out;
    size_t n;
    int ok;

    if ((in = fopen(from, "rb")) == NULL)
        return -1;
    if ((out = fopen(to, "wb")) == NULL) {
        fclose(in);
        return -1;
    }
    while ((n = fread(buf, 1, sizeof buf, in)) > 0 && fwrite(buf, 1, n, out) == n)
        ;
    ok = !ferror(in) && !ferror(out);
    fclose(in);
    return fclose(out) == 0 && ok ? 0 : -1;
}

int main(int argc, char **argv)
{
    char from[60], to[60], **args;
    int *field, i, e, n;

    if (argc < 4) {
        printf("usage: mkcmd 'command name' tool 'description' 'field name'...\n");
        return 0;
    }
    mkdir(DIR);                 /* fails if it's there already, which is fine */
    sprintf(from, "%s.run", argv[2]);
    sprintf(to, "%s%s.run", DIR, argv[2]);
    if (copy(from, to)) {
        printf("mkcmd: can't copy %s to %s: %s\n", from, to, strerror(errno));
        return 0;
    }
    args = malloc((argc + 4) * sizeof *args);
    field = malloc((argc + 4) * sizeof *field);
    if (!args || !field) {
        printf("mkcmd: not enough memory\n");
        return 0;
    }
    for (e = 0; e < (int)(sizeof editors / sizeof editors[0]); e++) {
        n = 0;
        args[n] = "New Command", field[n++] = 0;
        if (editors[e].casecode)
            args[n] = (char *)editors[e].casecode, field[n++] = 0;
        args[n] = argv[1], field[n++] = 1;      /* Command name */
        args[n] = to, field[n++] = 2;           /* Run file */
        for (i = 4; i < argc; i++)
            args[n] = argv[i], field[n++] = 3;  /* [Field names] */
        args[n] = argv[3], field[n++] = 4;      /* [Description] */
        args[n] = "no", field[n++] = 5;         /* [Overwrite ok?] */
        args[n] = NULL;
        execf(editors[e].run, args, field);     /* returns only if it can't */
    }
    printf("mkcmd: can't run New Command's program: %s\n", strerror(errno));
    return 0;
}
