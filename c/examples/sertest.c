/* sertest.c - checks serial.h on port A at 9600 baud: sends "hello serial",
 * waits up to 15 seconds for a line to come in, prints it, and sends it back
 * after "echo: ". */
#include <errno.h>
#include <serial.h>
#include <stdio.h>
#include <time.h>

int main(void)
{
    char line[80];
    int n = 0, c;
    unsigned polls = 0;
    time_t end;

    if (ser_open(SER_A, 9600)) {
        printf("ser_open: error %d\n", errno);
        return 1;
    }
    ser_puts("hello serial\r\n");
    end = time(NULL) + 15;
    for (;;) {
        /* look at the clock now and then, so the chip is polled often */
        if (++polls % 256 == 0 && time(NULL) >= end)
            break;
        if ((c = ser_getc()) < 0)
            continue;
        if (c == '\r' || c == '\n') {
            if (n)
                break;
            continue;
        }
        if (n < (int)sizeof line - 1)
            line[n++] = (char)c;
    }
    line[n] = 0;
    printf("got: %s\n", line);
    ser_puts("echo: ");
    ser_puts(line);
    ser_puts("\r\n");
    ser_close();
    printf("sertest done\n");
    return 0;
}
