/* ctostest.c - checks the CTOS-specific parts of the library: far memory,
 * messages, error text, directories, file details and execv. The first
 * argument is a volume, such as [f0], where a directory <T> is made and
 * removed. Prints each failure and a count; then, if all passed, runs
 * filetest with execv, giving it [f0]<C> and "via execv" as arguments. */
#include <ctosmsg.h>
#include <dirent.h>
#include <errno.h>
#include <farmem.h>
#include <process.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

static int checks, failures;
static char vol[16] = "", path[64];

static void check(int ok, int line, const char *what)
{
    checks++;
    if (!ok) {
        failures++;
        printf("FAIL line %d: %s (errno %d)\n", line, what, errno);
    }
}

#define CHECK(e) check((e) != 0, __LINE__, #e)

static const char *in_vol(const char *name)
{
    strcpy(path, vol);
    strcat(path, name);
    return path;
}

static void far_memory(void)
{
    static char near_buf[100];
    unsigned long before = farcoreleft(), after;
    char __far *a, __far *b;
    unsigned i;
    int ok;

    CHECK(before > 80000L);
    a = farmalloc(40000u);
    b = farmalloc(30000u);
    CHECK(a != 0 && b != 0 && FP_SEG(a) != FP_SEG(b) && FP_OFF(a) == 0);
    if (a == 0 || b == 0)
        return;
    after = farcoreleft();
    CHECK(before - after >= 70000L);
    _fmemset(a, 'a', 40000u);
    _fmemset(b, 'b', 30000u);
    for (ok = 1, i = 0; i < 40000u; i += 999)
        ok &= a[i] == 'a';
    CHECK(ok && a[39999u] == 'a' && b[29999u] == 'b');
    for (i = 0; i < sizeof near_buf; i++)
        near_buf[i] = (char)i;
    _fmemcpy(a + 1000, near_buf, sizeof near_buf);
    CHECK(_fmemcmp(a + 1000, near_buf, sizeof near_buf) == 0);
    _fmemmove(a + 1001, a + 1000, 50);
    CHECK(a[1001] == 0 && a[1050] == 49 && a[1000] == 0);
    CHECK(_fmemcmp(a, b, 10) < 0);
    CHECK(farmalloc(0) == 0);

    farfree(a);                 /* b is still allocated after it */
    CHECK(farcoreleft() == after);
    farfree(b);                 /* now both go back */
    CHECK(farcoreleft() == before);
}

static void messages(void)
{
    static char first[] = "first", second[] = "second";
    void __far *msg;
    Exch x, resp;

    CHECK(QueryDefaultRespExch(&resp) == 0 && resp != 0);
    CHECK(AllocExch(&x) == 0 && x != resp);
    CHECK(Check(x, &msg) == ercNoMessage);
    CHECK(Send(x, first) == 0 && Send(x, second) == 0);
    CHECK(Check(x, &msg) == 0 && msg == (void __far *)first);
    CHECK(Wait(x, &msg) == 0 && msg == (void __far *)second);
    CHECK(Check(x, &msg) == ercNoMessage);
    CHECK(DeallocExch(x) == 0);
}

static void error_text(void)
{
    CHECK(strcmp(strerror(203), "no such file") == 0);
    CHECK(strcmp(strerror(9999), "CTOS error 9999") == 0);
}

static void directories(void)
{
    struct dirent *e;
    struct stat st;
    DIR *d;
    FILE *f;
    time_t now = time(NULL);
    int n, seen_a, seen_b;
    char dir[24];

    strcpy(dir, in_vol("<T>"));
    CHECK(mkdir(dir) == 0);
    CHECK(mkdir(dir) == -1 && errno == 240);
    f = fopen(in_vol("<T>a.txt"), "w");
    CHECK(f != NULL && fputs("hello", f) == 0 && fclose(f) == 0);
    f = fopen(in_vol("<T>b.txt"), "w");
    CHECK(f != NULL && fclose(f) == 0);

    d = opendir(dir);
    CHECK(d != NULL);
    if (d) {
        for (n = seen_a = seen_b = 0; (e = readdir(d)) != NULL; n++) {
            seen_a |= strcmp(e->d_name, "a.txt") == 0;
            seen_b |= strcmp(e->d_name, "b.txt") == 0;
        }
        CHECK(n == 2 && seen_a && seen_b);
        rewinddir(d);
        CHECK(readdir(d) != NULL);
        CHECK(closedir(d) == 0);
    }

    CHECK(stat(in_vol("<T>a.txt"), &st) == 0);
    CHECK(st.st_size == 5 && st.st_alloc == 512);
    CHECK(st.st_mtime >= st.st_ctime && st.st_mtime - now < 120 && now - st.st_mtime < 120);
    CHECK(stat(in_vol("<T>none.txt"), &st) == -1 && errno == 203);

    CHECK(rmdir(dir) == -1 && errno == 241);
    CHECK(remove(in_vol("<T>a.txt")) == 0 && remove(in_vol("<T>b.txt")) == 0);
    CHECK(rmdir(dir) == 0);
    CHECK(opendir(dir) == NULL && errno == 204);

    /* this program is on the volume too */
    d = opendir(in_vol("<C>"));
    CHECK(d != NULL);
    if (d) {
        for (seen_a = 0; (e = readdir(d)) != NULL;)
            seen_a |= strcmp(e->d_name, "ctostest.run") == 0;
        CHECK(seen_a && closedir(d) == 0);
    }
}

int main(int argc, char **argv)
{
    static char *args[4], dir[24], runfile[40];

    if (argc > 1)
        strncpy(vol, argv[1], sizeof vol - 1);
    far_memory();
    messages();
    error_text();
    directories();
    CHECK(execv(in_vol("<C>nosuch.run"), argv) == -1 && errno == 203);
    printf("ctostest: %d checks, %d failed\n", checks, failures);
    if (failures)
        return failures;

    /* in_vol's result is overwritten by its next call, so copy each */
    strcpy(dir, in_vol("<C>"));
    strcpy(runfile, in_vol("<C>filetest.run"));
    args[0] = "filetest";
    args[1] = dir;
    args[2] = "via execv";
    args[3] = NULL;
    execv(runfile, args);
    perror("execv");
    return 1;
}
