/* libc.h - shared by the library's own files */
#ifndef LIBC_H
#define LIBC_H

void con_putc(int c);
void con_flush(void);
int con_getc(void);

void _closeall(void);

#endif
