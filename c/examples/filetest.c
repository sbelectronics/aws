/* filetest.c - checks argc and argv, and file I/O. The first argument is the
 * directory for the test files, for example [f0]<Zork>; without it they go in
 * the current path. Leaves keep.txt (63 bytes) behind, for checking from
 * outside. Prints each failure, then a count; exits with the number of
 * failures. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int checks, failures;
static char dir[64], path[96], path2[96];

static void check(int ok, int line, const char *what)
{
    checks++;
    if (!ok) {
        failures++;
        printf("FAIL line %d: %s (errno %d)\n", line, what, errno);
    }
}

#define CHECK(e) check((e) != 0, __LINE__, #e)

static const char *name(const char *file)
{
    strcpy(path, dir);
    strcat(path, file);
    return path;
}

#define LINES 200

static long text_file(void)
{
    char want[40], got[40];
    long len = 0;
    int i, ok;
    FILE *f;

    f = fopen(name("test.txt"), "w");
    CHECK(f != NULL);
    if (f == NULL)
        return 0;
    for (i = 0; i < LINES; i++)
        len += fprintf(f, "line %d of the test file\n", i);
    CHECK(ftell(f) == len);
    CHECK(fclose(f) == 0);

    f = fopen(name("test.txt"), "r");
    CHECK(f != NULL);
    if (f == NULL)
        return 0;
    for (ok = 1, i = 0; i < LINES; i++) {
        sprintf(want, "line %d of the test file\n", i);
        ok &= fgets(got, sizeof got, f) != NULL && strcmp(got, want) == 0;
    }
    CHECK(ok);
    CHECK(fgets(got, sizeof got, f) == NULL && feof(f));
    CHECK(ftell(f) == len);
    CHECK(fseek(f, 0L, SEEK_END) == 0 && ftell(f) == len);
    CHECK(fseek(f, 5L, SEEK_SET) == 0 && fgetc(f) == '0');
    i = fgetc(f);
    CHECK(i == ' ' && ungetc(i, f) == i && ftell(f) == 6 && fgetc(f) == ' ');
    CHECK(fclose(f) == 0);
    return len;
}

static void append_and_update(long len)
{
    char got[16];
    FILE *f;

    f = fopen(name("test.txt"), "a");
    CHECK(f != NULL && fputs("tail\n", f) == 0 && fclose(f) == 0);
    f = fopen(name("test.txt"), "r+");
    CHECK(f != NULL);
    if (f == NULL)
        return;
    CHECK(fseek(f, -5L, SEEK_END) == 0 && fgets(got, sizeof got, f) && strcmp(got, "tail\n") == 0);
    CHECK(ftell(f) == len + 5);
    CHECK(fseek(f, 600L, SEEK_SET) == 0 && fputc('X', f) == 'X');
    CHECK(fclose(f) == 0);
    f = fopen(name("test.txt"), "r");
    CHECK(f != NULL);
    if (f == NULL)
        return;
    CHECK(fseek(f, 600L, SEEK_SET) == 0 && fgetc(f) == 'X');
    CHECK(fseek(f, 0L, SEEK_END) == 0 && ftell(f) == len + 5);
    CHECK(fclose(f) == 0);
}

static void binary_file(void)
{
    static unsigned char out[3000], in[3000];
    unsigned i;
    FILE *f;

    for (i = 0; i < sizeof out; i++)
        out[i] = (unsigned char)(i * 7);
    f = fopen(name("bin.dat"), "w+");
    CHECK(f != NULL);
    if (f == NULL)
        return;
    CHECK(fwrite(out, 1, sizeof out, f) == sizeof out);
    rewind(f);
    CHECK(fread(in, 1, sizeof in, f) == sizeof in && memcmp(in, out, sizeof in) == 0);
    CHECK(fread(in, 1, 1, f) == 0 && feof(f));
    CHECK(fseek(f, 1000L, SEEK_SET) == 0 && fread(in, 10, 2, f) == 2 && memcmp(in, out + 1000, 20) == 0);
    CHECK(fclose(f) == 0);

    /* "w" empties an existing file */
    f = fopen(name("bin.dat"), "w");
    CHECK(f != NULL && fputs("short", f) == 0 && fclose(f) == 0);
    f = fopen(name("bin.dat"), "r");
    CHECK(f != NULL && fseek(f, 0L, SEEK_END) == 0 && ftell(f) == 5 && fclose(f) == 0);

    /* writing past the end leaves zeros in the gap */
    f = fopen(name("bin.dat"), "r+");
    CHECK(f != NULL);
    if (f == NULL)
        return;
    CHECK(fseek(f, 1500L, SEEK_SET) == 0 && fputc('Z', f) == 'Z');
    rewind(f);
    CHECK(fread(in, 1, 1501, f) == 1501 && memcmp(in, "short", 5) == 0 && in[1500] == 'Z');
    for (i = 5; i < 1500 && in[i] == 0; i++)
        ;
    CHECK(i == 1500);
    CHECK(fclose(f) == 0);
}

static void names(void)
{
    FILE *f;

    errno = 0;
    CHECK(fopen(name("nosuch.txt"), "r") == NULL && errno == 203);
    /* the new name needs the directory too: CTOS takes it from the current path */
    strcpy(path2, dir);
    strcat(path2, "test2.txt");
    CHECK(rename(name("test.txt"), path2) == 0);
    CHECK(fopen(name("test.txt"), "r") == NULL && errno == 203);
    f = fopen(name("test2.txt"), "r");
    CHECK(f != NULL && fclose(f) == 0);
    CHECK(remove(name("test2.txt")) == 0 && remove(name("bin.dat")) == 0);
    CHECK(fopen(name("test2.txt"), "r") == NULL);
    CHECK(remove(name("test2.txt")) == -1 && errno == 203);
}

static void keep(void)
{
    FILE *f = fopen(name("keep.txt"), "w");
    int i;

    CHECK(f != NULL);
    if (f == NULL)
        return;
    for (i = 0; i < 3; i++)
        fputs("The quick brown fox.\n", f);
    CHECK(fclose(f) == 0);
}

int main(int argc, char **argv)
{
    int i;

    printf("argc %d:", argc);
    for (i = 0; i < argc; i++)
        printf(" [%s]", argv[i]);
    printf("\n");
    CHECK(argc >= 1 && argv[argc] == NULL);
    if (argc > 1)
        strncpy(dir, argv[1], sizeof dir - 1);

    append_and_update(text_file());
    binary_file();
    names();
    keep();
    printf("filetest: %d checks, %d failed\n", checks, failures);
    return failures;
}
