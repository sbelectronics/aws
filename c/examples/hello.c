#include <stdio.h>

int main(void)
{
    char name[40];
    long big = 123456789L;

    printf("Hello from C on CTOS!\n");
    printf("%d %u %x %ld %s|%5d|%-5d|%05d\n", -42, 65535u, 0xBEEF, big, "ok", 7, 7, 7);
    printf("What's your name? ");
    if (fgets(name, sizeof name, stdin))
        printf("Hello, %s", name);
    return 0;
}
