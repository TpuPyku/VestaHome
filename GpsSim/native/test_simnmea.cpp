// test_simnmea.cpp - GpsSim's lines (simnmea.cpp) on the PC (test_simnmea.bat); exit code: the failed checks.
#include "simnmea.h"
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

int main()
{
    TestLines();
    TestAt();
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
