/* fonttest.c - checks scr_loadfont. It copies the system font to test.font
 * with the letter A made a checkerboard, loads that, shows some As, and after
 * a key loads the system font again. It prints what each load returned:
 * "load 0, restore 0". On the AWS, whose font is in ROM, the loads succeed
 * but change nothing. The first argument is the directory for test.font,
 * such as [f0]<C>. */
#include <errno.h>
#include <screen.h>
#include <stdio.h>
#include <string.h>

#define SYSFONT "[Sys]<Sys>t1Sys.font"

int main(int argc, char **argv)
{
    static unsigned char font[8201];
    char name[64];
    FILE *f;
    size_t n;
    int i, load = -2, restore = -2;

    strcpy(name, argc > 1 ? argv[1] : "");
    strcat(name, "test.font");
    f = fopen(SYSFONT, "rb");
    if (f == NULL) {
        printf("no %s (error %d)\n", SYSFONT, errno);
    } else {
        n = fread(font, 1, sizeof font, f);
        fclose(f);
        /* character 'A' is words 1-15 of its 16 */
        for (i = 1; i < 16; i++) {
            font['A' * 32 + 2 * i] = (unsigned char)(i & 1 ? 0xAA : 0x55);
            font['A' * 32 + 2 * i + 1] = (unsigned char)(i & 1 ? 0x00 : 0x01);
        }
        f = fopen(name, "wb");
        if (f && fwrite(font, 1, n, f) == n && fclose(f) == 0)
            load = scr_loadfont(name);
        scr_puts(10, 0, "AAAA: checkered on the NGEN");
        scr_getkey();
        restore = scr_loadfont(SYSFONT);
        remove(name);
    }
    scr_line(12);
    printf("load %d, restore %d\n", load, restore);
    return 0;
}
