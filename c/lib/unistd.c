/* unistd.c - getcwd and getopt */
#include <ctosmsg.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#define rqGetUCB 27

/* The User Control Block starts with a login id, then the default volume and
 * directory, each a length byte and up to 12 characters. */
#pragma pack(push, 1)
struct ucb {
    unsigned loginId;
    unsigned char vol[13], dir[13];
};
#pragma pack(pop)

char *getcwd(char *buf, int size)
{
    static unsigned ucbbuf[(sizeof(struct ucb) + 1) / 2];    /* the request wants it even */
    struct ucb *u = (struct ucb *)ucbbuf;
#pragma pack(push, 1)
    struct {
        RqHeader h;
        unsigned char reserved[6];
        void __far *pUcbRet;
        unsigned sUcbMax;
    } rq;
#pragma pack(pop)
    ErcType erc;

    memset(&rq, 0, sizeof rq);
    rq.h.sCntInfo = 6;
    rq.h.nRespPbCb = 1;
    rq.h.rqCode = rqGetUCB;
    rq.pUcbRet = u;
    rq.sUcbMax = sizeof *u;
    if ((erc = DoRequest(&rq)) != 0) {
        errno = erc;
        return NULL;
    }
    if (u->vol[0] > 12 || u->dir[0] > 12 || u->vol[0] + u->dir[0] + 5 > size)
        return NULL;
    buf[0] = 0;
    if (u->vol[0]) {
        strcat(buf, "[");
        strncat(buf, (char *)u->vol + 1, u->vol[0]);
        strcat(buf, "]");
    }
    if (u->dir[0]) {
        strcat(buf, "<");
        strncat(buf, (char *)u->dir + 1, u->dir[0]);
        strcat(buf, ">");
    }
    return buf;
}

char *optarg;
int optind = 1, optopt;

int getopt(int argc, char **argv, const char *opts)
{
    static int pos = 1;         /* the next letter in argv[optind] */
    const char *o;
    int c;

    if (optind >= argc || argv[optind][0] != '-' || argv[optind][1] == 0)
        return -1;
    if (strcmp(argv[optind], "--") == 0) {
        optind++;
        return -1;
    }
    c = argv[optind][pos++];
    o = strchr(opts, c);
    if (c == ':' || o == NULL) {
        optopt = c;
        c = '?';
    } else if (o[1] == ':') {
        if (argv[optind][pos])                  /* -k2 */
            optarg = argv[optind] + pos;
        else if (optind + 1 < argc)             /* -k 2 */
            optarg = argv[++optind];
        else {
            optopt = c;
            c = '?';
        }
        pos = 0;
    }
    if (pos == 0 || argv[optind][pos] == 0) {
        optind++;
        pos = 1;
    }
    return c;
}
