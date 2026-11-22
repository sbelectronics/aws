/* time.c - time_t is seconds since 1 January 1970.
 *
 * CTOS gives the date and time as a DWORD: the high word is half days since
 * 1 March 1952 (odd for the afternoon), the low word seconds into the half
 * day. 1 March 1952 is 6515 days before 1 January 1970. */
#include <ctos.h>
#include <stdio.h>
#include <time.h>
#include "libc.h"

#define DAY       86400L
#define CTOS_DAYS 6515L

static int leap(int year)
{
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

static const int mdays[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

time_t ctos_time(unsigned long dt)
{
    return (long)(dt >> 16) * (DAY / 2) + (long)(dt & 0xFFFF) - CTOS_DAYS * DAY;
}

time_t time(time_t *t)
{
    unsigned long dt;
    time_t v = -1;

    if (GetDateTime(&dt) == 0)
        v = ctos_time(dt);
    if (t)
        *t = v;
    return v;
}

/* seconds since the first call */
clock_t clock(void)
{
    static time_t start = -1;
    time_t now = time(NULL);

    if (start == -1)
        start = now;
    return now - start;
}

struct tm *gmtime(const time_t *t)
{
    static struct tm tm;
    long days = *t / DAY, rem = *t % DAY;
    int year = 1970, m, n;

    if (rem < 0) {
        rem += DAY;
        days--;
    }
    tm.tm_hour = (int)(rem / 3600);
    tm.tm_min = (int)(rem % 3600 / 60);
    tm.tm_sec = (int)(rem % 60);
    tm.tm_wday = (int)((days + 4) % 7);     /* 1 January 1970 was a Thursday */
    if (tm.tm_wday < 0)
        tm.tm_wday += 7;
    while (days < 0)
        days += 365 + leap(--year);
    while (days >= 365 + leap(year))
        days -= 365 + leap(year++);
    tm.tm_year = year - 1900;
    tm.tm_yday = (int)days;
    for (m = 0; m < 11; m++) {
        n = mdays[m] + (m == 1 && leap(year));
        if (days < n)
            break;
        days -= n;
    }
    tm.tm_mon = m;
    tm.tm_mday = (int)days + 1;
    tm.tm_isdst = 0;
    return &tm;
}

struct tm *localtime(const time_t *t)
{
    return gmtime(t);
}

/* Fields out of range are allowed (tm_mday 32 is the 1st of the next month);
 * *tm is rewritten with the normalised date. */
time_t mktime(struct tm *tm)
{
    int year = tm->tm_year + 1900 + tm->tm_mon / 12, mon = tm->tm_mon % 12, y, m;
    long days;
    time_t t;

    if (mon < 0) {
        mon += 12;
        year--;
    }
    days = 0;
    for (y = 1970; y < year; y++)
        days += 365 + leap(y);
    for (y = 1970; y > year; y--)
        days -= 365 + leap(y - 1);
    for (m = 0; m < mon; m++)
        days += mdays[m] + (m == 1 && leap(year));
    days += tm->tm_mday - 1;
    t = days * DAY + tm->tm_hour * 3600L + tm->tm_min * 60L + tm->tm_sec;
    *tm = *gmtime(&t);
    return t;
}

char *asctime(const struct tm *tm)
{
    static const char day[] = "SunMonTueWedThuFriSat";
    static const char mon[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    static char buf[26];

    sprintf(buf, "%.3s %.3s %2d %02d:%02d:%02d %d\n", day + 3 * tm->tm_wday,
            mon + 3 * tm->tm_mon, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec,
            tm->tm_year + 1900);
    return buf;
}

char *ctime(const time_t *t)
{
    return asctime(localtime(t));
}
