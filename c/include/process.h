/* process.h - running another program in place of this one */
#ifndef PROCESS_H
#define PROCESS_H

/* Runs the run file path with the arguments argv (argv[0] is its command
 * name; the list ends with NULL), as though from a command form with one
 * field per argument. Open files are closed first. Returns only if CTOS
 * can't start it, with -1 and the reason in errno. */
int execv(const char *path, char *const argv[]);

#endif
