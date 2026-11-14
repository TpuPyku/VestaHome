// test_simnmea.cpp - GpsSim's lines (simnmea.cpp) on the PC (test_simnmea.bat); exit code: the failed checks.
#include "simnmea.h"
#include "simdrive.h"
#include <stdio.h>
#include <stdlib.h>
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

static bool Line(const char* all, int k, const char* want)   // the k-th line (from 0) is want, CR LF after it
{
    const char* p = all;
    for (int i = 0; i < k && p; i++) {
        p = strstr(p, "\r\n");
        if (p)
            p += 2;
    }
    size_t n = strlen(want);
    bool ok = p && !strncmp(p, want, n) && p[n] == '\r' && p[n + 1] == '\n';
    if (!ok)
        printf("  line %d: got \"%.90s\", want \"%s\"\n", k, p ? p : "(none)", want);
    return ok;
}

static void TestLines()
{
    SYSTEMTIME t;
    memset(&t, 0, sizeof(t));
    t.wYear = 2026;
    t.wMonth = 11;
    t.wDay = 8;
    t.wHour = 9;
    t.wMinute = 5;
    t.wSecond = 7;
    // 55.791127 N, 37.530260 E, 54.0 km/h (29.16 kn), course 11.7, the wheel 12.5 left
    SimPoint p = { 55791127, 37530260, 540, 117, -125 };
    char s[512];
    int n = SimLines(p, t, s, sizeof(s));
    Check(n == (int)strlen(s), "lines: their length");
    Check(Line(s, 0, "$GPRMC,090507.00,A,5547.4676,N,03731.8156,E,29.16,11.7,081126,,,A*6E"),
          "rmc: time, the degrees and minutes, knots, course, date, checksum");
    Check(Line(s, 1, "$GPGGA,090507.00,5547.4676,N,03731.8156,E,1,09,0.9,150.0,M,0.0,M,,*5F"), "gga: a fix of 9");
    Check(Line(s, 2, "@,54,1.19*16"), "can: the speed, the module's version");
    Check(Line(s, 3, "@ANG,-12.5,*7D"), "can: the wheel, left negative");
    // south, west, standing, the wheel straight; a course just under north
    SimPoint q = { -33856000, -151215000, 0, 3599, 0 };
    n = SimLines(q, t, s, sizeof(s));
    Check(Line(s, 0, "$GPRMC,090507.00,A,3351.3600,S,15112.9000,W,0.00,359.9,081126,,,A*6E"),
          "rmc: south, west (no sign), standing");
    Check(Line(s, 3, "@ANG,0.0,*66"), "can: the wheel straight");
    Check(SimLines(p, t, s, 100) == 0 && s[0] == 0, "lines: no room - none, nothing half written");
}

static void TestAt()
{
    Check(SimAt(0, 10) == 0 && SimAt(999, 10) == 0 && SimAt(1000, 10) == 1, "at: a point a second");
    Check(SimAt(9999, 10) == 9 && SimAt(10000, 10) == 0 && SimAt(25000, 10) == 5, "at: round and round");
    Check(SimAt(12345, 0) == 0, "at: no drive - 0");
}

static SimPoint s_d[4000];

static void TestDrive()
{
    // 1 km north (0.009 degrees of latitude): stands, speeds up to 60, slows to a stop at the end
    int lat[2] = { 55000000, 55009000 }, lon[2] = { 37000000, 37000000 };
    int n = SimDrive(lat, lon, 2, s_d, 4000), top = 0;
    for (int i = 0; i < n; i++)
        top = s_d[i].kmh10 > top ? s_d[i].kmh10 : top;
    Check(n > kSimStand0 + 60 + kSimStand1 && n < kSimStand0 + 120 + kSimStand1, "drive: 1 km in about a minute and more");
    Check(s_d[0].kmh10 == 0 && s_d[kSimStand0 - 1].kmh10 == 0 && s_d[n - 1].kmh10 == 0, "drive: standing at both ends");
    Check(top >= 590 && top <= 600, "drive: up to 60 km/h");
    Check(s_d[n - 1].lat == 55009000 && s_d[0].lat == 55000000, "drive: from the first point to the last");
    Check(s_d[kSimStand0 + 5].course10 == 0, "drive: course north");
    bool smooth = true;
    for (int i = 1; i < n; i++)
        if (s_d[i].kmh10 - s_d[i - 1].kmh10 > 1.5 * 36 + 1)
            smooth = false;
    Check(smooth, "drive: speeds up 1.5 m/s2 at most");
    // 500 m north, then right (east) 500 m: slow at the corner, the wheel to the right there
    int la[3] = { 55000000, 55004500, 55004500 }, lo[3] = { 37000000, 37000000, 37007850 };
    n = SimDrive(la, lo, 3, s_d, 4000);
    int at = 0, best = 1 << 30;
    for (int i = 0; i < n; i++) {
        int d = abs(s_d[i].lat - 55004500) + abs(s_d[i].lon - 37000000);
        if (d < best) {
            best = d;
            at = i;
        }
    }
    int wmax = 0;
    for (int i = at - 2; i <= at + 2; i++)
        wmax = s_d[i].wheel10 > wmax ? s_d[i].wheel10 : wmax;
    Check(s_d[at].kmh10 < 250, "drive: slow at a right-angled corner");
    Check(wmax > 0, "drive: the wheel to the right at a right turn");
    Check(SimDrive(lat, lon, 2, s_d, 50) == 50, "drive: no more than cap");
    Check(SimDrive(lat, lon, 1, s_d, 4000) == 0, "drive: one point - none");
}

// No fix (the tunnels' test, 2026-11-14): RMC "V" with no position, GGA quality 0 - the CAN's speed still there
static void TestLost()
{
    SYSTEMTIME t;
    memset(&t, 0, sizeof(t));
    t.wYear = 2026;
    t.wMonth = 11;
    t.wDay = 8;
    t.wHour = 9;
    t.wMinute = 5;
    t.wSecond = 7;
    SimPoint p = { 55791127, 37530260, 540, 117, -125, 1 };
    char s[512];
    SimLines(p, t, s, sizeof(s));
    Check(Line(s, 0, "$GPRMC,090507.00,V,,,,,,,081126,,,N*7A"), "lost: rmc V, no position");
    Check(Line(s, 1, "$GPGGA,090507.00,,,,,0,00,,,M,,M,,*43"), "lost: gga quality 0");
    Check(Line(s, 2, "@,54,1.19*16"), "lost: the CAN's speed still");
    int box[8];
    int nb = SimBoxes("# Lefortovo\r\n55.75 37.68 55.78 37.72\r\nbad line\r\n55.1 37.1 55.2 37.2", box, 2);
    Check(nb == 2 && box[0] == 55750000 && box[1] == 37680000 && box[2] == 55780000 && box[3] == 37720000 &&
          box[4] == 55100000 && box[7] == 37200000, "boxes: two read, a comment and a bad line skipped");
    Check(SimBoxes("55.75 37.68 55.78 37.72\n55.1 37.1 55.2 37.2", box, 1) == 1, "boxes: at most cap");
    SimPoint d[3] = { { 55760000, 37700000, 0, 0, 0 }, { 55790000, 37700000, 0, 0, 0 },
                      { 55770000, 37600000, 0, 0, 0 } };
    int lost = SimLose(d, 3, box, 1);
    Check(lost == 1 && d[0].lost && !d[1].lost && !d[2].lost, "lose: only the point in the box");
}

int main()
{
    TestLines();
    TestAt();
    TestDrive();
    TestLost();
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
