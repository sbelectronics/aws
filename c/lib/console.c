/* console.c - stdout and stdin on CTOS video frame 0 and the keyboard.
 *
 * The first output clears frame 0 and writes from its top line; at the
 * bottom the frame scrolls up. The current line is kept in a buffer and drawn
 * when it ends or when output is flushed. Input is read a line at a time, with
 * echo and BACKSPACE; RETURN or GO ends the line.
 */
#include <ctos.h>
#include <string.h>
#include "libc.h"

#define COLS 80

static unsigned rows, row, col;
static char line[COLS];
static int ready;

static void init(void)
{
    unsigned n;

    /* frame 0's height is the first line PosFrameCursor refuses */
    for (n = 0; n < 60 && PosFrameCursor(0, 0, n) == 0; n++)
        ;
    rows = n ? n : 1;
    ResetFrame(0);
    row = col = 0;
    memset(line, ' ', COLS);
    ready = 1;
}

static void show(void)
{
    PutFrameChars(0, 0, row, line, COLS);
}

static void newline(void)
{
    show();
    if (row + 1 < rows)
        row++;
    else
        ScrollFrame(0, 0, rows, 1, 0xFF);
    col = 0;
    memset(line, ' ', COLS);
}

void con_putc(int c)
{
    if (!ready)
        init();
    switch (c) {
    case '\n':
        newline();
        break;
    case '\r':
        col = 0;
        break;
    case '\b':
        if (col)
            col--;
        break;
    case '\t':
        do
            con_putc(' ');
        while (col & 7);
        break;
    default:
        if (col >= COLS)
            newline();
        line[col++] = (char)c;
    }
}

void con_flush(void)
{
    if (!ready)
        return;
    show();
    PosFrameCursor(0, col < COLS ? col : COLS - 1, row);
}

static char ibuf[128];
static unsigned ilen, ipos;

static void readline(void)
{
    char ch;

    if (!ready)
        init();
    ilen = 0;
    for (;;) {
        con_flush();
        if (ReadKbd(&ch) != 0)
            ch = '\n';
        if (ch == 0x0A || ch == 0x0D || ch == 0x1B) {   /* RETURN or GO */
            ibuf[ilen++] = '\n';
            con_putc('\n');
            break;
        }
        if (ch == 0x08 || ch == 0x7F) {                  /* BACKSPACE */
            if (ilen && col) {
                ilen--;
                line[--col] = ' ';
            }
            continue;
        }
        if ((unsigned char)ch >= ' ' && ilen < sizeof ibuf - 1) {
            ibuf[ilen++] = ch;
            con_putc(ch);
        }
    }
    con_flush();
    ipos = 0;
}

int con_getc(void)
{
    if (ipos >= ilen)
        readline();
    return (unsigned char)ibuf[ipos++];
}
