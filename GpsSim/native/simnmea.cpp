// simnmea.cpp - GpsSim's lines for a point of the drive (simnmea.h).
#include "simnmea.h"
#include <stdio.h>
#include <string.h>

// "ddmm.mmmm" (w: the degrees' digits) and the hemisphere's letter, from millionths of a degree (whole numbers:
// the minutes' ten-thousandths are the millionths x 0.6)
static void Ddmm(int v, int w, char pos, char neg, char* out, char* hemi)
{
    *hemi = v < 0 ? neg : pos;
    unsigned a = v < 0 ? (unsigned)-v : (unsigned)v;
    unsigned m = ((a % 1000000) * 6 + 5) / 10;   // the minutes, ten-thousandths
    sprintf(out, "%0*u%02u.%04u", w, a / 1000000, m / 10000, m % 10000);
}

static int Put(char* out, int at, int cap, const char* body)   // "<body>*hh\r\n" at out + at; its length, 0: no room
{
    unsigned char sum = 0;
    for (const char* p = body + 1; *p; p++)
        sum ^= (unsigned char)*p;
    int n = (int)strlen(body) + 5;
    if (at + n >= cap)
        return 0;
    sprintf(out + at, "%s*%02X\r\n", body, sum);
    return n;
}

int SimLines(const SimPoint& p, const SYSTEMTIME& utc, char* out, int cap)
{
    char lat[16], lon[16], ns, ew, tm[16], b[4][120];
    Ddmm(p.lat, 2, 'N', 'S', lat, &ns);
    Ddmm(p.lon, 3, 'E', 'W', lon, &ew);
    sprintf(tm, "%02d%02d%02d.00", utc.wHour, utc.wMinute, utc.wSecond);
    int w = p.wheel10 < 0 ? -p.wheel10 : p.wheel10;
    sprintf(b[0], "$GPRMC,%s,A,%s,%c,%s,%c,%.2f,%d.%d,%02d%02d%02d,,,A", tm, lat, ns, lon, ew, p.kmh10 / 18.52,
            p.course10 / 10, p.course10 % 10, utc.wDay, utc.wMonth, utc.wYear % 100);
    sprintf(b[1], "$GPGGA,%s,%s,%c,%s,%c,1,09,0.9,150.0,M,0.0,M,,", tm, lat, ns, lon, ew);
    if (p.lost) {   // (a tunnel: what a receiver says with no fix - no position)
        sprintf(b[0], "$GPRMC,%s,V,,,,,,,%02d%02d%02d,,,N", tm, utc.wDay, utc.wMonth, utc.wYear % 100);
        sprintf(b[1], "$GPGGA,%s,,,,,0,00,,,M,,M,,", tm);
    }
    sprintf(b[2], "@,%d,1.19", (p.kmh10 + 5) / 10);
    sprintf(b[3], "@ANG,%s%d.%d,", p.wheel10 < 0 ? "-" : "", w / 10, w % 10);
    int at = 0;
    for (int i = 0; i < 4; i++) {
        int n = Put(out, at, cap, b[i]);
        if (!n) {
            if (cap > 0)
                out[0] = 0;   // nothing half written
            return 0;
        }
        at += n;
    }
    return at;
}

int SimAt(DWORD ms, int n)
{
    return n > 0 ? (int)(ms / 1000 % (DWORD)n) : 0;
}

int SimBoxes(const char* text, int* box, int cap)
{
    int n = 0;
    while (*text && n < cap) {
        double v[4];
        if (*text != '#' && sscanf(text, "%lf %lf %lf %lf", &v[0], &v[1], &v[2], &v[3]) == 4) {
            for (int i = 0; i < 4; i++)
                box[n * 4 + i] = (int)(v[i] * 1e6 + (v[i] < 0 ? -0.5 : 0.5));
            n++;
        }
        const char* e = strchr(text, '\n');
        if (!e)
            break;
        text = e + 1;
    }
    return n;
}

static bool InBox(const SimPoint& p, const int* b)
{
    return p.lat >= b[0] && p.lat <= b[2] && p.lon >= b[1] && p.lon <= b[3];
}

int SimLose(SimPoint* p, int n, const int* box, int nbox)
{
    int lost = 0;
    for (int i = 0; i < n; i++) {
        p[i].lost = 0;
        for (int b = 0; b < nbox && !p[i].lost; b++)
            p[i].lost = InBox(p[i], box + b * 4);
        lost += p[i].lost;
    }
    return lost;
}
