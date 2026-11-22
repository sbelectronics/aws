/* charset.c - shows all 256 character codes, 16 to a row, and a box drawn
 * with the BOX_ characters, then waits for a key. Codes C0h-FFh are the
 * line-drawing and block characters. On the AWS a line shows only the first
 * few separate special characters: see screen.h. */
#include <screen.h>
#include <stdio.h>
#include <string.h>

static void box(int top, int left, int height, int width, const char *label)
{
    char line[SCR_COLS + 1];
    int r;

    memset(line, BOX_H, width);
    line[0] = (char)BOX_TL;
    line[width - 1] = (char)BOX_TR;
    line[width] = 0;
    scr_puts(top, left, line);
    line[0] = (char)BOX_BL;
    line[width - 1] = (char)BOX_BR;
    scr_puts(top + height - 1, left, line);
    for (r = top + 1; r < top + height - 1; r++) {
        scr_putc(r, left, BOX_V);
        scr_putc(r, left + width - 1, BOX_V);
    }
    scr_puts(top + height / 2, left + (width - (int)strlen(label)) / 2, label);
}

int main(void)
{
    char buf[8];
    int row, col;

    scr_clear();
    scr_puts(0, 0, "Character codes (row + column, in hex). Press a key.");
    for (col = 0; col < 16; col++) {
        sprintf(buf, "%X", col);
        scr_puts(2, 6 + 3 * col, buf);
    }
    for (row = 0; row < 16; row++) {
        sprintf(buf, "%X0", row);
        scr_puts(3 + row, 0, buf);
        for (col = 0; col < 16; col++)
            scr_putc(3 + row, 6 + 3 * col, row * 16 + col);
    }
    box(4, 58, 7, 18, "a box");
    scr_getkey();
    return 0;
}
