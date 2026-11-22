/* sysinfo - what CTOS this is, and what it's running on.
 *
 *   sysinfo [-v] [>out]
 *
 * It shows the CTOS version and boot device, the processor, the date, the
 * path, the memory free and the screen, then each disk volume, from its
 * Volume Home Block. -v adds each volume's home block in detail and CTOS's
 * System Common table.
 *
 * System Common is a table of far pointers at 0:240h, in vectors CTOS
 * doesn't use, through which its own programs find its structures. The
 * names are those in CTOS's SysCom.Edf. The version string and boot device
 * name, which this reads, are the same on the AWS's CTOS 9 and the NGEN's
 * CTOS II; the structures the others point to differ between versions. */
#include <ctos.h>
#include <farmem.h>
#include <screen.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "util.h"

/* the command form, for tool_start and tools/mksub.py */
#define FORM "Show the CTOS version, memory and disk volumes|" \
    "[Details?] -v|[Output file] >"

/* System Common's entries, from 240h, four bytes each; 2F4h is reserved */
#define SYSCOM        0x240
#define VERSION_NO    0x254     /* the two entries this reads */
#define BOOT_DEV_NAME 0x2B8
static const char *const syscom[] = {
    "pSysTime", "pVcb", "pRgSysError", "pOPcbRun", "pParamBlk", "pVersionNo",
    "pIvWord", "pDSWord", "pComIntRet", "pMsgWaitCount", "pMsgWaitFree", "pMsgWaitReserve",
    "pRgInfoIKey", "pORgExchg", "pORgMsgWait", "pORgODcb", "pORgOFcb", "pORgOUcb",
    "pORgOVhb", "pORgPcb", "pORgPTiming", "pRgRqExchgSys", "pRgRqExchgUsr", "pRunQ",
    "pSaSemiHighest", "pSaSemiLowest", "pSaTempHighest", "pSaTempLowest", "pSaEndMemory",
    "pBootBlock", "pBootDevName", "pCrashFileDevName", "pHdcCntlBlk", "pFloppyCntlBlk",
    "pConfiguration", "pIoErrorLog", "pPerformance", "pSysDeviceNum",
    "pRgLocalServiceCodeSys", "pRgLocalServiceCodeUsr", "pContingencyLog", "pExtCntlRegAws",
    "pGraphicsPort", "pFontRamBuf", "pHdwState", NULL, "prgpVidMemLine", "pExSCAT",
};

static void __far *sys_ptr(unsigned at)
{
    void __far * __far *p = MK_FP(0, at);

    return *p;
}

/* A string with its length in its first byte, from System Common */
static char *sys_sb(unsigned at, char *buf, unsigned size)
{
    const unsigned char __far *p = sys_ptr(at);
    unsigned n, i;

    if (p == 0)
        return strcpy(buf, "?");
    n = p[0] < size ? p[0] : size - 1;
    for (i = 0; i < n; i++)
        buf[i] = p[1 + i] >= ' ' && p[1 + i] < 0x7F ? p[1 + i] : '.';
    buf[n] = 0;
    return buf;
}

/* The 8086 shifts by the whole count, so 1 << 33 is 0; the 80186 and later
 * take the count mod 32, giving 2. The 8086 and 80186 push SP as it is after
 * the push; the 80286 and later push it as it was. */
extern unsigned shift33(void);
#pragma aux shift33 = "mov ax,1" "mov cl,33" "shl ax,cl" value [ax] modify [cx];
extern unsigned push_sp(void);
#pragma aux push_sp = "push sp" "pop ax" "sub ax,sp" value [ax];

static const char *cpu(void)
{
    if (shift33() == 0)
        return "8086 or 8088";
    return push_sp() == 0 ? "80286 or later" : "80186 or 80188";
}

static int count_dir(const char *name, void *ctx)
{
    (void)name;
    ++*(unsigned *)ctx;
    return 0;
}

static void volumes(int verbose)
{
    static const char *const devs[] = {
        "[d0]", "[d1]", "[d2]", "[d3]", "[f0]", "[f1]", "[f2]", "[f3]"
    };
    char made[20], changed[20], size[12];
    unsigned i, dirs;
    Volume v;

    say("\n%-5s %-12s %9s %9s %6s %5s  %-10s  %s\n", "dev", "volume", "KB total", "KB free",
        "files", "dirs", "made", "changed");
    for (i = 0; i < sizeof devs / sizeof devs[0]; i++) {
        if (get_volume(devs[i], &v) != 0)
            continue;
        dirs = 0;
        each_dir(devs[i], count_dir, &dirs);
        if (v.totalPages)
            sprintf(size, "%lu", v.totalPages / 2);
        else
            strcpy(size, "?");
        date_str(v.created, made)[10] = 0;     /* the day is enough here */
        date_str(v.modified, changed)[10] = 0;
        say("%-5s %-12s %9s %9lu %6u %5u  %-10s  %s\n", devs[i], v.name, size,
            v.freePages / 2, headers_used(&v), dirs, made, changed);
        if (verbose) {
            say("       file headers %u pages (alternates every %u), %u free\n",
                v.headerPages, v.altOffset, v.freeHeaders);
            say("       pages: master directory %u, system image %u, crash dump %u, log %u\n",
                v.mfdPages, v.sysImagePages, v.crashPages, v.logPages);
        }
    }
}

int main(int argc, char **argv)
{
    char buf[40];
    unsigned paras, i, n;
    unsigned long dt;
    void __far *p;
    int c, verbose = 0;

    tool_start("sysinfo", FORM, &argc, &argv);
    while ((c = getopt(argc, argv, "v")) != -1) {
        if (c == 'v')
            verbose = 1;
        else
            usage("[-v]");
    }
    say("CTOS         %s\n", sys_sb(VERSION_NO, buf, sizeof buf));
    say("Booted from  [%s]\n", sys_sb(BOOT_DEV_NAME, buf, sizeof buf));
    say("Processor    %s\n", cpu());
    if (GetDateTime(&dt) == 0)
        say("Date         %s\n", date_str(dt, buf));
    say("Path         %s\n", getcwd(buf, sizeof buf) ? buf : "?");
    if (QueryMemAvail(&paras) == 0)
        say("Memory       %lu KB free for programs\n", (unsigned long)paras * 16 / 1024);
    say("Screen       %d lines of %d\n", scr_rows(), SCR_COLS);
    volumes(verbose);
    if (verbose) {
        say("\nSystem Common, at 0000:%04X\n", SYSCOM);
        for (i = n = 0; i < sizeof syscom / sizeof syscom[0]; i++) {
            if (syscom[i] == NULL)
                continue;
            p = sys_ptr(SYSCOM + 4 * i);
            say("%03X %-23s %04X:%04X%s", SYSCOM + 4 * i, syscom[i], FP_SEG(p), FP_OFF(p),
                ++n % 2 ? "   " : "\n");
        }
        say(n % 2 ? "\n" : "");
    }
    return tool_end();
}
