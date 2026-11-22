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

/* character codes of the special keys, as scr_getkey returns them; the same
 * on the AWS and the NGEN */
#define KEY_HELP       0x00
#define KEY_UP         0x01
#define KEY_MARK       0x02
#define KEY_FINISH     0x04
#define KEY_PREVPAGE   0x05
#define KEY_CANCEL     0x07
#define KEY_BACKSPACE  0x08
#define KEY_TAB        0x09
#define KEY_RETURN     0x0A
#define KEY_DOWN       0x0B
#define KEY_NEXTPAGE   0x0C
#define KEY_BOUND      0x0D
#define KEY_LEFT       0x0E
#define KEY_SCROLLUP   0x11
#define KEY_RIGHT      0x12
#define KEY_SCROLLDOWN 0x13
#define KEY_GO         0x1B
#define KEY_DELETE     0x7F
#define KEY_F1         0x15     /* f1 to f6 are 15h-1Ah; f7 to f10 skip GO */
#define KEY_F2         0x16
#define KEY_F3         0x17
#define KEY_F4         0x18
#define KEY_F5         0x19
#define KEY_F6         0x1A
#define KEY_F7         0x1C
#define KEY_F8         0x1D
#define KEY_F9         0x1E
#define KEY_F10        0x1F

/* Line-drawing and block characters, from the CTOS standard character set.
 * On the AWS, codes 80h and up are shown with the "special character"
 * attribute, and a screen line holds only 16 attribute changes: each separate
 * run of these characters uses two, as does each run of scr_attr. Adjacent
 * ones share a run, so a box's top and bottom edges are cheap, and its sides
 * cost two each per line. BLOCK is in the normal character set and costs
 * nothing. */
#define BOX_H       0xDA    /* single lines */
#define BOX_V       0xE1
#define BOX_TL      0xEF    /* corners: top left, top right, ... */
#define BOX_TR      0xF0
#define BOX_BL      0xED
#define BOX_BR      0xEE
#define BOX_LT      0xE2    /* tees: on the left edge, the right, the top, the bottom */
#define BOX_RT      0xE3
#define BOX_TT      0xDC
#define BOX_BT      0xDD
#define BOX_X       0xDB    /* crossing */
#define BLOCK       0x7F
#define SHADE_1     0xFD    /* the bar-chart fills */
#define SHADE_2     0xFE
#define SHADE_3     0xFF

int scr_rows(void);
void scr_clear(void);
void scr_puts(int row, int col, const char *s);
void scr_putc(int row, int col, int c);
void scr_attr(int row, int col, int n, int attr);
/* The AWS's cursor is a reverse block, so on a reverse character it cancels
 * out; the NGEN's is an underline. scr_cursor(SCR_NOCURSOR, SCR_NOCURSOR)
 * takes it away. */
#define SCR_NOCURSOR 255
void scr_cursor(int row, int col);
void scr_line(int row);

int scr_getkey(void);       /* waits for a key; no echo */
int scr_pollkey(void);      /* a key if one is waiting, otherwise -1 */
void delay(unsigned ms);    /* in steps of 100 ms */

/* Raw keys: with this on, scr_getkey and scr_pollkey return the keyboard's
 * own code for each key going down, and the code plus KEY_RELEASED for it
 * coming up, so a game can tell when keys are held. On the AWS, the arrows
 * are 01 (up), 0B (down), 0E (left) and 0F (right). A key that is down when
 * it's turned on reports its release, so a program may want to throw away
 * what comes at first. CTOS turns it off when the program ends. */
#define KEY_RELEASED 0x80
void scr_rawkeys(int on);

/* Loads the shapes of all 256 characters from a font file, on the NGEN,
 * whose font is in RAM. A font file holds 16 words per character, a
 * character 9 dots wide in the low 9 bits of words 1-15. The font stays
 * after the program ends: load the system font, [Sys]<Sys>t1Sys.font, back
 * before leaving. The AWS's font is in ROM, so there this changes nothing. */
int scr_loadfont(const char *name);

#endif
