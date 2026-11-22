/* unistd.h - the current directory, and option letters on the command line */
#ifndef UNISTD_H
#define UNISTD_H

/* The default volume and directory, set by the Executive's Path command, as
 * "[d0]<Sys>". Returns NULL if it doesn't fit in size bytes. */
char *getcwd(char *buf, int size);

/* Takes options like "-n" and "-k 2" off the front of argv, one call at a
 * time, as POSIX getopt does: opts lists the letters, a letter followed by
 * ':' takes a value, given in optarg. Returns '?' for a letter not in opts
 * (with optopt set to it), and -1 at the first argument that isn't an
 * option, which is argv[optind]. "--" ends the options. */
int getopt(int argc, char **argv, const char *opts);
extern char *optarg;
extern int optind, optopt;

#endif
