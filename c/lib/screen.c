/* screen.c - full-screen output and single keys on frame 0.
 *
 * Every function first makes sure the console has set up frame 0: otherwise
 * its first printf would clear what had been drawn. */
#include <ctos.h>
#include <screen.h>
#include <string.h>
#include "libc.h"

int scr_rows(void)
{
    return (int)con_rows();
}

void scr_clear(void)
{
    con_rows();
    ResetFrame(0);
    con_moveto(0);
}

void scr_puts(int row, int col, const char *s)
{
    unsigned n = strlen(s);

    con_rows();
    if (col < 0 || col >= SCR_COLS)
        return;
    if (n > (unsigned)(SCR_COLS - col))
        n = SCR_COLS - col;
    PutFrameChars(0, col, row, s, n);
}

void scr_putc(int row, int col, int c)
{
    char ch = (char)c;

    con_rows();
    if (col >= 0 && col < SCR_COLS)
        PutFrameChars(0, col, row, &ch, 1);
}

void scr_attr(int row, int col, int n, int attr)
{
    con_rows();
    if (col < 0 || col >= SCR_COLS || n <= 0)
        return;
    if (n > SCR_COLS - col)
        n = SCR_COLS - col;
    PutFrameAttrs(0, col, row, attr, n);
}

void scr_cursor(int row, int col)
{
    con_rows();
    PosFrameCursor(0, col, row);
}

void scr_line(int row)
{
    con_moveto(row);
}

int scr_getkey(void)
{
    char ch;

    con_flush();
    return ReadKbdDirect(0, &ch) == 0 ? (unsigned char)ch : -1;
}

int scr_pollkey(void)
{
    char ch;

    return ReadKbdDirect(1, &ch) == 0 ? (unsigned char)ch : -1;
}

void delay(unsigned ms)
{
    Delay(ms / 100 + (ms % 100 >= 50));
}
