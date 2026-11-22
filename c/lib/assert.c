#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

void _assert_fail(const char *expr, const char *file, int line)
{
    fprintf(stderr, "Assertion failed: %s, %s line %d\n", expr, file, line);
    abort();
}
