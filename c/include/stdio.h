/* stdio.h - console I/O, no files yet. stdout is CTOS video frame 0 and
 * stdin is the keyboard, read a line at a time with echo. */
#ifndef STDIO_H
#define STDIO_H

#include <stddef.h>
#include <stdarg.h>

#define EOF (-1)

typedef struct FILE FILE;
extern FILE *stdin, *stdout, *stderr;

int putchar(int c);
int puts(const char *s);
int fputc(int c, FILE *f);
int fputs(const char *s, FILE *f);
int fflush(FILE *f);
int getchar(void);
int fgetc(FILE *f);
char *fgets(char *s, int n, FILE *f);

int printf(const char *fmt, ...);
int fprintf(FILE *f, const char *fmt, ...);
int sprintf(char *buf, const char *fmt, ...);
int snprintf(char *buf, size_t n, const char *fmt, ...);
int vprintf(const char *fmt, va_list ap);
int vfprintf(FILE *f, const char *fmt, va_list ap);
int vsprintf(char *buf, const char *fmt, va_list ap);
int vsnprintf(char *buf, size_t n, const char *fmt, va_list ap);

#define putc(c, f) fputc(c, f)
#define getc(f)    fgetc(f)

#endif
