/* screentest.c - checks screen.h, delay and the date. It draws on the
 * screen, then shows the code of each key pressed until RETURN. The checks
 * are made from outside, on what is left on the screen:
 *   line 0   "screen test", in reverse video, from column 30
 *   line 1   "rows N", the height of the frame
 *   line 2   "clipp" at columns 75-79: text past column 79 is cut off
 *   line 3   "poll -1": no key was waiting
 *   line 4   "delay N": seconds a 3000 ms delay took (2 to 4)
 *   line 5   "date ...", the date and time
 *   line 6   "keys:" and the code of each key, in hex
 *   line 7   "raw:" and four codes read with raw keys on: two keys going
 *            down and up
 *   line 8   "printf on line 8"
 *   bottom   "bottom line" */
#include <screen.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

int main(void)
{
    char buf[80];
    int k, col;
    time_t t0, t1;

    scr_clear();
    scr_puts(0, 30, "screen test");
    scr_attr(0, 30, 11, ATTR_REVERSE);
    sprintf(buf, "rows %d", scr_rows());
    scr_puts(1, 0, buf);
    scr_puts(scr_rows() - 1, 0, "bottom line");
    scr_puts(2, 75, "clipped text");
    sprintf(buf, "poll %d", scr_pollkey());
    scr_puts(3, 0, buf);

    t0 = time(NULL);
    delay(3000);
    t1 = time(NULL);
    sprintf(buf, "delay %ld", t1 - t0);
    scr_puts(4, 0, buf);
    sprintf(buf, "date %s", ctime(&t1));
    buf[strlen(buf) - 1] = 0;
    scr_puts(5, 0, buf);

    scr_puts(6, 0, "keys:");
    for (col = 6; col < SCR_COLS - 3 && (k = scr_getkey()) != KEY_RETURN && k != -1; col += 3) {
        sprintf(buf, "%02X", k);
        scr_puts(6, col, buf);
    }

    /* the RETURN that ended the line above comes up after raw keys go on */
    scr_rawkeys(1);
    delay(500);
    while (scr_pollkey() != -1)
        ;
    scr_puts(7, 0, "raw:");
    for (col = 5; col < 17; col += 3) {
        sprintf(buf, "%02X", scr_getkey());
        scr_puts(7, col, buf);
    }
    scr_rawkeys(0);

    scr_line(8);
    printf("printf on line 8\n");
    return 0;
}
