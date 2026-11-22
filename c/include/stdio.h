/* stdio.h - stdout is CTOS video frame 0, stdin the keyboard (a line at a
 * time, with echo), and files are CTOS files. There is no text translation:
 * a file holds exactly the bytes written to it. */
#ifndef STDIO_H
#define STDIO_H

#include <stddef.h>
#include <stdarg.h>

#define EOF       (-1)
#define BUFSIZ    512
#define FOPEN_MAX 8
#define SEEK_SET  0
#define SEEK_CUR  1
#define SEEK_END  2

typedef struct FILE FILE;
extern FILE *stdin, *stdout, *stderr;

FILE *fopen(const char *name, const char *mode);
int fclose(FILE *f);
size_t fread(void *p, size_t size, size_t n, FILE *f);
size_t fwrite(const void *p, size_t size, size_t n, FILE *f);
int fseek(FILE *f, long offset, int whence);
long ftell(FILE *f);
void rewind(FILE *f);
int feof(FILE *f);
int ferror(FILE *f);
void clearerr(FILE *f);
int remove(const char *name);
int rename(const char *from, const char *to);
void perror(const char *s);

int putchar(int c);
int puts(const char *s);
int fputc(int c, FILE *f);
int fputs(const char *s, FILE *f);
int fflush(FILE *f);
int getchar(void);
int fgetc(FILE *f);
int ungetc(int c, FILE *f);
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
