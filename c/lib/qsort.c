#include <stdlib.h>

static void swap(char *a, char *b, size_t n)
{
    char t;

    while (n--) {
        t = *a;
        *a++ = *b;
        *b++ = t;
    }
}

/* Quicksort with the middle element as the pivot, recursing into the smaller
 * part so the stack stays shallow, and insertion sort for short runs. */
void qsort(void *base, size_t n, size_t size, int (*cmp)(const void *, const void *))
{
    char *b = base;
    size_t i, j, last;

    while (n > 1) {
        if (n < 8) {
            for (i = 1; i < n; i++)
                for (j = i; j > 0 && cmp(b + (j - 1) * size, b + j * size) > 0; j--)
                    swap(b + (j - 1) * size, b + j * size, size);
            return;
        }
        swap(b, b + n / 2 * size, size);
        last = 0;
        for (i = 1; i < n; i++)
            if (cmp(b + i * size, b) < 0)
                swap(b + ++last * size, b + i * size, size);
        swap(b, b + last * size, size);
        if (last < n - last - 1) {
            qsort(b, last, size, cmp);
            b += (last + 1) * size;
            n -= last + 1;
        } else {
            qsort(b + (last + 1) * size, n - last - 1, size, cmp);
            n = last;
        }
    }
}

void *bsearch(const void *key, const void *base, size_t n, size_t size,
              int (*cmp)(const void *, const void *))
{
    const char *b = base, *p;
    int c;

    while (n > 0) {
        p = b + n / 2 * size;
        c = cmp(key, p);
        if (c == 0)
            return (void *)p;
        if (c > 0) {
            b = p + size;
            n -= n / 2 + 1;
        } else {
            n /= 2;
        }
    }
    return NULL;
}
