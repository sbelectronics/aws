/* serial.c - the RS-232 ports, polled.
 *
 * The serial chip is an NEC 7201 (Intel 8274). A write register is set by
 * writing its number to the command port, then the value; reading the
 * command port gives RR0, where bit 0 says a byte has come and bit 2 that
 * the transmitter can take one. */
#include <ctosmsg.h>
#include <errno.h>
#include <serial.h>

/* in serio.asm */
unsigned ser_inb(unsigned port);
void ser_outb(unsigned port, unsigned v);
void __far *ser_isr_init(void);     /* the interrupt routines, for */
void __far *ser_isr_set(void);      /* InitCommLine and SetCommISR */

/* On the AWS's CTOS */
extern ErcType __far SetCommISR(unsigned iLine, void __far *pDS, void __far *pTxIsr,
                                void __far *pExtIsr, void __far *pRxIsr, void __far *pSpRxIsr);
extern ErcType __far ResetCommISR(unsigned iLine);
#pragma aux (CTOSCALL) SetCommISR;
#pragma aux (CTOSCALL) ResetCommISR;

#define rqInitCommLine  0x1006
#define rqResetCommLine 0x1007

/* InitCommLine's own errors: anything else means this CTOS doesn't have it
 * (the AWS's returns 6) */
#define COMMLINE_ERC(e) (((e) >= 60 && (e) <= 70) || (e) == 8400 || (e) == 8401)

#define RR0_RX  0x01
#define RR0_TX  0x04

/* The AWS's ports (hardware manual p. 2-91), and its baud rate counters:
 * an 8253 at 1,228,800 Hz, counter 2 for port A and 1 for port B. */
#define AWS_DATA(p)     (0xA8 + 2 * (p))
#define AWS_CMD(p)      (0xA9 + 2 * (p))
#define AWS_COUNTER(p)  (0xAE - (p))
#define AWS_TIMERMODE   0xAF
#define AWS_COUNTERHZ   1228800UL

static int line = -1, viaInit;
static unsigned cmd, data, handle;

#pragma pack(push, 1)
static struct {             /* InitCommLine's Communications Line Control Block */
    void __far *pDsBx;
    void __far *pTxIsr, __far *pExtIsr, __far *pRxIsr, __far *pSpRxIsr;
    unsigned char fRawTx, fRawExt, fRawRx, fRawSpRx;
    unsigned baudTx, baudRx;
    unsigned char fNRZI;
} clcb;
static struct {
    unsigned handle, ioCommCtl, ioCommData;
} ret;
#pragma pack(pop)

static void wr(unsigned reg, unsigned v)
{
    ser_outb(cmd, reg);
    ser_outb(cmd, v);
}

static ErcType init_comm_line(int port, unsigned baud)
{
    static char spec[] = "[comm]a";
#pragma pack(push, 1)
    struct {
        RqHeader h;
        unsigned char cnt[6];
        const char __far *pbSpec;
        unsigned cbSpec;
        void __far *pbClcb;
        unsigned cbClcb;
        void __far *pbRet;
        unsigned cbRet;
    } rq = { { 6, 0, 2, 1, 0, 0, 0, rqInitCommLine } };
#pragma pack(pop)
    ErcType erc;

    spec[6] = (char)('a' + port);
    clcb.pDsBx = &ret;
    clcb.pTxIsr = clcb.pExtIsr = clcb.pRxIsr = clcb.pSpRxIsr = ser_isr_init();
    clcb.baudTx = clcb.baudRx = baud;
    rq.pbSpec = spec;
    rq.cbSpec = sizeof spec - 1;
    rq.pbClcb = &clcb;
    rq.cbClcb = sizeof clcb;
    rq.pbRet = &ret;
    rq.cbRet = sizeof ret;
    if ((erc = DoRequest(&rq)) == 0) {
        handle = ret.handle;
        cmd = ret.ioCommCtl;
        data = ret.ioCommData;
    }
    return erc;
}

static ErcType set_comm_isr(int port, unsigned baud)
{
    unsigned count = (unsigned)(AWS_COUNTERHZ / 16 / baud);
    void __far *isr;
    ErcType erc;

    isr = ser_isr_set();
    if ((erc = SetCommISR(port, &ret, isr, isr, isr, isr)) != 0)
        return erc;
    cmd = AWS_CMD(port);
    data = AWS_DATA(port);
    /* counter, low then high byte, square wave, binary */
    ser_outb(AWS_TIMERMODE, port == SER_A ? 0xB6 : 0x76);
    ser_outb(AWS_COUNTER(port), count & 0xFF);
    ser_outb(AWS_COUNTER(port), count >> 8);
    ser_outb(cmd, 0x18);                    /* channel reset */
    return 0;
}

int ser_open(int port, unsigned baud)
{
    ErcType erc;

    if (line >= 0 || (port != SER_A && port != SER_B) || baud < 50)
        return -1;
    erc = init_comm_line(port, baud);
    viaInit = erc == 0;
    if (erc && !COMMLINE_ERC(erc))
        erc = set_comm_isr(port, baud);
    if (erc) {
        errno = erc;
        return -1;
    }
    line = port;
    wr(4, 0x44);    /* clock x16, 1 stop bit, no parity */
    wr(3, 0xC1);    /* receive 8 bits, receiver on */
    wr(5, 0xEA);    /* DTR, transmit 8 bits, transmitter on, RTS */
    wr(1, 0x04);    /* no interrupts; status affects vector, as CTOS asks */
    return 0;
}

int ser_putc(int c)
{
    if (line < 0)
        return -1;
    while (!(ser_inb(cmd) & RR0_TX))
        ;
    ser_outb(data, (unsigned char)c);
    return (unsigned char)c;
}

int ser_puts(const char *s)
{
    for (; *s; s++)
        if (ser_putc(*s) < 0)
            return -1;
    return 0;
}

int ser_getc(void)
{
    if (line < 0 || !(ser_inb(cmd) & RR0_RX))
        return -1;
    return (int)(ser_inb(data) & 0xFF);
}

void ser_close(void)
{
#pragma pack(push, 1)
    struct {
        RqHeader h;
        unsigned handle;
    } rq = { { 2, 0, 0, 0, 0, 0, 0, rqResetCommLine } };
#pragma pack(pop)

    if (line < 0)
        return;
    wr(5, 0x00);
    if (viaInit) {
        rq.handle = handle;
        DoRequest(&rq);
    } else {
        ResetCommISR(line);
    }
    line = -1;
}
