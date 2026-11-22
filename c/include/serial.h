/* serial.h - the RS-232 ports, at 8 data bits, no parity and 1 stop bit.
 *
 * The program takes the port from CTOS and then reads and writes the serial
 * chip directly, polling it: ser_getc returns at once, so call it often
 * enough not to miss bytes (at 9600 baud one arrives about every
 * millisecond, and the chip holds three).
 *
 * On CTOS II (the NGEN) the port is taken with InitCommLine, which also sets
 * the baud rate; on the AWS's CTOS, with SetCommISR, and the program sets the
 * baud rate itself. */
#ifndef SERIAL_H
#define SERIAL_H

#define SER_A 0
#define SER_B 1

int ser_open(int port, unsigned baud);  /* 0, or -1 with the reason in errno */
int ser_putc(int c);                    /* waits until the chip can take it */
int ser_puts(const char *s);
int ser_getc(void);                     /* a byte if one has come, otherwise -1 */
void ser_close(void);                   /* gives the port back to CTOS */

#endif
