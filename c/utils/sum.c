/* sum - the CRC-32 of each file, to check a copy or a transfer.
 *
 *   sum file... [>out]
 *
 * It's the CRC-32 of zip, Ethernet and PNG, so a PC's tools give the same
 * value for the same bytes: 7-Zip's CRC-32, Python's zlib.crc32, or crc32 on
 * Linux. Each line is the CRC in hex, the size in bytes, and the name. */
#include <errno.h>
#include <stdio.h>
#include "util.h"

/* the command form, for tool_start and tools/mksub.py */
#define FORM "Show the CRC-32 of files|" \
    "File list|[Output file] >"

static unsigned long table[256];
static char buf[4096];

static void make_table(void)
{
    unsigned long c;
    int i, k;

    for (i = 0; i < 256; i++) {
        c = i;
        for (k = 0; k < 8; k++)
            c = c & 1 ? 0xEDB88320UL ^ (c >> 1) : c >> 1;
        table[i] = c;
    }
}

static int sum(const char *name, void *ctx)
{
    unsigned long crc = 0xFFFFFFFFUL, size = 0;
    size_t n, i;
    FILE *f;

    (void)ctx;
    if ((f = fopen(name, "rb")) == NULL) {
        warn(name, errno);
        return 0;
    }
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) {
        for (i = 0; i < n; i++)
            crc = table[(crc ^ (unsigned char)buf[i]) & 0xFF] ^ (crc >> 8);
        size += n;
    }
    if (ferror(f))
        warn(name, errno);
    else
        say("%08lX %9lu  %s\n", crc ^ 0xFFFFFFFFUL, size, name);
    fclose(f);
    return 0;
}

int main(int argc, char **argv)
{
    int i;

    tool_start("sum", FORM, &argc, &argv);
    if (argc < 2)
        usage("file...");
    make_table();
    for (i = 1; i < argc; i++)
        each_file(argv[i], sum, NULL);
    return tool_end();
}
