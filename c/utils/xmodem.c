/* xmodem - sends and receives files over a serial port, with XMODEM.
 *
 *   xmodem -s file [-k] [-p B] [-b baud]       sends the file
 *   xmodem -r file [-t] [-p B] [-b baud]       receives into the file
 *
 * Start xmodem here, then the other end's send or receive, in a terminal
 * program such as Tera Term, ExtraPuTTY or minicom (with lrzsz's sx and
 * rx). It waits a minute for the other end; CANCEL stops it.
 *
 * The port is A unless -p B, at 9600 baud unless -b, with 8 data bits, no
 * parity and 1 stop bit. Each block has a CRC-16 if the other end can do
 * it, otherwise a checksum, and goes again when it arrives damaged. -k
 * sends 1 KB blocks (XMODEM-1K), which is quicker; xmodem always takes them.
 *
 * XMODEM sends whole blocks of 128 or 1024 bytes, so a received file ends
 * with up to 127 bytes of padding, ^Z (1Ah). -t drops the ^Zs at the end,
 * which is right for text, and for any file that doesn't end in one. */
#include <errno.h>
#include <screen.h>
#include <serial.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "util.h"

/* the command form, for tool_start and tools/mksub.py */
#define FORM "Send or receive a file over a serial port with XMODEM|" \
    "File|[Send?] -s|[Receive?] -r|[Port (A or B)] -p=|[Baud rate] -b=|" \
    "[1 KB blocks?] -k|[Drop the ^Z padding?] -t"

#define SOH 0x01
#define STX 0x02
#define EOT 0x04
#define ACK 0x06
#define NAK 0x15
#define CAN 0x18
#define SUB 0x1A

#define TRIES   10      /* for each block */
#define WAIT    60      /* seconds to wait for the other end to start */

static unsigned char block[1024 + 5];
static int crc_mode;
static unsigned long held;      /* ^Zs received and not yet written, with -t */

/* Gives up on CANCEL, telling the other end. */
static void cancel_key(void)
{
    if (scr_pollkey() == KEY_CANCEL) {
        ser_putc(CAN);
        ser_putc(CAN);
        ser_close();
        fprintf(stderr, "xmodem: stopped\n");
        exit(0);
    }
}

/* A byte from the port, or -1 after about secs seconds without one. The
 * clock is only looked at when the line has been quiet for a while: reading
 * it takes long enough that bytes coming in could be lost, as the chip holds
 * only three. */
static int get(int secs)
{
    unsigned quiet = 0;
    time_t end = 0;
    int c;

    for (;;) {
        if ((c = ser_getc()) >= 0)
            return c;
        if (++quiet < 2000)
            continue;
        quiet = 0;
        if (end == 0)
            end = time(NULL) + secs;
        else if (time(NULL) >= end)
            return -1;
        cancel_key();
    }
}

/* Waits until the line is quiet, after a damaged block. */
static void purge(void)
{
    while (get(1) >= 0)
        ;
}

static unsigned crc16(const unsigned char *p, unsigned n)
{
    unsigned crc = 0;
    int i;

    while (n--) {
        crc ^= (unsigned)*p++ << 8;
        for (i = 0; i < 8; i++)
            crc = crc & 0x8000 ? crc << 1 ^ 0x1021 : crc << 1;
    }
    return crc & 0xFFFF;      /* for a host whose unsigned is more than 16 bits */
}

static unsigned char checksum(const unsigned char *p, unsigned n)
{
    unsigned char s = 0;

    while (n--)
        s += *p++;
    return s;
}

/* Two CANs in a row are the other end giving up. */
static int cancelled(int c)
{
    if (c == CAN && get(2) == CAN) {
        fprintf(stderr, "xmodem: the other end stopped\n");
        return 1;
    }
    return 0;
}

static void send_file(const char *name, int big)
{
    unsigned char *data = block + 3;
    unsigned size, i, n, blockno = 1;
    unsigned long sent = 0;
    int c, tries;
    time_t end;
    FILE *f;

    if ((f = fopen(name, "rb")) == NULL) {
        warn(name, errno);
        return;
    }
    printf("xmodem: waiting for the other end to receive %s\n", name);
    end = time(NULL) + WAIT;
    for (;;) {
        c = get(5);
        if (c == 'C' || c == NAK)
            break;
        if (cancelled(c) || time(NULL) >= end) {
            if (c != CAN)
                fprintf(stderr, "xmodem: the other end didn't start\n");
            fclose(f);
            return;
        }
    }
    crc_mode = c == 'C';
    if (!crc_mode)
        big = 0;        /* 1 KB blocks need the CRC */
    for (;;) {
        size = big ? 1024 : 128;
        if ((n = fread(data, 1, size, f)) == 0)
            break;
        if (n <= 128)
            size = 128;
        memset(data + n, SUB, size - n);
        block[0] = size == 1024 ? STX : SOH;
        block[1] = (unsigned char)blockno;
        block[2] = (unsigned char)~blockno;
        if (crc_mode) {
            i = crc16(data, size);
            data[size] = (unsigned char)(i >> 8);
            data[size + 1] = (unsigned char)i;
        } else
            data[size] = checksum(data, size);
        for (tries = 0;; tries++) {
            if (tries == TRIES) {
                fprintf(stderr, "xmodem: block %u wasn't taken after %d tries\n", blockno, TRIES);
                ser_putc(CAN);
                ser_putc(CAN);
                fclose(f);
                return;
            }
            for (i = 0; i < 3 + size + 1 + crc_mode; i++)
                ser_putc(block[i]);
            /* anything else, NAK or nothing, sends it again */
            if ((c = get(10)) == ACK)
                break;
            if (cancelled(c)) {
                fclose(f);
                return;
            }
        }
        sent += n;
        blockno++;
        printf("\r%lu bytes", sent);
    }
    fclose(f);
    for (tries = 0; tries < TRIES; tries++) {
        ser_putc(EOT);
        if ((c = get(10)) == ACK) {
            printf("\rxmodem: sent %s, %lu bytes\n", name, sent);
            return;
        }
    }
    fprintf(stderr, "\nxmodem: the end of the file wasn't taken\n");
}

/* Reads the rest of a block whose first byte was c. Returns its size, 0 if
 * it was damaged, or -1 if the line went quiet. */
static int get_block(int c)
{
    unsigned size = c == STX ? 1024 : 128, n = 3 + size + 1 + crc_mode, i;
    int d;

    block[0] = (unsigned char)c;
    for (i = 1; i < n; i++) {
        if ((d = get(2)) < 0)
            return -1;
        block[i] = (unsigned char)d;
    }
    if (block[1] != (unsigned char)~block[2])
        return 0;
    if (crc_mode ? crc16(block + 3, size) != ((unsigned)block[3 + size] << 8 | block[4 + size])
                 : checksum(block + 3, size) != block[3 + size])
        return 0;
    return (int)size;
}

/* Writes the data, keeping back any ^Zs at the end until more comes: then
 * they were part of the file. */
static int write_data(FILE *f, const unsigned char *p, unsigned n, int text)
{
    unsigned keep = n;

    if (text) {
        while (keep > 0 && p[keep - 1] == SUB)
            keep--;
        if (keep > 0)
            for (; held > 0; held--)
                if (putc(SUB, f) == EOF)
                    return -1;
        held += n - keep;
    }
    return fwrite(p, 1, keep, f) == keep ? 0 : -1;
}

static void receive_file(const char *name, int text)
{
    unsigned char expect = 1;
    unsigned long got = 0;
    int c, size, errors = 0, starts = 0;
    FILE *f;

    if ((f = fopen(name, "wb")) == NULL) {
        warn(name, errno);
        return;
    }
    printf("xmodem: waiting for the other end to send %s\n", name);
    /* ask for CRCs, and after a few tries settle for checksums */
    crc_mode = 1;
    for (;;) {
        if (starts == 6)
            crc_mode = 0;
        ser_putc(crc_mode ? 'C' : NAK);
        c = get(3);
        if (c == SOH || c == STX || c == EOT)
            break;
        if (cancelled(c) || ++starts == WAIT / 3) {
            if (c != CAN)
                fprintf(stderr, "xmodem: the other end didn't start\n");
            fclose(f);
            return;
        }
    }
    for (;;) {
        if (c == EOT) {
            ser_putc(ACK);
            break;
        }
        if (c == SOH || c == STX) {
            size = get_block(c);
            if (size > 0 && block[1] == expect) {
                if (write_data(f, block + 3, size, text)) {
                    warn(name, errno);
                    ser_putc(CAN);
                    ser_putc(CAN);
                    fclose(f);
                    return;
                }
                got += size;
                expect++;
                errors = 0;
                printf("\r%lu bytes", got);
                ser_putc(ACK);
            } else if (size > 0 && block[1] == (unsigned char)(expect - 1)) {
                ser_putc(ACK);      /* our ACK was lost, and it came again */
            } else if (size > 0) {
                fprintf(stderr, "\nxmodem: block %u came out of order\n", block[1]);
                ser_putc(CAN);
                ser_putc(CAN);
                fclose(f);
                return;
            } else {
                purge();
                ser_putc(NAK);
                errors++;
            }
        } else if (cancelled(c)) {
            fclose(f);
            return;
        } else {
            if (c >= 0)
                purge();
            ser_putc(NAK);
            errors++;
        }
        if (errors == TRIES) {
            fprintf(stderr, "\nxmodem: too many damaged blocks\n");
            ser_putc(CAN);
            ser_putc(CAN);
            fclose(f);
            return;
        }
        c = get(10);
    }
    if (fclose(f) != 0)
        warn(name, errno);
    else
        printf("\rxmodem: received %s, %lu bytes\n", name, got - (text ? held : 0));
}

int main(int argc, char **argv)
{
    const char *use = "-s|-r [-k] [-t] [-p A|B] [-b baud] file";
    int c, sending = 0, receiving = 0, big = 0, text = 0, port = SER_A;
    unsigned baud = 9600;

    tool_start("xmodem", FORM, &argc, &argv);
    while ((c = getopt(argc, argv, "srktp:b:")) != -1) {
        switch (c) {
        case 's': sending = 1; break;
        case 'r': receiving = 1; break;
        case 'k': big = 1; break;
        case 't': text = 1; break;
        case 'p':
            if (optarg[0] == 'b' || optarg[0] == 'B')
                port = SER_B;
            else if (optarg[0] != 'a' && optarg[0] != 'A')
                usage(use);
            break;
        case 'b': baud = (unsigned)atol(optarg); break;
        default: usage(use);
        }
    }
    if (argc - optind != 1 || sending == receiving || baud < 300 || baud > 19200)
        usage(use);
    if (ser_open(port, baud)) {
        warn(port == SER_A ? "port A" : "port B", errno);
        return tool_end();
    }
    while (ser_getc() >= 0)     /* anything left from before */
        ;
    if (sending)
        send_file(argv[optind], big);
    else
        receive_file(argv[optind], text);
    ser_close();
    return tool_end();
}
