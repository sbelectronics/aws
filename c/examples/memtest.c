/* memtest.c - checks that CTOS gives a program the memory above its run file.
 *
 * The run file holds only the code and initialised data; the header's
 * cparMinAlloc asks for the rest of the segment, for _BSS and the stack (see
 * tools/mkrun.py). This takes every block of short-lived memory CTOS will
 * give, and checks that none of it overlaps that part of the segment. It
 * prints "memtest: N blocks, 0 overlap". */
#include <ctos.h>
#include <farmem.h>
#include <stdio.h>

static char bss[100];

int main(void)
{
    static const unsigned sizes[] = { 16384, 1024, 16 };
    unsigned seg = FP_SEG((void __far *)bss);
    unsigned top = *(unsigned *)2;      /* crt0 keeps the stack top there */
    unsigned long lo = (unsigned long)seg * 16, hi = lo + top, at;
    void __far *p;
    int i, blocks = 0, overlaps = 0, nonzero = 0;

    for (i = 0; i < sizeof bss; i++)
        nonzero += bss[i] != 0;
    printf("segment %04X, stack top %04X, %d bytes of _BSS not zero\n", seg, top, nonzero);
    for (i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        while (AllocMemorySL(sizes[i], &p) == 0) {
            at = (unsigned long)FP_SEG(p) * 16;
            blocks++;
            if (at < hi && at + sizes[i] > lo) {
                overlaps++;
                printf("overlap: %04X:0000, %u bytes\n", FP_SEG(p), sizes[i]);
            }
        }
    }
    printf("memtest: %d blocks, %d overlap\n", blocks, overlaps);
    return 0;
}
