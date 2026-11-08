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
#include "simdrive.h"
#include "vroute.h"   // VestaCore: OSMapsCE's route line
#include "route.inc"

static const int kCount = sizeof(kRoute) / sizeof(kRoute[0]);
enum { kW = 800, kH = 480, TIMER_DRAW = 1, TIMER_OSM = 2 };
static const COLORREF kInk = RGB(18, 20, 25), kPanel = RGB(30, 33, 40), kText = RGB(235, 235, 235),
                      kMuted = RGB(150, 155, 165), kOrange = RGB(255, 107, 26), kGreen = RGB(110, 210, 120);

static HWND          s_wnd;
static volatile LONG s_paused, s_mul = 1, s_restart;
static volatile LONG s_idx;            // the point last sent
static DWORD         s_vt;             // ms into the drive (the module's thread)
// The drive: the built-in one, or one along OSMapsCE's route (the user 2026-11-08) - swapped under s_rcs (the module's
// thread reads it)
static CRITICAL_SECTION s_rcs;
static const SimPoint* s_route = kRoute;
static int            s_count = kCount;
static SimPoint*      s_own;            // OSMapsCE's (NULL: the built-in one)
static float*         s_km;             // km from the start at each point
static wchar_t        s_name[96];
static const wchar_t* s_msg;            // a line for a while (no route in OSMapsCE)
static DWORD          s_msgUntil;

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
    EnterCriticalSection(&s_rcs);
    int i = SimAt(s_vt, s_count);
    SimPoint p = s_route[i];
    LeaveCriticalSection(&s_rcs);
    if (i == s_idx && now - s_sent < 1000)
        return 0;
    s_sent = now;
    InterlockedExchange((LONG*)&s_idx, i);
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

// Another drive (own: malloc'ed, kept until the next one; NULL - the built-in one): from its start.
static void SetRoute(const SimPoint* r, int n, SimPoint* own, const wchar_t* name)
{
    float* km = (float*)malloc(n * sizeof(float));
    if (!km) {
        free(own);
        return;
    }
    km[0] = 0;
    for (int i = 1; i < n; i++)
        km[i] = km[i - 1] + Metres(r[i - 1], r[i]) / 1000;
    EnterCriticalSection(&s_rcs);
    SimPoint* oldOwn = s_own;
    float* oldKm = s_km;
    s_route = r;
    s_count = n;
    s_own = own;
    s_km = km;
    s_idx = 0;
    wcsncpy(s_name, name, 95);
    s_name[95] = 0;
    LeaveCriticalSection(&s_rcs);
    free(oldOwn);
    free(oldKm);
    InterlockedExchange((LONG*)&s_restart, 1);
}

// The route built in OSMapsCE now (VestaCore vroute): driven along from its start
static void LoadOsm()
{
    int* lat = (int*)malloc(VROUTE_MAX * sizeof(int));
    int* lon = (int*)malloc(VROUTE_MAX * sizeof(int));
    int n = lat && lon ? VRouteRead(lat, lon, VROUTE_MAX, NULL) : 0;
    double m = 0;
    for (int i = 1; i < n; i++) {
        SimPoint a = { lat[i - 1], lon[i - 1], 0, 0, 0 }, b = { lat[i], lon[i], 0, 0, 0 };
        m += Metres(a, b);
    }
    int cap = (int)(m / 2) + kSimStand0 + kSimStand1 + 600;   // (at 2 m/s: it averages far more)
    SimPoint* d = n >= 2 ? (SimPoint*)malloc(cap * sizeof(SimPoint)) : NULL;
    int k = d ? SimDrive(lat, lon, n, d, cap) : 0;
    free(lat);
    free(lon);
    if (k < 2) {
        free(d);
        s_msg = L"В OSMapsCE нет маршрута: постройте его и нажмите снова";
        s_msgUntil = GetTickCount() + 4000;
        return;
    }
    wchar_t name[96];
    int km10 = (int)(m / 100 + 0.5);
    _snwprintf(name, 96, L"Маршрут из OSMapsCE · %d,%d км", km10 / 10, km10 % 10);
    SetRoute(d, k, d, name);
}

// ---- drawing. The fonts made once and kept (the user 2026-11-08: slow in front on the unit - each frame made a
// dozen smoothed fonts, the 96 px one too)
struct FontSlot { int size; bool bold; HFONT f; };
static FontSlot s_fonts[12];
static int      s_nFonts;

static HFONT Font(int h, bool bold)
{
    for (int i = 0; i < s_nFonts; i++)
        if (s_fonts[i].size == h && s_fonts[i].bold == bold)
            return s_fonts[i].f;
    LOGFONT lf;
    memset(&lf, 0, sizeof(lf));
    lf.lfHeight = -h;
    lf.lfWeight = bold ? FW_BOLD : FW_NORMAL;
    lf.lfQuality = ANTIALIASED_QUALITY;
    wcscpy(lf.lfFaceName, L"Tahoma");
    HFONT f = CreateFontIndirect(&lf);
    if (s_nFonts < 12) {
        FontSlot slot = { h, bold, f };
        s_fonts[s_nFonts++] = slot;
    }
    return f;   // (a 13th one would leak: there are 8 sizes)
}

static void Text(HDC dc, int x, int y, int w, int h, const wchar_t* s, int size, bool bold, COLORREF c, UINT al)
{
    HFONT old = (HFONT)SelectObject(dc, Font(size, bold));
    SetTextColor(dc, c);
    SetBkMode(dc, TRANSPARENT);
    RECT r = { x, y, x + w, y + h };
    DrawText(dc, s, -1, &r, al | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(dc, old);
}

static void Box(HDC dc, int x, int y, int w, int h, COLORREF c)
{
    RECT r = { x, y, x + w, y + h };
    HBRUSH b = CreateSolidBrush(c);
    FillRect(dc, &r, b);
    DeleteObject(b);
}

enum { B_PAUSE, B_SPEED, B_RESTART, B_HIDE, B_EXIT, B_COUNT };
static const RECT kBtn[B_COUNT] = { { 16, 400, 166, 464 }, { 172, 400, 322, 464 }, { 328, 400, 478, 464 },
                                    { 484, 400, 634, 464 }, { 640, 400, 784, 464 } };
static const RECT kRouteBtn = { 584, 74, 768, 114 };   // the drive: OSMapsCE's route / the built-in one

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
    EnterCriticalSection(&s_rcs);
    int n = s_count, i = s_idx < n ? s_idx : 0;
    const SimPoint p = s_route[i];
    float km0 = s_km[i], km1 = s_km[n - 1];
    bool own = s_own != NULL;
    Text(dc, 36, 72, 540, 32, s_name, 17, false, kMuted, DT_LEFT);
    LeaveCriticalSection(&s_rcs);
    Box(dc, kRouteBtn.left, kRouteBtn.top, kRouteBtn.right - kRouteBtn.left, kRouteBtn.bottom - kRouteBtn.top, kInk);
    Text(dc, kRouteBtn.left, kRouteBtn.top, kRouteBtn.right - kRouteBtn.left, kRouteBtn.bottom - kRouteBtn.top,
         own ? L"Встроенный" : L"Маршрут OSM", 17, true, kOrange, DT_CENTER);
    wchar_t s[160], a[16], b[16];
    Clock(a, 16, i);
    Clock(b, 16, n);
    int k0 = (int)(km0 * 10 + 0.5), k1 = (int)(km1 * 10 + 0.5);   // (a decimal comma)
    _snwprintf(s, 160, L"%s из %s · %d,%d из %d,%d км", a, b, k0 / 10, k0 % 10, k1 / 10, k1 % 10);
    Text(dc, 36, 104, kW - 72, 32, s, 19, false, kText, DT_LEFT);
    Box(dc, 36, 142, kW - 72, 10, RGB(55, 60, 70));
    Box(dc, 36, 142, (kW - 72) * (i + 1) / n, 10, kOrange);
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
    if (s_msg && GetTickCount() < s_msgUntil) {
        wcscpy(s, s_msg);
        c = kOrange;
    }
    Text(dc, 36, 330, kW - 72, 40, s, 18, true, c, DT_LEFT);
    static const wchar_t* const kSpeed[5] = { L"", L"×1", L"×2", L"", L"×4" };
    const wchar_t* label[B_COUNT] = { s_paused ? L"Поехали" : L"Пауза", kSpeed[s_mul], L"Сначала", L"Свернуть", L"Выход" };
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
    if (PtInRect(&kRouteBtn, pt)) {
        if (s_own)
            SetRoute(kRoute, kCount, NULL, kRouteName);
        else
            LoadOsm();
    } else if (PtInRect(&kBtn[B_PAUSE], pt))
        InterlockedExchange((LONG*)&s_paused, !s_paused);
    else if (PtInRect(&kBtn[B_SPEED], pt))
        InterlockedExchange((LONG*)&s_mul, s_mul == 4 ? 1 : s_mul * 2);
    else if (PtInRect(&kBtn[B_RESTART], pt))
        InterlockedExchange((LONG*)&s_restart, 1);
    else if (PtInRect(&kBtn[B_HIDE], pt))
        ShowWindow(s_wnd, SW_HIDE);   // (it drives on; started again - back in front)
    else if (PtInRect(&kBtn[B_EXIT], pt))
        DestroyWindow(s_wnd);
    InvalidateRect(s_wnd, NULL, FALSE);
}

// GpsSimOSM.exe (this program by another name; or "/osm"): a device that starts one program (a Shturmann, the user
// 2026-11-08) gets both - OSMapsCE started once the port is ours, this one out of its way. OSMapsCE looked for beside
// this folder, then on the usual volumes.
static bool s_osmMode;

static void StartOsm()
{
#ifdef UNDER_CE
    static const wchar_t* const kExe = L"OSMapsCE.exe";
#else
    static const wchar_t* const kExe = L"OSMapsPC.exe";
#endif
    wchar_t own[MAX_PATH], path[MAX_PATH];
    GetModuleFileName(NULL, own, MAX_PATH);
    for (int k = 0; k < 2; k++) {   // the exe's name off, then its folder: <card>\GpsSim -> <card>, its OSMapsCE
        wchar_t* sl = wcsrchr(own, L'\\');
        if (sl)
            *sl = 0;
    }
    const wchar_t* dirs[5] = { own, L"\\SDMMC", L"\\ResidentFlash", L"\\Storage Card", L"\\SDMemory" };
    for (int i = 0; i < 5; i++) {
        _snwprintf(path, MAX_PATH, L"%s\\OSMapsCE\\%s", dirs[i], kExe);
        path[MAX_PATH - 1] = 0;
        PROCESS_INFORMATION pi;
#ifdef UNDER_CE
        LPSTARTUPINFO si = NULL;   // (CE: none)
#else
        STARTUPINFO st;
        memset(&st, 0, sizeof(st));
        st.cb = sizeof(st);
        LPSTARTUPINFO si = &st;
#endif
        if (GetFileAttributes(path) != 0xFFFFFFFF &&
            CreateProcess(path, NULL, NULL, NULL, FALSE, 0, NULL, NULL, si, &pi)) {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            ShowWindow(s_wnd, SW_HIDE);
            return;
        }
    }
}

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    switch (m) {
    case WM_PAINT: {   // into a frame kept for the window's life, then onto the screen
        static HDC s_mem;
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        if (!s_mem) {
            s_mem = CreateCompatibleDC(dc);
            SelectObject(s_mem, CreateCompatibleBitmap(dc, kW, kH));
        }
        Paint(s_mem);
        BitBlt(dc, 0, 0, kW, kH, s_mem, 0, 0, SRCCOPY);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_TIMER: {   // drawn again only when it shows something else: another point, the port's line
        if (wp == TIMER_OSM) {
            KillTimer(h, TIMER_OSM);
            StartOsm();
            return 0;
        }
        static LONG s_drawn = -1;
        static int s_owner = -1;
        GpsState g;
        GpsGet(&g);
        if (s_idx != s_drawn || g.owner != s_owner) {
            s_drawn = s_idx;
            s_owner = g.owner;
            InvalidateRect(h, NULL, FALSE);
        }
        return 0;
    }
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
    if (other) {   // one is enough: it comes to the front (hidden by "Свернуть" too)
        ShowWindow(other, SW_SHOW);
        SetForegroundWindow(other);
        return 0;
    }
    {   // "...OSM..." in its name: OSMapsCE started too
        wchar_t own[MAX_PATH];
        GetModuleFileName(NULL, own, MAX_PATH);
        const wchar_t* name = wcsrchr(own, L'\\');
        _wcslwr(own);
        s_osmMode = wcsstr(name ? name : own, L"osm") != NULL;
    }
    InitializeCriticalSection(&s_rcs);
    SetRoute(kRoute, kCount, NULL, kRouteName);
    if (!s_km)
        return 1;
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
    if (s_osmMode)
        SetTimer(s_wnd, TIMER_OSM, 3000, NULL);   // (the port taken over first: OSMapsCE follows it then)
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    GpsStop();   // the port given back: the others take it over
    free(s_km);
    free(s_own);
    return 0;
}
