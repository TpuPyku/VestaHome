// clockfix.h: the MCU's clock taken whole or by its time of day only; the date from an HTTP answer's Date header.
#include "clockfix.h"
#include <stdio.h>
#include <string.h>

static int s_checks, s_failed;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAIL: %s\n", what);
    }
}

static bool Is(const SYSTEMTIME& t, int y, int mo, int d, int h, int mi, int s)
{
    return t.wYear == y && t.wMonth == mo && t.wDay == d && t.wHour == h && t.wMinute == mi && t.wSecond == s;
}

static void TestMcu()
{
    SYSTEMTIME t;
    const unsigned char good[7] = { 0, 18, 40, 38, 26, 10, 24 };
    memset(&t, 0, sizeof(t));
    Check(McuClock(good, 2005, &t) == MCLK_FULL && Is(t, 2026, 10, 24, 18, 40, 38), "a real date - all of it");
    // HU 2026-11-20 (VestaLogs 3): 02.01.2005 11:43:37 - the time of day taken, the date not
    const unsigned char lost[7] = { 0, 0x0B, 0x2B, 0x25, 5, 1, 2 };
    memset(&t, 0, sizeof(t));
    Check(McuClock(lost, 2005, &t) == MCLK_TIME && t.wHour == 11 && t.wMinute == 43 && t.wSecond == 37,
          "the date lost, the system's not set either - the time of day");
    Check(McuClock(lost, 2026, &t) == MCLK_NONE, "the system's date is a real one (GPS, net) - the MCU's not taken");
    Check(McuClock(good, 2026, &t) == MCLK_FULL, "a real date is taken even then");
    const unsigned char badTime[7] = { 0, 24, 0, 0, 26, 10, 24 };
    Check(McuClock(badTime, 2005, &t) == MCLK_NONE, "hour 24 - nothing");
    const unsigned char badMin[7] = { 0, 1, 60, 0, 5, 1, 2 };
    Check(McuClock(badMin, 2005, &t) == MCLK_NONE, "minute 60 - nothing");
    const unsigned char badSec[7] = { 0, 1, 2, 60, 5, 1, 2 };
    Check(McuClock(badSec, 2005, &t) == MCLK_NONE, "second 60 - nothing");
    const unsigned char badMon[7] = { 0, 1, 2, 3, 26, 13, 2 };
    Check(McuClock(badMon, 2005, &t) == MCLK_TIME, "month 13 - the time of day only");
    const unsigned char badDay[7] = { 0, 1, 2, 3, 26, 1, 0 };
    Check(McuClock(badDay, 2005, &t) == MCLK_TIME, "day 0 - the time of day only");
    const unsigned char day32[7] = { 0, 1, 2, 3, 26, 1, 32 };
    Check(McuClock(day32, 2005, &t) == MCLK_TIME, "day 32 - the time of day only");
    const unsigned char y19[7] = { 0, 1, 2, 3, 19, 1, 2 };
    Check(McuClock(y19, 2019, &t) == MCLK_TIME, "year 2019 - not a real one");
    const unsigned char y20[7] = { 0, 23, 59, 59, 20, 12, 31 };
    Check(McuClock(y20, 2005, &t) == MCLK_FULL && Is(t, 2020, 12, 31, 23, 59, 59), "2020 and the edges - all");
    const unsigned char m1[7] = { 0, 0, 0, 0, 20, 1, 1 };
    Check(McuClock(m1, 2005, &t) == MCLK_FULL && Is(t, 2020, 1, 1, 0, 0, 0), "01.01.2020 00:00:00 - all");
    Check(McuClock(lost, 2020, &t) == MCLK_NONE, "the system at 2020 is a real one");
    Check(McuClock(lost, 2019, &t) == MCLK_TIME, "the system at 2019 is not");
    const unsigned char mon0[7] = { 0, 1, 2, 3, 26, 0, 2 };
    Check(McuClock(mon0, 2005, &t) == MCLK_TIME, "month 0 - the time of day only");
    t.wMilliseconds = 500;
    McuClock(lost, 2005, &t);
    Check(t.wMilliseconds == 0, "the time of day: milliseconds 0");
}

static void TestHttp()
{
    SYSTEMTIME t;
    memset(&t, 0, sizeof(t));
    Check(HttpDate("Fri, 20 Nov 2026 10:54:07 GMT", &t) && Is(t, 2026, 11, 20, 10, 54, 7), "RFC 1123");
    Check(HttpDate("  Sun, 06 Nov 1994 08:49:37 GMT", &t) && Is(t, 1994, 11, 6, 8, 49, 37), "spaces before it");
    static const char* mon[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    for (int i = 0; i < 12; i++) {
        char s[40];
        _snprintf(s, sizeof(s), "Mon, 01 %s 2027 00:00:00 GMT", mon[i]);
        Check(HttpDate(s, &t) && t.wMonth == i + 1, mon[i]);
    }
    Check(!HttpDate("", &t), "empty - no");
    Check(!HttpDate("Fri, 20 Foo 2026 10:54:07 GMT", &t), "an unknown month - no");
    Check(!HttpDate("Fri, 20 Nov 2026 10:54 GMT", &t), "no seconds - no");
    Check(!HttpDate("Fri, 32 Nov 2026 10:54:07 GMT", &t), "day 32 - no");
    Check(!HttpDate("Fri, 00 Nov 2026 10:54:07 GMT", &t), "day 0 - no");
    Check(!HttpDate("Fri, 20 Nov 2026 24:00:00 GMT", &t), "hour 24 - no");
    Check(!HttpDate("Fri, 20 Nov 2026 10:60:00 GMT", &t), "minute 60 - no");
    Check(!HttpDate("Fri, 20 Nov 2026 10:00:61 GMT", &t), "second 61 - no");
    Check(!HttpDate("Fri, 20 Nov 2026 10:00:00 MSK", &t), "not GMT - no");
    Check(HttpDate("Fri, 20 Nov 2026 23:59:60 GMT", &t) && t.wSecond == 59, "a leap second - 59");
    Check(!HttpDate("Fri, 20 Nov 2026 -1:00:00 GMT", &t), "hour -1 - no");
    Check(HttpDate("Sat, 31 Oct 2026 10:00:00 GMT", &t) && t.wDay == 31, "day 31");
    Check(!HttpDate("Fri, 20 anF 2026 10:00:00 GMT", &t), "a month's name across two - no");
    t.wMilliseconds = 7;
    t.wDayOfWeek = 9;
    Check(HttpDate("Fri, 20 Nov 2026 10:00:00 GMT", &t) && t.wMilliseconds == 0 && t.wDayOfWeek == 0, "the rest 0");
}

int main()
{
    TestMcu();
    TestHttp();
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
