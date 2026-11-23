/* chess - plays chess against you, on a full-screen board.
 *
 *   chess [-b] [-t seconds] [-d depth] ['position']
 *
 * You play White, or Black with -b. Type a move in algebraic notation, such
 * as e4, Nf3, exd5, O-O or e8=Q, or as the squares it goes from and to, such
 * as e2e4 or e7e8q. The computer thinks for up to -t seconds a move (10
 * unless told), or searches -d moves ahead. The position, in FEN, sets up
 * the board: put it in single quotes, as it has spaces.
 *
 * Commands at the prompt: new, undo (takes back your last move and the
 * computer's), switch (the computer takes your side), level N (seconds a
 * move), depth N (0 for no limit), perft N (counts the positions N moves
 * ahead, to check the move generator), help, and quit or FINISH. While the
 * computer thinks, CANCEL makes it move at once.
 *
 * The engine: a 0x88 board, alpha-beta search with iterative deepening, a
 * quiescence search of captures, a hash table of positions in far memory,
 * and an evaluation of material and where each piece stands. It knows
 * castling, en passant, promotion, and draws by stalemate, repetition, the
 * fifty-move rule and too little material. */
#include <farmem.h>
#include <screen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define WHITE 8
#define BLACK 16
#define PAWN 1
#define KNIGHT 2
#define BISHOP 3
#define ROOK 4
#define QUEEN 5
#define KING 6
#define TYPE(p) ((p) & 7)
#define COLOR(p) ((p) & 24)
#define OFF(sq) ((sq) & 0x88)

#define MATE 30000
#define INF 32000
#define MAXPLY 24
#define MOVESTACK 1600
#define HISTORY 400         /* game moves and search plies */

/* move flags */
#define F_CAPTURE 1
#define F_EP      2
#define F_CASTLE  4
#define F_DOUBLE  8

typedef struct {
    unsigned char from, to, promo, flags;
    int score;              /* for ordering */
} Move;

typedef struct {
    Move m;
    unsigned char captured, rights, ep, fifty;
    unsigned long hash;
} Undo;

static unsigned char board[128];
static int side;                    /* WHITE or BLACK, to move */
static unsigned char rights;        /* castling: 1 K, 2 Q (White), 4 k, 8 q */
static unsigned char ep;            /* the en passant square, or 0x88 for none */
static unsigned char fifty;         /* half moves since a capture or pawn move */
static unsigned char kingsq[25];
static unsigned long hash;
static Undo undo[HISTORY];
static int nundo;                   /* moves made, in the game and the search */
static int moveno0;                 /* the full move number the game started at */

/* The moves of each ply in turn, in far memory: the segment is nearly full */
static Move __far *stack;
static int sp;

static const int knight_d[8] = { 33, 31, 18, 14, -33, -31, -18, -14 };
static const int king_d[8] = { 1, -1, 16, -16, 17, 15, -17, -15 };
static const int bishop_d[4] = { 17, 15, -17, -15 };
static const int rook_d[4] = { 1, -1, 16, -16 };
static unsigned char castle_mask[128];

/* Zobrist keys: one for each piece on each square, the side and the rights */
static unsigned long zpiece[2][7][64], zside, zrights[16], zep[8];

static unsigned long rnd_state = 0x2545F491UL;
static unsigned long rnd32(void)
{
    rnd_state ^= rnd_state << 13;
    rnd_state ^= rnd_state >> 17;
    rnd_state ^= rnd_state << 5;
    return rnd_state;
}

#define SQ64(sq) (((sq) + ((sq) & 7)) >> 1)     /* 0x88 to 0-63 */
#define ZP(p, sq) zpiece[COLOR(p) == BLACK][TYPE(p)][SQ64(sq)]

static void init_tables(void)
{
    int c, t, sq;

    for (c = 0; c < 2; c++)
        for (t = 0; t < 7; t++)
            for (sq = 0; sq < 64; sq++)
                zpiece[c][t][sq] = rnd32();
    zside = rnd32();
    for (t = 0; t < 16; t++)
        zrights[t] = rnd32();
    for (t = 0; t < 8; t++)
        zep[t] = rnd32();
    memset(castle_mask, 15, sizeof castle_mask);
    castle_mask[0x00] = 15 & ~2;
    castle_mask[0x07] = 15 & ~1;
    castle_mask[0x04] = 15 & ~3;
    castle_mask[0x70] = 15 & ~8;
    castle_mask[0x77] = 15 & ~4;
    castle_mask[0x74] = 15 & ~12;
}

static unsigned long full_hash(void)
{
    unsigned long h = 0;
    int sq;

    for (sq = 0; sq < 128; sq++)
        if (!OFF(sq) && board[sq])
            h ^= ZP(board[sq], sq);
    if (side == BLACK)
        h ^= zside;
    h ^= zrights[rights];
    if (ep != 0x88)
        h ^= zep[ep & 7];
    return h;
}

/* Sets up a position from FEN; returns 0, or -1 if it makes no sense. */
static int set_fen(const char *fen)
{
    static const char letters[] = "pnbrqk";
    const char *p;
    int rank = 7, file = 0, kings = 0;

    memset(board, 0, sizeof board);
    for (; *fen && *fen != ' '; fen++) {
        if (*fen == '/') {
            rank--;
            file = 0;
        } else if (*fen >= '1' && *fen <= '8')
            file += *fen - '0';
        else if ((p = strchr(letters, *fen | 0x20)) != NULL && rank >= 0 && file < 8) {
            board[rank * 16 + file] = (unsigned char)((p - letters + 1) | (*fen < 'a' ? WHITE : BLACK));
            if (TYPE(board[rank * 16 + file]) == KING) {
                kingsq[COLOR(board[rank * 16 + file])] = (unsigned char)(rank * 16 + file);
                kings++;
            }
            file++;
        } else
            return -1;
    }
    if (kings != 2)
        return -1;
    while (*fen == ' ')
        fen++;
    side = *fen == 'b' ? BLACK : WHITE;
    if (*fen)
        fen++;
    while (*fen == ' ')
        fen++;
    rights = 0;
    for (; *fen && *fen != ' '; fen++)
        rights |= *fen == 'K' ? 1 : *fen == 'Q' ? 2 : *fen == 'k' ? 4 : *fen == 'q' ? 8 : 0;
    /* only rights that still make sense */
    if (board[0x04] != (WHITE | KING) || board[0x07] != (WHITE | ROOK)) rights &= ~1;
    if (board[0x04] != (WHITE | KING) || board[0x00] != (WHITE | ROOK)) rights &= ~2;
    if (board[0x74] != (BLACK | KING) || board[0x77] != (BLACK | ROOK)) rights &= ~4;
    if (board[0x74] != (BLACK | KING) || board[0x70] != (BLACK | ROOK)) rights &= ~8;
    while (*fen == ' ')
        fen++;
    ep = 0x88;
    if (fen[0] >= 'a' && fen[0] <= 'h' && (fen[1] == '3' || fen[1] == '6'))
        ep = (unsigned char)((fen[1] - '1') * 16 + fen[0] - 'a');
    while (*fen && *fen != ' ')
        fen++;
    fifty = (unsigned char)atoi(fen);
    while (*fen == ' ')
        fen++;
    while (*fen && *fen != ' ')
        fen++;
    moveno0 = atoi(fen);
    if (moveno0 < 1)
        moveno0 = 1;
    nundo = 0;
    hash = full_hash();
    return 0;
}

static int attacked(int sq, int by)
{
    int i, t, d, p;

    /* pawns attack the way they move */
    d = by == WHITE ? -16 : 16;
    t = sq + d + 1;
    if (!OFF(t) && board[t] == (by | PAWN))
        return 1;
    t = sq + d - 1;
    if (!OFF(t) && board[t] == (by | PAWN))
        return 1;
    for (i = 0; i < 8; i++) {
        t = sq + knight_d[i];
        if (!OFF(t) && board[t] == (by | KNIGHT))
            return 1;
        t = sq + king_d[i];
        if (!OFF(t) && board[t] == (by | KING))
            return 1;
    }
    for (i = 0; i < 4; i++) {
        for (t = sq + bishop_d[i]; !OFF(t); t += bishop_d[i]) {
            if ((p = board[t]) != 0) {
                if (p == (by | BISHOP) || p == (by | QUEEN))
                    return 1;
                break;
            }
        }
        for (t = sq + rook_d[i]; !OFF(t); t += rook_d[i]) {
            if ((p = board[t]) != 0) {
                if (p == (by | ROOK) || p == (by | QUEEN))
                    return 1;
                break;
            }
        }
    }
    return 0;
}

#define IN_CHECK() attacked(kingsq[side], 24 - side)

static void add(int from, int to, int promo, int flags)
{
    Move __far *m;

    if (sp >= MOVESTACK)
        return;
    m = &stack[sp++];
    m->from = (unsigned char)from;
    m->to = (unsigned char)to;
    m->promo = (unsigned char)promo;
    m->flags = (unsigned char)flags;
    m->score = 0;
}

static void add_pawn(int from, int to, int flags)
{
    int rank = to >> 4;

    if (rank == 0 || rank == 7) {
        add(from, to, QUEEN, flags);
        add(from, to, KNIGHT, flags);
        add(from, to, ROOK, flags);
        add(from, to, BISHOP, flags);
    } else
        add(from, to, 0, flags);
}

/* Pushes the moves of the side to move onto the stack, not checking that
 * they leave the king safe; with captures set, only the captures and
 * promotions, for the quiescence search. */
static void generate(int captures)
{
    int sq, t, i, p, d, xside = 24 - side;

    for (sq = 0; sq < 128; sq++) {
        if (OFF(sq)) {
            sq += 7;
            continue;
        }
        p = board[sq];
        if (COLOR(p) != side)
            continue;
        switch (TYPE(p)) {
        case PAWN:
            d = side == WHITE ? 16 : -16;
            t = sq + d;
            if (!OFF(t) && board[t] == 0) {
                if (!captures || (t >> 4) == 0 || (t >> 4) == 7)
                    add_pawn(sq, t, 0);
                if (!captures && (sq >> 4) == (side == WHITE ? 1 : 6) && board[t + d] == 0)
                    add(sq, t + d, 0, F_DOUBLE);
            }
            for (i = -1; i <= 1; i += 2) {
                t = sq + d + i;
                if (OFF(t))
                    continue;
                if (board[t] && COLOR(board[t]) == xside)
                    add_pawn(sq, t, F_CAPTURE);
                else if (t == ep)
                    add(sq, t, 0, F_CAPTURE | F_EP);
            }
            break;
        case KNIGHT:
        case KING:
            for (i = 0; i < 8; i++) {
                t = sq + (TYPE(p) == KNIGHT ? knight_d[i] : king_d[i]);
                if (OFF(t))
                    continue;
                if (board[t] == 0) {
                    if (!captures)
                        add(sq, t, 0, 0);
                } else if (COLOR(board[t]) == xside)
                    add(sq, t, 0, F_CAPTURE);
            }
            if (TYPE(p) == KING && !captures) {
                int r = side == WHITE ? 0 : 0x70;
                int k = side == WHITE ? 1 : 4, q = side == WHITE ? 2 : 8;
                if ((rights & k) && !board[r + 5] && !board[r + 6] && !attacked(r + 4, xside) &&
                    !attacked(r + 5, xside) && !attacked(r + 6, xside))
                    add(r + 4, r + 6, 0, F_CASTLE);
                if ((rights & q) && !board[r + 3] && !board[r + 2] && !board[r + 1] &&
                    !attacked(r + 4, xside) && !attacked(r + 3, xside) && !attacked(r + 2, xside))
                    add(r + 4, r + 2, 0, F_CASTLE);
            }
            break;
        default:
            /* king_d has the rook's directions, then the bishop's */
            for (i = TYPE(p) == BISHOP ? 4 : 0; i < (TYPE(p) == ROOK ? 4 : 8); i++) {
                d = king_d[i];
                for (t = sq + d; !OFF(t); t += d) {
                    if (board[t] == 0) {
                        if (!captures)
                            add(sq, t, 0, 0);
                        continue;
                    }
                    if (COLOR(board[t]) == xside)
                        add(sq, t, 0, F_CAPTURE);
                    break;
                }
            }
        }
    }
}

static void make(const Move __far *m)
{
    Undo *u = &undo[nundo++];
    int p = board[m->from], d = side == WHITE ? 16 : -16, r;

    u->m = *m;
    u->rights = rights;
    u->ep = ep;
    u->fifty = fifty;
    u->hash = hash;
    u->captured = board[m->to];
    if (ep != 0x88)
        hash ^= zep[ep & 7];
    hash ^= zrights[rights];
    if (u->captured)
        hash ^= ZP(u->captured, m->to);
    if (m->flags & F_EP) {
        u->captured = board[m->to - d];
        board[m->to - d] = 0;
        hash ^= ZP(u->captured, m->to - d);
    }
    hash ^= ZP(p, m->from);
    board[m->from] = 0;
    if (m->promo)
        p = side | m->promo;
    board[m->to] = (unsigned char)p;
    hash ^= ZP(p, m->to);
    if (TYPE(p) == KING)
        kingsq[side] = m->to;
    if (m->flags & F_CASTLE) {
        r = m->to & 0x70;
        if ((m->to & 7) == 6) {
            board[r + 5] = board[r + 7];
            board[r + 7] = 0;
            hash ^= ZP(side | ROOK, r + 7) ^ ZP(side | ROOK, r + 5);
        } else {
            board[r + 3] = board[r];
            board[r] = 0;
            hash ^= ZP(side | ROOK, r) ^ ZP(side | ROOK, r + 3);
        }
    }
    rights &= castle_mask[m->from] & castle_mask[m->to];
    hash ^= zrights[rights];
    ep = 0x88;
    if (m->flags & F_DOUBLE) {
        ep = (unsigned char)(m->from + d);
        hash ^= zep[ep & 7];
    }
    fifty = (unsigned char)(TYPE(p) == PAWN || u->captured ? 0 : fifty + 1);
    side = 24 - side;
    hash ^= zside;
}

static void unmake(void)
{
    Undo *u = &undo[--nundo];
    const Move *m = &u->m;
    int p, r, d;

    side = 24 - side;
    d = side == WHITE ? 16 : -16;
    p = board[m->to];
    if (m->promo)
        p = side | PAWN;
    board[m->from] = (unsigned char)p;
    board[m->to] = 0;
    if (m->flags & F_EP)
        board[m->to - d] = u->captured;
    else
        board[m->to] = u->captured;
    if (TYPE(p) == KING)
        kingsq[side] = m->from;
    if (m->flags & F_CASTLE) {
        r = m->to & 0x70;
        if ((m->to & 7) == 6) {
            board[r + 7] = board[r + 5];
            board[r + 5] = 0;
        } else {
            board[r] = board[r + 3];
            board[r + 3] = 0;
        }
    }
    rights = u->rights;
    ep = u->ep;
    fifty = u->fifty;
    hash = u->hash;
}

/* Makes the move if it's legal; returns 0 if it would leave the king in
 * check, having taken it back. */
static int make_legal(const Move __far *m)
{
    make(m);
    if (attacked(kingsq[24 - side], side)) {
        unmake();
        return 0;
    }
    return 1;
}

/* Pushes the legal moves onto the stack, returning how many: the caller
 * takes them off again by setting sp back. */
static int push_legal(void)
{
    int base = sp, top, i, n = base;

    generate(0);
    top = sp;
    for (i = base; i < top; i++) {
        if (make_legal(&stack[i])) {
            unmake();
            stack[n++] = stack[i];
        }
    }
    sp = n;
    return n - base;
}

static int any_legal(void)
{
    int base = sp, n = push_legal();

    sp = base;
    return n;
}

/* Evaluation: material, and tables of where each piece is best, from
 * White's side, rank 8 first, so as they look on the board. */
static const int value[7] = { 0, 100, 320, 330, 500, 900, 0 };

static const signed char pst[7][64] = {
    { 0 },
    {   0,  0,  0,  0,  0,  0,  0,  0,      /* pawn */
       50, 50, 50, 50, 50, 50, 50, 50,
       10, 10, 20, 30, 30, 20, 10, 10,
        5,  5, 10, 25, 25, 10,  5,  5,
        0,  0,  0, 20, 20,  0,  0,  0,
        5, -5,-10,  0,  0,-10, -5,  5,
        5, 10, 10,-20,-20, 10, 10,  5,
        0,  0,  0,  0,  0,  0,  0,  0 },
    { -50,-40,-30,-30,-30,-30,-40,-50,      /* knight */
      -40,-20,  0,  0,  0,  0,-20,-40,
      -30,  0, 10, 15, 15, 10,  0,-30,
      -30,  5, 15, 20, 20, 15,  5,-30,
      -30,  0, 15, 20, 20, 15,  0,-30,
      -30,  5, 10, 15, 15, 10,  5,-30,
      -40,-20,  0,  5,  5,  0,-20,-40,
      -50,-40,-30,-30,-30,-30,-40,-50 },
    { -20,-10,-10,-10,-10,-10,-10,-20,      /* bishop */
      -10,  0,  0,  0,  0,  0,  0,-10,
      -10,  0,  5, 10, 10,  5,  0,-10,
      -10,  5,  5, 10, 10,  5,  5,-10,
      -10,  0, 10, 10, 10, 10,  0,-10,
      -10, 10, 10, 10, 10, 10, 10,-10,
      -10,  5,  0,  0,  0,  0,  5,-10,
      -20,-10,-10,-10,-10,-10,-10,-20 },
    {   0,  0,  0,  0,  0,  0,  0,  0,      /* rook */
        5, 10, 10, 10, 10, 10, 10,  5,
       -5,  0,  0,  0,  0,  0,  0, -5,
       -5,  0,  0,  0,  0,  0,  0, -5,
       -5,  0,  0,  0,  0,  0,  0, -5,
       -5,  0,  0,  0,  0,  0,  0, -5,
       -5,  0,  0,  0,  0,  0,  0, -5,
        0,  0,  0,  5,  5,  0,  0,  0 },
    { -20,-10,-10, -5, -5,-10,-10,-20,      /* queen */
      -10,  0,  0,  0,  0,  0,  0,-10,
      -10,  0,  5,  5,  5,  5,  0,-10,
       -5,  0,  5,  5,  5,  5,  0, -5,
        0,  0,  5,  5,  5,  5,  0, -5,
      -10,  5,  5,  5,  5,  5,  0,-10,
      -10,  0,  5,  0,  0,  0,  0,-10,
      -20,-10,-10, -5, -5,-10,-10,-20 },
    { -30,-40,-40,-50,-50,-40,-40,-30,      /* king, with pieces on the board */
      -30,-40,-40,-50,-50,-40,-40,-30,
      -30,-40,-40,-50,-50,-40,-40,-30,
      -30,-40,-40,-50,-50,-40,-40,-30,
      -20,-30,-30,-40,-40,-30,-30,-20,
      -10,-20,-20,-20,-20,-20,-20,-10,
       20, 20,  0,  0,  0,  0, 20, 20,
       20, 30, 10,  0,  0, 10, 30, 20 },
};

static const signed char king_end[64] = {   /* king, in the endgame */
    -50,-40,-30,-20,-20,-30,-40,-50,
    -30,-20,-10,  0,  0,-10,-20,-30,
    -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-30,  0,  0,  0,  0,-30,-30,
    -50,-30,-30,-30,-30,-30,-30,-50 };

/* the table index for a square, for White, or for Black seen from its side */
#define PST_W(sq) ((7 - ((sq) >> 4)) * 8 + ((sq) & 7))
#define PST_B(sq) (((sq) >> 4) * 8 + ((sq) & 7))

static int evaluate(void)
{
    int sq, p, t, score[2] = { 0, 0 }, pieces[2] = { 0, 0 }, bishops[2] = { 0, 0 }, c, i;

    for (sq = 0; sq < 128; sq++) {
        if (OFF(sq)) {
            sq += 7;
            continue;
        }
        if ((p = board[sq]) == 0)
            continue;
        t = TYPE(p);
        c = COLOR(p) == BLACK;
        i = c ? PST_B(sq) : PST_W(sq);
        score[c] += value[t];
        if (t != KING)
            score[c] += pst[t][i];
        if (t != PAWN && t != KING)
            pieces[c] += value[t];
        if (t == BISHOP)
            bishops[c]++;
    }
    for (c = 0; c < 2; c++) {
        sq = kingsq[c ? BLACK : WHITE];
        i = c ? PST_B(sq) : PST_W(sq);
        score[c] += pieces[0] + pieces[1] <= 1300 ? king_end[i] : pst[KING][i];
        if (bishops[c] >= 2)
            score[c] += 30;
    }
    return side == WHITE ? score[0] - score[1] : score[1] - score[0];
}

/* Too little to mate with: kings, and at most one knight or bishop. */
static int no_mating_material(void)
{
    int sq, minors = 0, t;

    for (sq = 0; sq < 128; sq++) {
        if (OFF(sq) || !board[sq])
            continue;
        t = TYPE(board[sq]);
        if (t == PAWN || t == ROOK || t == QUEEN)
            return 0;
        if (t == KNIGHT || t == BISHOP)
            minors++;
    }
    return minors <= 1;
}

/* How many times this position has been seen, back to the last capture or
 * pawn move. */
static int repeats(void)
{
    int i, n = 0;

    for (i = nundo - 2; i >= 0 && i >= nundo - fifty; i -= 2)
        if (undo[i].hash == hash)
            n++;
    return n;
}

/* The hash table, in far memory: what was found about each position. */
#define TT_EXACT 1
#define TT_LOWER 2      /* the score is at least this */
#define TT_UPPER 3      /* at most */
typedef struct {
    unsigned long key;
    int score;
    unsigned char depth, kind, from, to, promo, pad;
} Entry;
#define TT_SIZE 4096
static Entry __far *tt;

/* Mate scores count from the root, so they're stored counting from here. */
static int to_tt(int s, int ply) { return s > MATE - 100 ? s + ply : s < -MATE + 100 ? s - ply : s; }
static int from_tt(int s, int ply) { return s > MATE - 100 ? s - ply : s < -MATE + 100 ? s + ply : s; }

/* the search */
static unsigned long nodes;
static int stopped, max_depth, think_secs = 10;
static time_t deadline;
static Move pv[MAXPLY][MAXPLY];
static int pvlen[MAXPLY];
static Move killer[MAXPLY][2];
static int hist[2][7][64];          /* by side, the piece moved and where to */

#define SAME(a, b) ((a).from == (b).from && (a).to == (b).to && (a).promo == (b).promo)
#define HIST(m) hist[side == BLACK][TYPE(board[(m).from])][SQ64((m).to)]

static void check_time(void)
{
    if ((nodes & 255) != 0)
        return;
    if ((deadline && time(NULL) >= deadline) || scr_pollkey() == KEY_CANCEL)
        stopped = 1;
}

/* Puts the best-looking move first: the hash table's, then captures of the
 * most for the least, then killers and the history. */
static void order(int base, int top, const Move *best, int ply)
{
    Move __far *m;
    int i;

    for (i = base; i < top; i++) {
        m = &stack[i];
        if (best && SAME(*m, *best))
            m->score = 30000;
        else if (m->flags & F_CAPTURE)
            m->score = 20000 + value[TYPE(board[m->to])] * 8 - value[TYPE(board[m->from])] / 16 +
                       (m->flags & F_EP ? 800 : 0);
        else if (m->promo)
            m->score = 19000 + value[m->promo];
        else if (SAME(*m, killer[ply][0]))
            m->score = 18000;
        else if (SAME(*m, killer[ply][1]))
            m->score = 17000;
        else
            m->score = HIST(*m);
    }
}

static Move __far *pick(int i, int top)
{
    int j, b = i;
    Move t;

    for (j = i + 1; j < top; j++)
        if (stack[j].score > stack[b].score)
            b = j;
    if (b != i) {
        t = stack[i];
        stack[i] = stack[b];
        stack[b] = t;
    }
    return &stack[i];
}

static int quiesce(int alpha, int beta, int ply)
{
    int base, top, i, score;

    nodes++;
    check_time();
    if (stopped)
        return 0;
    score = evaluate();
    if (score >= beta)
        return score;
    if (score > alpha)
        alpha = score;
    if (ply >= MAXPLY - 1)
        return score;
    base = sp;
    generate(1);
    top = sp;
    order(base, top, NULL, ply);
    for (i = base; i < top; i++) {
        if (!make_legal(pick(i, top)))
            continue;
        score = -quiesce(-beta, -alpha, ply + 1);
        unmake();
        if (stopped)
            break;
        if (score > alpha) {
            alpha = score;
            if (score >= beta)
                break;
        }
    }
    sp = base;
    return alpha;
}

static int search(int alpha, int beta, int depth, int ply)
{
    int base, top, i, score, best = -INF, legal = 0, check, old_alpha = alpha;
    Move hashmove, bestmove, __far *m;
    Entry __far *e = NULL;

    pvlen[ply] = 0;
    if (ply > 0 && (fifty >= 100 || repeats() > 0 || no_mating_material()))
        return 0;
    check = IN_CHECK();
    if (check)
        depth++;
    if (depth <= 0 || ply >= MAXPLY - 1)
        return quiesce(alpha, beta, ply);
    nodes++;
    check_time();
    if (stopped)
        return 0;

    memset(&hashmove, 0, sizeof hashmove);
    memset(&bestmove, 0, sizeof bestmove);
    if (tt) {
        e = &tt[(unsigned)hash & (TT_SIZE - 1)];
        if (e->key == hash) {
            hashmove.from = e->from;
            hashmove.to = e->to;
            hashmove.promo = e->promo;
            if (ply > 0 && e->depth >= depth) {
                score = from_tt(e->score, ply);
                if (e->kind == TT_EXACT ||
                    (e->kind == TT_LOWER && score >= beta) ||
                    (e->kind == TT_UPPER && score <= alpha))
                    return score;
            }
        }
    }

    base = sp;
    generate(0);
    top = sp;
    order(base, top, e && e->key == hash ? &hashmove : NULL, ply);
    for (i = base; i < top; i++) {
        m = pick(i, top);
        if (!make_legal(m))
            continue;
        legal++;
        score = -search(-beta, -alpha, depth - 1, ply + 1);
        unmake();
        if (stopped)
            break;
        if (score > best) {
            best = score;
            bestmove = *m;
            if (score > alpha) {
                alpha = score;
                pv[ply][0] = *m;
                memcpy(&pv[ply][1], &pv[ply + 1][0], pvlen[ply + 1] * sizeof(Move));
                pvlen[ply] = pvlen[ply + 1] + 1;
                if (score >= beta) {
                    if (!(m->flags & F_CAPTURE)) {
                        if (!SAME(*m, killer[ply][0])) {
                            killer[ply][1] = killer[ply][0];
                            killer[ply][0] = *m;
                        }
                        if ((HIST(*m) += depth * depth) > 16000)
                            memset(hist, 0, sizeof hist);
                    }
                    break;
                }
            }
        }
    }
    sp = base;
    if (stopped)
        return 0;
    if (legal == 0)
        return check ? -MATE + ply : 0;
    if (e && (e->key != hash || e->depth <= depth)) {
        e->key = hash;
        e->score = to_tt(best, ply);
        e->depth = (unsigned char)depth;
        e->kind = best >= beta ? TT_LOWER : best > old_alpha ? TT_EXACT : TT_UPPER;
        e->from = bestmove.from;
        e->to = bestmove.to;
        e->promo = bestmove.promo;
    }
    return best;
}

/* The screen */

static int flipped;                 /* Black at the bottom */
static int you = WHITE;             /* the side you play */
#define BOARD_ROW 2
#define BOARD_COL 3
#define LIST_COL 36
#define STATUS_ROW 12
#define INFO_ROW 13
#define PROMPT_ROW 15
#define MSG_ROW 16

static char sanbuf[HISTORY][8];     /* the game's moves, as written */

static void clear_row(int row)
{
    static const char blanks[SCR_COLS + 1] =
        "                                                                                ";
    scr_puts(row, 0, blanks);
}

static void message(int row, const char *s)
{
    clear_row(row);
    scr_puts(row, 1, s);
}

static void draw_board(void)
{
    static const char letters[] = " PNBRQK";
    char line[4], label[2] = { 0, 0 };
    int r, f, sq, p, row, col;

    for (r = 0; r < 8; r++) {
        row = BOARD_ROW + (flipped ? r : 7 - r);
        label[0] = (char)('1' + r);
        scr_puts(row, BOARD_COL - 2, label);
        for (f = 0; f < 8; f++) {
            sq = r * 16 + f;
            col = BOARD_COL + 3 * (flipped ? 7 - f : f);
            p = board[sq];
            /* Black's pieces in brackets: on the AWS a p looks like a P, and
             * another attribute for them would take more of a line's 16 */
            line[0] = (char)(COLOR(p) == BLACK ? '(' : ' ');
            line[1] = p ? (char)(letters[TYPE(p)] | (COLOR(p) == BLACK ? 0x20 : 0)) : ' ';
            line[2] = (char)(COLOR(p) == BLACK ? ')' : ' ');
            line[3] = 0;
            scr_puts(row, col, line);
        }
        /* the light squares in reverse, h1 being light */
        for (f = 0; f < 8; f++)
            scr_attr(row, BOARD_COL + 3 * (flipped ? 7 - f : f), 3,
                     (r + f) & 1 ? ATTR_REVERSE : ATTR_NORMAL);
    }
    for (f = 0; f < 8; f++) {
        label[0] = (char)('a' + f);
        scr_puts(BOARD_ROW + 8, BOARD_COL + 1 + 3 * (flipped ? 7 - f : f), label);
    }
}

/* The game's moves, two to a line, the latest at the bottom. */
#define LIST_LINES 10
static void draw_moves(void)
{
    char line[40];
    int start = nundo & 1 ? 24 - side : side;   /* the side that moved first */
    int bf = start == BLACK, lines = (nundo + bf + 1) / 2, first, i, w;

    first = lines > LIST_LINES ? lines - LIST_LINES : 0;
    for (i = 0; i < LIST_LINES; i++) {
        w = (first + i) * 2 - bf;               /* White's move on this line */
        line[0] = 0;
        if (first + i < lines)
            sprintf(line, "%3d. %-7s %s", moveno0 + first + i, w >= 0 ? sanbuf[w] : "...",
                    w + 1 < nundo ? sanbuf[w + 1] : "");
        scr_puts(BOARD_ROW + i, LIST_COL, "                          ");
        scr_puts(BOARD_ROW + i, LIST_COL, line);
    }
}

static void square_name(int sq, char *s)
{
    s[0] = (char)('a' + (sq & 7));
    s[1] = (char)('1' + (sq >> 4));
    s[2] = 0;
}

/* The move as written in algebraic notation, from the current position. */
static void san(const Move __far *m, char *s)
{
    static const char letters[] = "  NBRQK";
    int base = sp, n, i, p = board[m->from], same_file = 0, same_rank = 0, ambiguous = 0;
    const Move __far *o;
    char sq[3];

    if (m->flags & F_CASTLE) {
        strcpy(s, (m->to & 7) == 6 ? "O-O" : "O-O-O");
    } else {
        n = push_legal();
        *s = 0;
        if (TYPE(p) != PAWN) {
            for (i = 0; i < n; i++) {
                o = &stack[base + i];
                if (o->to == m->to && o->from != m->from && board[o->from] == p) {
                    ambiguous = 1;
                    if ((o->from & 7) == (m->from & 7))
                        same_file = 1;
                    if ((o->from >> 4) == (m->from >> 4))
                        same_rank = 1;
                }
            }
            *s++ = letters[TYPE(p)];
            if (ambiguous) {
                square_name(m->from, sq);
                if (!same_file)
                    *s++ = sq[0];
                else if (!same_rank)
                    *s++ = sq[1];
                else {
                    *s++ = sq[0];
                    *s++ = sq[1];
                }
            }
        } else if (m->flags & F_CAPTURE)
            *s++ = (char)('a' + (m->from & 7));
        sp = base;
        if (m->flags & F_CAPTURE)
            *s++ = 'x';
        square_name(m->to, s);
        s += 2;
        if (m->promo) {
            *s++ = '=';
            *s++ = letters[m->promo];
        }
        *s = 0;
    }
    make(m);
    if (IN_CHECK())
        strcat(s, any_legal() ? "+" : "#");
    unmake();
}

/* Reads the move you typed: SAN, or the squares. Returns 0 if it's legal. */
static int parse_move(const char *in, Move *out)
{
    int base = sp, n, i, j;
    char want[12], have[12], buf[12];
    const Move __far *m;
    const char *b;

    for (i = j = 0; in[i] && j < (int)sizeof want - 1; i++) {
        if (strchr("x+#=-!?", in[i]))
            continue;
        want[j++] = in[i] == '0' || in[i] == 'o' ? 'O' : in[i];
    }
    want[j] = 0;
    n = push_legal();
    for (i = 0; i < n; i++) {
        m = &stack[base + i];
        san(m, buf);
        for (b = buf, j = 0; *b; b++)
            if (!strchr("x+#=-", *b))
                have[j++] = *b;
        have[j] = 0;
        if (strcmp(want, have) != 0) {
            /* or as squares: e2e4, e7e8q, and e7e8 for a queen */
            square_name(m->from, have);
            square_name(m->to, have + 2);
            have[4] = m->promo ? "  nbrq"[m->promo] : 0;
            have[5] = 0;
            if (strcmp(in, have) != 0 &&
                !(m->promo == QUEEN && strlen(in) == 4 && strncmp(in, have, 4) == 0))
                continue;
        }
        *out = *m;
        sp = base;
        return 0;
    }
    sp = base;
    return -1;
}

/* Whether the game is over, and if so, how, in s. */
static int game_over(char *s)
{
    if (!any_legal()) {
        if (IN_CHECK())
            strcpy(s, side == WHITE ? "Checkmate: Black wins" : "Checkmate: White wins");
        else
            strcpy(s, "Stalemate: a draw");
        return 1;
    }
    if (fifty >= 100) {
        strcpy(s, "A draw by the fifty-move rule");
        return 1;
    }
    if (repeats() >= 2) {
        strcpy(s, "A draw by repetition");
        return 1;
    }
    if (no_mating_material()) {
        strcpy(s, "A draw: neither side can mate");
        return 1;
    }
    return 0;
}

static void play(const Move *m)
{
    if (nundo < HISTORY - MAXPLY - 1) {
        san(m, sanbuf[nundo]);
        make(m);
    }
}

static void show_score(int score, char *s)
{
    if (score > MATE - 100)
        sprintf(s, "mate in %d", (MATE - score + 1) / 2);
    else if (score < -MATE + 100)
        sprintf(s, "mated in %d", (MATE + score) / 2);
    else
        sprintf(s, "%+d.%02d", score / 100, abs(score) % 100);
}

/* The computer's move; returns 0 if there's none. */
static int think(Move *best)
{
    char info[100], sc[16], buf[8];
    time_t start = time(NULL);
    int depth, score, n, i, k;

    n = push_legal();
    if (n == 0)
        return 0;
    *best = stack[sp - n];
    sp -= n;
    nodes = 0;
    stopped = 0;
    deadline = think_secs > 0 && !max_depth ? start + think_secs : 0;
    memset(killer, 0, sizeof killer);
    message(INFO_ROW, "Thinking... (CANCEL to move now)");
    for (depth = 1; depth <= (max_depth ? max_depth : MAXPLY - 4); depth++) {
        score = search(-INF, INF, depth, 0);
        /* stopped part way, pv holds the best of the moves it finished */
        if (pvlen[0] > 0)
            *best = pv[0][0];
        if (stopped)
            break;
        show_score(score, sc);
        k = sprintf(info, "depth %d  %s  %lu nodes  ", depth, sc, nodes);
        /* the line of play it expects */
        for (i = 0; i < pvlen[0] && i < 6 && k < 70; i++) {
            san(&pv[0][i], buf);
            k += sprintf(info + k, " %s", buf);
            make(&pv[0][i]);
        }
        while (i-- > 0)
            unmake();
        message(INFO_ROW, info);
        if (score > MATE - 100 || score < -MATE + 100 || n == 1)
            break;
        /* another depth would take several times as long */
        if (deadline && time(NULL) - start > think_secs / 3)
            break;
    }
    return 1;
}

static void help(void)
{
    message(MSG_ROW, "Moves: e4 Nf3 exd5 O-O e8=Q, or e2e4.  new  undo  switch  level N  depth N");
    message(MSG_ROW + 1, "perft N  quit (or FINISH).  CANCEL while it thinks makes it move now.");
}

static int read_line(char *buf, int size)
{
    int n = 0, k;

    buf[0] = 0;
    for (;;) {
        message(PROMPT_ROW, side == WHITE ? "White's move: " : "Black's move: ");
        scr_puts(PROMPT_ROW, 15, buf);
        scr_cursor(PROMPT_ROW, 15 + n);
        k = scr_getkey();
        if (k == KEY_FINISH)
            return -1;
        if (k == KEY_RETURN || k == '\r')
            break;
        if (k == KEY_HELP) {
            help();
            continue;
        }
        if ((k == KEY_BACKSPACE || k == KEY_DELETE) && n > 0)
            buf[--n] = 0;
        else if (k >= ' ' && k < 0x7F && n < size - 1) {
            buf[n++] = (char)k;
            buf[n] = 0;
        }
    }
    return n;
}

static unsigned long perft(int depth)
{
    unsigned long n = 0;
    int base = sp, i, top;

    if (depth == 0)
        return 1;
    generate(0);
    top = sp;
    for (i = base; i < top; i++) {
        if (make_legal(&stack[i])) {
            n += perft(depth - 1);
            unmake();
        }
    }
    sp = base;
    return n;
}

static void redraw(void)
{
    char s[60];

    draw_board();
    draw_moves();
    if (game_over(s))
        message(STATUS_ROW, s);
    else
        message(STATUS_ROW, IN_CHECK() ? (side == WHITE ? "White to move, in check" :
                                          "Black to move, in check")
                                       : side == WHITE ? "White to move" : "Black to move");
}

static const char start_fen[] = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

int main(int argc, char **argv)
{
    char line[40], s[80], fen[100];
    Move m;
    int c, i;

    while ((c = getopt(argc, argv, "bt:d:")) != -1) {
        switch (c) {
        case 'b': you = BLACK; break;
        case 't': think_secs = atoi(optarg); break;
        case 'd': max_depth = atoi(optarg); break;
        default:
            printf("usage: chess [-b] [-t seconds] [-d depth] ['position']\n");
            return 0;
        }
    }
    init_tables();
    /* the Executive gives a quoted FEN as one argument, but several also work */
    fen[0] = 0;
    for (i = optind; i < argc && strlen(fen) + strlen(argv[i]) + 2 < sizeof fen; i++) {
        if (i > optind)
            strcat(fen, " ");
        strcat(fen, argv[i]);
    }
    if (set_fen(fen[0] ? fen : start_fen)) {
        printf("chess: that position doesn't make sense\n");
        return 0;
    }
    if ((stack = farmalloc(MOVESTACK * sizeof(Move))) == 0) {
        printf("chess: not enough memory\n");
        return 0;
    }
    tt = farmalloc(TT_SIZE * sizeof(Entry));
    if (tt)
        _fmemset(tt, 0, TT_SIZE * sizeof(Entry));
    flipped = you == BLACK;
    scr_clear();
    scr_puts(0, 1, "Chess");
    scr_puts(0, 50, "HELP for the commands");
    redraw();
    for (;;) {
        if (side != you && !game_over(s)) {
            if (think(&m)) {
                play(&m);
                redraw();
            }
            continue;
        }
        if (read_line(line, sizeof line) < 0)
            break;
        message(MSG_ROW, "");
        clear_row(MSG_ROW + 1);
        if (line[0] == 0)
            continue;
        if (strcmp(line, "quit") == 0)
            break;
        if (strcmp(line, "help") == 0) {
            help();
        } else if (strcmp(line, "new") == 0) {
            set_fen(start_fen);
            you = flipped ? BLACK : WHITE;
            clear_row(INFO_ROW);
            redraw();
        } else if (strcmp(line, "undo") == 0) {
            /* back to your previous turn */
            if (nundo > 0)
                unmake();
            while (nundo > 0 && side != you)
                unmake();
            clear_row(INFO_ROW);
            redraw();
        } else if (strcmp(line, "switch") == 0) {
            you = 24 - side;
            flipped = you == BLACK;
            redraw();
        } else if (strncmp(line, "level ", 6) == 0) {
            think_secs = atoi(line + 6);
            max_depth = 0;
            sprintf(s, "Up to %d seconds a move", think_secs);
            message(MSG_ROW, s);
        } else if (strncmp(line, "depth ", 6) == 0) {
            max_depth = atoi(line + 6);
            sprintf(s, max_depth ? "Searching %d moves ahead" : "Searching for %d seconds",
                    max_depth ? max_depth : think_secs);
            message(MSG_ROW, s);
        } else if (strncmp(line, "perft ", 6) == 0) {
            i = atoi(line + 6);
            message(MSG_ROW, "Counting...");
            sprintf(s, "perft %d: %lu positions", i, perft(i));
            message(MSG_ROW, s);
        } else if (game_over(s)) {
            message(MSG_ROW, "The game is over: new starts another, or undo");
        } else if (parse_move(line, &m) == 0) {
            play(&m);
            clear_row(INFO_ROW);
            redraw();
        } else {
            sprintf(s, "%s isn't a legal move here", line);
            message(MSG_ROW, s);
        }
    }
    scr_clear();
    return 0;
}
