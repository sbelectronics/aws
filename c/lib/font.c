/* font.c - loading character shapes, on a machine whose font is in RAM */
#include <ctos.h>
#include <errno.h>
#include <farmem.h>
#include <screen.h>
#include <string.h>

#define FONTBUF 0x2200      /* the buffer size Convergent's own programs use */

int scr_loadfont(const char *name)
{
    unsigned fh;
    void __far *buf;
    ErcType erc;

    if ((erc = OpenFile(&fh, name, strlen(name), "", 0, modeRead)) != 0) {
        errno = erc;
        return -1;
    }
    if ((buf = farmalloc(FONTBUF)) == 0) {
        CloseFile(fh);
        return -1;
    }
    SetScreenVidAttr(1, 0);
    erc = LoadFontRam(fh, buf, FONTBUF);
    SetScreenVidAttr(1, 0xFF);
    farfree(buf);
    CloseFile(fh);
    if (erc) {
        errno = erc;
        return -1;
    }
    return 0;
}
