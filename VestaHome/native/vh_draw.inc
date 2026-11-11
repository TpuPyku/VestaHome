// vh_draw.inc - part of home.cpp (included there, in order): drawing: brushes, text, icons, skin pictures, the top bar, controls of the mockups.

// VestaHome's version (VestaCore vversion.h): <big releases>.<significant changes>.<the build's date YYMMDD> - the
// date comes by itself; the release ("0.5") raised by hand.
#include "vversion.h"

static const wchar_t* VhVersion()   // "0.5.261023"
{
    return AppVersion(L"0.6");
}

// ---------------------------------------------------------------- drawing helpers

// Brushes and pens by colour (and width), made once: a frame draws dozens of plates, dots and lines, and each
// made and deleted its own GDI objects (2026-10-21). The palette is small (the theme's colours, day and night);
// a full cache is emptied whole. The objects are always selected out again before a helper returns.
enum { kGdiCache = 64 };
static struct { COLORREF c; int w; HGDIOBJ o; } s_brushes[kGdiCache], s_pens[kGdiCache];
static int s_brushN, s_penN;

static HBRUSH SolidBrush(COLORREF c)   // c: the final colour (after C())
{
    for (int i = 0; i < s_brushN; i++)
        if (s_brushes[i].c == c)
            return (HBRUSH)s_brushes[i].o;
    if (s_brushN == kGdiCache) {
        for (int i = 0; i < s_brushN; i++)
            DeleteObject(s_brushes[i].o);
        s_brushN = 0;
    }
    s_brushes[s_brushN].c = c;
    s_brushes[s_brushN].w = 0;
    s_brushes[s_brushN].o = CreateSolidBrush(c);
    return (HBRUSH)s_brushes[s_brushN++].o;
}

static HPEN SolidPen(int w, COLORREF c)   // c: the final colour; w 0 = no pen (PS_NULL)
{
    for (int i = 0; i < s_penN; i++)
        if (s_pens[i].c == c && s_pens[i].w == w)
            return (HPEN)s_pens[i].o;
    if (s_penN == kGdiCache) {
        for (int i = 0; i < s_penN; i++)
            DeleteObject(s_pens[i].o);
        s_penN = 0;
    }
    s_pens[s_penN].c = c;
    s_pens[s_penN].w = w;
    s_pens[s_penN].o = w ? CreatePen(PS_SOLID, w, c) : CreatePen(PS_NULL, 0, 0);
    return (HPEN)s_pens[s_penN++].o;
}

static void Fill(HDC dc, int l, int t, int r, int b, COLORREF c)
{
    RECT rc = { l, t, r, b };
    FillRect(dc, &rc, SolidBrush(C(c)));
}

static void Poly(HDC dc, const POINT* p, int n, COLORREF fill)
{
    HGDIOBJ ob = SelectObject(dc, SolidBrush(C(fill))), op = SelectObject(dc, SolidPen(0, 0));
    Polygon(dc, p, n);
    SelectObject(dc, ob);
    SelectObject(dc, op);
}

// Filled polygon in the 48 x 48 icon grid (for the solid transport glyphs of the player widget).
static void FillPoly(HDC dc, int x, int y, int size, const double* xy, int n, COLORREF c)
{
    double s = size / 48.0;
    POINT p[16];
    for (int i = 0; i < n; i++) {
        p[i].x = x + (int)(xy[2 * i] * s + 0.5);
        p[i].y = y + (int)(xy[2 * i + 1] * s + 0.5);
    }
    Poly(dc, p, n, c);
}

// Filled circle in the 48 x 48 icon grid (dots of the solid glyphs).
static void FillDot(HDC dc, int x, int y, int size, double cx, double cy, double r, COLORREF c)
{
    double s = size / 48.0;
    HGDIOBJ ob = SelectObject(dc, SolidBrush(C(c))), op = SelectObject(dc, SolidPen(0, 0));
    Ellipse(dc, x + (int)((cx - r) * s + 0.5), y + (int)((cy - r) * s + 0.5),
            x + (int)((cx + r) * s + 0.5) + 1, y + (int)((cy + r) * s + 0.5) + 1);
    SelectObject(dc, ob);
    SelectObject(dc, op);
}

// ---------------------------------------------------------------- icons (48 x 48 design grid)

struct Pen {
    HDC dc; int x, y; double s; HPEN pen; HGDIOBJ old;
    Pen(HDC d, int px, int py, int size, COLORREF c) : dc(d), x(px), y(py), s(size / 48.0)
    {
        int w = (int)(3.5 * s + 0.5);
        pen = SolidPen(w < 2 ? 2 : w, C(c));
        old = SelectObject(dc, pen);
        SelectObject(dc, GetStockObject(NULL_BRUSH));
    }
    ~Pen() { SelectObject(dc, old); }
    int X(double v) const { return x + (int)(v * s + 0.5); }
    int Y(double v) const { return y + (int)(v * s + 0.5); }
    void Line(const double* xy, int n)
    {
        POINT p[16];
        for (int i = 0; i < n; i++) {
            p[i].x = X(xy[2 * i]);
            p[i].y = Y(xy[2 * i + 1]);
        }
        Polyline(dc, p, n);
    }
    void L2(double a, double b, double c, double d) { double v[4] = { a, b, c, d }; Line(v, 2); }
    void Circle(double cx, double cy, double r) { Ellipse(dc, X(cx - r), Y(cy - r), X(cx + r), Y(cy + r)); }
    void Box(double l, double t, double r, double b, double rad)
    {
        RoundRect(dc, X(l), Y(t), X(r), Y(b), (int)(rad * 2 * s), (int)(rad * 2 * s));
    }
    // GDI on CE has no Arc(): approximate with a polyline, angles in degrees, 0 = right, clockwise.
    void ArcP(double cx, double cy, double r, double a0, double a1)
    {
        POINT p[24];
        int n = 0;
        for (int i = 0; i <= 20; i++) {
            double a = (a0 + (a1 - a0) * i / 20) * 3.14159265 / 180;
            p[n].x = X(cx + r * cos(a));
            p[n].y = Y(cy + r * sin(a));
            n++;
        }
        Polyline(dc, p, n);
    }
    // The same along an ellipse with radii rx, ry (a0, a1 are its parameter angles).
    void ArcE(double cx, double cy, double rx, double ry, double a0, double a1)
    {
        POINT p[24];
        for (int i = 0; i <= 20; i++) {
            double a = (a0 + (a1 - a0) * i / 20) * 3.14159265 / 180;
            p[i].x = X(cx + rx * cos(a));
            p[i].y = Y(cy + ry * sin(a));
        }
        Polyline(dc, p, 21);
    }
};

static void PaintIcon(HDC dc, Icon ic, int x, int y, int size, COLORREF c)
{
    Pen p(dc, x, y, size, c);
    switch (ic) {
    case IC_MAP: {
        double v[] = { 5, 11, 17, 6, 31, 11, 43, 6, 43, 37, 31, 42, 17, 37, 5, 42, 5, 11 };
        p.Line(v, 9);
        p.L2(17, 6, 17, 37);
        p.L2(31, 11, 31, 42);
        break;
    }
    case IC_NAV: {
        double v[] = { 24, 5, 39, 41, 24, 32, 9, 41, 24, 5 };
        p.Line(v, 5);
        break;
    }
    case IC_RADIO:
        p.Box(5, 17, 43, 42, 4);
        p.L2(13, 17, 33, 6);
        p.Circle(32, 29.5, 5.5);
        p.L2(11, 25, 21, 25);
        p.L2(11, 31, 21, 31);
        p.L2(11, 37, 21, 37);
        break;
    case IC_MUSIC: {
        double v[] = { 18, 36, 18, 11, 39, 7, 39, 31 };
        p.Line(v, 4);
        p.Circle(13, 36, 5);
        p.Circle(34, 31, 5);
        break;
    }
    case IC_PHONE: {
        double v[] = { 15, 6, 21, 6, 24.5, 15, 19.5, 18.5, 22, 23, 25, 26, 29.5, 28.5, 33, 23.5, 42, 27, 42, 33,
                       38, 37, 26, 35, 16, 27, 11, 16, 11, 10, 15, 6 };
        p.Line(v, 16);
        break;
    }
    case IC_GEAR: {   // cog: 8 teeth (root radius 14, tip 20) around a hole
        POINT g[33];
        for (int k = 0; k < 8; k++) {
            static const double da[4] = { -15, -9, 9, 15 }, r[4] = { 14, 20, 20, 14 };
            for (int j = 0; j < 4; j++) {
                double a = (k * 45 + da[j]) * 3.14159265 / 180;
                g[k * 4 + j].x = p.X(24 + r[j] * cos(a));
                g[k * 4 + j].y = p.Y(24 + r[j] * sin(a));
            }
        }
        g[32] = g[0];
        Polyline(dc, g, 33);
        p.Circle(24, 24, 6);
        break;
    }
    case IC_HOME: {
        double v[] = { 6, 23, 24, 7, 42, 23 };
        double w[] = { 11, 19, 11, 41, 37, 41, 37, 19 };
        p.Line(v, 3);
        p.Line(w, 4);
        break;
    }
    case IC_MOON:   // crescent: outer circle (24,24,16) minus inner circle (32,16,13), meeting at 8.6 and 261.4 deg
        p.ArcP(24, 24, 16, 8.6, 261.4);
        p.ArcP(32, 16, 13, 53, 217);
        break;
    case IC_SUN:
        p.Circle(24, 24, 8);
        p.L2(24, 4, 24, 10);
        p.L2(24, 38, 24, 44);
        p.L2(4, 24, 10, 24);
        p.L2(38, 24, 44, 24);
        p.L2(10, 10, 13.5, 13.5);
        p.L2(34.5, 34.5, 38, 38);
        p.L2(10, 38, 13.5, 34.5);
        p.L2(34.5, 13.5, 38, 10);
        break;
    case IC_RETURN:   // (built-in: the skin's undo arrow normally; else the chevron)
    case IC_BACK: {
        double v[] = { 30, 9, 15, 24, 30, 39 };
        p.Line(v, 3);
        break;
    }
    case IC_NEXT: {
        double v[] = { 18, 9, 33, 24, 18, 39 };
        p.Line(v, 3);
        break;
    }
    case IC_CLOSE:
        p.L2(12, 12, 36, 36);
        p.L2(36, 12, 12, 36);
        break;
    case IC_APPS:
        p.Box(6, 6, 21, 21, 3);
        p.Box(27, 6, 42, 21, 3);
        p.Box(6, 27, 21, 42, 3);
        p.Box(27, 27, 42, 42, 3);
        break;
    case IC_VIDEO: {
        double v[] = { 32, 20, 43, 13, 43, 35, 32, 28 };
        p.Box(5, 12, 32, 36, 3);
        p.Line(v, 4);
        break;
    }
    case IC_FOLDER: {
        double v[] = { 5, 11, 18, 11, 22, 16, 43, 16, 43, 39, 5, 39, 5, 11 };
        p.Line(v, 7);
        break;
    }
    case IC_SHIFT: {
        double v[] = { 24, 7, 41, 24, 31, 24, 31, 40, 17, 40, 17, 24, 7, 24, 24, 7 };
        p.Line(v, 8);
        break;
    }
    case IC_BKSP: {
        double v[] = { 16, 11, 43, 11, 43, 37, 16, 37, 4, 24, 16, 11 };
        p.Line(v, 6);
        p.L2(23, 18, 35, 30);
        p.L2(35, 18, 23, 30);
        break;
    }
    case IC_AUX:   // jack plug: cable into a socket, then the U of the plug
        p.L2(24, 4, 24, 17);
        p.Box(18, 17, 30, 26, 2);
        p.L2(20, 26, 20, 43);
        p.L2(20, 43, 28, 43);
        p.L2(28, 43, 28, 26);
        break;
    case IC_PHOTO: {   // picture in a frame (concept v0.1: i-photo)
        double v[] = { 8, 35, 19, 24, 27, 31, 33, 25, 41, 33 };
        p.Box(5, 9, 43, 39, 3);
        p.Circle(16, 19, 3.5);
        p.Line(v, 5);
        break;
    }
    case IC_INFO:
        p.Circle(24, 24, 18);
        p.L2(24, 21, 24, 34);
        p.L2(24, 13, 24, 16);
        break;
    case IC_PREV: {   // solid transport glyphs of the player widget
        double bar[] = { 9, 10, 14, 10, 14, 38, 9, 38 };
        double tri[] = { 40, 10, 40, 38, 20, 24 };
        FillPoly(dc, x, y, size, bar, 4, c);
        FillPoly(dc, x, y, size, tri, 3, c);
        break;
    }
    case IC_FWD: {
        double bar[] = { 34, 10, 39, 10, 39, 38, 34, 38 };
        double tri[] = { 8, 10, 8, 38, 28, 24 };
        FillPoly(dc, x, y, size, bar, 4, c);
        FillPoly(dc, x, y, size, tri, 3, c);
        break;
    }
    case IC_PAUSE: {
        double a[] = { 12, 9, 20, 9, 20, 39, 12, 39 };
        double b[] = { 28, 9, 36, 9, 36, 39, 28, 39 };
        FillPoly(dc, x, y, size, a, 4, c);
        FillPoly(dc, x, y, size, b, 4, c);
        break;
    }
    case IC_BT: {
        double v[] = { 14, 15, 33, 32, 24, 41, 24, 7, 33, 16, 14, 33 };
        p.Line(v, 6);
        break;
    }
    case IC_USB: {
        double a[] = { 24, 30, 14, 22, 14, 16 };
        double b[] = { 24, 25, 34, 18, 34, 12 };
        double d[] = { 20, 10, 24, 5, 28, 10 };
        p.L2(24, 6, 24, 36.5);
        p.Circle(24, 40, 3.5);
        p.Line(a, 3);
        p.Line(b, 3);
        p.Line(d, 3);
        break;
    }
    case IC_SD: {   // a memory card: the cut corner, the contacts
        double v[] = { 18, 6, 38, 6, 38, 42, 10, 42, 10, 14, 18, 6 };
        p.Line(v, 6);
        p.L2(19, 12, 19, 19);
        p.L2(25, 12, 25, 19);
        p.L2(31, 12, 31, 19);
        break;
    }
    case IC_LIST:
        p.L2(16, 12, 42, 12);
        p.L2(16, 24, 42, 24);
        p.L2(16, 36, 42, 36);
        FillDot(dc, x, y, size, 7, 12, 3, c);
        FillDot(dc, x, y, size, 7, 24, 3, c);
        FillDot(dc, x, y, size, 7, 36, 3, c);
        break;
    case IC_VOL: {
        double v[] = { 6, 18, 14, 18, 25, 9, 25, 39, 14, 30, 6, 30, 6, 18 };
        p.Line(v, 7);
        p.ArcP(24.86, 24, 10, -44.4, 44.4);
        p.ArcP(24.55, 24, 18, -46.2, 46.2);
        break;
    }
    case IC_SEARCH:
        p.Circle(21, 21, 13);
        p.L2(31, 31, 42, 42);
        break;
    case IC_SHUFFLE: {
        double a[] = { 6, 14, 14, 14, 32, 34, 41, 34 };
        double b[] = { 6, 34, 14, 34, 19, 28 };
        double d[] = { 27, 20, 32, 14, 41, 14 };
        double e[] = { 36, 9, 41, 14, 36, 19 };
        double f[] = { 36, 29, 41, 34, 36, 39 };
        p.Line(a, 4);
        p.Line(b, 3);
        p.Line(d, 3);
        p.Line(e, 3);
        p.Line(f, 3);
        break;
    }
    case IC_REPEAT: {
        double a[] = { 33, 7, 38, 12, 33, 17 };
        double b[] = { 15, 31, 10, 36, 15, 41 };
        p.L2(8, 22, 8, 18);
        p.ArcP(14, 18, 6, 180, 270);
        p.L2(14, 12, 38, 12);
        p.L2(40, 26, 40, 30);
        p.ArcP(34, 30, 6, 0, 90);
        p.L2(34, 36, 10, 36);
        p.Line(a, 3);
        p.Line(b, 3);
        break;
    }
    case IC_CALL:
    case IC_HANGUP: {   // filled handset; hang up = the same turned by 135 degrees
        double v[] = { 15, 6, 21, 6, 24.5, 15, 19.5, 18.5, 22, 23, 25, 26, 29.5, 28.5, 33, 23.5, 42, 27, 42, 33,
                       38, 37, 26, 35, 16, 27, 11, 16, 11, 10, 15, 6 };
        if (ic == IC_HANGUP)
            for (int i = 0; i < 16; i++) {
                double dx = v[2 * i] - 24, dy = v[2 * i + 1] - 24;
                v[2 * i] = 24 - 0.7071 * dx - 0.7071 * dy;
                v[2 * i + 1] = 24 + 0.7071 * dx - 0.7071 * dy;
            }
        FillPoly(dc, x, y, size, v, 16, c);
        break;
    }
    case IC_WIFI:
        p.ArcP(24, 37.18, 27, 225.3, 314.7);
        p.ArcP(24, 37.45, 18, 223.8, 316.2);
        p.ArcP(24, 37.66, 9, 219, 321);
        FillDot(dc, x, y, size, 24, 38, 2.5, c);
        break;
    case IC_STAR:
    case IC_STAR_F: {
        double v[] = { 24, 5, 29.5, 17.5, 43, 18.5, 32.5, 27.5, 36, 41, 24, 33.5, 12, 41, 15.5, 27.5, 5, 18.5,
                       18.5, 17.5, 24, 5 };
        if (ic == IC_STAR_F)
            FillPoly(dc, x, y, size, v, 10, c);
        p.Line(v, 11);
        break;
    }
    case IC_USER:
        p.Circle(24, 16, 8);
        p.ArcE(24, 42, 16, 14, 180, 360);
        break;
    case IC_USERS:
        p.Circle(18, 17, 7);
        p.ArcE(18, 41, 14, 12, 180, 360);
        p.ArcP(27.4, 17, 7, -68.2, 68.2);
        p.ArcE(30, 41, 14, 12, -65, 0);
        break;
    case IC_CLOCK: {
        double v[] = { 24, 13, 24, 24, 32, 29 };
        p.Circle(24, 24, 18);
        p.Line(v, 3);
        break;
    }
    case IC_KEYPAD: {
        static const double dots[] = { 13, 10, 24, 10, 35, 10, 13, 21, 24, 21, 35, 21, 13, 32, 24, 32, 35, 32, 24, 42 };
        for (int i = 0; i < 10; i++)
            FillDot(dc, x, y, size, dots[2 * i], dots[2 * i + 1], 3.5, c);
        break;
    }
    case IC_MIC_OFF:
        p.Box(17, 5, 31, 29, 7);
        p.ArcP(24, 23, 14, 0, 180);
        p.L2(24, 37, 24, 43);
        p.L2(17, 43, 31, 43);
        p.L2(7, 7, 41, 41);
        break;
    case IC_HANDOVER: {
        double v[] = { 37, 17, 43, 23, 37, 29 };
        p.Box(6, 6, 24, 40, 3);
        p.L2(12, 34, 18, 34);
        p.L2(30, 23, 43, 23);
        p.Line(v, 3);
        break;
    }
    case IC_PLAY: {
        double v[] = { 14, 8, 40, 24, 14, 40 };
        FillPoly(dc, x, y, size, v, 3, c);
        break;
    }
    case IC_ROT_L: {   // a picture turned a quarter: its frame, an arrow round its corner (counter-clockwise)
        double v[] = { 15.7, 22.3, 8.7, 26.3, 4.7, 19.4 };
        p.Box(19, 21, 41, 43, 3);
        p.ArcP(30, 32, 22, 290, 195);
        p.Line(v, 3);
        break;
    }
    case IC_ROT_R: {   // the same clockwise
        double v[] = { 43.3, 19.4, 39.3, 26.3, 32.3, 22.3 };
        p.Box(7, 21, 29, 43, 3);
        p.ArcP(18, 32, 22, 250, 345);
        p.Line(v, 3);
        break;
    }
    case IC_GALLERY: {   // the photos: a picture in front of another
        double back[] = { 13, 13, 13, 7, 43, 7, 43, 31, 37, 31 };
        double hills[] = { 9, 37, 17, 28, 23, 33, 28, 28, 34, 35 };
        p.Line(back, 5);
        p.Box(5, 13, 37, 41, 3);
        p.Line(hills, 5);
        p.Circle(13, 20, 3);
        break;
    }
    case IC_WARN: {
        double v[] = { 24, 6, 44, 40, 4, 40, 24, 6 };
        p.Line(v, 4);
        p.L2(24, 19, 24, 29);
        FillDot(dc, x, y, size, 24, 34.5, 2.5, c);
        break;
    }
    case IC_CALL_IN: {
        double v[] = { 18, 16, 18, 30, 32, 30 };
        p.L2(38, 10, 18, 30);
        p.Line(v, 3);
        break;
    }
    case IC_CALL_OUT: {
        double v[] = { 20, 14, 34, 14, 34, 28 };
        p.L2(14, 34, 34, 14);
        p.Line(v, 3);
        break;
    }
    case IC_CALL_MISS: {
        double v[] = { 6, 16, 20, 30, 30, 20, 42, 32 };
        p.Line(v, 4);
        p.L2(6, 16, 15, 16);
        p.L2(6, 16, 6, 25);
        break;
    }
    case IC_LOCK:
        p.Box(10, 21, 38, 42, 3);
        p.L2(16, 21, 16, 15);
        p.ArcP(24, 15, 8, 180, 360);
        p.L2(32, 15, 32, 21);
        break;
    case IC_REFRESH: {
        double a[] = { 39, 8, 39, 20, 27, 20 };
        double b[] = { 9, 40, 9, 28, 21, 28 };
        p.ArcP(23.6, 24.4, 16, -16, -148.3);
        p.ArcP(24.4, 23.6, 16, 164, 31.7);
        p.Line(a, 3);
        p.Line(b, 3);
        break;
    }
    case IC_EQ: {
        double a[] = { 7, 26, 17, 26, 17, 33, 7, 33 };
        double b[] = { 19, 12, 29, 12, 29, 19, 19, 19 };
        double d[] = { 31, 30, 41, 30, 41, 37, 31, 37 };
        p.L2(12, 6, 12, 42);
        p.L2(24, 6, 24, 42);
        p.L2(36, 6, 36, 42);
        FillPoly(dc, x, y, size, a, 4, c);
        FillPoly(dc, x, y, size, b, 4, c);
        FillPoly(dc, x, y, size, d, 4, c);
        break;
    }
    case IC_DISC:
        p.Circle(24, 24, 18);
        p.Circle(24, 24, 5);
        break;
    case IC_EYE:
    case IC_EYE_OFF:   // (crossed out: a hidden widget)
        p.ArcE(24, 24, 20, 9, 180, 360);
        p.ArcE(24, 24, 20, 9, 0, 180);
        p.Circle(24, 24, 6);
        if (ic == IC_EYE_OFF)
            p.L2(6, 6, 42, 42);
        break;
    case IC_SIGNAL:   // the USB modem in the top bar: four bars up
        p.L2(9, 40, 9, 34);
        p.L2(19, 40, 19, 27);
        p.L2(29, 40, 29, 19);
        p.L2(39, 40, 39, 10);
        break;
    case IC_POWER:
        p.ArcP(24, 24.69, 15, 237.8, -57.8);
        p.L2(24, 5, 24, 23);
        break;
    case IC_EDIT: {   // pencil (mockup i-edit)
        double v[] = { 30, 8, 40, 18, 18, 40, 8, 40, 8, 30, 30, 8 };
        p.Line(v, 6);
        p.L2(26, 12, 36, 22);
        break;
    }
    case IC_ADD:
        p.L2(24, 9, 24, 39);
        p.L2(9, 24, 39, 24);
        break;
    case IC_FILE: {
        double v[] = { 11, 5, 28, 5, 37, 14, 37, 43, 11, 43, 11, 5 };
        double w[] = { 28, 5, 28, 14, 37, 14 };
        p.Line(v, 6);
        p.Line(w, 3);
        break;
    }
    case IC_FULL: {   // full screen: four corners outwards
        double a[] = { 6, 17, 6, 6, 17, 6 }, b[] = { 31, 6, 42, 6, 42, 17 }, c2[] = { 42, 31, 42, 42, 31, 42 },
               d[] = { 17, 42, 6, 42, 6, 31 };
        p.Line(a, 3);
        p.Line(b, 3);
        p.Line(c2, 3);
        p.Line(d, 3);
        break;
    }
    case IC_UNFULL: {   // back from full screen: the corners inwards
        double a[] = { 6, 17, 17, 17, 17, 6 }, b[] = { 31, 6, 31, 17, 42, 17 }, c2[] = { 42, 31, 31, 31, 31, 42 },
               d[] = { 17, 42, 17, 31, 6, 31 };
        p.Line(a, 3);
        p.Line(b, 3);
        p.Line(c2, 3);
        p.Line(d, 3);
        break;
    }
    case IC_UP: {
        double v[] = { 9, 31, 24, 16, 39, 31 };
        p.Line(v, 3);
        break;
    }
    case IC_DOWN: {
        double v[] = { 9, 17, 24, 32, 39, 17 };
        p.Line(v, 3);
        break;
    }
    case IC_CAR: {   // side view: roof and bonnet, two wheels
        double v[] = { 8.5, 33, 4, 33, 4, 25, 10, 23, 16, 13, 32, 13, 39, 23, 44, 25, 44, 33, 39.5, 33 };
        p.Line(v, 10);
        p.L2(17.5, 33, 30.5, 33);
        p.L2(10, 23, 39, 23);
        p.Circle(13, 33, 4.5);
        p.Circle(35, 33, 4.5);
        break;
    }
    case IC_NETRADIO:   // the internet radio: a mast sending waves both ways
        p.L2(24, 22, 15, 43);
        p.L2(24, 22, 33, 43);
        p.L2(18.5, 35, 29.5, 35);
        FillDot(dc, x, y, size, 24, 18, 3.5, c);
        p.ArcP(24, 18, 8, 140, 220);
        p.ArcP(24, 18, 8, 320, 400);
        p.ArcP(24, 18, 15, 145, 215);
        p.ArcP(24, 18, 15, 325, 395);
        break;
    case IC_BTMUSIC: {   // the phone's music: a note with the Bluetooth rune
        double v[] = { 13, 37, 13, 14, 27, 10, 27, 33 };
        p.Line(v, 4);
        p.Circle(9, 37, 4);
        p.Circle(23, 33, 4);
        double r[] = { 33, 19, 43, 29, 38, 34, 38, 10, 43, 15, 33, 25 };
        p.Line(r, 6);
        break;
    }
    case IC_CLOUD:   // three bumps on a flat base
        p.ArcP(15, 30, 7, 90, 250);
        p.ArcP(25, 25, 10, 190, 330);
        p.ArcP(34, 30, 7, 250, 450);
        p.L2(15, 37, 34, 37);
        break;
    case IC_TASKS: {   // task manager: a screen with the load's pulse
        double v[] = { 10, 26, 17, 26, 21, 17, 27, 33, 31, 23, 38, 23 };
        p.Box(5, 9, 43, 39, 4);
        p.Line(v, 6);
        break;
    }
    default:
        break;
    }
}

// ---------------------------------------------------------------- skin pictures

#define PIX(r, g, b) ((Pix)(((r) << 16) | ((g) << 8) | (b)))
#define PR(p) ((int)((p) >> 16) & 255)
#define PG(p) ((int)((p) >> 8) & 255)
#define PB(p) ((int)(p) & 255)

// 32-bit bottom-up DIB section (0x00RRGGBB), with pixels reachable for blending.
static HBITMAP MakeDib(int w, int h, Pix** bits)
{
    struct { BITMAPINFOHEADER h; DWORD masks[3]; } bi;
    memset(&bi, 0, sizeof(bi));
    bi.h.biSize = sizeof(BITMAPINFOHEADER);
    bi.h.biWidth = w;
    bi.h.biHeight = h;
    bi.h.biPlanes = 1;
    bi.h.biBitCount = 32;
    bi.h.biCompression = BI_RGB;
    HDC screen = GetDC(NULL);
    HBITMAP bmp = CreateDIBSection(screen, (BITMAPINFO*)&bi, DIB_RGB_COLORS, (void**)bits, NULL, 0);
    ReleaseDC(NULL, screen);
    return bmp;
}

// Blends a skin picture scaled (bilinear) to w x h at (x, y) into the back buffer; dimmed at night.
// With a tint the picture is only a mask: painted in that colour.
// One pixel of a picture scaled to w x h (bilinear, premultiplied RGBA) and tinted: s[0..3].
static void ImgSample(const Img& im, int w, int h, COLORREF tint, int dx, int dy, int* s)
{
    int fy = (2 * dy + 1) * im.h * 128 / h - 128;   // source position, 8-bit fraction
    if (fy < 0)
        fy = 0;
    int y0 = fy >> 8, wy = fy & 255, y1 = y0 + 1 < im.h ? y0 + 1 : y0;
    int fx = (2 * dx + 1) * im.w * 128 / w - 128;
    if (fx < 0)
        fx = 0;
    int x0 = fx >> 8, wx = fx & 255, x1 = x0 + 1 < im.w ? x0 + 1 : x0;
    const unsigned char* r0 = im.px + y0 * im.w * 4;
    const unsigned char* r1 = im.px + y1 * im.w * 4;
    const unsigned char *pa = r0 + x0 * 4, *pb = r0 + x1 * 4, *pc = r1 + x0 * 4, *pd = r1 + x1 * 4;
    for (int k = 0; k < 4; k++)
        s[k] = ((pa[k] * (256 - wx) + pb[k] * wx) * (256 - wy) + (pc[k] * (256 - wx) + pd[k] * wx) * wy) >> 16;
    if (tint != CLR_INVALID && s[3]) {
        s[0] = GetRValue(tint) * s[3] / 255;
        s[1] = GetGValue(tint) * s[3] / 255;
        s[2] = GetBValue(tint) * s[3] / 255;
    }
}

// The pictures as DrawImg blends them, scaled and tinted once rather than every frame (the skin's icons come in a
// few sizes and colours each): up to 128 x 128, kImgCopies of them, 256 KB at most (the unit's memory) - past it
// all dropped at once; dropped too when a picture is freed (ImgCopiesDrop: the programs' icons).
struct ImgCopy {
    const unsigned char* src;
    int                  w, h;
    COLORREF             tint;
    unsigned char*       px;   // w * h * 4: s[0..3] of ImgSample
};
enum { kImgCopies = 48, kImgCopyBytes = 256 * 1024 };
static ImgCopy s_imgCopy[kImgCopies];
static int     s_imgCopyN, s_imgCopyBytes;

static void ImgCopiesDrop()
{
    for (int i = 0; i < s_imgCopyN; i++)
        free(s_imgCopy[i].px);
    s_imgCopyN = s_imgCopyBytes = 0;
}

static const unsigned char* ImgScaled(const Img& im, int w, int h, COLORREF tint)
{
    if (w > 128 || h > 128)
        return NULL;
    for (int i = 0; i < s_imgCopyN; i++) {
        const ImgCopy& c = s_imgCopy[i];
        if (c.src == im.px && c.w == w && c.h == h && c.tint == tint)
            return c.px;
    }
    int bytes = w * h * 4;
    if (s_imgCopyN == kImgCopies || s_imgCopyBytes + bytes > kImgCopyBytes)
        ImgCopiesDrop();
    unsigned char* px = (unsigned char*)malloc(bytes);
    if (!px)
        return NULL;
    int s[4];
    for (int dy = 0; dy < h; dy++)
        for (int dx = 0; dx < w; dx++) {
            ImgSample(im, w, h, tint, dx, dy, s);
            unsigned char* q = px + (dy * w + dx) * 4;
            q[0] = (unsigned char)s[0];
            q[1] = (unsigned char)s[1];
            q[2] = (unsigned char)s[2];
            q[3] = (unsigned char)s[3];
        }
    ImgCopy& c = s_imgCopy[s_imgCopyN++];
    c.src = im.px;
    c.w = w;
    c.h = h;
    c.tint = tint;
    c.px = px;
    s_imgCopyBytes += bytes;
    return px;
}

static void DrawImg(const Img& im, int x, int y, int w, int h, COLORREF tint = CLR_INVALID)
{
#ifndef UNDER_CE
    GdiFlush();   // desktop GDI batches drawing: finish it before touching the pixels
#endif
    const unsigned char* copy = ImgScaled(im, w, h, tint);
    for (int dy = 0; dy < h; dy++) {
        int py = y + dy;
        if (py < s_clip.top || py >= s_clip.bottom)
            continue;
        Pix* dst = s_backBits + (479 - py) * 800;
        for (int dx = 0; dx < w; dx++) {
            int px = x + dx;
            if (px < s_clip.left || px >= s_clip.right)
                continue;
            int s[4];
            if (copy) {
                const unsigned char* q = copy + (dy * w + dx) * 4;
                s[0] = q[0];
                s[1] = q[1];
                s[2] = q[2];
                s[3] = q[3];
            } else {
                ImgSample(im, w, h, tint, dx, dy, s);
            }
            if (!s[3])
                continue;
            if (s_night)
                for (int k = 0; k < 3; k++)
                    s[k] = s[k] * 2 / 3;
            unsigned p = dst[px];
            int inv = 255 - s[3];
            int r = s[0] + PR(p) * inv / 255;
            int g = s[1] + PG(p) * inv / 255;
            int b = s[2] + PB(p) * inv / 255;
            dst[px] = PIX(r > 255 ? 255 : r, g > 255 ? 255 : g, b > 255 ? 255 : b);
        }
    }
}


// ---------------------------------------------------------------- frame

// The text's face (2026-10-23, the user: Golos Text, design 0.6): VestaHome's own - Fonts\ in its folder (SIL OFL,
// static TrueType instances, autohinted for the unsmoothed text below), added at the start (FontsLoad: AddFontResource,
// no registry - the unit resets it); not there or not taken - the unit's Tahoma.
static const wchar_t* s_face = L"Tahoma";

static int FontsLoad()   // the fonts taken (2 = both: the face is ours)
{
    static const wchar_t* files[3] = { L"GolosText-Regular.ttf", L"GolosText-Bold.ttf", L"OFL.txt" };
    int ok = 0;
    for (int i = 0; i < 2; i++) {
        wchar_t p[MAX_PATH];
        _snwprintf(p, MAX_PATH, L"%sFonts\\%s", s_dir, files[i]);
        p[MAX_PATH - 1] = 0;
        ok += AddFontResource(p) > 0;   // (the PC: for this session)
    }
    if (ok == 2)
        s_face = L"Golos Text";
#ifdef UNDER_CE
    // the install before \ResidentFlash left them in the old \SDMMC\VestaHome (taken by the running VestaHome, the
    // installer could not delete them; HU 2026-10-27-2): away now, and the folder if nothing else is in it
    if (!_wcsnicmp(s_dir, L"\\ResidentFlash\\", 15)) {
        for (int i = 0; i < 3; i++) {
            wchar_t p[MAX_PATH];
            _snwprintf(p, MAX_PATH, L"\\SDMMC\\VestaHome\\Fonts\\%s", files[i]);
            DeleteFile(p);
        }
        RemoveDirectory(L"\\SDMMC\\VestaHome\\Fonts");
        RemoveDirectory(L"\\SDMMC\\VestaHome");
    }
#endif
    return ok;
}

static HFONT MakeFont(int height, int weight)   // (not smoothed: VestaCore vfont.h)
{
    return VFontMake(s_face, height, weight);
}

// Whole file into a malloc'ed buffer with a terminating zero; NULL if it can't be read.
static char* ReadAll(const wchar_t* path, DWORD* size)
{
    HANDLE h = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return NULL;
    DWORD n = GetFileSize(h, NULL), got = 0;
    char* buf = n == 0xFFFFFFFF ? NULL : (char*)malloc(n + 1);
    if (buf && !ReadFile(h, buf, n, &got, NULL)) {
        free(buf);
        buf = NULL;
    }
    CloseHandle(h);
    if (buf) {
        buf[got] = 0;
        *size = got;
    }
    return buf;
}

// The text of ReadAll's buffer: past its UTF-8 BOM.
static char* Utf8Text(char* buf)
{
    unsigned char* u = (unsigned char*)buf;
    return u[0] == 0xEF && u[1] == 0xBB && u[2] == 0xBF ? buf + 3 : buf;
}

// The next line of a UTF-8 text (Utf8Text) into `line` ("" when it does not fit); false at the end. The text is cut
// at its '\n's.
static bool NextUtf8Line(char** p, wchar_t* line, int cap)
{
    if (!**p)
        return false;
    char* end = strchr(*p, '\n');
    if (end)
        *end = 0;
    if (MultiByteToWideChar(CP_UTF8, 0, *p, -1, line, cap) <= 0)
        line[0] = 0;
    *p = end ? end + 1 : *p + strlen(*p);
    return true;
}

// A skin's 800 x 480 picture into bits (as is: night dims it when a frame is drawn).
static bool LoadBackground(const wchar_t* path, Pix* bits)
{
    DWORD size;
    char* data = ReadAll(path, &size);
    if (!data)
        return false;
    int w, h, comp;
    unsigned char* rgb = stbi_load_from_memory((unsigned char*)data, size, &w, &h, &comp, 3);
    free(data);
    if (!rgb)
        return false;
    for (int y = 0; y < 480; y++) {
        Pix* dst = bits + (479 - y) * 800;
        for (int x = 0; x < 800; x++) {
            COLORREF c = kInk;
            if (x < w && y < h) {
                const unsigned char* p = rgb + (y * w + x) * 3;
                c = RGB(p[0], p[1], p[2]);
            }
            dst[x] = PIX(GetRValue(c), GetGValue(c), GetBValue(c));
        }
    }
    stbi_image_free(rgb);
    return true;
}

enum { kTileW = 64, kTileH = 480 };   // design 0.6: a column of the mosaic's cells (64 x 112), the whole height
// The orange glow at the top right (the mockup's radial gradient: 7 % of the accent at 85 %, 0 fading out by 55 % of
// a 120 % x 90 % ellipse): its strength (of 255) over x kGlowX..800, y 0..kGlowH, made once.
enum { kGlowX = 152, kGlowW = 800 - kGlowX, kGlowH = 238 };
static unsigned char* s_glow;

// Night: a third less brightness, as C() does.
static Pix DimPix(Pix p)
{
    return PIX(PR(p) * 2 / 3, PG(p) * 2 / 3, PB(p) * 2 / 3);
}

// The skin's tile (skin.ini "tile", 2026-10-21: a picture repeated over every screen's background instead of
// the triangles): its pixels, day and night, into s_tile; false = none, or not readable (the mosaic then).
static bool LoadTile(const wchar_t* path)
{
    DWORD size;
    char* data = ReadAll(path, &size);
    if (!data)
        return false;
    int w, h, comp;
    unsigned char* rgb = stbi_load_from_memory((unsigned char*)data, size, &w, &h, &comp, 3);
    free(data);
    if (!rgb)
        return false;
    bool ok = w >= 8 && h >= 8 && w <= 400 && h <= 400;
    if (ok) {
        s_tile[0] = (Pix*)malloc(w * h * sizeof(Pix));
        s_tile[1] = (Pix*)malloc(w * h * sizeof(Pix));
        ok = s_tile[0] && s_tile[1];
    }
    if (ok) {
        for (int i = 0; i < w * h; i++) {
            const unsigned char* p = rgb + i * 3;
            s_tile[0][i] = PIX(p[0], p[1], p[2]);
            s_tile[1][i] = DimPix(s_tile[0][i]);
        }
        s_tileW = w;
        s_tileH = h;
    } else {
        free(s_tile[0]);
        free(s_tile[1]);
        s_tile[0] = s_tile[1] = NULL;
    }
    stbi_image_free(rgb);
    return ok;
}

// The background is periodic: one period is kept, day and night (the mosaic's column 120 KB each, or the skin's
// tile), and copied into the frame row by row (Pattern) instead of keeping whole 800 x 480 backgrounds (1.5 MB
// each). Only the skin's own main menu picture, if it gives one, is kept full-size. Built once at
// start, in day colours.
static void BuildBackground()
{
    if (s_skinTile[0] && !LoadTile(s_skinTile))
        s_skinTile[0] = 0;   // (the triangles below)
    if (!s_skinTile[0]) {   // design 0.6, as the mockup: rhombi of four triangles meeting in a 64 x 112 cell's
        // middle, darkened towards the bottom (25 % .. 70 % of the background) row of cells by row; each triangle one
        // solid colour (the user 2026-10-23: no dither noise, no glow)
        s_tile[0] = (Pix*)malloc(kTileW * kTileH * sizeof(Pix));
        s_tile[1] = (Pix*)malloc(kTileW * kTileH * sizeof(Pix));
        if (s_tile[0] && s_tile[1]) {
            int ir = GetRValue(kInk), ig = GetGValue(kInk), ib = GetBValue(kInk);
            for (int y = 0; y < kTileH; y++) {
                int cy = y / 112 * 112 + 56, a = 64 + 115 * (cy < kTileH ? cy : kTileH - 1) / (kTileH - 1);
                int fy = y % 112 - 56;
                for (int x = 0; x < kTileW; x++) {
                    int fx = x - 32;
                    // which of the four: the diagonals through the middle (|fx| / 32 against |fy| / 56)
                    int k = abs(fx) * 56 > abs(fy) * 32 ? (fx < 0 ? 0 : 1) : (fy < 0 ? 2 : 3);
                    COLORREF c = kTri[k];
                    int r = (GetRValue(c) * (256 - a) + ir * a) / 256, g = (GetGValue(c) * (256 - a) + ig * a) / 256,
                        b = (GetBValue(c) * (256 - a) + ib * a) / 256;
                    s_tile[0][y * kTileW + x] = PIX(r, g, b);
                    s_tile[1][y * kTileW + x] = PIX(r * 2 / 3, g * 2 / 3, b * 2 / 3);
                }
            }
            s_tileW = kTileW;
            s_tileH = kTileH;
        }
        s_glow = (unsigned char*)malloc(kGlowW * kGlowH);
        if (s_glow)
            for (int y = 0; y < kGlowH; y++)
                for (int x = 0; x < kGlowW; x++) {
                    double dx = (kGlowX + x - 680) / 528.0, dy = y / 238.0, e = sqrt(dx * dx + dy * dy);
                    s_glow[y * kGlowW + x] = (unsigned char)(e < 1 ? (1 - e) * 255 + 0.5 : 0);
                }
    }
    if (s_skinBg[0]) {
        s_bg = MakeDib(800, 480, &s_bgBits);
        if (s_bg && !LoadBackground(s_skinBg, s_bgBits)) {
            DeleteObject(s_bg);
            s_bg = NULL;
        }
    }
}

// One row of the mosaic (y counted from anchor) into dst's pixels [x0, x1).
static void TileRow(Pix* dst, const Pix* t, int y, int anchor, int x0, int x1)
{
    const Pix* src = t + ((y - anchor) % s_tileH) * s_tileW;
    for (int x = x0; x < x1;) {
        int o = x % s_tileW, n = s_tileW - o < x1 - x ? s_tileW - o : x1 - x;
        memcpy(dst + x, src + o, n * sizeof(Pix));
        x += n;
    }
}

// The orange glow at the top right over row y's pixels [x0, x1) (y < kGlowH, x0 >= kGlowX).
static void GlowRow(Pix* row, int y, int x0, int x1, COLORREF o)
{
    int orr = GetRValue(o), og = GetGValue(o), ob = GetBValue(o);
    const int kMax = 7864;   // 12 % of 65536 (HU 2026-10-24: brighter than the mockup's 7 %)
    const unsigned char* g = s_glow + y * kGlowW - kGlowX;
    for (int x = x0; x < x1; x++) {
        int a = g[x] * kMax / 255;   // of 65536
        if (!a)
            continue;
        Pix p = row[x];
        int r = PR(p), gg = PG(p), b = PB(p);
        row[x] = PIX(r + ((orr - r) * a + 32768) / 65536, gg + ((og - gg) * a + 32768) / 65536,
                     b + ((ob - b) * a + 32768) / 65536);
    }
}

// The background's top rows [0, kGlowH) with the glow, 800 wide, made once for the night and the accent it shows
// (the glow was blended into every frame again: 154 000 pixels); NULL - no glow, or no memory (then per frame).
static Pix*     s_glowBg;
static int      s_glowBgNight = -1;
static COLORREF s_glowBgColor;

static const Pix* GlowBackground(const Pix* t)
{
    if (!s_glow)
        return NULL;
    COLORREF o = C(kOrange);
    if (s_glowBg && s_glowBgNight == (int)s_night && s_glowBgColor == o)
        return s_glowBg;
    if (!s_glowBg)
        s_glowBg = (Pix*)malloc(800 * kGlowH * sizeof(Pix));
    if (!s_glowBg)
        return NULL;
    for (int y = 0; y < kGlowH; y++) {
        TileRow(s_glowBg + y * 800, t, y, 0, 0, 800);
        GlowRow(s_glowBg + y * 800, y, kGlowX, 800, o);
    }
    s_glowBgNight = s_night;
    s_glowBgColor = o;
    return s_glowBg;
}

// Frame rows [top, bottom) filled with the mosaic, its rows counted from anchor (<= top); the glow over it.
static void Pattern(int top, int bottom, int anchor)
{
    const Pix* t = s_tile[s_night ? 1 : 0];
    if (!t)
        return;
#ifndef UNDER_CE
    GdiFlush();
#endif
    if (top < s_clip.top)
        top = s_clip.top;
    if (bottom > s_clip.bottom)
        bottom = s_clip.bottom;
    const Pix* gb = anchor == 0 ? GlowBackground(t) : NULL;
    int x0 = s_clip.left > kGlowX ? s_clip.left : kGlowX;
    for (int y = top; y < bottom; y++) {
        Pix* dst = s_backBits + (479 - y) * 800;
        if (gb && y < kGlowH) {
            memcpy(dst + s_clip.left, gb + y * 800 + s_clip.left, (s_clip.right - s_clip.left) * sizeof(Pix));
            continue;
        }
        TileRow(dst, t, y, anchor, s_clip.left, s_clip.right);
        if (s_glow && y < kGlowH)
            GlowRow(dst, y, x0, s_clip.right, C(kOrange));
    }
}

// ---------------------------------------------------------------- the other screens
// Drawn after the mockups: skin/vesta-skin-v0.1.html (radio, player, phone keypad, display settings) and
// skin/make_v01_screens.py (the rest), with their coordinates: x, y, w, h as in the SVG, text y = baseline.
// Buttons of these screens are registered while they are drawn (Zone), so a picture and its hit area match.

static void Blit(HDC dc, HBITMAP bmp)
{
    HDC src = CreateCompatibleDC(dc);
    HGDIOBJ ob = SelectObject(src, bmp);
    BitBlt(dc, 0, 0, 800, 480, src, 0, 0, SRCCOPY);
    SelectObject(src, ob);
    DeleteDC(src);
}

// The mosaic under the top bar too (design 0.6).
static void SubBackground(HDC dc)
{
    Pattern(0, 480, 0);
}

// A full-screen picture into the frame, a third darker with the night theme (as C()).
static void BlitNight(HDC dc, HBITMAP bmp)
{
    Blit(dc, bmp);
    if (s_night) {
#ifndef UNDER_CE
        GdiFlush();
#endif
        for (int y = s_clip.top; y < s_clip.bottom; y++) {
            Pix* row = s_backBits + (479 - y) * 800;
            for (int x = s_clip.left; x < s_clip.right; x++)
                row[x] = DimPix(row[x]);
        }
    }
}

// The main screen: the skin's background picture (its night copy, a third darker, made once - it was dimmed pixel
// by pixel on every frame), otherwise the same as the others.
static HBITMAP         s_bgDim;
static Pix*            s_bgDimBits;

static void MainBackground(HDC dc)
{
    if (!s_bg) {
        SubBackground(dc);
        return;
    }
    if (s_night && !s_bgDim && s_bgBits) {
        s_bgDim = MakeDib(800, 480, &s_bgDimBits);
        if (s_bgDim && s_bgDimBits)
            for (int i = 0; i < 800 * 480; i++)
                s_bgDimBits[i] = DimPix(s_bgBits[i]);
    }
    if (s_night && s_bgDim)
        Blit(dc, s_bgDim);
    else
        BlitNight(dc, s_bg);
}

// The screen saver's background (Settings -> General -> Theme, the user 2026-10-23): 0 the mosaic - the skin's saver
// picture without a light, the soft spot over the clock drawn in the accent colour; 1 a picture of one's own (any
// PNG / JPG / BMP, fitted to cover the screen the way the photos are: scaled down while decoding; the example, the
// headlights, is Skin\saver2.png). Loaded on its first frame, freed when it closes (SaverFree); none - plain dark.
static int            s_saverSel;       // the choice (ApplyAccent sets it from the settings)
static wchar_t        s_saverFile[MAX_PATH];   // its own picture's full path (ApplyAccent)
static int            s_saverLoaded = -1;
static bool           s_saverFailed[2];
static unsigned char* s_saverGlow;      // the spot's strength (of 255) at half the resolution, 400 x 240

static void SaverFree()
{
    if (s_saverBg) {
        DeleteObject(s_saverBg);
        s_saverBg = NULL;
    }
    free(s_saverGlow);
    s_saverGlow = NULL;
    s_saverLoaded = -1;
}

// The spot in the accent colour: 16 % in the middle (400, 192), a Gaussian fall-off (as make_saver.py drew it).
static void SaverSpot()
{
    if (!s_saverGlow) {
        s_saverGlow = (unsigned char*)malloc(400 * 240);
        if (!s_saverGlow)
            return;
        for (int y = 0; y < 240; y++)
            for (int x = 0; x < 400; x++) {
                double dx = (x * 2 - 400) / 330.0, dy = (y * 2 - 192) / 200.0;
                s_saverGlow[y * 400 + x] = (unsigned char)(255 * exp(-2.2 * (dx * dx + dy * dy)) + 0.5);
            }
    }
#ifndef UNDER_CE
    GdiFlush();
#endif
    COLORREF o = C(kOrange);
    int orr = GetRValue(o), og = GetGValue(o), ob = GetBValue(o);
    const int kMax = 10486;   // 16 % of 65536
    for (int y = s_clip.top; y < s_clip.bottom; y++) {
        const unsigned char* g = s_saverGlow + (y / 2) * 400;
        Pix* row = s_backBits + (479 - y) * 800;
        for (int x = s_clip.left; x < s_clip.right; x++) {
            int a = g[x / 2] * kMax / 255;
            if (!a)
                continue;
            Pix p = row[x];
            int r = PR(p), gg = PG(p), b = PB(p);
            row[x] = PIX(r + ((orr - r) * a + 32768) / 65536, gg + ((og - gg) * a + 32768) / 65536,
                         b + ((ob - b) * a + 32768) / 65536);
        }
    }
}

static void SaverBackground(HDC dc)
{
    int sel = s_saverSel == 1 ? 1 : 0;
    if (s_saverLoaded != sel)
        SaverFree();
    if (!s_saverBg && !s_saverFailed[sel]) {
        const wchar_t* path = sel == 1 ? s_saverFile : s_skinSaver;
        s_saverBg = path[0] ? MakeDib(800, 480, &s_saverBits) : NULL;
        bool ok = false;
        if (s_saverBg && sel == 1) {
            Pic pic = { 0, 0, NULL, NULL };
            if (PicLoad(path, 800, 480, true, 0, true, &pic, NULL, NULL, true) == PIC_OK && pic.px32) {
                for (int y = 0; y < 480; y++)   // (the DIB is bottom-up)
                    for (int x = 0; x < 800; x++)
                        s_saverBits[(479 - y) * 800 + x] = x < pic.w && y < pic.h ? pic.px32[y * pic.w + x]
                                                 : PIX(GetRValue(kInk), GetGValue(kInk), GetBValue(kInk));
                ok = true;
            }
            PicFree(&pic);
        } else if (s_saverBg) {
            ok = LoadBackground(path, s_saverBits);
        }
        if (s_saverBg && !ok) {
            DeleteObject(s_saverBg);
            s_saverBg = NULL;
        }
        s_saverFailed[sel] = !s_saverBg;   // not again
        s_saverLoaded = sel;
    }
    if (s_saverBg)
        BlitNight(dc, s_saverBg);
    else
        Fill(dc, 0, 0, 800, 480, kInk);
    if (sel == 0)
        SaverSpot();
}

// a over b with alpha 0..255
static COLORREF Mix(COLORREF a, COLORREF b, int alpha)
{
    return RGB((GetRValue(a) * alpha + GetRValue(b) * (255 - alpha)) / 255,
               (GetGValue(a) * alpha + GetGValue(b) * (255 - alpha)) / 255,
               (GetBValue(a) * alpha + GetBValue(b) * (255 - alpha)) / 255);
}

struct FontEnt { int size, weight; HFONT f; };
static FontEnt s_fonts[160];
static int     s_fontCount;

// Fonts by pixel size and weight (400 / 600 / 700 as in the mockups), created once.
static HFONT Font(int size, int weight)
{
    for (int i = 0; i < s_fontCount; i++)
        if (s_fonts[i].size == size && s_fonts[i].weight == weight)
            return s_fonts[i].f;
    HFONT f = MakeFont(size, weight);
    if (s_fontCount < 160) {
        s_fonts[s_fontCount].size = size;
        s_fonts[s_fontCount].weight = weight;
        s_fonts[s_fontCount++].f = f;
    }
    return f;
}

static DWORD s_txtMeasures;   // GetTextExtent calls so far: a frame's count is a measure of its text work

static int TextWidth(HDC dc, const wchar_t* s, int size, int weight)
{
    s_txtMeasures++;
    SIZE sz;
    HGDIOBJ of = SelectObject(dc, Font(size, weight));
    GetTextExtentPoint32(dc, s, (int)wcslen(s), &sz);
    SelectObject(dc, of);
    return sz.cx;
}

enum { AL_START, AL_MID, AL_END };

// How many characters of s fit into maxw (one GetTextExtentExPoint instead of measuring every length).
static int FitChars(HDC dc, const wchar_t* s, int n, int size, int weight, int maxw)
{
    s_txtMeasures++;
    SIZE sz;
    int fit = 0;
    HGDIOBJ of = SelectObject(dc, Font(size, weight));
    if (!GetTextExtentExPoint(dc, s, n, maxw > 0 ? maxw : 0, &fit, NULL, &sz))
        fit = n;
    SelectObject(dc, of);
    return fit < 0 ? 0 : fit > n ? n : fit;
}

// s cut to fit maxw with an ellipsis at its end (into out, cap characters); returns its width.
static int Ellipsize(HDC dc, const wchar_t* s, int size, int weight, int maxw, wchar_t* out, int cap)
{
    int n = (int)wcslen(s);
    if (n > cap - 2)
        n = cap - 2;
    int fit = FitChars(dc, s, n, size, weight, maxw - TextWidth(dc, L"…", size, weight)) + 1;
    if (fit < n)   // the start that fits with the ellipsis (+1: checked below, as before, one by one down)
        n = fit;
    for (;;) {
        wcsncpy(out, s, n);
        out[n] = 0;
        while (n > 0 && out[n - 1] == L' ')
            out[--n] = 0;
        if (n < (int)wcslen(s))
            wcscat(out, L"…");
        int w = TextWidth(dc, out, size, weight);
        if (w <= maxw || n == 0)
            return w;
        n--;
    }
}

// Text like SVG <text>: baseline at y, anchored at its start, middle or end. A line wider than maxw
// (0 = no limit) is set smaller until it fits: Tahoma is wider than the mockups' font; still too wide at 10 px,
// it is cut with an ellipsis. Returns its right edge.
static int T(HDC dc, int x, int y, const wchar_t* s, int size, COLORREF c, int weight = 600, int anchor = AL_START,
             int maxw = 0)
{
    int w = TextWidth(dc, s, size, weight);
    if (maxw > 0 && w > maxw && size > 10) {   // the biggest size that fits: guessed by proportion, then +-1
        int top = size;
        size = size * maxw / w;
        size = size < 10 ? 10 : size >= top ? top - 1 : size;
        w = TextWidth(dc, s, size, weight);
        while (w <= maxw && size + 1 < top) {
            int w2 = TextWidth(dc, s, size + 1, weight);
            if (w2 > maxw)
                break;
            size++;
            w = w2;
        }
        while (w > maxw && size > 10)
            w = TextWidth(dc, s, --size, weight);
    }
    wchar_t cut[256];
    if (maxw > 0 && w > maxw) {
        w = Ellipsize(dc, s, size, weight, maxw, cut, 256);
        s = cut;
    }
    int left = anchor == AL_MID ? x - w / 2 : anchor == AL_END ? x - w : x;
    HGDIOBJ of = SelectObject(dc, Font(size, weight));
    SetTextAlign(dc, TA_LEFT | TA_BASELINE);
    SetTextColor(dc, C(c));
    SetBkMode(dc, TRANSPARENT);
    ExtTextOut(dc, left, y, 0, NULL, s, (int)wcslen(s), NULL);
    SetTextAlign(dc, TA_LEFT | TA_TOP);
    SelectObject(dc, of);
    return left + w;
}

// A widget's status or error: words wrapped into up to `lines` lines of maxw (the size kept), the last one cut
// with an ellipsis; baselines y, y + lineH, ... Returns the lines used.
static int TWrap(HDC dc, int x, int y, const wchar_t* s, int size, COLORREF c, int weight, int maxw, int lines,
                 int lineH)
{
    wchar_t line[256];
    int used = 0;
    while (*s && used < lines) {
        while (*s == L' ')
            s++;
        int n = (int)wcslen(s), take = n;
        if (used < lines - 1 && TextWidth(dc, s, size, weight) > maxw) {   // the longest start up to a space
            int fit = FitChars(dc, s, n < 255 ? n : 255, size, weight, maxw);
            take = 0;
            for (int i = fit; i > 0; i--)   // the last word end within what fits
                if (i == n || s[i] == L' ') {
                    take = i;
                    break;
                }
            if (!take)   // one long word: cut it on the last line's terms
                take = n;
        }
        if (take > 255)
            take = 255;
        wcsncpy(line, s, take);
        line[take] = 0;
        wchar_t cut[256];
        const wchar_t* out = line;
        if (TextWidth(dc, line, size, weight) > maxw) {   // the last line (or one long word): cut, same size
            Ellipsize(dc, line, size, weight, maxw, cut, 256);
            out = cut;
        }
        T(dc, x, y + used * lineH, out, size, c, weight);
        s += take;
        used++;
        if (used == lines)
            break;
    }
    return used;
}

// A running line (as the stock shell shows the station and the song): a text wider than w goes round inside
// x..x+w, s_marq px along (TIMER_MARQ moves it while the radio screen is in front); a shorter one stands still.
enum { TIMER_MARQ = 10, kMarqMs = 300, kMarqStep = 16, kMarqGap = 60 };
static int  s_marq;          // how far the running lines have gone
static bool s_marqOn;        // a line of this frame did not fit: keep the timer going

static void Marquee(HDC dc, int x, int y, int w, const wchar_t* s, int size, COLORREF c, int weight)
{
    int tw = TextWidth(dc, s, size, weight);
    if (tw <= w) {
        T(dc, x, y, s, size, c, weight);
        return;
    }
    s_marqOn = true;
    int off = s_marq % (tw + kMarqGap);
    RECT r = { x, y - size * 5 / 4, x + w, y + size / 2 };
    HGDIOBJ of = SelectObject(dc, Font(size, weight));
    SetTextAlign(dc, TA_LEFT | TA_BASELINE);
    SetTextColor(dc, C(c));
    SetBkMode(dc, TRANSPARENT);
    ExtTextOut(dc, x - off, y, ETO_CLIPPED, &r, s, (int)wcslen(s), NULL);
    ExtTextOut(dc, x - off + tw + kMarqGap, y, ETO_CLIPPED, &r, s, (int)wcslen(s), NULL);
    SetTextAlign(dc, TA_LEFT | TA_TOP);
    SelectObject(dc, of);
}

// Rounded rectangle like SVG <rect rx>: fill and 2 px stroke, CLR_INVALID = none.
static void R(HDC dc, int x, int y, int w, int h, int rx, COLORREF fill, COLORREF stroke = CLR_INVALID)
{
    if (w <= 0 || h <= 0)
        return;
    if (rx == 0 && stroke == CLR_INVALID) {
        if (fill != CLR_INVALID)
            Fill(dc, x, y, x + w, y + h, fill);
        return;
    }
    HBRUSH br = fill == CLR_INVALID ? NULL : SolidBrush(C(fill));
    HPEN pen = stroke == CLR_INVALID ? SolidPen(0, 0) : SolidPen(2, C(stroke));
    HGDIOBJ ob = SelectObject(dc, br ? (HGDIOBJ)br : GetStockObject(NULL_BRUSH)), op = SelectObject(dc, pen);
    int e = stroke == CLR_INVALID ? 1 : 0;   // without a pen GDI leaves out the right and bottom edge
    RoundRect(dc, x, y, x + w + e, y + h + e, rx * 2, rx * 2);
    SelectObject(dc, ob);
    SelectObject(dc, op);
}

static void Disc(HDC dc, int cx, int cy, int r, COLORREF fill, COLORREF stroke = CLR_INVALID)
{
    HBRUSH br = fill == CLR_INVALID ? NULL : SolidBrush(C(fill));
    HPEN pen = stroke == CLR_INVALID ? SolidPen(0, 0) : SolidPen(2, C(stroke));
    HGDIOBJ ob = SelectObject(dc, br ? (HGDIOBJ)br : GetStockObject(NULL_BRUSH)), op = SelectObject(dc, pen);
    int e = stroke == CLR_INVALID ? 1 : 0;
    Ellipse(dc, cx - r, cy - r, cx + r + e, cy + r + e);
    SelectObject(dc, ob);
    SelectObject(dc, op);
}

// 2 px dashed line, horizontal or vertical.
static void Dash(HDC dc, int x0, int y0, int x1, int y1, int on, int off, COLORREF c)
{
    bool hor = y0 == y1;
    int len = hor ? x1 - x0 : y1 - y0;
    for (int p = 0; p < len; p += on + off) {
        int q = p + on < len ? p + on : len;
        if (hor)
            Fill(dc, x0 + p, y0 - 1, x0 + q, y0 + 1, c);
        else
            Fill(dc, x0 - 1, y0 + p, x0 + 1, y0 + q, c);
    }
}

// Dashed frame of an empty slot (the corners stay open).
static void DashRect(HDC dc, int x, int y, int w, int h, int rx, COLORREF c)
{
    Dash(dc, x + rx, y + 1, x + w - rx, y + 1, 6, 6, c);
    Dash(dc, x + rx, y + h - 1, x + w - rx, y + h - 1, 6, 6, c);
    Dash(dc, x + 1, y + rx, x + 1, y + h - rx, 6, 6, c);
    Dash(dc, x + w - 1, y + rx, x + w - 1, y + h - rx, 6, 6, c);
}

static void Line(HDC dc, int x0, int y0, int x1, int y1, int width, COLORREF c)
{
    HGDIOBJ op = SelectObject(dc, SolidPen(width, C(c)));
    POINT p[2] = { { x0, y0 }, { x1, y1 } };
    Polyline(dc, p, 2);
    SelectObject(dc, op);
}

// ---- pixels: the pattern, translucency (the back buffer is 32-bit and reachable)

static Pix Px(COLORREF c)
{
    c = C(c);
    return PIX(GetRValue(c), GetGValue(c), GetBValue(c));
}

// Shade index of the orange triangle mosaic (TriFill) at (u, v) from its origin: rows of 64 x 56 triangles.
static int TriIndex(int u, int v, int n)
{
    const int w = 64, h = 56;   // design 0.6: a larger mosaic
    int row = v / h, f = v % h, col = u / (w / 2), d = u % (w / 2);
    bool up = ((row + col) & 1) == 0;
    // this half-cell holds the left half of triangle col and the right half of triangle col - 1
    if (up ? d * h < (w / 2) * (h - f) : d * h < (w / 2) * f) {
        col--;
        up = !up;
    }
    return ((row * 7 + col * 3 + (up ? 1 : 0)) % n + n) % n;
}

// The orange mosaic (#tri-or) inside a rounded rectangle; rx = w / 2 makes a circle.
static void TriFill(int x, int y, int w, int h, int rx)
{
#ifndef UNDER_CE
    GdiFlush();
#endif
    Pix sh[4];
    for (int k = 0; k < 4; k++)
        sh[k] = Px(kTriOr[k]);
    for (int py = y; py < y + h; py++) {
        if (py < s_clip.top || py >= s_clip.bottom)
            continue;
        Pix* row = s_backBits + (479 - py) * 800;
        int dy = py < y + rx ? y + rx - py : py >= y + h - rx ? py - (y + h - rx - 1) : 0;
        for (int px = x; px < x + w; px++) {
            if (px < s_clip.left || px >= s_clip.right)
                continue;
            int dx = px < x + rx ? x + rx - px : px >= x + w - rx ? px - (x + w - rx - 1) : 0;
            if (dx && dy && dx * dx + dy * dy > rx * rx)
                continue;
            row[px] = sh[TriIndex(px - x, py - y, 4)];
        }
    }
}

// Darkens towards the background colour: alpha a0 in the top row to a1 in the bottom one (0..255).
static void Shade(int x, int y, int w, int h, int a0, int a1)
{
#ifndef UNDER_CE
    GdiFlush();
#endif
    COLORREF ink = C(kInk);
    for (int py = y; py < y + h && py < s_clip.bottom; py++) {
        if (py < s_clip.top)
            continue;
        int a = a0 + (a1 - a0) * (py - y) / (h > 1 ? h - 1 : 1);
        int ir = GetRValue(ink) * a, ig = GetGValue(ink) * a, ib = GetBValue(ink) * a;
        Pix* row = s_backBits + (479 - py) * 800;
        for (int px = x > s_clip.left ? x : s_clip.left; px < x + w && px < s_clip.right; px++) {
            unsigned p = row[px];
            int r = (PR(p) * (255 - a) + ir) / 255;
            int g = (PG(p) * (255 - a) + ig) / 255;
            int b = (PB(p) * (255 - a) + ib) / 255;
            row[px] = PIX(r, g, b);
        }
    }
}

// The selected's soft orange (design 0.6): the accent at 16 % over what is drawn there (the mosaic shows through,
// as the mockup's rgba), a rounded rectangle. The background is drawn again under every redraw, so it never piles up.
static void RSoft(int x, int y, int w, int h, int rx)
{
#ifndef UNDER_CE
    GdiFlush();
#endif
    COLORREF o = C(kOrange);
    int orr = GetRValue(o), og = GetGValue(o), ob = GetBValue(o), a = 41;
    for (int py = y; py < y + h; py++) {
        if (py < s_clip.top || py >= s_clip.bottom)
            continue;
        Pix* row = s_backBits + (479 - py) * 800;
        int dy = py < y + rx ? y + rx - py : py >= y + h - rx ? py - (y + h - rx - 1) : 0;
        for (int px = x; px < x + w; px++) {
            if (px < s_clip.left || px >= s_clip.right)
                continue;
            int dx = px < x + rx ? x + rx - px : px >= x + w - rx ? px - (x + w - rx - 1) : 0;
            if (dx && dy && dx * dx + dy * dy > rx * rx)
                continue;
            unsigned p = row[px];
            int r = PR(p), g = PG(p), b = PB(p);
            r += ((orr - r) * a + 128) / 256;
            g += ((og - g) * a + 128) / 256;
            b += ((ob - b) * a + 128) / 256;
            row[px] = PIX(r, g, b);
        }
    }
}

// A tile's or row's plate: plain (fill), selected (the soft orange over the background), pressed (the orange).
static void RSel(HDC dc, int x, int y, int w, int h, int rx, bool sel, bool down, COLORREF fill = kPanel)
{
    if (down)
        R(dc, x, y, w, h, rx, kOrange);
    else if (sel)
        RSoft(x, y, w, h, rx);
    else if (fill != CLR_INVALID)
        R(dc, x, y, w, h, rx, fill);
}

// ---- widgets of the mockups (make_v01_screens.py helpers of the same names)

// Registers a button of the current screen; true while it is pressed.
static bool Zone(int x, int y, int w, int h, Action a, int arg = 0)
{
    if (s_partial) {   // the buttons of the full frame stay, taking the actions drawn now (the presets' arrows: the
        int i = s_zoneIdx++;   // next preset, HU sheet 28 p. 35); another layout - the whole frame (RedrawArea)
        if (ZoneSame(i < kBtnCount ? &s_btns[i].rc : NULL, x, y, w, h)) {
            s_btns[i].action = a;
            s_btns[i].arg = arg;
        } else {
            s_zoneMoved = true;
        }
        return i == s_pressed;
    }
    if (kBtnCount >= kMaxBtns)
        return false;
    Btn& b = s_btns[kBtnCount];
    b.rc.left = x;
    b.rc.top = y;
    b.rc.right = x + w;
    b.rc.bottom = y + h;
    b.action = a;
    b.arg = arg;
    return kBtnCount++ == s_pressed;
}

// Icon in any colour: the skin's picture (as is in the text colour, otherwise tinted) or the built-in drawing.
static void Ico(HDC dc, Icon ic, int x, int y, int size, COLORREF c)
{
    for (int k = 0; k < 2; k++)   // the skin's picture at its first use (once: a bad file is not tried again)
        if (s_iconPath[ic][k]) {
            LoadImg(s_iconPath[ic][k], &s_icons[ic][k]);
            free(s_iconPath[ic][k]);
            s_iconPath[ic][k] = NULL;
        }
    const Img& im = s_icons[ic][0];
    const Img& dn = s_icons[ic][1];
    if (c == kInk && dn.px)
        DrawImg(dn, x, y, size, size);   // the skin's own pressed picture, if it gives one
    else if (im.px)
        DrawImg(im, x, y, size, size, c == kText ? CLR_INVALID : c);
    else
        PaintIcon(dc, ic, x, y, size, c);
}

static void Sep(HDC dc, int x, int y, int w)   // design 0.6: 1 px
{
    Fill(dc, x, y, x + w, y + 1, kLine);
}

// A card (design 0.6): a surface a shade lighter than the background, no border.
static void Panel(HDC dc, int x, int y, int w, int h, int rx = 18)
{
    R(dc, x, y, w, h, rx, kPanel);
}

// Button states: plain, selected (source, tab), pressed / active, unavailable, filled, the dialog's main action.
// Design 0.6: pills without borders; selected = the soft orange with orange text, pressed and the main action =
// the full orange.
enum { B_N, B_SEL, B_ACT, B_DIS, B_FILL, B_PRI };

static void Button(HDC dc, int x, int y, int w, int h, const wchar_t* label, int state, Icon ic = IC_NONE,
                   int size = 18, int isz = 0, Action a = A_NONE, int arg = 0)
{
    if (Zone(x, y, w, h, a, arg))
        state = B_ACT;
    int rx = h / 2 < 24 ? h / 2 : 24;
    COLORREF fg;
    if (state == B_ACT || state == B_PRI) {
        R(dc, x, y, w, h, rx, kOrange);
        fg = kInk;
    } else if (state == B_SEL) {
        RSoft(x, y, w, h, rx);
        fg = kOrange;
    } else if (state == B_DIS) {
        R(dc, x, y, w, h, rx, kPanel);
        fg = kOff;
    } else {
        R(dc, x, y, w, h, rx, kPanel2);
        fg = label && state == B_N ? kMuted : kText;
    }
    if (ic != IC_NONE && label) {
        if (!isz)
            isz = h - 20 < 32 ? h - 20 : 32;
        int tw = TextWidth(dc, label, size, 700);
        int x0 = x + (w - isz - 10 - tw) / 2;
        Ico(dc, ic, x0, y + (h - isz) / 2, isz, fg);
        T(dc, x0 + isz + 10, y + h / 2 + size * 36 / 100, label, size, fg, 700);
    } else if (ic != IC_NONE) {
        if (!isz)
            isz = h - 14 < 44 ? h - 14 : 44;
        Ico(dc, ic, x + (w - isz) / 2, y + (h - isz) / 2, isz, fg);
    } else if (label) {
        T(dc, x + w / 2, y + h / 2 + size * 36 / 100, label, size, fg, 700, AL_MID, w - 12);
    }
}

// A bare icon (docks, transport): its button reaches 8 px around it, pressed = an orange circle.
static void IconBtn(HDC dc, Icon ic, int x, int y, int size, COLORREF c, Action a = A_NONE, int arg = 0)
{
    bool down = Zone(x - 8, y - 8, size + 16, size + 16, a, arg);
    if (down)
        Disc(dc, x + size / 2, y + size / 2, size / 2 + 8, kOrange);
    Ico(dc, ic, x, y, size, down ? kInk : c);
}

// A switch (design 0.6: 52 x 30) centred in the 72 x 36 box at x, y the screens place it by; the tap area around it.
static void Toggle(HDC dc, int x, int y, int id)
{
    bool on = s_on[id] != Zone(x - 10, y - 10, 92, 56, A_TOGGLE, id);   // pressed shows the new state
    x += 10;
    y += 3;
    R(dc, x, y, 52, 30, 15, on ? kOrange : kPanel2);
    Disc(dc, on ? x + 37 : x + 15, y + 15, 11, on ? kInk : kDim);
}

// Segmented choice s_seg[id] among n options.
static void Seg(HDC dc, int x, int y, int w, int h, const wchar_t* const* opts, int n, int id, int size = 17)
{
    R(dc, x, y, w, h, h / 2, kPanel2);
    int cw = (w - 4) / n;
    for (int i = 0; i < n; i++) {
        int cx = x + 2 + i * cw;
        bool on = Zone(cx, y, cw, h, A_SEG, id * 16 + i) || s_seg[id] == i;
        if (on)
            R(dc, cx, y + 2, cw, h - 4, (h - 4) / 2, kOrange);
        T(dc, cx + cw / 2, y + h / 2 + size * 36 / 100, opts[i], size, on ? kInk : kMuted, 700, AL_MID, cw - 6);
    }
}

static void VSlider(HDC dc, int x, int y, int h, int pct, const wchar_t* label, const wchar_t* val)
{
    int k = h * pct / 100;
    R(dc, x - 4, y, 8, h, 4, kLine);
    R(dc, x - 4, y + h - k, 8, k, 4, kOrange);
    Disc(dc, x, y + h - k, 13, kText);
    T(dc, x, y - 14, val, 16, kText, 700, AL_MID);
    T(dc, x, y + h + 28, label, 14, kMuted, 600, AL_MID);
}

static void Stepper(HDC dc, int x, int y, int w, const wchar_t* value, int h = 48, int size = 20,
                    Action a = A_NONE)   // a: the left arrow sends arg -1, the right one +1
{
    R(dc, x, y, w, h, h / 2, kPanel2);
    bool l = Zone(x, y, 48, h, a, -1), r = Zone(x + w - 48, y, 48, h, a, 1);
    if (l)
        R(dc, x + 2, y + 2, 44, h - 4, (h - 4) / 2, kOrange);
    if (r)
        R(dc, x + w - 46, y + 2, 44, h - 4, (h - 4) / 2, kOrange);
    Ico(dc, IC_BACK, x + 6, y + (h - 32) / 2, 32, l ? kInk : kText);
    Ico(dc, IC_NEXT, x + w - 38, y + (h - 32) / 2, 32, r ? kInk : kText);
    T(dc, x + w / 2, y + h / 2 + size * 36 / 100, value, size, kText, 700, AL_MID, w - 96);
}

enum { kRowH = 52, kRowStep = 58 };   // every list's row: its height, the next one's offset (ListPages)

// An empty list's lines, the same everywhere: what is (not) there, then what to do (hint, NULL none).
static void EmptyList(HDC dc, int x, int y, int w, const wchar_t* text, const wchar_t* hint = NULL,
                      COLORREF c = CLR_INVALID)
{
    T(dc, x, y, text, 18, c != CLR_INVALID ? c : kMuted, 400, AL_START, w);
    if (hint)
        T(dc, x, y + 30, hint, 16, kDim, 400, AL_START, w);
}

enum { L_N, L_CUR, L_DIM };

// List row, one button: optional number, icon, second line and right-hand text; current = the soft orange with
// orange text, pressed = the full orange. icColor: the icon's own colour (folders in the browsers), CLR_INVALID =
// muted like the text.
static void ListRow(HDC dc, int x, int y, int w, int h, const wchar_t* text, const wchar_t* right, int state,
                    Icon ic = IC_NONE, const wchar_t* sub = NULL, const wchar_t* num = NULL, Action a = A_NONE,
                    int arg = 0, COLORREF icColor = CLR_INVALID)
{
    bool down = Zone(x, y, w, h, a, arg);
    bool cur = state == L_CUR && !down;
    COLORREF fg = down ? kInk : cur ? kOrange : state == L_DIM ? kOff : kText,
             mg = down ? kInk : cur ? kOrange : kMuted;
    RSel(dc, x, y, w, h, 14, cur, down);
    int tx = x + 16;
    if (num) {
        T(dc, tx, y + h / 2 + 6, num, 16, down ? kInk : cur ? kOrange : kDim, 700);
        tx += 40;
    }
    if (ic != IC_NONE) {
        Ico(dc, ic, tx, y + (h - 30) / 2, 30, down ? kInk : cur ? kOrange : icColor != CLR_INVALID ? icColor : kMuted);
        tx += 44;
    }
    int maxw = x + w - (right ? TextWidth(dc, right, 16, 600) + 28 : 16) - tx;
    if (sub) {
        T(dc, tx, y + h / 2 - 3, text, 19, fg, 700, AL_START, maxw);
        T(dc, tx, y + h / 2 + 19, sub, 15, mg, 400, AL_START, maxw);
    } else {
        T(dc, tx, y + h / 2 + 7, text, 19, fg, 700, AL_START, maxw);
    }
    if (right)
        T(dc, x + w - 16, y + h / 2 + 6, right, 16, mg, 600, AL_END);
}

// A volume's name for people ("\SDMemory\" -> "SD-карта"); *rest: the path after it. NULL: not a volume we know.
static const wchar_t* VolName(const wchar_t* path, const wchar_t** rest)
{
    static const wchar_t* vols[6][2] = { { L"\\SDMemory\\", L"SD-карта" }, { L"\\USBDisk\\", L"USB-диск" },
                                         { L"\\USBDevice\\", L"USB-диск" }, { L"\\SDMMC\\", L"Встроенная карта" },
                                         { L"\\StorageFlash\\", L"Память ГУ" }, { L"\\ResidentFlash\\", L"ResidentFlash" } };
    for (int i = 0; i < 6; i++) {
        size_t l = wcslen(vols[i][0]);
        if (!_wcsnicmp(path, vols[i][0], l)) {
            *rest = path + l;
            return vols[i][1];
        }
    }
    *rest = path;
    return NULL;
}

// The browsers' path line (design 0.6, y 108..152): "‹" one level up (inside a folder), then the volume and the
// folders "SD-карта › Музыка › Rock" (the last white, the first ones dropped when it is too long; a tap on it - the
// volumes: rootArg), up to `right`.
static void DirPath(HDC dc, const wchar_t* dir, Action up, int upArg, int rootArg, int right)
{
    int x = 24;
    if (dir[0]) {
        IconBtn(dc, IC_BACK, 16, 116, 28, kMuted, up, upArg);
        x = 60;
    }
    wchar_t part[8][MAX_PATH];   // the crumbs, the volume first
    int n = 0;
    const wchar_t* rest;
    const wchar_t* vol = VolName(dir, &rest);
    if (!dir[0]) {
        wcscpy(part[n++], L"Носители");
    } else {
        if (vol) {
            wcscpy(part[n++], vol);
        } else {   // "C:\" (the PC)
            wcsncpy(part[n], dir, 2);
            part[n++][2] = 0;
            rest = dir[2] == L'\\' ? dir + 3 : dir + 2;
        }
        while (*rest && n < 8) {
            const wchar_t* e = wcschr(rest, L'\\');
            int l = e ? (int)(e - rest) : (int)wcslen(rest);
            if (n == 7) {   // too deep: the first ones go, the last one stays
                memmove(part[1], part[2], 5 * sizeof(part[0]));
                n = 6;
            }
            wcsncpy(part[n], rest, l);
            part[n++][l] = 0;
            rest = e ? e + 1 : rest + l;
        }
    }
    int first = 0, sep = TextWidth(dc, L"  ›  ", 17, 700), w = 0;   // as many from the end as fit
    for (int i = n - 1; i >= 0; i--) {
        int pw = TextWidth(dc, part[i], 17, 700) + (i < n - 1 ? sep : 0);
        if (w + pw > right - x && i < n - 1) {
            first = i + 1;
            break;
        }
        w += pw;
    }
    if (dir[0])
        Zone(x, 108, w < right - x ? w : right - x, 44, up, rootArg);
    for (int i = first; i < n; i++) {
        bool last = i == n - 1;
        x = T(dc, x, 137, part[i], 17, last ? kText : kMuted, 700, AL_START, right - x);
        if (!last)
            x = T(dc, x, 137, L"  ›  ", 17, kDim, 700);
    }
}

// The browsers "volumes -> folders -> files" (music, video), design 0.6: the entries in columns of `rows` rows of
// 52 px from y 160 (folders end in '\\', orange icon; volumes by their names), a file's second line its type and size
// ("MP3 · 8,5 МБ"); the one playing (curName, NULL none) soft orange; nothing there - its line.
static void DirEntries(HDC dc, const wchar_t* dir, wchar_t* const* names, int count, int page, int rows, int cols,
                       Action pick, Icon ic, const wchar_t* curName, const wchar_t* none)
{
    int per = rows * cols, first = page * per, cw = (768 - (cols - 1) * 12) / cols;
    wchar_t s[MAX_PATH + 1], sub[48], path[MAX_PATH];
    for (int i = first, cell = 0; i < count && i < first + per; i++, cell++) {
        const wchar_t* n = names[i];
        bool isDir = n[wcslen(n) - 1] == L'\\';
        const wchar_t* rest;
        const wchar_t* vol = dir[0] ? NULL : VolName(n, &rest);
        wcsncpy(s, vol ? vol : n, MAX_PATH);
        s[MAX_PATH] = 0;
        if (isDir && s[0] && s[wcslen(s) - 1] == L'\\')
            s[wcslen(s) - 1] = 0;
        sub[0] = 0;
        WIN32_FILE_ATTRIBUTE_DATA fa;
        _snwprintf(path, MAX_PATH, L"%s%s", dir, n);
        path[MAX_PATH - 1] = 0;
        if (!isDir && GetFileAttributesEx(path, GetFileExInfoStandard, &fa)) {
            const wchar_t* dot = wcsrchr(n, L'.');
            wchar_t ext[8] = L"";
            if (dot) {
                wcsncpy(ext, dot + 1, 7);
                ext[7] = 0;
                CharUpperBuff(ext, (DWORD)wcslen(ext));
            }
            double kb = (fa.nFileSizeHigh * 4294967296.0 + fa.nFileSizeLow) / 1024;
            if (kb >= 1024 * 1024)
                _snwprintf(sub, 48, L"%s · %.1f ГБ", ext, kb / 1024 / 1024);
            else if (kb >= 1024)
                _snwprintf(sub, 48, L"%s · %.1f МБ", ext, kb / 1024);
            else
                _snwprintf(sub, 48, L"%s · %d КБ", ext, (int)kb + 1);
            sub[47] = 0;
            for (wchar_t* c = sub; *c; c++)   // "8,5 МБ"
                if (*c == L'.' && c[1] >= L'0' && c[1] <= L'9')
                    *c = L',';
        }
        bool cur = !isDir && curName && !_wcsicmp(n, curName);   // the one playing: "▶ играет" (design 0.6)
        ListRow(dc, 16 + cell / rows * (cw + 12), 160 + cell % rows * kRowStep, cw, kRowH, s, cur ? L"▶ играет" : NULL,
                cur ? L_CUR : L_N, isDir ? IC_FOLDER : ic, sub[0] ? sub : NULL,
                NULL, pick, i, isDir ? kOrange : CLR_INVALID);   // folders orange, files muted
    }
    if (!count)
        EmptyList(dc, 40, 190, 720, dir[0] ? none : L"Нет носителей");
}

// A list that scrolls by px - the internet radio's stations, the licenses, the contacts, the calls: the finger moves
// the one under the press (WndProc), its arrows (A_SCROLL, arg -1 / 1) by step. One per frame: s_scroll is the one
// drawn last (a window's over its screen's). Between ScrollBegin and ScrollEnd the rows are drawn at
// view.top - pos + their offset; ScrollEnd cuts them and their buttons at the view (a row half above it must not take
// the top bar's taps) and draws the arrows with the bar between them.
struct Scroll {
    int  pos, content, step;   // scrolled by / the height of all (the screen sets it) / an arrow's step
    RECT view, area;           // the rows' window / it with the arrows (redrawn while the finger moves it)
};
static Scroll* s_scroll;

static int ScrollMax(const Scroll& s)
{
    int v = s.view.bottom - s.view.top;
    return s.content > v ? s.content - v : 0;
}

static void ScrollTo(Scroll& s, int px)
{
    int m = ScrollMax(s);
    s.pos = px < 0 ? 0 : px > m ? m : px;
}

static int  s_scrollBtn0, s_scrollSaved;   // the first button of the rows, the DC's state
static RECT s_scrollClip;                  // s_clip outside the list (the pictures cut themselves: DrawImg)

static void ScrollBegin(HDC dc, Scroll& s, int l, int t, int r, int b)
{
    SetRect(&s.view, l, t, r, b);
    s.area = s.view;
    s_scroll = &s;
    ScrollTo(s, s.pos);
    s_scrollBtn0 = kBtnCount;
    s_scrollSaved = SaveDC(dc);
    IntersectClipRect(dc, l, t, r, b);
    s_scrollClip = s_clip;
    IntersectRect(&s_clip, &s_clip, &s.view);
}

// The arrows at x, y (w wide, ah high each) and the bar between them, over h; only when it does not fit.
static void ScrollEnd(HDC dc, Scroll& s, int x, int y, int w, int h, int ah)
{
    RestoreDC(dc, s_scrollSaved);
    s_clip = s_scrollClip;
    for (int k = s_scrollBtn0; k < kBtnCount; k++) {
        RECT& rc = s_btns[k].rc;
        rc.top = rc.top < s.view.top ? s.view.top : rc.top;
        rc.bottom = rc.bottom > s.view.bottom ? s.view.bottom : rc.bottom;
        if (rc.bottom < rc.top)
            rc.bottom = rc.top;
    }
    RECT a = { x, y, x + w, y + h };
    UnionRect(&s.area, &s.view, &a);
    InflateRect(&s.area, 4, 4);
    int m = ScrollMax(s);
    if (m <= 0)
        return;
    bool big = ah >= 90;
    Button(dc, x, y, w, ah, NULL, s.pos > 0 ? B_N : B_DIS, IC_UP, big ? 18 : 14, big ? 30 : 22, A_SCROLL, -1);
    int ty0 = y + ah + 8, track = h - 2 * ah - 16, view = s.view.bottom - s.view.top;
    int th = track * view / s.content, ty = ty0 + (track - th) * s.pos / m;
    th = th < 16 ? 16 : th;
    ty = ty + th > ty0 + track ? ty0 + track - th : ty;
    R(dc, x + w / 2 - 3, ty0, 6, track, 3, kLine);
    R(dc, x + w / 2 - 3, ty, 6, th, 3, kMuted);
    Button(dc, x, y + h - ah, w, ah, NULL, s.pos < m ? B_N : B_DIS, IC_DOWN, big ? 18 : 14, big ? 30 : 22, A_SCROLL,
           1);
}

static void Dock(HDC dc)   // from y 408, as the bottom bar (NavBar): nothing jumps between them
{
    Fill(dc, 0, 408, 800, 480, kInk);
}

static void Now(wchar_t* s, int n)   // the clock: 24 h, or 12 h ("General -> Time format", time.12h)
{
    SYSTEMTIME t;
    GetLocalTime(&t);
    if (s_seg[G_TIME_FMT] == 0)
        _snwprintf(s, n, L"%d:%02d", t.wHour % 12 ? t.wHour % 12 : 12, t.wMinute);
    else
        _snwprintf(s, n, L"%02d:%02d", t.wHour, t.wMinute);
}

// Top bar of every screen: back button (when there is a screen to go back to, or a given action) and title,
// the last action's result, then the status marks (GPS, Bluetooth, USB), the volume (MCU) and the clock.
// A main screen may put one action button of its own before the marks (act). Demo: the Bluetooth mark opens
// the incoming call window, the clock the volume window. VestaHome is closed from the settings' diagnostics.
static bool OutsideTemp(int* t);
static bool VolGet(int* vol, int* mute, int* src);
static bool CamChip(wchar_t* s, int n);
static bool PlugCanBack();   // (vh_plugins.inc: an add-on screen under the one shown)
static bool CamOver();
static void SpeedSign(HDC dc, int cx, int cy, int r, int limit);
static int  CamLimit();
static int  CarKmh();
static bool HvacChip(wchar_t* s, int n);
static DWORD s_hvacShowTick;   // a climate change came: its plate shows in the top bar for 4 s

static bool GpsHasFix();
static int  s_mediaMask;   // 1: an SD card in its slot, 2: a USB disk (MediaCheck, once a second)

static int MediaMask()
{
    int m = 0;
    DWORD a = GetFileAttributes(L"\\SDMemory");
    if (a != 0xFFFFFFFF && (a & FILE_ATTRIBUTE_DIRECTORY))
        m |= 1;
    static const wchar_t* usb[3] = { L"\\USBDisk", L"\\USBDevice", L"\\USB Disk" };
    for (int i = 0; i < 3; i++) {
        a = GetFileAttributes(usb[i]);
        if (a != 0xFFFFFFFF && (a & FILE_ATTRIBUTE_DIRECTORY))
            m |= 2;
    }
    return m;
}

// The removable volume a path is on: 1 the SD card, 2 a USB disk (MediaMask's bits), 0 none (the built-in card).
static int VolumeBit(const wchar_t* path)
{
    if (!_wcsnicmp(path, L"\\SDMemory\\", 10))
        return 1;
    return !_wcsnicmp(path, L"\\USBDisk\\", 9) || !_wcsnicmp(path, L"\\USBDevice\\", 11) ||
           !_wcsnicmp(path, L"\\USB Disk\\", 10) ? 2 : 0;
}

// The status marks of the top bar, right to left from `end`: the USB disk and the SD card (only when there),
// Wi-Fi (orange when connected, dim when not; Settings -> Wi-Fi), Bluetooth (orange with a phone connected, dim
// without; none while switched off - the user 2026-11-05), GPS (orange with a fix). The ones that would reach left of minLeft are left out. Returns the left edge.
struct BtState;
static bool BtGet(BtState* st, LONG* seq);
static int BtPowerState();   // (vh_screens.inc)
static Icon PlayingIcon(Screen* to);   // what plays (vh_tasks.inc): its icon and screen, IC_NONE nothing

static bool ModemShown(bool* online);   // (vh_gps.inc)

static int StatusMarks(HDC dc, int end, int minLeft)
{
    BtState bt;
    bool btOn = BtGet(&bt, NULL) && (bt.hf == 2 || bt.av >= 1);   // a phone connected (proxy Bluetooth)
    WifiInfo w;
    WifiGet(&w);
    struct Mark { Icon ic; COLORREF c; bool shown; Action a; int arg; };
    Screen playTo = S_NONE;
    Icon play = PlayingIcon(&playTo);
    ObdInfo obd;   // the engine's trouble codes (OBD): a warning, a tap - their screen
    ObdGet(&obd);
    bool dtc = obd.state == OBD_ON && (obd.dtcs > 0 || obd.mil == 1);
    bool mdmOn, mdmIn = ModemShown(&mdmOn);   // the USB modem in (the user 2026-11-05): grey, online - the accent
    Mark m[8] = {
        { IC_USB, kOrange, (s_mediaMask & 2) != 0, A_NONE, 0 },
        { IC_SD, kOrange, (s_mediaMask & 1) != 0, A_NONE, 0 },
        { IC_WIFI, w.state == WIFI_CONNECTED ? kOrange : kDim, w.state >= WIFI_DISCONNECTED, A_NAV, S_SET_WIFI },
        { IC_BT, btOn ? kOrange : kDim, BtPowerState() != BT_POW_OFF, A_NAV, S_PHONE },
        { IC_SIGNAL, mdmOn ? kOrange : kDim, mdmIn, A_NAV, S_SET_NET },
        { IC_NAV, GpsHasFix() ? kOrange : kDim, true, A_NONE, 0 },
        { play, kOrange, play != IC_NONE, A_NAV, playTo },
        { IC_WARN, RGB(255, 204, 0), dtc, A_SCREEN, S_OBD_DTC } };   // the trouble codes: always yellow (the user)
    int x = end;
    for (int i = 0; i < 8; i++) {
        if (!m[i].shown)
            continue;
        if (x - 34 < minLeft)   // design 0.6: icons of 20, 14 apart
            break;
        x -= 34;
        bool d = m[i].a != A_NONE && Zone(x - 4, 4, 34, 48, m[i].a, m[i].arg);
        if (d)
            R(dc, x - 4, 4, 34, 48, 10, kOrange);
        Ico(dc, m[i].ic, x + 3, 18, 20, d ? kInk : m[i].c);
    }
    return x;
}

static Action s_frameBack = A_NONE;   // this frame's "back" (Frame sets it, BackButton draws it at the bottom)

// The route's manoeuvre in the top bar (ideas 0.7, item 2): the sign in the accent's disc, the distance, the street and
// the arrival as far as there is room; a tap brings the navigator up. Its data: vh_plugins.inc.
static bool NavChipGet(VNav* n);
enum { kChipMin = 120, kChipMax = 320 };

static void NavChip(HDC dc, int x, int right, const VNav& n)
{
    int w = right - x > kChipMax ? kChipMax : right - x;
    bool d = Zone(x, 8, w, 40, A_NAVCHIP);
    R(dc, x, 8, w, 40, 20, d ? kPanel2 : kPanel);
    Disc(dc, x + 20, 28, 16, kOrange);
    RECT sign = { x + 6, 14, x + 34, 42 };
    VNavDrawTurn(dc, sign, n.turn, C(kInk));
    int end = T(dc, x + 44, 34, n.dist, 15, kText, 700, AL_START, w - 52);
    wchar_t s[80];
    _snwprintf(s, 80, L"%s · %s", n.street, n.eta);
    s[79] = 0;
    if (x + w - 14 - (end + 10) >= 40)
        T(dc, end + 10, 33, s, 13, kMuted, 400, AL_START, x + w - 14 - (end + 10));
}

static void Frame(HDC dc, const wchar_t* title, Action back = A_BACK, Icon act = IC_NONE, Action a = A_NONE,
                  bool sel = false, Icon act2 = IC_NONE, Action a2 = A_NONE, int arg2 = 0)
{
    int vol, mute, src, marksEnd;   // design 0.6: no line under the bar
    wchar_t s[40];
    {   // the volume (MCU, through the proxy): a speaker and the level, "–" before the first value; opens its window
        bool have = VolGet(&vol, &mute, &src);
        if (!have)
            wcscpy(s, L"–");
        else if (mute)
            wcscpy(s, L"×");
        else
            _snwprintf(s, 8, L"%d", vol);
        // its width that of two digits whatever it shows (the user 2026-11-08: 9 -> 10 moved the marks left of it)
        int w = TextWidth(dc, L"00", 18, 600), ws = TextWidth(dc, s, 18, 600);
        if (ws > w)
            w = ws;
        int x0 = 700 - w - 30;
        bool dv = Zone(x0 - 4, 4, w + 38, 48, A_SCREEN, S_VOLUME);
        if (dv)
            R(dc, x0 - 4, 4, w + 38, 48, 10, kOrange);
        COLORREF c = dv ? kInk : have && !mute ? kMuted : kDim;
        Ico(dc, IC_VOL, x0 + 4, 18, 20, c);
        T(dc, 700, 36, s, 18, c, 600, AL_END);
        marksEnd = x0 - 8;
    }
    int marksLeft = StatusMarks(dc, marksEnd, act != IC_NONE ? 540 : 0);
    Now(s, 8);
    T(dc, 776, 36, s, 22, kText, 700, AL_END);   // the clock (design 0.6: 22 bold): no action (the volume opens
                                                 // from the speaker)
    int x = 20, right = act != IC_NONE ? 470 : 530;
    if (right > marksLeft - 12)
        right = marksLeft - 12;
    if (act != IC_NONE)
        Button(dc, 478, 6, 56, 44, NULL, sel ? B_SEL : B_N, act, 18, 28, a);
    if (act2 != IC_NONE) {   // a second one to its left
        Button(dc, 418, 6, 56, 44, NULL, B_N, act2, 18, 28, a2, arg2);
        right = 410;
    }
    if (CamChip(s, 40)) {   // a speed camera ahead, on every screen: its sign, red when speeding
        int cx = right - 156;
        bool over = CamOver();
        R(dc, cx, 8, 152, 40, 10, over ? kStop : kOrange);
        SpeedSign(dc, cx + 22, 28, 17, CamLimit());
        T(dc, cx + 96, 34, s, 14, over ? RGB(255, 255, 255) : kInk, 700, AL_MID, 104);
        right = cx - 8;
    } else if (s_hvacShowTick && GetTickCount() - s_hvacShowTick < 4000 && HvacChip(s, 40)) {
        int cx = right - 196;   // the climate was changed: what it is now, like the stock shell's tip window
        R(dc, cx, 8, 192, 40, 10, kPanel2);   // no border (design 0.6)
        T(dc, cx + 96, 34, s, 14, kText, 700, AL_MID, 184);
        right = cx - 8;
    }
    if (back != A_NONE && (back != A_BACK || s_histLen > 0 || PlugCanBack())) {
        if (s_screen == S_KEYS) {   // the keyboard fills the bottom: its "back" stays up here
            bool d = Zone(0, 0, 64, 56, back);
            R(dc, 8, 8, 48, 40, 20, d ? kOrange : kPanel);
            Ico(dc, IC_BACK, 14, 10, 36, d ? kInk : kText);
            x = 72;
        } else {
            s_frameBack = back;   // drawn at the bottom left over the screen (BackButton, the user 2026-10-24)
        }
    }
    VNav nav;
    bool chip = NavChipGet(&nav);
    int end = title[0] ? T(dc, x + 4, 35, title, 20, kText, 700, AL_START, s_status[0] || chip ? 300 : right - x) : x;
    if (chip && right - end - 12 >= kChipMin)
        NavChip(dc, end + 12, right, nav);
    else if (s_status[0])
        T(dc, end + 16, 36, s_status, 15, kMuted, 400, AL_START, right - end - 16);
}

// Sections of the bottom bar; each opens its main screen (media: the source shown last).
struct NavItem { Icon ic; const wchar_t* label; Screen to; };
static const NavItem kNav[6] = { { IC_HOME, L"Главная", S_HOME }, { IC_RADIO, L"Радио", S_RADIO },
                                 { IC_MUSIC, L"Медиа", S_AUDIO }, { IC_PHONE, L"Телефон", S_PHONE },
                                 { IC_APPS, L"Приложения", S_APPS }, { IC_GEAR, L"Настройки", S_SETTINGS } };
static Screen s_media = S_AUDIO;

// Add-ons' buttons (vh_plugins.inc, up to two): sections 6 and 7, placed before "Settings"; the places share the bar.
static int PlugNavCount();
static Icon PlugNavIcon(int k);
static const wchar_t* PlugNavLabel(int k);
static int PlugNavSection();

static int NavItems() { return 6 + PlugNavCount(); }   // the sections (the day / night key not counted)
static int NavPos(int i) { int a = PlugNavCount(); return i < 5 ? i : i == 5 ? 5 + a : 5 + (i - 6); }
static Icon NavIc(int i) { return i < 6 ? kNav[i].ic : PlugNavIcon(i - 6); }
static const wchar_t* NavLabel(int i) { return i < 6 ? kNav[i].label : PlugNavLabel(i - 6); }

// Section of a main screen (index in kNav); -1: not a main screen, it has no bottom bar (its own dock instead).
static int Section(Screen s)
{
    switch (s) {
    case S_HOME:
        return 0;
    case S_RADIO:
        return 1;
    case S_AUDIO: case S_NETRADIO: case S_AUDIO_BT: case S_AUX: case S_VIDEO: case S_PHOTO:
        return 2;
    case S_PHONE: case S_CONTACTS: case S_FAVS: case S_CALLS:   // (the pills' screens: the bottom bar, as media's)
        return 3;
    case S_APPS:
        return 4;
    case S_PLUGIN:
        return PlugNavSection();   // an add-on's screen with its own button: its section, else none
    case S_SETTINGS: case S_SET_SOUND: case S_SET_BALANCE: case S_SET_SCREEN: case S_SET_RADIO: case S_SET_BT:
    case S_SET_WIFI: case S_SET_VIDEO: case S_SET_PHOTO: case S_INFO: case S_SET_CAR: case S_SET_VOLUME:
    case S_SET_OBD: case S_BT_PAIRED: case S_BT_PAIRNEW: case S_SET_THEME: case S_WIFI_SAVED: case S_SET_PLUGINS:
    case S_SET_NET:
        return 5;
    default:
        return -1;
    }
}

// The section whose bottom bar a screen shows: the main screens only - their sub-pages (the balance, the paired
// phones, the theme, the music list...) have none, their "back" sits there instead (the user 2026-10-24).
static int NavSection(Screen s)
{
    switch (s) {
    case S_SET_BALANCE: case S_SET_VOLUME: case S_SET_OBD: case S_BT_PAIRED: case S_BT_PAIRNEW: case S_SET_THEME:
    case S_AUDIO_LIST: case S_WIFI_SAVED: case S_SET_PLUGINS:
        return -1;
    default:
        return Section(s);
    }
}

// The "back" of a nested screen (Frame's), at the bottom left where the bar's home is: over the screen, its button
// the last one (it wins a tap over what lies under it).
static void BackButton(HDC dc)
{
    if (s_frameBack == A_NONE)
        return;
    bool d = Zone(8, 412, 72, 68, s_frameBack);
    Disc(dc, 44, 446, 26, d ? kOrange : kPanel2);
    Ico(dc, IC_RETURN, 30, 432, 28, d ? kInk : kText);   // a turning-back arrow (the user 2026-10-24)
}

// The bar's change of section, animated (the user 2026-10-23): the pill of the section left shrinks with its name to
// a circle, its icon goes plain; the circle slides to the new section (ease in and out; 150 ms a place - Главная to
// Настройки under a second), its icon turns orange and the circle opens into the pill with the name. The time counts
// from the first frame of the new screen (its own drawing must not eat the start).
enum { TIMER_NAVANIM = 21, kNavShrinkMs = 120, kNavStepMs = 150, kNavFrameMs = 25 };   // (HU 2026-10-24: a bit faster)
static int   s_navFrom = -1, s_navTo = -1;
static DWORD s_navStart;   // 0: not started yet

static int NavCxPos(int pos)   // the centre of a place: the sections, then day / night
{
    return 56 + pos * 688 / NavItems();
}

static int NavCx(int i)
{
    return NavCxPos(NavPos(i));
}

static int NavPillW(HDC dc, int i)   // with add-ons' buttons the places are narrow: the pill without its name
{
    int w = 18 + 24 + 8 + TextWidth(dc, NavLabel(i), 15, 700) + 18;
    return NavItems() > 6 ? 64 : w;   // (a name would run into the neighbours' icons)
}

static int NavPillX(int cx, int w)
{
    int x = cx - w / 2;
    return x < 16 ? 16 : x + w > 784 ? 784 - w : x;
}

static int NavAnimMs()
{
    return 2 * kNavShrinkMs + kNavStepMs * abs(NavPos(s_navTo) - NavPos(s_navFrom));
}

static void NavAnimStart(int from, int to)
{
    if (from < 0 || to < 0 || from > 5 || to > 5 || from == to)
        return;
    s_navFrom = from;
    s_navTo = to;
    s_navStart = 0;
    SetTimer(s_wnd, TIMER_NAVANIM, kNavFrameMs, NULL);
}

static double Ease(double p)   // in and out
{
    p = p < 0 ? 0 : p > 1 ? 1 : p;
    return p < 0.5 ? 2 * p * p : 1 - 2 * (1 - p) * (1 - p);
}

// The pill of section i at width w (44: a circle) with its orange icon and as much of its name as fits.
static void NavPill(HDC dc, int i, int w)
{
    int cx = NavCx(i), full = NavPillW(dc, i), x = NavPillX(cx, w), fx = NavPillX(cx, full);
    RSoft(x, 426, w, 44, 22);
    int ix = (cx - 12) + (fx + 18 - (cx - 12)) * (w - 44) / (full - 44 > 0 ? full - 44 : 1);   // centred -> its place
    Ico(dc, NavIc(i), ix, 436, 24, kOrange);
    if (w > 64) {   // the name, cut at the pill's right edge
        int saved = SaveDC(dc);
        IntersectClipRect(dc, x, 426, x + w - 12, 470);
        T(dc, ix + 32, 454, NavLabel(i), 15, kOrange, 700);
        RestoreDC(dc, saved);
    }
}

// Bottom bar of the main screens (design 0.6): the six sections and day/night as icons without labels in seven fixed
// places (nothing moves when the section changes), the current section a soft orange pill with its name, grown around
// its place (it stays clear of the neighbours' icons). 64 px high (416..480) on the background colour; the main screens
// keep their content above y 400.
static void NavBar(HDC dc, int active)
{
    Dock(dc);
    int animIc = -1;   // the icon the animation draws itself
    if (s_navFrom >= 0 && s_navTo == active) {
        if (!s_navStart)
            s_navStart = GetTickCount();
        int el = (int)(GetTickCount() - s_navStart), move = NavAnimMs() - 2 * kNavShrinkMs;
        if (el < kNavShrinkMs) {   // the pill left shrinks to a circle
            int full = NavPillW(dc, s_navFrom);
            NavPill(dc, s_navFrom, full - (int)((full - 44) * Ease((double)el / kNavShrinkMs)));
            animIc = s_navFrom;
        } else if (el < kNavShrinkMs + move) {   // the circle slides
            int cx = NavCx(s_navFrom) + (int)((NavCx(s_navTo) - NavCx(s_navFrom)) * Ease((double)(el - kNavShrinkMs) / move));
            RSoft(cx - 22, 426, 44, 44, 22);
        } else if (el < NavAnimMs()) {   // it opens into the new pill
            int full = NavPillW(dc, s_navTo);
            NavPill(dc, s_navTo, 44 + (int)((full - 44) * Ease((double)(el - kNavShrinkMs - move) / kNavShrinkMs)));
            animIc = s_navTo;
        } else {
            s_navFrom = -1;
        }
    } else {
        s_navFrom = -1;
    }
    bool anim = s_navFrom >= 0;
    int items = NavItems(), sp = 688 / items;
    for (int i = 0; i <= items; i++) {   // the sections, then day / night (i == items)
        bool night = i == items;
        int cx = night ? NavCxPos(items) : NavCx(i);
        Icon ic = !night ? NavIc(i) : s_night ? IC_SUN : IC_MOON;
        Screen to = i == 2 ? s_media : i < 6 ? kNav[i].to : night ? S_NONE : S_PLUGIN;
        bool on = i == active && !anim;
        Action a = night ? A_NIGHT : i >= 6 ? (i == active ? A_NONE : A_PLUGNAV)
                   : i == active && s_histLen == 0 && s_screen == to ? A_NONE : A_NAV;
        bool d = Zone(cx - sp / 2, 416, sp, 64, a, i >= 6 && !night ? i - 6 : to);
        if (i == animIc && !d)
            continue;
        int w = on ? NavPillW(dc, i) : 64, x = NavPillX(cx, w);
        if (on || d)
            RSel(dc, x, 426, w, 44, 22, on, d, CLR_INVALID);
        COLORREF fg = d ? kInk : on ? kOrange : kMuted;
        if (on && w > 64) {
            Ico(dc, ic, x + 18, 436, 24, fg);
            T(dc, x + 50, 454, NavLabel(i), 15, fg, 700);
        } else {
            Ico(dc, ic, cx - 13, 435, 26, fg);
        }
    }
}

// Page dots under the content of a paged main screen; tapping them (or swiping) turns the page.
static void PageDots(HDC dc, int page, int pages, int cx = 400, int y = 398)
{
    if (pages < 2)
        return;
    Zone(cx - 120, y - 6, 240, 16, A_PAGE);
    // at most 5 dots (the user 2026-10-23: a long folder's went off the screen): a window round the page shown
    int n = pages < 5 ? pages : 5, first = page - 2;
    first = first > pages - n ? pages - n : first;
    first = first < 0 ? 0 : first;
    int w = 28 + (n - 1) * 28, dx = cx - w / 2;
    for (int p = first; p < first + n; p++) {
        int dw = p == page ? 28 : 20;
        R(dc, dx, y, dw, 6, 3, p == page ? kOrange : kDim);
        dx += dw + 8;
    }
}

// The lists (the user 2026-11-10): one row - kRowH high, kRowStep apart, each its own plate on the background (no
// card under them: the music list's look). A long list turns pages: "‹" "›" (the resistive screen's buttons, at the
// dock's right - y 412, h 52 - or a main screen's path line), a swipe the same (the screen in kPagers, the bars stay),
// the dots above the bottom bar show where. A short one (Wi-Fi) scrolls (Scroll). The rows: kRowH, kRowStep.

static void ListPages(HDC dc, int page, int pages, int y = 412, int h = 52)
{
    if (pages < 2)
        return;
    bool l = page > 0, r = page + 1 < pages;
    Button(dc, 624, y, 76, h, NULL, l ? B_N : B_DIS, IC_BACK, 18, 28, l ? A_PAGESTEP : A_NONE, -1);
    Button(dc, 708, y, 76, h, NULL, r ? B_N : B_DIS, IC_NEXT, 18, 28, r ? A_PAGESTEP : A_NONE, 1);
    PageDots(dc, page, pages, 400, 396);
}

struct Tab { Icon ic; const wchar_t* label; Screen to; };

// A section's screens as pills on top (design 0.6: media, phone), each as wide as its name, spread up to `right`;
// the current one soft orange.
static void TabPills(HDC dc, const Tab* tabs, int n, Screen active, int right)
{
    int w[8], sum = 0;
    for (int i = 0; i < n; i++)
        sum += w[i] = TextWidth(dc, tabs[i].label, 15, 700) + 36;
    int gap = (right - 16 - sum) / (n - 1);
    gap = gap > 10 ? 10 : gap < 2 ? 2 : gap;
    for (int i = 0, x = 16; i < n; x += w[i] + gap, i++) {
        bool on = tabs[i].to == active;
        Button(dc, x, 68, w[i], 40, tabs[i].label, on ? B_SEL : B_N, IC_NONE, 15, 0, on ? A_NONE : A_TAB, tabs[i].to);
    }
}

// The phone's screens (design 0.6: pills on top, no side tabs); its lists' page arrows at the right of them.
static void PhoneTabs(HDC dc, Screen active)
{
    static const Tab tabs[4] = { { IC_KEYPAD, L"Набор", S_PHONE }, { IC_USERS, L"Контакты", S_CONTACTS },
                                 { IC_STAR, L"Избранное", S_FAVS }, { IC_CLOCK, L"Журнал", S_CALLS } };
    TabPills(dc, tabs, 4, active, 600);
}

// Settings, a main screen: ten sections as a 2 x 5 grid of 102 x 62 tiles on the left (an icon, its name under it;
// the current one soft orange), the card 64..400.
static void Settings(HDC dc, Screen active)
{
    static const Tab items[10] = { { IC_GEAR, L"Общие", S_SETTINGS }, { IC_VOL, L"Звук", S_SET_SOUND },
                                  { IC_SUN, L"Экран", S_SET_SCREEN }, { IC_RADIO, L"Радио", S_SET_RADIO },
                                  { IC_BT, L"Bluetooth", S_SET_BT }, { IC_WIFI, L"Wi-Fi", S_SET_WIFI },
                                  { IC_VIDEO, L"Медиа", S_SET_VIDEO }, { IC_CLOUD, L"Сеть", S_SET_NET },
                                  { IC_CAR, L"Автомобиль", S_SET_CAR }, { IC_INFO, L"О системе", S_INFO } };
    Frame(dc, L"Настройки");
    for (int i = 0; i < 10; i++) {
        int x = 16 + (i % 2) * 110, y = 64 + (i / 2) * 68;
        bool on = items[i].to == active || (active == S_SET_BALANCE && items[i].to == S_SET_SOUND) ||
                  (active == S_SET_PLUGINS && items[i].to == S_SETTINGS) ||
                  (active == S_SET_THEME && items[i].to == S_SET_SCREEN) ||
                  (active == S_SET_PHOTO && items[i].to == S_SET_VIDEO) ||
                  (active == S_WIFI_SAVED && items[i].to == S_SET_WIFI) ||
                  ((active == S_SET_OBD || active == S_BT_PAIRED || active == S_BT_PAIRNEW) &&
                   items[i].to == S_SET_BT);
        bool d = Zone(x, y, 102, 62, items[i].to == active ? A_NONE : A_TAB, items[i].to);
        RSel(dc, x, y, 102, 62, 16, on, d);
        COLORREF fg = d ? kInk : on ? kOrange : kMuted;
        Ico(dc, items[i].ic, x + 38, y + 8, 26, fg);
        T(dc, x + 51, y + 53, items[i].label, 13, fg, on ? 700 : 400, AL_MID, 94);
    }
    Panel(dc, 240, 64, 544, 336, 20);
}

// Settings row (rows at 64 + i * kSetH - one height on every page, the user 2026-11-10): label (and a second line),
// separator under it unless last; the control is drawn by the caller. With an action the label part is a button of
// its own.
enum { kSetH = 56 };

// A settings row's own part (the settings' and the widgets' settings' rows): the label 18 and the second line 13
// (design 0.6), the button of the label part (zoneW, with an action), the separator (sepW) unless last; true pressed.
static bool RowCore(HDC dc, int x, int y, int h, int zoneW, int sepW, int textW, const wchar_t* label,
                    const wchar_t* sub, Action a, int arg, bool last)
{
    bool down = a != A_NONE && Zone(x, y + 2, zoneW, h - 4, a, arg);
    if (down)
        R(dc, x, y + 2, zoneW, h - 4, 10, kOrange);
    T(dc, x + 10, y + h / 2 + (sub ? -2 : 7), label, 18, down ? kInk : kText, 400, AL_START, textW);
    if (sub)
        T(dc, x + 10, y + h / 2 + 17, sub, 13, down ? kInk : kDim, 400, AL_START, textW);
    if (!last)
        Sep(dc, x, y + h - 1, sepW);
    return down;
}

static void SetRow(HDC dc, int y, const wchar_t* label, const wchar_t* sub = NULL, bool last = false,
                   Action a = A_NONE, int arg = 0, int h = kSetH)
{
    RowCore(dc, 256, y, h, 420, 512, 400, label, sub, a, arg, last);
}

static void Chevron(HDC dc, int x, int y)
{
    Ico(dc, IC_NEXT, x, y, 32, kMuted);
}

// A settings page of more rows than its card holds (six): they scroll - the finger, the arrows at the card's right
// (x 720), so their controls end at x 712 (760 on the others). Between them the rows go at the y returned + i * kSetH.
static Scroll s_setScroll = { 0, 0, 3 * kSetH };

static int SetScrollBegin(HDC dc, int rows)
{
    static DWORD s_seen;
    if (s_seen != s_screenTick) {   // opened: from the top
        s_seen = s_screenTick;
        s_setScroll.pos = 0;
    }
    s_setScroll.content = rows * kSetH;
    ScrollBegin(dc, s_setScroll, 256, 64, 712, 400);
    return 64 - s_setScroll.pos;
}

static void SetScrollEnd(HDC dc)
{
    ScrollEnd(dc, s_setScroll, 720, 68, 48, 328, 52);
}

// ---- picture placeholders (content, not skin)

static void Cover(HDC dc, Icon ic, int x, int y, int s)
{
    TriFill(x, y, s, s, 20);   // design 0.6: no border
    int isz = s * 100 / 236;
    Ico(dc, ic, x + (s - isz) / 2, y + (s - isz) / 2, isz, Mix(kInk, kTriOr[1], 140));
}

// Sources of the media main screens as pills on top (music from USB, internet radio, Bluetooth, AUX, video, photo),
// each as wide as its name, then a round button of the source's own (the list, the phone), if it has one (the play
// and settings buttons gone: the user 2026-10-23; the settings are in "Настройки").
static void MediaTabs(HDC dc, Screen active, Icon ic, Action a, int arg)
{
    static const Tab tabs[6] = { { IC_MUSIC, L"Музыка", S_AUDIO }, { IC_RADIO, L"Интернет", S_NETRADIO },
                                 { IC_BT, L"Bluetooth", S_AUDIO_BT }, { IC_AUX, L"AUX", S_AUX },
                                 { IC_VIDEO, L"Видео", S_VIDEO }, { IC_PHOTO, L"Фото", S_PHOTO } };
    TabPills(dc, tabs, 6, active, 720);   // up to x 720: the round button after
    if (ic != IC_NONE)
        Button(dc, 736, 64, 48, 48, NULL, B_N, ic, 18, 26, a, arg);
}
