#ifndef MSN_CLOCK_H
#define MSN_CLOCK_H
/* The iPod RTC holds local wall time. Do not use libc mktime here: simulator
 * hosts apply a timezone while native Rockbox does not. The companion uses
 * the same civil-date epoch, so delivery stays at the authored local hour. */
static inline long msn_local_timestamp(const struct tm *tm)
{
    static const int before_month[] =
        { 0,31,59,90,120,151,181,212,243,273,304,334 };
    int year = tm->tm_year + 1900;
    if (year < 2020 || year > 2037 || tm->tm_mon < 0 || tm->tm_mon > 11 ||
        tm->tm_mday < 1 || tm->tm_mday > 31) return 0;
    long days = 0;
    for (int y = 1970; y < year; y++)
        days += 365 + (y%4 == 0 && (y%100 != 0 || y%400 == 0));
    days += before_month[tm->tm_mon] + tm->tm_mday-1;
    if (tm->tm_mon > 1 && year%4 == 0 && (year%100 != 0 || year%400 == 0)) days++;
    return days*86400 + tm->tm_hour*3600 + tm->tm_min*60 + tm->tm_sec;
}
#endif
