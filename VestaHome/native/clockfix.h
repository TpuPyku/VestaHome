// clockfix.h: the clock on a unit without GPS time (the user 2026-11-20: a car first with the screenless unit - no
// GPS lines in the CAN module's stream, the MCU's clock 02.01.2005 after each engine off). Pure, header only: VestaStub
// (the MCU's 0x3901) and VestaHome (the net's date) include it.
#ifndef CLOCKFIX_H
#define CLOCKFIX_H

#include <windows.h>
#include <stdio.h>
#include <string.h>

enum { MCLK_NONE, MCLK_TIME, MCLK_FULL };

// The MCU's clock message 0x3901, its data d (7 bytes: 00, hour, minute, second, year - 2000, month, day); sysYear -
// the system clock's year now. MCLK_FULL: all of t; MCLK_TIME: the date is not a real one (year < 2020) and the system's
// is not either - only t's hour, minute, second (the date stays); MCLK_NONE: nothing to take.
static bool ClockDayOk(int h, int m, int s)
{
    return (unsigned)h < 24 && (unsigned)m < 60 && (unsigned)s < 60;
}

static int McuClock(const unsigned char* d, int sysYear, SYSTEMTIME* t)
{
    if (!ClockDayOk(d[1], d[2], d[3]))
        return MCLK_NONE;
    bool date = d[4] >= 20 && d[5] >= 1 && d[5] <= 12 && d[6] >= 1 && d[6] <= 31;
    if (!date && sysYear >= 2020)
        return MCLK_NONE;
    t->wHour = d[1];
    t->wMinute = d[2];
    t->wSecond = d[3];
    t->wMilliseconds = 0;
    if (!date)
        return MCLK_TIME;
    t->wYear = (WORD)(2000 + d[4]);
    t->wMonth = d[5];
    t->wDay = d[6];
    return MCLK_FULL;
}

static int HttpMonth(const char* mon)   // "Jan" -> 1 .. "Dec" -> 12; 0 - not one
{
    static const char kMon[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    const char* at = strstr(kMon, mon);
    return at && (at - kMon) % 3 == 0 ? (int)(at - kMon) / 3 + 1 : 0;
}

// An HTTP Date header's value (RFC 1123: "Sun, 06 Nov 1994 08:49:37 GMT") -> t (UTC); false - not that.
static bool HttpDate(const char* s, SYSTEMTIME* t)
{
    char wd[4], mon[4], zone[4];
    int day, year, h, m, sec;
    if (sscanf(s, " %3[A-Za-z], %d %3s %d %d:%d:%d %3s", wd, &day, mon, &year, &h, &m, &sec, zone) != 8 ||
        strcmp(zone, "GMT"))
        return false;
    int month = HttpMonth(mon);
    if (!month || day < 1 || day > 31 || !ClockDayOk(h, m, sec > 59 ? sec - 1 : sec))
        return false;
    memset(t, 0, sizeof(*t));
    t->wYear = (WORD)year;
    t->wMonth = (WORD)month;
    t->wDay = (WORD)day;
    t->wHour = (WORD)h;
    t->wMinute = (WORD)m;
    t->wSecond = (WORD)(sec > 59 ? 59 : sec);   // (a leap second, 60)
    return true;
}

#endif
