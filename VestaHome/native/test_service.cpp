// test_service.cpp - the car's service (ТО) due by km or a year (service.cpp) on the PC (test_service.bat).
#include <windows.h>
#include <stdio.h>
#include <wchar.h>
#include "service.h"

static int s_failed, s_checks;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAIL: %s\n", what);
    }
}

static bool Text(double odo, double last, int lastYmd, int today, int every, const wchar_t* want, int level)
{
    wchar_t s[96];
    int l = ServiceText(odo, last, lastYmd, today, every, s, 96);
    if (wcscmp(s, want) || l != level) {
        char a[200];
        WideCharToMultiByte(CP_UTF8, 0, s, -1, a, 200, NULL, NULL);
        printf("  got \"%s\" level %d\n", a, l);
        return false;
    }
    return true;
}

int main()
{
    Check(YmdDays(20000101) == 0, "2000-01-01: day 0");
    Check(YmdDays(20000301) - YmdDays(20000228) == 2, "2000 is leap: 29 Feb between");
    Check(YmdDays(21000301) - YmdDays(21000228) == 1, "2100 is not leap");
    Check(YmdDays(20271114) - YmdDays(20261114) == 365, "a year without 29 Feb: 365 days");
    Check(YmdDays(20280301) - YmdDays(20270301) == 366, "a year over 29 Feb 2028: 366 days");
    Check(YmdDays(20270101) - YmdDays(20261231) == 1, "over the new year: 1 day");
    Check(ServiceDueYmd(20261114) == 20271114, "a year later: the same date");
    Check(ServiceDueYmd(20280229) == 20290301, "29 Feb: 1 Mar of the next year");
    ServiceLeft l = ServiceLeftAt(31200.4, 30000, 20261114, 20261214, 15000);
    Check(l.km == 13799, "15000 from 30000, now 31200.4: 13799 km left (whole km down)");
    Check(l.days == 335, "a year from 14.11.2026, now 14.12.2026: 335 days left");
    l = ServiceLeftAt(37300, 30000, 20261114, 20271120, 7000);
    Check(l.km == -300, "7000 km interval, 300 over");
    Check(l.days == -6, "6 days past the date");
    l = ServiceLeftAt(30500, 30000, 0, 20261114, 15000);
    Check(l.days == kNoDays, "no date known: by km alone");
    Check(Text(31200.4, 30000, 20261114, 20261214, 15000, L"ТО через 13 799 км или до 14.11.2027", 0),
          "fine: km and the date");
    Check(Text(30500, 30000, 0, 20261114, 15000, L"ТО через 14 500 км", 0), "no date: km alone");
    Check(Text(36500, 30000, 20261114, 20261214, 7000, L"ТО через 500 км или до 14.11.2027", 1),
          "under 1000 km: soon");
    Check(Text(31000, 30000, 20261114, 20271101, 15000, L"ТО через 14 000 км или до 14.11.2027", 1),
          "under 30 days: soon");
    Check(Text(31000, 30000, 20261114, 20271015, 15000, L"ТО через 14 000 км или до 14.11.2027", 0),
          "30 days and more: fine");
    Check(Text(37300, 30000, 20261114, 20270101, 7000, L"ТО просрочено на 300 км", 2), "over the km: overdue");
    Check(Text(31000, 30000, 20261114, 20271120, 15000, L"ТО просрочено с 14.11.2027", 2),
          "past the date: overdue since it");
    Check(Text(37300, 30000, 20261114, 20271120, 7000, L"ТО просрочено на 300 км", 2), "both: the km told");
    Check(Text(45000, 30000, 0, 20261114, 15000, L"ТО через 0 км", 1), "exactly at the km: soon, not overdue");
    Check(Text(-1, 0, 0, 20261114, 15000, L"ТО: пробег — в настройках виджета", 0), "no odometer typed: the hint");
    Check(Text(1234567, 1220000, 0, 20261114, 15000, L"ТО через 433 км", 1), "a big odometer");
    double km = -5;
    // the interval typed (the user 2026-11-24: one's own number, not only 7000 / 15000)
    int every = 5;
    Check(ParseEvery(L"10000", &every) && every == 10000, "every: plain");
    Check(ParseEvery(L" 7 500 ", &every) && every == 7500, "every: with spaces");
    Check(ParseEvery(L"1000", &every) && every == 1000 && ParseEvery(L"100000", &every) && every == 100000,
          "every: 1000 and 100 000 - the ends");
    every = 5;
    Check(!ParseEvery(L"999", &every) && !ParseEvery(L"100001", &every) && every == 5, "every: out of 1000..100 000 - kept");
    Check(!ParseEvery(L"", &every) && !ParseEvery(L"7a", &every) && every == 5, "every: not a number - kept");
    Check(ParseEvery(L"7999,6", &every) && every == 8000, "every: a tenth - rounded");
    every = 5;
    Check(!ParseEvery(L"999,4", &every) && ParseEvery(L"999,5", &every) && every == 1000, "every: rounded, then its range");
    Check(ParseEvery(L"100000,4", &every) && every == 100000 && !ParseEvery(L"100000,5", &every), "every: the top, rounded");
    Check(ServiceEveryOf(1000, 0) == 1000 && ServiceEveryOf(100000, 0) == 100000 && ServiceEveryOf(999, 0) == 15000 &&
          ServiceEveryOf(100001, 1) == 7000, "every: the settings' value at the ends");
    Check(ServiceEveryOf(0, 0) == 15000 && ServiceEveryOf(0, 1) == 7000 && ServiceEveryOf(12000, 1) == 12000 &&
          ServiceEveryOf(500, 0) == 15000, "every: from the settings - its own, else the old 7000 switch, else 15 000");
    Check(ParseKm(L"31200", &km) && km == 31200, "km: plain");
    Check(ParseKm(L" 31 200 ", &km) && km == 31200, "km: with spaces");
    Check(ParseKm(L"31200,5", &km) && km == 31200.5, "km: a comma's tenth");
    Check(ParseKm(L"0", &km) && km == 0, "km: 0 (a new car)");
    Check(ParseKm(L"31200,", &km) && km == 31200, "km: a comma with nothing after it");
    Check(!ParseKm(L"1234567890123456789012345", &km), "km: too long - not one");
    km = 7;
    Check(!ParseKm(L"", &km) && km == 7, "km: empty - not one, kept");
    Check(!ParseKm(L"12a", &km), "km: a letter - not one");
    Check(!ParseKm(L"-5", &km), "km: negative - not one");
    Check(!ParseKm(L"10000000", &km), "km: 10 million - not one");
    Check(ParseDmy(L"14.11.2026") == 20261114, "date: dd.mm.yyyy");
    Check(ParseDmy(L" 1.2.2027 ") == 20270201, "date: one digit day and month");
    Check(ParseDmy(L"29.02.2028") == 20280229, "date: 29 Feb of a leap year");
    Check(ParseDmy(L"29.02.2027") == 0, "date: 29 Feb of a common year - not one");
    Check(ParseDmy(L"31.04.2027") == 0, "date: 31 Apr - not one");
    Check(ParseDmy(L"14.13.2026") == 0, "date: month 13 - not one");
    Check(ParseDmy(L"14.11.26") == 0, "date: two-digit year - not one");
    Check(ParseDmy(L"14.11.2026x") == 0, "date: trailing junk - not one");
    Check(ParseDmy(L"") == 0, "date: empty - not one");
    Check(ParseKm(L"9 999 999", &km) && km == 9999999, "km: the most, with nines");
    Check(ParseKm(L"00000000000000000000001", &km) && km == 1, "km: 23 characters - taken");
    Check(!ParseKm(L"000000000000000000000001", &km), "km: 24 characters - too long");
    Check(ParseDmy(L"1.1.2027") == 20270101, "date: January");
    Check(ParseDmy(L"31.12.2026") == 20261231, "date: 31 December");
    Check(ParseDmy(L"32.12.2026") == 0, "date: 32 December - not one");
    Check(ParseDmy(L"30.04.2027") == 20270430, "date: 30 April");
    Check(ParseDmy(L"0.11.2026") == 0, "date: day 0 - not one");
    Check(ParseDmy(L"0014.11.2026") == 20261114, "date: a day of four digits with zeros");
    Check(ParseDmy(L"00014.11.2026") == 0, "date: five digits - not one");
    Check(ParseDmy(L"14.1x.2026") == 0, "date: a letter in the month - not one");
    Check(ParseDmy(L"1a.11.2026") == 0, "date: a letter in the day - not one");
    Check(ParseDmy(L"14.11.2026 5") == 0, "date: more after a space - not one");
    Check(ParseDmy(L"14.11.2026.1") == 0, "date: a fourth part - not one");
    Check(ParseDmy(L"1:.11.2026") == 0, "date: ':' (after '9') - not a digit");
    Check(Text(31000, 30000, 20261114, 20261214, 2000, L"ТО через 1 000 км или до 14.11.2027", 0),
          "exactly 1000 km left: fine, the thousands spaced");
    Check(Text(31000, 30000, 20261114, 20271015, 15000, L"ТО через 14 000 км или до 14.11.2027", 0),
          "exactly 30 days left: fine");
    Check(Text(31000, 30000, 20261114, 20271114, 15000, L"ТО через 14 000 км или до 14.11.2027", 1),
          "the date today: soon, not overdue");
    Check(Text(0, 0, 0, 20261114, 15000, L"ТО через 15 000 км", 0), "a new car, 0 km: counted, not the hint");
    {
        wchar_t s[10];
        ServiceText(31000, 30000, 0, 20261114, 15000, s, 10);
        Check(wcslen(s) == 9, "a small buffer: cut, ended");
        ServiceText(-1, 0, 0, 20261114, 15000, s, 10);
        Check(wcslen(s) == 9, "the hint in a small buffer: cut, ended");
    }
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
