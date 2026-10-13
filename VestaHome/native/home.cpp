// VestaHome - our own shell for the Lada Vesta MMC (Windows CE 6.0, 800x480), style of the skin concept v0.1;
// for now a design demo that doesn't depend on the stock shell G3NavHMI. Every screen has the same top bar
// (back, title, status marks, clock); the main screens (main, radio, media, phone, apps, settings) also share the
// bottom bar of sections. The main screen is a set of widgets. Values are examples, nothing comes from the car yet.
// "Apps" are VestaLaunch's programs built in (apps.txt next to the exe); VestaLaunch stays a separate program
// for the stock shell.
// Colours and pictures come from Skin\skin.ini; whatever it doesn't give is drawn by the built-in code.
// Also builds as a desktop Win32 app for testing on a PC; "VestaHomePC.exe /export" writes the built-in
// graphics to Skin\ as PNG files plus a default skin.ini.
#include <winsock2.h>
#include <windows.h>
#include <mmsystem.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <shellapi.h>
#include "https.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_ONLY_PNG
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_SIMD
#define STBI_NO_THREAD_LOCALS
#define STBI_ASSERT(x) ((void)0)
#include "stb_image.h"

#define APP_CLASS L"VestaHome_Wnd"

// ---------------------------------------------------------------- palette (concept v0.1 tokens; skin.ini may override)

static COLORREF kInk      = RGB(0x0b, 0x0c, 0x0e);   // background
static COLORREF kPanel    = RGB(0x17, 0x19, 0x1c);   // tiles, widgets
static const COLORREF kPanel2   = RGB(0x22, 0x25, 0x2a);   // pressed / nested
static COLORREF kLine     = RGB(0x34, 0x38, 0x3f);   // 2 px borders
static COLORREF kText     = RGB(0xf3, 0xf1, 0xec);
static COLORREF kMuted    = RGB(0xa3, 0xa8, 0xb0);
static COLORREF kDim      = RGB(0x5d, 0x63, 0x6c);
static COLORREF kOrange   = RGB(0xff, 0x6b, 0x1a);   // the one accent: action / active
static COLORREF kFacet    = RGB(0xc2, 0x4a, 0x0b);
static COLORREF kTri[3]   = { RGB(0x0e, 0x0f, 0x12), RGB(0x14, 0x16, 0x1a), RGB(0x17, 0x19, 0x1d) };
static const COLORREF kTriOr[4] = { RGB(0x8f, 0x36, 0x08), RGB(0xc2, 0x4a, 0x0b), RGB(0xa8, 0x3f, 0x09), RGB(0xe6, 0x5a, 0x12) };
static const COLORREF kOff    = RGB(0x3d, 0x42, 0x4a);   // unavailable
static const COLORREF kOk     = RGB(0x2f, 0xbf, 0x71);   // call
static const COLORREF kStop   = RGB(0xe8, 0x48, 0x3b);   // hang up, missed call
static const COLORREF kMargin = RGB(0x08, 0x09, 0x0a);   // around a photo

// ---------------------------------------------------------------- layout

enum Icon { IC_NONE, IC_MAP, IC_NAV, IC_RADIO, IC_MUSIC, IC_PHONE, IC_GEAR, IC_HOME, IC_MOON, IC_BACK, IC_SUN,
            IC_APPS, IC_VIDEO, IC_NEXT, IC_CLOSE, IC_FOLDER, IC_SHIFT, IC_BKSP,
            IC_AUX, IC_PHOTO, IC_INFO, IC_PREV, IC_FWD, IC_PAUSE,
            // built-in only, not in the skin: icons of the other screens (the mockups' set, 48 grid)
            IC_BT, IC_USB, IC_LIST, IC_VOL, IC_SEARCH, IC_SHUFFLE, IC_REPEAT, IC_CALL, IC_HANGUP, IC_WIFI,
            IC_STAR, IC_STAR_F, IC_USER, IC_USERS, IC_CLOCK, IC_KEYPAD, IC_MIC_OFF, IC_HANDOVER, IC_PLAY,
            IC_ROT_L, IC_ROT_R, IC_WARN, IC_CALL_IN, IC_CALL_OUT, IC_CALL_MISS, IC_LOCK, IC_REFRESH, IC_EQ,
            IC_DISC, IC_EYE, IC_POWER, IC_EDIT, IC_ADD, IC_FILE, IC_UP, IC_CAR, IC_CLOUD, IC_COUNT };

// Picture names in skin.ini (and Skin\<name>.png), by icon; NULL = no picture, always the built-in drawing.
static const wchar_t* kIconKey[IC_COUNT] = { NULL, NULL, NULL, L"radio", L"music", L"phone", L"settings", L"home",
    L"night", L"prev", L"day", L"apps", L"video", L"next", L"exit", L"folder", L"shift", L"backspace", L"aux",
    L"photo", L"info", L"rew", L"fwd", L"pause" };

// A_NAV opens a main screen from the bottom bar (no history), A_SCREEN opens a screen over the current one,
// A_TAB opens it instead of the current one (sections, sources), A_BACK returns. A_TOGGLE, A_SEG, A_KEY and A_DEL
// only change the demo values below. A_PAGE and the rest belong to the programs ("Apps", the shortcut editor).
enum Action { A_NONE, A_NIGHT, A_EXIT, A_NAV, A_SCREEN, A_TAB, A_BACK, A_TOGGLE, A_SEG, A_KEY, A_DEL, A_PAGE,
              A_RUN, A_ADD, A_EDITMODE, A_FIELD, A_SAVE, A_DELETE, A_CANCEL, A_OK, A_CHAR, A_SHIFT, A_LANG,
              A_BKSP, A_FILE, A_UP, A_FPREV, A_FNEXT, A_NOICON, A_MINIMIZE, A_CALIBRATE, A_SOON, A_TEST,
              A_KEYS, A_HOMEGPS, A_WXNOW, A_CAMFILE, A_KIND, A_KINDS, A_REGION, A_REGPAGE, A_CAMDL, A_TZ,
              A_GPSPORT, A_GPSBAUD };

// S_SETTINGS is the general section; S_VIDEO and S_PHOTO are the file lists.
// S_INCOMING, S_VOLUME and S_EXIT (closing VestaHome) are windows over the previous screen.
// S_APPS: the programs; S_EDIT, S_KEYS, S_FILES: their shortcut editor, keyboard and file browser.
// S_DIAG: diagnostics (a settings section), S_TEST_*: its tests. S_WSET_*: widgets' settings.
enum Screen { S_HOME, S_RADIO, S_RADIO_LIST, S_RADIO_PTY, S_AUDIO, S_AUDIO_LIST, S_AUDIO_BT, S_AUX,
              S_VIDEO, S_VIDEO_PLAY, S_PHOTO, S_PHOTO_VIEW, S_PHOTO_BAD,
              S_PHONE, S_CONTACTS, S_CONTACT, S_CALLS, S_CALL, S_INCOMING, S_PHONE_OFF,
              S_SETTINGS, S_SET_SOUND, S_SET_BALANCE, S_SET_SCREEN, S_SET_RADIO, S_SET_BT, S_SET_WIFI, S_WIFI_KEYS,
              S_SET_VIDEO, S_SET_PHOTO, S_INFO, S_WARNING, S_VOLUME, S_SAVER, S_EXIT,
              S_CAR, S_APPS, S_EDIT, S_KEYS, S_FILES, S_DIAG, S_TEST_SCREEN, S_TEST_TOUCH, S_TEST_MEM,
              S_WSET_WEATHER, S_WSET_SPEED, S_WSET_CAR, S_CAMDL, S_CAMREG, S_NONE };

struct Btn {
    RECT        rc;
    Action      action;
    int         arg;       // A_NAV, A_SCREEN, A_TAB: Screen; A_TOGGLE: T_*; A_SEG: G_* * 16 + option;
                           // A_KEY: K_* * 65536 + character; A_DEL: K_*; A_RUN: index in s_apps; A_FIELD: F_*;
                           // A_CHAR: character; A_FILE: index in s_files
};

// Values of the screens' switches, segmented choices and two text fields: demo ones, except the settings kept
// in VestaHome.ini (T_GPS_TIME, T_W_GPS .. T_CAM_SOUND, G_WPROV, G_CAMDIST: see CfgToControls).
enum { T_SAVER, T_GPS_TIME, T_LOUDNESS, T_NOISE, T_MW, T_RDS, T_AF, T_CT, T_TA, T_BT, T_AUTOCONNECT, T_AUTOANSWER,
       T_WIFI, T_VIDEO_MOVING, T_SUBTITLES, T_SLIDES_OPEN, T_AUTOROTATE, T_MUTED,
       T_W_GPS, T_CLIMATE, T_CAM_ALERT, T_CAM_SOUND, T_CAM_WEEKLY, T_COUNT };
static bool s_on[T_COUNT] = { true, true, true, true, false, true, false, true, true, true, true, false,
                              true, false, true, false, true, true };
enum { G_PRESET, G_NIGHT, G_TIME_FMT, G_EQ, G_SPEED_VOL, G_VIDEO_SCALE, G_VIDEO_FMT, G_PHOTO_SCALE, G_CALLS,
       G_WPROV, G_CAMDIST, G_COUNT };
static int s_seg[G_COUNT] = { 2, 0, 1, 2, 2, 1, 0, 0, 0 };
enum { K_DIAL, K_PASS };   // the number being dialled, the Wi-Fi password
enum { K_WKEY, K_HOMENAME, K_HOMECOORD };   // settings typed on the keyboard (A_KEYS)
static wchar_t s_dial[20] = L"+7 912 345-67-8";
static wchar_t s_pass[24] = L"vesta2026";

static Screen s_screen = S_HOME;
static Screen s_hist[16];         // the screens under the current one, for A_BACK
static int    s_histLen;
static DWORD  s_screenTick;       // when the current screen was opened: windows that close by themselves
static DWORD  s_callTick;         // when the call started: its timer

enum { kMaxBtns = 96 };
static Btn   s_btns[kMaxBtns];    // buttons of the current screen
static int   kBtnCount;

// Skin pictures, premultiplied RGBA; px == NULL = the built-in drawing is used.
struct Img { int w, h; unsigned char* px; };
static Img     s_icons[IC_COUNT][2];   // [1] = pressed variant
static wchar_t s_skinBg[MAX_PATH];     // main menu background picture, optional

static HWND    s_wnd;
static HBITMAP s_back;          // whole frame, drawn off-screen
static HBITMAP s_bg;            // the skin's main menu background picture, only if it gives one (750 KB)
static unsigned short* s_backBits;   // their pixels: RGB565, bottom-up, 800 per row
static unsigned short* s_bgBits;
static unsigned short* s_tile[2];    // one period of the triangle mosaic (kTileW x kTileH, top-down), day / night
static int     s_pressed = -1;
static bool    s_night;
static wchar_t s_dir[MAX_PATH];

// Events log, VestaHome.log next to the exe: what happened just before a hang of the unit. Opened and closed
// for every line, so a reset keeps it; started again past 256 KB. Uptime first - the clock may jump.
static CRITICAL_SECTION s_logLock;

static void VLog(const wchar_t* fmt, ...)
{
    wchar_t s[400];
    va_list a;
    va_start(a, fmt);
    _vsnwprintf(s, 399, fmt, a);
    va_end(a);
    s[399] = 0;
    SYSTEMTIME t;
    GetLocalTime(&t);
    wchar_t line[460];
    _snwprintf(line, 460, L"%6u %02u:%02u:%02u %s\r\n", (unsigned)(GetTickCount() / 1000), t.wHour, t.wMinute,
               t.wSecond, s);
    line[459] = 0;
    char buf[1400];
    int n = WideCharToMultiByte(CP_UTF8, 0, line, -1, buf, sizeof(buf), NULL, NULL) - 1;
    if (n <= 0)
        return;
    wchar_t path[MAX_PATH];
    _snwprintf(path, MAX_PATH, L"%sVestaHome.log", s_dir);
    EnterCriticalSection(&s_logLock);
    HANDLE h = CreateFile(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, 0, NULL);
    if (h != INVALID_HANDLE_VALUE) {
        if (GetFileSize(h, NULL) > 256 * 1024)
            SetEndOfFile(h);   // at 0: start over
        else
            SetFilePointer(h, 0, NULL, FILE_END);
        DWORD w;
        WriteFile(h, buf, n, &w, NULL);
        CloseHandle(h);
    }
    LeaveCriticalSection(&s_logLock);
}

static wchar_t s_status[96];    // last action result, shown in the top bar
static int     s_downX;         // where the press started: a horizontal swipe turns the page
#ifdef UNDER_CE
static HWND    s_taskbar;
#endif

static const wchar_t* kMonths[12] = { L"января", L"февраля", L"марта", L"апреля", L"мая", L"июня",
                                      L"июля", L"августа", L"сентября", L"октября", L"ноября", L"декабря" };
static const wchar_t* kDays[7] = { L"воскресенье", L"понедельник", L"вторник", L"среда", L"четверг",
                                   L"пятница", L"суббота" };

// Night mode only dims the menu: colours lose a third of their brightness.
static COLORREF C(COLORREF c)
{
    if (!s_night)
        return c;
    return RGB(GetRValue(c) * 2 / 3, GetGValue(c) * 2 / 3, GetBValue(c) * 2 / 3);
}

// ---------------------------------------------------------------- drawing helpers

static void Fill(HDC dc, int l, int t, int r, int b, COLORREF c)
{
    RECT rc = { l, t, r, b };
    HBRUSH br = CreateSolidBrush(C(c));
    FillRect(dc, &rc, br);
    DeleteObject(br);
}

static void Poly(HDC dc, const POINT* p, int n, COLORREF fill)
{
    HBRUSH br = CreateSolidBrush(C(fill));
    HPEN pen = CreatePen(PS_NULL, 0, 0);
    HGDIOBJ ob = SelectObject(dc, br), op = SelectObject(dc, pen);
    Polygon(dc, p, n);
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(br);
    DeleteObject(pen);
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
    HBRUSH br = CreateSolidBrush(C(c));
    HPEN pen = CreatePen(PS_NULL, 0, 0);
    HGDIOBJ ob = SelectObject(dc, br), op = SelectObject(dc, pen);
    Ellipse(dc, x + (int)((cx - r) * s + 0.5), y + (int)((cy - r) * s + 0.5),
            x + (int)((cx + r) * s + 0.5) + 1, y + (int)((cy + r) * s + 0.5) + 1);
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(br);
    DeleteObject(pen);
}

// Equilateral-triangle mosaic, three close shades (the 2018 Vesta trim).
static void Triangles(HDC dc, int l, int t, int r, int b, const COLORREF* shades, int nshades)
{
    const int w = 48, h = 42;
    HRGN clip = CreateRectRgn(l, t, r, b);
    SelectClipRgn(dc, clip);
    for (int row = 0; t + row * h < b; row++) {
        int y0 = t + row * h, y1 = y0 + h;
        for (int col = -1; l + col * w / 2 < r; col++) {
            int x = l + col * w / 2;
            bool up = ((row + col) & 1) == 0;
            POINT p[3];
            if (up) {
                p[0].x = x;         p[0].y = y1;
                p[1].x = x + w / 2; p[1].y = y0;
                p[2].x = x + w;     p[2].y = y1;
            } else {
                p[0].x = x;         p[0].y = y0;
                p[1].x = x + w;     p[1].y = y0;
                p[2].x = x + w / 2; p[2].y = y1;
            }
            Poly(dc, p, 3, shades[(row * 7 + col * 3 + (up ? 1 : 0)) % nshades]);
        }
    }
    SelectClipRgn(dc, NULL);
    DeleteObject(clip);
}

// ---------------------------------------------------------------- icons (48 x 48 design grid)

struct Pen {
    HDC dc; int x, y; double s; HPEN pen; HGDIOBJ old;
    Pen(HDC d, int px, int py, int size, COLORREF c) : dc(d), x(px), y(py), s(size / 48.0)
    {
        int w = (int)(3.5 * s + 0.5);
        pen = CreatePen(PS_SOLID, w < 2 ? 2 : w, C(c));
        old = SelectObject(dc, pen);
        SelectObject(dc, GetStockObject(NULL_BRUSH));
    }
    ~Pen() { SelectObject(dc, old); DeleteObject(pen); }
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
    case IC_ROT_L: {
        double v[] = { 5, 15, 12, 22, 19, 15 };
        p.ArcP(25.5, 25.7, 14, 195.3, 492.7);
        p.Line(v, 3);
        break;
    }
    case IC_ROT_R: {
        double v[] = { 43, 15, 36, 22, 29, 15 };
        p.ArcP(22.5, 25.7, 14, -15.3, -312.7);
        p.Line(v, 3);
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
        p.ArcE(24, 24, 20, 9, 180, 360);
        p.ArcE(24, 24, 20, 9, 0, 180);
        p.Circle(24, 24, 6);
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
    case IC_UP: {
        double v[] = { 9, 31, 24, 16, 39, 31 };
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
    case IC_CLOUD:   // three bumps on a flat base
        p.ArcP(15, 30, 7, 90, 250);
        p.ArcP(25, 25, 10, 190, 330);
        p.ArcP(34, 30, 7, 250, 450);
        p.L2(15, 37, 34, 37);
        break;
    default:
        break;
    }
}

// ---------------------------------------------------------------- skin pictures

#define RGB565(r, g, b) ((unsigned short)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))

// RGB565 bottom-up DIB section (800 x 480: the unit's screen format), with pixels reachable for blending.
static HBITMAP MakeDib(int w, int h, unsigned short** bits)
{
    struct { BITMAPINFOHEADER h; DWORD masks[3]; } bi;
    memset(&bi, 0, sizeof(bi));
    bi.h.biSize = sizeof(BITMAPINFOHEADER);
    bi.h.biWidth = w;
    bi.h.biHeight = h;
    bi.h.biPlanes = 1;
    bi.h.biBitCount = 16;
    bi.h.biCompression = BI_BITFIELDS;
    bi.masks[0] = 0xF800;
    bi.masks[1] = 0x07E0;
    bi.masks[2] = 0x001F;
    HDC screen = GetDC(NULL);
    HBITMAP bmp = CreateDIBSection(screen, (BITMAPINFO*)&bi, DIB_RGB_COLORS, (void**)bits, NULL, 0);
    ReleaseDC(NULL, screen);
    return bmp;
}

// Blends a skin picture scaled (bilinear) to w x h at (x, y) into the back buffer; dimmed at night.
// With a tint the picture is only a mask: painted in that colour.
static void DrawImg(const Img& im, int x, int y, int w, int h, COLORREF tint = CLR_INVALID)
{
#ifndef UNDER_CE
    GdiFlush();   // desktop GDI batches drawing: finish it before touching the pixels
#endif
    for (int dy = 0; dy < h; dy++) {
        int py = y + dy;
        if (py < 0 || py >= 480)
            continue;
        int fy = (2 * dy + 1) * im.h * 128 / h - 128;   // source position, 8-bit fraction
        if (fy < 0)
            fy = 0;
        int y0 = fy >> 8, wy = fy & 255, y1 = y0 + 1 < im.h ? y0 + 1 : y0;
        const unsigned char* r0 = im.px + y0 * im.w * 4;
        const unsigned char* r1 = im.px + y1 * im.w * 4;
        unsigned short* dst = s_backBits + (479 - py) * 800;
        for (int dx = 0; dx < w; dx++) {
            int px = x + dx;
            if (px < 0 || px >= 800)
                continue;
            int fx = (2 * dx + 1) * im.w * 128 / w - 128;
            if (fx < 0)
                fx = 0;
            int x0 = fx >> 8, wx = fx & 255, x1 = x0 + 1 < im.w ? x0 + 1 : x0;
            const unsigned char *pa = r0 + x0 * 4, *pb = r0 + x1 * 4, *pc = r1 + x0 * 4, *pd = r1 + x1 * 4;
            int s[4];
            for (int k = 0; k < 4; k++)
                s[k] = ((pa[k] * (256 - wx) + pb[k] * wx) * (256 - wy) + (pc[k] * (256 - wx) + pd[k] * wx) * wy) >> 16;
            if (!s[3])
                continue;
            if (tint != CLR_INVALID) {
                s[0] = GetRValue(tint) * s[3] / 255;
                s[1] = GetGValue(tint) * s[3] / 255;
                s[2] = GetBValue(tint) * s[3] / 255;
            }
            if (s_night)
                for (int k = 0; k < 3; k++)
                    s[k] = s[k] * 2 / 3;
            unsigned p = dst[px];
            int inv = 255 - s[3];
            int r = s[0] + ((p >> 8) & 0xF8) * inv / 255;
            int g = s[1] + ((p >> 3) & 0xFC) * inv / 255;
            int b = s[2] + ((p << 3) & 0xF8) * inv / 255;
            dst[px] = RGB565(r, g, b);
        }
    }
}


// ---------------------------------------------------------------- frame

static HFONT MakeFont(int height, int weight)
{
    LOGFONT lf;
    memset(&lf, 0, sizeof(lf));
    lf.lfHeight = -height;
    lf.lfWeight = weight;
    lf.lfCharSet = DEFAULT_CHARSET;
    // no smoothing: on the unit ANTIALIASED_QUALITY comes out as ClearType - coloured fringes in the 16-bit
    // frame, small text blurred; Tahoma is hinted for crisp unsmoothed pixels
    lf.lfQuality = NONANTIALIASED_QUALITY;
    wcscpy(lf.lfFaceName, L"Tahoma");
    return CreateFontIndirect(&lf);
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

// The skin's 800 x 480 background picture (as is: night dims it when a frame is drawn).
static bool LoadBackground()
{
    DWORD size;
    char* data = ReadAll(s_skinBg, &size);
    if (!data)
        return false;
    int w, h, comp;
    unsigned char* rgb = stbi_load_from_memory((unsigned char*)data, size, &w, &h, &comp, 3);
    free(data);
    if (!rgb)
        return false;
    for (int y = 0; y < 480; y++) {
        unsigned short* dst = s_bgBits + (479 - y) * 800;
        for (int x = 0; x < 800; x++) {
            COLORREF c = kInk;
            if (x < w && y < h) {
                const unsigned char* p = rgb + (y * w + x) * 3;
                c = RGB(p[0], p[1], p[2]);
            }
            dst[x] = RGB565(GetRValue(c), GetGValue(c), GetBValue(c));
        }
    }
    stbi_image_free(rgb);
    return true;
}

enum { kTileW = 48, kTileH = 252 };   // the mosaic repeats every two triangles across and six rows down

// Night: a third less brightness, as C() does (RGB565).
static unsigned short Dim565(unsigned p)
{
    return (unsigned short)((((p >> 11) * 2 / 3) << 11) | ((((p >> 5) & 63) * 2 / 3) << 5) | ((p & 31) * 2 / 3));
}

// The triangle mosaic is periodic: one period is kept, day and night (24 KB each), and copied into the frame
// row by row (Pattern) instead of keeping whole 800 x 480 backgrounds (750 KB each). Only the skin's own
// background picture, if it gives one, is kept full-size. Built once at start, in day colours.
static void BuildBackground()
{
    unsigned short* bits;
    HBITMAP bmp = MakeDib(kTileW, kTileH, &bits);
    if (bmp) {
        HDC dc = CreateCompatibleDC(NULL);
        HGDIOBJ ob = SelectObject(dc, bmp);
        Triangles(dc, 0, 0, kTileW, kTileH, kTri, 3);
        SelectObject(dc, ob);
        DeleteDC(dc);
#ifndef UNDER_CE
        GdiFlush();
#endif
        s_tile[0] = (unsigned short*)malloc(kTileW * kTileH * 2);
        s_tile[1] = (unsigned short*)malloc(kTileW * kTileH * 2);
        if (s_tile[0] && s_tile[1])
            for (int y = 0; y < kTileH; y++)
                for (int x = 0; x < kTileW; x++) {
                    unsigned short p = bits[(kTileH - 1 - y) * kTileW + x];   // the DIB is bottom-up
                    s_tile[0][y * kTileW + x] = p;
                    s_tile[1][y * kTileW + x] = Dim565(p);
                }
        DeleteObject(bmp);
    }
    if (s_skinBg[0]) {
        s_bg = MakeDib(800, 480, &s_bgBits);
        if (s_bg && !LoadBackground()) {
            DeleteObject(s_bg);
            s_bg = NULL;
        }
    }
}

// Frame rows [top, bottom) filled with the mosaic, its rows counted from anchor (<= top).
static void Pattern(int top, int bottom, int anchor)
{
    const unsigned short* t = s_tile[s_night ? 1 : 0];
    if (!t)
        return;
#ifndef UNDER_CE
    GdiFlush();
#endif
    for (int y = top; y < bottom; y++) {
        const unsigned short* src = t + ((y - anchor) % kTileH) * kTileW;
        unsigned short* dst = s_backBits + (479 - y) * 800;
        for (int x = 0; x < 800; x += kTileW)
            memcpy(dst + x, src, (x + kTileW <= 800 ? kTileW : 800 - x) * 2);
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

// Graphite under the top bar, the mosaic below it.
static void SubBackground(HDC dc)
{
    Fill(dc, 0, 0, 800, 56, kInk);
    Pattern(56, 480, 56);
}

// The main screen: the skin's background picture (dimmed per frame at night), otherwise the same as the others.
static void MainBackground(HDC dc)
{
    if (!s_bg) {
        SubBackground(dc);
        return;
    }
    Blit(dc, s_bg);
    if (s_night) {
#ifndef UNDER_CE
        GdiFlush();
#endif
        for (int i = 0; i < 800 * 480; i++)
            s_backBits[i] = Dim565(s_backBits[i]);
    }
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

static int TextWidth(HDC dc, const wchar_t* s, int size, int weight)
{
    SIZE sz;
    HGDIOBJ of = SelectObject(dc, Font(size, weight));
    GetTextExtentPoint32(dc, s, (int)wcslen(s), &sz);
    SelectObject(dc, of);
    return sz.cx;
}

enum { AL_START, AL_MID, AL_END };

// Text like SVG <text>: baseline at y, anchored at its start, middle or end. A line wider than maxw
// (0 = no limit) is set smaller until it fits: Tahoma is wider than the mockups' font. Returns its right edge.
static int T(HDC dc, int x, int y, const wchar_t* s, int size, COLORREF c, int weight = 600, int anchor = AL_START,
             int maxw = 0)
{
    int w = TextWidth(dc, s, size, weight);
    while (maxw > 0 && w > maxw && size > 10)
        w = TextWidth(dc, s, --size, weight);
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
    HBRUSH br = fill == CLR_INVALID ? NULL : CreateSolidBrush(C(fill));
    HPEN pen = stroke == CLR_INVALID ? CreatePen(PS_NULL, 0, 0) : CreatePen(PS_SOLID, 2, C(stroke));
    HGDIOBJ ob = SelectObject(dc, br ? (HGDIOBJ)br : GetStockObject(NULL_BRUSH)), op = SelectObject(dc, pen);
    int e = stroke == CLR_INVALID ? 1 : 0;   // without a pen GDI leaves out the right and bottom edge
    RoundRect(dc, x, y, x + w + e, y + h + e, rx * 2, rx * 2);
    SelectObject(dc, ob);
    SelectObject(dc, op);
    if (br)
        DeleteObject(br);
    DeleteObject(pen);
}

static void Disc(HDC dc, int cx, int cy, int r, COLORREF fill, COLORREF stroke = CLR_INVALID)
{
    HBRUSH br = fill == CLR_INVALID ? NULL : CreateSolidBrush(C(fill));
    HPEN pen = stroke == CLR_INVALID ? CreatePen(PS_NULL, 0, 0) : CreatePen(PS_SOLID, 2, C(stroke));
    HGDIOBJ ob = SelectObject(dc, br ? (HGDIOBJ)br : GetStockObject(NULL_BRUSH)), op = SelectObject(dc, pen);
    int e = stroke == CLR_INVALID ? 1 : 0;
    Ellipse(dc, cx - r, cy - r, cx + r + e, cy + r + e);
    SelectObject(dc, ob);
    SelectObject(dc, op);
    if (br)
        DeleteObject(br);
    DeleteObject(pen);
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
    HPEN pen = CreatePen(PS_SOLID, width, C(c));
    HGDIOBJ op = SelectObject(dc, pen);
    POINT p[2] = { { x0, y0 }, { x1, y1 } };
    Polyline(dc, p, 2);
    SelectObject(dc, op);
    DeleteObject(pen);
}

static void PolyD(HDC dc, const double* xy, int n, COLORREF c)
{
    POINT p[8];
    for (int i = 0; i < n; i++) {
        p[i].x = (int)(xy[2 * i] + 0.5);
        p[i].y = (int)(xy[2 * i + 1] + 0.5);
    }
    Poly(dc, p, n, c);
}

static void Clip(HDC dc, int x, int y, int w, int h)
{
    HRGN rgn = CreateRectRgn(x, y, x + w, y + h);
    SelectClipRgn(dc, rgn);
    DeleteObject(rgn);
}

// ---- pixels: the pattern, translucency (the back buffer is RGB565 and reachable)

static unsigned short Px(COLORREF c)
{
    c = C(c);
    return RGB565(GetRValue(c), GetGValue(c), GetBValue(c));
}

// Shade index of the triangle mosaic at (u, v) from its origin: the same tiling as Triangles().
static int TriIndex(int u, int v, int n)
{
    const int w = 48, h = 42;
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
    unsigned short sh[4];
    for (int k = 0; k < 4; k++)
        sh[k] = Px(kTriOr[k]);
    for (int py = y; py < y + h; py++) {
        if (py < 0 || py >= 480)
            continue;
        unsigned short* row = s_backBits + (479 - py) * 800;
        int dy = py < y + rx ? y + rx - py : py >= y + h - rx ? py - (y + h - rx - 1) : 0;
        for (int px = x; px < x + w; px++) {
            if (px < 0 || px >= 800)
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
    for (int py = y; py < y + h && py < 480; py++) {
        int a = a0 + (a1 - a0) * (py - y) / (h > 1 ? h - 1 : 1);
        int ir = GetRValue(ink) * a, ig = GetGValue(ink) * a, ib = GetBValue(ink) * a;
        unsigned short* row = s_backBits + (479 - py) * 800;
        for (int px = x; px < x + w && px < 800; px++) {
            unsigned p = row[px];
            int r = (((p >> 8) & 0xF8) * (255 - a) + ir) / 255;
            int g = (((p >> 3) & 0xFC) * (255 - a) + ig) / 255;
            int b = (((p << 3) & 0xF8) * (255 - a) + ib) / 255;
            row[px] = RGB565(r, g, b);
        }
    }
}

// ---- widgets of the mockups (make_v01_screens.py helpers of the same names)

// Registers a button of the current screen; true while it is pressed.
static bool Zone(int x, int y, int w, int h, Action a, int arg = 0)
{
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
    const Img& im = s_icons[ic][0];
    const Img& dn = s_icons[ic][1];
    if (c == kInk && dn.px)
        DrawImg(dn, x, y, size, size);   // the skin's own pressed picture, if it gives one
    else if (im.px)
        DrawImg(im, x, y, size, size, c == kText ? CLR_INVALID : c);
    else
        PaintIcon(dc, ic, x, y, size, c);
}

static void Sep(HDC dc, int x, int y, int w)
{
    Fill(dc, x, y, x + w, y + 2, kLine);
}

static void Panel(HDC dc, int x, int y, int w, int h, int rx = 14)
{
    R(dc, x, y, w, h, rx, kPanel, kLine);
}

// Button states: outlined, selected source, pressed / active, unavailable, filled, the dialog's main action.
// The shell has no separate "selected" text colour, so selected, active and main action are all orange.
enum { B_N, B_SEL, B_ACT, B_DIS, B_FILL, B_PRI };

static void Button(HDC dc, int x, int y, int w, int h, const wchar_t* label, int state, Icon ic = IC_NONE,
                   int size = 18, int isz = 0, Action a = A_NONE, int arg = 0)
{
    if (Zone(x, y, w, h, a, arg))
        state = B_ACT;
    COLORREF fg;
    if (state == B_ACT || state == B_SEL || state == B_PRI) {
        R(dc, x, y, w, h, 10, kOrange);
        fg = kInk;
    } else if (state == B_DIS) {
        R(dc, x, y, w, h, 10, RGB(0x11, 0x13, 0x16), kPanel2);
        fg = kOff;
    } else if (state == B_FILL) {
        R(dc, x, y, w, h, 10, kPanel, kLine);
        fg = kText;
    } else {
        R(dc, x, y, w, h, 10, CLR_INVALID, kLine);
        fg = label ? kMuted : kText;
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

// A bare icon (docks, transport): its button reaches 8 px around it, pressed = an orange plate.
static void IconBtn(HDC dc, Icon ic, int x, int y, int size, COLORREF c, Action a = A_NONE, int arg = 0)
{
    bool down = Zone(x - 8, y - 8, size + 16, size + 16, a, arg);
    if (down)
        R(dc, x - 8, y - 8, size + 16, size + 16, 10, kOrange);
    Ico(dc, ic, x, y, size, down ? kInk : c);
}

// The orange "pause" circle of the players.
static void PauseBtn(HDC dc, int cx, int cy, int r, int isz)
{
    bool down = Zone(cx - r, cy - r, 2 * r, 2 * r, A_NONE);
    Disc(dc, cx, cy, r, down ? kFacet : kOrange);
    Ico(dc, IC_PAUSE, cx - isz / 2, cy - isz / 2, isz, kInk);
}

// Switch s_on[id].
static void Toggle(HDC dc, int x, int y, int id)
{
    bool on = s_on[id] != Zone(x - 10, y - 10, 92, 56, A_TOGGLE, id);   // pressed shows the new state
    if (on) {
        R(dc, x, y, 72, 36, 18, kOrange);
        Disc(dc, x + 54, y + 18, 14, kInk);
    } else {
        R(dc, x, y, 72, 36, 18, kPanel2, kLine);
        Disc(dc, x + 18, y + 18, 13, kDim);
    }
}

// Segmented choice s_seg[id] among n options.
static void Seg(HDC dc, int x, int y, int w, int h, const wchar_t* const* opts, int n, int id, int size = 17)
{
    R(dc, x, y, w, h, 10, kInk, kLine);
    int cw = (w - 4) / n;
    for (int i = 0; i < n; i++) {
        int cx = x + 2 + i * cw;
        bool on = Zone(cx, y, cw, h, A_SEG, id * 16 + i) || s_seg[id] == i;
        if (on)
            R(dc, cx, y + 2, cw, h - 4, 8, kOrange);
        T(dc, cx + cw / 2, y + h / 2 + size * 36 / 100, opts[i], size, on ? kInk : kMuted, 700, AL_MID, cw - 6);
    }
}

static void Slider(HDC dc, int x, int y, int w, int pct, const wchar_t* val)
{
    int k = w * pct / 100;
    R(dc, x, y, w, 8, 4, kLine);
    R(dc, x, y, k > 8 ? k : 8, 8, 4, kOrange);
    Disc(dc, x + k, y + 4, 13, kText);
    if (val)
        T(dc, x + w + 64, y + 12, val, 22, kText, 700, AL_END);
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
    R(dc, x, y, w, h, 10, kInk, kLine);
    bool l = Zone(x, y, 48, h, a, -1), r = Zone(x + w - 48, y, 48, h, a, 1);
    if (l)
        R(dc, x + 2, y + 2, 44, h - 4, 8, kOrange);
    if (r)
        R(dc, x + w - 46, y + 2, 44, h - 4, 8, kOrange);
    Ico(dc, IC_BACK, x + 6, y + (h - 32) / 2, 32, l ? kInk : kText);
    Ico(dc, IC_NEXT, x + w - 38, y + (h - 32) / 2, 32, r ? kInk : kText);
    T(dc, x + w / 2, y + h / 2 + size * 36 / 100, value, size, kText, 700, AL_MID, w - 96);
}

static void Scrollbar(HDC dc, int x, int y, int h, int pos, int len)   // pos, len in % of h
{
    R(dc, x, y, 6, h, 3, kLine);
    R(dc, x, y + h * pos / 100, 6, h * len / 100, 3, kDim);
}

static void Avatar(HDC dc, int cx, int cy, int r, const wchar_t* initials, int size)
{
    Disc(dc, cx, cy, r, kPanel2);
    T(dc, cx, cy + size * 36 / 100, initials, size, kMuted, 700, AL_MID);
}

static void AvatarOr(HDC dc, int cx, int cy, int r, const wchar_t* initials)
{
    TriFill(cx - r, cy - r, 2 * r, 2 * r, r);
    Disc(dc, cx, cy, r, CLR_INVALID, kLine);
    T(dc, cx, cy + r * 3 / 10, initials, r * 8 / 10, Mix(kInk, kTriOr[1], 191), 700, AL_MID);
}

enum { L_N, L_CUR, L_DIM };

// List row, one button: optional number, icon, second line and right-hand text; current = orange.
static void ListRow(HDC dc, int x, int y, int w, int h, const wchar_t* text, const wchar_t* right, int state,
                    Icon ic = IC_NONE, const wchar_t* sub = NULL, const wchar_t* num = NULL, Action a = A_NONE,
                    int arg = 0)
{
    if (Zone(x, y, w, h, a, arg))
        state = L_CUR;
    bool cur = state == L_CUR;
    COLORREF fg = cur ? kInk : state == L_DIM ? kOff : kText, mg = cur ? kInk : kMuted;
    R(dc, x, y, w, h, 10, cur ? kOrange : kPanel);
    int tx = x + 16;
    if (num) {
        T(dc, tx, y + h / 2 + 6, num, 16, cur ? kInk : kDim, 700);
        tx += 40;
    }
    if (ic != IC_NONE) {
        Ico(dc, ic, tx, y + (h - 30) / 2, 30, cur ? kInk : kMuted);
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

static void Dock(HDC dc)
{
    Fill(dc, 0, 398, 800, 480, kInk);
    Sep(dc, 0, 398, 800);
}

static void Now(wchar_t* s, int n)
{
    SYSTEMTIME t;
    GetLocalTime(&t);
    _snwprintf(s, n, L"%02d:%02d", t.wHour, t.wMinute);
}

// Top bar of every screen: back button (when there is a screen to go back to, or a given action) and title,
// the last action's result, then the status marks (GPS, Bluetooth, USB), the outside temperature and the clock.
// A main screen may put one action button of its own before the marks (act). Demo: the Bluetooth mark opens
// the incoming call window, the clock the volume window. VestaHome is closed from the settings' diagnostics.
static bool OutsideTemp(int* t);
static bool CamChip(wchar_t* s, int n);

static void Frame(HDC dc, const wchar_t* title, Action back = A_BACK, Icon act = IC_NONE, Action a = A_NONE,
                  bool sel = false)
{
    Sep(dc, 0, 54, 800);
    Ico(dc, IC_NAV, 546, 16, 24, kMuted);
    bool d = Zone(576, 4, 44, 48, A_SCREEN, S_INCOMING);
    if (d)
        R(dc, 576, 4, 44, 48, 10, kOrange);
    Ico(dc, IC_BT, 584, 14, 28, d ? kInk : kMuted);
    Ico(dc, IC_USB, 622, 14, 28, kMuted);
    int temp;
    wchar_t s[40];
    if (OutsideTemp(&temp)) {   // from the weather for now; the car's own sensor (MCU) later
        _snwprintf(s, 8, L"%+d°", temp);
        T(dc, 700, 36, s, 18, kMuted, 600, AL_END);
    }
    Now(s, 8);
    d = Zone(706, 4, 90, 48, A_SCREEN, S_VOLUME);
    if (d)
        R(dc, 706, 4, 90, 48, 10, kOrange);
    T(dc, 784, 38, s, 26, d ? kInk : kText, 600, AL_END);
    int x = 20, right = act != IC_NONE ? 470 : 530;
    if (act != IC_NONE)
        Button(dc, 478, 6, 56, 44, NULL, sel ? B_SEL : B_N, act, 18, 28, a);
    if (CamChip(s, 40)) {   // a speed camera ahead, on every screen
        int cx = right - 156;
        R(dc, cx, 8, 152, 40, 10, kOrange);
        T(dc, cx + 76, 34, s, 14, kInk, 700, AL_MID, 144);
        right = cx - 8;
    }
    if (back != A_NONE && (back != A_BACK || s_histLen > 0)) {
        d = Zone(0, 0, 64, 56, back);
        R(dc, 8, 8, 48, 40, 8, d ? kOrange : kPanel);
        Ico(dc, IC_BACK, 14, 10, 36, d ? kInk : kText);
        x = 72;
    }
    int end = title[0] ? T(dc, x, 37, title, 22, kText, 600, AL_START, s_status[0] ? 300 : right - x) : x;
    if (s_status[0])
        T(dc, end + 16, 36, s_status, 15, kMuted, 400, AL_START, right - end - 16);
}

// Sections of the bottom bar; each opens its main screen (media: the source shown last).
struct NavItem { Icon ic; const wchar_t* label; Screen to; };
static const NavItem kNav[6] = { { IC_HOME, L"ГЛАВНАЯ", S_HOME }, { IC_RADIO, L"РАДИО", S_RADIO },
                                 { IC_MUSIC, L"МЕДИА", S_AUDIO }, { IC_PHONE, L"ТЕЛЕФОН", S_PHONE },
                                 { IC_APPS, L"ПРИЛОЖЕНИЯ", S_APPS }, { IC_GEAR, L"НАСТРОЙКИ", S_SETTINGS } };
static Screen s_media = S_AUDIO;

// Section of a main screen (index in kNav); -1: not a main screen, it has no bottom bar (its own dock instead).
static int Section(Screen s)
{
    switch (s) {
    case S_HOME:
        return 0;
    case S_RADIO:
        return 1;
    case S_AUDIO: case S_AUDIO_BT: case S_AUX: case S_VIDEO: case S_PHOTO:
        return 2;
    case S_PHONE:
        return 3;
    case S_APPS:
        return 4;
    case S_SETTINGS: case S_SET_SOUND: case S_SET_BALANCE: case S_SET_SCREEN: case S_SET_RADIO: case S_SET_BT:
    case S_SET_WIFI: case S_SET_VIDEO: case S_SET_PHOTO: case S_INFO: case S_DIAG:
        return 5;
    default:
        return -1;
    }
}

// Bottom bar of the main screens: the six sections and day/night, 104 x 58 each; the current section is orange.
// The main screens keep their content above y 400.
static void NavBar(HDC dc, int active)
{
    Fill(dc, 0, 408, 800, 480, kInk);
    Sep(dc, 0, 408, 800);
    for (int i = 0; i < 7; i++) {
        int x = 8 + i * 112;
        Icon ic = i < 6 ? kNav[i].ic : s_night ? IC_SUN : IC_MOON;
        const wchar_t* label = i < 6 ? kNav[i].label : s_night ? L"ДЕНЬ" : L"НОЧЬ";
        Screen to = i == 2 ? s_media : i < 6 ? kNav[i].to : S_NONE;
        bool on = i == active;
        bool d = Zone(x, 416, 104, 58, i == 6 ? A_NIGHT : on && s_histLen == 0 && s_screen == to ? A_NONE : A_NAV, to);
        if (on || d)
            R(dc, x, 416, 104, 58, 10, kOrange);
        COLORREF fg = on || d ? kInk : kMuted;
        Ico(dc, ic, x + 37, 421, 30, fg);
        T(dc, x + 52, 467, label, 12, fg, 700, AL_MID, 98);
    }
}

// Page dots under the content of a paged main screen; tapping them (or swiping) turns the page.
static void PageDots(HDC dc, int page, int pages)
{
    if (pages < 2)
        return;
    Zone(280, 392, 240, 16, A_PAGE);
    int w = 28 + (pages - 1) * 28, dx = 400 - w / 2;
    for (int p = 0; p < pages; p++) {
        int dw = p == page ? 28 : 20;
        R(dc, dx, 398, dw, 6, 3, p == page ? kOrange : kDim);
        dx += dw + 8;
    }
}

// Audio sources in the dock; the current one is selected, the others switch to their screen.
static void SourcesDock(HDC dc, const wchar_t* active, Screen local)
{
    static const wchar_t* names[4] = { L"USB", L"SD", L"BT", L"AUX" };
    Screen to[4] = { local, local, S_AUDIO_BT, S_AUX };
    Dock(dc);
    for (int i = 0; i < 4; i++) {
        bool on = !wcscmp(names[i], active);
        Button(dc, 16 + i * 120, 410, 112, 58, names[i], on ? B_SEL : B_N, IC_NONE, 18, 0, on ? A_NONE : A_TAB, to[i]);
    }
}

struct Tab { Icon ic; const wchar_t* label; Screen to; };

// Column of section buttons on the left (media lists, phone); to = S_NONE: no screen of its own.
static void SideTabs(HDC dc, const Tab* tabs, int n, int active, int step, int h, int y0 = 68)
{
    for (int i = 0; i < n; i++) {
        int y = y0 + i * step;
        bool on = i == active || Zone(16, y, 160, h, i == active || tabs[i].to == S_NONE ? A_NONE : A_TAB, tabs[i].to);
        R(dc, 16, y, 160, h, 10, on ? kOrange : kPanel);
        Ico(dc, tabs[i].ic, 30, y + (h - 32) / 2, 32, on ? kInk : kMuted);
        T(dc, 72, y + h / 2 + 6, tabs[i].label, 13, on ? kInk : kMuted, 700, AL_START, 100);
    }
}

static void PhoneNav(HDC dc, int active)
{
    static const Tab tabs[5] = { { IC_KEYPAD, L"НАБОР", S_PHONE }, { IC_USERS, L"КОНТАКТЫ", S_CONTACTS },
                                 { IC_STAR, L"ИЗБРАННОЕ", S_CONTACTS }, { IC_CLOCK, L"ЖУРНАЛ", S_CALLS },
                                 { IC_BT, L"МУЗЫКА", S_AUDIO_BT } };
    SideTabs(dc, tabs, 5, active, 78, 70);
}

// Settings, a main screen: ten sections as a 2 x 5 grid of 102 x 62 tiles on the left, the panel 64..400.
static void Settings(HDC dc, Screen active)
{
    static const Tab items[10] = { { IC_GEAR, L"ОБЩИЕ", S_SETTINGS }, { IC_VOL, L"ЗВУК", S_SET_SOUND },
                                  { IC_MOON, L"ЭКРАН", S_SET_SCREEN }, { IC_RADIO, L"РАДИО", S_SET_RADIO },
                                  { IC_BT, L"ТЕЛЕФОН", S_SET_BT }, { IC_WIFI, L"WI-FI", S_SET_WIFI },
                                  { IC_VIDEO, L"ВИДЕО", S_SET_VIDEO }, { IC_PHOTO, L"ФОТО", S_SET_PHOTO },
                                  { IC_INFO, L"О СИСТЕМЕ", S_INFO }, { IC_SEARCH, L"ДИАГНОСТИКА", S_DIAG } };
    Frame(dc, L"НАСТРОЙКИ");
    for (int i = 0; i < 10; i++) {
        int x = 16 + (i % 2) * 110, y = 64 + (i / 2) * 68;
        bool on = items[i].to == active || (active == S_SET_BALANCE && items[i].to == S_SET_SOUND);
        on = Zone(x, y, 102, 62, items[i].to == active ? A_NONE : A_TAB, items[i].to) || on;
        R(dc, x, y, 102, 62, 10, on ? kOrange : kPanel);
        Ico(dc, items[i].ic, x + 37, y + 7, 28, on ? kInk : kMuted);
        T(dc, x + 51, y + 53, items[i].label, 12, on ? kInk : kMuted, 700, AL_MID, 94);
    }
    Panel(dc, 240, 64, 544, 336);
}

// Settings row (rows at 64 + i * 56): label (and a second line), separator under it unless last; the control is
// drawn by the caller. With an action the label part is a button of its own.
static void SetRow(HDC dc, int y, const wchar_t* label, const wchar_t* sub = NULL, bool last = false,
                   Action a = A_NONE, int arg = 0, int h = 56)
{
    bool down = a != A_NONE && Zone(256, y + 2, 420, h - 4, a, arg);
    if (down)
        R(dc, 256, y + 2, 420, h - 4, 10, kOrange);
    T(dc, 264, y + h / 2 + (sub ? -2 : 7), label, 19, down ? kInk : kText, 700, AL_START, 400);
    if (sub)
        T(dc, 264, y + h / 2 + 18, sub, 15, down ? kInk : kMuted, 400, AL_START, 400);
    if (!last)
        Sep(dc, 256, y + h - 1, 512);
}

static void Chevron(HDC dc, int x, int y)
{
    Ico(dc, IC_NEXT, x, y, 32, kMuted);
}

// ---- picture placeholders (content, not skin)

static void Road(HDC dc, int x, int y, int w, int h)   // night road: the video frame
{
    Clip(dc, x, y, w, h);
    double hz = y + h * 0.45, cx = x + w / 2.0;
    double road[] = { cx - 40, hz, cx + 40, hz, x + w * 0.95, y + h, x + w * 0.05, y + h };
    double mark[] = { cx - 2, hz + 8, cx + 2, hz + 8, cx + 7, y + h, cx - 7, y + h };
    double hill1[] = { x, hz, x + w * 0.2, hz - 40, x + w * 0.42, hz, x, hz + 10 };
    double hill2[] = { x + w * 0.58, hz, x + w * 0.8, hz - 50, x + w, hz };
    Fill(dc, x, y, x + w, y + h, RGB(0x14, 0x1a, 0x26));
    Fill(dc, x, (int)hz, x + w, y + h, RGB(0x10, 0x11, 0x14));
    PolyD(dc, road, 4, RGB(0x1f, 0x21, 0x25));
    PolyD(dc, mark, 4, Mix(RGB(0xc9, 0xbf, 0x8f), RGB(0x1f, 0x21, 0x25), 204));
    PolyD(dc, hill1, 4, RGB(0x0d, 0x10, 0x18));
    PolyD(dc, hill2, 3, RGB(0x0d, 0x10, 0x18));
    Disc(dc, (int)cx + 6, (int)hz + 4, 4, Mix(RGB(0xf3, 0xe3, 0xb0), RGB(0x1f, 0x21, 0x25), 178));
    SelectClipRgn(dc, NULL);
}

static void Landscape(HDC dc, int x, int y, int w, int h)   // mountains at sunset: a photo
{
    Clip(dc, x, y, w, h);
    double hz = y + h * 0.62;
    double hills[] = { x, hz, x + w * 0.25, y + h * 0.3, x + w * 0.45, y + h * 0.5, x + w * 0.62, y + h * 0.26,
                       x + w, hz };
    double path[] = { x + w * 0.45, y + h, x + w * 0.52, hz, x + w * 0.6, y + h };
    Fill(dc, x, y, x + w, y + h, RGB(0x3a, 0x3f, 0x4a));
    Fill(dc, x, y + h * 35 / 100, x + w, y + h * 65 / 100, RGB(0x4c, 0x4a, 0x4c));
    Disc(dc, x + w * 7 / 10, y + h * 36 / 100, h * 9 / 100, RGB(0xe7, 0xa3, 0x6b));
    PolyD(dc, hills, 5, RGB(0x26, 0x2a, 0x33));
    Fill(dc, x, (int)hz, x + w, y + h, RGB(0x1d, 0x2a, 0x22));
    PolyD(dc, path, 3, RGB(0x5b, 0x54, 0x46));
    SelectClipRgn(dc, NULL);
}

// Cover of the players: the orange mosaic with the source's sign.
static void Cover(HDC dc, Icon ic, int x, int y, int s)
{
    TriFill(x, y, s, s, 14);
    R(dc, x, y, s, s, 14, CLR_INVALID, kLine);
    int isz = s * 100 / 236;
    Ico(dc, ic, x + (s - isz) / 2, y + (s - isz) / 2, isz, Mix(kInk, kTriOr[1], 140));
}

// Sources of the media main screens as tabs on top (music from USB, Bluetooth, AUX, video, photo), then up to
// two buttons of the source: its own (list, player, slides) and its settings.
static void MediaTabs(HDC dc, Screen active, Icon ic, Action a, int arg, Screen settings)
{
    static const Tab tabs[5] = { { IC_MUSIC, L"МУЗЫКА", S_AUDIO }, { IC_BT, L"BLUETOOTH", S_AUDIO_BT },
                                 { IC_AUX, L"AUX", S_AUX }, { IC_VIDEO, L"ВИДЕО", S_VIDEO },
                                 { IC_PHOTO, L"ФОТО", S_PHOTO } };
    for (int i = 0; i < 5; i++) {
        bool on = tabs[i].to == active;
        Button(dc, 16 + i * 118, 64, 112, 48, tabs[i].label, on ? B_SEL : B_N, IC_NONE, 15, 0, on ? A_NONE : A_TAB,
               tabs[i].to);
    }
    if (ic != IC_NONE)
        Button(dc, 616, 64, 80, 48, NULL, B_N, ic, 18, 30, a, arg);
    Button(dc, 704, 64, 80, 48, NULL, B_N, IC_GEAR, 18, 30, A_SCREEN, settings);
}

// ---- screens

struct Station { int f10; const wchar_t* freq; const wchar_t* shortName; const wchar_t* name; };
static const Station kPresets[7] = {
    { 1017, L"101.7", L"Хит FM", L"Хит FM" }, { 1025, L"102.5", L"Маяк", L"Маяк" },
    { 1062, L"106.2", L"Европа+", L"Европа Плюс" },
    { 1043, L"104.3", L"Авторадио", L"Авторадио" }, { 1074, L"107.4", L"Рок", L"Радио Рок" },
    { 883, L"88.3", L"Ретро", L"Ретро FM" },
    { 992, L"99.2", L"Вести FM", L"Вести FM" } };

static void RadioDock(HDC dc, bool listOn)
{
    Dock(dc);
    Button(dc, 16, 410, 96, 58, L"FM", B_SEL);
    Button(dc, 120, 410, 96, 58, L"AM", B_N);
    IconBtn(dc, IC_PREV, 268, 417, 44, kText);
    Button(dc, 344, 410, 112, 58, L"ПОИСК", B_N);
    IconBtn(dc, IC_FWD, 488, 417, 44, kText);
    if (listOn) {   // the list is open: its button is selected and closes it
        bool d = Zone(590, 410, 64, 58, A_BACK);
        R(dc, 590, 410, 64, 58, 10, d ? kOrange : kPanel2);
        Ico(dc, IC_LIST, 600, 417, 44, d ? kInk : kOrange);
    } else {
        IconBtn(dc, IC_LIST, 600, 417, 44, kText, A_SCREEN, S_RADIO_LIST);
    }
    IconBtn(dc, IC_GEAR, 712, 417, 44, kText, A_SCREEN, S_SET_RADIO);
}

// Radio, a main screen: the station panel, its controls in a row, eight presets.
static void ScrRadio(HDC dc)
{
    const Station& st = kPresets[s_seg[G_PRESET] < 7 ? s_seg[G_PRESET] : 2];
    Frame(dc, L"РАДИО");
    Panel(dc, 16, 64, 768, 128);
    T(dc, 40, 146, st.freq, 80, kText, 700, AL_START, 250);
    T(dc, 300, 100, L"FM 1", 20, kOrange, 700);
    T(dc, 300, 130, st.name, 28, kText, 600, AL_START, 340);
    T(dc, 300, 156, L"RDS · Хиты нон-стоп", 17, kMuted, 400);
    R(dc, 660, 82, 104, 30, 15, CLR_INVALID, kMuted);
    T(dc, 712, 103, L"СТЕРЕО", 15, kMuted, 600, AL_MID);
    for (int i = 0; i <= 20; i++) {   // tuning scale 87.5..108 over x 40..740
        int x = 40 + i * 35, h = i % 2 ? 5 : 8;
        Fill(dc, x - 1, 182 - h, x + 1, 182, kDim);
    }
    int mark = 40 + (st.f10 - 875) * 700 / 205;
    R(dc, mark - 2, 168, 4, 18, 2, kOrange);
    Button(dc, 16, 200, 96, 52, L"FM", B_SEL);
    Button(dc, 120, 200, 96, 52, L"AM", B_N);
    Button(dc, 232, 200, 80, 52, NULL, B_N, IC_PREV, 18, 30);
    Button(dc, 320, 200, 144, 52, L"ПОИСК", B_N, IC_SEARCH, 17);
    Button(dc, 472, 200, 80, 52, NULL, B_N, IC_FWD, 18, 30);
    Button(dc, 600, 200, 88, 52, NULL, B_N, IC_LIST, 18, 32, A_SCREEN, S_RADIO_LIST);
    Button(dc, 696, 200, 88, 52, NULL, B_N, IC_GEAR, 18, 32, A_SCREEN, S_SET_RADIO);
    for (int i = 0; i < 8; i++) {   // presets 4 x 2; the last slot is empty
        int x = 16 + (i % 4) * 194, y = 264 + (i / 4) * 72;
        bool down = Zone(x, y, 182, 64, i < 7 ? A_SEG : A_NONE, G_PRESET * 16 + i);
        if (i == 7) {
            if (down)
                R(dc, x, y, 182, 64, 12, kOrange);
            else
                DashRect(dc, x, y, 182, 64, 12, kLine);
            T(dc, x + 91, y + 40, L"ЗАПОМНИТЬ", 16, down ? kInk : kDim, 600, AL_MID);
            continue;
        }
        bool cur = down || s_seg[G_PRESET] == i;
        wchar_t n[4];
        _snwprintf(n, 4, L"%d", i + 1);
        R(dc, x, y, 182, 64, 12, cur ? kOrange : kPanel, cur ? CLR_INVALID : kLine);
        T(dc, x + 16, y + 24, n, 14, cur ? kInk : kDim, cur ? 700 : 600);
        T(dc, x + 16, y + 52, kPresets[i].freq, 24, cur ? kInk : kText, 700);
        T(dc, x + 166, y + 52, kPresets[i].shortName, 14, cur ? kInk : kMuted, cur ? 700 : 400, AL_END, 80);
    }
}

static void ScrRadioList(HDC dc)
{
    static const wchar_t* st[5][3] = { { L"87.9", L"Радио Шансон", L"RDS" }, { L"88.3", L"Ретро FM", NULL },
                                       { L"99.2", L"Вести FM", L"RDS" }, { L"101.7", L"Хит FM", L"RDS" },
                                       { L"106.2", L"Европа Плюс", L"RDS" } };
    Frame(dc, L"РАДИО · СПИСОК");
    Panel(dc, 16, 68, 600, 322);
    for (int i = 0; i < 5; i++) {   // a station is chosen: back to the radio
        int y = 76 + i * 62;
        bool cur = Zone(24, y, 568, 56, A_BACK) || i == 4;
        R(dc, 24, y, 568, 56, 10, cur ? kOrange : kPanel);
        T(dc, 40, y + 38, st[i][0], 26, cur ? kInk : kText, 700);
        T(dc, 148, y + 36, st[i][1], 19, cur ? kInk : kMuted, 700);
        if (st[i][2])
            T(dc, 576, y + 34, st[i][2], 16, cur ? kInk : kMuted, 600, AL_END);
    }
    Scrollbar(dc, 600, 80, 296, 10, 55);
    Button(dc, 632, 68, 152, 72, L"АВТО", B_N, IC_SEARCH, 17);
    Button(dc, 632, 150, 152, 72, L"PTY", B_N, IC_NONE, 18, 0, A_SCREEN, S_RADIO_PTY);
    R(dc, 632, 232, 152, 158, 12, kPanel);
    T(dc, 708, 280, L"НАЙДЕНО", 13, kMuted, 600, AL_MID);
    T(dc, 708, 330, L"18", 44, kText, 700, AL_MID);
    T(dc, 708, 362, L"СТАНЦИЙ", 13, kMuted, 600, AL_MID);
    RadioDock(dc, true);
}

static void ScrRadioPty(HDC dc)
{
    static const wchar_t* types[15] = { L"ВСЕ", L"НОВОСТИ", L"СОБЫТИЯ", L"ИНФОРМАЦИЯ", L"СПОРТ", L"ОБРАЗОВАНИЕ",
                                        L"ДРАМА", L"КУЛЬТУРА", L"НАУКА", L"РАЗНОЕ", L"ПОП", L"РОК", L"ЛЁГКАЯ",
                                        L"КЛАССИКА", L"ДРУГАЯ" };
    Frame(dc, L"РАДИО · ТИП ПРОГРАММЫ");
    for (int i = 0; i < 15; i++)
        Button(dc, 16 + (i % 3) * 260, 68 + (i / 3) * 64, 248, 56, types[i], i == 11 ? B_ACT : B_FILL, IC_NONE, 17);
    Dock(dc);
    Button(dc, 16, 410, 180, 58, L"ОТМЕНА", B_N, IC_NONE, 18, 0, A_BACK);
    T(dc, 400, 446, L"Ищет станции с типом «Рок»", 17, kMuted, 400, AL_MID, 390);
    Button(dc, 604, 410, 180, 58, L"ИСКАТЬ", B_PRI, IC_SEARCH, 18, 0, A_BACK);
}

// Media main screens: the source tabs on top, the player under them.
static void ScrAudio(HDC dc)
{
    Frame(dc, L"МЕДИА · USB");
    MediaTabs(dc, S_AUDIO, IC_LIST, A_SCREEN, S_AUDIO_LIST, S_SET_SOUND);
    Cover(dc, IC_MUSIC, 16, 124, 232);
    T(dc, 272, 150, L"ТРЕК 3 ИЗ 11", 14, kMuted, 600);
    T(dc, 272, 192, L"Группа крови", 38, kText, 700, AL_START, 512);
    T(dc, 272, 222, L"Кино · «Группа крови», 1988", 20, kMuted, 400, AL_START, 512);
    R(dc, 272, 248, 512, 8, 4, kLine);
    R(dc, 272, 248, 184, 8, 4, kOrange);
    Disc(dc, 456, 252, 11, kText);
    T(dc, 272, 282, L"1:42", 17, kMuted, 600);
    T(dc, 784, 282, L"4:46", 17, kMuted, 600, AL_END);
    IconBtn(dc, IC_SHUFFLE, 290, 316, 40, kDim);
    IconBtn(dc, IC_PREV, 392, 312, 48, kText);
    PauseBtn(dc, 528, 336, 42, 48);
    IconBtn(dc, IC_FWD, 616, 312, 48, kText);
    IconBtn(dc, IC_REPEAT, 726, 316, 40, kOrange);
}

static void ScrAudioList(HDC dc)
{
    static const Tab tabs[4] = { { IC_FOLDER, L"ПАПКИ", S_NONE }, { IC_USER, L"АРТИСТЫ", S_NONE },
                                 { IC_DISC, L"АЛЬБОМЫ", S_NONE }, { IC_LIST, L"ТРЕКИ", S_NONE } };
    static const wchar_t* tracks[4][3] = { { L"Группа крови", L"4:46", L"3" }, { L"Закрой за мной дверь", L"4:15", L"4" },
                                           { L"Кукушка", L"6:37", L"5" }, { L"Пачка сигарет", L"4:28", L"6" } };
    Frame(dc, L"АУДИО · USB · КИНО");
    SideTabs(dc, tabs, 4, 0, 82, 74);
    Panel(dc, 192, 68, 592, 322);
    ListRow(dc, 200, 76, 560, 56, L"Наверх · /Music/Кино", NULL, L_N, IC_FOLDER);
    for (int i = 0; i < 4; i++)   // a track is chosen: back to the player
        ListRow(dc, 200, 138 + i * 62, 560, 56, tracks[i][0], tracks[i][1], i == 0 ? L_CUR : L_N, IC_NONE, NULL,
                tracks[i][2], A_BACK);
    Ico(dc, IC_VOL, 646, 151, 30, kInk);
    Scrollbar(dc, 768, 80, 296, 0, 45);
    SourcesDock(dc, L"USB", S_AUDIO);
    IconBtn(dc, IC_MUSIC, 600, 417, 44, kText, A_BACK);
    IconBtn(dc, IC_GEAR, 712, 417, 44, kText, A_SCREEN, S_SET_SOUND);
}

static void ScrAudioBt(HDC dc)
{
    Frame(dc, L"МЕДИА · BLUETOOTH");
    MediaTabs(dc, S_AUDIO_BT, IC_PHONE, A_SCREEN, S_PHONE, S_SET_BT);
    Cover(dc, IC_BT, 16, 124, 232);
    T(dc, 272, 150, L"BLUETOOTH · REDMI NOTE", 14, kMuted, 600);
    T(dc, 272, 192, L"Звезда по имени Солнце", 34, kText, 700, AL_START, 512);
    T(dc, 272, 222, L"Кино · «Звезда по имени Солнце»", 20, kMuted, 400, AL_START, 512);
    R(dc, 272, 248, 512, 8, 4, kLine);
    R(dc, 272, 248, 128, 8, 4, kOrange);
    Disc(dc, 400, 252, 11, kText);
    T(dc, 272, 282, L"1:05", 17, kMuted);
    T(dc, 784, 282, L"3:45", 17, kMuted, 600, AL_END);
    IconBtn(dc, IC_PREV, 392, 312, 48, kText);
    PauseBtn(dc, 528, 336, 42, 48);
    IconBtn(dc, IC_FWD, 616, 312, 48, kText);
}

static void ScrAux(HDC dc)
{
    Frame(dc, L"МЕДИА · AUX");
    MediaTabs(dc, S_AUX, IC_NONE, A_NONE, 0, S_SET_SOUND);
    Cover(dc, IC_AUX, 16, 124, 232);
    T(dc, 272, 150, L"ВНЕШНИЙ ИСТОЧНИК", 14, kMuted, 600);
    T(dc, 272, 200, L"AUX", 52, kText, 700);
    T(dc, 272, 236, L"Звук идёт с разъёма 3,5 мм. Управляйте", 18, kMuted, 400, AL_START, 512);
    T(dc, 272, 260, L"воспроизведением на самом устройстве.", 18, kMuted, 400, AL_START, 512);
    T(dc, 272, 326, L"Уровень входа", 19, kText, 700);
    Stepper(dc, 560, 296, 224, L"+2", 52);
}

static void ScrVideo(HDC dc)
{
    static const Tab tabs[2] = { { IC_FOLDER, L"ФАЙЛЫ", S_NONE }, { IC_CLOCK, L"НЕДАВНИЕ", S_NONE } };
    static const wchar_t* vids[4][2] = { { L"DASHCAM_0410.MP4", L"2:58 · 180 МБ" }, { L"DASHCAM_0411.MP4", L"3:00 · 182 МБ" },
                                         { L"DASHCAM_0412.MP4", L"3:45 · 226 МБ" }, { L"ROAD_TRIP_SOCHI.AVI", L"41:20 · 1,4 ГБ" } };
    Frame(dc, L"МЕДИА · ВИДЕО · DASHCAM");
    MediaTabs(dc, S_VIDEO, IC_PLAY, A_SCREEN, S_VIDEO_PLAY, S_SET_VIDEO);
    SideTabs(dc, tabs, 2, 0, 70, 62, 124);
    Panel(dc, 192, 124, 592, 276);
    for (int i = 0; i < 4; i++) {
        int y = 132 + i * 66;
        bool cur = Zone(200, y, 560, 60, A_SCREEN, S_VIDEO_PLAY) || i == 2;
        R(dc, 200, y, 560, 60, 10, cur ? kOrange : kPanel);
        Road(dc, 208, y + 6, 88, 48);
        Ico(dc, IC_PLAY, 240, y + 18, 24, kText);
        T(dc, 312, y + 27, vids[i][0], 18, cur ? kInk : kText, 700, AL_START, 430);
        T(dc, 312, y + 49, vids[i][1], 15, cur ? kInk : kMuted, 400);
    }
    Scrollbar(dc, 768, 136, 252, 20, 50);
}

static void ScrVideoPlay(HDC dc)
{
    Frame(dc, L"ВИДЕО · DASHCAM_0412.MP4");
    Road(dc, 0, 56, 800, 424);
    Shade(0, 56, 800, 64, 230, 34);   // #fade-band at 90 %
    Shade(0, 330, 800, 150, 209, 209);   // control panel: graphite 82 % over the frame
    T(dc, 16, 368, L"1:12", 17, kMuted);
    T(dc, 784, 368, L"3:45", 17, kMuted, 600, AL_END);
    R(dc, 80, 360, 640, 8, 4, kLine);
    R(dc, 80, 360, 200, 8, 4, kOrange);
    Disc(dc, 280, 364, 11, kText);
    IconBtn(dc, IC_LIST, 24, 404, 44, kText, A_BACK);
    IconBtn(dc, IC_PREV, 228, 400, 48, kText);
    PauseBtn(dc, 400, 424, 40, 44);
    IconBtn(dc, IC_FWD, 524, 400, 48, kText);
    IconBtn(dc, IC_REPEAT, 630, 404, 40, kOrange);
    IconBtn(dc, IC_GEAR, 726, 402, 44, kText, A_SCREEN, S_SET_VIDEO);
}

static void PhotoDock(HDC dc, Screen prev, Screen next)
{
    Dock(dc);
    IconBtn(dc, IC_APPS, 24, 417, 44, kText, A_BACK);
    IconBtn(dc, IC_PREV, 132, 417, 44, kText, prev == S_NONE ? A_NONE : A_TAB, prev);
    IconBtn(dc, IC_ROT_L, 244, 417, 44, kText);
    bool d = Zone(330, 410, 140, 58, A_NONE);   // slide show running: selected
    R(dc, 330, 410, 140, 58, 10, d ? kOrange : kPanel2);
    Ico(dc, IC_PLAY, 378, 417, 44, d ? kInk : kOrange);
    IconBtn(dc, IC_ROT_R, 512, 417, 44, kText);
    IconBtn(dc, IC_FWD, 624, 417, 44, kText, next == S_NONE ? A_NONE : A_TAB, next);
    IconBtn(dc, IC_GEAR, 732, 417, 44, kText, A_SCREEN, S_SET_PHOTO);
}

static void ScrPhoto(HDC dc)
{
    Frame(dc, L"МЕДИА · ФОТО · ПУТЕШЕСТВИЯ");
    MediaTabs(dc, S_PHOTO, IC_PLAY, A_SCREEN, S_PHOTO_VIEW, S_SET_PHOTO);
    for (int i = 0; i < 8; i++) {
        int x = 16 + (i % 4) * 194, y = 124 + (i / 4) * 138;
        bool down = Zone(x, y, 184, 128, i ? A_SCREEN : A_NONE, S_PHOTO_VIEW);
        if (i == 0) {   // a folder
            R(dc, x, y, 184, 128, 12, down ? kOrange : kPanel, down ? CLR_INVALID : kLine);
            Ico(dc, IC_FOLDER, x + 64, y + 22, 56, down ? kInk : kMuted);
            T(dc, x + 92, y + 112, L"КАМЕРА", 15, down ? kInk : kMuted, 600, AL_MID);
            continue;
        }
        bool cur = down || i == 3;
        wchar_t name[16];
        _snwprintf(name, 16, L"IMG_07%02d.JPG", 10 + i);
        R(dc, x, y, 184, 128, 12, cur ? kOrange : kPanel);
        Landscape(dc, x + 6, y + 6, 172, 92);
        T(dc, x + 92, y + 118, name, 14, cur ? kInk : kMuted, 700, AL_MID);
    }
}

static void ScrPhotoView(HDC dc)
{
    Frame(dc, L"ФОТО · 3 / 24");
    R(dc, 0, 56, 800, 342, 0, kMargin);
    Landscape(dc, 172, 56, 456, 342);
    PhotoDock(dc, S_NONE, S_PHOTO_BAD);
}

static void ScrPhotoBad(HDC dc)
{
    Frame(dc, L"ФОТО · 4 / 24");
    R(dc, 0, 56, 800, 342, 0, kMargin);
    Panel(dc, 220, 110, 360, 230);
    Ico(dc, IC_PHOTO, 360, 140, 80, kOff);
    Line(dc, 352, 140, 448, 220, 4, kMuted);
    T(dc, 400, 262, L"ФОРМАТ НЕ ПОДДЕРЖИВАЕТСЯ", 18, kText, 700, AL_MID, 336);
    T(dc, 400, 292, L"DACHA_2026.HEIC", 17, kMuted, 400, AL_MID);
    T(dc, 400, 318, L"Пропустить можно стрелками внизу", 15, kDim, 400, AL_MID, 336);
    PhotoDock(dc, S_PHOTO_VIEW, S_NONE);
}

// Phone, a main screen: the number, recent calls and the way to the contacts on the left, the keypad on the right.
static void ScrPhone(HDC dc)
{
    static const wchar_t* keys = L"123456789*0#";
    static const wchar_t* recent[3][2] = { { L"Мама", L"12:05" }, { L"Сервис Лада", L"вчера" },
                                           { L"+7 495 000-11-22", L"пн" } };
    Frame(dc, L"ТЕЛЕФОН · Redmi Note");
    Panel(dc, 16, 64, 356, 72, 12);
    int end = T(dc, 32, 112, s_dial, 28, kText, 700, AL_START, 270);
    R(dc, end + 4, 86, 3, 28, 0, kOrange);
    IconBtn(dc, IC_BKSP, 320, 81, 38, kMuted, A_DEL, K_DIAL);
    T(dc, 16, 162, L"НЕДАВНИЕ", 14, kMuted, 600);
    for (int i = 0; i < 3; i++) {   // demo: the recent calls open the call log
        int y = 172 + i * 60;
        bool d = Zone(16, y, 356, 54, A_SCREEN, S_CALLS);
        R(dc, 16, y, 356, 54, 10, d ? kOrange : kPanel);
        T(dc, 32, y + 33, recent[i][0], 18, d ? kInk : i == 2 ? kStop : kText, 700, AL_START, 240);
        T(dc, 356, y + 33, recent[i][1], 18, d ? kInk : kMuted, 400, AL_END);
    }
    Button(dc, 16, 354, 356, 44, L"КОНТАКТЫ", B_N, IC_USERS, 16, 26, A_SCREEN, S_CONTACTS);
    for (int i = 0; i < 12; i++) {   // keypad 3 x 4
        int x = 388 + (i % 3) * 136, y = 64 + (i / 3) * 62;
        wchar_t k[2] = { keys[i], 0 };
        bool d = Zone(x, y, 124, 56, A_KEY, K_DIAL * 65536 + keys[i]);
        R(dc, x, y, 124, 56, 10, d ? kOrange : kPanel, d ? CLR_INVALID : kLine);
        T(dc, x + 62, y + 39, k, 30, d ? kInk : kText, 700, AL_MID);
    }
    bool d = Zone(388, 316, 192, 82, A_SCREEN, S_CALL);
    R(dc, 388, 316, 192, 82, 12, d ? Mix(kOk, kInk, 160) : kOk);
    Ico(dc, IC_CALL, 462, 335, 44, kInk);
    d = Zone(592, 316, 192, 82, A_NONE);
    R(dc, 592, 316, 192, 82, 12, d ? Mix(kStop, kInk, 160) : kStop);
    Ico(dc, IC_HANGUP, 666, 335, 44, kInk);
}

struct Person { const wchar_t* name; const wchar_t* number; const wchar_t* initials; };

static void ScrContacts(HDC dc)
{
    static const Person people[5] = { { L"Мама", L"+7 912 000-12-34", L"М" },
                                      { L"Матвей Никифоров", L"+7 917 123-45-67", L"МН" },
                                      { L"Мария Никифорова", L"+7 927 765-43-21", L"МН" },
                                      { L"Сервис Лада", L"+7 8482 55-12-00", L"СЛ" },
                                      { L"Сева", L"+7 903 111-22-33", L"С" } };
    static const wchar_t* letters = L"АБВГДЕЖЗИКЛМНОПРСТУФХЦЧШЭЮЯ";
    Frame(dc, L"ТЕЛЕФОН · КОНТАКТЫ");
    PhoneNav(dc, 1);
    R(dc, 192, 68, 560, 56, 10, kPanel, kLine);
    Ico(dc, IC_SEARCH, 206, 80, 32, kMuted);
    T(dc, 250, 104, L"Поиск по имени или номеру", 18, kDim, 400);
    for (int i = 0; i < 5; i++) {
        int y = 136 + i * 64;
        bool d = Zone(192, y, 560, 58, A_SCREEN, S_CONTACT);
        R(dc, 192, y, 560, 58, 10, d ? kOrange : kPanel);
        Avatar(dc, 222, y + 29, 19, people[i].initials, 14);
        T(dc, 256, y + 25, people[i].name, 19, d ? kInk : kText, 700);
        T(dc, 256, y + 47, people[i].number, 15, d ? kInk : kMuted, 400);
        Ico(dc, i == 0 ? IC_STAR_F : IC_NEXT, 708, y + 13, 32, d ? kInk : i == 0 ? kOrange : kDim);
    }
    for (int i = 0; letters[i]; i++) {   // letter bar: the one under the finger is highlighted
        wchar_t ch[2] = { letters[i], 0 };
        bool on = letters[i] == L'М';
        if (on)
            Disc(dc, 772, 136 + i * 23 / 2, 9, kOrange);
        T(dc, 772, 140 + i * 23 / 2, ch, 10, on ? kInk : kDim, 700, AL_MID);
    }
}

static void ScrContact(HDC dc)
{
    static const wchar_t* nums[3][2] = { { L"МОБИЛЬНЫЙ", L"+7 917 123-45-67" }, { L"РАБОЧИЙ", L"+7 8482 55-12-00" },
                                         { L"ДОМАШНИЙ", L"+7 8482 31-44-90" } };
    Frame(dc, L"ТЕЛЕФОН · КОНТАКТ");
    PhoneNav(dc, 1);
    Panel(dc, 192, 68, 592, 322);
    AvatarOr(dc, 260, 136, 52, L"МН");
    T(dc, 332, 128, L"Матвей Никифоров", 28, kText, 700, AL_START, 440);
    Ico(dc, IC_STAR_F, 332, 144, 26, kOrange);
    T(dc, 366, 164, L"В избранном", 16, kMuted, 400);
    for (int i = 0; i < 3; i++) {
        int y = 204 + i * 62;
        R(dc, 208, y, 560, 56, 10, kPanel2);
        T(dc, 224, y + 22, nums[i][0], 12, kMuted, 700);
        T(dc, 224, y + 45, nums[i][1], 20, kText, 700);
        bool d = Zone(688, y + 6, 72, 44, A_SCREEN, S_CALL);
        R(dc, 688, y + 6, 72, 44, 10, d ? Mix(kOk, kInk, 160) : kOk);
        Ico(dc, IC_CALL, 708, y + 10, 32, kInk);
    }
}

static void ScrCalls(HDC dc)
{
    static const wchar_t* filters[4] = { L"ВСЕ", L"ВХОДЯЩИЕ", L"ИСХОДЯЩИЕ", L"ПРОПУЩ." };
    static const Icon icons[5] = { IC_CALL_IN, IC_CALL_MISS, IC_CALL_OUT, IC_CALL_OUT, IC_CALL_IN };
    static const wchar_t* calls[5][2] = { { L"Мама", L"12:05 · 3 мин" }, { L"+7 495 000-11-22", L"11:40" },
                                          { L"Сервис Лада", L"вчера · 1 мин" }, { L"Матвей Никифоров", L"вчера · 12 мин" },
                                          { L"Мария Никифорова", L"пн · 5 мин" } };
    Frame(dc, L"ТЕЛЕФОН · ЖУРНАЛ");
    PhoneNav(dc, 3);
    Seg(dc, 192, 68, 592, 52, filters, 4, G_CALLS, 15);
    for (int i = 0; i < 5; i++) {
        int y = 132 + i * 66;
        R(dc, 192, y, 592, 60, 10, kPanel);
        Ico(dc, icons[i], 206, y + 14, 32, i == 1 ? kStop : kMuted);
        T(dc, 254, y + 27, calls[i][0], 19, kText, 700);
        T(dc, 254, y + 49, calls[i][1], 15, kMuted, 400);
        if (i == 1) {   // missed twice
            R(dc, 560, y + 18, 34, 24, 12, kStop);
            T(dc, 577, y + 36, L"2", 15, kText, 700, AL_MID);
        }
        bool d = Zone(704, y + 6, 72, 48, A_SCREEN, S_CALL);
        R(dc, 704, y + 6, 72, 48, 10, d ? kOrange : kPanel2);
        Ico(dc, IC_CALL, 724, y + 14, 32, d ? kInk : kOk);
    }
}

static void ScrCall(HDC dc)
{
    static const Icon icons[3] = { IC_MIC_OFF, IC_KEYPAD, IC_HANDOVER };
    static const wchar_t* labels[3] = { L"МИКРОФОН", L"НАБОР", L"НА ТЕЛЕФОН" };
    Frame(dc, L"ТЕЛЕФОН");
    AvatarOr(dc, 128, 168, 88, L"МН");
    T(dc, 256, 112, L"РАЗГОВОР", 14, kMuted, 700);
    T(dc, 256, 160, L"Матвей Никифоров", 40, kText, 700, AL_START, 528);
    T(dc, 256, 196, L"+7 917 123-45-67 · мобильный", 20, kMuted, 400, AL_START, 528);
    DWORD sec = (GetTickCount() - s_callTick) / 1000;
    wchar_t s[16];
    _snwprintf(s, 16, L"%02u:%02u", (unsigned)(sec / 60), (unsigned)(sec % 60));
    T(dc, 256, 246, s, 34, kText, 700);
    for (int i = 0; i < 3; i++) {   // microphone off = the pressed state
        int x = 16 + i * 136;
        bool d = Zone(x, 296, 124, 96, i == 0 ? A_TOGGLE : A_NONE, T_MUTED);
        bool act = i == 0 ? s_on[T_MUTED] != d : d;
        R(dc, x, 296, 124, 96, 12, act ? kOrange : kPanel, act ? CLR_INVALID : kLine);
        Ico(dc, icons[i], x + 42, 308, 40, act ? kInk : kText);
        T(dc, x + 62, 374, labels[i], 13, act ? kInk : kMuted, 700, AL_MID, 116);
    }
    bool d = Zone(424, 296, 360, 96, A_BACK);
    R(dc, 424, 296, 360, 96, 12, d ? Mix(kStop, kInk, 160) : kStop);
    Ico(dc, IC_HANGUP, 580, 320, 48, kInk);
    Dock(dc);   // in a call the volume is the main control
    IconBtn(dc, IC_VOL, 24, 417, 44, kMuted, A_SCREEN, S_VOLUME);
    Slider(dc, 88, 435, 560, 45, NULL);
    T(dc, 700, 448, L"12", 22, kText, 700, AL_MID);
}

static void ScrPhoneOff(HDC dc)
{
    Frame(dc, L"ТЕЛЕФОН");
    Disc(dc, 400, 140, 56, kPanel);
    Ico(dc, IC_BT, 368, 108, 64, kMuted);
    T(dc, 400, 236, L"ТЕЛЕФОН НЕ ПОДКЛЮЧЁН", 22, kText, 700, AL_MID, 760);
    T(dc, 400, 268, L"Включите Bluetooth на телефоне и найдите устройство «VESTA»", 18, kMuted, 400, AL_MID, 760);
    R(dc, 192, 296, 416, 64, 10, kPanel);
    Avatar(dc, 226, 328, 18, L"R", 14);
    T(dc, 258, 322, L"Redmi Note", 19, kText, 700);
    T(dc, 258, 344, L"подключался вчера", 15, kMuted, 400);
    Button(dc, 470, 304, 130, 48, L"ПОДКЛЮЧИТЬ", B_PRI, IC_NONE, 14, 0, A_TAB, S_PHONE);
    Dock(dc);
    Button(dc, 16, 410, 300, 58, L"НАСТРОЙКИ BLUETOOTH", B_N, IC_GEAR, 16, 0, A_SCREEN, S_SET_BT);
}

static int TzMinutes();
static void GpsPortText(wchar_t* s, int n);
static void GpsBaudText(wchar_t* s, int n);
static void GpsStatusText(wchar_t* s, int n, bool brief = false);

// Seven rows of 48 px: the GPS port and its speed are set here for the whole shell (stepped like in the
// navigation programs: COM1..COM9, the speed list with "leave as is").
static void ScrSetGeneral(HDC dc)
{
    static const wchar_t* fmt[2] = { L"12", L"24" };
    enum { H = 48 };
    SYSTEMTIME t;
    GetLocalTime(&t);
    wchar_t now[40], time[8], tz[16], port[16], baud[16], gps[160];
    Now(time, 8);
    _snwprintf(now, 40, L"сейчас %02d.%02d.%04d  %s", t.wDay, t.wMonth, t.wYear, time);
    int m = TzMinutes();
    if (m % 60)
        _snwprintf(tz, 16, L"UTC%c%d:%02d", m < 0 ? L'-' : L'+', abs(m) / 60, abs(m) % 60);
    else
        _snwprintf(tz, 16, L"UTC%c%d", m < 0 ? L'-' : L'+', abs(m) / 60);
    GpsPortText(port, 16);
    GpsBaudText(baud, 16);
    GpsStatusText(gps, 160, true);
    Settings(dc, S_SETTINGS);
    SetRow(dc, 64, L"Часовой пояс", now, false, A_NONE, 0, H);
    Stepper(dc, 512, 68, 248, tz, 40, 19, A_TZ);
    SetRow(dc, 112, L"Синхронизация по GPS", NULL, false, A_NONE, 0, H);
    Toggle(dc, 688, 118, T_GPS_TIME);
    SetRow(dc, 160, L"Порт GPS", gps, false, A_NONE, 0, H);
    Stepper(dc, 512, 164, 248, port, 40, 19, A_GPSPORT);
    SetRow(dc, 208, L"Скорость порта", L"или как у оболочки", false, A_NONE, 0, H);
    Stepper(dc, 512, 212, 248, baud, 40, 19, A_GPSBAUD);
    SetRow(dc, 256, L"Формат времени", NULL, false, A_NONE, 0, H);
    Seg(dc, 560, 260, 200, 40, fmt, 2, G_TIME_FMT);
    SetRow(dc, 304, L"Язык", NULL, false, A_NONE, 0, H);
    T(dc, 720, 335, L"Русский", 18, kMuted, 400, AL_END);
    Chevron(dc, 728, 312);
    SetRow(dc, 352, L"Заводские настройки", NULL, true, A_NONE, 0, H);
    Button(dc, 616, 356, 144, 40, L"СБРОС", B_N, IC_NONE, 16);
}

static void ScrSetSound(HDC dc)
{
    static const wchar_t* presets[6] = { L"СВОЙ", L"ПОП", L"РОК", L"ДЖАЗ", L"КЛАСС.", L"ГОЛОС" };
    static const wchar_t* bands[5][2] = { { L"60", L"+4" }, { L"250", L"+2" }, { L"1К", L"-1" }, { L"4К", L"+3" },
                                          { L"12К", L"+6" } };
    static const int level[5] = { 62, 55, 45, 58, 70 };
    Settings(dc, S_SET_SOUND);
    T(dc, 264, 96, L"Эквалайзер", 19, kText, 700);
    Seg(dc, 256, 106, 512, 44, presets, 6, G_EQ, 14);
    for (int i = 0; i < 5; i++)
        VSlider(dc, 300 + i * 80, 188, 126, level[i], bands[i][0], bands[i][1]);
    bool d = Zone(696, 172, 64, 158, A_SCREEN, S_SET_BALANCE);   // the second page: balance and fader
    R(dc, 696, 172, 64, 158, 10, d ? kOrange : kPanel2);
    Ico(dc, IC_EQ, 704, 227, 48, d ? kInk : kMuted);
    Sep(dc, 256, 350, 512);
    T(dc, 264, 384, L"Тонкомпенсация", 19, kText, 700);
    Toggle(dc, 688, 358, T_LOUDNESS);
}

static void ScrSetBalance(HDC dc)
{
    static const wchar_t* speed[4] = { L"ВЫКЛ", L"1", L"2", L"3" };
    Settings(dc, S_SET_BALANCE);
    R(dc, 276, 76, 200, 316, 40, kPanel2);   // the car from above, four seats
    R(dc, 296, 126, 70, 76, 10, kInk);
    R(dc, 386, 126, 70, 76, 10, kInk);
    R(dc, 296, 256, 70, 76, 10, kInk);
    R(dc, 386, 256, 70, 76, 10, kInk);
    Dash(dc, 376, 84, 376, 384, 4, 6, kDim);
    Dash(dc, 284, 230, 468, 230, 4, 6, kDim);
    Disc(dc, 396, 204, 18, kOrange);
    Disc(dc, 396, 204, 6, kInk);
    T(dc, 504, 96, L"Баланс", 19, kText, 700);
    Stepper(dc, 504, 108, 256, L"П 2", 48);
    T(dc, 504, 184, L"Фейдер", 19, kText, 700);
    Stepper(dc, 504, 196, 256, L"ПЕРЕД 3", 48);
    T(dc, 504, 272, L"Громкость от скорости", 19, kText, 700, AL_START, 260);
    Seg(dc, 504, 284, 256, 44, speed, 4, G_SPEED_VOL, 15);
    Button(dc, 504, 344, 256, 48, L"В ЦЕНТР", B_N, IC_NONE, 16);
}

static void ScrSetScreen(HDC dc)
{
    static const wchar_t* night[3] = { L"АВТО", L"ВКЛ", L"ВЫКЛ" };
    Settings(dc, S_SET_SCREEN);
    T(dc, 264, 96, L"Подсветка днём", 19, kText, 700);
    Slider(dc, 264, 112, 432, 80, L"80");
    Sep(dc, 256, 142, 512);
    T(dc, 264, 176, L"Подсветка ночью", 19, kText, 700);
    Slider(dc, 264, 192, 432, 30, L"30");
    Sep(dc, 256, 222, 512);
    T(dc, 264, 262, L"Ночной режим", 19, kText, 700);
    Seg(dc, 480, 236, 288, 44, night, 3, G_NIGHT);
    Sep(dc, 256, 286, 512);
    SetRow(dc, 288, L"Заставка с часами", NULL, false, A_SCREEN, S_SAVER);   // demo: opens the screen saver
    Toggle(dc, 696, 298, T_SAVER);
    SetRow(dc, 344, L"Калибровка экрана", NULL, true);
    Chevron(dc, 736, 356);
}

static void ScrSetRadio(HDC dc)
{
    static const wchar_t* rows[6] = { L"Шумоподавление", L"Диапазон СВ", L"RDS", L"AF · альтернативные частоты",
                                      L"CT · время по радио", L"TA · дорожные сообщения" };
    static const int ids[6] = { T_NOISE, T_MW, T_RDS, T_AF, T_CT, T_TA };
    Settings(dc, S_SET_RADIO);
    for (int i = 0; i < 6; i++) {
        SetRow(dc, 64 + i * 56, rows[i], NULL, i == 5);
        Toggle(dc, 688, 74 + i * 56, ids[i]);
    }
}

static void ScrSetBt(HDC dc)
{
    Settings(dc, S_SET_BT);
    SetRow(dc, 64, L"Имя устройства");
    T(dc, 720, 99, L"VESTA", 18, kMuted, 700, AL_END);
    Chevron(dc, 728, 76);
    SetRow(dc, 120, L"Bluetooth");
    Toggle(dc, 688, 130, T_BT);
    SetRow(dc, 176, L"Подключено к", L"Redmi Note");
    Button(dc, 560, 182, 200, 44, L"ОТКЛЮЧИТЬ", B_N, IC_POWER, 15, 0, A_SCREEN, S_PHONE_OFF);
    SetRow(dc, 232, L"Автосоединение");
    Toggle(dc, 688, 242, T_AUTOCONNECT);
    SetRow(dc, 288, L"Автоответ");
    Toggle(dc, 688, 298, T_AUTOANSWER);
    SetRow(dc, 344, L"Телефонная книга", L"обновлена вчера, 412 контактов", true);
    Button(dc, 600, 350, 160, 44, L"ОБНОВИТЬ", B_N, IC_REFRESH, 15);
}

static void ScrSetWifi(HDC dc)
{
    static const wchar_t* nets[4] = { L"Vesta_Garage", L"Keenetic-4213", L"MTS_Router_5G", L"AndroidAP" };
    static const bool locked[4] = { true, true, false, true };
    Settings(dc, S_SET_WIFI);
    SetRow(dc, 64, L"Wi-Fi", NULL, !s_on[T_WIFI]);
    Toggle(dc, 688, 74, T_WIFI);
    if (!s_on[T_WIFI])
        return;
    for (int i = 0; i < 4; i++) {   // a network asks for its password
        int y = 128 + i * 68;
        bool cur = Zone(256, y, 512, 62, A_SCREEN, S_WIFI_KEYS) || i == 0;
        R(dc, 256, y, 512, 62, 10, cur ? kOrange : kPanel2);
        Ico(dc, IC_WIFI, 272, y + 13, 36, cur ? kInk : kMuted);
        T(dc, 324, y + (i == 0 ? 28 : 38), nets[i], 19, cur ? kInk : kText, 700);
        if (i == 0)
            T(dc, 324, y + 50, L"подключено", 15, kInk, 400);
        if (locked[i])
            Ico(dc, IC_LOCK, 716, y + 17, 28, cur ? kInk : kDim);
    }
}

static void ScrWifiKeys(HDC dc)
{
    static const wchar_t* rows[4] = { L"1234567890", L"qwertyuiop", L"asdfghjkl-", L"zxcvbnm_.@" };
    Frame(dc, L"WI-FI · VESTA_GARAGE");
    R(dc, 16, 68, 600, 60, 10, kPanel, kLine);
    int end = T(dc, 32, 106, s_pass, 26, kText, 700, AL_START, 520);
    R(dc, end + 4, 84, 3, 28, 0, kOrange);
    IconBtn(dc, IC_EYE, 566, 82, 32, kMuted);
    Button(dc, 628, 68, 156, 60, L"ГОТОВО", B_PRI, IC_NONE, 18, 0, A_BACK);
    for (int r = 0; r < 4; r++)
        for (int c = 0; rows[r][c]; c++) {
            int x = 13 + c * 78, y = 140 + r * 66;
            wchar_t ch[2] = { rows[r][c], 0 };
            bool d = Zone(x, y, 72, 60, A_KEY, K_PASS * 65536 + rows[r][c]);
            R(dc, x, y, 72, 60, 10, d ? kOrange : kPanel, d ? CLR_INVALID : kLine);
            T(dc, x + 36, y + 40, ch, 26, d ? kInk : kText, 700, AL_MID);
        }
    Button(dc, 13, 408, 150, 60, NULL, B_FILL, IC_SHIFT, 18, 32);
    Button(dc, 169, 408, 150, 60, L"АБВ", B_FILL, IC_NONE, 20);
    bool d = Zone(325, 408, 306, 60, A_KEY, K_PASS * 65536 + L' ');
    R(dc, 325, 408, 306, 60, 10, d ? kOrange : kPanel, d ? CLR_INVALID : kLine);
    R(dc, 410, 442, 136, 4, 2, d ? kInk : kMuted);
    Button(dc, 637, 408, 150, 60, NULL, B_FILL, IC_BKSP, 18, 36, A_DEL, K_PASS);
}

static void ScrSetVideo(HDC dc)
{
    static const wchar_t* scale[2] = { L"ОБЫЧНЫЙ", L"АВТО" };
    static const wchar_t* format[2] = { L"16:9", L"4:3" };
    Settings(dc, S_SET_VIDEO);
    SetRow(dc, 64, L"Масштаб");
    Seg(dc, 512, 70, 248, 44, scale, 2, G_VIDEO_SCALE, 15);
    SetRow(dc, 120, L"Формат");
    Seg(dc, 512, 126, 248, 44, format, 2, G_VIDEO_FMT, 16);
    SetRow(dc, 176, L"Видео в движении", L"по ПДД — выключено");
    Toggle(dc, 688, 186, T_VIDEO_MOVING);
    SetRow(dc, 232, L"Субтитры", NULL, true);
    Toggle(dc, 688, 242, T_SUBTITLES);
}

static void ScrSetPhoto(HDC dc)
{
    static const wchar_t* scale[2] = { L"ВПИСАТЬ", L"ЗАПОЛН." };
    Settings(dc, S_SET_PHOTO);
    SetRow(dc, 64, L"Интервал слайд-шоу");
    Stepper(dc, 560, 68, 200, L"5 С", 48, 18);
    SetRow(dc, 120, L"Масштаб");
    Seg(dc, 512, 126, 248, 44, scale, 2, G_PHOTO_SCALE, 15);
    SetRow(dc, 176, L"Слайд-шоу при открытии");
    Toggle(dc, 688, 186, T_SLIDES_OPEN);
    SetRow(dc, 232, L"Автоповорот", NULL, true);
    Toggle(dc, 688, 242, T_AUTOROTATE);
}

// "About the system", the last section of the settings.
static void ScrInfo(HDC dc)
{
    static const wchar_t* rows[5][2] = { { L"Модель", L"LADA Vesta · MMC" }, { L"Версия ПО", L"G3 4120" },
                                         { L"ОС", L"Windows CE 6.0" }, { L"Bluetooth", L"00:1A:7D:DA:71:13" },
                                         { L"VestaHome", L"0.5 · 2026-10-12" } };
    Settings(dc, S_INFO);
    for (int i = 0; i < 5; i++) {
        int y = 64 + i * 50;
        T(dc, 264, y + 32, rows[i][0], 17, kMuted, 400);
        T(dc, 760, y + 32, rows[i][1], 17, kText, 700, AL_END, 330);
        Sep(dc, 256, y + 48, 512);
    }
    T(dc, 264, 340, L"SD-карта", 17, kMuted, 400);
    R(dc, 372, 330, 200, 10, 5, kLine);
    R(dc, 372, 330, 152, 10, 5, kMuted);
    T(dc, 760, 340, L"11,2 из 14,8 ГБ", 17, kText, 700, AL_END);
    Button(dc, 256, 352, 248, 44, L"ОБНОВЛЕНИЕ ПО", B_N, IC_REFRESH, 15);
    // demo: "Licences" shows the start-up warning (the same kind of legal text)
    Button(dc, 520, 352, 248, 44, L"ЛИЦЕНЗИИ", B_N, IC_INFO, 15, 0, A_SCREEN, S_WARNING);
}

// ---- diagnostics: the stock shell hides its engineering menu (CEngMainMenuDlg: CAN/UART, speakers, keys, fan,
// GPS, Wi-Fi MAC, touch, TFT calibration...) behind the four corners of its "Info" window; here it is a settings
// section. The tests that need the MCU protocol are grey until it is known.

static int s_testStep;            // the screen test's current fill
enum { kTestSteps = 7, kTouchMax = 256 };
static POINT s_touch[kTouchMax];  // the touch test's trail, oldest first
static int   s_touchCount;

struct Test { const wchar_t* label; Action a; int arg; };
static const Test kTests[8] = { { L"ЭКРАН", A_SCREEN, S_TEST_SCREEN }, { L"КАСАНИЯ", A_SCREEN, S_TEST_TOUCH },
                                { L"КАЛИБРОВКА", A_CALIBRATE, 0 }, { L"ПАМЯТЬ И ДИСКИ", A_SCREEN, S_TEST_MEM },
                                { L"КНОПКИ", A_SOON, 0 }, { L"ДИНАМИКИ", A_SOON, 0 }, { L"CAN / UART", A_SOON, 0 },
                                { L"GPS", A_SOON, 1 } };

// Diagnostics: the tests 2 x 4, then closing and minimizing VestaHome.
static void ScrDiag(HDC dc)
{
    Settings(dc, S_DIAG);
    for (int i = 0; i < 8; i++)
        Button(dc, 256 + (i % 2) * 264, 76 + (i / 2) * 60, 248, 52, kTests[i].label,
               kTests[i].a == A_SOON ? B_DIS : B_N, IC_NONE, 16, 0, kTests[i].a, kTests[i].arg);
    T(dc, 264, 332, L"Серые тесты ждут протокола MCU (GPS — чтения NMEA)", 14, kDim, 400, AL_START, 504);
    Sep(dc, 256, 342, 512);
    Button(dc, 256, 352, 248, 44, L"ЗАВЕРШИТЬ VH", B_N, IC_POWER, 15, 0, A_SCREEN, S_EXIT);
    Button(dc, 520, 352, 248, 44, L"СВЕРНУТЬ VH", B_N, IC_NONE, 15, 0, A_MINIMIZE);
}

// Screen test: full-screen white, red, green, blue, black (dead pixels, uniformity), a grey ramp (banding) and a
// 40 px grid (geometry); the colours are pure, night doesn't dim them. A tap shows the next one, after the last
// one diagnostics come back.
static void ScrTestScreen(HDC dc)
{
    static const COLORREF fills[5] = { RGB(255, 255, 255), RGB(255, 0, 0), RGB(0, 255, 0), RGB(0, 0, 255),
                                       RGB(0, 0, 0) };
    Zone(0, 0, 800, 480, A_TEST);
#ifndef UNDER_CE
    GdiFlush();
#endif
    for (int y = 0; y < 480; y++) {
        unsigned short* row = s_backBits + (479 - y) * 800;
        for (int x = 0; x < 800; x++) {
            int v;
            if (s_testStep < 5) {
                COLORREF c = fills[s_testStep];
                row[x] = RGB565(GetRValue(c), GetGValue(c), GetBValue(c));
                continue;
            }
            if (s_testStep == 5)
                v = x * 255 / 799;
            else
                v = x % 40 == 0 || y % 40 == 0 || x == 799 || y == 479 ? 255 : 0;
            row[x] = RGB565(v, v, v);
        }
    }
    if (s_testStep == 0) {   // on the first fill only: the hint would hide pixels of the others
        wchar_t s[48];
        _snwprintf(s, 48, L"Нажмите — следующий (всего %d)", kTestSteps);
        T(dc, 400, 300, s, 18, kInk, 600, AL_MID);
    }
}

// Touch test: every point the finger passes is marked, the last one with a crosshair and its coordinates.
static void ScrTestTouch(HDC dc)
{
    Fill(dc, 0, 0, 800, 480, kInk);
    for (int x = 0; x < 800; x += 80)
        Fill(dc, x, 0, x + 1, 480, kPanel2);
    for (int y = 0; y < 480; y += 80)
        Fill(dc, 0, y, 800, y + 1, kPanel2);
    for (int i = 0; i < s_touchCount; i++)
        Fill(dc, s_touch[i].x - 2, s_touch[i].y - 2, s_touch[i].x + 3, s_touch[i].y + 3, kOrange);
    if (s_touchCount) {
        const POINT& p = s_touch[s_touchCount - 1];
        Fill(dc, 0, p.y, 800, p.y + 1, kText);
        Fill(dc, p.x, 0, p.x + 1, 480, kText);
        wchar_t s[48];
        _snwprintf(s, 48, L"x %d · y %d · точек %d", p.x, p.y, s_touchCount);
        T(dc, 16, 36, s, 18, kText, 700);
    } else {
        T(dc, 16, 36, L"Проведите пальцем по экрану, углы и края тоже", 18, kMuted, 400);
    }
    Button(dc, 680, 8, 112, 48, L"ВЫХОД", B_FILL, IC_NONE, 16, 0, A_BACK);
}

static void AddTouch(int x, int y)
{
    if (s_touchCount == kTouchMax) {
        memmove(s_touch, s_touch + 1, (kTouchMax - 1) * sizeof(POINT));
        s_touchCount--;
    }
    s_touch[s_touchCount].x = x;
    s_touch[s_touchCount].y = y;
    s_touchCount++;
}

// "123 МБ" or "12,3 ГБ".
static void Size(wchar_t* s, int n, unsigned __int64 bytes)
{
    unsigned mb = (unsigned)(bytes / (1024 * 1024));
    if (mb < 2048)
        _snwprintf(s, n, L"%u МБ", mb);
    else
        _snwprintf(s, n, L"%u,%u ГБ", mb / 1024, mb % 1024 * 10 / 1024);
}

// Memory and volumes: free RAM, uptime, free space of the unit's volumes (absent ones say so).
static void ScrTestMem(HDC dc)
{
#ifdef UNDER_CE
    static const wchar_t* vols[5] = { L"\\SDMMC", L"\\ResidentFlash", L"\\StorageFlash", L"\\SDMemory",
                                      L"\\USBDisk" };
#else
    static const wchar_t* vols[5] = { L"C:\\", L"D:\\", L"E:\\", L"F:\\", L"G:\\" };
#endif
    Frame(dc, L"ДИАГНОСТИКА · ПАМЯТЬ И ДИСКИ");
    Panel(dc, 16, 68, 768, 396);
    for (int i = 0; i < 7; i++) {
        int y = 76 + i * 54;
        wchar_t label[40], free_[24], total[24], v[64];
        int pct = -1;
        if (i == 0) {
            MEMORYSTATUS ms;
            ms.dwLength = sizeof(ms);
            GlobalMemoryStatus(&ms);
            wcscpy(label, L"Оперативная память");
            Size(free_, 24, ms.dwAvailPhys);
            Size(total, 24, ms.dwTotalPhys);
            _snwprintf(v, 64, L"свободно %s из %s", free_, total);
            pct = ms.dwTotalPhys ? (int)((ms.dwTotalPhys - ms.dwAvailPhys) / (ms.dwTotalPhys / 100 + 1)) : 0;
        } else if (i == 1) {
            DWORD m = GetTickCount() / 60000;
            wcscpy(label, L"Время работы");
            _snwprintf(v, 64, L"%u ч %02u мин", (unsigned)(m / 60), (unsigned)(m % 60));
        } else {
            const wchar_t* vol = vols[i - 2];
            ULARGE_INTEGER avail, size, freeAll;
            wcsncpy(label, vol, 39);
            label[39] = 0;
            if (GetDiskFreeSpaceEx(vol, &avail, &size, &freeAll) && size.QuadPart) {
                Size(free_, 24, freeAll.QuadPart);
                Size(total, 24, size.QuadPart);
                _snwprintf(v, 64, L"свободно %s из %s", free_, total);
                pct = (int)((size.QuadPart - freeAll.QuadPart) * 100 / size.QuadPart);
            } else {
                wcscpy(v, L"нет");
            }
        }
        v[63] = 0;
        T(dc, 40, y + 33, label, 17, kMuted, 400, AL_START, 220);
        if (pct >= 0) {
            R(dc, 270, y + 22, 180, 10, 5, kLine);
            R(dc, 270, y + 22, pct * 180 / 100 > 10 ? pct * 180 / 100 : 10, 10, 5, pct > 90 ? kStop : kMuted);
        }
        T(dc, 760, y + 33, v, 17, kText, 700, AL_END, 290);
        if (i < 6)
            Sep(dc, 32, y + 52, 736);
    }
}

// Touch calibration: the system's crosshair screen (coredll's TouchCalibrate, as G3NavHMI's engineering menu
// does). The registry is reset on every boot, so the result holds until the next start.
static void Calibrate()
{
#ifdef UNDER_CE
    typedef BOOL (*CalibrateFn)(void);
    CalibrateFn f = (CalibrateFn)GetProcAddress(GetModuleHandle(L"coredll.dll"), L"TouchCalibrate");
    if (!f) {
        wcscpy(s_status, L"в системе нет TouchCalibrate");
        return;
    }
    SetWindowPos(s_wnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);   // its screen must not stay under us
    BOOL ok = f();
    SetWindowPos(s_wnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    SetForegroundWindow(s_wnd);
    wcscpy(s_status, ok ? L"калибровка действует до перезагрузки" : L"калибровка отменена");
#else
    wcscpy(s_status, L"калибровка — только на ГУ");
#endif
}

// "Minimize VH": VestaHome hides, Explorer's desktop and taskbar come up (Explorer is started if it doesn't run).
// Starting VestaHome.exe again shows it back (the second copy finds this one).
static void MinimizeHome()
{
#ifdef UNDER_CE
    s_taskbar = FindWindow(L"HHTaskBar", NULL);
    if (!s_taskbar) {
        PROCESS_INFORMATION pi;
        if (!CreateProcess(L"\\Windows\\explorer.exe", NULL, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi)) {
            wcscpy(s_status, L"explorer.exe не запускается");
            return;
        }
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
    ShowWindow(s_wnd, SW_HIDE);
    if (s_taskbar)
        ShowWindow(s_taskbar, SW_SHOW);
#else
    ShowWindow(s_wnd, SW_MINIMIZE);   // PC: Explorer runs anyway
#endif
}

static void ScrWarning(HDC dc)
{
    Frame(dc, L"", A_NONE);
    Panel(dc, 100, 84, 600, 320, 16);
    Ico(dc, IC_WARN, 132, 112, 56, kText);
    T(dc, 204, 150, L"ВНИМАНИЕ", 26, kText, 700);
    T(dc, 132, 212, L"Не отвлекайтесь от дороги. Управляйте системой", 19, kMuted, 400, AL_START, 540);
    T(dc, 132, 242, L"только когда это безопасно. Правила дорожного", 19, kMuted, 400, AL_START, 540);
    T(dc, 132, 272, L"движения важнее подсказок навигации.", 19, kMuted, 400, AL_START, 540);
    Button(dc, 452, 324, 216, 60, L"ПРИНЯТЬ", B_PRI, IC_NONE, 19, 0, A_BACK);
    T(dc, 132, 360, L"Окно закроется само через 10 с", 15, kDim, 400, AL_START, 310);
}

static void DrawScreen(HDC dc, Screen s);

// Windows over the previous screen: it is drawn (with its bottom bar), dimmed below the top bar and loses its
// buttons.
static void Under(HDC dc, int alpha)
{
    Screen prev = s_histLen ? s_hist[s_histLen - 1] : S_HOME;
    int pressed = s_pressed;
    s_pressed = -1;   // the pressed button is the window's, not one of the screen under it
    s_histLen--;      // as it was drawn: the window is not in its history
    if (prev == S_HOME)
        MainBackground(dc);
    DrawScreen(dc, prev);
    if (Section(prev) >= 0)
        NavBar(dc, Section(prev));
    s_histLen++;
    s_pressed = pressed;
    kBtnCount = 0;
    Shade(0, 56, 800, 424, alpha, alpha);
}

// Closing VestaHome (diagnostics, "Close VH"). The stock shell and started programs keep running.
static void ScrExit(HDC dc)
{
    Under(dc, 184);
    Panel(dc, 150, 120, 500, 240, 16);
    Ico(dc, IC_POWER, 182, 150, 48, kText);
    T(dc, 246, 186, L"ЗАКРЫТЬ VESTAHOME?", 24, kText, 700, AL_START, 380);
    T(dc, 182, 244, L"Штатная оболочка продолжит работать.", 19, kMuted, 400, AL_START, 436);
    Button(dc, 182, 276, 200, 60, L"ОТМЕНА", B_N, IC_NONE, 18, 0, A_BACK);
    Button(dc, 418, 276, 200, 60, L"ЗАКРЫТЬ", B_PRI, IC_NONE, 18, 0, A_EXIT);
}

static void ScrIncoming(HDC dc)
{
    Under(dc, 184);
    Panel(dc, 100, 84, 600, 340, 16);
    AvatarOr(dc, 196, 184, 64, L"МН");
    T(dc, 286, 150, L"ВХОДЯЩИЙ ВЫЗОВ", 14, kMuted, 700);
    T(dc, 286, 194, L"Мария Никифорова", 32, kText, 700, AL_START, 400);
    T(dc, 286, 226, L"+7 927 765-43-21", 20, kMuted, 400);
    bool d = Zone(124, 300, 266, 92, A_TAB, S_CALL);   // green and red: here the colour carries the meaning
    R(dc, 124, 300, 266, 92, 12, d ? Mix(kOk, kInk, 160) : kOk);
    Ico(dc, IC_CALL, 233, 322, 48, kInk);
    d = Zone(410, 300, 266, 92, A_BACK);
    R(dc, 410, 300, 266, 92, 12, d ? Mix(kStop, kInk, 160) : kStop);
    Ico(dc, IC_HANGUP, 519, 322, 48, kInk);
}

static void ScrVolume(HDC dc)
{
    Under(dc, 153);
    Zone(0, 0, 800, 480, A_BACK);
    Panel(dc, 150, 150, 500, 180, 16);
    Ico(dc, IC_VOL, 182, 182, 48, kText);
    T(dc, 246, 216, L"ГРОМКОСТЬ", 15, kMuted, 700);
    T(dc, 618, 222, L"18", 48, kText, 700, AL_END);
    for (int i = 0; i < 30; i++) {
        int h = 14 + i * 12 / 10;
        R(dc, 182 + i * 15, 300 - h, 10, h, 2, i < 18 ? kOrange : kLine);
    }
}

static void ScrSaver(HDC dc)
{
    SYSTEMTIME t;
    GetLocalTime(&t);
    wchar_t clock[8], date[48];
    Now(clock, 8);
    _snwprintf(date, 48, L"%s, %d %s", kDays[t.wDayOfWeek % 7], t.wDay, kMonths[(t.wMonth + 11) % 12]);
    CharUpperBuff(date, 1);
    Zone(0, 0, 800, 480, A_BACK);
    Pattern(0, 404, 0);
    TriFill(0, 404, 800, 76, 0);
    Shade(0, 404, 800, 76, 140, 0);   // #fade-up at 55 %
    R(dc, 0, 402, 800, 3, 0, kOrange);
    T(dc, 400, 260, clock, 168, kText, 700, AL_MID);
    T(dc, 400, 316, date, 26, kMuted, 400, AL_MID);
    Ico(dc, IC_MUSIC, 24, 424, 36, kText);
    T(dc, 72, 450, L"Кино — Группа крови", 18, kText, 700);
    int temp;
    if (OutsideTemp(&temp)) {
        wchar_t ts[12];
        _snwprintf(ts, 12, L"%+d °C", temp);
        T(dc, 776, 450, ts, 18, kText, 400, AL_END);
    }
}

static void Trim(wchar_t* s);

// ---------------------------------------------------------------- services: settings, GPS, weather, cameras

// ---- settings: VestaHome.ini next to the exe (UTF-8, key=value). The unit resets its registry on every boot;
// only the card keeps them.
enum { W_WTTR, W_OWM, W_GISMETEO, W_COUNT };
static const char* kProvKey[W_COUNT] = { "wttr", "owm", "gismeteo" };

struct Cfg {
    int     weatherProv;
    wchar_t weatherKey[W_COUNT][64];   // API key per provider (wttr.in needs none)
    bool    weatherGps;                // the place where the car is; without a fix the home point
    wchar_t homeName[48];
    double  homeLat, homeLon;          // both 0 = not set
    wchar_t gpsPort[16];               // the shell's NMEA port, only read
    DWORD   gpsBaud;                   // 0 = leave the speed the shell set
    bool    gpsTime;                   // set the clock from GPS
    int     tzMin;                     // local time = UTC + this (minutes): the unit's own zone resets with the registry
    bool    climate;                   // the car has climate control: its widget is shown
    bool    camAlert, camSound;
    int     camDist;                   // m, 0 = by speed
    wchar_t camFile[MAX_PATH];         // the camera file (iGO / Navitel), relative = from the exe's folder
    wchar_t camRegions[200];           // speedcamonline.ru regions to download: "63,73", "-1" = all of Russia
    wchar_t camKinds[100];             // its camera kinds (the site's filter numbers)
    bool    camWeekly;                 // download them again once a week
    int     camUpdated;                // yyyymmdd of the last download, 0 = never
};
static Cfg s_cfg;
static const int kCamDists[4] = { 0, 200, 400, 600 };   // 0 = by speed
// Camera bases live in their own folder next to the exe (kept out of git): downloads and the default file.
static const wchar_t* kCamFolder  = L"speedcam\\";
static const wchar_t* kCamDefault = L"speedcam\\speedcam.txt";

static void CfgDefaults()
{
    memset(&s_cfg, 0, sizeof(s_cfg));
    s_cfg.weatherProv = W_WTTR;
    s_cfg.weatherGps = true;
    wcscpy(s_cfg.gpsPort, L"COM5:");
    s_cfg.gpsTime = true;
    s_cfg.tzMin = 180;                            // Moscow
    s_cfg.camAlert = s_cfg.camSound = true;
    s_cfg.camDist = 0;
    wcscpy(s_cfg.camFile, kCamDefault);
    wcscpy(s_cfg.camKinds, L"0,14,11,110,2,3");   // the speed cameras
    s_cfg.camWeekly = true;
}

static void CfgPath(wchar_t* path)
{
    _snwprintf(path, MAX_PATH, L"%sVestaHome.ini", s_dir);
}

static bool CfgBool(const wchar_t* v) { return v[0] == L'1' || !_wcsicmp(v, L"true") || !_wcsicmp(v, L"yes"); }

static void SaveCfg();

static void LoadCfg()
{
    CfgDefaults();
    wchar_t path[MAX_PATH];
    CfgPath(path);
    DWORD size;
    char* buf = ReadAll(path, &size);
    if (!buf)
        return;
    char* p = buf;
    if ((unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF)
        p += 3;
    while (*p) {
        char* end = strchr(p, '\n');
        if (end)
            *end = 0;
        wchar_t line[400];
        int n = MultiByteToWideChar(CP_UTF8, 0, p, -1, line, 400);
        p = end ? end + 1 : p + strlen(p);
        wchar_t* eq = n > 0 && line[0] != L'#' ? wcschr(line, L'=') : NULL;
        if (!eq)
            continue;
        *eq = 0;
        wchar_t* k = line;
        wchar_t* v = eq + 1;
        Trim(k);
        Trim(v);
        if (!wcscmp(k, L"weather.provider")) {
            for (int i = 0; i < W_COUNT; i++) {
                wchar_t w[16];
                _snwprintf(w, 16, L"%S", kProvKey[i]);
                if (!_wcsicmp(v, w))
                    s_cfg.weatherProv = i;
            }
        } else if (!wcsncmp(k, L"weather.key.", 12)) {
            for (int i = 0; i < W_COUNT; i++) {
                wchar_t w[16];
                _snwprintf(w, 16, L"%S", kProvKey[i]);
                if (!_wcsicmp(k + 12, w)) {
                    wcsncpy(s_cfg.weatherKey[i], v, 63);
                    s_cfg.weatherKey[i][63] = 0;
                }
            }
        } else if (!wcscmp(k, L"weather.gps")) {
            s_cfg.weatherGps = CfgBool(v);
        } else if (!wcscmp(k, L"home.name")) {
            wcsncpy(s_cfg.homeName, v, 47);
            s_cfg.homeName[47] = 0;
        } else if (!wcscmp(k, L"home.lat")) {
            s_cfg.homeLat = wcstod(v, NULL);
        } else if (!wcscmp(k, L"home.lon")) {
            s_cfg.homeLon = wcstod(v, NULL);
        } else if (!wcscmp(k, L"gps.port")) {
            wcsncpy(s_cfg.gpsPort, v, 15);
            s_cfg.gpsPort[15] = 0;
        } else if (!wcscmp(k, L"gps.baud")) {
            s_cfg.gpsBaud = wcstoul(v, NULL, 10);
        } else if (!wcscmp(k, L"gps.time")) {
            s_cfg.gpsTime = CfgBool(v);
        } else if (!wcscmp(k, L"time.tz")) {
            s_cfg.tzMin = _wtoi(v);
        } else if (!wcscmp(k, L"car.climate")) {
            s_cfg.climate = CfgBool(v);
        } else if (!wcscmp(k, L"cam.alert")) {
            s_cfg.camAlert = CfgBool(v);
        } else if (!wcscmp(k, L"cam.sound")) {
            s_cfg.camSound = CfgBool(v);
        } else if (!wcscmp(k, L"cam.distance")) {
            s_cfg.camDist = _wtoi(v);
        } else if (!wcscmp(k, L"cam.regions")) {
            wcsncpy(s_cfg.camRegions, v, 199);
            s_cfg.camRegions[199] = 0;
        } else if (!wcscmp(k, L"cam.kinds")) {
            wcsncpy(s_cfg.camKinds, v, 99);
            s_cfg.camKinds[99] = 0;
        } else if (!wcscmp(k, L"cam.weekly")) {
            s_cfg.camWeekly = CfgBool(v);
        } else if (!wcscmp(k, L"cam.updated")) {
            s_cfg.camUpdated = _wtoi(v);
        } else if (!wcscmp(k, L"cam.file")) {
            wcsncpy(s_cfg.camFile, v, MAX_PATH - 1);
            s_cfg.camFile[MAX_PATH - 1] = 0;
        }
    }
    free(buf);
    // the old default next to the exe: the file moves into the speedcam folder, the setting with it
    if (!_wcsicmp(s_cfg.camFile, L"speedcam.txt")) {
        wchar_t from[MAX_PATH], dir[MAX_PATH], to[MAX_PATH];
        _snwprintf(from, MAX_PATH, L"%sspeedcam.txt", s_dir);
        _snwprintf(dir, MAX_PATH, L"%s%s", s_dir, kCamFolder);
        _snwprintf(to, MAX_PATH, L"%s%s", s_dir, kCamDefault);
        CreateDirectory(dir, NULL);
        if (GetFileAttributes(to) == 0xFFFFFFFF)
            MoveFile(from, to);
        wcscpy(s_cfg.camFile, kCamDefault);
        SaveCfg();
    }
}

// Written whole through a temporary file, like apps.txt.
static void SaveCfg()
{
    wchar_t text[3000];
    int n = _snwprintf(text, 3000,
        L"# VestaHome settings (changed from its screens; the file is rewritten whole).\r\n"
        L"weather.provider=%S\r\nweather.key.owm=%s\r\nweather.key.gismeteo=%s\r\n"
        L"weather.gps=%d\r\nhome.name=%s\r\nhome.lat=%.5f\r\nhome.lon=%.5f\r\n"
        L"# gps.baud: 0 = leave the port's speed as the shell set it\r\n"
        L"gps.port=%s\r\ngps.baud=%u\r\ngps.time=%d\r\n"
        L"# time.tz: local time = UTC + minutes (180 = Moscow)\r\ntime.tz=%d\r\ncar.climate=%d\r\n"
        L"# cam.file: the camera file (iGO or Navitel), a relative path is from the VestaHome folder;\r\n"
        L"# downloads go to speedcam\\speedcam.txt\r\n"
        L"cam.alert=%d\r\ncam.sound=%d\r\ncam.distance=%d\r\ncam.file=%s\r\n"
        L"# cam.regions, cam.kinds: what to download from speedcamonline.ru (region codes, -1 = Russia; kinds)\r\n"
        L"cam.regions=%s\r\ncam.kinds=%s\r\ncam.weekly=%d\r\ncam.updated=%d\r\n",
        kProvKey[s_cfg.weatherProv], s_cfg.weatherKey[W_OWM], s_cfg.weatherKey[W_GISMETEO], s_cfg.weatherGps, s_cfg.homeName, s_cfg.homeLat, s_cfg.homeLon,
        s_cfg.gpsPort, (unsigned)s_cfg.gpsBaud, s_cfg.gpsTime, s_cfg.tzMin, s_cfg.climate, s_cfg.camAlert, s_cfg.camSound,
        s_cfg.camDist, s_cfg.camFile, s_cfg.camRegions, s_cfg.camKinds, s_cfg.camWeekly, s_cfg.camUpdated);
    if (n < 0)
        return;
    static char out[9000];
    int len = WideCharToMultiByte(CP_UTF8, 0, text, -1, out, sizeof(out), NULL, NULL);
    wchar_t path[MAX_PATH], tmp[MAX_PATH];
    CfgPath(path);
    _snwprintf(tmp, MAX_PATH, L"%sVestaHome.tmp", s_dir);
    HANDLE h = CreateFile(tmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return;
    DWORD w;
    bool ok = len > 1 && WriteFile(h, out, len - 1, &w, NULL);
    CloseHandle(h);
    if (ok) {
        DeleteFile(path);
        MoveFile(tmp, path);
    } else {
        DeleteFile(tmp);
    }
}

// ---- GPS: the shell's NMEA port, as YaMapsCE reads it (native/gps.cpp): only GENERIC_READ, the port's
// settings left alone (the speed only if gps.baud asks), polling with ClearCommError. On the Vesta the stream
// of that port also carries other data: only printable sentences count, the checksum is checked when present.
// It reads all the time (a pause between chunks keeps the load low): the clock, the weather's place, the speed
// widget and the camera warnings use it. PC test: gps.port=track.nmea plays a recording from the exe folder.
#define WM_APP_GPS (WM_APP + 10)
#define WM_APP_WX  (WM_APP + 11)

struct Gps {
    int    open;                 // 1 open, 0 not (yet), -1 busy
    DWORD  lines;                // good sentences since start
    int    valid;                // the last RMC had status 'A'
    double lat, lon, speedKmh, course, alt;
    int    sats;
    DWORD  fixTick, dataTick;    // GetTickCount() of the last valid fix / sentence
    int    date, time;           // yyyymmdd, hhmmss (UTC) of the last valid RMC
};
static CRITICAL_SECTION s_gpsCs;
static Gps              s_gps;
static HANDLE           s_gpsThread, s_gpsWake;
static volatile bool    s_gpsStop, s_gpsReopen;
static volatile LONG    s_gpsPosted;
static char             s_nmea[128];
static int              s_nmeaLen;

static void GpsGet(Gps* g)
{
    EnterCriticalSection(&s_gpsCs);
    *g = s_gps;
    LeaveCriticalSection(&s_gpsCs);
    s_gpsPosted = 0;
}

static bool GpsFresh(const Gps& g, DWORD ms)
{
    return g.valid && GetTickCount() - g.fixTick < ms;
}

static int HexDigit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static double NmeaDeg(const char* v, char hemi)   // "5549.3932" + 'N' -> 55.823220
{
    double raw = atof(v);
    int deg = (int)(raw / 100);
    double d = deg + (raw - deg * 100) / 60.0;
    return hemi == 'S' || hemi == 'W' ? -d : d;
}

static void ParseNmea(char* s)
{
    if (s[0] != '$')
        return;
    for (const char* p = s; *p; p++)
        if ((unsigned char)*p < 0x20 || (unsigned char)*p > 0x7E)
            return;
    char* star = strchr(s, '*');
    bool checked = star != NULL;
    if (star) {
        unsigned char sum = 0;
        for (char* p = s + 1; p < star; p++)
            sum ^= (unsigned char)*p;
        int hi = HexDigit(star[1]), lo = HexDigit(star[2]);
        if (hi < 0 || lo < 0 || sum != hi * 16 + lo)
            return;
        *star = 0;
    }
    char* f[24];
    int n = 0;
    f[n++] = s + 1;
    for (char* p = s + 1; *p && n < 24; p++)
        if (*p == ',') {
            *p = 0;
            f[n++] = p + 1;
        }
    if (strlen(f[0]) != 5)
        return;
    const char* type = f[0] + 2;   // after the talker id (GP, GN, GL...)
    if (!checked && ((!strcmp(type, "RMC") && n < 12) || (!strcmp(type, "GGA") && n < 14)))
        return;   // a cut sentence without a checksum
    EnterCriticalSection(&s_gpsCs);
    s_gps.lines++;
    s_gps.dataTick = GetTickCount();
    if (!strcmp(type, "RMC") && n >= 10) {
        bool valid = f[2][0] == 'A' && f[3][0] && f[5][0];
        if (valid) {
            s_gps.lat = NmeaDeg(f[3], f[4][0]);
            s_gps.lon = NmeaDeg(f[5], f[6][0]);
            s_gps.speedKmh = atof(f[7]) * 1.852;
            if (f[8][0])
                s_gps.course = atof(f[8]);
            s_gps.fixTick = s_gps.dataTick;
            if (strlen(f[9]) == 6 && strlen(f[1]) >= 6) {
                int d = atoi(f[9]);   // ddmmyy
                s_gps.date = 20000000 + (d % 100) * 10000 + (d / 100 % 100) * 100 + d / 10000;
                s_gps.time = atoi(f[1]);
            }
        }
        s_gps.valid = valid;
    } else if (!strcmp(type, "GGA") && n >= 10) {
        s_gps.sats = atoi(f[7]);
        if (f[9][0])
            s_gps.alt = atof(f[9]);
    }
    LeaveCriticalSection(&s_gpsCs);
    if (!InterlockedExchange((LONG*)&s_gpsPosted, 1) && !PostMessage(s_wnd, WM_APP_GPS, 0, 0))
        s_gpsPosted = 0;   // one message at a time: the window takes the state when it can
}

static void FeedNmea(const char* buf, int len)
{
    for (int i = 0; i < len; i++) {
        char c = buf[i];
        if (c == '$')
            s_nmeaLen = 0;
        if (c == '\r' || c == '\n') {
            s_nmea[s_nmeaLen] = 0;
            if (s_nmeaLen > 6)
                ParseNmea(s_nmea);
            s_nmeaLen = 0;
        } else if (s_nmeaLen < (int)sizeof(s_nmea) - 1) {
            s_nmea[s_nmeaLen++] = c;
        }
    }
}

static void GpsSetOpen(int open)
{
    EnterCriticalSection(&s_gpsCs);
    s_gps.open = open;
    if (open <= 0)
        s_gps.valid = 0;
    LeaveCriticalSection(&s_gpsCs);
}

#ifndef UNDER_CE
// PC: a recorded track next to the exe, one RMC per 250 ms (4x real time).
static void GpsPlayFile(const wchar_t* name)
{
    wchar_t path[MAX_PATH];
    _snwprintf(path, MAX_PATH, L"%s%s", s_dir, name);
    DWORD len;
    char* data = ReadAll(path, &len);
    if (!data) {
        GpsSetOpen(0);
        WaitForSingleObject(s_gpsWake, 3000);
        return;
    }
    GpsSetOpen(1);
    while (!s_gpsStop && !s_gpsReopen) {
        DWORD start = 0;
        for (DWORD i = 0; i < len && !s_gpsStop && !s_gpsReopen; i++) {
            if (data[i] != '\n')
                continue;
            FeedNmea(data + start, i + 1 - start);
            if (i - start > 6 && !strncmp(data + start + 3, "RMC", 3))
                WaitForSingleObject(s_gpsWake, 250);
            start = i + 1;
        }
    }
    free(data);
}
#endif

static DWORD WINAPI GpsThread(LPVOID)
{
    while (!s_gpsStop) {
        wchar_t port[16];
        EnterCriticalSection(&s_gpsCs);
        wcscpy(port, s_cfg.gpsPort);
        DWORD baud = s_cfg.gpsBaud;
        s_gpsReopen = false;
        LeaveCriticalSection(&s_gpsCs);
#ifndef UNDER_CE
        if (wcsstr(port, L".nmea")) {
            GpsPlayFile(port);
            continue;
        }
#endif
        HANDLE h = CreateFile(port, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        if (h == INVALID_HANDLE_VALUE) {
            DWORD err = GetLastError();
            GpsSetOpen(err == ERROR_ACCESS_DENIED || err == ERROR_SHARING_VIOLATION || err == ERROR_BUSY ? -1 : 0);
            WaitForSingleObject(s_gpsWake, 5000);
            continue;
        }
        DCB dcb;
        memset(&dcb, 0, sizeof(dcb));
        dcb.DCBlength = sizeof(dcb);
        if (baud && GetCommState(h, &dcb) && dcb.BaudRate != baud) {
            dcb.BaudRate = baud;
            SetCommState(h, &dcb);
        }
        GpsSetOpen(1);
        s_nmeaLen = 0;
        while (!s_gpsStop && !s_gpsReopen) {
            DWORD errors = 0;
            COMSTAT st;
            memset(&st, 0, sizeof(st));
            if (!ClearCommError(h, &errors, &st))
                break;
            if (st.cbInQue) {
                char buf[2048];
                DWORD got = 0;
                if (!ReadFile(h, buf, st.cbInQue < sizeof(buf) ? st.cbInQue : sizeof(buf), &got, NULL))
                    break;
                FeedNmea(buf, (int)got);
                Sleep(100);   // let the driver collect a chunk instead of spinning on bytes
            } else {
                WaitForSingleObject(s_gpsWake, 250);
            }
        }
        CloseHandle(h);
        GpsSetOpen(0);
    }
    return 0;
}

static void GpsStart()
{
    InitializeCriticalSection(&s_gpsCs);
    s_gpsWake = CreateEvent(NULL, FALSE, FALSE, NULL);
    s_gpsThread = CreateThread(NULL, 0, GpsThread, NULL, 0, NULL);
}

static void GpsReopen()
{
    s_gpsReopen = true;
    SetEvent(s_gpsWake);
}

static void GpsStopThread()
{
    if (!s_gpsThread)
        return;
    s_gpsStop = true;
    SetEvent(s_gpsWake);
    WaitForSingleObject(s_gpsThread, 3000);
}

// One line for the settings and the speed widget: what the port gives.
// brief: a short form for a row with a control on the right (no port name, no coordinates).
static void GpsStatusText(wchar_t* s, int n, bool brief)
{
    Gps g;
    GpsGet(&g);
    if (g.open < 0)
        _snwprintf(s, n, brief ? L"занят" : L"%s занят", s_cfg.gpsPort);
    else if (!g.open)
        _snwprintf(s, n, brief ? L"не открывается" : L"%s не открывается", s_cfg.gpsPort);
    else if (!g.lines || GetTickCount() - g.dataTick > 10000)
        wcsncpy(s, brief ? L"нет NMEA" : L"нет NMEA: выдачу включает навигация", n);
    else if (!GpsFresh(g, 5000))
        _snwprintf(s, n, brief ? L"нет фикса · спутников %d" : L"NMEA есть, нет фикса · спутников %d", g.sats);
    else if (brief)
        _snwprintf(s, n, L"фикс · спутников %d", g.sats);
    else
        _snwprintf(s, n, L"фикс · спутников %d · %.5f, %.5f", g.sats, g.lat, g.lon);
    s[n - 1] = 0;
}

// The port and its speed on the "General" settings: arrows step through COM1..COM9 and kGpsBauds (as in
// YaMapsCE / OSMapsCE); every step is saved and the port reopened. A port that is not COMn (the PC's
// track.nmea) is shown as it is until an arrow is pressed.
static const DWORD kGpsBauds[] = { 0, 4800, 9600, 19200, 38400, 57600, 115200 };   // 0 = leave as is
enum { kGpsBaudCount = sizeof(kGpsBauds) / sizeof(kGpsBauds[0]) };

static int GpsPortNum()
{
    const wchar_t* p = s_cfg.gpsPort;
    return !_wcsnicmp(p, L"COM", 3) && p[3] >= L'1' && p[3] <= L'9' ? p[3] - L'0' : 0;
}

static void GpsPortText(wchar_t* s, int n)
{
    int k = GpsPortNum();
    if (k)
        _snwprintf(s, n, L"COM%d", k);
    else
        wcsncpy(s, s_cfg.gpsPort, n);
    s[n - 1] = 0;
}

static void GpsBaudText(wchar_t* s, int n)
{
    if (s_cfg.gpsBaud)
        _snwprintf(s, n, L"%u", (unsigned)s_cfg.gpsBaud);
    else
        wcsncpy(s, L"не менять", n);
    s[n - 1] = 0;
}

static void GpsPortStep(int dir)
{
    int k = GpsPortNum();
    k = k ? k + dir : dir > 0 ? 1 : 9;
    k = k < 1 ? 9 : k > 9 ? 1 : k;
    _snwprintf(s_cfg.gpsPort, 16, L"COM%d:", k);
    SaveCfg();
    GpsReopen();
}

static void GpsBaudStep(int dir)
{
    int i = 0;
    while (i < kGpsBaudCount && kGpsBauds[i] != s_cfg.gpsBaud)
        i++;
    i = i == kGpsBaudCount ? 0 : (i + dir + kGpsBaudCount) % kGpsBaudCount;
    s_cfg.gpsBaud = kGpsBauds[i];
    SaveCfg();
    GpsReopen();
}

// SYSTEMTIME moved by minutes (through FILETIME, 100 ns units).
static bool AddMinutes(SYSTEMTIME* t, int minutes)
{
    FILETIME ft;
    if (!SystemTimeToFileTime(t, &ft))
        return false;
    *(__int64*)&ft += (__int64)minutes * 600000000;
    return FileTimeToSystemTime(&ft, t) != 0;
}

// The clock from GPS (RMC is UTC): when it is off by more than 30 s, at most once an hour.
// Set as local time = UTC + time.tz: the unit's own time zone comes back to the CE default (Pacific, UTC-8)
// with the registry on every boot, so SetSystemTime(UTC) showed 22:00 for 09:00 Moscow.
static DWORD s_timeSyncTick;
static bool  s_timeSynced;

static void GpsTimeSync(const Gps& g)
{
    if (!s_cfg.gpsTime || !GpsFresh(g, 2000) || !g.date || (s_timeSynced && GetTickCount() - s_timeSyncTick < 3600000))
        return;
    SYSTEMTIME gt, now;
    memset(&gt, 0, sizeof(gt));
    gt.wYear = (WORD)(g.date / 10000);
    gt.wMonth = (WORD)(g.date / 100 % 100);
    gt.wDay = (WORD)(g.date % 100);
    gt.wHour = (WORD)(g.time / 10000);
    gt.wMinute = (WORD)(g.time / 100 % 100);
    gt.wSecond = (WORD)(g.time % 100);
    if (!AddMinutes(&gt, s_cfg.tzMin))
        return;
    GetLocalTime(&now);
    FILETIME a, b;
    if (!SystemTimeToFileTime(&gt, &a) || !SystemTimeToFileTime(&now, &b))
        return;
    __int64 d = (*(__int64*)&a - *(__int64*)&b) / 10000000;
    s_timeSynced = true;
    s_timeSyncTick = GetTickCount();
    if (d > 30 || d < -30) {
        VLog(L"часы: по GPS %02u:%02u:%02u (UTC%+d мин), было %02u:%02u:%02u, сдвиг %+d с", gt.wHour, gt.wMinute,
             gt.wSecond, s_cfg.tzMin, now.wHour, now.wMinute, now.wSecond, (int)d);
        SetLocalTime(&gt);
        VLog(L"часы: выставлены");
        _snwprintf(s_status, 96, L"время по GPS (было %+d с)", (int)-d);
    }
}

static int TzMinutes()
{
    return s_cfg.tzMin;
}

// The zone arrows: one hour a step, UTC-12 .. UTC+14. The clock moves with the zone right away (as a phone's
// does), and the next GPS fix sets it again with the new zone.
static void TzStep(int dir)
{
    int m = s_cfg.tzMin + dir * 60;
    if (m < -12 * 60 || m > 14 * 60)
        return;
    SYSTEMTIME t;
    GetLocalTime(&t);
    if (AddMinutes(&t, m - s_cfg.tzMin))
        SetLocalTime(&t);
    VLog(L"часовой пояс: %+d -> %+d мин", s_cfg.tzMin, m);
    s_cfg.tzMin = m;
    s_timeSynced = false;
    SaveCfg();
}

// ---- HTTP and HTTPS through https.cpp (BearSSL; the copy of OSMapsCE's client).
// Body of a 200 answer (malloc'ed, zero-terminated) or NULL with the reason in err.
static char* Fetch(const char* host, bool tls, const char* path, const char* headers, wchar_t* err, int errCap)
{
    unsigned char* body = NULL;
    int len = 0, status = 0;
    int r = HttpGet(host, tls, path, headers, &body, &len, &status);
    if (r != NET_OK) {
        _snwprintf(err, errCap, L"%S: %s", host, NetErrorText(r));
        return NULL;
    }
    if (status != 200) {
        _snwprintf(err, errCap, L"%S: ответ %d", host, status);
        free(body);
        return NULL;
    }
    return (char*)body;
}

// Minimal JSON lookups: the value after "key": (from p on), as a number or a string.
static const char* JsonAt(const char* p, const char* key)
{
    char k[48];
    _snprintf(k, sizeof(k), "\"%s\"", key);
    p = p ? strstr(p, k) : NULL;
    if (!p)
        return NULL;
    p += strlen(k);
    while (*p == ' ' || *p == ':')
        p++;
    return p;
}

static double JsonNum(const char* p, const char* key, double def)
{
    p = JsonAt(p, key);
    if (!p)
        return def;
    if (*p == '"')
        p++;
    return atof(p);
}

static bool JsonStr(const char* p, const char* key, wchar_t* out, int cap)
{
    p = JsonAt(p, key);
    if (!p || *p != '"')
        return false;
    p++;
    char tmp[200];
    int n = 0;
    while (*p && *p != '"' && n < (int)sizeof(tmp) - 1) {
        if (*p == '\\' && p[1])
            p++;
        tmp[n++] = *p++;
    }
    tmp[n] = 0;
    return MultiByteToWideChar(CP_UTF8, 0, tmp, -1, out, cap) > 0;
}

// ---- weather: wttr.in (open, no key) or OpenWeatherMap (free key) over HTTP; Gismeteo over HTTPS with a token
// (Yandex dropped: paid only). Open-Meteo did not answer from the user's network (2026-10-12). Refreshed every 30 minutes, on
// a tap on the widget and after a change of the settings.
enum { WX_SUN, WX_PART, WX_CLOUD, WX_RAIN, WX_SNOW, WX_STORM, WX_FOG };
struct Weather {
    int     ok;                    // 1 data, 0 none yet, -1 the last try failed (old data may stay)
    bool    have;                  // the values below are from some successful fetch
    int     temp, tmax, tmin, wind, kind;
    wchar_t desc[48], place[48], err[80], rise[8], set[8];
    SYSTEMTIME at;                 // local time of the fetch
    DWORD   tick;
};
static CRITICAL_SECTION s_wxCs;
static Weather          s_wx;
static volatile LONG    s_wxBusy;
static DWORD            s_wxTry;      // tick of the last attempt

static int WttrKind(int code)
{
    switch (code) {
    case 113: return WX_SUN;
    case 116: return WX_PART;
    case 119: case 122: return WX_CLOUD;
    case 143: case 248: case 260: return WX_FOG;
    case 200: case 386: case 389: case 392: case 395: return WX_STORM;
    case 179: case 227: case 230: case 323: case 326: case 329: case 332: case 335: case 338: case 368: case 371:
        return WX_SNOW;
    default: return WX_RAIN;
    }
}

static int OwmKind(int id)
{
    if (id >= 200 && id < 300) return WX_STORM;
    if (id >= 600 && id < 700) return WX_SNOW;
    if (id >= 700 && id < 800) return WX_FOG;
    if (id == 800) return WX_SUN;
    if (id == 801 || id == 802) return WX_PART;
    if (id > 802) return WX_CLOUD;
    return WX_RAIN;
}

// "07:04 AM" -> "7:04"; "05:53 PM" -> "17:53"
static void Clock12(const wchar_t* s, wchar_t* out)
{
    int h = _wtoi(s), m = s[0] && s[1] && s[2] ? _wtoi(s + 3) : 0;
    if (wcsstr(s, L"PM") && h < 12)
        h += 12;
    if (wcsstr(s, L"AM") && h == 12)
        h = 0;
    _snwprintf(out, 8, L"%d:%02d", h, m);
}

static void UnixClock(double t, double tz, wchar_t* out)
{
    long s = (long)fmod(t + tz, 86400.0);
    _snwprintf(out, 8, L"%ld:%02ld", s / 3600, s / 60 % 60);
}

static DWORD WINAPI WeatherThread(LPVOID)
{
    Weather w;
    memset(&w, 0, sizeof(w));
    int prov = s_cfg.weatherProv;
    Gps g;
    GpsGet(&g);
    bool here = s_cfg.weatherGps && GpsFresh(g, 600000);
    bool home = s_cfg.homeLat != 0 || s_cfg.homeLon != 0;
    double lat = here ? g.lat : s_cfg.homeLat, lon = here ? g.lon : s_cfg.homeLon;
    VLog(L"погода: запрос, провайдер %d, %s", prov, here ? L"по GPS" : home ? L"дом" : L"по адресу");
    char key[64];
    WideCharToMultiByte(CP_UTF8, 0, s_cfg.weatherKey[prov], -1, key, sizeof(key), NULL, NULL);
    char path[300];
    char* body = NULL;
    if (prov == W_WTTR) {
        if (here || home)
            _snprintf(path, sizeof(path), "/%.3f,%.3f?format=j1&lang=ru", lat, lon);
        else
            strcpy(path, "/?format=j1&lang=ru");   // wttr.in finds the place by the address we come from
        body = Fetch("wttr.in", false, path, NULL, w.err, 80);
        if (body) {
            const char* cur = JsonAt(body, "current_condition");
            w.temp = (int)JsonNum(cur, "temp_C", 0);
            w.wind = (int)(JsonNum(cur, "windspeedKmph", 0) / 3.6 + 0.5);
            w.kind = WttrKind((int)JsonNum(cur, "weatherCode", 113));
            if (!JsonStr(JsonAt(cur, "lang_ru"), "value", w.desc, 48))
                JsonStr(JsonAt(cur, "weatherDesc"), "value", w.desc, 48);
            const char* day = JsonAt(body, "weather");
            w.tmax = (int)JsonNum(day, "maxtempC", w.temp);
            w.tmin = (int)JsonNum(day, "mintempC", w.temp);
            wchar_t t[16];
            if (JsonStr(day, "sunrise", t, 16))
                Clock12(t, w.rise);
            if (JsonStr(day, "sunset", t, 16))
                Clock12(t, w.set);
            JsonStr(JsonAt(body, "areaName"), "value", w.place, 48);
            w.ok = 1;
        }
    } else if (prov == W_OWM) {
        if (!key[0]) {
            wcscpy(w.err, L"нужен ключ API OpenWeatherMap");
        } else if (!here && !home) {
            wcscpy(w.err, L"нет места: GPS или домашняя точка");
        } else {
            _snprintf(path, sizeof(path), "/data/2.5/weather?lat=%.3f&lon=%.3f&units=metric&lang=ru&appid=%s",
                      lat, lon, key);
            body = Fetch("api.openweathermap.org", false, path, NULL, w.err, 80);
            if (body) {
                w.temp = (int)floor(JsonNum(JsonAt(body, "main"), "temp", 0) + 0.5);
                w.tmax = (int)floor(JsonNum(JsonAt(body, "main"), "temp_max", w.temp) + 0.5);
                w.tmin = (int)floor(JsonNum(JsonAt(body, "main"), "temp_min", w.temp) + 0.5);
                w.wind = (int)(JsonNum(JsonAt(body, "wind"), "speed", 0) + 0.5);
                w.kind = OwmKind((int)JsonNum(JsonAt(body, "weather"), "id", 800));
                JsonStr(JsonAt(body, "weather"), "description", w.desc, 48);
                JsonStr(body, "name", w.place, 48);
                double tz = JsonNum(body, "timezone", 0);
                UnixClock(JsonNum(JsonAt(body, "sys"), "sunrise", 0), tz, w.rise);
                UnixClock(JsonNum(JsonAt(body, "sys"), "sunset", 0), tz, w.set);
                w.ok = 1;
            }
        }
    } else if (!key[0]) {   // Gismeteo: HTTPS with the token its b2b office gives (not tried without one)
        wcscpy(w.err, L"нужен токен Gismeteo");
    } else if (!here && !home) {
        wcscpy(w.err, L"нет места: GPS или домашняя точка");
    } else {
        char hdr[120];
        _snprintf(hdr, sizeof(hdr), "X-Gismeteo-Token: %s\r\n", key);
        _snprintf(path, sizeof(path), "/v2/weather/current/?latitude=%.3f&longitude=%.3f&lang=ru", lat, lon);
        body = Fetch("api.gismeteo.net", true, path, hdr, w.err, 80);
        if (body) {
            const char* r = JsonAt(body, "response");
            w.temp = (int)floor(JsonNum(JsonAt(JsonAt(r, "temperature"), "air"), "C", 0) + 0.5);
            w.tmax = w.tmin = w.temp;
            w.wind = (int)(JsonNum(JsonAt(JsonAt(r, "wind"), "speed"), "m_s", 0) + 0.5);
            JsonStr(JsonAt(r, "description"), "full", w.desc, 48);
            int pr = (int)JsonNum(JsonAt(r, "precipitation"), "type", 0), cl = (int)JsonNum(JsonAt(r, "cloudiness"), "type", 0);
            const char* st = JsonAt(r, "storm");
            w.kind = st && !strncmp(st, "true", 4) ? WX_STORM : pr == 2 ? WX_SNOW : pr ? WX_RAIN
                     : cl == 0 ? WX_SUN : cl == 1 ? WX_PART : WX_CLOUD;
            w.ok = 1;
        }
    }
    free(body);
    if (w.ok && !here && home && s_cfg.homeName[0])
        wcscpy(w.place, s_cfg.homeName);
    EnterCriticalSection(&s_wxCs);
    if (w.ok) {
        GetLocalTime(&w.at);
        w.tick = GetTickCount();
        w.have = true;
        s_wx = w;
    } else {
        s_wx.ok = -1;
        wcscpy(s_wx.err, w.err);
    }
    LeaveCriticalSection(&s_wxCs);
    VLog(L"погода: %s %s", w.ok ? L"ok" : L"ошибка", w.ok ? w.place : w.err);
    s_wxBusy = 0;
    PostMessage(s_wnd, WM_APP_WX, 0, 0);
    return 0;
}

static void WeatherRefresh()
{
    if (InterlockedExchange((LONG*)&s_wxBusy, 1))
        return;
    s_wxTry = GetTickCount();
    HANDLE h = CreateThread(NULL, 0, WeatherThread, NULL, 0, NULL);
    if (h)
        CloseHandle(h);
    else
        s_wxBusy = 0;
}

static void WxGet(Weather* w)
{
    EnterCriticalSection(&s_wxCs);
    *w = s_wx;
    LeaveCriticalSection(&s_wxCs);
}

// ---- speed cameras: speedcam.txt in the iGO (X,Y,TYPE,SPEED,DIRTYPE,DIRECTION) or Navitel
// (IDX,X,Y,TYPE,SPEED,DIRTYPE,DIRECTION) format, columns found by the header. Only the cameras within 150 km of
// the car (or the home point) are kept, reloaded after 75 km: a country's list doesn't fit the unit's memory.
// A camera ahead (see CamCheck) shows on the speed widget and in the top bar, with two beeps when it comes up.
struct Cam { float lat, lon; unsigned char speed, dirType; short dir; };
enum { kMaxCams = 6000 };
static Cam    s_cams[kMaxCams];
static int    s_camCount, s_camTotal;   // kept / seen in the file
static double s_camLat, s_camLon;       // centre of the kept ones
static bool   s_camLoaded;
static int    s_camNear = -1;           // camera ahead, -1 none
static int    s_camNearDist;

static double Dist(double lat1, double lon1, double lat2, double lon2)   // m, fine for some km
{
    double k = 3.14159265 / 180, x = (lon2 - lon1) * k * cos((lat1 + lat2) * k / 2), y = (lat2 - lat1) * k;
    return sqrt(x * x + y * y) * 6371000;
}

static double Bearing(double lat1, double lon1, double lat2, double lon2)   // degrees from north
{
    double k = 3.14159265 / 180;
    double b = atan2((lon2 - lon1) * cos(lat1 * k), lat2 - lat1) / k;
    return b < 0 ? b + 360 : b;
}

static double AngleDiff(double a, double b)
{
    double d = fmod(fabs(a - b), 360.0);
    return d > 180 ? 360 - d : d;
}

static void CamPath(wchar_t* path)
{
    if (s_cfg.camFile[0] == L'\\' || wcschr(s_cfg.camFile, L':'))
        wcscpy(path, s_cfg.camFile);
    else
        _snwprintf(path, MAX_PATH, L"%s%s", s_dir, s_cfg.camFile);
}

static int    s_camCol[6];   // columns of X, Y, TYPE, SPEED, DIRTYPE, DIRECTION
static bool   s_camFirst;
static double s_camDLat, s_camDLon;

// One line of the file: the header (column names) or a camera.
static void CamLine(char* line)
{
    if (strlen(line) < 3)
        return;
    char* f[12];
    int n = 0;
    f[n++] = line;
    for (char* p = line; *p && n < 12; p++)
        if (*p == ',' || *p == ';') {
            *p = 0;
            f[n++] = p + 1;
        }
    bool header = s_camFirst && ((line[0] >= 'A' && line[0] <= 'Z') || (line[0] >= 'a' && line[0] <= 'z'));
    s_camFirst = false;
    if (header) {
        static const char* names[6] = { "X", "Y", "TYPE", "SPEED", "DIRTYPE", "DIRECTION" };
        for (int k = 0; k < 6; k++)
            for (int j = 0; j < n; j++) {
                const char* nm = f[j];
                while (*nm == ' ')
                    nm++;
                int l = (int)strlen(names[k]);
                if (!_strnicmp(nm, names[k], l) && (nm[l] == 0 || nm[l] == ' '))
                    s_camCol[k] = j;
            }
        return;
    }
    if (n <= s_camCol[1])
        return;
    double x = atof(f[s_camCol[0]]), y = atof(f[s_camCol[1]]);
    if (x == 0 && y == 0)
        return;
    s_camTotal++;
    if (fabs(y - s_camLat) > s_camDLat || fabs(x - s_camLon) > s_camDLon || s_camCount >= kMaxCams)
        return;
    Cam& cm = s_cams[s_camCount++];
    cm.lat = (float)y;
    cm.lon = (float)x;
    cm.speed = (unsigned char)(n > s_camCol[3] ? atoi(f[s_camCol[3]]) : 0);
    cm.dirType = (unsigned char)(n > s_camCol[4] ? atoi(f[s_camCol[4]]) : 0);
    cm.dir = (short)(n > s_camCol[5] ? atoi(f[s_camCol[5]]) : 0);
}

static void LoadCams(double lat, double lon)
{
    s_camCount = s_camTotal = 0;
    s_camNear = -1;
    s_camLoaded = true;
    s_camLat = lat;
    s_camLon = lon;
    s_camDLat = 150000.0 / 111000;
    s_camDLon = s_camDLat / (cos(lat * 3.14159265 / 180) + 0.01);
    for (int k = 0; k < 6; k++)
        s_camCol[k] = k;   // the iGO order when there is no header
    s_camFirst = true;
    wchar_t path[MAX_PATH];
    CamPath(path);
    HANDLE h = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        VLog(L"камеры: нет файла %s", path);
        return;
    }
    DWORD t0 = GetTickCount();
    VLog(L"камеры: читаю %s (%u Б) у %.4f, %.4f", path, (unsigned)GetFileSize(h, NULL), lat, lon);
    static char chunk[16384];
    char line[256];
    int ll = 0;
    for (;;) {
        DWORD got = 0;
        bool eof = !ReadFile(h, chunk, sizeof(chunk), &got, NULL) || !got;
        for (DWORD i = 0; i < got; i++) {
            char c = chunk[i];
            if (c == '\n' || c == '\r') {
                line[ll] = 0;
                CamLine(line);
                ll = 0;
            } else if (ll < (int)sizeof(line) - 1) {
                line[ll++] = c;
            }
        }
        if (eof) {
            line[ll] = 0;
            CamLine(line);
            break;
        }
    }
    CloseHandle(h);
    VLog(L"камеры: %d в 150 км из %d за %u мс", s_camCount, s_camTotal, (unsigned)(GetTickCount() - t0));
}

static HWAVEOUT s_beepOut;
static WAVEHDR  s_beepHdr;
static short    s_beep[11025 / 2];   // 2 x 120 ms at 1.2 kHz with a gap, 11 kHz mono

static void Beep2()
{
    if (s_beepOut) {
        if (!(s_beepHdr.dwFlags & WHDR_DONE))
            return;
        waveOutUnprepareHeader(s_beepOut, &s_beepHdr, sizeof(s_beepHdr));
        waveOutClose(s_beepOut);
        s_beepOut = NULL;
    }
    WAVEFORMATEX wf;
    memset(&wf, 0, sizeof(wf));
    wf.wFormatTag = WAVE_FORMAT_PCM;
    wf.nChannels = 1;
    wf.nSamplesPerSec = 11025;
    wf.wBitsPerSample = 16;
    wf.nBlockAlign = 2;
    wf.nAvgBytesPerSec = 22050;
    if (waveOutOpen(&s_beepOut, WAVE_MAPPER, &wf, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        s_beepOut = NULL;
        return;
    }
    int n = sizeof(s_beep) / sizeof(s_beep[0]);
    for (int i = 0; i < n; i++) {
        int t = i * 1000 / 11025;   // ms
        bool on = t < 120 || (t >= 200 && t < 320);
        s_beep[i] = on ? (short)(9000 * sin(i * 2 * 3.14159265 * 1200 / 11025)) : 0;
    }
    memset(&s_beepHdr, 0, sizeof(s_beepHdr));
    s_beepHdr.lpData = (LPSTR)s_beep;
    s_beepHdr.dwBufferLength = sizeof(s_beep);
    waveOutPrepareHeader(s_beepOut, &s_beepHdr, sizeof(s_beepHdr));
    waveOutWrite(s_beepOut, &s_beepHdr, sizeof(s_beepHdr));
}

// Without the road graph a camera is matched to the car's line of travel: in front of the car along the course
// (`along`) and close to that line sideways (`cross` < kCamCross: a parallel street is further), and for a camera
// with a direction the course must agree with it (one-way: within kCamDir; both ways: either way). The warning
// distance is the setting, or by speed (kCamLeadS seconds ahead, 100..500 m) when it is "by speed". The course is
// taken only above 10 km/h and smoothed; a camera once shown stays until it is passed (or left aside), so the
// sign doesn't blink. Left wrong: a frontage road right next to the controlled one, a sharp bend before it.
enum { kCamCross = 30, kCamCrossKeep = 45, kCamDir = 35, kCamLeadS = 10 };
static double s_course = -1;   // smoothed course, -1 = not known yet

static int CamAlertDist(double kmh)
{
    if (s_cfg.camDist > 0)
        return s_cfg.camDist;
    int d = (int)(kmh / 3.6 * kCamLeadS);
    return d < 100 ? 100 : d > 500 ? 500 : d;
}

// Where the camera is from the car: metres ahead along the course and sideways off it.
static void CamOffset(const Gps& g, const Cam& cm, double* along, double* cross)
{
    double k = 3.14159265 / 180;
    double e = (cm.lon - g.lon) * k * cos(g.lat * k) * 6371000, n = (cm.lat - g.lat) * k * 6371000;
    double hx = sin(s_course * k), hy = cos(s_course * k);
    *along = e * hx + n * hy;
    *cross = fabs(e * hy - n * hx);
}

static bool CamFacing(const Cam& cm)
{
    if (cm.dirType == 1)
        return AngleDiff(cm.dir, s_course) <= kCamDir;
    if (cm.dirType == 2) {
        double d = AngleDiff(cm.dir, s_course);
        return d <= kCamDir || d >= 180 - kCamDir;
    }
    return true;   // 0: any direction
}

// On every GPS update: the camera to warn about; true when what is shown changed.
static bool CamCheck(const Gps& g)
{
    int was = s_camNear, wasDist = s_camNearDist / 50;
    if (!s_cfg.camAlert || !GpsFresh(g, 5000)) {
        s_camNear = -1;
        return s_camNear != was;
    }
    if (!s_camLoaded || Dist(g.lat, g.lon, s_camLat, s_camLon) > 75000)
        LoadCams(g.lat, g.lon);
    if (g.speedKmh > 10)   // a standing car's course is noise
        s_course = s_course < 0 || AngleDiff(g.course, s_course) > 60 ? g.course   // a turn: take it as is
                   : s_course + 0.5 * (fmod(g.course - s_course + 540.0, 360.0) - 180);
    if (s_course < 0)
        return false;
    s_course = fmod(s_course + 360.0, 360.0);
    int limit = CamAlertDist(g.speedKmh);
    // keep the shown camera until it is passed or the car turned away from it
    if (s_camNear >= 0) {
        double along, cross;
        CamOffset(g, s_cams[s_camNear], &along, &cross);
        if (along > -10 && along < limit + 100 && cross < kCamCrossKeep) {
            s_camNearDist = (int)(along > 0 ? along : 0);
            return s_camNearDist / 50 != wasDist;
        }
        s_camNear = -1;
    }
    double best = limit;
    for (int i = 0; i < s_camCount; i++) {
        const Cam& cm = s_cams[i];
        if (fabs(cm.lat - g.lat) > 0.01 || fabs(cm.lon - g.lon) > 0.02)
            continue;   // far for sure: skip the trigonometry
        double along, cross;
        CamOffset(g, cm, &along, &cross);
        if (along < 0 || along > best || cross > kCamCross || !CamFacing(cm))
            continue;
        best = along;
        s_camNear = i;
    }
    s_camNearDist = s_camNear >= 0 ? (int)best : 0;
    if (s_camNear >= 0)
        VLog(L"камера: %d м впереди, лимит %d, курс %.0f%s", s_camNearDist, s_cams[s_camNear].speed, s_course,
             s_cfg.camSound ? L", сигнал" : L"");
    if (s_camNear >= 0 && s_cfg.camSound)
        Beep2();
    return s_camNear != was || s_camNearDist / 50 != wasDist;
}

static bool OutsideTemp(int* t)
{
    Weather w;
    WxGet(&w);
    *t = w.temp;
    return w.have;
}

static bool CamChip(wchar_t* s, int n)
{
    if (s_camNear < 0)
        return false;
    if (s_cams[s_camNear].speed)
        _snwprintf(s, n, L"КАМЕРА %d · %d м", s_cams[s_camNear].speed, s_camNearDist);
    else
        _snwprintf(s, n, L"КАМЕРА · %d м", s_camNearDist);
    return true;
}

// ---------------------------------------------------------------- programs (VestaLaunch's, built in)
// The "Apps" screen lists programs from apps.txt next to the exe; shortcuts are added, changed and removed
// right there (own on-screen keyboard and file browser: the unit has neither). The same code as VestaLaunch
// (native/launch.cpp), without its part for the stock shell: no Nav_* events, VestaHome doesn't hide.

static const wchar_t* kKeysRu[4] = { L"1234567890-_", L"йцукенгшщзхъ", L"фывапролджэё", L"ячсмитьбю.,!" };
static const wchar_t* kKeysEn[4] = { L"1234567890-_", L"qwertyuiop()", L"asdfghjkl:;\"", L"zxcvbnm.,/\\=" };
enum { F_NAME, F_PATH, F_PARAMS, F_ICON, F_CAMS };   // fields of the shortcut editor; F_CAMS: the camera file

// Programs of the "Apps" screen, from apps.txt: "Name | \path\to\program.exe | class!title | parameters | icon".
struct App {
    wchar_t name[48];
    wchar_t path[MAX_PATH];
    wchar_t wclass[64];    // optional window to raise, "class!title" like AppLauncher; empty = any window of the process
    wchar_t wtitle[96];
    wchar_t params[200];   // command line passed to the program, may be empty
    wchar_t iconPath[MAX_PATH];   // own icon: .png, .ico or an exe/dll to take it from; relative = from our folder
    HICON   icon;          // the icon from an exe/dll/ico (the program's own if no iconPath), NULL if none
    Img     img;           // the icon from a .png, at most 64 x 64
    bool    exists;
};

// VestaHome is topmost (above the stock shell), so a started program must be raised above it explicitly.
// After the start its window is looked for every 300 ms for up to kWaitMs. The guard: while a raised program
// runs, every window of its process is kept topmost (a dialog it opens is a normal window and would land under
// VestaHome, and the program with it); when it closes, VestaHome is in front again.
enum { TIMER_WAIT = 3, TIMER_GUARD = 4, kWaitMs = 20000, kGuardMs = 500, kMaxRaised = 16 };
static DWORD s_waitPid;
static int   s_waitApp = -1;
static DWORD s_waitTick;
static HWND  s_raised[kMaxRaised];   // windows we made topmost; dropped again when the program closes
static int   s_raisedCount;
static HANDLE s_guard;        // process of the program in front; NULL = no guard
static DWORD  s_guardPid;
static bool   s_guardOwned;   // s_guard was opened here (otherwise it is s_running's handle, not ours to close)

// Programs started from here: their processes, so that a second tap brings the running copy to the front
// instead of starting another one.
enum { kMaxRunning = 16 };
struct Running { wchar_t path[MAX_PATH]; HANDLE proc; DWORD pid; };
static Running s_running[kMaxRunning];

enum { kMaxApps = 64, kAppsPerPage = 8 };
static App   s_apps[kMaxApps];
static int   s_appCount;
static int   s_page;

// Shortcut editor: works on a copy, written to apps.txt on "Save".
static bool  s_editMode;          // apps screen: a tile opens its editor instead of starting the program
static int   s_editIdx;           // index in s_apps, -1 = new shortcut
static App   s_edit;
static bool  s_delArmed;          // "Delete" was pressed once; the second press deletes

static wchar_t*       s_keysTarget;   // the text being typed: a shortcut's field or a setting
static int            s_keysCap;
static const wchar_t* s_keysTitle;
static Screen         s_keysBack;     // where "Done" and "Cancel" return to
static wchar_t s_keysText[200];
static bool    s_keysEn, s_keysShift;

// File browser for the program path: folders and .exe files of s_fdir.
struct FileEnt { wchar_t name[100]; bool dir; };
enum { kMaxFiles = 256, kFilesPerPage = 10 };
static FileEnt s_files[kMaxFiles];
static int     s_fileCount, s_filePage;
static wchar_t s_fdir[MAX_PATH];  // ends with a backslash
static int     s_filesFor;        // F_PATH: .exe files; F_ICON: pictures (.png, .ico) and .exe/.dll

static bool LoadImg(const wchar_t* path, Img* im);

static int AppsPages()
{
    int tiles = s_appCount + (s_appCount < kMaxApps ? 1 : 0);   // + the "Add" tile
    return (tiles + kAppsPerPage - 1) / kAppsPerPage;
}

static int FilePages()
{
    int pages = (s_fileCount + kFilesPerPage - 1) / kFilesPerPage;
    return pages < 1 ? 1 : pages;
}

// A line too long for maxw shows its end after "…" (paths, typed text). Returns its right edge.
static int TTail(HDC dc, int x, int y, const wchar_t* s, int size, COLORREF c, int weight, int maxw)
{
    if (TextWidth(dc, s, size, weight) <= maxw)
        return T(dc, x, y, s, size, c, weight);
    wchar_t buf[MAX_PATH + 2];
    const wchar_t* p = s;
    do {
        p++;
        _snwprintf(buf, MAX_PATH + 2, L"…%s", p);
        buf[MAX_PATH + 1] = 0;
    } while (*p && TextWidth(dc, buf, size, weight) > maxw);
    return T(dc, x, y, buf, size, c, weight);
}

static bool Exists(const wchar_t* path)
{
    return GetFileAttributes(path) != 0xFFFFFFFF;
}

#define APPS_HEADER \
    "# Programs of the \"Apps\" screen: one per line, \"Name | full path to the exe\".\r\n" \
    "# Lines starting with # are ignored. Save the file as UTF-8.\r\n" \
    "# The started program is raised above the stock shell. If it starts another program (a launcher,\r\n" \
    "# a MortScript), name the window to raise as the third field: \"Name | path | class!title\".\r\n" \
    "# The fourth field is the program's command line: \"Name | path | | parameters\" (third field may be empty).\r\n" \
    "# The fifth field is an own icon: \"Name | path | | | Icons\\name.png\" - a .png (up to 256 x 256), .ico or\r\n" \
    "# an exe/dll to take the icon from; a path without a drive/root is from the launcher's folder.\r\n" \
    "# Shortcuts changed on the \"Apps\" screen rewrite this file; comments other than these are not kept.\r\n"

static const char kDefaultApps[] =
    APPS_HEADER
    "\xD0\xAF\xD0\xBD\xD0\xB4\xD0\xB5\xD0\xBA\xD1\x81 \xD0\xBA\xD0\xB0\xD1\x80\xD1\x82\xD1\x8B | \\SDMMC\\YaMapsCE\\YaMapsCE.exe\r\n"
    "OSM \xD0\xBA\xD0\xB0\xD1\x80\xD1\x82\xD1\x8B | \\SDMMC\\OSMapsCE\\OSMapsCE.exe\r\n"
    "CityGuide | \\SDMMC\\Navi\\CityGuide\\CityGuideCE.exe\r\n";

static void FreeAppIcon(App& a)
{
    if (a.icon)
        DestroyIcon(a.icon);
    if (a.img.px)
        free(a.img.px);
    a.icon = NULL;
    a.img.px = NULL;
}

// A shortcut's own icon is kept at most 64 x 64 (the tile's size): averaged down from a bigger picture.
static void ShrinkImg(Img* im)
{
    if (im->w <= 64 && im->h <= 64)
        return;
    int w = im->w > 64 ? 64 : im->w, h = im->h > 64 ? 64 : im->h;
    unsigned char* out = (unsigned char*)malloc(w * h * 4);
    if (!out)
        return;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int x0 = x * im->w / w, x1 = (x + 1) * im->w / w, y0 = y * im->h / h, y1 = (y + 1) * im->h / h;
            int s[4] = { 0, 0, 0, 0 }, n = (x1 - x0) * (y1 - y0);
            for (int sy = y0; sy < y1; sy++)
                for (int sx = x0; sx < x1; sx++)
                    for (int k = 0; k < 4; k++)
                        s[k] += im->px[(sy * im->w + sx) * 4 + k];
            for (int k = 0; k < 4; k++)
                out[(y * w + x) * 4 + k] = (unsigned char)(s[k] / n);
        }
    free(im->px);
    im->px = out;
    im->w = w;
    im->h = h;
}

// The tile's icon: the one chosen for the shortcut (if it loads), otherwise the program's own.
static void LoadAppIcon(App& a)
{
    a.icon = NULL;
    a.img.px = NULL;
    if (a.iconPath[0]) {
        wchar_t file[MAX_PATH];
        if (a.iconPath[0] == L'\\' || wcschr(a.iconPath, L':'))
            wcscpy(file, a.iconPath);
        else
            _snwprintf(file, MAX_PATH, L"%s%s", s_dir, a.iconPath);
        file[MAX_PATH - 1] = 0;
        int n = (int)wcslen(file);
        if (n > 4 && !_wcsicmp(file + n - 4, L".png")) {
            if (LoadImg(file, &a.img)) {
                ShrinkImg(&a.img);
                return;
            }
        } else if (ExtractIconEx(file, 0, &a.icon, NULL, 1) && a.icon) {
            return;
        }
        a.icon = NULL;
    }
    if (a.exists)
        ExtractIconEx(a.path, 0, &a.icon, NULL, 1);
}

// Reads apps.txt (UTF-8, optional BOM); writes an example list on the first start.
static void LoadApps()
{
    for (int i = 0; i < s_appCount; i++)
        FreeAppIcon(s_apps[i]);
    s_appCount = 0;

    wchar_t path[MAX_PATH];
    _snwprintf(path, MAX_PATH, L"%sapps.txt", s_dir);
    DWORD got;
    char* buf = ReadAll(path, &got);
    if (!buf) {
        HANDLE h = CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        if (h != INVALID_HANDLE_VALUE) {
            DWORD w;
            WriteFile(h, kDefaultApps, sizeof(kDefaultApps) - 1, &w, NULL);
            CloseHandle(h);
        }
        buf = ReadAll(path, &got);
        if (!buf)
            return;
    }
    char* p = buf;
    if ((unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF)
        p += 3;
    while (*p && s_appCount < kMaxApps) {
        char* end = strchr(p, '\n');
        if (end)
            *end = 0;
        wchar_t line[400];
        int n = MultiByteToWideChar(CP_UTF8, 0, p, -1, line, 400);
        p = end ? end + 1 : p + strlen(p);
        if (n <= 0)
            continue;
        Trim(line);
        wchar_t* bar = wcschr(line, L'|');
        if (!line[0] || line[0] == L'#' || line[0] == L';' || !bar)
            continue;
        *bar = 0;
        App& a = s_apps[s_appCount];
        wcsncpy(a.name, line, 47);
        a.name[47] = 0;
        Trim(a.name);
        // optional third field: window "class!title" (a leading '!' as in AppLauncher profiles is allowed),
        // optional fourth: the program's command line; optional fifth: its own icon
        a.wclass[0] = a.wtitle[0] = a.params[0] = a.iconPath[0] = 0;
        wchar_t* bar2 = wcschr(bar + 1, L'|');
        if (bar2) {
            *bar2 = 0;
            wchar_t* w = bar2 + 1;
            wchar_t* bar3 = wcschr(w, L'|');
            if (bar3) {
                *bar3 = 0;
                wchar_t* bar4 = wcschr(bar3 + 1, L'|');
                if (bar4) {
                    *bar4 = 0;
                    wcsncpy(a.iconPath, bar4 + 1, MAX_PATH - 1);
                    a.iconPath[MAX_PATH - 1] = 0;
                    Trim(a.iconPath);
                }
                wcsncpy(a.params, bar3 + 1, 199);
                a.params[199] = 0;
                Trim(a.params);
            }
            Trim(w);
            if (*w == L'!')
                w++;
            wchar_t* ex = wcschr(w, L'!');
            if (ex) {
                *ex = 0;
                wcsncpy(a.wtitle, ex + 1, 95);
                a.wtitle[95] = 0;
            }
            wcsncpy(a.wclass, w, 63);
            a.wclass[63] = 0;
            Trim(a.wclass);
            Trim(a.wtitle);
        }
        wcsncpy(a.path, bar + 1, MAX_PATH - 1);
        a.path[MAX_PATH - 1] = 0;
        Trim(a.path);
        if (!a.name[0] || !a.path[0])
            continue;
        a.exists = Exists(a.path);
        LoadAppIcon(a);
        s_appCount++;
    }
    free(buf);
}

// Writes s_apps back to apps.txt, through a temporary file so a power cut can't leave half a list.
static bool SaveApps()
{
    wchar_t path[MAX_PATH], tmp[MAX_PATH];
    _snwprintf(path, MAX_PATH, L"%sapps.txt", s_dir);
    _snwprintf(tmp, MAX_PATH, L"%sapps.tmp", s_dir);
    HANDLE h = CreateFile(tmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return false;
    DWORD w;
    bool ok = WriteFile(h, APPS_HEADER, sizeof(APPS_HEADER) - 1, &w, NULL) != 0;
    for (int i = 0; i < s_appCount && ok; i++) {
        const App& a = s_apps[i];
        wchar_t win[170], line[1000];
        if (a.wtitle[0])
            _snwprintf(win, 170, L"!%s!%s", a.wclass, a.wtitle);
        else
            wcscpy(win, a.wclass);
        if (a.iconPath[0])
            _snwprintf(line, 1000, L"%s | %s | %s | %s | %s\r\n", a.name, a.path, win, a.params, a.iconPath);
        else if (a.params[0])
            _snwprintf(line, 1000, L"%s | %s | %s | %s\r\n", a.name, a.path, win, a.params);
        else if (win[0])
            _snwprintf(line, 1000, L"%s | %s | %s\r\n", a.name, a.path, win);
        else
            _snwprintf(line, 1000, L"%s | %s\r\n", a.name, a.path);
        line[999] = 0;
        char out[3000];
        int n = WideCharToMultiByte(CP_UTF8, 0, line, -1, out, sizeof(out), NULL, NULL);
        ok = n > 1 && WriteFile(h, out, n - 1, &w, NULL);
    }
    CloseHandle(h);
    if (!ok) {
        DeleteFile(tmp);
        return false;
    }
    DeleteFile(path);
    return MoveFile(tmp, path) != 0;
}

static bool Alive(HANDLE proc)
{
    return proc && WaitForSingleObject(proc, 0) == WAIT_TIMEOUT;
}

// The process of a program started from here that still runs, 0 if none. Finished ones are forgotten.
static DWORD RunningPid(const wchar_t* path)
{
    DWORD pid = 0;
    for (int k = 0; k < kMaxRunning; k++) {
        Running& r = s_running[k];
        if (r.proc && !Alive(r.proc)) {
            CloseHandle(r.proc);
            r.proc = NULL;
        }
        if (r.proc && !_wcsicmp(r.path, path))
            pid = r.pid;
    }
    return pid;
}

static void Remember(const wchar_t* path, HANDLE proc, DWORD pid)
{
    int k = 0;
    while (k < kMaxRunning - 1 && s_running[k].proc)
        k++;
    if (s_running[k].proc)   // all slots busy: the last one is forgotten
        CloseHandle(s_running[k].proc);
    wcscpy(s_running[k].path, path);
    s_running[k].proc = proc;
    s_running[k].pid = pid;
}

struct FindApp { HWND found; };
static BOOL CALLBACK EnumApp(HWND h, LPARAM lp);

static void RunApp(int i)
{
    const App& a = s_apps[i];
    // Already running (started from here, or its window "class!title" is open): bring it to the front.
    s_waitPid = RunningPid(a.path);
    s_waitApp = i;
    FindApp f = { NULL };
    if (!s_waitPid && (a.wclass[0] || a.wtitle[0]))
        EnumWindows(EnumApp, (LPARAM)&f);
    if (s_waitPid || f.found) {
        s_waitTick = GetTickCount();
        SetTimer(s_wnd, TIMER_WAIT, 300, NULL);
        _snwprintf(s_status, 96, L"уже запущена: %s", a.name);
        return;
    }
    s_waitApp = -1;
    PROCESS_INFORMATION pi;
#ifdef UNDER_CE
    LPSTARTUPINFO si = NULL;          // must be NULL on Windows CE
    wchar_t cmd[200];                 // CE passes just the arguments
    wcscpy(cmd, a.params);
    wchar_t* args = cmd[0] ? cmd : NULL;
#else
    STARTUPINFO sib;
    memset(&sib, 0, sizeof(sib));
    sib.cb = sizeof(sib);
    LPSTARTUPINFO si = &sib;          // desktop Windows requires it
    wchar_t cmd[MAX_PATH + 204];      // desktop: the program name comes first
    _snwprintf(cmd, MAX_PATH + 204, L"\"%s\" %s", a.path, a.params);
    wchar_t* args = cmd;
#endif
    if (a.exists && CreateProcess(a.path, args, NULL, NULL, FALSE, 0, NULL, NULL, si, &pi)) {
        CloseHandle(pi.hThread);
        Remember(a.path, pi.hProcess, pi.dwProcessId);
        s_waitPid = pi.dwProcessId;
        s_waitApp = i;
        s_waitTick = GetTickCount();
        SetTimer(s_wnd, TIMER_WAIT, 300, NULL);
        _snwprintf(s_status, 96, L"запускается: %s", a.name);
    } else {
        _snwprintf(s_status, 96, L"не запускается: %s", a.name);
    }
}

// Window of the program being started: by class/title from apps.txt, otherwise any visible
// top-level window of its process.

static BOOL CALLBACK EnumApp(HWND h, LPARAM lp)
{
    if (h == s_wnd || !IsWindowVisible(h) || GetParent(h))
        return TRUE;
    const App& a = s_apps[s_waitApp];
    bool match;
    if (a.wclass[0] || a.wtitle[0]) {
        wchar_t cls[64] = L"", title[96] = L"";
        GetClassName(h, cls, 64);
        GetWindowText(h, title, 96);
        match = (!a.wclass[0] || !wcscmp(cls, a.wclass)) && (!a.wtitle[0] || !wcscmp(title, a.wtitle));
    } else {
        DWORD pid = 0;
        GetWindowThreadProcessId(h, &pid);
        match = pid == s_waitPid;
    }
    if (match) {
        ((FindApp*)lp)->found = h;
        return FALSE;
    }
    return TRUE;
}

static void StopGuard()
{
    KillTimer(s_wnd, TIMER_GUARD);
    if (s_guard && s_guardOwned)
        CloseHandle(s_guard);
    s_guard = NULL;
}

static void StartGuard(DWORD pid)
{
    StopGuard();
#ifdef UNDER_CE
    s_guard = OpenProcess(0, FALSE, pid);
#else
    s_guard = OpenProcess(SYNCHRONIZE, FALSE, pid);
#endif
    s_guardOwned = s_guard != NULL;
    for (int k = 0; !s_guard && k < kMaxRunning; k++)   // no OpenProcess: the handle we started it with
        if (s_running[k].proc && s_running[k].pid == pid)
            s_guard = s_running[k].proc;
    s_guardPid = pid;
    if (s_guard)
        SetTimer(s_wnd, TIMER_GUARD, kGuardMs, NULL);
}

struct Guarded { HWND h[16]; int n; };

static BOOL CALLBACK EnumGuard(HWND h, LPARAM lp)
{
    Guarded* g = (Guarded*)lp;
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    if (pid == s_guardPid && IsWindowVisible(h) && !(GetWindowLong(h, GWL_EXSTYLE) & WS_EX_TOPMOST) && g->n < 16)
        g->h[g->n++] = h;
    return TRUE;
}

// Programs we raised stay above everything: put them back into the normal z-order.
static void DropRaised()
{
    for (int i = 0; i < s_raisedCount; i++)
        if (IsWindow(s_raised[i]))
            SetWindowPos(s_raised[i], HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    s_raisedCount = 0;
}

// Every 0.5 s while a raised program runs: its windows that are not topmost (a new dialog) are raised,
// bottom one first so that their order stays; not while VestaHome is in front (the program stepped aside).
// When it has closed, VestaHome (under it all the time) gets the focus back.
static void OnGuardTimer()
{
    if (!Alive(s_guard)) {
        StopGuard();
        DropRaised();
        SetForegroundWindow(s_wnd);
        InvalidateRect(s_wnd, NULL, FALSE);
        return;
    }
    if (GetForegroundWindow() == s_wnd)
        return;
    Guarded g;
    g.n = 0;
    EnumWindows(EnumGuard, (LPARAM)&g);   // top to bottom
    for (int k = g.n - 1; k >= 0; k--) {
        SetWindowPos(g.h[k], HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        if (s_raisedCount < kMaxRaised)
            s_raised[s_raisedCount++] = g.h[k];
    }
}

static void OnWaitTimer()
{
    if (s_waitApp < 0) {
        KillTimer(s_wnd, TIMER_WAIT);
        return;
    }
    FindApp f = { NULL };
    EnumWindows(EnumApp, (LPARAM)&f);
    if (f.found) {
        KillTimer(s_wnd, TIMER_WAIT);
        SetWindowPos(f.found, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        SetForegroundWindow(f.found);
        if (s_raisedCount < kMaxRaised)
            s_raised[s_raisedCount++] = f.found;
        DWORD pid = 0;
        GetWindowThreadProcessId(f.found, &pid);
        StartGuard(pid);
        s_status[0] = 0;
        s_waitApp = -1;
    } else if (GetTickCount() - s_waitTick > kWaitMs) {
        KillTimer(s_wnd, TIMER_WAIT);
        _snwprintf(s_status, 96, L"окно не найдено: %s", s_apps[s_waitApp].name);
        s_waitApp = -1;
    }
}

// ---- shortcut editor

static void OpenEditor(int i)
{
    s_editIdx = i;
    if (i >= 0)
        s_edit = s_apps[i];
    else
        memset(&s_edit, 0, sizeof(s_edit));
    s_edit.icon = NULL;   // the icons stay owned by s_apps
    s_edit.img.px = NULL;
    s_delArmed = false;
    s_status[0] = 0;
    s_screen = S_EDIT;
}

// Writes the list, reads it back (icons, "no file" marks) and returns to the apps screen.
static void FinishEdit()
{
    bool ok = SaveApps();
    LoadApps();
    s_screen = S_APPS;
    if (s_page >= AppsPages())
        s_page = AppsPages() - 1;
    if (ok)
        s_status[0] = 0;
    else
        wcscpy(s_status, L"не удалось записать apps.txt");
}

static void SaveEdit()
{
    if (!s_edit.name[0] || !s_edit.path[0]) {
        wcscpy(s_status, L"нужны название и программа");
        return;
    }
    if (s_editIdx < 0) {
        s_apps[s_appCount++] = s_edit;
    } else {
        HICON icon = s_apps[s_editIdx].icon;   // freed by the reload in FinishEdit
        Img img = s_apps[s_editIdx].img;
        s_apps[s_editIdx] = s_edit;
        s_apps[s_editIdx].icon = icon;
        s_apps[s_editIdx].img = img;
    }
    FinishEdit();
}

static void DeleteEdit()
{
    FreeAppIcon(s_apps[s_editIdx]);
    memmove(&s_apps[s_editIdx], &s_apps[s_editIdx + 1], (s_appCount - s_editIdx - 1) * sizeof(App));
    s_appCount--;
    s_waitApp = -1;   // indices moved: stop waiting for a window of a started program
    s_delArmed = false;
    FinishEdit();
}

static void OpenKeysFor(wchar_t* target, int cap, const wchar_t* title, Screen back)
{
    s_keysTarget = target;
    s_keysCap = cap;
    s_keysTitle = title;
    s_keysBack = back;
    wcsncpy(s_keysText, target, 199);
    s_keysText[199] = 0;
    s_keysShift = false;
    s_screen = S_KEYS;
}

static void OpenKeys(int field)
{
    if (field == F_NAME)
        OpenKeysFor(s_edit.name, 47, L"НАЗВАНИЕ", S_EDIT);
    else
        OpenKeysFor(s_edit.params, 199, L"ПАРАМЕТРЫ ЗАПУСКА", S_EDIT);
}

static void KeyType(wchar_t ch)
{
    int n = (int)wcslen(s_keysText);
    if (n >= s_keysCap)
        return;
    if (s_keysShift) {   // one-shot, like a phone keyboard
        CharUpperBuff(&ch, 1);
        s_keysShift = false;
    }
    s_keysText[n] = ch;
    s_keysText[n + 1] = 0;
}

static void KeyBack()
{
    int n = (int)wcslen(s_keysText);
    if (n > 0)
        s_keysText[n - 1] = 0;
}

static void CfgKeysDone();

static void KeysDone()
{
    wcscpy(s_keysTarget, s_keysText);
    Trim(s_keysTarget);
    s_screen = s_keysBack;
    if (s_keysBack != S_EDIT)
        CfgKeysDone();
}

static int CmpFiles(const void* pa, const void* pb)
{
    const FileEnt* a = (const FileEnt*)pa;
    const FileEnt* b = (const FileEnt*)pb;
    if (a->dir != b->dir)
        return a->dir ? -1 : 1;
    return _wcsicmp(a->name, b->name);
}

static bool HasExt(const wchar_t* name, const wchar_t* ext)
{
    int n = (int)wcslen(name);
    return n > 4 && !_wcsicmp(name + n - 4, ext);
}

// Folders and .exe files of s_fdir (for an icon: .png, .ico, .exe, .dll; for the cameras: .txt), folders first.
static void ListDir()
{
    s_fileCount = 0;
    s_filePage = 0;
    wchar_t mask[MAX_PATH];
    _snwprintf(mask, MAX_PATH, L"%s*", s_fdir);
    WIN32_FIND_DATA fd;
    HANDLE h = FindFirstFile(mask, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return;
    do {
        bool dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        int n = (int)wcslen(fd.cFileName);
        if (n >= 100 || !wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L".."))
            continue;
        bool want = s_filesFor == F_CAMS ? HasExt(fd.cFileName, L".txt") : HasExt(fd.cFileName, L".exe") ||
                    (s_filesFor == F_ICON && (HasExt(fd.cFileName, L".png") || HasExt(fd.cFileName, L".ico") ||
                                              HasExt(fd.cFileName, L".dll")));
        if (!dir && !want)
            continue;
        s_files[s_fileCount].dir = dir;
        wcscpy(s_files[s_fileCount].name, fd.cFileName);
        s_fileCount++;
    } while (s_fileCount < kMaxFiles && FindNextFile(h, &fd));
    FindClose(h);
    qsort(s_files, s_fileCount, sizeof(FileEnt), CmpFiles);
}

// Points the browser at the folder of file, if it has one that exists.
static bool BrowseFolderOf(const wchar_t* file)
{
    const wchar_t* slash = wcsrchr(file, L'\\');
    if (!slash || slash == file)
        return false;
    wchar_t dir[MAX_PATH];
    int n = (int)(slash - file);
    wcsncpy(dir, file, n);
    dir[n] = 0;
    DWORD attr = GetFileAttributes(dir);
    if (attr == 0xFFFFFFFF || !(attr & FILE_ATTRIBUTE_DIRECTORY))
        return false;
    wcscpy(s_fdir, dir);
    wcscat(s_fdir, L"\\");
    return true;
}

// For the program the browser opens in its folder, otherwise at the root. For the icon: in the current
// icon's folder, otherwise our Icons\ folder, otherwise the program's folder.
static void OpenFiles(int field)
{
    s_filesFor = field;
    wcscpy(s_fdir, L"\\");
    wchar_t icons[MAX_PATH];
    _snwprintf(icons, MAX_PATH, L"%sIcons\\", s_dir);
    if (field == F_ICON) {
        wchar_t cur[MAX_PATH];
        if (s_edit.iconPath[0] == L'\\' || wcschr(s_edit.iconPath, L':') || !s_edit.iconPath[0])
            wcscpy(cur, s_edit.iconPath);
        else
            _snwprintf(cur, MAX_PATH, L"%s%s", s_dir, s_edit.iconPath);
        if (!(cur[0] && BrowseFolderOf(cur)) && !BrowseFolderOf(icons))
            BrowseFolderOf(s_edit.path);
    } else if (field == F_CAMS) {
        wchar_t cur[MAX_PATH];
        if (s_cfg.camFile[0] == L'\\' || wcschr(s_cfg.camFile, L':'))
            wcscpy(cur, s_cfg.camFile);
        else
            _snwprintf(cur, MAX_PATH, L"%s%s", s_dir, s_cfg.camFile);
        if (!BrowseFolderOf(cur))
            BrowseFolderOf(s_dir);
    } else {
        BrowseFolderOf(s_edit.path);
    }
    ListDir();
    s_screen = S_FILES;
}

static void UpDir()
{
    int n = (int)wcslen(s_fdir);
    if (n <= 1)
        return;   // already at the root
    s_fdir[n - 1] = 0;
    wchar_t* slash = wcsrchr(s_fdir, L'\\');
    if (slash)
        slash[1] = 0;
    else
        wcscpy(s_fdir, L"\\");
    ListDir();
}

// A folder opens; an exe becomes the program, and names the shortcut if it has no name yet.
static void PickFile(int i)
{
    const FileEnt& f = s_files[i];
    if (wcslen(s_fdir) + wcslen(f.name) + 2 >= MAX_PATH)
        return;
    if (f.dir) {
        wcscat(s_fdir, f.name);
        wcscat(s_fdir, L"\\");
        ListDir();
        return;
    }
    if (s_filesFor == F_CAMS) {   // like the icon: relative inside our folder
        int n = (int)wcslen(s_dir);
        _snwprintf(s_cfg.camFile, MAX_PATH, L"%s%s", _wcsnicmp(s_fdir, s_dir, n) ? s_fdir : s_fdir + n, f.name);
        s_cfg.camFile[MAX_PATH - 1] = 0;
        SaveCfg();
        s_camLoaded = false;   // read again at the next fix
        s_screen = S_WSET_SPEED;
        return;
    }
    if (s_filesFor == F_ICON) {   // inside our folder kept relative: survives a move of the card's folders
        int n = (int)wcslen(s_dir);
        if (!_wcsnicmp(s_fdir, s_dir, n))
            _snwprintf(s_edit.iconPath, MAX_PATH, L"%s%s", s_fdir + n, f.name);
        else
            _snwprintf(s_edit.iconPath, MAX_PATH, L"%s%s", s_fdir, f.name);
        s_edit.iconPath[MAX_PATH - 1] = 0;
        s_screen = S_EDIT;
        return;
    }
    wcscpy(s_edit.path, s_fdir);
    wcscat(s_edit.path, f.name);
    if (!s_edit.name[0]) {
        wcsncpy(s_edit.name, f.name, 47);
        s_edit.name[47] = 0;
        int n = (int)wcslen(s_edit.name);
        if (n > 4 && !_wcsicmp(s_edit.name + n - 4, L".exe"))
            s_edit.name[n - 4] = 0;
    }
    s_screen = S_EDIT;
}

// ---- programs: screens

// A program's icon: its own picture or icon, otherwise its first letter in a ring.
static void AppIcon(HDC dc, const App& a, int x, int y, int size, bool d)
{
    if (a.img.px) {
        DrawImg(a.img, x, y, size, size);
    } else if (a.icon) {
        DrawIconEx(dc, x, y, a.icon, size, size, 0, NULL, DI_NORMAL);
    } else {
        wchar_t letter[2] = { a.name[0], 0 };
        Pen ring(dc, x, y, size, d ? kInk : kText);
        ring.Circle(24, 24, 20);
        T(dc, x + size / 2, y + size * 42 / 64, letter, size * 26 / 64, d ? kInk : kText, 700, AL_MID);
    }
}

// Apps, a main screen: program tiles 184 x 150, 4 x 2 per page, the last one "Add" as a dashed empty slot; in edit
// mode (the pencil in the top bar) the tiles are framed orange and open the editor. Page dots under the tiles.
static void DrawApps(HDC dc)
{
    Frame(dc, s_editMode ? L"ПРИЛОЖЕНИЯ · ИЗМЕНЕНИЕ" : L"ПРИЛОЖЕНИЯ", A_BACK, IC_EDIT, A_EDITMODE, s_editMode);
    int first = s_page * kAppsPerPage;
    int tiles = s_appCount + (s_appCount < kMaxApps ? 1 : 0);
    for (int k = 0; k < kAppsPerPage && first + k < tiles; k++) {
        int i = first + k, x = 16 + (k % 4) * 194, y = 64 + (k / 4) * 162;
        if (i == s_appCount) {   // "Add"
            bool d = Zone(x, y, 184, 150, A_ADD);
            if (d)
                R(dc, x, y, 184, 150, 14, kOrange);
            else
                DashRect(dc, x, y, 184, 150, 14, kLine);
            Ico(dc, IC_ADD, x + 68, y + 30, 48, d ? kInk : kDim);
            T(dc, x + 92, y + 124, L"ДОБАВИТЬ", 15, d ? kInk : kDim, 600, AL_MID);
            continue;
        }
        const App& a = s_apps[i];
        bool d = Zone(x, y, 184, 150, A_RUN, i);
        R(dc, x, y, 184, 150, 14, d ? kOrange : kPanel, d ? CLR_INVALID : s_editMode ? kOrange : kLine);
        if (!d) {   // orange facet in the top right corner, following the rounded corner (radius 14)
            POINT f[12];
            int n = 0;
            f[n].x = x + 184 - 38; f[n].y = y + 1; n++;
            for (int j = 0; j <= 8; j++, n++) {
                double an = (-90 + j * 90 / 8.0) * 3.14159265 / 180;
                f[n].x = x + 184 - 14 + (int)(13 * cos(an) + 0.5);
                f[n].y = y + 14 + (int)(13 * sin(an) + 0.5);
            }
            f[n].x = x + 184 - 1; f[n].y = y + 38; n++;
            Poly(dc, f, n, kFacet);
        }
        AppIcon(dc, a, x + 60, y + 24, 64, d);
        T(dc, x + 92, y + 124, a.exists ? a.name : L"нет файла", 15, d ? kInk : a.exists ? kMuted : kDim,
          d ? 700 : 600, AL_MID, 168);
    }
    if (s_appCount == 0) {
        T(dc, 400, 270, L"Список программ пуст", 22, kText, 700, AL_MID);
        T(dc, 400, 302, L"Нажмите «Добавить» или впишите программы в apps.txt", 17, kMuted, 400, AL_MID, 760);
    }
    PageDots(dc, s_page, AppsPages());
}

static const wchar_t* kFieldCaption[4] = { L"НАЗВАНИЕ", L"ПРОГРАММА", L"ПАРАМЕТРЫ ЗАПУСКА", L"ИКОНКА" };
static const wchar_t* kFieldEmpty[4] = { L"нажмите, чтобы ввести", L"нажмите, чтобы выбрать exe", L"нет",
    L"своя у программы; нажмите, чтобы выбрать png, ico или exe" };

// One shortcut: four fields (keyboard or file browser behind each), cancel / delete / save.
// A chosen icon has a "reset" button at the end of its field.
static void DrawEdit(HDC dc)
{
    Frame(dc, s_editIdx < 0 ? L"НОВЫЙ ЯРЛЫК" : L"ИЗМЕНИТЬ ЯРЛЫК", A_CANCEL);
    for (int k = 0; k < 4; k++) {
        int y = 72 + k * 80;
        const wchar_t* v = k == F_NAME ? s_edit.name : k == F_PATH ? s_edit.path :
                           k == F_PARAMS ? s_edit.params : s_edit.iconPath;
        bool reset = k == F_ICON && v[0];
        int w = reset ? 640 : 768;
        bool d = Zone(16, y, w, 70, A_FIELD, k);
        R(dc, 16, y, w, 70, 10, d ? kOrange : kPanel, d ? CLR_INVALID : kLine);
        T(dc, 36, y + 26, kFieldCaption[k], 13, d ? kInk : kMuted, 700);
        if (v[0])
            TTail(dc, 36, y + 55, v, 22, d ? kInk : kText, 700, w - 88);
        else
            T(dc, 36, y + 55, kFieldEmpty[k], 18, d ? kInk : kDim, 400, AL_START, w - 88);
        Ico(dc, IC_NEXT, 16 + w - 48, y + 19, 32, d ? kInk : kMuted);
        if (reset)
            Button(dc, 668, y, 116, 70, L"СБРОС", B_N, IC_NONE, 17, 0, A_NOICON);
    }
    Dock(dc);
    Button(dc, 16, 410, 220, 58, L"ОТМЕНА", B_N, IC_NONE, 18, 0, A_CANCEL);
    if (s_editIdx >= 0)   // the second press deletes
        Button(dc, 290, 410, 220, 58, s_delArmed ? L"ТОЧНО УДАЛИТЬ?" : L"УДАЛИТЬ", s_delArmed ? B_FILL : B_N,
               IC_NONE, 18, 0, A_DELETE);
    Button(dc, 564, 410, 220, 58, L"СОХРАНИТЬ", B_PRI, IC_NONE, 18, 0, A_SAVE);
}

// On-screen keyboard (like the mockup's Wi-Fi keyboard): the field and "Done" on top, 4 rows of 12 keys,
// then Shift, language, space, backspace.
static void DrawKeys(HDC dc)
{
    Frame(dc, s_keysTitle, A_CANCEL);
    R(dc, 16, 68, 600, 60, 10, kPanel, kLine);
    int end = TTail(dc, 32, 106, s_keysText, 26, kText, 700, 560);
    R(dc, end + 4, 84, 3, 28, 0, kOrange);
    Button(dc, 628, 68, 156, 60, L"ГОТОВО", B_PRI, IC_NONE, 18, 0, A_OK);
    const wchar_t** rows = s_keysEn ? kKeysEn : kKeysRu;
    for (int r = 0; r < 4; r++)
        for (int c = 0; rows[r][c]; c++) {
            int x = 13 + c * 65, y = 140 + r * 66;
            wchar_t ch[2] = { rows[r][c], 0 };
            if (s_keysShift)
                CharUpperBuff(ch, 1);
            bool d = Zone(x, y, 60, 60, A_CHAR, rows[r][c]);
            R(dc, x, y, 60, 60, 10, d ? kOrange : kPanel, d ? CLR_INVALID : kLine);
            T(dc, x + 30, y + 40, ch, 24, d ? kInk : kText, 700, AL_MID);
        }
    Button(dc, 13, 404, 125, 60, NULL, s_keysShift ? B_SEL : B_FILL, IC_SHIFT, 18, 32, A_SHIFT);
    Button(dc, 143, 404, 125, 60, s_keysEn ? L"ENG" : L"РУС", B_FILL, IC_NONE, 20, 0, A_LANG);
    bool d = Zone(273, 404, 385, 60, A_CHAR, L' ');
    R(dc, 273, 404, 385, 60, 10, d ? kOrange : kPanel, d ? CLR_INVALID : kLine);
    R(dc, 397, 438, 136, 4, 2, d ? kInk : kMuted);
    Button(dc, 663, 404, 125, 60, NULL, B_FILL, IC_BKSP, 18, 36, A_BKSP);
}

// Choosing the program: the folder's path, folders and .exe files 2 x 5 per page.
static void DrawFiles(HDC dc)
{
    Frame(dc, s_filesFor == F_ICON ? L"ВЫБОР ИКОНКИ" : s_filesFor == F_CAMS ? L"ФАЙЛ КАМЕР" : L"ВЫБОР ПРОГРАММЫ",
          A_CANCEL);
    TTail(dc, 20, 84, s_fdir, 15, kMuted, 400, 760);
    int first = s_filePage * kFilesPerPage;
    for (int k = 0; k < kFilesPerPage && first + k < s_fileCount; k++) {
        const FileEnt& f = s_files[first + k];
        int x = 16 + (k / 5) * 388, y = 96 + (k % 5) * 60;
        bool d = Zone(x, y, 380, 54, A_FILE, first + k);
        R(dc, x, y, 380, 54, 10, d ? kOrange : kPanel);
        Ico(dc, f.dir ? IC_FOLDER : IC_FILE, x + 14, y + 12, 30, d ? kInk : kMuted);
        T(dc, x + 58, y + 34, f.name, 18, d ? kInk : kText, 700, AL_START, 310);
    }
    if (s_fileCount == 0)
        T(dc, 400, 240, s_filesFor == F_ICON ? L"Здесь нет папок и иконок" : s_filesFor == F_CAMS ?
          L"Здесь нет папок и .txt" : L"Здесь нет папок и программ", 22, kText, 700, AL_MID);
    Dock(dc);
    Button(dc, 16, 410, 120, 58, NULL, B_N, IC_UP, 18, 0, A_UP);
    Button(dc, 148, 410, 120, 58, NULL, B_N, IC_BACK, 18, 0, A_FPREV);
    wchar_t s[16];
    _snwprintf(s, 16, L"%d / %d", s_filePage + 1, FilePages());
    T(dc, 400, 448, s, 22, kText, 700, AL_MID);
    Button(dc, 532, 410, 120, 58, NULL, B_N, IC_NEXT, 18, 0, A_FNEXT);
    Button(dc, 664, 410, 120, 58, L"ОТМЕНА", B_N, IC_NONE, 18, 0, A_CANCEL);
}

// ---------------------------------------------------------------- the main screen: widgets
// Two pages of five widgets (3 x 2 grid of 248 x 158 cells, the first of each page two cells wide), swiped or
// turned with the dots. A widget opens its screen; small buttons inside it act on their own; a caption with a gear
// opens the widget's settings. Real: the date, the programs, the weather, the speed (GPS) and the cameras; the
// rest are demo values until the car's bus through the MCU is known.

static int s_homePage;
enum { kHomePages = 2 };

// Widget panel with its caption; pressed = orange like a tile (returns it: the content follows in ink).
// With a settings screen the caption strip is a button of its own (a gear at its end) leading there.
static bool Widget(HDC dc, int x, int y, int w, int h, const wchar_t* caption, Icon ic, Action a, int arg = 0,
                   Screen settings = S_NONE)
{
    bool d = a != A_NONE && Zone(x, y, w, h, a, arg);
    bool hd = settings != S_NONE && Zone(x, y, w, 42, A_SCREEN, settings);
    R(dc, x, y, w, h, 14, d ? kOrange : kPanel, d ? CLR_INVALID : kLine);
    if (hd)
        R(dc, x + 2, y + 2, w - 4, 40, 12, kOrange);
    Ico(dc, ic, x + 16, y + 12, 22, d || hd ? kInk : kOrange);
    T(dc, x + 46, y + 29, caption, 13, d || hd ? kInk : kMuted, 700, AL_START, w - (settings != S_NONE ? 90 : 62));
    if (settings != S_NONE)
        Ico(dc, IC_GEAR, x + w - 34, y + 12, 20, d || hd ? kInk : kDim);
    return d;
}

static void WMedia(HDC dc, int x, int y)
{
    bool d = Widget(dc, x, y, 504, 158, L"СЕЙЧАС ИГРАЕТ · USB", IC_MUSIC, A_NAV, S_AUDIO);
    COLORREF fg = d ? kInk : kText, mg = d ? kInk : kMuted;
    TriFill(x + 16, y + 42, 100, 100, 12);
    Ico(dc, IC_MUSIC, x + 40, y + 66, 52, Mix(kInk, kTriOr[1], 140));
    int tx = x + 136;
    T(dc, tx, y + 70, L"Группа крови", 26, fg, 700, AL_START, 352);
    T(dc, tx, y + 94, L"Кино · трек 3 из 11", 16, mg, 400, AL_START, 260);
    T(dc, x + 488, y + 94, L"1:42 / 4:46", 15, mg, 600, AL_END);
    R(dc, tx, y + 106, 352, 6, 3, d ? kInk : kLine);
    R(dc, tx, y + 106, 126, 6, 3, d ? kInk : kOrange);
    IconBtn(dc, IC_PREV, tx + 60, y + 122, 28, fg);
    PauseBtn(dc, tx + 176, y + 136, 20, 22);
    IconBtn(dc, IC_FWD, tx + 264, y + 122, 28, fg);
}

static const wchar_t* kHeadings[8] = { L"С", L"СВ", L"В", L"ЮВ", L"Ю", L"ЮЗ", L"З", L"СЗ" };

// Speed from GPS; a camera ahead shows as its speed sign ("КАМ" without a limit) with the distance to it.
static void WSpeed(HDC dc, int x, int y)
{
    Gps g;
    GpsGet(&g);
    bool fix = GpsFresh(g, 5000);
    Widget(dc, x, y, 248, 158, L"СКОРОСТЬ · GPS", IC_NAV, A_NONE, 0, S_WSET_SPEED);
    wchar_t v[64];
    if (fix)
        _snwprintf(v, 8, L"%d", (int)(g.speedKmh + 0.5));
    else
        wcscpy(v, L"—");
    int end = T(dc, x + 16, y + 112, v, 72, fix ? kText : kDim, 700);
    if (s_camNear >= 0) {
        const Cam& cm = s_cams[s_camNear];
        Disc(dc, x + 204, y + 80, 28, kStop);
        Disc(dc, x + 204, y + 80, 21, kText);
        if (cm.speed) {
            _snwprintf(v, 8, L"%d", cm.speed);
            T(dc, x + 204, y + 88, v, 20, kInk, 700, AL_MID);
        } else {
            T(dc, x + 204, y + 85, L"КАМ", 12, kInk, 700, AL_MID);
        }
        _snwprintf(v, 16, L"%d м", s_camNearDist);
        T(dc, x + 204, y + 128, v, 14, kOrange, 700, AL_MID);
    } else {
        T(dc, end + 8, y + 112, L"км/ч", 18, kMuted, 600);
    }
    if (fix)
        _snwprintf(v, 64, L"%s · %d м · спутников %d", kHeadings[((int)(g.course + 22.5) / 45) % 8], (int)g.alt, g.sats);
    else
        GpsStatusText(v, 64);
    T(dc, x + 16, y + 144, v, 14, fix ? kMuted : kDim, 400, AL_START, 216);
}

static void WRadio(HDC dc, int x, int y)
{
    int cur = s_seg[G_PRESET] < 7 ? s_seg[G_PRESET] : 2;
    bool d = Widget(dc, x, y, 248, 158, L"РАДИО · FM", IC_RADIO, A_NAV, S_RADIO);
    T(dc, x + 16, y + 88, kPresets[cur].freq, 44, d ? kInk : kText, 700);
    T(dc, x + 16, y + 114, kPresets[cur].name, 17, d ? kInk : kMuted, 600, AL_START, 216);
    IconBtn(dc, IC_PREV, x + 24, y + 124, 26, d ? kInk : kText, A_SEG, G_PRESET * 16 + (cur + 6) % 7);
    IconBtn(dc, IC_FWD, x + 82, y + 124, 26, d ? kInk : kText, A_SEG, G_PRESET * 16 + (cur + 1) % 7);
}

// The weather's sign in a 64 x 64 box: sun, sun and cloud, cloud, and the cloud with rain, snow or lightning; fog.
static void WxIcon(HDC dc, int kind, int x, int y, COLORREF fg)
{
    if (kind == WX_SUN) {
        Ico(dc, IC_SUN, x + 4, y + 4, 56, kOrange);
        return;
    }
    if (kind == WX_FOG) {
        for (int i = 0; i < 3; i++)
            R(dc, x + 6 + i * 4, y + 20 + i * 12, 52 - i * 8, 4, 2, fg);
        return;
    }
    if (kind == WX_PART)
        Ico(dc, IC_SUN, x - 2, y - 2, 42, kOrange);
    Ico(dc, IC_CLOUD, x + 12, y + 6, 50, fg);
    for (int i = 0; i < 3; i++) {
        if (kind == WX_RAIN || kind == WX_STORM)
            Line(dc, x + 26 + i * 11, y + 46, x + 22 + i * 11, y + 58, 3, kind == WX_STORM && i == 1 ? kOrange : kMuted);
        else if (kind == WX_SNOW)
            Disc(dc, x + 26 + i * 11, y + 52, 3, fg);
    }
}

// Weather: a tap on it refreshes, the caption leads to its settings (provider, place).
static void WWeather(HDC dc, int x, int y)
{
    Weather w;
    WxGet(&w);
    wchar_t cap[64], s[100];
    if (w.have && w.place[0])
        _snwprintf(cap, 64, L"ПОГОДА · %s", w.place);
    else
        wcscpy(cap, L"ПОГОДА");
    CharUpperBuff(cap, (DWORD)wcslen(cap));
    bool d = Widget(dc, x, y, 248, 158, cap, IC_CLOUD, A_WXNOW, 0, S_WSET_WEATHER);
    COLORREF fg = d ? kInk : kText, mg = d ? kInk : kMuted;
    if (!w.have) {
        T(dc, x + 16, y + 90, s_wxBusy ? L"Загрузка…" : L"Нет данных", 22, fg, 700, AL_START, 216);
        T(dc, x + 16, y + 118, w.ok < 0 ? w.err : L"нажмите, чтобы обновить", 14, mg, 400, AL_START, 216);
        return;
    }
    WxIcon(dc, w.kind, x + 16, y + 44, fg);
    _snwprintf(s, 16, L"%+d°", w.temp);
    T(dc, x + 96, y + 96, s, 44, fg, 700);
    _snwprintf(s, 100, L"%s, ветер %d м/с", w.desc, w.wind);
    CharUpperBuff(s, 1);
    T(dc, x + 16, y + 126, s, 15, mg, 400, AL_START, 216);
    if (w.ok < 0)
        _snwprintf(s, 100, L"не обновилась: %s", w.err);
    else
        _snwprintf(s, 100, L"днём %+d°, ночью %+d° · %02d:%02d", w.tmax, w.tmin, w.at.wHour, w.at.wMinute);
    T(dc, x + 16, y + 146, s, 14, d ? kInk : kDim, 400, AL_START, 216);
}

static void WCar(HDC dc, int x, int y)
{
    static const wchar_t* rows[3][2] = { { L"Двигатель", L"90 °C" }, { L"Аккумулятор", L"14,2 В" },
                                         { L"ТО через", L"3 200 км" } };
    bool d = Widget(dc, x, y, 248, 158, L"АВТОМОБИЛЬ", IC_CAR, A_SCREEN, S_CAR, S_WSET_CAR);
    Disc(dc, x + 24, y + 58, 7, d ? kInk : kOk);
    T(dc, x + 40, y + 65, L"Ошибок нет", 19, d ? kInk : kText, 700);
    for (int i = 0; i < 3; i++) {
        T(dc, x + 16, y + 94 + i * 24, rows[i][0], 15, d ? kInk : kMuted, 400);
        T(dc, x + 232, y + 94 + i * 24, rows[i][1], 15, d ? kInk : kText, 700, AL_END);
    }
}

static void WTrip(HDC dc, int x, int y)
{
    static const wchar_t* cells[4][3] = { { L"РАСХОД", L"7,4", L"л/100 км" }, { L"ЗАПАС ХОДА", L"420", L"км" },
                                          { L"ПРОБЕГ", L"34,5", L"км" }, { L"СРЕДНЯЯ", L"49", L"км/ч" } };
    bool d = Widget(dc, x, y, 504, 158, L"БОРТОВОЙ КОМПЬЮТЕР · ПОЕЗДКА", IC_CLOCK, A_SCREEN, S_CAR);
    for (int i = 0; i < 4; i++) {
        int cx = x + 16 + i * 120;
        if (i)
            Fill(dc, cx - 8, y + 50, cx - 6, y + 126, d ? kInk : kLine);
        T(dc, cx, y + 62, cells[i][0], 12, d ? kInk : kMuted, 700, AL_START, 108);
        T(dc, cx, y + 102, cells[i][1], 32, d ? kInk : kText, 700);
        T(dc, cx, y + 122, cells[i][2], 14, d ? kInk : kMuted, 400);
    }
    T(dc, x + 16, y + 146, L"В пути 0:42 · с 08:10", 14, d ? kInk : kDim, 400);
}

static void WDate(HDC dc, int x, int y)
{
    SYSTEMTIME t;
    GetLocalTime(&t);
    wchar_t day[4];
    _snwprintf(day, 4, L"%d", t.wDay);
    bool d = Widget(dc, x, y, 248, 158, L"КАЛЕНДАРЬ", IC_CLOCK, A_NAV, S_SETTINGS);
    int end = T(dc, x + 16, y + 112, day, 64, d ? kInk : kText, 700);
    T(dc, end + 12, y + 84, kMonths[(t.wMonth + 11) % 12], 19, d ? kInk : kText, 700, AL_START, x + 232 - end - 12);
    T(dc, end + 12, y + 108, kDays[t.wDayOfWeek % 7], 15, d ? kInk : kMuted, 400, AL_START, x + 232 - end - 12);
    Weather w;   // sunrise and sunset come with the weather
    WxGet(&w);
    wchar_t s[48];
    if (w.have && w.rise[0])
        _snwprintf(s, 48, L"Восход %s · закат %s", w.rise, w.set);
    else
        wcscpy(s, L"Восход и закат — с погодой");
    T(dc, x + 16, y + 144, s, 14, d ? kInk : kDim, 400, AL_START, 216);
}

// Climate control, only when the car has it (the car widget's settings); demo until the MCU protocol is known.
static void WClimate(HDC dc, int x, int y)
{
    Widget(dc, x, y, 248, 158, L"КЛИМАТ · ДЕМО", IC_CAR, A_NONE, 0, S_WSET_CAR);
    T(dc, x + 16, y + 100, L"22,0°", 46, kText, 700);
    T(dc, x + 156, y + 72, L"АВТО", 15, kOrange, 700);
    for (int i = 0; i < 5; i++)   // fan speed 3 of 5
        R(dc, x + 156 + i * 15, y + 100 - (8 + i * 4), 10, 8 + i * 4, 2, i < 3 ? kText : kLine);
    R(dc, x + 16, y + 116, 64, 30, 8, kOrange);
    T(dc, x + 48, y + 137, L"A/C", 15, kInk, 700, AL_MID);
    R(dc, x + 88, y + 116, 104, 30, 8, CLR_INVALID, kLine);
    T(dc, x + 140, y + 137, L"РЕЦИРК.", 13, kMuted, 700, AL_MID);
}

static void WPhone(HDC dc, int x, int y)
{
    bool d = Widget(dc, x, y, 248, 158, L"ТЕЛЕФОН · REDMI NOTE", IC_PHONE, A_NAV, S_PHONE);
    T(dc, x + 16, y + 62, L"ПОСЛЕДНИЙ ВЫЗОВ", 12, d ? kInk : kDim, 700);
    T(dc, x + 16, y + 94, L"Мама", 24, d ? kInk : kText, 700, AL_START, 140);
    T(dc, x + 16, y + 118, L"сегодня, 12:05", 15, d ? kInk : kMuted, 400, AL_START, 150);
    bool c = Zone(x + 168, y + 98, 64, 48, A_SCREEN, S_CALL);
    R(dc, x + 168, y + 98, 64, 48, 10, c ? Mix(kOk, kInk, 160) : kOk);
    Ico(dc, IC_CALL, x + 184, y + 106, 32, kInk);
}

// The first three programs of the list; the widget itself leads to all of them.
static void WFavorites(HDC dc, int x, int y)
{
    bool d = Widget(dc, x, y, 248, 158, L"ПРОГРАММЫ", IC_APPS, A_NAV, S_APPS);
    if (s_appCount == 0) {
        T(dc, x + 16, y + 92, L"Добавьте программы", 16, d ? kInk : kMuted, 400, AL_START, 216);
        T(dc, x + 16, y + 114, L"в «Приложениях»", 16, d ? kInk : kMuted, 400, AL_START, 216);
        return;
    }
    for (int i = 0; i < 3 && i < s_appCount; i++) {
        int cx = x + 8 + i * 78;
        bool p = Zone(cx, y + 44, 76, 106, A_RUN, i);
        if (p)
            R(dc, cx, y + 44, 76, 106, 10, kOrange);
        AppIcon(dc, s_apps[i], cx + 16, y + 54, 44, p || d);
        T(dc, cx + 38, y + 132, s_apps[i].name, 12, p || d ? kInk : kMuted, 700, AL_MID, 72);
    }
}

// Navigation: starts the first program of the list (the example list begins with the maps).
static void WNavi(HDC dc, int x, int y)
{
    const wchar_t* name = s_appCount ? s_apps[0].name : L"Карты";
    bool d = Widget(dc, x, y, 248, 158, L"НАВИГАЦИЯ", IC_NAV, s_appCount ? A_RUN : A_NAV, s_appCount ? 0 : S_APPS);
    Ico(dc, IC_MAP, x + 16, y + 52, 48, d ? kInk : kText);
    T(dc, x + 78, y + 74, name, 19, d ? kInk : kText, 700, AL_START, 154);
    T(dc, x + 78, y + 96, L"маршрут не задан", 15, d ? kInk : kMuted, 400, AL_START, 154);
    T(dc, x + 16, y + 144, L"нажмите, чтобы открыть карты", 14, d ? kInk : kDim, 400, AL_START, 216);
}

// The main screen: the date as the title, the widgets, the page dots.
static void DrawHome(HDC dc)
{
    SYSTEMTIME t;
    GetLocalTime(&t);
    wchar_t s[48];
    _snwprintf(s, 48, L"%s, %d %s", kDays[t.wDayOfWeek % 7], t.wDay, kMonths[(t.wMonth + 11) % 12]);
    CharUpperBuff(s, 1);
    Frame(dc, s);
    if (s_homePage == 0) {
        WMedia(dc, 16, 64);
        WSpeed(dc, 536, 64);
        WRadio(dc, 16, 232);
        WWeather(dc, 276, 232);
        WCar(dc, 536, 232);
    } else {
        WTrip(dc, 16, 64);
        if (s_cfg.climate)
            WClimate(dc, 536, 64);
        else
            WDate(dc, 536, 64);
        WPhone(dc, 16, 232);
        WFavorites(dc, 276, 232);
        WNavi(dc, 536, 232);
    }
    PageDots(dc, s_homePage, kHomePages);
}

// The car's systems and the trip, opened from the widgets.
static void ScrCar(HDC dc)
{
    static const wchar_t* sys[6][2] = { { L"Двигатель", L"в норме" }, { L"Тормозная система", L"в норме" },
                                        { L"ABS / ESP", L"в норме" }, { L"Подушки безопасности", L"в норме" },
                                        { L"Аккумулятор", L"14,2 В" }, { L"Охлаждающая жидкость", L"90 °C" } };
    static const wchar_t* trip[5][2] = { { L"Средний расход", L"7,4 л/100 км" }, { L"Запас хода", L"420 км" },
                                         { L"Пробег поездки", L"34,5 км" }, { L"Время в пути", L"0:42" },
                                         { L"Средняя скорость", L"49 км/ч" } };
    Frame(dc, L"АВТОМОБИЛЬ");
    Panel(dc, 16, 68, 376, 396);
    Disc(dc, 44, 102, 10, kOk);
    T(dc, 64, 110, L"ОШИБОК НЕТ", 22, kText, 700);
    T(dc, 32, 138, L"проверено сегодня в 12:04", 15, kMuted, 400);
    for (int i = 0; i < 6; i++) {
        int y = 152 + i * 50;
        T(dc, 32, y + 32, sys[i][0], 17, kText, 600, AL_START, 220);
        T(dc, 376, y + 32, sys[i][1], 17, i < 4 ? kOk : kText, 700, AL_END);
        if (i < 5)
            Sep(dc, 32, y + 49, 344);
    }
    Panel(dc, 408, 68, 376, 300);
    T(dc, 424, 100, L"ПОЕЗДКА", 14, kMuted, 700);
    for (int i = 0; i < 5; i++) {
        int y = 112 + i * 50;
        T(dc, 424, y + 32, trip[i][0], 17, kMuted, 400);
        T(dc, 768, y + 32, trip[i][1], 17, kText, 700, AL_END);
        if (i < 4)
            Sep(dc, 424, y + 49, 344);
    }
    Panel(dc, 408, 380, 376, 84);
    Ico(dc, IC_GEAR, 424, 402, 40, kOrange);
    T(dc, 480, 416, L"ТО через 3 200 км", 19, kText, 700);
    T(dc, 480, 442, L"или 41 день · масло и фильтры", 15, kMuted, 400, AL_START, 290);
}

// ---- widgets' settings (opened from a widget's caption): sub screens, rows of 56 px over the whole width

static wchar_t s_coordText[48];   // the home point's coordinates while they are typed

// Row: label and a second line; the control is drawn by the caller to the right of `right`. With an action the
// left part is a button of its own.
static void WRow(HDC dc, int y, const wchar_t* label, const wchar_t* sub, Action a = A_NONE, int arg = 0,
                 int right = 520, bool last = false)
{
    bool down = a != A_NONE && Zone(24, y + 2, right - 24, 52, a, arg);
    if (down)
        R(dc, 24, y + 2, right - 24, 52, 10, kOrange);
    T(dc, 36, y + (sub ? 26 : 35), label, 19, down ? kInk : kText, 700, AL_START, right - 52);
    if (sub)
        T(dc, 36, y + 47, sub, 15, down ? kInk : kMuted, 400, AL_START, right - 52);
    if (a != A_NONE)
        Ico(dc, IC_NEXT, right - 36, y + 14, 28, down ? kInk : kDim);
    if (!last)
        Sep(dc, 24, y + 55, 752);
}

static void ScrWSetWeather(HDC dc)
{
    static const wchar_t* prov[3] = { L"WTTR.IN", L"OPENWEATHER", L"GISMETEO" };
    Frame(dc, L"ПОГОДА · НАСТРОЙКИ");
    Panel(dc, 16, 64, 768, 400);
    wchar_t s[120];
    int p = s_cfg.weatherProv;
    WRow(dc, 68, L"Источник", p == W_WTTR ? L"открытый, без ключа" : p == W_OWM ? L"бесплатный ключ на openweathermap.org"
         : L"токен — по запросу на gismeteo.ru/b2b", A_NONE, 0, 300);
    Seg(dc, 300, 74, 468, 44, prov, 3, G_WPROV, 14);
    const wchar_t* key = s_cfg.weatherKey[p];
    WRow(dc, 124, p == W_GISMETEO ? L"Токен" : L"Ключ API", p == W_WTTR ? L"не нужен" : key[0] ? key : L"не задан — нажмите, чтобы ввести",
         p == W_WTTR ? A_NONE : A_KEYS, K_WKEY, 776);
    WRow(dc, 180, L"Место по GPS", L"основное; без фикса — домашняя точка", A_NONE, 0, 640);
    Toggle(dc, 688, 190, T_W_GPS);
    WRow(dc, 236, L"Домашняя точка", s_cfg.homeName[0] ? s_cfg.homeName : L"название — нажмите, чтобы ввести", A_KEYS,
         K_HOMENAME, 776);
    if (s_cfg.homeLat || s_cfg.homeLon)
        _snwprintf(s, 120, L"%.5f, %.5f", s_cfg.homeLat, s_cfg.homeLon);
    else
        wcscpy(s, L"не заданы — ввести или взять из GPS");
    WRow(dc, 292, L"Координаты дома", s, A_KEYS, K_HOMECOORD, 568);
    Button(dc, 576, 298, 192, 44, L"ИЗ GPS", B_N, IC_NAV, 15, 22, A_HOMEGPS);
    Weather w;
    WxGet(&w);
    if (s_wxBusy)
        wcscpy(s, L"загрузка…");
    else if (w.ok < 0)
        _snwprintf(s, 120, L"ошибка: %s", w.err);
    else if (w.have)
        _snwprintf(s, 120, L"обновлено %02d:%02d · %s · %+d°", w.at.wHour, w.at.wMinute, w.place, w.temp);
    else
        wcscpy(s, L"ещё не загружалась");
    WRow(dc, 348, L"Состояние", s, A_NONE, 0, 568, true);
    Button(dc, 576, 354, 192, 44, L"ОБНОВИТЬ", B_PRI, IC_REFRESH, 15, 22, A_WXNOW);
    T(dc, 36, 440, L"Обновляется раз в 30 мин. Open-Meteo из сети пользователя не отвечал.",
      13, kDim, 400, AL_START, 728);
}

static void RegionsText(wchar_t* s, int n);

static void ScrWSetSpeed(HDC dc)
{
    static const wchar_t* dist[4] = { L"ПО СКОРОСТИ", L"200 М", L"400 М", L"600 М" };
    Frame(dc, L"СКОРОСТЬ И КАМЕРЫ");
    Panel(dc, 16, 64, 768, 400);
    wchar_t s[160], label[48];
    WRow(dc, 68, L"Предупреждать о камерах", L"знак на виджете и в верхнем баре", A_NONE, 0, 640);
    Toggle(dc, 688, 78, T_CAM_ALERT);
    WRow(dc, 124, L"Звуковой сигнал", L"два коротких, когда камера появляется впереди", A_NONE, 0, 640);
    Toggle(dc, 688, 134, T_CAM_SOUND);
    WRow(dc, 180, L"Дистанция", L"по скорости: за 10 с, 100–500 м", A_NONE, 0, 340);
    Seg(dc, 352, 186, 416, 44, dist, 4, G_CAMDIST, 14);
    wchar_t path[MAX_PATH];
    CamPath(path);
    if (GetFileAttributes(path) == 0xFFFFFFFF)
        _snwprintf(s, 160, L"%s — нет файла", s_cfg.camFile);
    else if (!s_camLoaded)
        _snwprintf(s, 160, L"%s · загрузится по GPS", s_cfg.camFile);
    else
        _snwprintf(s, 160, L"%s · рядом %d из %d", s_cfg.camFile, s_camCount, s_camTotal);
    WRow(dc, 236, L"Файл камер (iGO / Navitel)", s, A_CAMFILE, 0, 776);
    wchar_t port[16], baud[16];
    GpsPortText(port, 16);
    GpsBaudText(baud, 16);
    _snwprintf(label, 48, L"Порт GPS · %s · %s", port, baud);
    GpsStatusText(s, 160);
    WRow(dc, 292, label, s, A_SCREEN, S_SETTINGS, 776);   // set on "General" for the whole shell
    WRow(dc, 348, L"Время по GPS", L"часы по спутникам, если расходятся больше чем на 30 с", A_NONE, 0, 640);
    Toggle(dc, 688, 358, T_GPS_TIME);
    wchar_t regs[160];
    RegionsText(regs, 160);
    if (s_cfg.camUpdated)
        _snwprintf(s, 160, L"%s · обновлено %02d.%02d", regs, s_cfg.camUpdated % 100, s_cfg.camUpdated / 100 % 100);
    else
        wcscpy(s, regs);
    WRow(dc, 404, L"Скачать базу камер (speedcamonline.ru)", s, A_SCREEN, S_CAMDL, 776, true);
}

static void ScrWSetCar(HDC dc)
{
    Frame(dc, L"АВТОМОБИЛЬ · НАСТРОЙКИ");
    Panel(dc, 16, 64, 768, 400);
    WRow(dc, 68, L"Климат-контроль в машине", L"виджет «Климат» на главной вместо календаря", A_NONE, 0, 640, true);
    Toggle(dc, 688, 78, T_CLIMATE);
    T(dc, 36, 160, L"Ошибки, температуры, климат и датчик за бортом придут из MCU, когда будет", 15, kMuted, 400,
      AL_START, 728);
    T(dc, 36, 184, L"разобран его протокол. Сейчас значения на виджетах — демо.", 15, kMuted, 400, AL_START, 728);
}

// The keyboard opened from these screens: which setting it edits.
static void OpenCfgKeys(int which)
{
    switch (which) {
    case K_WKEY:
        OpenKeysFor(s_cfg.weatherKey[s_cfg.weatherProv], 63, L"КЛЮЧ API", S_WSET_WEATHER);
        break;
    case K_HOMENAME:
        OpenKeysFor(s_cfg.homeName, 47, L"ДОМАШНЯЯ ТОЧКА", S_WSET_WEATHER);
        break;
    case K_HOMECOORD:
        if (s_cfg.homeLat || s_cfg.homeLon)
            _snwprintf(s_coordText, 48, L"%.5f, %.5f", s_cfg.homeLat, s_cfg.homeLon);
        else
            s_coordText[0] = 0;
        OpenKeysFor(s_coordText, 47, L"ШИРОТА, ДОЛГОТА", S_WSET_WEATHER);
        break;
    }
}

// Typed into a setting: coordinates are parsed, the file written, what depends on it restarted.
static void CfgKeysDone()
{
    if (s_keysTarget == s_coordText) {
        wchar_t* e;
        double lat = wcstod(s_coordText, &e);
        while (*e == L',' || *e == L';' || *e == L' ')
            e++;
        double lon = wcstod(e, NULL);
        if (lat >= -90 && lat <= 90 && lon >= -180 && lon <= 180) {
            s_cfg.homeLat = lat;
            s_cfg.homeLon = lon;
        }
    }
    SaveCfg();
    WeatherRefresh();
}

// The settings shown as switches and choices: s_cfg <-> s_on / s_seg.
static void CfgToControls()
{
    s_on[T_GPS_TIME] = s_cfg.gpsTime;
    s_on[T_W_GPS] = s_cfg.weatherGps;
    s_on[T_CLIMATE] = s_cfg.climate;
    s_on[T_CAM_ALERT] = s_cfg.camAlert;
    s_on[T_CAM_SOUND] = s_cfg.camSound;
    s_on[T_CAM_WEEKLY] = s_cfg.camWeekly;
    s_seg[G_WPROV] = s_cfg.weatherProv;
    s_seg[G_CAMDIST] = 0;
    for (int i = 0; i < 4; i++)
        if (kCamDists[i] == s_cfg.camDist)
            s_seg[G_CAMDIST] = i;
}

static bool IsCfgToggle(int id)
{
    return id == T_GPS_TIME || id == T_W_GPS || id == T_CLIMATE || id == T_CAM_ALERT || id == T_CAM_SOUND ||
           id == T_CAM_WEEKLY;
}

static void ControlsToCfg()
{
    bool weather = s_cfg.weatherGps != s_on[T_W_GPS] || s_cfg.weatherProv != s_seg[G_WPROV];
    s_cfg.gpsTime = s_on[T_GPS_TIME];
    s_cfg.weatherGps = s_on[T_W_GPS];
    s_cfg.climate = s_on[T_CLIMATE];
    s_cfg.camAlert = s_on[T_CAM_ALERT];
    s_cfg.camSound = s_on[T_CAM_SOUND];
    s_cfg.camWeekly = s_on[T_CAM_WEEKLY];
    s_cfg.weatherProv = s_seg[G_WPROV];
    s_cfg.camDist = kCamDists[s_seg[G_CAMDIST] & 3];
    if (!s_cfg.camAlert)
        s_camNear = -1;
    SaveCfg();
    if (weather)
        WeatherRefresh();
}

// "From GPS": the current fix becomes the home point.
static void HomeFromGps()
{
    Gps g;
    GpsGet(&g);
    if (!GpsFresh(g, 10000)) {
        wcscpy(s_status, L"нет фикса GPS");
        return;
    }
    s_cfg.homeLat = g.lat;
    s_cfg.homeLon = g.lon;
    SaveCfg();
    WeatherRefresh();
}

// ---- speed camera base from speedcamonline.ru (HTTPS only): the site's own export link for iGO,
// /igo/Rus&filter_id=&cam_name=&cam_adress=&cam_N=&cam_E=&region=RR&kind_list=1000,k1,k2,... (kind_list must start
// with 1000; the kinds are the site's filter checkboxes e<k>). The site's "verified only" switch doesn't change
// the export (checked 2026-10-12). Regions are the car number codes, Moscow is in 50 with its oblast, 00 is abroad;
// -1 = the whole of Russia (some 50 000 cameras, 2 MB). Each region is one request; they are joined into
// speedcam.txt through speedcam.tmp. A finished download is told to the window (WM_APP_CAM), which takes the file.
struct CamKind { int id; const wchar_t* name; int group; };   // group: 0 speed, 1 other fixed, 2 mobile/useless
static const CamKind kCamKinds[18] = {
    { 0, L"СТАЦ. РАДАР", 0 }, { 14, L"МАЛОМОЩНЫЙ", 0 }, { 11, L"СР. СКОР.: НАЧАЛО", 0 }, { 110, L"СР. СКОР.: КОНЕЦ", 0 },
    { 2, L"ПОТОК", 0 }, { 3, L"СТРЕЛКА", 0 }, { 12, L"ВИДЕОБЛОК", 1 }, { 5, L"СВЕТОФОРЫ", 1 },
    { 16, L"КОНТР. ОСТАНОВКИ", 1 }, { 1, L"КАМЕРА ПОСТА ДПС", 1 }, { 10, L"ПОСТ ДПС", 1 }, { 6, L"Ж/Д ПЕРЕЕЗД", 1 },
    { 9, L"МОБИЛЬНЫЙ ПОСТ", 2 }, { 7, L"МОБ. ЗАСАДА", 2 }, { 8, L"ТРЕНОГА", 2 }, { 4, L"НАБЛЮДЕНИЕ", 2 },
    { 300, L"МУЛЯЖ", 2 }, { 999, L"НЕ ОПРЕДЕЛЁН", 2 } };

struct CamRegion { const char* code; const wchar_t* name; };
static const CamRegion kCamRegions[] = {
    { "-1", L"Вся Россия" }, { "50", L"Москва и обл." }, { "78", L"Санкт-Петербург" }, { "47", L"Ленинградская" },
    { "01", L"Адыгея" }, { "02", L"Башкортостан" }, { "03", L"Бурятия" }, { "04", L"Респ. Алтай" },
    { "05", L"Дагестан" }, { "06", L"Ингушетия" }, { "07", L"Кабардино-Балк." }, { "08", L"Калмыкия" },
    { "09", L"Карачаево-Черк." }, { "10", L"Карелия" }, { "11", L"Коми" }, { "12", L"Марий Эл" },
    { "13", L"Мордовия" }, { "14", L"Якутия" }, { "15", L"Сев. Осетия" }, { "16", L"Татарстан" },
    { "17", L"Тыва" }, { "18", L"Удмуртия" }, { "19", L"Хакасия" }, { "21", L"Чувашия" },
    { "22", L"Алтайский кр." }, { "23", L"Краснодарский кр." }, { "24", L"Красноярский кр." }, { "25", L"Приморский кр." },
    { "26", L"Ставропольский кр." }, { "27", L"Хабаровский кр." }, { "28", L"Амурская" }, { "29", L"Архангельская" },
    { "30", L"Астраханская" }, { "31", L"Белгородская" }, { "32", L"Брянская" }, { "33", L"Владимирская" },
    { "34", L"Волгоградская" }, { "35", L"Вологодская" }, { "36", L"Воронежская" }, { "37", L"Ивановская" },
    { "38", L"Иркутская" }, { "39", L"Калининградская" }, { "40", L"Калужская" }, { "41", L"Камчатский кр." },
    { "42", L"Кемеровская" }, { "43", L"Кировская" }, { "44", L"Костромская" }, { "45", L"Курганская" },
    { "46", L"Курская" }, { "48", L"Липецкая" }, { "49", L"Магаданская" }, { "51", L"Мурманская" },
    { "52", L"Нижегородская" }, { "53", L"Новгородская" }, { "54", L"Новосибирская" }, { "55", L"Омская" },
    { "56", L"Оренбургская" }, { "57", L"Орловская" }, { "58", L"Пензенская" }, { "59", L"Пермский кр." },
    { "60", L"Псковская" }, { "61", L"Ростовская" }, { "62", L"Рязанская" }, { "63", L"Самарская" },
    { "64", L"Саратовская" }, { "65", L"Сахалинская" }, { "66", L"Свердловская" }, { "67", L"Смоленская" },
    { "68", L"Тамбовская" }, { "69", L"Тверская" }, { "70", L"Томская" }, { "71", L"Тульская" },
    { "72", L"Тюменская" }, { "73", L"Ульяновская" }, { "74", L"Челябинская" }, { "75", L"Забайкальский кр." },
    { "76", L"Ярославская" }, { "79", L"Еврейская АО" }, { "82", L"Крым" }, { "83", L"Ненецкий АО" },
    { "86", L"ХМАО — Югра" }, { "89", L"ЯНАО" }, { "92", L"Севастополь" }, { "95", L"Чечня" } };
enum { kCamRegionCount = sizeof(kCamRegions) / sizeof(kCamRegions[0]), kRegionsPerPage = 24 };

#define WM_APP_CAM (WM_APP + 12)
static volatile LONG s_dlBusy;
static wchar_t       s_dlStatus[100];   // the download's progress or result
static DWORD         s_dlTry;           // tick of the last automatic attempt
static int           s_regPage;

// Comma lists of the settings ("63,73"): membership and toggling of one item.
static bool InList(const wchar_t* list, const wchar_t* item)
{
    int n = (int)wcslen(item);
    for (const wchar_t* p = list; *p;) {
        const wchar_t* e = wcschr(p, L',');
        int len = e ? (int)(e - p) : (int)wcslen(p);
        if (len == n && !wcsncmp(p, item, n))
            return true;
        if (!e)
            break;
        p = e + 1;
    }
    return false;
}

static void ToggleInList(wchar_t* list, int cap, const wchar_t* item)
{
    wchar_t out[256] = L"";
    bool had = false;
    int n = (int)wcslen(item);
    for (const wchar_t* p = list; *p;) {
        const wchar_t* e = wcschr(p, L',');
        int len = e ? (int)(e - p) : (int)wcslen(p);
        if (len == n && !wcsncmp(p, item, n)) {
            had = true;
        } else if (len) {
            if (out[0])
                wcscat(out, L",");
            wcsncat(out, p, len);
        }
        if (!e)
            break;
        p = e + 1;
    }
    if (!had && (int)(wcslen(out) + n + 2) < cap) {
        if (out[0])
            wcscat(out, L",");
        wcscat(out, item);
    }
    wcsncpy(list, out, cap - 1);
    list[cap - 1] = 0;
}

static bool KindOn(int k)
{
    wchar_t s[8];
    _snwprintf(s, 8, L"%d", kCamKinds[k].id);
    return InList(s_cfg.camKinds, s);
}

static void ToggleKind(int k)
{
    wchar_t s[8];
    _snwprintf(s, 8, L"%d", kCamKinds[k].id);
    ToggleInList(s_cfg.camKinds, 100, s);
    SaveCfg();
}

// Presets: 0 speed only, 1 every fixed camera (no mobile posts, tripods, dummies), 2 all.
static void KindsPreset(int preset)
{
    s_cfg.camKinds[0] = 0;
    for (int k = 0; k < 18; k++)
        if (kCamKinds[k].group <= preset) {
            wchar_t s[8];
            _snwprintf(s, 8, L"%d", kCamKinds[k].id);
            ToggleInList(s_cfg.camKinds, 100, s);
        }
    SaveCfg();
}

static bool RegionOn(int i)
{
    wchar_t s[8];
    _snwprintf(s, 8, L"%S", kCamRegions[i].code);
    return InList(s_cfg.camRegions, s);
}

// "The whole of Russia" excludes the regions and the other way round.
static void ToggleRegion(int i)
{
    if (i == 0)
        s_cfg.camRegions[0] = 0;
    else if (InList(s_cfg.camRegions, L"-1"))
        s_cfg.camRegions[0] = 0;
    wchar_t s[8];
    _snwprintf(s, 8, L"%S", kCamRegions[i].code);
    ToggleInList(s_cfg.camRegions, 200, s);
    SaveCfg();
}

static void RegionsText(wchar_t* s, int n)
{
    s[0] = 0;
    for (int i = 0; i < kCamRegionCount; i++)
        if (RegionOn(i) && (int)wcslen(s) + 40 < n) {
            if (s[0])
                wcscat(s, L", ");
            wcscat(s, kCamRegions[i].name);
        }
    if (!s[0])
        wcscpy(s, L"не выбраны — нажмите, чтобы выбрать");
}

static long YmdDays(int ymd)   // days since 2000-01-01, enough to count a week
{
    int y = ymd / 10000, m = ymd / 100 % 100, d = ymd % 100;
    if (m < 3) {
        y--;
        m += 12;
    }
    return 365L * y + y / 4 - y / 100 + y / 400 + (153 * (m - 3) + 2) / 5 + d;
}

static int Today()
{
    SYSTEMTIME t;
    GetLocalTime(&t);
    return t.wYear * 10000 + t.wMonth * 100 + t.wDay;
}

static DWORD WINAPI CamDownloadThread(LPVOID)
{
    char regions[200], kinds[120];
    WideCharToMultiByte(CP_UTF8, 0, s_cfg.camRegions, -1, regions, sizeof(regions), NULL, NULL);
    strcpy(kinds, "1000");
    if (s_cfg.camKinds[0]) {
        strcat(kinds, ",");
        WideCharToMultiByte(CP_UTF8, 0, s_cfg.camKinds, -1, kinds + 5, sizeof(kinds) - 5, NULL, NULL);
    }
    int count = 1;
    for (const char* p = regions; *p; p++)
        count += *p == ',';
    wchar_t tmp[MAX_PATH], dir[MAX_PATH];
    _snwprintf(dir, MAX_PATH, L"%s%s", s_dir, kCamFolder);
    CreateDirectory(dir, NULL);   // not in the release: made by the first download
    _snwprintf(tmp, MAX_PATH, L"%sspeedcam.tmp", dir);
    HANDLE h = CreateFile(tmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    bool ok = h != INVALID_HANDLE_VALUE;
    int cams = 0, idx = 0;
    wchar_t err[80] = L"не записать speedcam.tmp";
    for (char* r = regions; ok && *r; idx++) {
        char* e = strchr(r, ',');
        if (e)
            *e = 0;
        _snwprintf(s_dlStatus, 100, L"скачивается %S (%d из %d)…", r, idx + 1, count);
        PostMessage(s_wnd, WM_APP_CAM, 0, 0);
        char path[300];
        _snprintf(path, sizeof(path), "/igo/Rus&filter_id=&cam_name=&cam_adress=&cam_N=&cam_E=&region=%s&kind_list=%s",
                  r, kinds);
        char* body = Fetch("speedcamonline.ru", true, path, NULL, err, 80);
        if (!body) {
            ok = false;
            break;
        }
        char* data = body;
        if (idx > 0) {   // the header only once
            char* nl = strchr(data, '\n');
            data = nl ? nl + 1 : data + strlen(data);
        }
        for (const char* p = data; *p; p++)
            cams += *p == '\n';
        DWORD w;
        ok = WriteFile(h, data, (DWORD)strlen(data), &w, NULL) != 0;
        free(body);
        r = e ? e + 1 : r + strlen(r);
    }
    if (h != INVALID_HANDLE_VALUE)
        CloseHandle(h);
    if (ok) {
        wchar_t dst[MAX_PATH];
        _snwprintf(dst, MAX_PATH, L"%s%s", s_dir, kCamDefault);
        DeleteFile(dst);
        ok = MoveFile(tmp, dst) != 0;
        _snwprintf(s_dlStatus, 100, ok ? L"загружено камер: %d" : L"не заменить speedcam.txt", cams - 1);
    } else {
        DeleteFile(tmp);
        _snwprintf(s_dlStatus, 100, L"не скачалось: %s", err);
    }
    s_dlBusy = 0;
    PostMessage(s_wnd, WM_APP_CAM, ok ? 1 : 0, 0);
    return 0;
}

static void CamDownload()
{
    if (!s_cfg.camRegions[0]) {
        wcscpy(s_dlStatus, L"выберите регионы");
        return;
    }
    if (InterlockedExchange((LONG*)&s_dlBusy, 1))
        return;
    wcscpy(s_dlStatus, L"соединение…");
    HANDLE h = CreateThread(NULL, 0, CamDownloadThread, NULL, 0, NULL);
    if (h)
        CloseHandle(h);
    else
        s_dlBusy = 0;
}

// The download has finished well: speedcam\speedcam.txt becomes the camera file, read again.
static void CamDownloaded()
{
    wcscpy(s_cfg.camFile, kCamDefault);
    s_cfg.camUpdated = Today();
    SaveCfg();
    s_camLoaded = false;
    Gps g;
    GpsGet(&g);
    if (GpsFresh(g, 10000))
        LoadCams(g.lat, g.lon);
    else if (s_cfg.homeLat || s_cfg.homeLon)
        LoadCams(s_cfg.homeLat, s_cfg.homeLon);
}

// Once a week by itself when the setting is on (tried again in an hour if the network isn't there).
static void CamAutoUpdate()
{
    if (!s_cfg.camWeekly || !s_cfg.camRegions[0] || s_dlBusy || GetTickCount() - s_dlTry < 3600000)
        return;
    if (s_cfg.camUpdated && YmdDays(Today()) - YmdDays(s_cfg.camUpdated) < 7)
        return;
    s_dlTry = GetTickCount();
    CamDownload();
}

static void ScrCamDl(HDC dc)
{
    Frame(dc, L"БАЗА КАМЕР · SPEEDCAMONLINE.RU");
    Panel(dc, 16, 64, 768, 330);
    wchar_t s[200];
    RegionsText(s, 200);
    WRow(dc, 68, L"Регионы", s, A_SCREEN, S_CAMREG, 776);
    T(dc, 36, 150, L"Типы камер", 19, kText, 700);
    Button(dc, 372, 126, 136, 36, L"СКОРОСТЬ", B_N, IC_NONE, 13, 0, A_KINDS, 0);
    Button(dc, 516, 126, 136, 36, L"БЕЗ МОБИЛЬНЫХ", B_N, IC_NONE, 13, 0, A_KINDS, 1);
    Button(dc, 660, 126, 108, 36, L"ВСЕ", B_N, IC_NONE, 13, 0, A_KINDS, 2);
    for (int k = 0; k < 18; k++)
        Button(dc, 28 + (k % 4) * 188, 174 + (k / 4) * 43, 180, 37, kCamKinds[k].name, KindOn(k) ? B_SEL : B_N,
               IC_NONE, 12, 0, A_KIND, k);
    Dock(dc);
    if (!s_dlStatus[0] && s_cfg.camUpdated)
        _snwprintf(s_dlStatus, 100, L"обновлено %02d.%02d.%04d", s_cfg.camUpdated % 100, s_cfg.camUpdated / 100 % 100,
                   s_cfg.camUpdated / 10000);
    T(dc, 20, 446, s_dlStatus[0] ? s_dlStatus : L"файл speedcam\\speedcam.txt (iGO) в папке VestaHome", 15, kMuted, 400,
      AL_START, 360);
    Button(dc, 392, 410, 176, 58, L"РАЗ В НЕДЕЛЮ", s_on[T_CAM_WEEKLY] ? B_SEL : B_N, IC_REFRESH, 13, 24, A_TOGGLE,
           T_CAM_WEEKLY);
    Button(dc, 576, 410, 208, 58, s_dlBusy ? L"ЗАГРУЗКА…" : L"СКАЧАТЬ", s_dlBusy ? B_DIS : B_PRI, IC_NONE, 18, 0,
           s_dlBusy ? A_NONE : A_CAMDL);
}

static void ScrCamReg(HDC dc)
{
    int pages = (kCamRegionCount + kRegionsPerPage - 1) / kRegionsPerPage;
    if (s_regPage >= pages)
        s_regPage = 0;
    Frame(dc, L"РЕГИОНЫ КАМЕР");
    for (int k = 0; k < kRegionsPerPage; k++) {
        int i = s_regPage * kRegionsPerPage + k;
        if (i >= kCamRegionCount)
            break;
        wchar_t s[48];
        if (i == 0)
            wcscpy(s, kCamRegions[0].name);
        else
            _snwprintf(s, 48, L"%S  %s", kCamRegions[i].code, kCamRegions[i].name);
        Button(dc, 16 + (k % 4) * 194, 64 + (k / 4) * 55, 184, 48, s, RegionOn(i) ? B_SEL : B_FILL, IC_NONE, 14, 0,
               A_REGION, i);
    }
    Dock(dc);
    Button(dc, 16, 410, 120, 58, NULL, B_N, IC_BACK, 18, 0, A_REGPAGE, -1);
    wchar_t s[16];
    _snwprintf(s, 16, L"%d / %d", s_regPage + 1, pages);
    T(dc, 210, 448, s, 22, kText, 700, AL_MID);
    Button(dc, 284, 410, 120, 58, NULL, B_N, IC_NEXT, 18, 0, A_REGPAGE, 1);
    Button(dc, 584, 410, 200, 58, L"ГОТОВО", B_PRI, IC_NONE, 18, 0, A_BACK);
}

static void DrawScreen(HDC dc, Screen s)
{
    switch (s) {
    case S_HOME:        DrawHome(dc); break;
    case S_RADIO:       ScrRadio(dc); break;
    case S_RADIO_LIST:  ScrRadioList(dc); break;
    case S_RADIO_PTY:   ScrRadioPty(dc); break;
    case S_AUDIO:       ScrAudio(dc); break;
    case S_AUDIO_LIST:  ScrAudioList(dc); break;
    case S_AUDIO_BT:    ScrAudioBt(dc); break;
    case S_AUX:         ScrAux(dc); break;
    case S_VIDEO:       ScrVideo(dc); break;
    case S_VIDEO_PLAY:  ScrVideoPlay(dc); break;
    case S_PHOTO:       ScrPhoto(dc); break;
    case S_PHOTO_VIEW:  ScrPhotoView(dc); break;
    case S_PHOTO_BAD:   ScrPhotoBad(dc); break;
    case S_PHONE:       ScrPhone(dc); break;
    case S_CONTACTS:    ScrContacts(dc); break;
    case S_CONTACT:     ScrContact(dc); break;
    case S_CALLS:       ScrCalls(dc); break;
    case S_CALL:        ScrCall(dc); break;
    case S_INCOMING:    ScrIncoming(dc); break;
    case S_PHONE_OFF:   ScrPhoneOff(dc); break;
    case S_SETTINGS:    ScrSetGeneral(dc); break;
    case S_SET_SOUND:   ScrSetSound(dc); break;
    case S_SET_BALANCE: ScrSetBalance(dc); break;
    case S_SET_SCREEN:  ScrSetScreen(dc); break;
    case S_SET_RADIO:   ScrSetRadio(dc); break;
    case S_SET_BT:      ScrSetBt(dc); break;
    case S_SET_WIFI:    ScrSetWifi(dc); break;
    case S_WIFI_KEYS:   ScrWifiKeys(dc); break;
    case S_SET_VIDEO:   ScrSetVideo(dc); break;
    case S_SET_PHOTO:   ScrSetPhoto(dc); break;
    case S_INFO:        ScrInfo(dc); break;
    case S_WARNING:     ScrWarning(dc); break;
    case S_VOLUME:      ScrVolume(dc); break;
    case S_SAVER:       ScrSaver(dc); break;
    case S_EXIT:        ScrExit(dc); break;
    case S_CAR:         ScrCar(dc); break;
    case S_APPS:        DrawApps(dc); break;
    case S_EDIT:        DrawEdit(dc); break;
    case S_KEYS:        DrawKeys(dc); break;
    case S_FILES:       DrawFiles(dc); break;
    case S_DIAG:        ScrDiag(dc); break;
    case S_TEST_SCREEN: ScrTestScreen(dc); break;
    case S_TEST_TOUCH:  ScrTestTouch(dc); break;
    case S_TEST_MEM:    ScrTestMem(dc); break;
    case S_WSET_WEATHER: ScrWSetWeather(dc); break;
    case S_WSET_SPEED:  ScrWSetSpeed(dc); break;
    case S_WSET_CAR:    ScrWSetCar(dc); break;
    case S_CAMDL:       ScrCamDl(dc); break;
    case S_CAMREG:      ScrCamReg(dc); break;
    default:            break;
    }
}

// A screen and, for the main ones, the bottom bar of sections.
static void DrawFrame(HDC dc)
{
    if (s_screen == S_HOME)
        MainBackground(dc);
    else
        SubBackground(dc);
    kBtnCount = 0;
    DrawScreen(dc, s_screen);
    if (Section(s_screen) >= 0)
        NavBar(dc, Section(s_screen));
}

static int s_drawnMinute = -1;   // the minute on the clock of the last frame

static void Redraw()
{
    SYSTEMTIME t;
    GetLocalTime(&t);
    s_drawnMinute = t.wMinute;
    HDC hdc = GetDC(s_wnd);
    HDC dc = CreateCompatibleDC(hdc);
    HGDIOBJ ob = SelectObject(dc, s_back);
    DrawFrame(dc);
    BitBlt(hdc, 0, 0, 800, 480, dc, 0, 0, SRCCOPY);
    SelectObject(dc, ob);
    DeleteDC(dc);
    ReleaseDC(s_wnd, hdc);
}

// A page turn: the new page slides in from the right (dir 1) or the left (dir -1) and pushes the old one out,
// in the band between the top bar and the bottom bar (they stay). The old frame is what s_back holds now; the
// copy of it lives only for the animation (768 KB).
static void RedrawSlide(int dir)
{
    unsigned short* oldBits = NULL;
    HBITMAP old = MakeDib(800, 480, &oldBits);
    if (!old || !oldBits) {
        if (old)
            DeleteObject(old);
        Redraw();
        return;
    }
    memcpy(oldBits, s_backBits, 800 * 480 * 2);
    SYSTEMTIME t;
    GetLocalTime(&t);
    s_drawnMinute = t.wMinute;
    HDC hdc = GetDC(s_wnd);
    HDC dc = CreateCompatibleDC(hdc), odc = CreateCompatibleDC(hdc);
    HGDIOBJ ob = SelectObject(dc, s_back), oo = SelectObject(odc, old);
    DrawFrame(dc);
    int top = 56, bottom = Section(s_screen) >= 0 ? 408 : 480, h = bottom - top;
    BitBlt(hdc, 0, 0, 800, top, dc, 0, 0, SRCCOPY);
    BitBlt(hdc, 0, bottom, 800, 480 - bottom, dc, 0, bottom, SRCCOPY);
    const DWORD kMs = 220;
    DWORD t0 = GetTickCount();
    for (;;) {
        DWORD el = GetTickCount() - t0;
        double p = el >= kMs ? 1.0 : (double)el / kMs;
        p = 1 - (1 - p) * (1 - p) * (1 - p);   // ease-out
        int off = (int)(800 * p + 0.5);
        if (dir > 0) {
            BitBlt(hdc, 0, top, 800 - off, h, odc, off, top, SRCCOPY);
            BitBlt(hdc, 800 - off, top, off, h, dc, 0, top, SRCCOPY);
        } else {
            BitBlt(hdc, off, top, 800 - off, h, odc, 0, top, SRCCOPY);
            BitBlt(hdc, 0, top, off, h, dc, 800 - off, top, SRCCOPY);
        }
        if (el >= kMs)
            break;
        Sleep(10);
    }
    SelectObject(odc, oo);
    SelectObject(dc, ob);
    DeleteDC(odc);
    DeleteDC(dc);
    ReleaseDC(s_wnd, hdc);
    DeleteObject(old);
}

static int s_slideDir;   // set by an action that turned a page: the next frame slides in

static void Trim(wchar_t* s)
{
    wchar_t* p = s;
    while (*p == L' ' || *p == L'\t')
        p++;
    memmove(s, p, (wcslen(p) + 1) * sizeof(wchar_t));
    int n = (int)wcslen(s);
    while (n > 0 && (s[n - 1] == L' ' || s[n - 1] == L'\t' || s[n - 1] == L'\r'))
        s[--n] = 0;
}

// ---------------------------------------------------------------- buttons

// The buttons are registered while the screen is drawn; a changed screen starts with none.
static void BuildButtons()
{
    kBtnCount = 0;
}

// Opens screen s over the current one (push) or instead of it. A screen that is already under the current
// one is returned to, so going around in circles doesn't pile up the history.
static void Go(Screen s, bool push)
{
    s_status[0] = 0;
    for (int k = 0; k < s_histLen; k++)
        if (s_hist[k] == s) {
            s_histLen = k;
            s_screen = s;
            return;
        }
    if (push) {
        if (s_histLen == 16) {
            memmove(s_hist, s_hist + 1, 15 * sizeof(Screen));
            s_histLen--;
        }
        s_hist[s_histLen++] = s_screen;
    }
    s_screen = s;
    s_screenTick = GetTickCount();
    if (Section(s) == 2)
        s_media = s;
    if (s == S_CALL)
        s_callTick = s_screenTick;
    s_testStep = 0;
    s_touchCount = 0;
}

static void Back()
{
    s_screen = s_histLen ? s_hist[--s_histLen] : S_HOME;
}

// A main screen from the bottom bar (or a widget): the history starts over.
static void Nav(Screen s)
{
    s_histLen = 0;
    s_status[0] = 0;
    s_editMode = false;
    s_screen = s;
    s_screenTick = GetTickCount();
    if (Section(s) == 2)
        s_media = s;
}

// A swipe turns the page of the main screen, the programs or the file browser (it doesn't go round);
// false at the first / last page.
static bool Swipe(int dir)
{
    int was = s_homePage + s_page * 100 + s_filePage * 10000;
    if (s_screen == S_HOME) {
        s_homePage += dir;
        s_homePage = s_homePage < 0 ? 0 : s_homePage >= kHomePages ? kHomePages - 1 : s_homePage;
    } else if (s_screen == S_APPS) {
        s_page += dir;
        s_page = s_page < 0 ? 0 : s_page >= AppsPages() ? AppsPages() - 1 : s_page;
    } else if (s_screen == S_FILES) {
        s_filePage += dir;
        s_filePage = s_filePage < 0 ? 0 : s_filePage >= FilePages() ? FilePages() - 1 : s_filePage;
    }
    return s_homePage + s_page * 100 + s_filePage * 10000 != was;
}

static void DoAction(const Btn& b)
{
    if (b.action != A_DELETE)
        s_delArmed = false;
    switch (b.action) {
    case A_NAV:      Nav((Screen)b.arg); break;
    case A_NIGHT:    s_night = !s_night; break;
    case A_SCREEN:   Go((Screen)b.arg, true); break;
    case A_TAB:      Go((Screen)b.arg, false); break;
    case A_BACK:     Back(); break;
    case A_TOGGLE:
        s_on[b.arg] = !s_on[b.arg];
        if (IsCfgToggle(b.arg))
            ControlsToCfg();
        break;
    case A_SEG:
        s_seg[b.arg / 16] = b.arg % 16;
        if (b.arg / 16 == G_WPROV || b.arg / 16 == G_CAMDIST)
            ControlsToCfg();
        break;
    case A_KEYS:     OpenCfgKeys(b.arg); break;
    case A_HOMEGPS:  HomeFromGps(); break;
    case A_TZ:       TzStep(b.arg); break;
    case A_GPSPORT:  GpsPortStep(b.arg); break;
    case A_GPSBAUD:  GpsBaudStep(b.arg); break;
    case A_WXNOW:    WeatherRefresh(); break;
    case A_CAMFILE:  OpenFiles(F_CAMS); break;
    case A_KIND:     ToggleKind(b.arg); break;
    case A_KINDS:    KindsPreset(b.arg); break;
    case A_REGION:   ToggleRegion(b.arg); break;
    case A_REGPAGE: {
        int pages = (kCamRegionCount + kRegionsPerPage - 1) / kRegionsPerPage;
        s_regPage = (s_regPage + b.arg + pages) % pages;
        s_slideDir = b.arg;
        break;
    }
    case A_CAMDL:    CamDownload(); break;
    case A_KEY:
    case A_DEL: {    // typing into the dialled number or the Wi-Fi password
        int field = b.action == A_KEY ? b.arg / 65536 : b.arg;
        wchar_t* text = field == K_DIAL ? s_dial : s_pass;
        int cap = field == K_DIAL ? 19 : 23, n = (int)wcslen(text);
        if (b.action == A_DEL && n > 0) {
            text[n - 1] = 0;
        } else if (b.action == A_KEY && n < cap) {
            text[n] = (wchar_t)(b.arg % 65536);
            text[n + 1] = 0;
        }
        break;
    }
    case A_PAGE:     // the dots go round
        if (s_screen == S_HOME)
            s_homePage = (s_homePage + 1) % kHomePages;
        else
            s_page = (s_page + 1) % AppsPages();
        s_slideDir = 1;
        break;
    case A_RUN:      if (s_editMode) OpenEditor(b.arg); else RunApp(b.arg); break;
    case A_ADD:      OpenEditor(-1); break;
    case A_EDITMODE: s_editMode = !s_editMode; break;
    case A_FIELD:    if (b.arg == F_PATH || b.arg == F_ICON) OpenFiles(b.arg); else OpenKeys(b.arg); break;
    case A_NOICON:   s_edit.iconPath[0] = 0; break;
    case A_SAVE:     SaveEdit(); break;
    case A_DELETE:   if (s_delArmed) DeleteEdit(); else s_delArmed = true; break;
    case A_CANCEL:   // editor -> apps, keyboard -> where it was opened from, browser -> editor / cameras
        if (s_screen == S_EDIT) {
            s_screen = S_APPS;
            s_status[0] = 0;
        } else if (s_screen == S_KEYS) {
            s_screen = s_keysBack;
        } else {
            s_screen = s_filesFor == F_CAMS ? S_WSET_SPEED : S_EDIT;
        }
        break;
    case A_OK:       KeysDone(); break;
    case A_CHAR:     KeyType((wchar_t)b.arg); break;
    case A_SHIFT:    s_keysShift = !s_keysShift; break;
    case A_LANG:     s_keysEn = !s_keysEn; break;
    case A_BKSP:     KeyBack(); break;
    case A_FILE:     PickFile(b.arg); break;
    case A_UP:       UpDir(); break;
    case A_FPREV:    Swipe(-1); break;
    case A_FNEXT:    Swipe(1); break;
    case A_MINIMIZE: MinimizeHome(); break;
    case A_CALIBRATE: Calibrate(); break;
    case A_SOON:     wcscpy(s_status, b.arg ? L"нужно чтение GPS (NMEA)" : L"нужен протокол MCU"); break;
    case A_TEST:     if (++s_testStep >= kTestSteps) Back(); break;
    case A_EXIT:     DestroyWindow(s_wnd); return;
    default:         break;
    }
    BuildButtons();
}

// The last registered button wins: one drawn over another (a widget's own button) is the one pressed.
static int HitTest(int x, int y)
{
    for (int i = kBtnCount - 1; i >= 0; i--) {
        const RECT& r = s_btns[i].rc;
        if (x >= r.left && x < r.right && y >= r.top && y < r.bottom)
            return i;
    }
    return -1;
}

// ---------------------------------------------------------------- skin

struct SkinColor { const wchar_t* key; COLORREF* c; const char* note; };
static const SkinColor kSkinColors[] = {
    { L"ink",     &kInk,     "background" },
    { L"panel",   &kPanel,   "tiles, buttons, widgets" },
    { L"line",    &kLine,    "borders" },
    { L"text",    &kText,    "main text, built-in icons" },
    { L"muted",   &kMuted,   "secondary text" },
    { L"dim",     &kDim,     "inactive text" },
    { L"accent",  &kOrange,  "pressed button, active mode, headings" },
    { L"facet",   &kFacet,   "tile corner" },
    { L"mosaic1", &kTri[0],  "built-in background triangles (unused with a background picture)" },
    { L"mosaic2", &kTri[1],  "" },
    { L"mosaic3", &kTri[2],  "" },
};

// PNG icon into premultiplied RGBA. Up to 256 x 256: icons only, keeps memory bounded.
static bool LoadImg(const wchar_t* path, Img* im)
{
    DWORD size;
    char* data = ReadAll(path, &size);
    if (!data)
        return false;
    int comp;
    im->px = stbi_load_from_memory((unsigned char*)data, size, &im->w, &im->h, &comp, 4);
    free(data);
    if (im->px && (im->w > 256 || im->h > 256)) {
        stbi_image_free(im->px);
        im->px = NULL;
    }
    if (!im->px)
        return false;
    for (int i = 0; i < im->w * im->h; i++) {
        unsigned char* p = im->px + i * 4;
        for (int k = 0; k < 3; k++)
            p[k] = (unsigned char)(p[k] * p[3] / 255);
    }
    return true;
}

// Skin\skin.ini: [colors] name=RRGGBB, [images] name=file.png (in the Skin folder). Read once at start.
static void LoadSkin()
{
    wchar_t path[MAX_PATH];
    _snwprintf(path, MAX_PATH, L"%sSkin\\skin.ini", s_dir);
    DWORD size;
    char* buf = ReadAll(path, &size);
    if (!buf)
        return;
    bool images = false;
    char* p = buf;
    if ((unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF)
        p += 3;
    while (*p) {
        char* end = strchr(p, '\n');
        if (end)
            *end = 0;
        wchar_t line[300];
        int n = MultiByteToWideChar(CP_UTF8, 0, p, -1, line, 300);
        p = end ? end + 1 : p + strlen(p);
        if (n <= 0)
            continue;
        wchar_t* semi = wcschr(line, L';');
        if (semi)
            *semi = 0;
        Trim(line);
        if (line[0] == L'[') {
            images = !_wcsicmp(line, L"[images]");
            continue;
        }
        wchar_t* eq = wcschr(line, L'=');
        if (!eq)
            continue;
        *eq = 0;
        wchar_t* val = eq + 1;
        Trim(line);
        Trim(val);
        if (!images) {
            if (*val == L'#')
                val++;
            wchar_t* e;
            unsigned long v = wcstoul(val, &e, 16);
            if (e - val != 6)
                continue;
            for (int k = 0; k < (int)(sizeof(kSkinColors) / sizeof(kSkinColors[0])); k++)
                if (!_wcsicmp(line, kSkinColors[k].key))
                    *kSkinColors[k].c = RGB(v >> 16, (v >> 8) & 255, v & 255);
            continue;
        }
        if (!val[0])
            continue;
        wchar_t file[MAX_PATH];
        _snwprintf(file, MAX_PATH, L"%sSkin\\%s", s_dir, val);
        file[MAX_PATH - 1] = 0;
        if (!_wcsicmp(line, L"background")) {
            wcscpy(s_skinBg, file);
            continue;
        }
        int len = (int)wcslen(line);
        int down = len > 5 && !_wcsicmp(line + len - 5, L"_down");
        if (down)
            line[len - 5] = 0;
        for (int ic = 0; ic < IC_COUNT; ic++)
            if (kIconKey[ic] && !_wcsicmp(line, kIconKey[ic]) && !s_icons[ic][down].px)
                LoadImg(file, &s_icons[ic][down]);
    }
    free(buf);
}

#ifndef UNDER_CE
// ---------------------------------------------------------------- skin export (PC: VestaHomePC.exe /export)

static unsigned long Crc32(unsigned long c, const unsigned char* p, unsigned long n)
{
    c = ~c;
    while (n--) {
        c ^= *p++;
        for (int k = 0; k < 8; k++)
            c = (c >> 1) ^ (0xEDB88320UL & (0 - (c & 1)));
    }
    return ~c;
}

static void PutBE(unsigned char* p, unsigned long v)
{
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

static void PngChunk(FILE* f, const char* type, const unsigned char* data, unsigned long n)
{
    unsigned char b[4];
    PutBE(b, n);
    fwrite(b, 1, 4, f);
    fwrite(type, 1, 4, f);
    fwrite(data, 1, n, f);
    PutBE(b, Crc32(Crc32(0, (const unsigned char*)type, 4), data, n));
    fwrite(b, 1, 4, f);
}

// Minimal PNG writer, 8-bit RGB (ch = 3) or RGBA (ch = 4), deflate "stored" blocks: no compression.
static void WritePng(const wchar_t* path, const unsigned char* px, int w, int h, int ch)
{
    unsigned long line = w * ch + 1, raw = line * h;
    unsigned char* rows = (unsigned char*)malloc(raw);
    unsigned char* z = (unsigned char*)malloc(raw + raw / 65535 * 5 + 16);
    for (int y = 0; y < h; y++) {
        rows[y * line] = 0;   // filter: none
        memcpy(rows + y * line + 1, px + y * w * ch, w * ch);
    }
    unsigned char* q = z;
    *q++ = 0x78;
    *q++ = 0x01;
    for (unsigned long i = 0; i < raw; i += 65535) {
        unsigned long n = raw - i < 65535 ? raw - i : 65535;
        *q++ = i + n == raw ? 1 : 0;
        *q++ = (unsigned char)n;
        *q++ = (unsigned char)(n >> 8);
        *q++ = (unsigned char)~n;
        *q++ = (unsigned char)(~n >> 8);
        memcpy(q, rows + i, n);
        q += n;
    }
    unsigned long a = 1, b = 0;
    for (unsigned long i = 0; i < raw; i++) {
        a = (a + rows[i]) % 65521;
        b = (b + a) % 65521;
    }
    PutBE(q, (b << 16) | a);
    q += 4;
    unsigned char hdr[13];
    PutBE(hdr, w);
    PutBE(hdr + 4, h);
    hdr[8] = 8;
    hdr[9] = ch == 4 ? 6 : 2;
    hdr[10] = hdr[11] = hdr[12] = 0;
    FILE* f = _wfopen(path, L"wb");
    if (f) {
        fwrite("\x89PNG\r\n\x1a\n", 1, 8, f);
        PngChunk(f, "IHDR", hdr, 13);
        PngChunk(f, "IDAT", z, (unsigned long)(q - z));
        PngChunk(f, "IEND", NULL, 0);
        fclose(f);
    }
    free(rows);
    free(z);
}

// Writes the built-in icons to Skin\ as <name>.png (64 x 64, drawn 4x larger white on black and averaged
// into the alpha channel: antialiased lines) and skin.ini with the built-in colours. No pressed variants and
// no background picture: they cost memory on the unit (the pressed state tints the icon dark, the background
// is the built-in mosaic). Doesn't read the existing skin.ini.
static void ExportSkin()
{
    wchar_t dir[MAX_PATH], path[MAX_PATH];
    _snwprintf(dir, MAX_PATH, L"%sSkin\\", s_dir);
    CreateDirectory(dir, NULL);

    BITMAPINFO bi;
    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = 800;
    bi.bmiHeader.biHeight = -480;   // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = NULL;
    HBITMAP bmp = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    HDC dc = CreateCompatibleDC(NULL);
    HGDIOBJ ob = SelectObject(dc, bmp);
    const unsigned* px = (const unsigned*)bits;   // 0x00RRGGBB

    static unsigned char out[64 * 64 * 4];
    for (int ic = 0; ic < IC_COUNT; ic++) {
        if (!kIconKey[ic])
            continue;
        Fill(dc, 0, 0, 256, 256, RGB(0, 0, 0));
        PaintIcon(dc, (Icon)ic, 0, 0, 256, RGB(255, 255, 255));
        GdiFlush();
        for (int y = 0; y < 64; y++)
            for (int x = 0; x < 64; x++) {
                int sum = 0;
                for (int j = 0; j < 4; j++)
                    for (int i = 0; i < 4; i++)
                        sum += (px[(y * 4 + j) * 800 + x * 4 + i] >> 16) & 255;
                unsigned char* o = out + (y * 64 + x) * 4;
                o[0] = GetRValue(kText);
                o[1] = GetGValue(kText);
                o[2] = GetBValue(kText);
                o[3] = (unsigned char)(sum / 16);
            }
        _snwprintf(path, MAX_PATH, L"%s%s.png", dir, kIconKey[ic]);
        WritePng(path, out, 64, 64, 4);
    }
    SelectObject(dc, ob);
    DeleteDC(dc);
    DeleteObject(bmp);

    _snwprintf(path, MAX_PATH, L"%sskin.ini", dir);
    FILE* f = _wfopen(path, L"w");
    if (!f)
        return;
    fprintf(f, "; VestaHome skin: the Skin folder next to VestaHome.exe. Read once at start.\n"
               "; [colors]: RRGGBB. [images]: PNG files in this folder, transparency supported.\n"
               "; Icons are square and drawn scaled: 60 px on tiles, 40 px on buttons, other sizes on the\n"
               "; other screens, where they are tinted with that screen's colour unless it is the text colour.\n"
               "; Optional: <name>_down = a separate pressed picture (otherwise the icon is tinted dark),\n"
               ";   background = an 800 x 480 picture for the main menu instead of the built-in mosaic.\n"
               "; Each picture costs memory: a 64 x 64 icon 16 KB, the background 750 KB.\n"
               "; A missing line or file falls back to the built-in drawing.\n\n[colors]\n");
    for (int k = 0; k < (int)(sizeof(kSkinColors) / sizeof(kSkinColors[0])); k++) {
        COLORREF c = *kSkinColors[k].c;
        fprintf(f, "%-8S= %02X%02X%02X%s%s\n", kSkinColors[k].key, GetRValue(c), GetGValue(c), GetBValue(c),
                kSkinColors[k].note[0] ? "   ; " : "", kSkinColors[k].note);
    }
    fprintf(f, "\n[images]\n");
    for (int ic = 0; ic < IC_COUNT; ic++)
        if (kIconKey[ic])
            fprintf(f, "%S = %S.png\n", kIconKey[ic], kIconKey[ic]);
    fclose(f);
}
#endif

// ---------------------------------------------------------------- window

// VestaLaunch follows our day/night: it asks with this message, 1 = night.
#define WM_VESTA_NIGHT (WM_APP + 2)

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE: {
        s_wnd = hwnd;
        s_back = MakeDib(800, 480, &s_backBits);
        LoadSkin();
        BuildBackground();
#ifdef UNDER_CE
        s_taskbar = FindWindow(L"HHTaskBar", NULL);
        if (s_taskbar)
            ShowWindow(s_taskbar, SW_HIDE);
#endif
        LoadApps();
        LoadCfg();
        CfgToControls();
        {
            WSADATA wsa;
            WSAStartup(MAKEWORD(2, 2), &wsa);
        }
        InitializeCriticalSection(&s_wxCs);
        HttpInit();
        GpsStart();
        WeatherRefresh();
        BuildButtons();
        SetTimer(hwnd, 1, 1000, NULL);
        return 0;
    }
    case WM_TIMER: {
        if (wp == TIMER_GUARD) {
            OnGuardTimer();
            return 0;
        }
        if (wp == TIMER_WAIT) {
            OnWaitTimer();
            if (IsWindowVisible(hwnd))
                Redraw();   // the status line may have changed
            return 0;
        }
        // the volume window closes by itself after 2.5 s, the warning after 10 s
        DWORD open = GetTickCount() - s_screenTick;
        if ((s_screen == S_VOLUME && open > 2500) || (s_screen == S_WARNING && open > 10000)) {
            Back();
            BuildButtons();
            Redraw();
            return 0;
        }
        {   // the weather: every 30 min, after a failure or without data again in 5 min
            Weather w;
            WxGet(&w);
            DWORD now = GetTickCount();
            if (!s_wxBusy && ((w.have && w.ok > 0 && now - w.tick > 1800000) || (w.ok <= 0 && now - s_wxTry > 300000)))
                WeatherRefresh();
        }
        CamAutoUpdate();
        // a whole frame costs: redraw only for a new minute (or the call timer), and only when in front
        SYSTEMTIME t;
        GetLocalTime(&t);
        if (IsWindowVisible(hwnd) && GetForegroundWindow() == hwnd && (t.wMinute != s_drawnMinute || s_screen == S_CALL))
            Redraw();
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        Redraw();
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_ACTIVATE:   // back in front: the clock may be behind
        if (LOWORD(wp) != WA_INACTIVE)
            Redraw();
        return 0;
    case WM_VESTA_NIGHT:
        return s_night ? 1 : 0;
    case WM_APP_GPS: {   // at most once a second: the clock, the cameras, the speed widget
        static DWORD s_drawTick;
        static bool s_wxByGps;
        static DWORD s_logTick;
        Gps g;
        GpsGet(&g);
        if (GetTickCount() - s_logTick >= 60000) {   // once a minute: the last line before a hang tells when
            s_logTick = GetTickCount();
            MEMORYSTATUS ms;
            ms.dwLength = sizeof(ms);
            GlobalMemoryStatus(&ms);
            VLog(L"gps: фикс %d, спутников %d, %.0f км/ч; память занята %u%%, свободно %u КБ", GpsFresh(g, 5000) ? 1 : 0,
                 g.sats, g.speedKmh, (unsigned)ms.dwMemoryLoad, (unsigned)(ms.dwAvailPhys / 1024));
        }
        GpsTimeSync(g);
        if (g.date)
            HttpSetGpsDate(g.date);   // checks the certificates when the clock is wrong
        bool cam = CamCheck(g);
        if (s_cfg.weatherGps && !s_wxByGps && GpsFresh(g, 5000)) {   // the first fix: the weather for here
            s_wxByGps = true;
            WeatherRefresh();
        }
        bool shown = (s_screen == S_HOME && s_homePage == 0) || s_screen == S_WSET_SPEED;
        if (IsWindowVisible(hwnd) && GetForegroundWindow() == hwnd && (cam || (shown && GetTickCount() - s_drawTick >= 1000))) {
            s_drawTick = GetTickCount();
            Redraw();
        }
        return 0;
    }
    case WM_APP_WX:
        if (IsWindowVisible(hwnd))
            Redraw();
        return 0;
    case WM_APP_CAM:   // the camera base download's progress; wp = 1: done well
        if (wp)
            CamDownloaded();
        if (IsWindowVisible(hwnd))
            Redraw();
        return 0;
    case WM_LBUTTONDOWN:
        s_downX = (short)LOWORD(lp);
        s_pressed = HitTest((short)LOWORD(lp), (short)HIWORD(lp));
        if (s_screen == S_TEST_TOUCH && s_pressed < 0)
            AddTouch((short)LOWORD(lp), (short)HIWORD(lp));
        SetCapture(hwnd);
        Redraw();
        return 0;
    case WM_MOUSEMOVE:   // the touch test follows the finger
        if (s_screen == S_TEST_TOUCH && (wp & MK_LBUTTON) && s_pressed < 0) {
            AddTouch((short)LOWORD(lp), (short)HIWORD(lp));
            Redraw();
        }
        return 0;
    case WM_SHOWWINDOW:   // shown again after "Minimize VH": the taskbar goes away again
#ifdef UNDER_CE
        if (wp) {
            s_taskbar = FindWindow(L"HHTaskBar", NULL);
            if (s_taskbar)
                ShowWindow(s_taskbar, SW_HIDE);
        }
#endif
        return 0;
    case WM_LBUTTONUP: {
        ReleaseCapture();
        int x = (short)LOWORD(lp), dx = x - s_downX;
        int i = HitTest(x, (short)HIWORD(lp));
        int was = s_pressed;
        s_pressed = -1;
        if ((s_screen == S_HOME || s_screen == S_APPS || s_screen == S_FILES) && (dx > 120 || dx < -120)) {
            Redraw();   // the old page without the pressed look: the frame the new one pushes out
            if (Swipe(dx < 0 ? 1 : -1)) {
                BuildButtons();
                RedrawSlide(dx < 0 ? 1 : -1);
            }
            return 0;
        }
        Redraw();
        if (i >= 0 && i == was) {
            s_slideDir = 0;
            DoAction(s_btns[i]);
            if (IsWindow(hwnd) && IsWindowVisible(hwnd)) {
                if (s_slideDir)
                    RedrawSlide(s_slideDir);
                else
                    Redraw();
            }
            s_slideDir = 0;
        }
        return 0;
    }
#ifndef UNDER_CE
    case WM_CHAR:      // PC test: typing on the keyboard screen; Enter = done, Esc = cancel
        if (s_screen != S_KEYS)
            return 0;
        if (wp == VK_ESCAPE)
            s_screen = s_keysBack;
        else if (wp == VK_RETURN)
            KeysDone();
        else if (wp == VK_BACK)
            KeyBack();
        else if (wp >= 32)
            KeyType((wchar_t)wp);
        BuildButtons();
        Redraw();
        return 0;
    case WM_KEYDOWN:   // PC test: N = night, Esc = exit (not while typing)
        if (s_screen == S_KEYS)
            return 0;
        if (wp == 'N') {
            s_night = !s_night;
            Redraw();
        } else if (wp == VK_ESCAPE) {
            DestroyWindow(hwnd);
        }
        return 0;
#endif
    case WM_DESTROY:
        GpsStopThread();
        HttpCloseAll();
        KillTimer(hwnd, 1);
#ifdef UNDER_CE
        if (s_taskbar)
            ShowWindow(s_taskbar, SW_SHOW);
#endif
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

#ifdef UNDER_CE
int WINAPI WinMain(HINSTANCE inst, HINSTANCE, LPWSTR, int)
#else
int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, LPWSTR cmd, int)
#endif
{
    GetModuleFileName(NULL, s_dir, MAX_PATH);
    wchar_t* slash = wcsrchr(s_dir, L'\\');
    if (slash)
        slash[1] = 0;
    InitializeCriticalSection(&s_logLock);
#ifndef UNDER_CE
    if (wcsstr(cmd, L"/export")) {
        ExportSkin();
        return 0;
    }
#endif

    // One copy: a second start only shows the running copy. The named mutex also catches a start racing
    // the first one before its window exists (a double tap while the first copy is still loading).
    CreateMutex(NULL, FALSE, L"VestaHome_Once");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        for (int i = 0; i < 50; i++) {   // the first copy may still be loading: wait for its window up to 5 s
            HWND prev = FindWindow(APP_CLASS, NULL);
            if (prev) {
#ifdef UNDER_CE
                ShowWindow(prev, SW_SHOW);   // also after "Minimize VH" (hidden)
#else
                ShowWindow(prev, IsIconic(prev) ? SW_RESTORE : SW_SHOW);   // PC: "Minimize VH" minimizes
#endif
#ifdef UNDER_CE
                SetWindowPos(prev, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
#endif
                SetForegroundWindow(prev);
                break;
            }
            Sleep(100);
        }
        return 0;
    }
    VLog(L"=== VestaHome запущен");

    WNDCLASS wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.lpszClassName = APP_CLASS;
#ifndef UNDER_CE
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
#endif
    RegisterClass(&wc);

#ifdef UNDER_CE
    DWORD style = WS_POPUP | WS_VISIBLE;
    int x = 0, y = 0, w = 800, h = 480;
#else
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE;
    RECT rc = { 0, 0, 800, 480 };
    AdjustWindowRect(&rc, style, FALSE);
    int x = CW_USEDEFAULT, y = CW_USEDEFAULT, w = rc.right - rc.left, h = rc.bottom - rc.top;
#endif
#ifdef UNDER_CE
    DWORD exStyle = WS_EX_TOPMOST;   // the stock shell is topmost, a normal window would stay under it
#else
    DWORD exStyle = 0;
#endif
    HWND hwnd = CreateWindowEx(exStyle, APP_CLASS, L"VestaHome", style, x, y, w, h, NULL, NULL, inst, NULL);
    if (!hwnd)
        return 1;
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    SetForegroundWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
