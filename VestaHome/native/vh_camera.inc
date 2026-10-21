// vh_camera.inc - part of home.cpp (included there, in order): the rear view camera.

// ---- the rear view camera (rvc.cpp) when "own" is on (Settings -> Car): the reverse gear polled every 250 ms;
// in reverse a topmost window over everything paints the colour key (the camera's video shows there) and the
// guide lines with the CAN module's steering angle, drawn again when the angle moves by half a degree.
// The window is painted by GDI straight in the screen's format: the key must stay exactly RGB(1, 1, 0).
#define RVC_CLASS L"VestaHome_Rvc"
enum { TIMER_RVC = 8, kRvcMs = 250 };
static HWND s_rvcWnd;
static int  s_rvcAngle;   // the angle drawn, tenths of a degree
static bool s_rvcValid;
static bool s_rvcManual;  // the camera shown by hand ("Настройки -> Автомобиль"), a tap closes it
#ifndef UNDER_CE
static bool s_rvcFake;    // PC test build: R toggles a "reverse"
#endif

static void RvcAngleNow(int* a10, bool* valid)
{
    Gps g;
    GpsGet(&g);
    *valid = g.angTick && GetTickCount() - g.angTick < 2000;
    *a10 = s_cfg.rvcInvert ? -g.angle10 : g.angle10;
}

static LRESULT CALLBACK RvcProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;
    case WM_LBUTTONUP:   // the camera shown by hand: a tap closes it (RvcTick hides the window)
        s_rvcManual = false;
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        HDC dc = CreateCompatibleDC(hdc);
        HBITMAP bmp = CreateCompatibleBitmap(hdc, 800, 480);   // the screen's own format
        HGDIOBJ ob = SelectObject(dc, bmp);
        RECT rc = { 0, 0, 800, 480 };
        HBRUSH key = CreateSolidBrush(RvcKey());
        FillRect(dc, &rc, key);
        DeleteObject(key);
        RvcDrawLines(dc, s_rvcAngle, s_rvcValid);
#ifndef UNDER_CE
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(200, 200, 200));
        wchar_t s[64];
        _snwprintf(s, 64, L"камера (ПК: без видео и разметки), руль %d.%d", s_rvcAngle / 10, abs(s_rvcAngle % 10));
        TextOut(dc, 20, 20, s, (int)wcslen(s));
#endif
        BitBlt(hdc, 0, 0, 800, 480, dc, 0, 0, SRCCOPY);
        SelectObject(dc, ob);
        DeleteObject(bmp);
        DeleteDC(dc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

static void RvcShow()
{
    static bool s_registered;
    if (!s_registered) {
        WNDCLASS wc;
        memset(&wc, 0, sizeof(wc));
        wc.lpfnWndProc = RvcProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = RVC_CLASS;
        s_registered = RegisterClass(&wc) != 0;
    }
    RvcAngleNow(&s_rvcAngle, &s_rvcValid);
    VidPauseForAudio();
#ifdef UNDER_CE
    s_rvcWnd = CreateWindowEx(WS_EX_TOPMOST, RVC_CLASS, L"VestaHome camera", WS_POPUP | WS_VISIBLE, 0, 0, 800, 480,
                              NULL, NULL, GetModuleHandle(NULL), NULL);
#else
    RECT r;
    GetWindowRect(s_wnd, &r);
    s_rvcWnd = CreateWindowEx(WS_EX_TOPMOST, RVC_CLASS, L"VestaHome camera", WS_POPUP | WS_VISIBLE, r.left, r.top,
                              800, 480, s_wnd, NULL, GetModuleHandle(NULL), NULL);
#endif
    RvcVideo(true);
    VLog(L"камера: задний ход, своё окно %s", s_rvcWnd ? L"открыто" : L"не создано");
}

static void RvcHide()
{
    RvcVideo(false);
    if (s_rvcWnd)
        DestroyWindow(s_rvcWnd);
    s_rvcWnd = NULL;
    VLog(L"камера: задний ход выключен");
    if (IsWindowVisible(s_wnd))
        Redraw();
}

// The shell's own camera (ours off): after the reverse its window stays in front - VestaHome, if it was in front
// before the reverse, comes back 1.5 s after it (HU 2026-10-20).
static void RvcAfterShell(int rev)
{
    static int s_seen = -1;
    static DWORD s_front, s_off;
    static bool s_wasFront;
    DWORD now = GetTickCount();
    if (GetForegroundWindow() == s_wnd && IsWindowVisible(s_wnd))
        s_front = now;
    if (rev != s_seen) {
        if (rev == 1)
            s_wasFront = s_front && now - s_front < 3000;
        else if (s_seen == 1 && !s_cfg.rvcOwn && s_wasFront)
            s_off = now;
        s_seen = rev;
    }
    if (s_off && now - s_off > 1500) {
        s_off = 0;
        if (GetForegroundWindow() != s_wnd) {
            VLog(L"камера: задний ход кончился - VestaHome снова впереди");
            ShowWindow(s_wnd, SW_SHOW);
#ifdef UNDER_CE
            SetWindowPos(s_wnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
#endif
            SetForegroundWindow(s_wnd);
            Redraw();
        }
    }
}

static void RvcTick()
{
    int rev = RvcReverse();
#ifndef UNDER_CE
    rev = s_rvcFake ? 1 : 0;
#endif
    RvcAfterShell(rev);
    bool want = (s_cfg.rvcOwn && rev == 1) || s_rvcManual;
    if (want && !s_rvcWnd)
        RvcShow();
    else if (!want && s_rvcWnd)
        RvcHide();
    if (!s_rvcWnd)
        return;
    int a;
    bool v;
    RvcAngleNow(&a, &v);
    if (v != s_rvcValid || a - s_rvcAngle >= 5 || s_rvcAngle - a >= 5) {   // half a degree of the wheel
        s_rvcAngle = a;
        s_rvcValid = v;
        InvalidateRect(s_rvcWnd, NULL, FALSE);
    }
}
