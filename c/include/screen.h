/* screen.h - full-screen output and single keys, for programs that draw the
 * screen rather than print lines. Everything is on CTOS video frame 0, the
 * Executive's main frame: rows count from 0 at its top line, and it is
 * SCR_COLS wide. printf output continues on the line set by scr_line (the top
 * after scr_clear). */
#ifndef SCREEN_H
#define SCREEN_H

#define SCR_COLS 80

/* attributes for scr_attr; they combine */
#define ATTR_NORMAL     0
#define ATTR_HALFBRIGHT 1
#define ATTR_UNDERLINE  2
#define ATTR_REVERSE    4
#define ATTR_BLINK      8

/* character codes of some keys, as scr_getkey returns them (the arrows as
 * found on the AWS) */
#define KEY_UP        0x01
#define KEY_BACKSPACE 0x08
#define KEY_RETURN    0x0A
#define KEY_DOWN      0x0B
#define KEY_LEFT      0x0E
#define KEY_RIGHT     0x12
#define KEY_GO        0x1B

int scr_rows(void);
void scr_clear(void);
void scr_puts(int row, int col, const char *s);
void scr_putc(int row, int col, int c);
void scr_attr(int row, int col, int n, int attr);
void scr_cursor(int row, int col);
void scr_line(int row);

int scr_getkey(void);       /* waits for a key; no echo */
int scr_pollkey(void);      /* a key if one is waiting, otherwise -1 */
void delay(unsigned ms);    /* in steps of 100 ms */

#endif
