// gpssim.cpp - GpsSim: a drive along a real route for a head unit (or a Shturmann) without a GPS signal, e.g. one
// on a desk (the user 2026-11-08). It takes the GPS port over from whoever reads it (VestaCore vgps: the event
// "VestaGPS_Yield") and gives its own lines instead - the receiver's NMEA and the CAN module's "@" lines (speed,
// the steering wheel) - so VestaHome, OSMapsCE and VestaHome's COM6 for the navigators all get the drive. The
// drive is built in (route.inc, tools/make_route.py): 27 km of Moscow with roundabouts, forks and the MKAD's
// interchanges; it goes round and round. Closing it gives the port back (the others take it over again).
#include <windows.h>
#include <stdio.h>
#include <math.h>
#include "vgps.h"
#include "simnmea.h"
#include "route.inc"

static const int kCount = sizeof(kRoute) / sizeof(kRoute[0]);
enum { kW = 800, kH = 480, TIMER_DRAW = 1 };
static const COLORREF kInk = RGB(18, 20, 25), kPanel = RGB(30, 33, 40), kText = RGB(235, 235, 235),
                      kMuted = RGB(150, 155, 165), kOrange = RGB(255, 107, 26), kGreen = RGB(110, 210, 120);

static HWND          s_wnd;
static volatile LONG s_paused, s_mul = 1, s_restart;
static volatile LONG s_idx;            // the point last sent
static DWORD         s_vt;             // ms into the drive (the module's thread)
static float*        s_km;             // km from the start at each point

// The module's thread, every ~100 ms: the drive's time goes on (x1, x2, x4; not while paused), the point's lines
// once a second of real time or when the point changed. Paused: standing where it is.
static int Source(char* buf, int cap)
{
    static DWORD s_last, s_sent;
    DWORD now = GetTickCount();
    if (InterlockedExchange((LONG*)&s_restart, 0))
        s_vt = 0;
    if (s_last && !s_paused)
        s_vt += (now - s_last) * (DWORD)s_mul;
    s_last = now;
    int i = SimAt(s_vt, kCount);
    if (i == s_idx && now - s_sent < 1000)
        return 0;
    s_sent = now;
    InterlockedExchange((LONG*)&s_idx, i);
    SimPoint p = kRoute[i];
    if (s_paused)
        p.kmh10 = p.wheel10 = 0;
    SYSTEMTIME utc;
    GetSystemTime(&utc);
    return SimLines(p, utc, buf, cap);
}

static float Metres(const SimPoint& a, const SimPoint& b)
{
    double k = cos((a.lat + b.lat) / 2e6 * 3.14159265 / 180);
    double dx = (b.lon - a.lon) * k, dy = b.lat - a.lat;
    return (float)(sqrt(dx * dx + dy * dy) / 1e6 * 111320);
}

// ---- drawing
static HFONT Font(int h, bool bold)
{
    LOGFONT lf;
    memset(&lf, 0, sizeof(lf));
    lf.lfHeight = -h;
    lf.lfWeight = bold ? FW_BOLD : FW_NORMAL;
    lf.lfQuality = ANTIALIASED_QUALITY;
    wcscpy(lf.lfFaceName, L"Tahoma");
    return CreateFontIndirect(&lf);
}

static void Text(HDC dc, int x, int y, int w, int h, const wchar_t* s, int size, bool bold, COLORREF c, UINT al)
{
    HFONT f = Font(size, bold), old = (HFONT)SelectObject(dc, f);
    SetTextColor(dc, c);
    SetBkMode(dc, TRANSPARENT);
    RECT r = { x, y, x + w, y + h };
    DrawText(dc, s, -1, &r, al | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(dc, old);
    DeleteObject(f);
}

static void Box(HDC dc, int x, int y, int w, int h, COLORREF c)
{
    RECT r = { x, y, x + w, y + h };
    HBRUSH b = CreateSolidBrush(c);
    FillRect(dc, &r, b);
    DeleteObject(b);
}

enum { B_PAUSE, B_SPEED, B_RESTART, B_EXIT, B_COUNT };
static const RECT kBtn[B_COUNT] = { { 16, 400, 206, 464 }, { 214, 400, 404, 464 }, { 412, 400, 602, 464 },
                                    { 610, 400, 784, 464 } };

static void Clock(wchar_t* s, int cap, DWORD sec)
{
    _snwprintf(s, cap, L"%u:%02u", sec / 60, sec % 60);
}

static void PortLine(wchar_t* s, int cap, COLORREF* c)
{
    GpsState g;
    GpsGet(&g);
    *c = g.owner ? kGreen : kOrange;
    if (g.owner)
        _snwprintf(s, cap, L"Порт GPS наш: VestaHome, OSMapsCE и COM6 получают маршрут");
    else
        _snwprintf(s, cap, L"Ждём порт GPS: его читает %s", g.ownerApp[0] ? g.ownerApp : L"другая программа");
    s[cap - 1] = 0;
}

static void Paint(HDC dc)
{
    Box(dc, 0, 0, kW, kH, kInk);
    Text(dc, 20, 0, 500, 56, L"Имитатор GPS", 26, true, kText, DT_LEFT);
    Box(dc, 16, 64, kW - 32, 320, kPanel);
    Text(dc, 36, 72, kW - 72, 32, kRouteName, 17, false, kMuted, DT_LEFT);
    int i = s_idx;
    const SimPoint& p = kRoute[i];
    wchar_t s[160], a[16], b[16];
    Clock(a, 16, i);
    Clock(b, 16, kCount);
    int k0 = (int)(s_km[i] * 10 + 0.5), k1 = (int)(s_km[kCount - 1] * 10 + 0.5);   // (a decimal comma)
    _snwprintf(s, 160, L"%s из %s · %d,%d из %d,%d км", a, b, k0 / 10, k0 % 10, k1 / 10, k1 % 10);
    Text(dc, 36, 104, kW - 72, 32, s, 19, false, kText, DT_LEFT);
    Box(dc, 36, 142, kW - 72, 10, RGB(55, 60, 70));
    Box(dc, 36, 142, (kW - 72) * (i + 1) / kCount, 10, kOrange);
    _snwprintf(s, 160, L"%d", s_paused ? 0 : (p.kmh10 + 5) / 10);
    Text(dc, 36, 166, 220, 110, s, 96, true, kText, DT_LEFT);
    Text(dc, 36, 276, 220, 30, L"км/ч", 20, false, kMuted, DT_LEFT);
    int w = s_paused ? 0 : p.wheel10;
    _snwprintf(s, 160, L"руль %s%d,%d°", w < 0 ? L"−" : L"", abs(w) / 10, abs(w) % 10);
    Text(dc, 300, 176, 450, 40, s, 26, false, kText, DT_LEFT);
    _snwprintf(s, 160, L"курс %d° · %.5f, %.5f", p.course10 / 10, p.lat / 1e6, p.lon / 1e6);
    Text(dc, 300, 222, 450, 32, s, 18, false, kMuted, DT_LEFT);
    COLORREF c;
    PortLine(s, 160, &c);
    Text(dc, 36, 330, kW - 72, 40, s, 18, true, c, DT_LEFT);
    static const wchar_t* const kSpeed[5] = { L"", L"Скорость ×1", L"Скорость ×2", L"", L"Скорость ×4" };
    const wchar_t* label[B_COUNT] = { s_paused ? L"Поехали" : L"Пауза", kSpeed[s_mul], L"Сначала", L"Выход" };
    for (int k = 0; k < B_COUNT; k++) {
        const RECT& r = kBtn[k];
        Box(dc, r.left, r.top, r.right - r.left, r.bottom - r.top, k == B_PAUSE && s_paused ? kOrange : kPanel);
        Text(dc, r.left, r.top, r.right - r.left, r.bottom - r.top, label[k], 20, true,
             k == B_PAUSE && s_paused ? kInk : kText, DT_CENTER);
    }
}

static void Tap(int x, int y)
{
    POINT pt = { x, y };
    if (PtInRect(&kBtn[B_PAUSE], pt))
        InterlockedExchange((LONG*)&s_paused, !s_paused);
    else if (PtInRect(&kBtn[B_SPEED], pt))
        InterlockedExchange((LONG*)&s_mul, s_mul == 4 ? 1 : s_mul * 2);
    else if (PtInRect(&kBtn[B_RESTART], pt))
        InterlockedExchange((LONG*)&s_restart, 1);
    else if (PtInRect(&kBtn[B_EXIT], pt))
        DestroyWindow(s_wnd);
    InvalidateRect(s_wnd, NULL, FALSE);
}

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    switch (m) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps), mem = CreateCompatibleDC(dc);
        HBITMAP bmp = CreateCompatibleBitmap(dc, kW, kH), old = (HBITMAP)SelectObject(mem, bmp);
        Paint(mem);
        BitBlt(dc, 0, 0, kW, kH, mem, 0, 0, SRCCOPY);
        SelectObject(mem, old);
        DeleteObject(bmp);
        DeleteDC(mem);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_TIMER:
        InvalidateRect(h, NULL, FALSE);
        return 0;
    case WM_LBUTTONUP:
        Tap((short)LOWORD(lp), (short)HIWORD(lp));
        return 0;
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE)
            DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(h, m, wp, lp);
}

#ifdef UNDER_CE
int WINAPI WinMain(HINSTANCE inst, HINSTANCE, LPWSTR, int)
#else
int WINAPI WinMain(HINSTANCE inst, HINSTANCE, LPSTR, int)
#endif
{
    HWND other = FindWindow(L"GpsSim_Wnd", NULL);
    if (other) {   // one is enough: it comes to the front
        SetForegroundWindow(other);
        return 0;
    }
    s_km = (float*)malloc(kCount * sizeof(float));
    if (!s_km)
        return 1;
    s_km[0] = 0;
    for (int i = 1; i < kCount; i++)
        s_km[i] = s_km[i - 1] + Metres(kRoute[i - 1], kRoute[i]) / 1000;
    WNDCLASS wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.lpszClassName = L"GpsSim_Wnd";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);
#ifdef UNDER_CE
    s_wnd = CreateWindow(L"GpsSim_Wnd", L"GpsSim", WS_VISIBLE, 0, 0, kW, kH, NULL, NULL, inst, NULL);
#else
    RECT r = { 0, 0, kW, kH };
    AdjustWindowRect(&r, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
    s_wnd = CreateWindow(L"GpsSim_Wnd", L"GpsSim", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE,
                         CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, NULL, NULL, inst, NULL);
#endif
    if (!s_wnd)
        return 1;
    GpsConfig c;
    memset(&c, 0, sizeof(c));
    c.app = L"GpsSim";
    c.source = Source;
    c.takeOver = true;
    GpsStart(&c);
    SetTimer(s_wnd, TIMER_DRAW, 500, NULL);
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    GpsStop();   // the port given back: the others take it over
    free(s_km);
    return 0;
}
