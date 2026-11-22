/* process.h - the program's command form, and running another program in
 * place of this one */
#ifndef PROCESS_H
#define PROCESS_H

/* Runs the run file path with the arguments argv (argv[0] is its command
 * name; the list ends with NULL), as though from a command form with one
 * field per argument. Open files are closed first. Returns only if CTOS
 * can't start it, with -1 and the reason in errno. */
int execv(const char *path, char *const argv[]);

/* The same, with argv[i] in the field numbered field[i]: 0 for argv[0], then
 * 1 for the first field, never going down. Several arguments in one field
 * are its words, as though typed there with spaces between; a field that no
 * argument names is empty. */
int execf(const char *path, char *const argv[], const int field[]);

/* The field of the command form that argv[i] came from: 1 for the first
 * field, and 0 for argv[0], the command's name. A field holding several words
 * gives several arguments, each with its number. With Run, the fields are its
 * [Param 1], [Param 2] and so on, and argv[0] is empty. */
int arg_field(int i);

#endif
