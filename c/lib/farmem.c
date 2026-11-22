/* farmem.c - far blocks from CTOS short-lived memory. A table keeps them in
 * allocation order, so freed blocks can go back to CTOS last first. */
#include <ctos.h>
#include <errno.h>
#include <farmem.h>

#define MAXBLOCKS 32

static struct {
    void __far *p;
    unsigned size;
    int freed;
} blocks[MAXBLOCKS];
static int nblocks;

void __far *farmalloc(unsigned size)
{
    void __far *p;
    ErcType erc;

    if (size == 0 || size > 65520u || nblocks == MAXBLOCKS)
        return 0;
    if ((erc = AllocMemorySL(size, &p)) != 0) {
        errno = erc;
        return 0;
    }
    blocks[nblocks].p = p;
    blocks[nblocks].size = size;
    blocks[nblocks].freed = 0;
    nblocks++;
    return p;
}

void farfree(void __far *p)
{
    int i;

    for (i = nblocks - 1; i >= 0; i--)
        if (blocks[i].p == p && !blocks[i].freed)
            break;
    if (i < 0)
        return;
    blocks[i].freed = 1;
    while (nblocks > 0 && blocks[nblocks - 1].freed) {
        nblocks--;
        DeallocMemorySL(blocks[nblocks].p, blocks[nblocks].size);
    }
}

unsigned long farcoreleft(void)
{
    unsigned paras;

    return QueryMemAvail(&paras) == 0 ? (unsigned long)paras * 16 : 0;
}

void __far *_fmemcpy(void __far *d, const void __far *s, unsigned n)
{
    char __far *dp = d;
    const char __far *sp = s;

    while (n--)
        *dp++ = *sp++;
    return d;
}

void __far *_fmemmove(void __far *d, const void __far *s, unsigned n)
{
    char __far *dp = d;
    const char __far *sp = s;

    /* compare as linear addresses: the pointers may use different segments */
    unsigned long dl = ((unsigned long)FP_SEG(dp) << 4) + FP_OFF(dp);
    unsigned long sl = ((unsigned long)FP_SEG(sp) << 4) + FP_OFF(sp);

    if (dl < sl) {
        while (n--)
            *dp++ = *sp++;
    } else {
        dp += n;
        sp += n;
        while (n--)
            *--dp = *--sp;
    }
    return d;
}

void __far *_fmemset(void __far *d, int c, unsigned n)
{
    char __far *dp = d;

    while (n--)
        *dp++ = (char)c;
    return d;
}

int _fmemcmp(const void __far *a, const void __far *b, unsigned n)
{
    const unsigned char __far *p = a, __far *q = b;

    for (; n; n--, p++, q++)
        if (*p != *q)
            return *p - *q;
    return 0;
}
