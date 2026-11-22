/* hexedit.c - a full-screen hex editor for CTOS.
 *
 *   hexedit [file]
 *
 * The file stays on disk. The editor keeps the document as a list of pieces,
 * each a run of bytes from the file or from the edit buffer in memory, so a
 * file can be larger than memory and an edit costs a few pieces. Undo keeps
 * the pieces each edit replaced, and the ones it put in. Saving writes the
 * changed pages in place when no byte has moved; otherwise it writes a new
 * file and renames it over the old one.
 */
#include <ctos.h>
#include <errno.h>
#include <farmem.h>
#include <screen.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef unsigned long Pos;

#define PAGE    512u
#define NOBLOCK 0xFFFFFFFFUL    /* an empty cache slot */
#define ADDED   0x80000000UL    /* in a piece's start: its bytes are in the edit buffer */

typedef struct {
    Pos start, len;
} Piece;

/* the document */
static Piece __far *pieces;
static unsigned npieces, maxpieces;
static Pos docsize;
static unsigned char __far *addbuf;
static unsigned addlen, addmax;

/* the file */
static char fname[80];
static unsigned fh;
static int haveFile, readOnly;
static Pos filesize;
static ErcType ioerc;           /* the last error reading the file */

/* The file is read a block at a time, as a floppy is much quicker at one
 * long read than at several short ones. */
#define IOBLOCK 4096u
#define NCACHE  6
static unsigned char __far *cache;
static Pos cacheblock[NCACHE];
static unsigned cacheuse[NCACHE], cacheclock;
static unsigned char __far *iobuf;      /* IOBLOCK bytes, for saving */

/* the screen */
#define MAXROWS 40
#define HEXCOL(i) (12 + 3 * (i) + ((i) >= 8))
#define TEXTCOL(i) (63 + (i))
#define BAR1 10                 /* the vertical lines */
#define BAR2 61
#define MAXRUNS 16              /* attribute runs the AWS can show on a line */
static int rows, ndata, rowRule, rowStatus, rowMsg, rowKeys;
static char rc[SCR_COLS + 1];
static unsigned char ra[SCR_COLS];
static char shc[MAXROWS][SCR_COLS];
static unsigned char sha[MAXROWS][SCR_COLS];
static char shok[MAXROWS];

/* editing */
static Pos cur, top, mark;
static int nib, textSide, insertMode, marking, amendOK;
static char message[SCR_COLS + 1];
static char findbuf[61], gotobuf[24];
static unsigned char pat[32];
static unsigned patlen;

#define A_SEL     ATTR_REVERSE
#define A_CUR     ATTR_REVERSE
#define A_CUR2    (ATTR_REVERSE | ATTR_HALFBRIGHT)     /* the cursor on the other side */
#define A_CHANGED ATTR_UNDERLINE

static void msg(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(message, sizeof message, fmt, ap);
    va_end(ap);
}

/* Reading the file */

/* The cached copy of the file from at to the end of its block */
static unsigned char __far *file_at(Pos at)
{
    Pos block = at & ~(Pos)(IOBLOCK - 1);
    unsigned i, old = 0, cb = 0, n, in = (unsigned)at & (IOBLOCK - 1);
    unsigned char __far *p;
    ErcType erc;

    for (i = 0; i < NCACHE; i++) {
        if (cacheblock[i] == block) {
            cacheuse[i] = ++cacheclock;
            return cache + i * IOBLOCK + in;
        }
        if (cacheuse[i] < cacheuse[old])
            old = i;
    }
    p = cache + old * IOBLOCK;
    /* reading past the file's last page is an error, not end of file */
    n = IOBLOCK;
    if (filesize < block + IOBLOCK)
        n = filesize > block ? ((unsigned)(filesize - block) + PAGE - 1) & ~(PAGE - 1) : 0;
    erc = n ? Read(fh, p, n, block, &cb) : 0;
    if (erc != 0 && erc != ercEOF) {
        ioerc = erc;
        cb = 0;
        cacheblock[old] = NOBLOCK;   /* try again next time */
    } else {
        cacheblock[old] = block;
        cacheuse[old] = ++cacheclock;
    }
    _fmemset(p + cb, 0, IOBLOCK - cb);
    return p + in;
}

static void cache_clear(void)
{
    unsigned i;

    for (i = 0; i < NCACHE; i++)
        cacheblock[i] = NOBLOCK;
}

/* The pieces */

static unsigned lastI;          /* find_piece's last answer, where the next one starts looking */
static Pos lastS;

/* the piece holding pos (< docsize), and where it starts */
static unsigned find_piece(Pos pos, Pos *start)
{
    unsigned i = 0;
    Pos s = 0;

    if (pos >= lastS && lastI < npieces) {
        i = lastI;
        s = lastS;
    }
    while (i < npieces && s + pieces[i].len <= pos)
        s += pieces[i++].len;
    lastI = i;
    lastS = s;
    *start = s;
    return i;
}

static int piece_byte(const Piece __far *pc, Pos off)
{
    Pos at = pc->start + off;

    if (pc->start & ADDED)
        return addbuf[(unsigned)(at & ~ADDED)];
    return *file_at(at);
}

typedef struct {
    unsigned i;
    Pos off;
    int added;
} Iter;

static void it_seek(Iter *it, Pos pos)
{
    Pos s;

    it->off = 0;
    if (pos >= docsize) {
        it->i = npieces;
        return;
    }
    it->i = find_piece(pos, &s);
    it->off = pos - s;
}

/* Points *p at the bytes from the iterator on that lie together in memory,
 * at most max of them, moves past them and returns how many: 0 at the end.
 * They stay put only until the next read of the file. */
static unsigned it_span(Iter *it, const unsigned char __far **p, unsigned max)
{
    const Piece __far *pc;
    Pos at, left;
    unsigned n;

    while (it->i < npieces && it->off >= pieces[it->i].len) {
        it->i++;
        it->off = 0;
    }
    if (it->i >= npieces)
        return 0;
    pc = &pieces[it->i];
    at = pc->start + it->off;
    left = pc->len - it->off;
    if (pc->start & ADDED) {
        *p = addbuf + (unsigned)(at & ~ADDED);
        n = max;
    } else {
        *p = file_at(at);
        n = IOBLOCK - ((unsigned)at & (IOBLOCK - 1));
        if (n > max)
            n = max;
    }
    if (n > left)
        n = (unsigned)left;
    it->added = (pc->start & ADDED) != 0;
    it->off += n;
    return n;
}

/* the next byte, or -1 at the end */
static int it_next(Iter *it)
{
    const unsigned char __far *p;

    return it_span(it, &p, 1) ? *p : -1;
}

/* Copies n bytes from the iterator on to dst. */
static void it_copy(Iter *it, unsigned char __far *dst, unsigned n)
{
    const unsigned char __far *p;
    unsigned m;

    while (n && (m = it_span(it, &p, n)) != 0) {
        _fmemcpy(dst, p, m);
        dst += m;
        n -= m;
    }
}

static int byte_at(Pos pos)
{
    Iter it;

    it_seek(&it, pos);
    return it_next(&it);
}

/* Makes a piece start at pos and returns its index. */
static unsigned split(Pos pos)
{
    unsigned i;
    Pos s, n;

    if (pos >= docsize)
        return npieces;
    i = find_piece(pos, &s);
    if (s == pos)
        return i;
    n = pos - s;
    _fmemmove(pieces + i + 1, pieces + i, (npieces - i) * sizeof(Piece));
    npieces++;
    pieces[i].len = n;
    pieces[i + 1].start += n;
    pieces[i + 1].len -= n;
    return i + 1;
}

/* Joins piece i to the one before it if they continue each other. */
static void join(unsigned i)
{
    Piece __far *p, __far *q;

    if (i == 0 || i >= npieces)
        return;
    p = &pieces[i - 1];
    q = &pieces[i];
    if (((p->start ^ q->start) & ADDED) == 0 && p->start + p->len == q->start) {
        p->len += q->len;
        _fmemmove(q, q + 1, (npieces - i - 1) * sizeof(Piece));
        npieces--;
    }
}

/* Undo. The undo area is a stack of records: a header, the pieces an edit took
 * out, the pieces it put in, and the record's size, so the stack can be
 * walked down. Records above utop, up to uend, are the ones to redo. */

typedef struct {
    Pos pos;
    unsigned nrem, nins;
} UndoHdr;

#define NEVER (-30000)          /* a saved level that can't come back */

static unsigned char __far *undo;
static unsigned utop, uend, umax;
static int level, savedLevel;   /* records below utop, and what that was at the last save */

static int changed(void)
{
    return level != savedLevel;
}

static unsigned rec_size(const UndoHdr *h)
{
    return sizeof *h + (h->nrem + h->nins) * sizeof(Piece) + 2;
}

static void undo_push(Pos pos, const Piece __far *rem, unsigned nrem,
                      const Piece __far *ins, unsigned nins)
{
    UndoHdr h;
    unsigned long need = sizeof h + ((unsigned long)nrem + nins) * sizeof(Piece) + 2;
    unsigned size, first;
    unsigned char __far *p;

    if (savedLevel > level)     /* the saved state was among the redos */
        savedLevel = NEVER;
    uend = utop;
    if (need > umax) {          /* too big to undo: forget everything */
        utop = uend = 0;
        level = 0;
        savedLevel = NEVER;
        return;
    }
    size = (unsigned)need;
    while (umax - utop < size) {    /* forget the oldest */
        _fmemcpy(&h, undo, sizeof h);
        first = rec_size(&h);
        _fmemmove(undo, undo + first, utop - first);
        utop -= first;
        level--;
        if (savedLevel != NEVER && --savedLevel < 0)
            savedLevel = NEVER;
    }
    h.pos = pos;
    h.nrem = nrem;
    h.nins = nins;
    p = undo + utop;
    _fmemcpy(p, &h, sizeof h);
    p += sizeof h;
    _fmemcpy(p, rem, nrem * sizeof(Piece));
    p += nrem * sizeof(Piece);
    _fmemcpy(p, ins, nins * sizeof(Piece));
    p += nins * sizeof(Piece);
    *(unsigned __far *)p = size;
    utop = uend = utop + size;
    level++;
}

/* The second hex digit of a byte replaces what the first one put in, in the
 * same undo record. */
static void undo_amend(const Piece __far *pc)
{
    UndoHdr h;
    unsigned char __far *r;

    if (utop == 0)
        return;
    r = undo + utop - *(unsigned __far *)(undo + utop - 2);
    _fmemcpy(&h, r, sizeof h);
    if (h.nins == 1)
        _fmemcpy(r + sizeof h + h.nrem * sizeof(Piece), pc, sizeof(Piece));
}

/* Editing */

enum { REC_NONE, REC_NEW, REC_AMEND };

static Pos pieces_len(const Piece __far *p, unsigned n)
{
    Pos len = 0;

    while (n--)
        len += p++->len;
    return len;
}

/* Replaces dellen bytes at pos with the pieces ins. */
static int replace(Pos pos, Pos dellen, const Piece __far *ins, unsigned nins, int rec)
{
    unsigned a, b, nrem;

    if (npieces + 2 + nins > maxpieces) {
        msg("Too many changes to keep track of: save the file (f10) first");
        return -1;
    }
    a = split(pos);
    b = split(pos + dellen);
    nrem = b - a;
    if (rec == REC_NEW)
        undo_push(pos, pieces + a, nrem, ins, nins);
    else if (rec == REC_AMEND)
        undo_amend(ins);
    if (nins != nrem)
        _fmemmove(pieces + a + nins, pieces + b, (npieces - b) * sizeof(Piece));
    _fmemcpy(pieces + a, ins, nins * sizeof(Piece));
    npieces = npieces - nrem + nins;
    docsize = docsize - dellen + pieces_len(ins, nins);
    join(a + nins);
    if (nins)
        join(a);
    lastI = 0;
    lastS = 0;
    return 0;
}

static int editable(void)
{
    if (readOnly)
        msg("%s is read only", fname);
    return !readOnly;
}

static int add_bytes(const unsigned char *b, unsigned n, Piece *pc)
{
    if (addmax - addlen < n) {
        msg("The edit buffer is full: save the file (f10) to go on");
        return -1;
    }
    _fmemcpy(addbuf + addlen, b, n);
    pc->start = ADDED | addlen;
    pc->len = n;
    addlen += n;
    return 0;
}

static int put_byte(Pos pos, int b, int insert, int rec)
{
    unsigned char c = (unsigned char)b;
    Piece pc;

    if (add_bytes(&c, 1, &pc))
        return -1;
    return replace(pos, insert || pos >= docsize ? 0 : 1, &pc, 1, rec);
}

static void do_undo(void)
{
    UndoHdr h;
    unsigned size;
    const Piece __far *rem;

    if (utop == 0) {
        msg("Nothing to undo");
        return;
    }
    size = *(unsigned __far *)(undo + utop - 2);
    _fmemcpy(&h, undo + utop - size, sizeof h);
    rem = (const Piece __far *)(undo + utop - size + sizeof h);
    if (replace(h.pos, pieces_len(rem + h.nrem, h.nins), rem, h.nrem, REC_NONE) == 0) {
        utop -= size;
        level--;
        cur = h.pos;
    }
}

static void do_redo(void)
{
    UndoHdr h;
    const Piece __far *rem;

    if (utop >= uend) {
        msg("Nothing to redo");
        return;
    }
    _fmemcpy(&h, undo + utop, sizeof h);
    rem = (const Piece __far *)(undo + utop + sizeof h);
    if (replace(h.pos, pieces_len(rem, h.nrem), rem + h.nrem, h.nins, REC_NONE) == 0) {
        utop += rec_size(&h);
        level++;
        cur = h.pos;
    }
}

/* Blocks */

#define CLIPMAX 200
static Piece clip[CLIPMAX];
static unsigned nclip;
static Pos cliplen;

static void block(Pos *s, Pos *e)
{
    *s = mark < cur ? mark : cur;
    *e = (mark < cur ? cur : mark) + 1;
    if (*e > docsize)
        *e = docsize;
}

static int copy_block(Pos s, Pos e)
{
    unsigned i;
    Pos ps, at, off, n;

    nclip = 0;
    cliplen = 0;
    if (s >= e)
        return 0;
    i = find_piece(s, &ps);
    for (at = s; at < e; ps += pieces[i++].len) {
        if (nclip == CLIPMAX) {
            nclip = 0;
            cliplen = 0;
            msg("The block is in too many pieces to copy: save the file first");
            return -1;
        }
        off = at - ps;
        n = pieces[i].len - off;
        if (n > e - at)
            n = e - at;
        clip[nclip].start = pieces[i].start + off;
        clip[nclip].len = n;
        nclip++;
        cliplen += n;
        at += n;
    }
    return 0;
}

static void delete_bytes(Pos s, Pos e)
{
    if (s < e && editable() && replace(s, e - s, 0, 0, REC_NEW) == 0)
        cur = s;
}

static void paste(void)
{
    Pos s = cur, e = cur;

    if (!editable())
        return;
    if (nclip == 0) {
        msg("Nothing to paste: mark a block with f7 and copy it with f8");
        return;
    }
    if (marking)
        block(&s, &e);
    else if (!insertMode)
        e = cur + cliplen > docsize ? docsize : cur + cliplen;
    if (replace(s, e - s, clip, nclip, REC_NEW) == 0) {
        cur = s + cliplen;
        marking = 0;
        msg("Pasted %lu byte%s", cliplen, cliplen == 1 ? "" : "s");
    }
}

/* The screen */

static void row_begin(void)
{
    memset(rc, ' ', SCR_COLS);
    memset(ra, 0, SCR_COLS);
}

static void row_text(int col, const char *s)
{
    while (*s && col < SCR_COLS)
        rc[col++] = *s++;
}

static void row_set(int col, int n, int attr)
{
    while (n-- > 0 && col < SCR_COLS)
        ra[col++] = (unsigned char)attr;
}

static void row_add(int col, int n, int attr)
{
    while (n-- > 0 && col < SCR_COLS)
        ra[col++] |= (unsigned char)attr;
}

/* The runs of one attribute along the row, counting the special-character
 * set the AWS uses for codes 80h and up as an attribute: an AWS screen line
 * holds 16 runs. */
static int row_runs(void)
{
    int i, n = 1, a, prev = ra[0] | ((unsigned char)rc[0] >= 0x80 ? 0x10 : 0);

    for (i = 1; i < SCR_COLS; i++) {
        a = ra[i] | ((unsigned char)rc[i] >= 0x80 ? 0x10 : 0);
        if (a != prev)
            n++;
        prev = a;
    }
    return n;
}

/* Shows the row built in rc and ra, if it differs from what's there. */
static void row_put(int row)
{
    int i, j;

    if (row < 0 || row >= MAXROWS)
        return;
    if (shok[row] && memcmp(shc[row], rc, SCR_COLS) == 0 && memcmp(sha[row], ra, SCR_COLS) == 0)
        return;
    rc[SCR_COLS] = 0;
    scr_puts(row, 0, rc);
    scr_attr(row, 0, SCR_COLS, ATTR_NORMAL);
    for (i = 0; i < SCR_COLS; i = j) {
        for (j = i + 1; j < SCR_COLS && ra[j] == ra[i]; j++)
            ;
        if (ra[i] != ATTR_NORMAL)
            scr_attr(row, i, j - i, ra[i]);
    }
    memcpy(shc[row], rc, SCR_COLS);
    memcpy(sha[row], ra, SCR_COLS);
    shok[row] = 1;
}

static void redraw_all(void)
{
    memset(shok, 0, sizeof shok);
}

static void draw_title(void)
{
    char s[48];
    int n;

    row_begin();
    row_set(0, SCR_COLS, ATTR_REVERSE);
    row_text(1, "HexEdit");
    n = strlen(fname);
    row_text(10, n > 36 ? fname + n - 36 : fname);
    sprintf(s, "%lu bytes%s", docsize,
            readOnly ? "  read only" : changed() ? "  changed" : "");
    row_text(67 - (int)strlen(s), s);
    /* the modes, in a normal-video box at the end of the bar */
    sprintf(s, " %s  %s", insertMode ? "INS" : "OVR", textSide ? "TEXT" : "HEX");
    row_text(69, s);
    row_set(69, SCR_COLS - 69, ATTR_NORMAL);
    row_put(0);
}

static const char hexdigit[] = "0123456789ABCDEF";

static void put_hex(int col, int b)
{
    rc[col] = hexdigit[b >> 4];
    rc[col + 1] = hexdigit[b & 15];
}

static void draw_header(void)
{
    int i, c = (int)(cur & 15);

    row_begin();
    row_text(2, "Offset");
    rc[BAR1] = (char)BOX_V;
    rc[BAR2] = (char)BOX_V;
    for (i = 0; i < 16; i++) {
        put_hex(HEXCOL(i), i);
        rc[TEXTCOL(i)] = hexdigit[i];
    }
    row_set(0, SCR_COLS, ATTR_HALFBRIGHT | ATTR_UNDERLINE);
    row_set(HEXCOL(c), 2, ATTR_UNDERLINE);
    row_set(TEXTCOL(c), 1, ATTR_UNDERLINE);
    row_put(1);
}

static void draw_data(int r, int showChanged)
{
    Pos base = top + 16UL * r, p, s = 0, e = 0;
    char off[10];
    Iter it;
    int i, b, hc, tc;

    row_begin();
    rc[BAR1] = (char)BOX_V;
    rc[BAR2] = (char)BOX_V;
    if (base > docsize) {
        row_put(2 + r);
        return;
    }
    sprintf(off, "%08lX", base);
    row_text(1, off);
    if (cur < base || cur >= base + 16)
        row_set(1, 8, ATTR_HALFBRIGHT);
    if (marking)
        block(&s, &e);
    it_seek(&it, base);
    for (i = 0; i < 16; i++) {
        p = base + i;
        hc = HEXCOL(i);
        tc = TEXTCOL(i);
        if (p < docsize && (b = it_next(&it)) >= 0) {
            put_hex(hc, b);
            rc[tc] = (char)(b >= 0x20 && b < 0x7F ? b : '.');
            if (it.added && showChanged) {
                row_add(hc, 2, A_CHANGED);
                row_add(tc, 1, A_CHANGED);
            }
        }
        if (marking && p >= s && p < e) {
            row_add(hc, 2, A_SEL);
            row_add(tc, 1, A_SEL);
        }
        if (p == cur) {
            row_add(hc, 2, textSide ? A_CUR2 : A_CUR);
            row_add(tc, 1, textSide ? A_CUR : A_CUR2);
            if (nib)            /* half typed: dim the digit that's done */
                row_add(hc, 1, ATTR_HALFBRIGHT);
        }
    }
    /* The AWS shows reverse and underline together as underline alone, so
     * the cursor and the block hide the changed mark. */
    for (i = 0; i < SCR_COLS; i++)
        if (ra[i] & ATTR_REVERSE)
            ra[i] &= ~ATTR_UNDERLINE;
    /* what two neighbouring bytes share carries across the space between
     * them, so a run of bytes is one run of attributes */
    for (i = 1; i < 16; i++) {
        hc = HEXCOL(i);
        row_set(HEXCOL(i - 1) + 2, hc - HEXCOL(i - 1) - 2, ra[HEXCOL(i - 1) + 1] & ra[hc]);
    }
    if (showChanged && row_runs() > MAXRUNS) {
        draw_data(r, 0);
        return;
    }
    row_put(2 + r);
}

static void draw_rule(void)
{
    memset(rc, BOX_H, SCR_COLS);
    memset(ra, 0, SCR_COLS);
    rc[BAR1] = (char)BOX_BT;
    rc[BAR2] = (char)BOX_BT;
    row_put(rowRule);
}

static void bits(char *s, int b)
{
    int i;

    for (i = 7; i >= 0; i--)
        *s++ = (char)('0' + ((b >> i) & 1));
    *s = 0;
}

static void draw_status(void)
{
    static const char *const labels[] = { "Offset", " = ", "Byte", "Word", "Long", "Block", 0 };
    char s[SCR_COLS + 1], bin[9], *p;
    unsigned char v[4];
    Pos bs, be;
    Iter it;
    int i, n, c;

    row_begin();
    if (marking) {
        block(&bs, &be);
        sprintf(s, "Block %08lX-%08lX, %lu byte%s: f8 copies it, DELETE removes it",
                bs, be ? be - 1 : 0, be - bs, be - bs == 1 ? "" : "s");
    } else {
        n = sprintf(s, "Offset %08lX = %-8lu", cur, cur);
        it_seek(&it, cur);
        for (i = 0; i < 4 && (c = it_next(&it)) >= 0; i++)
            v[i] = (unsigned char)c;
        if (i > 0) {
            bits(bin, v[0]);
            n += sprintf(s + n, "  Byte %02X %3u '%c' %s", v[0], v[0],
                         v[0] >= 0x20 && v[0] < 0x7F ? v[0] : '.', bin);
        }
        if (i > 1)
            n += sprintf(s + n, "  Word %02X%02X", v[1], v[0]);
        if (i > 3)
            sprintf(s + n, "  Long %02X%02X%02X%02X", v[3], v[2], v[1], v[0]);
    }
    row_text(1, s);
    for (i = 0; labels[i]; i++)
        if ((p = strstr(s, labels[i])) != NULL)
            row_set(1 + (int)(p - s), strlen(labels[i]), ATTR_HALFBRIGHT);
    row_put(rowStatus);
}

static void draw_message(void)
{
    row_begin();
    row_text(1, message);
    row_put(rowMsg);
}

static void draw_keys(const char *s)
{
    row_begin();
    row_set(0, SCR_COLS, ATTR_REVERSE);
    row_text(0, s);
    row_put(rowKeys);
}

static const char keybar[] =
    "f1 Goto f2 Find f3 Next f4 Ins f5 Undo f6 Redo f7 Mark f8 Copy f9 Paste f10 Save";

/* The editor shows its cursor with attributes, which look the same on both
 * machines, and keeps the screen's cursor for the prompts. */
static void hide_cursor(void)
{
    scr_cursor(SCR_NOCURSOR, SCR_NOCURSOR);
}

static void draw(void)
{
    int r;

    draw_title();
    draw_header();
    for (r = 0; r < ndata; r++)
        draw_data(r, 1);
    draw_rule();
    draw_status();
    draw_message();
    draw_keys(keybar);
    hide_cursor();
}

/* Keeps the cursor in the document and on the screen. */
static void settle(void)
{
    Pos page = 16UL * ndata;

    if (cur > docsize)
        cur = docsize;
    if (cur == docsize)
        nib = 0;
    if (cur < top)
        top = cur & ~15UL;
    if (cur >= top + page)
        top = (cur & ~15UL) - (page - 16);
}

/* Edits a line of text on the message row, starting with what buf holds:
 * typing replaces it, BACKSPACE edits it. Returns 0 if CANCEL was pressed. */
static int prompt(const char *label, char *buf, unsigned size)
{
    unsigned n = strlen(buf);
    int col = strlen(label) + 2, k, fresh = 1;

    for (;;) {
        row_begin();
        row_text(1, label);
        row_text(col, buf);
        row_set(col, SCR_COLS - 1 - col, ATTR_UNDERLINE);
        row_put(rowMsg);
        draw_keys(" RETURN or GO to accept, CANCEL to stop, BACKSPACE to rub out");
        scr_cursor(rowMsg, col + n);
        k = scr_getkey();
        if (k == KEY_RETURN || k == KEY_GO)
            break;
        if (k == KEY_CANCEL) {
            message[0] = 0;
            return 0;
        }
        if (fresh && k >= 0x20 && k < 0x7F)
            buf[n = 0] = 0;
        fresh = 0;
        if (k == KEY_BACKSPACE && n > 0)
            buf[--n] = 0;
        else if (k >= 0x20 && k < 0x7F && n + 1 < size && col + n < SCR_COLS - 2) {
            buf[n++] = (char)k;
            buf[n] = 0;
        } else
            Beep();
    }
    message[0] = 0;
    return 1;
}

static int hexval(int c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

static void do_goto(void)
{
    const char *s = gotobuf;
    Pos v = 0;
    int sign = 0, base = 16, d, any = 0;

    if (!prompt("Go to offset:", gotobuf, sizeof gotobuf))
        return;
    while (*s == ' ')
        s++;
    if (*s == '$') {
        cur = docsize ? docsize - 1 : 0;
        nib = 0;
        return;
    }
    if (*s == '+' || *s == '-')
        sign = *s++;
    if (*s == '#') {
        base = 10;
        s++;
    }
    for (; (d = hexval(*s)) >= 0 && d < base; s++, any = 1)
        v = v * base + d;
    if (base == 16 && (*s == 'h' || *s == 'H'))
        s++;
    while (*s == ' ')
        s++;
    if (!any || *s) {
        msg("Not an offset: %s (see HELP)", gotobuf);
        return;
    }
    if (sign == '+')
        v += cur;
    else if (sign == '-')
        v = v > cur ? 0 : cur - v;
    cur = v > docsize ? docsize : v;
    nib = 0;
}

/* Reads hex bytes and "quoted text" into pat. */
static int parse_pattern(const char *s)
{
    int h, l;

    patlen = 0;
    while (*s) {
        if (*s == ' ') {
            s++;
        } else if (*s == '"') {
            for (s++; *s && *s != '"'; s++) {
                if (patlen == sizeof pat)
                    return -1;
                pat[patlen++] = (unsigned char)*s;
            }
            if (*s)
                s++;
        } else {
            if ((h = hexval(s[0])) < 0 || patlen == sizeof pat)
                return -1;
            if ((l = hexval(s[1])) >= 0) {
                pat[patlen++] = (unsigned char)(h * 16 + l);
                s += 2;
            } else if (s[1] == 0 || s[1] == ' ') {
                pat[patlen++] = (unsigned char)h;
                s++;
            } else
                return -1;
        }
    }
    return patlen ? 0 : -1;
}

#define NOTFOUND 0xFFFFFFFFUL
#define STOPPED  0xFFFFFFFEUL

/* Looks for pat starting at from or after, and ending by stop. Returns where
 * it starts, NOTFOUND, or STOPPED if CANCEL was pressed. */
static Pos scan(Pos from, Pos stop)
{
    unsigned char ring[32];     /* the last bytes read; sizeof pat, a power of two */
    unsigned char last = pat[patlen - 1], c;
    const unsigned char __far *p;
    Pos at = from, next = from + 16384;
    unsigned j, k, n;
    Iter it;

    it_seek(&it, from);
    while (at < stop && (n = it_span(&it, &p, IOBLOCK)) != 0) {
        if (n > stop - at)
            n = (unsigned)(stop - at);
        for (k = 0; k < n; k++) {
            c = p[k];
            ring[(unsigned)at & 31] = c;
            at++;
            if (c == last && at - from >= patlen) {
                for (j = 0; j < patlen && ring[(unsigned)(at - patlen + j) & 31] == pat[j]; j++)
                    ;
                if (j == patlen)
                    return at - patlen;
            }
        }
        if (at >= next) {
            next += 16384;
            msg("Searching, at %lu%%; CANCEL stops", at / (docsize / 100 + 1));
            draw_message();
            if (scr_pollkey() == KEY_CANCEL)
                return STOPPED;
        }
    }
    return NOTFOUND;
}

/* Searches from from to the end, then from the start round to from. */
static void search(Pos from)
{
    Pos at = scan(from, docsize), end = from + patlen - 1;
    int wrapped = 0;

    if (at == NOTFOUND && from > 0) {
        at = scan(0, end < docsize ? end : docsize);
        wrapped = 1;
    }
    if (at == STOPPED) {
        msg("Stopped");
    } else if (at == NOTFOUND) {
        msg("Not found");
    } else {
        cur = at;
        nib = 0;
        msg(wrapped ? "Found at %08lX, back at the start" : "Found at %08lX", at);
    }
}

static void do_find(int again)
{
    if (!again || patlen == 0) {
        if (!prompt("Find:", findbuf, sizeof findbuf))
            return;
        if (parse_pattern(findbuf)) {
            patlen = 0;
            msg("Give hex bytes and \"text\", such as 4D 5A or \"PK\" 03 04");
            return;
        }
        search(cur);
    } else
        search(cur + 1);
}

/* Saving */

/* True if every byte from the file is still where it was, so only the
 * edited pages need writing. */
static int in_place(void)
{
    Pos at = 0;
    unsigned i;

    if (!haveFile || docsize != filesize)
        return 0;
    for (i = 0; i < npieces; at += pieces[i++].len)
        if (!(pieces[i].start & ADDED) && pieces[i].start != at)
            return 0;
    return 1;
}

static ErcType write_in_place(void)
{
    Pos at = 0, page, end, done = 0;
    unsigned i, cb;
    ErcType erc;
    Iter it;

    for (i = 0; i < npieces; at += pieces[i++].len) {
        if (!(pieces[i].start & ADDED))
            continue;
        page = at & ~(Pos)(PAGE - 1);
        if (page < done)
            page = done;
        for (end = at + pieces[i].len; page < end; page += PAGE) {
            /* the page as it is, with the document over it up to the end */
            _fmemcpy(iobuf, file_at(page), PAGE);
            it_seek(&it, page);
            it_copy(&it, iobuf, docsize - page < PAGE ? (unsigned)(docsize - page) : PAGE);
            if (ioerc)
                return ioerc;
            if ((erc = Write(fh, iobuf, PAGE, page, &cb)) != 0)
                return erc;
            _fmemcpy(file_at(page), iobuf, PAGE);
        }
        done = page;
    }
    return 0;
}

static ErcType write_new(const char *name)
{
    unsigned nfh, cb, n, len = strlen(name);
    Pos at;
    ErcType erc;
    Iter it;

    remove(name);               /* left from a save that failed */
    erc = CreateFile(name, len, "", 0, (docsize + PAGE - 1) & ~(Pos)(PAGE - 1));
    if (erc == 0 && (erc = OpenFile(&nfh, name, len, "", 0, modeModify)) != 0)
        remove(name);
    if (erc)
        return erc;
    it_seek(&it, 0);
    for (at = 0; at < docsize && erc == 0; at += n) {
        n = docsize - at < IOBLOCK ? (unsigned)(docsize - at) : IOBLOCK;
        it_copy(&it, iobuf, n);
        erc = ioerc ? ioerc : Write(nfh, iobuf, (n + PAGE - 1) & ~(PAGE - 1), at, &cb);
        if (docsize > 4 * IOBLOCK) {
            msg("Saving, %lu%%", (at + n) / (docsize / 100 + 1));
            draw_message();
        }
    }
    if (erc == 0)
        erc = SetFileStatus(nfh, statusEOF, &docsize, sizeof docsize);
    CloseFile(nfh);
    if (erc)
        remove(name);
    return erc;
}

static void reset_doc(Pos size)
{
    npieces = 0;
    if (size) {
        pieces[0].start = 0;
        pieces[0].len = size;
        npieces = 1;
    }
    docsize = size;
    addlen = 0;
    lastI = 0;
    lastS = 0;
    cache_clear();
}

static ErcType open_doc(void)
{
    ErcType erc;

    haveFile = 0;
    erc = OpenFile(&fh, fname, strlen(fname), "", 0, modeModify);
    if (erc != 0 && erc != ercNoSuchFile &&
        OpenFile(&fh, fname, strlen(fname), "", 0, modeRead) == 0) {
        readOnly = 1;
        erc = 0;
    }
    if (erc)
        return erc;
    haveFile = 1;
    if ((erc = GetFileStatus(fh, statusEOF, &filesize, sizeof filesize)) != 0) {
        CloseFile(fh);
        haveFile = 0;
    }
    return erc;
}

/* Copies the clipboard's bytes out: its pieces point into the file and the
 * edit buffer, which saving replaces. */
static void clip_bytes(unsigned char __far *to)
{
    unsigned i;
    Pos off;

    for (i = 0; i < nclip; i++)
        for (off = 0; off < clip[i].len; off++)
            *to++ = (unsigned char)piece_byte(&clip[i], off);
}

static int save(void)
{
    static char tmp[90];
    unsigned char __far *keep = 0;
    Pos keeplen = cliplen;
    ErcType erc;
    int renamed = 1;

    if (!editable())
        return -1;
    ioerc = 0;
    if (nclip && cliplen <= addmax && (keep = farmalloc((unsigned)cliplen)) != 0)
        clip_bytes(keep);
    msg("Saving");
    draw_message();
    if (in_place()) {
        erc = write_in_place();
    } else if (!haveFile) {
        erc = write_new(fname);
    } else {
        sprintf(tmp, "%s-new", fname);
        erc = write_new(tmp);
        if (erc == 0) {
            CloseFile(fh);
            haveFile = 0;
            if (remove(fname) != 0) {
                erc = (ErcType)errno;
                remove(tmp);
            } else if (rename(tmp, fname) != 0) {
                renamed = 0;
                strcpy(fname, tmp);
            }
        }
    }
    if (!haveFile) {
        ErcType erc2 = open_doc();
        if (erc == 0)
            erc = erc2;
    }

    if (erc) {
        msg("Can't save %s (error %u)", fname, erc);
    } else {
        filesize = docsize;
        reset_doc(docsize);
        utop = uend = 0;
        level = savedLevel = 0;
        if (renamed)
            msg("Saved %lu bytes", docsize);
        else
            msg("Saved as %s: the old file is gone, but the rename failed", fname);
        nclip = 0;
        if (keep) {
            _fmemcpy(addbuf, keep, (unsigned)keeplen);
            addlen = (unsigned)keeplen;
            clip[0].start = ADDED;
            clip[0].len = keeplen;
            nclip = 1;
        }
    }
    if (keep)
        farfree(keep);
    return erc ? -1 : 0;
}

/* Help */

static const char *const helptext[] = {
    "Moving                                Changing",
    "  arrows          a byte or a row       0-9, A-F    hex digits (hex side)",
    "  PREV/NEXT PAGE  a screen              any key     characters (text side)",
    "  SCROLL UP/DOWN  a row at a time       f4          insert or overtype",
    "  TAB             hex or text side      DELETE      remove byte or block",
    "  f1              go to an offset       BACKSPACE   back, deleting in INS",
    "                                        f5, f6      undo, redo",
    "Blocks                                Files",
    "  f7 or MARK      start or drop one     f10         save",
    "  f8              copy it               FINISH      leave, asking to save",
    "  f9              paste, replacing      HELP        this screen",
    "                  the block if any      CANCEL      drop the block",
    "",
    "f2 finds hex bytes and quoted text, such as 4D 5A or \"PK\" 03 04, and f3",
    "finds the next one. f1 takes hex (1A2F), decimal after # (#6703), +n or -n",
    "from the cursor, or $ for the last byte. Underlined bytes are unsaved.",
    "",
    0
};

static void show_help(void)
{
    char s[SCR_COLS + 1];
    int r, i, col, heading, last = rowMsg;

    row_begin();
    memset(rc + 1, BOX_H, SCR_COLS - 2);
    rc[1] = (char)BOX_TL;
    rc[SCR_COLS - 2] = (char)BOX_TR;
    row_put(1);
    for (r = 2, i = 0; r < last; r++) {
        row_begin();
        rc[1] = (char)BOX_V;
        rc[SCR_COLS - 2] = (char)BOX_V;
        heading = i == 0 || i == 7;
        if (helptext[i])
            row_text(3, helptext[i++]);
        else if (r == last - 1) {
            sprintf(s, "Room left: %u bytes of typing, %u pieces, %lu bytes of memory",
                    addmax - addlen, maxpieces - npieces, farcoreleft());
            row_text(3, s);
            row_set(3, strlen(s), ATTR_HALFBRIGHT);
        }
        if (heading)
            for (col = 3; col < SCR_COLS - 2; col++)
                if (rc[col] != ' ')
                    ra[col] = ATTR_UNDERLINE;
        row_put(r);
    }
    row_begin();
    memset(rc + 1, BOX_H, SCR_COLS - 2);
    rc[1] = (char)BOX_BL;
    rc[SCR_COLS - 2] = (char)BOX_BR;
    row_put(last);
    draw_keys(" Press any key to go back to the file");
    hide_cursor();
    scr_getkey();
}

/* The program */

static void __far *grab(unsigned want, unsigned least, unsigned *got)
{
    void __far *p;

    for (; want >= least; want /= 2)
        if ((p = farmalloc(want)) != 0) {
            *got = want;
            return p;
        }
    return 0;
}

static int leave(void)
{
    int k;

    if (!changed() || readOnly)
        return 1;
    msg("Save the changes? Y saves them, N throws them away, CANCEL goes back");
    draw_message();
    for (;;) {
        k = scr_getkey();
        if (k == 'y' || k == 'Y')
            return save() == 0;
        if (k == 'n' || k == 'N')
            return 1;
        if (k == KEY_CANCEL) {
            message[0] = 0;
            return 0;
        }
        Beep();
    }
}

static void key(int k)
{
    Pos page = 16UL * ndata, s, e;
    int am = amendOK, d;

    amendOK = 0;
    message[0] = 0;
    switch (k) {
    case KEY_LEFT:
        if (nib)
            nib = 0;
        else if (cur > 0)
            cur--;
        return;
    case KEY_RIGHT:
        if (cur < docsize)
            cur++;
        nib = 0;
        return;
    case KEY_UP:
        if (cur >= 16)
            cur -= 16;
        nib = 0;
        return;
    case KEY_DOWN:
        cur = cur + 16 > docsize ? docsize : cur + 16;
        nib = 0;
        return;
    case KEY_PREVPAGE:
        cur = cur > page ? cur - page : 0;
        top = top > page ? top - page : 0;
        nib = 0;
        return;
    case KEY_NEXTPAGE:
        if (top + page <= docsize) {
            top += page;
            cur = cur + page > docsize ? docsize : cur + page;
        } else
            cur = docsize;
        nib = 0;
        return;
    case KEY_SCROLLUP:
        if (top >= 16) {
            top -= 16;
            if (cur >= 16)
                cur -= 16;
        }
        nib = 0;
        return;
    case KEY_SCROLLDOWN:
        if (top + 16 <= docsize) {
            top += 16;
            cur = cur + 16 > docsize ? docsize : cur + 16;
        }
        nib = 0;
        return;
    case KEY_TAB:
        textSide = !textSide;
        nib = 0;
        return;
    case KEY_F1:
        do_goto();
        return;
    case KEY_F2:
    case KEY_F3:
        do_find(k == KEY_F3);
        return;
    case KEY_F4:
        insertMode = !insertMode;
        return;
    case KEY_F5:
        marking = 0;
        nib = 0;
        do_undo();
        return;
    case KEY_F6:
        marking = 0;
        nib = 0;
        do_redo();
        return;
    case KEY_F7:
    case KEY_MARK:
        marking = !marking;
        mark = cur;
        return;
    case KEY_F8:
        if (!marking) {
            msg("Mark a block first: f7 at one end, then move to the other");
            return;
        }
        block(&s, &e);
        if (copy_block(s, e) == 0)
            msg("Copied %lu byte%s", cliplen, cliplen == 1 ? "" : "s");
        marking = 0;
        return;
    case KEY_F9:
        paste();
        return;
    case KEY_F10:
        save();
        return;
    case KEY_DELETE:
        if (marking) {
            block(&s, &e);
            delete_bytes(s, e);
            marking = 0;
        } else if (cur < docsize)
            delete_bytes(cur, cur + 1);
        nib = 0;
        return;
    case KEY_BACKSPACE:
        if (nib)
            nib = 0;
        else if (cur > 0) {
            if (insertMode)
                delete_bytes(cur - 1, cur);
            else
                cur--;
        }
        return;
    case KEY_CANCEL:
        marking = 0;
        return;
    case KEY_HELP:
        show_help();
        redraw_all();
        return;
    }

    if (k < 0x20 || k >= 0x7F) {
        Beep();
        return;
    }
    if (!editable())
        return;
    if (textSide) {
        if (put_byte(cur, k, insertMode, REC_NEW) == 0)
            cur++;
        return;
    }
    if ((d = hexval(k)) < 0) {
        Beep();
        return;
    }
    if (nib == 0) {
        int ins = insertMode || cur >= docsize;
        int old = ins ? 0 : byte_at(cur) & 0x0F;
        if (put_byte(cur, d << 4 | old, ins, REC_NEW) == 0) {
            nib = 1;
            amendOK = 1;
        }
    } else if (put_byte(cur, (byte_at(cur) & 0xF0) | d, 0, am ? REC_AMEND : REC_NEW) == 0) {
        nib = 0;
        cur++;
    }
}

int main(int argc, char **argv)
{
    unsigned got;
    ErcType erc;
    int k;

    scr_clear();
    rows = scr_rows();
    if (rows > MAXROWS)
        rows = MAXROWS;
    ndata = rows - 6;
    rowRule = 2 + ndata;
    rowStatus = rowRule + 1;
    rowMsg = rowStatus + 1;
    rowKeys = rowMsg + 1;

    if (argc > 1) {
        strncpy(fname, argv[1], sizeof fname - 1);
    } else {
        draw_keys("");
        if (!prompt("File to edit:", fname, sizeof fname) || !fname[0]) {
            scr_clear();
            return 0;
        }
    }

    cache = farmalloc(NCACHE * IOBLOCK);
    iobuf = farmalloc(IOBLOCK);
    pieces = grab(32000, 1600, &got);
    addbuf = grab(32768u, 2048, &addmax);
    undo = grab(24000, 2000, &umax);
    if (!cache || !iobuf || !pieces || !addbuf || !undo) {
        scr_clear();
        printf("hexedit: not enough memory\n");
        return 1;
    }
    maxpieces = got / sizeof(Piece);
    cache_clear();

    erc = open_doc();
    if (erc == ercNoSuchFile) {
        filesize = 0;
        msg("New file: f10 makes it");
    } else if (erc) {
        scr_clear();
        printf("hexedit: can't open %s (error %u)\n", fname, erc);
        return 1;
    } else if (readOnly)
        msg("Read only: this file can be looked at but not changed");
    reset_doc(filesize);

    for (;;) {
        settle();
        draw();
        if (ioerc) {
            msg("Error %u reading %s", ioerc, fname);
            ioerc = 0;
            draw_message();
        }
        k = scr_getkey();
        if (k == KEY_FINISH) {
            if (leave())
                break;
        } else
            key(k);
    }
    if (haveFile)
        CloseFile(fh);
    scr_clear();
    return 0;
}
