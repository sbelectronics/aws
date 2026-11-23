/* patrol - clear the raiders out of the Reach before time runs out.
 *
 *   patrol [-s seed] [-e]
 *
 * You command the patrol cutter Kestrel. The Reach is 8 by 8 zones, each 8
 * by 8 sectors, with raiders hiding in some of them, depots that restock
 * you, and stars in the way. Destroy every raider before the deadline. -e
 * gives an easier game, with fewer raiders and more time; -s picks the
 * Reach by number, so a game can be played again.
 *
 * Commands, by their first letter or more:
 *   move heading sectors   heading in degrees: 0 is up, 90 right
 *   lance energy           a beam, its energy split among the raiders in
 *                          the zone; it weakens with distance
 *   dart [heading]         a missile, aimed at the nearest raider unless
 *                          given a heading; stars stop it
 *   shields level          moves energy into the shields or back
 *   chart                  scans the zones around and shows what's known
 *   nav zx zy [sx sy]      the heading and distance to a zone and sector
 *   help, quit (or FINISH)
 *
 * Raiders close in and fire after each thing you do, though not as you
 * arrive. Shields take the hits; when they're gone, so is the Kestrel. Next to a depot you're docked:
 * restocked, mended, and safe. Hard hits damage systems, which mend with
 * time. Moving takes a tenth of a day for each sector. */
#include <screen.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define EMPTY  0
#define SHIP   1
#define RAIDER 2
#define DEPOT  3
#define STAR   4

#define FULL_ENERGY 3000
#define FULL_DARTS  10

/* systems */
#define ENGINES 0
#define LANCE   1
#define DARTS   2
#define SCANNER 3
#define CHART   4
#define NSYS    5
static const char *const sysname[NSYS] = { "engines", "lance", "dart tubes", "scanner", "chart" };

/* sin of 0 to 90 degrees, in 256ths */
static const short sintab[91] = {
      0,   4,   9,  13,  18,  22,  27,  31,  36,  40,  44,  49,  53,
     58,  62,  66,  71,  75,  79,  83,  88,  92,  96, 100, 104, 108,
    112, 116, 120, 124, 128, 132, 136, 139, 143, 147, 150, 154, 158,
    161, 165, 168, 171, 175, 178, 181, 184, 187, 190, 193, 196, 199,
    202, 204, 207, 210, 212, 215, 217, 219, 222, 224, 226, 228, 230,
    232, 234, 236, 237, 239, 241, 242, 243, 245, 246, 247, 248, 249,
    250, 251, 252, 253, 254, 254, 255, 255, 255, 256, 256, 256, 256,
};

static unsigned char zraiders[8][8], zdepot[8][8], zstars[8][8], known[8][8];
static unsigned char grid[8][8];        /* the zone you're in, [y][x] */
static struct {
    int x, y, hp;
} raider[8];
static int nraiders;                    /* in this zone */
static int zx, zy, sx, sy;              /* your zone and sector */
static int energy, shields, darts, left, docked, over;
static int arrived;                     /* just came into a zone with raiders */
static int now, deadline;               /* in tenths of a day */
static int damaged[NSYS];               /* tenths of a day until mended */
static int start_raiders;

static unsigned long seed;
static int rnd(int n)                   /* 0 to n - 1 */
{
    seed = (seed * 1103515245UL + 12345UL) & 0xFFFFFFFFUL;
    return (int)((seed >> 16) % (unsigned)n);
}

/* sin and cos in 256ths, of a heading in degrees */
static int isin(int d)
{
    d = ((d % 360) + 360) % 360;
    if (d <= 90)
        return sintab[d];
    if (d <= 180)
        return isin(180 - d);
    return -isin(d - 180);
}
#define ICOS(d) isin((d) + 90)

static int isqrt(long n)
{
    long r = 0;

    while ((r + 1) * (r + 1) <= n)
        r++;
    return (int)r;
}

/* The heading from one place to another, in degrees, 0 being up. */
static int heading_to(long dx, long dy)
{
    int d, best = 0;
    long err, best_err = -1;

    /* the heading whose direction is closest: the cross product is smallest
     * and the dot product positive */
    for (d = 0; d < 360; d++) {
        long ux = isin(d), uy = -ICOS(d);
        err = dx * uy - dy * ux;
        if (err < 0)
            err = -err;
        if (dx * ux + dy * uy < 0)
            continue;
        if (best_err < 0 || err < best_err) {
            best_err = err;
            best = d;
        }
    }
    return best;
}

/* The screen */

#define GRID_ROW 2
#define GRID_COL 3
#define STAT_COL 38
#define MSG_ROW 13
static int rows, msg_lines;
static char msgs[12][SCR_COLS + 1];
static int nmsgs;

static void blank(int row, int col, int n)
{
    static const char spaces[SCR_COLS + 1] =
        "                                                                                ";
    scr_puts(row, col, spaces + SCR_COLS - n);
}

#define clear_row(row) blank(row, 0, SCR_COLS)

static void show_messages(void)
{
    int i;

    for (i = 0; i < msg_lines; i++) {
        clear_row(MSG_ROW + i);
        if (i < nmsgs)
            scr_puts(MSG_ROW + i, 1, msgs[i]);
    }
}

static void say(const char *fmt, ...)
{
    va_list ap;

    if (nmsgs == msg_lines) {
        memmove(msgs[0], msgs[1], (msg_lines - 1) * sizeof msgs[0]);
        nmsgs--;
    }
    va_start(ap, fmt);
    vsnprintf(msgs[nmsgs++], sizeof msgs[0], fmt, ap);
    va_end(ap);
}

static void draw_zone(void)
{
    static const char *const shape[] = { " . ", "{K}", ">R<", "[D]", " * " };
    char label[4];
    int x, y;

    for (y = 0; y < 8; y++) {
        sprintf(label, "%d", y + 1);
        scr_puts(GRID_ROW + y, GRID_COL - 2, label);
        for (x = 0; x < 8; x++)
            scr_puts(GRID_ROW + y, GRID_COL + 3 * x,
                     damaged[SCANNER] && grid[y][x] != SHIP ? " ? " : shape[grid[y][x]]);
    }
    scr_puts(GRID_ROW + 8, GRID_COL, " 1  2  3  4  5  6  7  8 ");
}

/* What's known of each zone: its raiders, and d for a depot. */
static void draw_chart(void)
{
    char cell[6], label[4];
    int x, y;

    for (y = 0; y < 8; y++) {
        sprintf(label, "%d", y + 1);
        scr_puts(GRID_ROW + y, GRID_COL - 2, label);
        for (x = 0; x < 8; x++) {
            if (!known[y][x])
                strcpy(cell, "  ? ");
            else
                sprintf(cell, " %c%c ", zraiders[y][x] ? '0' + zraiders[y][x] : '-',
                        zdepot[y][x] ? 'd' : ' ');
            if (x == zx && y == zy) {
                cell[0] = '<';
                cell[3] = '>';
            }
            scr_puts(GRID_ROW + y, GRID_COL + 4 * x - 1, cell);
        }
    }
    scr_puts(GRID_ROW + 8, GRID_COL, "  1   2   3   4   5   6   7   8 ");
}

static void draw_status(void)
{
    char s[40];
    int i, r = GRID_ROW;

    for (i = GRID_ROW; i < GRID_ROW + 10; i++)
        blank(i, STAT_COL, SCR_COLS - STAT_COL);
    sprintf(s, "Day %d.%d of %d", now / 10, now % 10, deadline / 10);
    scr_puts(r++, STAT_COL, s);
    sprintf(s, "Condition %s", docked ? "docked" : nraiders ? "RED" :
            energy + shields < 600 ? "yellow" : "green");
    scr_puts(r++, STAT_COL, s);
    sprintf(s, "Zone %d,%d  sector %d,%d", zx + 1, zy + 1, sx + 1, sy + 1);
    scr_puts(r++, STAT_COL, s);
    sprintf(s, "Energy %d", energy);
    scr_puts(r++, STAT_COL, s);
    sprintf(s, "Shields %d", shields);
    scr_puts(r++, STAT_COL, s);
    sprintf(s, "Darts %d", darts);
    scr_puts(r++, STAT_COL, s);
    sprintf(s, "Raiders left %d", left);
    scr_puts(r++, STAT_COL, s);
    for (i = 0; i < NSYS; i++) {
        if (damaged[i] && r < GRID_ROW + 10) {
            sprintf(s, "Damaged: %s, %d.%d days", sysname[i], damaged[i] / 10, damaged[i] % 10);
            scr_puts(r++, STAT_COL, s);
        }
    }
}

static void redraw(int chart)
{
    int r;

    for (r = GRID_ROW; r <= GRID_ROW + 8; r++)
        blank(r, 0, STAT_COL);
    if (chart)
        draw_chart();
    else
        draw_zone();
    draw_status();
    show_messages();
}

/* The Reach */

static void new_game(int easy)
{
    int x, y, n;

    memset(zraiders, 0, sizeof zraiders);
    memset(zdepot, 0, sizeof zdepot);
    memset(known, 0, sizeof known);
    left = 0;
    for (y = 0; y < 8; y++) {
        for (x = 0; x < 8; x++) {
            n = rnd(100);
            zraiders[y][x] = (unsigned char)(n < 3 ? 3 : n < 8 ? 2 : n < (easy ? 15 : 22) ? 1 : 0);
            left += zraiders[y][x];
            zstars[y][x] = (unsigned char)(1 + rnd(6));
        }
    }
    if (left < 6) {
        zraiders[3][4] += 2;
        left += 2;
    }
    for (n = 0; n < 3; n++)
        zdepot[rnd(8)][rnd(8)] = 1;
    start_raiders = left;
    zx = rnd(8);
    zy = rnd(8);
    sx = rnd(8);
    sy = rnd(8);
    shields = 500;
    energy = FULL_ENERGY - shields;
    darts = FULL_DARTS;
    now = 0;
    deadline = (left * 25 + (easy ? 120 : 50)) / 10 * 10;
    memset(damaged, 0, sizeof damaged);
}

static void place(int what, int *px, int *py)
{
    int x, y;

    do {
        x = rnd(8);
        y = rnd(8);
    } while (grid[y][x] != EMPTY);
    grid[y][x] = (unsigned char)what;
    if (px) {
        *px = x;
        *py = y;
    }
}

static void check_docked(void)
{
    int x, y;

    docked = 0;
    for (y = sy - 1; y <= sy + 1; y++)
        for (x = sx - 1; x <= sx + 1; x++)
            if (x >= 0 && x < 8 && y >= 0 && y < 8 && grid[y][x] == DEPOT)
                docked = 1;
    if (docked && (energy + shields < FULL_ENERGY || darts < FULL_DARTS)) {
        energy = FULL_ENERGY - shields;
        darts = FULL_DARTS;
        memset(damaged, 0, sizeof damaged);
        say("Docked: the depot restocks and mends the Kestrel.");
    }
}

/* Lays out the zone you've come into. */
static void enter_zone(void)
{
    int i;

    memset(grid, 0, sizeof grid);
    grid[sy][sx] = SHIP;
    known[zy][zx] = 1;
    nraiders = zraiders[zy][zx];
    for (i = 0; i < nraiders; i++) {
        place(RAIDER, &raider[i].x, &raider[i].y);
        raider[i].hp = 150 + rnd(150);
    }
    if (zdepot[zy][zx])
        place(DEPOT, NULL, NULL);
    for (i = 0; i < zstars[zy][zx]; i++)
        place(STAR, NULL, NULL);
    arrived = nraiders > 0;
    if (nraiders)
        say("Zone %d,%d: %d raider%s here!", zx + 1, zy + 1, nraiders, nraiders > 1 ? "s" : "");
    check_docked();
}

static int dist_to(int x, int y)
{
    return isqrt((long)(x - sx) * (x - sx) + (long)(y - sy) * (y - sy));
}

static void kill_raider(int i)
{
    say("The raider at %d,%d is destroyed.", raider[i].x + 1, raider[i].y + 1);
    grid[raider[i].y][raider[i].x] = EMPTY;
    raider[i] = raider[--nraiders];
    zraiders[zy][zx]--;
    left--;
}

/* Time passes: systems mend, and the deadline comes closer. */
static void pass_time(int tenths)
{
    int i;

    now += tenths;
    for (i = 0; i < NSYS; i++) {
        if (damaged[i]) {
            damaged[i] = damaged[i] > tenths ? damaged[i] - tenths : 0;
            if (!damaged[i])
                say("Mended: the %s.", sysname[i]);
        }
    }
}

/* The raiders' turn: each comes a sector closer, then fires. */
static void raiders_turn(void)
{
    int i, d, hit, dx, dy, nx, ny, s;

    if (docked) {
        if (nraiders)
            say("The depot's guns keep the raiders off.");
        return;
    }
    if (arrived) {
        say("Taken by surprise, they hold their fire for a moment.");
        arrived = 0;
        return;
    }
    for (i = 0; i < nraiders && !over; i++) {
        dx = sx > raider[i].x ? 1 : sx < raider[i].x ? -1 : 0;
        dy = sy > raider[i].y ? 1 : sy < raider[i].y ? -1 : 0;
        nx = raider[i].x + dx;
        ny = raider[i].y + dy;
        if (dist_to(raider[i].x, raider[i].y) > 1 && grid[ny][nx] == EMPTY) {
            grid[raider[i].y][raider[i].x] = EMPTY;
            grid[ny][nx] = RAIDER;
            raider[i].x = nx;
            raider[i].y = ny;
        }
        d = dist_to(raider[i].x, raider[i].y);
        hit = (40 + rnd(60)) * 6 / (2 + d);
        shields -= hit;
        if (shields < 0) {
            say("A raider's hit of %d gets through. The Kestrel is lost.", hit);
            over = 1;
            return;
        }
        say("Hit for %d from %d,%d; shields %d.", hit, raider[i].x + 1, raider[i].y + 1, shields);
        if (hit > 60 && rnd(3) == 0) {
            s = rnd(NSYS);
            damaged[s] += 5 + rnd(25);
            say("Damaged: the %s.", sysname[s]);
        }
    }
}

/* Commands */

static void cmd_move(int heading, int n)
{
    long x, y;
    int step, gx, gy, ox = zx, oy = zy, moved = 0, max = damaged[ENGINES] ? 2 : 64;

    if (n < 1 || n > max) {
        say(damaged[ENGINES] ? "With the engines damaged, 2 sectors at most." : "1 to 64 sectors.");
        return;
    }
    if (energy < n * 4 + 10) {
        say("Not enough energy to go that far.");
        return;
    }
    x = (long)(zx * 8 + sx) * 256 + 128;
    y = (long)(zy * 8 + sy) * 256 + 128;
    grid[sy][sx] = EMPTY;
    for (step = 0; step < n; step++) {
        x += isin(heading);
        y -= ICOS(heading);
        if (x < 0 || y < 0 || x >= 64L * 256 || y >= 64L * 256) {
            say("That's the edge of the Reach.");
            break;
        }
        gx = (int)(x / 256);
        gy = (int)(y / 256);
        if (gx / 8 == zx && gy / 8 == zy && grid[gy % 8][gx % 8] != EMPTY) {
            say("Stopped short of %s at %d,%d.", grid[gy % 8][gx % 8] == STAR ? "a star" :
                grid[gy % 8][gx % 8] == DEPOT ? "the depot" : "a raider", gx % 8 + 1, gy % 8 + 1);
            break;
        }
        sx = gx % 8;
        sy = gy % 8;
        if (gx / 8 != zx || gy / 8 != zy) {
            /* out of this zone: the others aren't laid out until you stop */
            zx = gx / 8;
            zy = gy / 8;
            memset(grid, 0, sizeof grid);
            nraiders = 0;
        }
        moved++;
    }
    energy -= moved * 4 + 10;
    pass_time(moved > 0 ? moved : 1);
    if (zx != ox || zy != oy)
        enter_zone();
    else {
        grid[sy][sx] = SHIP;
        check_docked();
    }
}

static void cmd_lance(int e)
{
    int i, share, d, hit;

    if (damaged[LANCE]) {
        say("The lance is damaged.");
        return;
    }
    if (nraiders == 0) {
        say("There are no raiders in this zone.");
        return;
    }
    if (e <= 0 || e > energy) {
        say("You have %d energy for it.", energy);
        return;
    }
    energy -= e;
    share = e / nraiders;
    for (i = 0; i < nraiders; i++) {
        d = dist_to(raider[i].x, raider[i].y);
        hit = (int)((long)share * (75 + rnd(50)) / 100 * 4 / (3 + d));
        raider[i].hp -= hit;
        if (raider[i].hp <= 0)
            kill_raider(i--);
        else
            say("The lance hits the raider at %d,%d for %d; it has %d left.",
                raider[i].x + 1, raider[i].y + 1, hit, raider[i].hp);
    }
    pass_time(1);
}

static void cmd_dart(int heading, int aimed)
{
    long x, y;
    int i, gx, gy, best = -1, step;

    if (damaged[DARTS]) {
        say("The dart tubes are damaged.");
        return;
    }
    if (darts == 0) {
        say("You have no darts left.");
        return;
    }
    if (!aimed) {
        if (nraiders == 0) {
            say("There's no raider in this zone to aim at.");
            return;
        }
        for (i = 0; i < nraiders; i++)
            if (best < 0 || dist_to(raider[i].x, raider[i].y) < dist_to(raider[best].x, raider[best].y))
                best = i;
        heading = heading_to(raider[best].x - sx, raider[best].y - sy);
    }
    darts--;
    x = (long)sx * 256 + 128;
    y = (long)sy * 256 + 128;
    for (step = 0; step < 48; step++) {
        x += isin(heading) / 4;
        y -= ICOS(heading) / 4;
        if (x < 0 || y < 0 || x >= 8 * 256 || y >= 8 * 256)
            break;
        gx = (int)(x / 256);
        gy = (int)(y / 256);
        switch (grid[gy][gx]) {
        case RAIDER:
            for (i = 0; raider[i].x != gx || raider[i].y != gy; i++)
                ;
            kill_raider(i);
            pass_time(1);
            return;
        case STAR:
            say("The dart (heading %d) burns up in the star at %d,%d.", heading, gx + 1, gy + 1);
            pass_time(1);
            return;
        case DEPOT:
            say("The depot shoots the dart down.");
            pass_time(1);
            return;
        }
    }
    say("The dart (heading %d) leaves the zone without hitting anything.", heading);
    pass_time(1);
}

static void cmd_shields(int level)
{
    if (level < 0 || level > energy + shields) {
        say("Shields can be 0 to %d.", energy + shields);
        return;
    }
    energy += shields - level;
    shields = level;
    say("Shields at %d.", shields);
}

static void cmd_chart(void)
{
    int x, y;

    if (damaged[CHART]) {
        say("The chart is damaged.");
        return;
    }
    for (y = zy - 1; y <= zy + 1; y++)
        for (x = zx - 1; x <= zx + 1; x++)
            if (x >= 0 && x < 8 && y >= 0 && y < 8)
                known[y][x] = 1;
    say("The chart: raiders in each zone, d for a depot, ? not yet seen.");
}

static void cmd_nav(int tx, int ty, int tsx, int tsy)
{
    long dx = (long)(tx * 8 + tsx) - (zx * 8 + sx), dy = (long)(ty * 8 + tsy) - (zy * 8 + sy);

    if (tx < 0 || tx > 7 || ty < 0 || ty > 7 || tsx < 0 || tsx > 7 || tsy < 0 || tsy > 7) {
        say("Zones and sectors are 1 to 8.");
        return;
    }
    if (dx == 0 && dy == 0) {
        say("You're there.");
        return;
    }
    say("To zone %d,%d sector %d,%d: heading %d, %d sectors.", tx + 1, ty + 1, tsx + 1, tsy + 1,
        heading_to(dx, dy), isqrt(dx * dx + dy * dy));
}

static void help(void)
{
    say("move heading sectors (0 up, 90 right), lance energy, dart [heading],");
    say("shields level, chart, nav zx zy [sx sy], quit. First letters will do.");
}

static int read_line(char *buf, int size)
{
    int n = 0, k;

    buf[0] = 0;
    for (;;) {
        clear_row(rows - 2);
        scr_puts(rows - 2, 1, "Command: ");
        scr_puts(rows - 2, 10, buf);
        scr_cursor(rows - 2, 10 + n);
        k = scr_getkey();
        if (k == KEY_FINISH)
            return -1;
        if (k == KEY_RETURN || k == '\r')
            return n;
        if (k == KEY_HELP) {
            help();
            show_messages();
        } else if ((k == KEY_BACKSPACE || k == KEY_DELETE) && n > 0)
            buf[--n] = 0;
        else if (k >= ' ' && k < 0x7F && n < size - 1) {
            buf[n++] = (char)k;
            buf[n] = 0;
        }
    }
}

int main(int argc, char **argv)
{
    char line[40], word[16];
    int c, a[4], n, easy = 0, chart = 0, acted;

    seed = (unsigned long)time(NULL);
    while ((c = getopt(argc, argv, "s:e")) != -1) {
        switch (c) {
        case 's': seed = (unsigned long)atol(optarg); break;
        case 'e': easy = 1; break;
        default:
            printf("usage: patrol [-s seed] [-e]\n");
            return 0;
        }
    }
    rows = scr_rows();
    msg_lines = rows - 2 - MSG_ROW;
    if (msg_lines > 12)
        msg_lines = 12;
    new_game(easy);
    scr_clear();
    scr_puts(0, 1, "Patrol");
    scr_puts(0, 50, "HELP for the commands");
    say("%d raiders are loose in the Reach. Clear them out by day %d.", left, deadline / 10);
    say("Type help for the commands.");
    enter_zone();
    while (!over) {
        redraw(chart);
        chart = 0;
        if (read_line(line, sizeof line) < 0)
            break;
        nmsgs = 0;
        a[0] = a[1] = a[2] = a[3] = -1;
        n = sscanf(line, "%15s %d %d %d %d", word, &a[0], &a[1], &a[2], &a[3]);
        if (n < 1)
            continue;
        acted = 1;
        if (strncmp("move", word, strlen(word)) == 0 && n >= 3)
            cmd_move(a[0], a[1]);
        else if (strncmp("lance", word, strlen(word)) == 0 && n >= 2)
            cmd_lance(a[0]);
        else if (strncmp("dart", word, strlen(word)) == 0)
            cmd_dart(a[0], n >= 2);
        else {
            acted = 0;
            if (strncmp("shields", word, strlen(word)) == 0 && n >= 2)
                cmd_shields(a[0]);
            else if (strncmp("chart", word, strlen(word)) == 0) {
                cmd_chart();
                chart = !damaged[CHART];
            } else if (strncmp("nav", word, strlen(word)) == 0 && n >= 3)
                cmd_nav(a[0] - 1, a[1] - 1, n >= 5 ? a[2] - 1 : 3, n >= 5 ? a[3] - 1 : 3);
            else if (strncmp("quit", word, strlen(word)) == 0)
                break;
            else
                help();
        }
        if (acted)
            raiders_turn();
        if (left == 0) {
            say("The last raider is gone, on day %d.%d. The Reach is safe: well done!",
                now / 10, now % 10);
            over = 1;
        } else if (!over && now >= deadline) {
            say("Day %d: time is up, with %d raiders still loose.", deadline / 10, left);
            over = 1;
        } else if (!over && energy + shields < 14 && !docked) {
            say("The Kestrel is out of energy, adrift.");
            over = 1;
        }
    }
    if (over) {
        say("You destroyed %d of %d raiders. FINISH or RETURN to leave.",
            start_raiders - left, start_raiders);
        redraw(0);
        scr_getkey();
    }
    scr_clear();
    return 0;
}
