/* libc.h - shared by the library's own files */
#ifndef LIBC_H
#define LIBC_H

void con_putc(int c);
void con_flush(void);
int con_getc(void);
unsigned con_rows(void);
void con_moveto(unsigned row);

void _closeall(void);

/* a CTOS date and time as seconds since 1970 */
long _ctos_time(unsigned long dt);

#endif
