// simdrive.cpp - GpsSim's drive along a line (simdrive.h); tools/make_route.py does the same for the built-in one.
#include "simdrive.h"
#include <math.h>
#include <stdlib.h>

static const double kSide = 2.5, kUp = 1.5, kDown = 2.5, kStep = 5;   // m/s2; the speed's grid, m
static const double kArcM = 15;                                        // a bend's arc at most, m
static const double kBase = 2.6, kRatio = 16;                           // the car's wheelbase, m; the steering 16:1
static const double kPi = 3.14159265358979;

struct Pt { double x, y; };   // metres east, north of the first point

static double Course(const Pt& a, const Pt& b)   // degrees, 0 north, clockwise
{
    double c = atan2(b.x - a.x, b.y - a.y) * 180 / kPi;
    return c < 0 ? c + 360 : c;
}

static double Turn(double c0, double c1)   // -180..180, right positive
{
    double t = c1 - c0;
    while (t > 180)
        t -= 360;
    while (t <= -180)
        t += 360;
    return t;
}

static double Len(const Pt& a, const Pt& b)
{
    return sqrt((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y));
}

// Each piece's top speed, m/s: kSimTopKmh, and the bends' at its ends (sideways kSide)
static void Limits(const Pt* p, int n, double* v)
{
    for (int i = 0; i < n - 1; i++)
        v[i] = kSimTopKmh / 3.6;
    for (int i = 1; i < n - 1; i++) {
        double a = fabs(Turn(Course(p[i - 1], p[i]), Course(p[i], p[i + 1]))) * kPi / 180;
        // the bend's arc: its pieces' lengths, a corner of long ones taken in kArcM (a junction's turn is short)
        double d = (Len(p[i - 1], p[i]) + Len(p[i], p[i + 1])) / 2;
        d = d < kArcM ? d : kArcM;
        if (a > 0.02) {
            double vb = sqrt(kSide * (d > 1 ? d : 1) / a);
            v[i - 1] = vb < v[i - 1] ? vb : v[i - 1];
            v[i] = vb < v[i] ? vb : v[i];
        }
    }
    for (int i = 0; i < n - 1; i++)
        v[i] = v[i] > 3 ? v[i] : 3;
}

static SimPoint At(const Pt* p, const double* cum, int n, int lat0, int lon0, double kx, double s, int* j)
{
    while (*j < n - 2 && cum[*j + 1] < s)
        (*j)++;
    double seg = cum[*j + 1] - cum[*j], f = seg > 0 ? (s - cum[*j]) / seg : 0;
    f = f < 0 ? 0 : f > 1 ? 1 : f;
    const Pt& a = p[*j];
    const Pt& b = p[*j + 1];
    SimPoint o;
    o.lat = lat0 + (int)floor((a.y + (b.y - a.y) * f) / 111320 * 1e6 + 0.5);
    o.lon = lon0 + (int)floor((a.x + (b.x - a.x) * f) / kx * 1e6 + 0.5);
    o.course10 = (int)(Course(a, b) * 10 + 0.5) % 3600;
    o.kmh10 = o.wheel10 = o.lost = 0;
    return o;
}

static int Put(SimPoint* out, int at, int cap, const SimPoint& p)
{
    if (at < cap)
        out[at] = p;
    return at + 1;
}

int SimDrive(const int* lat, const int* lon, int n, SimPoint* out, int cap)
{
    if (n < 2 || cap <= 0)
        return 0;
    double kx = 111320 * cos(lat[0] / 1e6 * kPi / 180);
    Pt* p = (Pt*)malloc(n * sizeof(Pt));
    double* cum = (double*)malloc(n * sizeof(double));
    double* vmax = (double*)malloc(n * sizeof(double));
    int grid = 0;
    double* lim = NULL;
    int at = 0;
    if (p && cum && vmax) {
        for (int i = 0; i < n; i++) {
            p[i].x = (lon[i] - lon[0]) / 1e6 * kx;
            p[i].y = (lat[i] - lat[0]) / 1e6 * 111320;
            cum[i] = i ? cum[i - 1] + Len(p[i - 1], p[i]) : 0;
        }
        Limits(p, n, vmax);
        grid = (int)(cum[n - 1] / kStep) + 2;
        lim = (double*)malloc(grid * sizeof(double));
    }
    if (lim) {
        for (int g = 0, j = 0; g < grid; g++) {   // the limit along the way, braking in time for each
            while (j < n - 2 && cum[j + 1] < g * kStep)
                j++;
            lim[g] = vmax[j];
        }
        lim[grid - 1] = 0;
        for (int g = grid - 2; g >= 0; g--) {
            double b = sqrt(lim[g + 1] * lim[g + 1] + 2 * kDown * kStep);
            lim[g] = b < lim[g] ? b : lim[g];
        }
        int j = 0;
        SimPoint first = At(p, cum, n, lat[0], lon[0], kx, 0, &j);
        for (int k = 0; k < kSimStand0; k++)
            at = Put(out, at, cap, first);
        double s = 0, v = 0, c0 = first.course10 / 10.0;
        j = 0;
        while (s < cum[n - 1] && at < cap) {
            SimPoint o = At(p, cum, n, lat[0], lon[0], kx, s, &j);
            double c1 = o.course10 / 10.0, w = Turn(c0, c1) * kPi / 180;   // the course's change this second
            o.kmh10 = (int)(v * 36 + 0.5);
            o.wheel10 = v < 1 ? 0 : (int)(kRatio * atan(kBase * w / v) * 1800 / kPi);
            o.wheel10 = o.wheel10 > 5400 ? 5400 : o.wheel10 < -5400 ? -5400 : o.wheel10;
            at = Put(out, at, cap, o);
            c0 = c1;
            double top = lim[(int)(s / kStep) < grid ? (int)(s / kStep) : grid - 1];
            v = v < top ? (v + kUp < top ? v + kUp : top) : (v - kDown > top ? v - kDown : top);
            s += v > 0.5 ? v : 0.5;
        }
        SimPoint last;
        last.lat = lat[n - 1];
        last.lon = lon[n - 1];
        last.course10 = (int)(Course(p[n - 2], p[n - 1]) * 10 + 0.5) % 3600;
        last.kmh10 = last.wheel10 = last.lost = 0;
        for (int k = 0; k < kSimStand1; k++)
            at = Put(out, at, cap, last);
    }
    free(p);
    free(cum);
    free(vmax);
    free(lim);
    return at < cap ? at : cap;
}
