// vh_camera.inc - part of home.cpp (included there, in order): the rear view camera.

// ---- the rear view camera (rvc.cpp), always VestaHome's; by hand from Diagnostics: the reverse polled every 250 ms;
// in reverse a topmost window over everything paints the colour key (the camera's video shows there) and the
// guide lines with the CAN module's steering angle, drawn again when the angle moves by half a degree.
// The window is painted by GDI straight in the screen's format: the key must stay exactly RGB(1, 1, 0).
#define RVC_CLASS L"VestaHome_Rvc"
enum { TIMER_RVC = 8, kRvcMs = 250 };
static HWND s_rvcWnd;
static int  s_rvcAngle;   // the angle drawn, tenths of a degree
static bool s_rvcValid;
static int  s_rvcSignal = -1;   // the video input's signal at the show (0: none - the camera is off)
static bool s_rvcManual;  // the camera shown by hand ("Настройки -> Автомобиль"), a tap closes it
static DWORD s_rvcManualTick;   // when (it closes by itself after kRvcManualMs, and when a reverse ends)
enum { kRvcManualMs = 20000 };
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
    case WM_LBUTTONDOWN:
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
        if (s_rvcManual && s_rvcSignal == 0) {   // shown by hand out of reverse: the camera has no power then
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, RGB(240, 240, 240));
            HGDIOBJ of = SelectObject(dc, Font(22, 700));
            RECT tr = { 40, 180, 760, 300 };
            DrawText(dc, L"Нет сигнала камеры.\nОна включается только на заднем ходу.\nНажмите, чтобы закрыть "
                         L"(само закроется через 20 с).", -1,
                     &tr, DT_CENTER | DT_WORDBREAK);
            SelectObject(dc, of);
        }
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
    s_rvcSignal = RvcVideo(true);
    if (s_rvcWnd)
        InvalidateRect(s_rvcWnd, NULL, FALSE);
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

static void RvcTick()
{
    int rev = RvcReverse();
#ifndef UNDER_CE
    rev = s_rvcFake ? 1 : 0;
#endif
    // shown by hand (HU 2026-10-24-2: a tap did not close it, nor did the reverse - the shell's window over ours took
    // the taps, it seems): closed by itself after 20 s, and when a reverse ends
    static int s_revWas = -1;
    // a tap seen here too: the window's own taps never came on the unit (HU 2026-10-29: "по клику не закрывается",
    // it closed after 20 s); the first 0.7 s skipped - the tap that opened it
    if (s_rvcManual && (GetAsyncKeyState(VK_LBUTTON) & 0x8000) && GetTickCount() - s_rvcManualTick > 700) {
        s_rvcManual = false;
        VLog(L"камера: показ вручную закрыт касанием");
    }
    if (s_rvcManual && ((s_revWas == 1 && rev != 1) || GetTickCount() - s_rvcManualTick > kRvcManualMs)) {
        s_rvcManual = false;
        VLog(L"камера: показ вручную закрыт сам (%s)", s_revWas == 1 && rev != 1 ? L"задний ход кончился" : L"20 с");
    }
    s_revWas = rev;
    // always ours (the user 2026-10-29: the switch gone - with it off the reverse showed OSMapsCE's picture)
    bool want = rev == 1 || s_rvcManual;
    if (want && !s_rvcWnd)
        RvcShow();
    else if (!want && s_rvcWnd)
        RvcHide();
    if (!s_rvcWnd)
        return;
    if (GetForegroundWindow() != s_rvcWnd) {   // over everything, the maps too (OSMapsCE is topmost as well)
        SetWindowPos(s_rvcWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        SetForegroundWindow(s_rvcWnd);
    }
    int a;
    bool v;
    RvcAngleNow(&a, &v);
    if (v != s_rvcValid || a - s_rvcAngle >= 5 || s_rvcAngle - a >= 5) {   // half a degree of the wheel
        s_rvcAngle = a;
        s_rvcValid = v;
        InvalidateRect(s_rvcWnd, NULL, FALSE);
    }
}
