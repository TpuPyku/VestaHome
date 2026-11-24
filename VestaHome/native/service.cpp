// service.cpp - the car's scheduled service (ТО): see service.h.
#include <windows.h>
#include <stdio.h>
#include <wchar.h>
#include "service.h"

int YmdDays(int ymd)   // (the days-from-civil count: March the year's first month, 29 Feb its last day)
{
    int y = ymd / 10000, m = ymd / 100 % 100, d = ymd % 100;
    if (m <= 2)
        y--;
    int era = y / 400, yoe = y - era * 400, mp = (m + 9) % 12;
    int doy = (153 * mp + 2) / 5 + d - 1, doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 730425;   // (2000-03-01 is 730485 - 60 from 0000-03-01)
}

int ServiceDueYmd(int lastYmd)
{
    return lastYmd % 10000 == 229 ? (lastYmd / 10000 + 1) * 10000 + 301 : lastYmd + 10000;
}

ServiceLeft ServiceLeftAt(double odoKm, double lastKm, int lastYmd, int todayYmd, int everyKm)
{
    ServiceLeft l;
    double km = lastKm + everyKm - odoKm;
    l.km = km >= 0 ? (int)km : -(int)(-km + 0.999);
    l.days = lastYmd ? YmdDays(ServiceDueYmd(lastYmd)) - YmdDays(todayYmd) : kNoDays;
    return l;
}

static void Km(int km, wchar_t* out, int n)   // 13799 -> "13 799"
{
    if (km >= 1000)
        _snwprintf(out, n, L"%d %03d", km / 1000, km % 1000);
    else
        _snwprintf(out, n, L"%d", km);   // (16 always enough: never cut)
}

static int Level(const ServiceLeft& l)   // 0 fine, 1 soon, 2 overdue
{
    return l.km < 0 || l.days < 0 ? 2 : l.km < 1000 || l.days < 30 ? 1 : 0;
}

static void DueText(const ServiceLeft& l, int due, bool dated, wchar_t* out, int n)
{
    wchar_t k[16];
    Km(l.km < 0 ? -l.km : l.km, k, 16);
    if (l.km < 0)
        _snwprintf(out, n, L"ТО просрочено на %s км", k);
    else if (l.days < 0)
        _snwprintf(out, n, L"ТО просрочено с %02d.%02d.%d", due % 100, due / 100 % 100, due / 10000);
    else if (dated)
        _snwprintf(out, n, L"ТО через %s км или до %02d.%02d.%d", k, due % 100, due / 100 % 100, due / 10000);
    else
        _snwprintf(out, n, L"ТО через %s км", k);
    out[n - 1] = 0;
}

int ServiceText(double odoKm, double lastKm, int lastYmd, int todayYmd, int everyKm, wchar_t* out, int n)
{
    if (odoKm < 0) {
        _snwprintf(out, n, L"ТО: пробег — в настройках виджета");
        out[n - 1] = 0;
        return 0;
    }
    ServiceLeft l = ServiceLeftAt(odoKm, lastKm, lastYmd, todayYmd, everyKm);
    DueText(l, ServiceDueYmd(lastYmd), lastYmd != 0, out, n);
    return Level(l);
}

static bool NoSpaces(const wchar_t* s, wchar_t* t, int cap)   // s without its spaces into t; false: too long
{
    int n = 0;
    for (; *s; s++)
        if (*s != L' ') {
            if (n == cap - 1) {
                t[n] = 0;
                return false;
            }
            t[n++] = *s;
        }
    t[n] = 0;
    return true;
}

static int Digits(const wchar_t* s, double* v)   // the digits s starts with: their count, their number
{
    int n = 0;
    for (*v = 0; s[n] >= L'0' && s[n] <= L'9'; n++)
        *v = *v * 10 + (s[n] - L'0');
    return n;
}

bool ParseKm(const wchar_t* s, double* km)
{
    wchar_t t[24];
    if (!NoSpaces(s, t, 24))
        return false;
    double whole, frac = 0;
    int n = Digits(t, &whole);
    const wchar_t* e = t + n;
    if (n && (*e == L',' || *e == L'.')) {   // a tenth (the rest of the fraction dropped)
        double f;
        int m = Digits(e + 1, &f);
        frac = m ? (e[1] - L'0') / 10.0 : 0;
        e += 1 + m;
    }
    if (!n || *e || whole > 9999999)
        return false;
    *km = whole + frac;
    return true;
}

static const wchar_t* Spaces(const wchar_t* s)   // past the spaces
{
    while (*s == L' ')
        s++;
    return s;
}

static bool DmyChar(wchar_t c, int* f, int* len, int* k)   // one character of "d.m.y" taken; false: not one
{
    if (c == L'.' && *k < 2 && len[*k]) {
        (*k)++;
        return true;
    }
    if (c < L'0' || c > L'9' || len[*k] >= 4)
        return false;
    f[*k] = f[*k] * 10 + (c - L'0');
    len[*k]++;
    return true;
}

static bool DmyFields(const wchar_t* s, int* f, int* len)   // "d.m.y" (spaces round it): its numbers and lengths
{
    int k = 0;
    for (s = Spaces(s); *s && *s != L' '; s++)
        if (!DmyChar(*s, f, len, &k))
            return false;
    return !*Spaces(s) && k == 2;
}

static bool DayExists(int y, int m, int d)   // (31 Apr counts as 1 May: not before the next month's 1st)
{
    if (m < 1 || m > 12 || d < 1)
        return false;
    int next = m == 12 ? (y + 1) * 10000 + 101 : y * 10000 + (m + 1) * 100 + 1;
    return YmdDays(y * 10000 + m * 100 + d) < YmdDays(next);
}

int ParseDmy(const wchar_t* s)
{
    int f[3] = { 0, 0, 0 }, len[3] = { 0, 0, 0 };
    if (!DmyFields(s, f, len) || len[2] != 4 || !DayExists(f[2], f[1], f[0]))
        return 0;
    return f[2] * 10000 + f[1] * 100 + f[0];
}

bool ParseEvery(const wchar_t* s, int* km)
{
    double v;
    if (!ParseKm(s, &v))
        return false;
    int k = (int)(v + 0.5);
    if (k < kSvcEveryMin || k > kSvcEveryMax)
        return false;
    *km = k;
    return true;
}

int ServiceEveryOf(int every, bool old7000)
{
    if (every >= kSvcEveryMin && every <= kSvcEveryMax)
        return every;
    return old7000 ? 7000 : kSvcEveryVesta;
}
