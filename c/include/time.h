/* time.h - the date and time from CTOS. CTOS has no time zones, so localtime
 * is the same as gmtime. clock counts whole seconds. There is no difftime,
 * since there is no floating point: subtract the time_t values. */
#ifndef TIME_H
#define TIME_H

#include <stddef.h>

typedef long time_t;
typedef long clock_t;

#define CLOCKS_PER_SEC 1

struct tm {
    int tm_sec, tm_min, tm_hour;
    int tm_mday, tm_mon, tm_year;   /* tm_mon 0-11, tm_year since 1900 */
    int tm_wday, tm_yday, tm_isdst; /* tm_wday 0-6 from Sunday, tm_yday 0-365 */
};

time_t time(time_t *t);
clock_t clock(void);
time_t mktime(struct tm *tm);
struct tm *gmtime(const time_t *t);
struct tm *localtime(const time_t *t);
char *asctime(const struct tm *tm);
char *ctime(const time_t *t);

/* A CTOS date and time, as in a file header or GetDateTime, as a time_t */
time_t ctos_time(unsigned long dt);

#endif
