/* farmem.h - memory outside the program's 64 KB segment, from CTOS's
 * short-lived memory. Each block is its own segment, at offset 0, of up to
 * 65520 bytes, and goes when the program ends.
 *
 * CTOS frees short-lived memory only in the reverse order of allocation, so
 * farfree gives a block back to CTOS once every block allocated after it has
 * been freed too. Free blocks in that order to get the memory back at once. */
#ifndef FARMEM_H
#define FARMEM_H

#define MK_FP(seg, off) ((void __far *)(((unsigned long)(seg) << 16) | (unsigned)(off)))
#define FP_SEG(p)       ((unsigned)((unsigned long)(void __far *)(p) >> 16))
#define FP_OFF(p)       ((unsigned)(unsigned long)(void __far *)(p))

/* returns 0 (not NULL, which is a near pointer) if there's no memory */
void __far *farmalloc(unsigned size);
void farfree(void __far *p);
unsigned long farcoreleft(void);   /* bytes CTOS could still allocate */

void __far *_fmemcpy(void __far *d, const void __far *s, unsigned n);
void __far *_fmemmove(void __far *d, const void __far *s, unsigned n);
void __far *_fmemset(void __far *d, int c, unsigned n);
int _fmemcmp(const void __far *a, const void __far *b, unsigned n);

#endif
