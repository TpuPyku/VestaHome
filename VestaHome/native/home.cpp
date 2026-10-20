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
#include <tlhelp32.h>   // our threads' load (OwnThreadsLoad); toolhelp.dll itself is loaded at run time
#include "https.h"
#include "video.h"
#include "wifi.h"
#include "obd.h"
#include "secret.h"
#include "rvc.h"
#include "vlogdir.h"   // VestaCore: logs only on the SD card / USB disk
#include "vnight.h"    // VestaCore: the night mode shared with OSMapsCE
#include "../../SPIPower/bt/btshm.h"   // the Bluetooth proxy's shared memory

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG   // photos: progressive JPEGs (TJpgDec reads baseline ones), small ones only
#define STBI_ONLY_BMP
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_SIMD
#define STBI_NO_THREAD_LOCALS
#define STBI_ASSERT(x) ((void)0)
#include "stb_image.h"
#include "photo.h"

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
            IC_DISC, IC_EYE, IC_POWER, IC_EDIT, IC_ADD, IC_FILE, IC_UP, IC_CAR, IC_CLOUD, IC_TASKS, IC_SD, IC_DOWN, IC_GALLERY, IC_COUNT };

// Picture names in skin.ini (and Skin\<name>.png), by icon; NULL = no picture, always the built-in drawing.
static const wchar_t* kIconKey[IC_COUNT] = { NULL, NULL, NULL, L"radio", L"music", L"phone", L"settings", L"home",
    L"night", L"prev", L"day", L"apps", L"video", L"next", L"exit", L"folder", L"shift", L"backspace", L"aux_ico",
    L"photo", L"info", L"rew", L"fwd", L"pause" };

// A_NAV opens a main screen from the bottom bar (no history), A_SCREEN opens a screen over the current one,
// A_TAB opens it instead of the current one (sections, sources), A_BACK returns. A_TOGGLE, A_SEG, A_KEY and A_DEL
// only change the demo values below. A_PAGE and the rest belong to the programs ("Apps", the shortcut editor).
enum Action { A_NONE, A_NIGHT, A_EXIT, A_NAV, A_SCREEN, A_TAB, A_BACK, A_TOGGLE, A_SEG, A_KEY, A_DEL, A_PAGE,
              A_RUN, A_ADD, A_EDITMODE, A_FIELD, A_SAVE, A_DELETE, A_CANCEL, A_OK, A_CHAR, A_SHIFT, A_LANG,
              A_BKSP, A_FILE, A_UP, A_NOICON, A_MINIMIZE, A_CALIBRATE, A_SOON, A_TEST,
              A_KEYS, A_HOMEGPS, A_WXNOW, A_CAMFILE, A_KIND, A_KINDS, A_REGION, A_REGPAGE, A_CAMDL, A_TZ,
              A_GPSPORT, A_GPSBAUD, A_HOUR, A_MINUTE, A_CANLOG, A_CANMARK, A_MCULOG, A_MCUREQ, A_EQ, A_EQPRESET,
              A_BAL, A_FADE, A_BALCENTER, A_BALXY, A_AVTEST, A_AVDEV, A_PRESET, A_RKEY, A_REBOOTNO, A_BTMUSIC, A_BTCMD, A_RPAGE, A_RLIST, A_RLISTPAGE, A_RSCAN, A_SNDVOL, A_PLAY, A_PREVTRK, A_NEXTTRK, A_SEEK, A_SHUFFLE, A_REPEAT,
              A_MFILE, A_MUP, A_MALL, A_PHONECALL, A_PHONECLR, A_PHONEANS, A_PHONEEND, A_THEME, A_LIGHT, A_WGVIS, A_WGMOVE, A_PHFILE, A_PHSTEP, A_PHROT, A_PHSHOW, A_PHINT, A_PHPLAY, A_LICSCROLL, A_USERWG, A_SHPAGE, A_MVOL, A_TASKTAB, A_TASKSORT, A_TASKPAGE, A_TASKKILL, A_TASKSWITCH,
              A_TASKCLOSE, A_KEYFIELD, A_CLIPCOPY, A_CLIPPASTE, A_CLIPCLEAR,
              A_VFILE, A_VUP, A_VPLAY, A_VPREV, A_VNEXT, A_VSEEK, A_VFULL,
              A_NRPLAY, A_NRSTEP, A_NRPICK, A_NRPAGE, A_WIFINET, A_WIFIFORGET, A_WIFISCAN, A_WIFIPAGE, A_WIFIAP,
              A_OBDSCAN, A_OBDPICK, A_OBDPIN, A_OBDPAIR, A_OBDCONN, A_BTUNPAIR, A_RVCVIEW };

// S_SETTINGS is the general section; S_VIDEO and S_PHOTO are the file lists.
// S_INCOMING, S_VOLUME, S_EXIT (closing VestaHome) and S_REBOOT (the knob held) are windows over the previous screen.
// S_APPS: the programs; S_EDIT, S_KEYS, S_FILES: their shortcut editor, keyboard and file browser; S_TASKS: the
// task manager (processes, windows, CPU and memory).
// S_SET_CAR: the car section of the settings (the data port, CAN). S_DIAG: diagnostics, opened from "About the
// system" (as the stock shell's engineering menu from its "Info"), S_TEST_*: its tests. S_WSET_*: widgets' settings.
enum Screen { S_HOME, S_RADIO, S_RADIO_LIST, S_RADIO_PTY, S_AUDIO, S_AUDIO_LIST, S_AUDIO_BT, S_AUX,
              S_VIDEO, S_VIDEO_PLAY, S_PHOTO, S_PHOTO_VIEW,
              S_PHONE, S_CONTACTS, S_CONTACT, S_CALLS, S_CALL, S_INCOMING, S_PHONE_OFF,
              S_SETTINGS, S_SET_SOUND, S_SET_BALANCE, S_SET_SCREEN, S_SET_RADIO, S_SET_BT, S_SET_WIFI, S_WIFI_KEYS,
              S_SET_VIDEO, S_SET_PHOTO, S_INFO, S_WARNING, S_VOLUME, S_SAVER, S_EXIT, S_REBOOT,
              S_CAR, S_APPS, S_EDIT, S_KEYS, S_FILES, S_DIAG, S_TEST_SCREEN, S_TEST_TOUCH, S_TEST_MEM,
              S_WSET_WEATHER, S_WSET_SPEED, S_WSET_CAR, S_SET_HOME, S_CAMDL, S_CAMREG, S_SET_CAR, S_TEST_CAN, S_TEST_MCU,
              S_INFO_MORE, S_LICENSES, S_SET_VOLUME, S_TASKS, S_NETRADIO, S_NETRADIO_LIST, S_TEST_AV, S_SET_OBD, S_BT_PAIRED, S_NONE };

struct Btn {
    RECT        rc;
    Action      action;
    int         arg;       // A_NAV, A_SCREEN, A_TAB: Screen; A_TOGGLE: T_*; A_SEG: G_* * 16 + option;
                           // A_KEY: K_* * 65536 + character; A_DEL: K_*; A_RUN: index in s_apps; A_FIELD: F_*;
                           // A_CHAR: character; A_FILE: index in s_files; A_TASKKILL: process id;
                       // A_TASKSWITCH, A_TASKCLOSE: the window (HWND)
};

// Values of the screens' switches, segmented choices and two text fields: demo ones, except the settings kept
// in VestaHome.ini (T_GPS_TIME, T_W_GPS .. T_CAM_ALERT, G_WPROV, G_CAMDIST, G_CAMSND: see CfgToControls).
enum { T_SAVER, T_GPS_TIME, T_LOUDNESS, T_NOISE, T_MW, T_RDS, T_AF, T_CT, T_TA, T_BT, T_AUTOCONNECT, T_AUTOANSWER,
       T_WIFI, T_VIDEO_MOVING, T_SUBTITLES, T_SLIDES_OPEN, T_AUTOROTATE, T_MUTED,
       T_W_GPS, T_CLIMATE, T_CAM_ALERT, T_CAM_WEEKLY, T_RVC_OWN, T_WIFI_AP_AUTO, T_MS, T_COUNT };
static bool s_on[T_COUNT] = { true, true, true, true, false, true, false, true, true, true, true, false,
                              true, false, true, false, true, true };
enum { G_PRESET, G_NIGHT, G_TIME_FMT, G_EQ, G_SPEED_VOL, G_VIDEO_SCALE, G_VIDEO_FMT, G_PHOTO_SCALE, G_CALLS,
       G_WPROV, G_CAMDIST, G_CAMSND, G_SAVER, G_COUNT };
static int s_seg[G_COUNT] = { 2, 0, 1, 2, 2, 1, 0, 0, 0 };
enum { K_DIAL, K_PASS };   // the number being dialled, the Wi-Fi password
enum { K_WKEY, K_HOMENAME, K_HOMECOORD };   // settings typed on the keyboard (A_KEYS)
static wchar_t s_dial[20];          // the number typed on the phone's keypad
static wchar_t s_callOut[20];       // the number dialled from VestaHome (the stack does not tell it back)
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
static wchar_t s_skinSaver[MAX_PATH];  // the screen saver's picture, optional

static HWND    s_wnd;
static HBITMAP s_back;          // whole frame, drawn off-screen
static HBITMAP s_bg;            // the skin's main menu background picture, only if it gives one (750 KB)
static unsigned short* s_backBits;   // their pixels: RGB565, bottom-up, 800 per row
static unsigned short* s_bgBits;
static HBITMAP s_saverBg;       // the screen saver's picture: loaded while it shows (750 KB)
static unsigned short* s_saverBits;
static unsigned short* s_tile[2];    // one period of the triangle mosaic (kTileW x kTileH, top-down), day / night
static int     s_pressed = -1;
static bool    s_night;
static int     s_mcuNight = -1, s_bright = -1;   // the backlight: 0x4105 day / night, 0x4107 level
// A partial frame (RedrawArea): everything is drawn clipped to s_clip, the buttons are not registered again
// (the layout is the same), only the area goes to the screen and only when its pixels changed.
static RECT    s_clip = { 0, 0, 800, 480 };
static bool    s_partial;
static int     s_zoneIdx;   // the buttons counted during a partial frame (for the pressed look)
static wchar_t s_dir[MAX_PATH];

// Events log VestaHome.log - on the unit only on the SD card / USB disk (VestaCore LogDir: VestaLogs folder), never
// the built-in flash; without a card no log. What happened just before a hang of the unit. Opened and closed
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
    EnterCriticalSection(&s_logLock);
    if (!LogDir(path, s_dir)) {
        LeaveCriticalSection(&s_logLock);
        return;
    }
    wcscat(path, L"VestaHome.log");
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
static int     s_downX, s_downY;   // where the press started: a horizontal swipe turns the page
static bool    s_drag;          // a page follows the finger (DragStart .. DragEnd): no other redraws meanwhile
static bool    s_listDrag;      // a list follows the finger up / down (the internet radio's stations, the licenses)
static int     s_listDrag0;     // its scroll when the finger started
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
        if (py < s_clip.top || py >= s_clip.bottom)
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
            if (px < s_clip.left || px >= s_clip.right)
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

// A skin's 800 x 480 picture into bits (as is: night dims it when a frame is drawn).
static bool LoadBackground(const wchar_t* path, unsigned short* bits)
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
        unsigned short* dst = bits + (479 - y) * 800;
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
        if (s_bg && !LoadBackground(s_skinBg, s_bgBits)) {
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
    if (top < s_clip.top)
        top = s_clip.top;
    if (bottom > s_clip.bottom)
        bottom = s_clip.bottom;
    for (int y = top; y < bottom; y++) {
        const unsigned short* src = t + ((y - anchor) % kTileH) * kTileW;
        unsigned short* dst = s_backBits + (479 - y) * 800;
        for (int x = s_clip.left; x < s_clip.right;) {
            int o = x % kTileW, n = kTileW - o < s_clip.right - x ? kTileW - o : s_clip.right - x;
            memcpy(dst + x, src + o, n * 2);
            x += n;
        }
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

// A full-screen picture into the frame, a third darker with the night theme (as C()).
static void BlitNight(HDC dc, HBITMAP bmp)
{
    Blit(dc, bmp);
    if (s_night) {
#ifndef UNDER_CE
        GdiFlush();
#endif
        for (int y = s_clip.top; y < s_clip.bottom; y++) {
            unsigned short* row = s_backBits + (479 - y) * 800;
            for (int x = s_clip.left; x < s_clip.right; x++)
                row[x] = Dim565(row[x]);
        }
    }
}

// The main screen: the skin's background picture (dimmed per frame at night), otherwise the same as the others.
static void MainBackground(HDC dc)
{
    if (!s_bg) {
        SubBackground(dc);
        return;
    }
    BlitNight(dc, s_bg);
}

// The screen saver's picture: loaded on its first frame, freed when it closes (SaverFree); none - plain dark.
static void SaverBackground(HDC dc)
{
    if (!s_saverBg && s_skinSaver[0]) {
        s_saverBg = MakeDib(800, 480, &s_saverBits);
        if (s_saverBg && !LoadBackground(s_skinSaver, s_saverBits)) {
            DeleteObject(s_saverBg);
            s_saverBg = NULL;
            s_skinSaver[0] = 0;   // not again
        }
    }
    if (s_saverBg)
        BlitNight(dc, s_saverBg);
    else
        Fill(dc, 0, 0, 800, 480, kInk);
}

static void SaverFree()
{
    if (s_saverBg) {
        DeleteObject(s_saverBg);
        s_saverBg = NULL;
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

// s cut to fit maxw with an ellipsis at its end (into out, cap characters); returns its width.
static int Ellipsize(HDC dc, const wchar_t* s, int size, int weight, int maxw, wchar_t* out, int cap)
{
    int n = (int)wcslen(s);
    if (n > cap - 2)
        n = cap - 2;
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
    while (maxw > 0 && w > maxw && size > 10)
        w = TextWidth(dc, s, --size, weight);
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
            take = 0;
            for (int i = 1; i <= n && i < 255; i++) {
                if (i < n && s[i] != L' ')
                    continue;
                wcsncpy(line, s, i);
                line[i] = 0;
                if (TextWidth(dc, line, size, weight) > maxw)
                    break;
                take = i;
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
        if (py < s_clip.top || py >= s_clip.bottom)
            continue;
        unsigned short* row = s_backBits + (479 - py) * 800;
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
        unsigned short* row = s_backBits + (479 - py) * 800;
        for (int px = x > s_clip.left ? x : s_clip.left; px < x + w && px < s_clip.right; px++) {
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
    if (s_partial)   // the buttons of the full frame stay; only the pressed look is told
        return s_zoneIdx++ == s_pressed;
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
// icColor: the icon's own colour (folders in the browsers), CLR_INVALID = muted like the text.
static void ListRow(HDC dc, int x, int y, int w, int h, const wchar_t* text, const wchar_t* right, int state,
                    Icon ic = IC_NONE, const wchar_t* sub = NULL, const wchar_t* num = NULL, Action a = A_NONE,
                    int arg = 0, COLORREF icColor = CLR_INVALID)
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
        Ico(dc, ic, tx, y + (h - 30) / 2, 30, cur ? kInk : icColor != CLR_INVALID ? icColor : kMuted);
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
// the last action's result, then the status marks (GPS, Bluetooth, USB), the volume (MCU) and the clock.
// A main screen may put one action button of its own before the marks (act). Demo: the Bluetooth mark opens
// the incoming call window, the clock the volume window. VestaHome is closed from the settings' diagnostics.
static bool OutsideTemp(int* t);
static bool VolGet(int* vol, int* mute, int* src);
static bool CamChip(wchar_t* s, int n);
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

// The status marks of the top bar, right to left from `end`: the USB disk and the SD card (only when there),
// Wi-Fi (orange when connected, dim when not; Settings -> Wi-Fi), Bluetooth (orange with a phone connected), GPS (orange
// with a fix). The ones that would reach left of minLeft are left out. Returns the left edge.
struct BtState;
static bool BtGet(BtState* st, LONG* seq);

static int StatusMarks(HDC dc, int end, int minLeft)
{
    BtState bt;
    bool btOn = BtGet(&bt, NULL) && (bt.hf == 2 || bt.av >= 1);   // a phone connected (proxy Bluetooth)
    WifiInfo w;
    WifiGet(&w);
    struct Mark { Icon ic; COLORREF c; bool shown; Action a; int arg; };
    Mark m[5] = {
        { IC_USB, kOrange, (s_mediaMask & 2) != 0, A_NONE, 0 },
        { IC_SD, kOrange, (s_mediaMask & 1) != 0, A_NONE, 0 },
        { IC_WIFI, w.state == WIFI_CONNECTED ? kOrange : kDim, w.state >= WIFI_DISCONNECTED, A_NAV, S_SET_WIFI },
        { IC_BT, btOn ? kOrange : kDim, true, A_NAV, S_PHONE },
        { IC_NAV, GpsHasFix() ? kOrange : kMuted, true, A_NONE, 0 } };
    int x = end;
    for (int i = 0; i < 5; i++) {
        if (!m[i].shown)
            continue;
        if (x - 36 < minLeft)
            break;
        x -= 36;
        bool d = m[i].a != A_NONE && Zone(x - 4, 4, 38, 48, m[i].a, m[i].arg);
        if (d)
            R(dc, x - 4, 4, 38, 48, 10, kOrange);
        Ico(dc, m[i].ic, x + 1, 14, 28, d ? kInk : m[i].c);
    }
    return x;
}

static void Frame(HDC dc, const wchar_t* title, Action back = A_BACK, Icon act = IC_NONE, Action a = A_NONE,
                  bool sel = false, Icon act2 = IC_NONE, Action a2 = A_NONE, int arg2 = 0)
{
    Sep(dc, 0, 54, 800);
    int vol, mute, src, marksEnd;
    wchar_t s[40];
    {   // the volume (MCU, through the proxy): a speaker and the level, "–" before the first value; opens its window
        bool have = VolGet(&vol, &mute, &src);
        if (!have)
            wcscpy(s, L"–");
        else if (mute)
            wcscpy(s, L"×");
        else
            _snwprintf(s, 8, L"%d", vol);
        int w = TextWidth(dc, s, 18, 600);
        int x0 = 700 - w - 30;
        bool dv = Zone(x0 - 4, 4, w + 38, 48, A_SCREEN, S_VOLUME);
        if (dv)
            R(dc, x0 - 4, 4, w + 38, 48, 10, kOrange);
        COLORREF c = dv ? kInk : have && !mute ? kMuted : kDim;
        Ico(dc, IC_VOL, x0, 16, 24, c);
        T(dc, 700, 36, s, 18, c, 600, AL_END);
        marksEnd = x0 - 8;
    }
    int marksLeft = StatusMarks(dc, marksEnd, act != IC_NONE ? 540 : 0);
    Now(s, 8);
    T(dc, 784, 38, s, 26, kText, 600, AL_END);   // the clock: no action (the volume opens from the speaker)
    int x = 20, right = act != IC_NONE ? 470 : 530;
    if (right > marksLeft - 12)
        right = marksLeft - 12;
    if (act != IC_NONE)
        Button(dc, 478, 6, 56, 44, NULL, sel ? B_SEL : B_N, act, 18, 28, a);
    if (act2 != IC_NONE) {   // a second one to its left
        Button(dc, 418, 6, 56, 44, NULL, B_N, act2, 18, 28, a2, arg2);
        right = 410;
    }
    if (CamChip(s, 40)) {   // a speed camera ahead, on every screen
        int cx = right - 156;
        R(dc, cx, 8, 152, 40, 10, kOrange);
        T(dc, cx + 76, 34, s, 14, kInk, 700, AL_MID, 144);
        right = cx - 8;
    } else if (s_hvacShowTick && GetTickCount() - s_hvacShowTick < 4000 && HvacChip(s, 40)) {
        int cx = right - 196;   // the climate was changed: what it is now, like the stock shell's tip window
        R(dc, cx, 8, 192, 40, 10, kPanel2, kOrange);
        T(dc, cx + 96, 34, s, 14, kText, 700, AL_MID, 184);
        right = cx - 8;
    }
    if (back != A_NONE && (back != A_BACK || s_histLen > 0)) {
        bool d = Zone(0, 0, 64, 56, back);
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
    case S_AUDIO: case S_NETRADIO: case S_AUDIO_BT: case S_AUX: case S_VIDEO: case S_PHOTO:
        return 2;
    case S_PHONE:
        return 3;
    case S_APPS:
        return 4;
    case S_SETTINGS: case S_SET_SOUND: case S_SET_BALANCE: case S_SET_SCREEN: case S_SET_RADIO: case S_SET_BT:
    case S_SET_WIFI: case S_SET_VIDEO: case S_SET_PHOTO: case S_INFO: case S_SET_CAR: case S_SET_VOLUME:
    case S_SET_OBD: case S_BT_PAIRED:
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
static void PageDots(HDC dc, int page, int pages, int cx = 400, int y = 398)
{
    if (pages < 2)
        return;
    Zone(cx - 120, y - 6, 240, 16, A_PAGE);
    int w = 28 + (pages - 1) * 28, dx = cx - w / 2;
    for (int p = 0; p < pages; p++) {
        int dw = p == page ? 28 : 20;
        R(dc, dx, y, dw, 6, 3, p == page ? kOrange : kDim);
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
                                  { IC_BT, L"BLUETOOTH", S_SET_BT }, { IC_WIFI, L"WI-FI", S_SET_WIFI },
                                  { IC_VIDEO, L"ВИДЕО", S_SET_VIDEO }, { IC_PHOTO, L"ФОТО", S_SET_PHOTO },
                                  { IC_CAR, L"АВТОМОБИЛЬ", S_SET_CAR }, { IC_INFO, L"О СИСТЕМЕ", S_INFO } };
    Frame(dc, L"НАСТРОЙКИ");
    for (int i = 0; i < 10; i++) {
        int x = 16 + (i % 2) * 110, y = 64 + (i / 2) * 68;
        bool on = items[i].to == active || (active == S_SET_BALANCE && items[i].to == S_SET_SOUND) ||
                  ((active == S_SET_OBD || active == S_BT_PAIRED) && items[i].to == S_SET_BT);
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

static void Cover(HDC dc, Icon ic, int x, int y, int s)
{
    TriFill(x, y, s, s, 14);
    R(dc, x, y, s, s, 14, CLR_INVALID, kLine);
    int isz = s * 100 / 236;
    Ico(dc, ic, x + (s - isz) / 2, y + (s - isz) / 2, isz, Mix(kInk, kTriOr[1], 140));
}

// Sources of the media main screens as tabs on top (music from USB, internet radio, Bluetooth, AUX, video,
// photo), then up to two buttons of the source: its own (list, player, slides) and its settings.
static void MediaTabs(HDC dc, Screen active, Icon ic, Action a, int arg, Screen settings)
{
    static const Tab tabs[6] = { { IC_MUSIC, L"МУЗЫКА", S_AUDIO }, { IC_RADIO, L"ИНТЕРНЕТ", S_NETRADIO },
                                 { IC_BT, L"BLUETOOTH", S_AUDIO_BT }, { IC_AUX, L"AUX", S_AUX },
                                 { IC_VIDEO, L"ВИДЕО", S_VIDEO }, { IC_PHOTO, L"ФОТО", S_PHOTO } };
    for (int i = 0; i < 6; i++) {
        bool on = tabs[i].to == active;
        Button(dc, 16 + i * 99, 64, 95, 48, tabs[i].label, on ? B_SEL : B_N, IC_NONE, 14, 0, on ? A_NONE : A_TAB,
               tabs[i].to);
    }
    if (ic != IC_NONE)
        Button(dc, 616, 64, 80, 48, NULL, B_N, ic, 18, 30, a, arg);
    Button(dc, 704, 64, 80, 48, NULL, B_N, IC_GEAR, 18, 30, A_SCREEN, settings);
}

// ---- the shell's Bluetooth through our proxy of FLC_BLUETOOTH_SDK.dll (SPIPower/bt, btshm.h): its state as the
// stack reports it, and commands the proxy carries out in the shell's process. Without the proxy the mapping is
// ours and empty (magic 0): the screens say so.
static BtShm* s_bt;

static BtShm* BtShmGet()
{
    if (!s_bt) {
        HANDLE m = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(BtShm), BT_SHM_NAME);
        if (m)
            s_bt = (BtShm*)MapViewOfFile(m, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(BtShm));
    }
    return s_bt;
}

// The state; false = no proxy (or its original SDK did not load). seq: the state's version, for "changed?".
static bool BtGet(BtState* st, LONG* seq)
{
    BtShm* b = BtShmGet();
    memset(st, 0, sizeof(*st));
    st->av = st->avrcp = st->hf = st->hfAudio = st->battery = st->signal = -1;
    if (!b || b->magic != BT_SHM_MAGIC || !b->origOk)
        return false;
    for (int tries = 0; tries < 10; tries++) {
        LONG a = b->seq;
        if (a & 1) {
            Sleep(1);
            continue;
        }
        *st = b->st;
        if (b->seq == a) {
            if (seq)
                *seq = a + b->evSeq;
            return true;
        }
    }
    return true;   // a copy taken while it changed: good enough for a screen
}

static void BtCmd(int cmd)
{
    BtShm* b = BtShmGet();
    if (!b || b->magic != BT_SHM_MAGIC)
        return;
    b->cmd = cmd;
    InterlockedIncrement((LONG*)&b->cmdSeq);
    static HANDLE s_ev;
    if (!s_ev)
        s_ev = CreateEvent(NULL, FALSE, FALSE, BT_CMD_EVENT);
    if (s_ev)
        SetEvent(s_ev);
}

static void BtText(const char* u8, wchar_t* w, int cap)   // the stack's texts are UTF-8
{
    if (!MultiByteToWideChar(CP_UTF8, 0, u8, -1, w, cap))
        MultiByteToWideChar(CP_ACP, 0, u8, -1, w, cap);
    w[cap - 1] = 0;
}

// ---- screens

enum { RSET_NOISE, RSET_MW, RSET_RDS, RSET_AF, RSET_CT, RSET_TA, RSET_MS, RSET_COUNT };

// The tuner as the MCU reports it (mcu.log 2026-10-16/17, PROJECT.md): 0x0901 00 00 <freq BE, 10 kHz>; 0x0908 00 01
// <from> <to> <count> + 8 frequencies (presets in blocks of 8: FM1..FM3 = 24, the low band "УКВ" from 65 MHz = 8);
// 0x0907 00 01 <the preset playing, FF none>; 0x090B 00 00 00 + RDS PS (8 chars); 0x0931 00 00 00 <part> + RadioText.
// More (SPIPower.log 2026-10-18-2): 0x0901 00 <band: 0 FM, 1 УКВ> <freq>; the station list after the list key 0x3C -
// 0x0908 00 00 <from> <to> <count> + frequencies (up to 30) and 0x0930 00 <from> <to> <count> + 4 names x 8 chars;
// 0x0904 00 <flag> <0/1> (all of them after 0x30; flag 01 is set when a search starts).
enum { kTunList = 30 };
struct Tuner {
    int     f10;              // MHz * 100, 0 = not seen yet
    int     band;             // 0 FM, 1 УКВ, -1 not seen
    DWORD   fTick;            // when the frequency last changed (a search runs while it keeps changing)
    int     list[kTunList], listCount;   // the station list (after 0x3C), MHz * 100
    wchar_t listName[kTunList][9];
    int     flags[8];         // 0x0904
    int     preset[24];       // the same units, 0 = empty
    int     presets;          // how many the band has (24 FM, 8 УКВ), 0 = not seen
    int     cur;              // the preset playing, -1 none
    wchar_t ps[9], rt[2][33];
    int     set[7];           // the radio's settings (RSET_*): 0 / 1, as the MCU reports them
    int     setKnown;         // which of them came (bit 1 << RSET_*)
};
static void TunerGet(Tuner* t);
static const wchar_t* RadioName(const Tuner& t, int f10);   // a station's name, "" when not known
static void McuKey(int code, bool ext);
static void RadioPreset(int i);
static bool McuSend3(const char* name, int a, int b, int c);
static bool VolGet(int* vol, int* mute, int* src);

static void FreqText(int f10, wchar_t* s, int n)   // 9840 -> "98.4", 10625 -> "106.25"; СВ: kHz, 1053 -> "1053"
{
    if (f10 <= 0)
        wcscpy(s, L"—");
    else if (f10 < 3000)   // the MW band's kHz (522..1710)
        _snwprintf(s, n, L"%d", f10);
    else if (f10 % 10)
        _snwprintf(s, n, L"%d.%02d", f10 / 100, f10 % 100);
    else
        _snwprintf(s, n, L"%d.%d", f10 / 100, f10 % 100 / 10);
}

static void RadioDock(HDC dc, bool listOn)
{
    Dock(dc);
    IconBtn(dc, IC_PREV, 268, 417, 44, kText, A_RKEY, 0x13B);
    IconBtn(dc, IC_FWD, 488, 417, 44, kText, A_RKEY, 0x13C);
    if (listOn) {   // the list is open: its button is selected and closes it
        bool d = Zone(590, 410, 64, 58, A_BACK);
        R(dc, 590, 410, 64, 58, 10, d ? kOrange : kPanel2);
        Ico(dc, IC_LIST, 600, 417, 44, d ? kInk : kOrange);
    } else {
        IconBtn(dc, IC_LIST, 600, 417, 44, kText, A_SCREEN, S_RADIO_LIST);
    }
    IconBtn(dc, IC_GEAR, 712, 417, 44, kText, A_SCREEN, S_SET_RADIO);
}

// Radio, a main screen, on the MCU's tuner (s_tun): the frequency, RDS name and text, the presets. Four pages of
// 8 presets as the shell's band button steps them: FM1..FM3 (FM's 24 presets, local pages: the MCU sends all 24 at
// once and does not care) and УКВ (its band: switched in the MCU, RadioBand). One button named after the page
// steps them round (as the shell's), a swipe steps to the next / previous, dots under the presets show where.
// A preset tap tunes it (SPApi_SendListItemControl(1, n)), a long press saves the station there ((1, n, 1), the
// shell's own, MCU 0x0909); seek - the extended keys. Opening the screen with nothing known asks the MCU (0x30,
// as the shell's screen does).
static void OursQuiet();
static void BtQuiet();
static void MediaSource();

// The tuner's state comes only on a change or when asked (key 0x30). Right after the unit starts the MCU may not
// answer for 20-30 s (HU 2026-10-20: the radio key at 187 s - nothing, the same at 207 s - the tuner at once).
// The shell gets it at once at its start: SendRadioNaviVoiceGuidance(0) + SendRadioIsNaviSourceCouldIn(0), and
// the MCU sends the whole tuner (SPIPower.log 2026-10-20-2, 36.3 s) - so the same first; then, while the radio
// is wanted and the tuner has not spoken in this run, 0x30 again every 3 s, for a minute.
static volatile bool s_tunLive;   // a 0x0901 came since the start
static volatile LONG s_tun0901;   // 0x0901 replies so far: one a band key press (RadioBandStep)
static DWORD s_radioAsk;          // the radio wanted (RadioOn, its screen), 0 = not

// The source switches and how they are checked (HU 2026-10-20-3): the radio's keys 0x30 + 0x7F switched to the
// radio only sometimes - a preset (SendListItemControl) always does; SendMediaSourceSelect(5, 3) is acked (0x9101
// 00 05 06) but the MCU goes "media" only when the shell's own player plays - while the Bluetooth source (09, keys
// 0x84 + 0x54) is the CE's own audio path too: our waveOut sounds with it (BT -> our music worked). So ours take
// that path; the radio's keys are checked a moment later and a preset taken if they did nothing.
static DWORD s_srcByUs;      // we switched the source: the watcher (MODE, the shell) leaves our players alone
static DWORD s_radioSrcAsk;  // RadioOn's keys sent (checked 1.5 s later)
static DWORD s_btPathTick;   // the Bluetooth path taken for our player: the phone paused once it is up

// СВ (MW, 2026-10-20): the shell's AM key 0x3A, band 2 in 0x0901 with the frequency in kHz (522, 531, ... a 9 kHz
// step), 8 presets; 0x3B from it goes back to FM1. The band takes the "СВ" radio setting (0x092E) to be on.
enum { kRadioPages = 5, RP_UKV = 3, RP_MW = 4 };

static int PageBand(int page)   // the MCU's band of a page: 0 FM, 1 УКВ, 2 СВ
{
    return page == RP_UKV ? 1 : page == RP_MW ? 2 : 0;
}

static int BandPage(int band)   // the first page of a band
{
    return band == 1 ? RP_UKV : band == 2 ? RP_MW : 0;
}
static int s_radioPage;          // 0..2 FM1..FM3, 3 УКВ
static int s_bandWant = -1;      // RadioBand: the band asked for, -1 none
static int s_bandPresses;
static DWORD s_bandTick;
static LONG s_bandSeq;           // s_tun0901 at the last press
static bool s_bandFailed;        // given up with the band not the page's: the page goes back to the band's
static void RadioBand(int want);

// A band switch under way (the MCU not answering for 8 s: given up).
static bool BandPending()
{
    if (s_bandWant >= 0 && GetTickCount() - s_bandTick > 8000) {
        s_bandWant = -1;
        s_bandFailed = true;
    }
    return s_bandWant >= 0;
}

// The radio's page to, always (the page is the user's): FM's pages locally; the MCU's band follows the page -
// switched when it differs, the target changed (or the switch dropped) when one is still under way.
static void RadioPageTo(int page)
{
    Tuner t;
    TunerGet(&t);
    page = (page + kRadioPages) % kRadioPages;
    int want = PageBand(page);
    if (BandPending()) {
        if (want != s_bandWant) {
            if (t.band == want)
                s_bandWant = -1;   // left before the band changed: nothing to switch back
            else
                RadioBand(want);
        }
    } else if (t.band >= 0 && t.band != want) {
        RadioBand(want);
    }
    s_radioPage = page;
}

static void ScrRadio(HDC dc)
{
    Tuner t;
    TunerGet(&t);
    static DWORD s_seen;
    if (s_seen != s_screenTick) {   // opened: the page of the preset playing; nothing known - ask
        s_seen = s_screenTick;
        if (t.band == 1 || t.band == 2)
            s_radioPage = BandPage(t.band);
        else if (t.cur >= 0 && t.cur < 24)
            s_radioPage = t.cur / 8;
        if (!t.f10 || !t.presets || !s_tunLive) {
            McuKey(0x30, false);
            if (!s_tunLive)
                s_radioAsk = GetTickCount();
        }
    }
    wchar_t f[16], band[8];
    FreqText(t.f10, f, 16);
    bool low = t.band == 1 || (t.band < 0 && t.f10 >= 6500 && t.f10 < 8750);   // the OIRT band, 65..74 MHz
    bool mw = t.band == 2 || (t.band < 0 && t.f10 > 0 && t.f10 < 3000);
    bool pending = BandPending();
    if (s_bandFailed && !pending) {   // the MCU did not switch: the page of the band it is in
        s_bandFailed = false;
        if (t.band >= 0 && t.band != PageBand(s_radioPage)) {
            s_radioPage = BandPage(t.band);
            wcscpy(s_status, L"диапазон не переключился - ещё раз");
        }
    }
    static int s_lastBand = -2;
    if (t.band != s_lastBand) {   // the band changed elsewhere (the shell): its page
        if (!pending && s_lastBand != -2 && t.band >= 0 && t.band != PageBand(s_radioPage))
            s_radioPage = BandPage(t.band);
        s_lastBand = t.band;
    }
    int pb = PageBand(s_radioPage);
    bool ready = pb == (mw ? 2 : low ? 1 : 0);   // the MCU is in the page's band (not while it switches)
    int page = pb ? 0 : s_radioPage;              // the presets' page in the band
    if (pb)
        wcscpy(band, pb == 1 ? L"УКВ" : L"СВ");
    else
        _snwprintf(band, 8, L"FM%d", page + 1);
    const wchar_t* name = t.ps[0] ? t.ps : RadioName(t, t.f10);
    Frame(dc, L"РАДИО");
    Panel(dc, 16, 64, 768, 128);
    T(dc, 40, 146, f, 80, t.f10 ? kText : kDim, 700, AL_START, 250);
    T(dc, 300, 100, t.f10 ? band : L"нет данных — включаю радио…", 20, t.f10 ? kOrange : kDim, 700, AL_START, 460);
    Marquee(dc, 300, 130, 460, name, 28, kText, 600);
    wchar_t rt[68];
    _snwprintf(rt, 68, L"%s%s", t.rt[0], t.rt[1]);
    rt[67] = 0;
    Marquee(dc, 300, 156, 460, rt, 17, kMuted, 400);
    if (s_marqOn)
        SetTimer(s_wnd, TIMER_MARQ, kMarqMs, NULL);   // (again: the same timer, restarted)
    int lo = mw ? 522 : low ? 6500 : 8750, hi = mw ? 1620 : low ? 7400 : 10800;
    for (int i = 0; i <= 20; i++) {   // tuning scale over x 40..740
        int x = 40 + i * 35, h = i % 2 ? 5 : 8;
        Fill(dc, x - 1, 182 - h, x + 1, 182, kDim);
    }
    if (t.f10 >= lo && t.f10 <= hi) {
        int mark = 40 + (t.f10 - lo) * 700 / (hi - lo);
        R(dc, mark - 2, 168, 4, 18, 2, kOrange);
    }
    Button(dc, 16, 200, 80, 52, NULL, B_N, IC_PREV, 18, 30, A_RKEY, 0x13B);    // seek down
    Button(dc, 104, 200, 80, 52, NULL, B_N, IC_FWD, 18, 30, A_RKEY, 0x13C);    // seek up
    wchar_t btn[12];
    _snwprintf(btn, 12, L"%s%s", band, pending ? L"…" : L"");
    Button(dc, 200, 200, 120, 52, btn, B_N, IC_NONE, 20, 0, A_RPAGE);   // the next page
    Button(dc, 600, 200, 88, 52, NULL, B_N, IC_LIST, 18, 32, A_SCREEN, S_RADIO_LIST);
    Button(dc, 696, 200, 88, 52, NULL, B_N, IC_GEAR, 18, 32, A_SCREEN, S_SET_RADIO);
    for (int i = 0; i < 8; i++) {   // the page's presets, 4 x 2: the frequency and the name
        int x = 16 + (i % 4) * 194, y = 262 + (i / 4) * 66, n = page * 8 + i;
        int pf = n < 24 && ready ? t.preset[n] : 0;
        bool have = t.presets > 0 && pf > 0;
        bool down = Zone(x, y, 182, 60, t.presets > 0 ? A_PRESET : A_NONE, n);
        bool cur = down || (have && n == t.cur);
        wchar_t num[4], pfs[16];
        _snwprintf(num, 4, L"%d", i + 1);
        FreqText(pf, pfs, 16);
        R(dc, x, y, 182, 60, 12, cur ? kOrange : kPanel, cur ? CLR_INVALID : kLine);
        T(dc, x + 166, y + 21, num, 13, cur ? kInk : kDim, cur ? 700 : 600, AL_END);
        T(dc, x + 14, y + 28, have ? pfs : L"—", 22, cur ? kInk : have ? kText : kDim, 700);
        const wchar_t* nm = have ? RadioName(t, pf) : L"";
        T(dc, x + 14, y + 51, nm[0] ? nm : have ? L"" : L"удерживать — сохранить", 14, cur ? kInk : kMuted, 600,
          AL_START, 156);
    }
    PageDots(dc, s_radioPage, kRadioPages);
}

// The band: the MCU's band key 0x3B steps FM1 -> FM2 -> FM3 -> УКВ (СВ -> FM1), and only the band shows in its
// replies (0x0901's band): pressed again on each reply until the band is the one wanted, at most 4 times; СВ is
// its own key 0x3A.
static void RadioBand(int want)
{
    s_bandWant = want;
    s_bandPresses = 1;
    s_bandTick = GetTickCount();
    s_bandSeq = s_tun0901;
    s_bandFailed = false;
    Tuner t;
    TunerGet(&t);
    if (want == 2 && (t.setKnown >> RSET_MW & 1) && !t.set[RSET_MW])
        McuKey(0xA9, true);   // СВ off in the MCU's settings: on first (kRsetKey[RSET_MW][1]; no switch of ours)
    McuKey(want == 2 ? 0x3A : 0x3B, false);
}

static void RadioBandStep(const Tuner& t)   // a tuner reply came
{
    if (s_bandWant < 0)
        return;
    if (s_tun0901 == s_bandSeq)
        return;   // the press's 0x0901 not here yet (the other replies of a press do not count)
    s_bandSeq = s_tun0901;
    if (t.band == s_bandWant) {
        s_bandWant = -1;
        return;
    }
    if (s_bandPresses >= 4 || GetTickCount() - s_bandTick > 8000) {
        s_bandWant = -1;
        s_bandFailed = true;
        return;
    }
    s_bandPresses++;
    s_bandTick = GetTickCount();
    McuKey(s_bandWant == 2 ? 0x3A : 0x3B, false);
}

static void RadioPresetSave(int i)   // a long press: the station playing goes to preset i (the shell's (1, n, 1))
{
    if (McuSend3("SPApi_SendListItemControl", 1, i, 1))
        _snwprintf(s_status, 96, L"сохранено в пресет %d", i % 8 + 1);
}

// A preset of the radio screen or the widget: as CRadioMainDlg::SlotListItemSelect (Ghidra 2026-10-18).
static void RadioPreset(int i)
{
    McuSend3("SPApi_SendListItemControl", 1, i, 0);
}

// A key to the MCU as the stock shell sends it: pressed, released; ext = SPApi_SendExtendKeyEvent.
static void McuKey(int code, bool ext)
{
    const char* fn = ext ? "SPApi_SendExtendKeyEvent" : "SPApi_SendKeyEvent";
    if (McuSend3(fn, code, 0, 0))
        McuSend3(fn, code, 1, 1);
}


// The radio as the MCU's source: the stock shell's main menu "Radio" is key 0x7F. The shell sends 0x30 before
// every source change (SPIPower.log 2026-10-18); without it our 0x7F was ignored at first - so the same.
// One source at a time: our players and the phone's music stop.
static void RadioOn()
{
    OursQuiet();
    BtQuiet();
    if (!s_tunLive)
        s_radioAsk = GetTickCount();
    int vol, mute, src;
    VolGet(&vol, &mute, &src);
    if (src == 0)
        return;   // already the radio
    McuKey(0x30, false);
    McuKey(0x7F, false);
    s_srcByUs = s_radioSrcAsk = GetTickCount();
}

// The radio screen's keys as the shell's (SPIPower.log 2026-10-18): seek down / up = extended 0x3B / 0x3C,
// the band FM1 -> FM2 -> FM3 -> УКВ = 0x3B, the search (scan; again = stop) = 0x3F. arg: code | 0x100 = extended.
static void RadioKey(int arg)
{
    McuKey(arg & 0xFF, (arg & 0x100) != 0);
}

// Radio -> list: the MCU's station list. Opening it is the shell's list key 0x3C - the MCU then sends the stations
// with their names, and only in this mode does the search key 0x3F work (HU 2026-10-18-2). A tap tunes the row
// (SPApi_SendListItemControl(0, row)); "Автопоиск" sweeps the band, then the list is asked for again.
static int   s_rlistPage;
static DWORD s_scanStart;   // the search started (0 = none)

static void ScrRadioList(HDC dc)
{
    static DWORD s_seen;
    if (s_seen != s_screenTick) {
        s_seen = s_screenTick;
        McuKey(0x3C, false);
    }
    Tuner t;
    TunerGet(&t);
    Frame(dc, L"РАДИО · СПИСОК");
    Panel(dc, 16, 68, 600, 322);
    int n = t.listCount < kTunList ? t.listCount : kTunList, pages = (n + 4) / 5;
    if (s_rlistPage >= pages)
        s_rlistPage = pages ? pages - 1 : 0;
    bool scanning = s_scanStart != 0;
    if (scanning) {   // the search: the frequency running, dots moving
        wchar_t f[16], s[40];
        FreqText(t.f10, f, 16);
        static const wchar_t* dots[4] = { L"", L".", L"..", L"..." };
        _snwprintf(s, 40, L"Ищу станции%s", dots[GetTickCount() / 400 % 4]);
        T(dc, 40, 180, s, 26, kText, 700);
        T(dc, 40, 250, f, 60, kOrange, 700);
        int x = 40 + (t.f10 > 8750 && t.f10 <= 10800 ? (t.f10 - 8750) * 540 / 2050 : 0);
        R(dc, 40, 290, 540, 6, 3, kLine);
        R(dc, x, 286, 14, 14, 7, kOrange);
    } else if (!n) {
        T(dc, 40, 180, L"Список пуст или ещё не пришёл.", 20, kMuted, 600, AL_START, 560);
        T(dc, 40, 214, L"«АВТОПОИСК» найдёт станции заново.", 17, kDim, 400, AL_START, 560);
    }
    for (int r = 0; r < 5 && !scanning; r++) {
        int i = s_rlistPage * 5 + r;
        if (i >= n)
            break;
        int y = 76 + r * 62;
        bool cur = t.list[i] == t.f10;
        bool d = Zone(24, y, 568, 56, A_RLIST, i);
        R(dc, 24, y, 568, 56, 10, d || cur ? kOrange : kPanel);
        wchar_t f[16];
        FreqText(t.list[i], f, 16);
        T(dc, 40, y + 38, f, 26, d || cur ? kInk : kText, 700);
        const wchar_t* nm = t.listName[i][0] ? t.listName[i] : RadioName(t, t.list[i]);
        T(dc, 180, y + 36, nm, 19, d || cur ? kInk : kMuted, 700, AL_START, 400);
    }
    if (pages > 1 && !scanning) {
        Button(dc, 632, 232, 72, 48, NULL, s_rlistPage ? B_N : B_DIS, IC_BACK, 18, 28, A_RLISTPAGE, -1);
        Button(dc, 712, 232, 72, 48, NULL, s_rlistPage + 1 < pages ? B_N : B_DIS, IC_NEXT, 18, 28, A_RLISTPAGE, 1);
    }
    Button(dc, 632, 68, 152, 72, scanning ? L"СТОП" : L"АВТОПОИСК", scanning ? B_SEL : B_N, IC_SEARCH, 15, 0, A_RSCAN);
    R(dc, 632, 292, 152, 98, 12, kPanel);
    wchar_t c[8];
    _snwprintf(c, 8, L"%d", n);
    T(dc, 708, 322, L"НАЙДЕНО", 13, kMuted, 600, AL_MID);
    T(dc, 708, 362, c, 36, kText, 700, AL_MID);
    RadioDock(dc, true);
}

static void RadioScan()   // the list's "Автопоиск" (again = stop, the same key)
{
    s_scanStart = s_scanStart ? 0 : GetTickCount();
    McuKey(0x3F, false);
}

// Every second on the list: the search is over once the frequency stood still 2 s (and 3 s passed) - the list again.
static void RadioScanTick()
{
    if (!s_scanStart)
        return;
    Tuner t;
    TunerGet(&t);
    DWORD now = GetTickCount();
    if (now - s_scanStart > 3000 && now - t.fTick > 2000) {
        s_scanStart = 0;
        s_rlistPage = 0;
        McuKey(0x3C, false);
    }
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

// Media -> music and its list: the player (BASS), defined with it below.
static void ScrAudio(HDC dc);
static void ScrAudioList(HDC dc);

// Bluetooth music: the stock shell's Bluetooth stack plays it (FLC SDK in G3NavHMI). The MCU's source (0x4904 =
// 09) and switching to it with the shell's key 0x54 (after 0x30, SPIPower.log 2026-10-18); with our proxy of the
// SDK (btshm.h) also the phone, the track (AVRCP) and the buttons (the SDK's own Av_* calls, in the shell).
enum { TIMER_BTSRC = 12 };
static int s_btHold;   // ⏮ / ⏭ held: the release command to send when the finger goes up

static void BtMusicOn()   // the shell's "Phone" key 0x84 (the MCU answers 0x8905), then its BT music key 0x54
{                         // (HU 2026-10-18-2: 0x54 alone did nothing; after 0x84 the source turned 09)
    OursQuiet();
    s_srcByUs = GetTickCount();
    McuKey(0x84, false);
    SetTimer(s_wnd, TIMER_BTSRC, 800, NULL);
}

static void ScrAudioBt(HDC dc)
{
    Frame(dc, L"МЕДИА · BLUETOOTH");
    MediaTabs(dc, S_AUDIO_BT, IC_PHONE, A_SCREEN, S_PHONE, S_SET_BT);
    Cover(dc, IC_BT, 16, 124, 232);
    int vol, mute, src;
    VolGet(&vol, &mute, &src);
    bool on = src == 9;
    BtState bt;
    bool proxy = BtGet(&bt, NULL);
    static DWORD s_asked;   // the track and the play state, asked for on opening (the shell does the same)
    if (proxy && bt.av >= 1 && (!s_asked || GetTickCount() - s_asked > 10000)) {
        s_asked = GetTickCount();
        BtCmd(BT_CMD_MEDIAINFO);
    }
    static DWORD s_seen;   // opened with the phone connected: its music becomes the source at once
    if (s_seen != s_screenTick) {
        s_seen = s_screenTick;
        if (proxy && bt.av >= 1 && !on)
            BtMusicOn();
    }
    wchar_t cap[160], title[BT_TEXT], artist[BT_TEXT], album[BT_TEXT], line[300];
    if (proxy && bt.devName[0]) {
        BtText(bt.devName, title, BT_TEXT);
        _snwprintf(cap, 160, L"BLUETOOTH · %s", title);
        CharUpperBuff(cap, (DWORD)wcslen(cap));
    } else {
        wcscpy(cap, L"BLUETOOTH-МУЗЫКА");
    }
    T(dc, 272, 150, cap, 14, kMuted, 600, AL_START, 512);
    if (proxy && bt.title[0]) {   // the track, as the phone tells it (AVRCP)
        BtText(bt.title, title, BT_TEXT);
        BtText(bt.artist, artist, BT_TEXT);
        BtText(bt.album, album, BT_TEXT);
        T(dc, 272, 192, title, 30, kText, 700, AL_START, 512);
        if (artist[0] && album[0])
            _snwprintf(line, 300, L"%s · %s", artist, album);
        else
            wcscpy(line, artist[0] ? artist : album);
        T(dc, 272, 222, line, 18, kMuted, 400, AL_START, 512);
        int len = bt.lengthMs > 0 ? bt.lengthMs : bt.durationMs;
        if (len > 0) {   // the bar: the position the stack reports every 2 s while playing, moved on in between
            int pos = bt.posMs;
            if (bt.playing == 1 && bt.playTick)
                pos += (int)(GetTickCount() - bt.playTick);
            pos = pos < 0 ? 0 : pos > len ? len : pos;
            int px = 512 * (pos / 1000) / (len / 1000 > 0 ? len / 1000 : 1);
            R(dc, 272, 252, 512, 8, 4, kLine);
            R(dc, 272, 252, px, 8, 4, kOrange);
            Disc(dc, 272 + px, 256, 10, kText);   // the knob; ⏮ / ⏭ held - rewind / fast forward
            _snwprintf(line, 32, L"%d:%02d", pos / 60000, pos / 1000 % 60);
            T(dc, 272, 284, line, 17, kMuted, 600);
            _snwprintf(line, 32, L"%d:%02d", len / 60000, len / 1000 % 60);
            T(dc, 784, 284, line, 17, kMuted, 600, AL_END);
        }
    } else {
        const wchar_t* head = on ? L"Играет с телефона" : L"Сейчас другой источник";
        if (proxy && bt.av == 0)
            head = L"Телефон не подключён";
        T(dc, 272, 192, head, 30, on ? kText : kMuted, 700, AL_START, 512);
        T(dc, 272, 222, on ? L"Звук идёт с подключённого телефона." : L"Телефон должен быть подключён по Bluetooth.",
          18, kMuted, 400, AL_START, 512);
    }
    if (!on)   // not the source (the phone not connected, or the switch did not take): by hand
        Button(dc, 584, 124, 200, 40, L"ВКЛЮЧИТЬ ЗВУК", B_N, IC_BT, 14, 20, A_BTMUSIC);
    bool ctl = proxy && bt.av >= 1;   // the buttons: the SDK's AVRCP keys, through the proxy
    bool playing = bt.playing ? bt.playing == 1 : bt.av == 2;
    Button(dc, 392, 312, 64, 56, NULL, ctl ? B_N : B_DIS, IC_PREV, 18, 40, A_BTCMD, BT_CMD_PREV);
    Button(dc, 496, 312, 64, 56, NULL, ctl ? B_N : B_DIS, playing ? IC_PAUSE : IC_PLAY, 18, 40, A_BTCMD,
           BT_CMD_PLAYPAUSE);
    Button(dc, 600, 312, 64, 56, NULL, ctl ? B_N : B_DIS, IC_FWD, 18, 40, A_BTCMD, BT_CMD_NEXT);
    if (!proxy) {
        T(dc, 272, 384, L"Название трека и кнопки — с proxy Bluetooth (кнопка 2 установщика).", 14, kDim, 400,
          AL_START, 512);
        T(dc, 272, 402, L"Пока — с телефона или кнопками руля.", 14, kDim, 400, AL_START, 512);
    }
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

static void ScrPhoto(HDC dc);       // (with the photos' worker, below)
static void ScrPhotoView(HDC dc);

// Phone, a main screen: the number, recent calls and the way to the contacts on the left, the keypad on the right.
// Phone: the stock shell's Bluetooth stack through our proxy (btshm.h): the connected phone, its operator, battery
// and network; the keypad dials (Flc_Bluetooth_Hf_DialNumber), an incoming call opens its window by itself (the
// 1 s timer), answer / end - the SDK's Hf_Answer / Hf_End. Pairing stays in the shell's own screens for now; the
// phone book and the call log (PBAP) are the next step.
static void BtCmd(int cmd);
static void BtText(const char* u8, wchar_t* w, int cap);

static void BtCmdText(int cmd, const wchar_t* text)
{
    BtShm* b = BtShmGet();
    if (!b || b->magic != BT_SHM_MAGIC)
        return;
    WideCharToMultiByte(CP_ACP, 0, text, -1, b->cmdText, sizeof(b->cmdText), NULL, NULL);
    b->cmdText[sizeof(b->cmdText) - 1] = 0;
    BtCmd(cmd);
}

static void ScrPhone(HDC dc)
{
    static const wchar_t* keys = L"123456789*0#";
    BtState bt;
    bool proxy = BtGet(&bt, NULL);
    bool on = proxy && bt.hf == 2;
    wchar_t title[160], name[BT_TEXT], s[120];
    if (on && bt.devName[0]) {
        BtText(bt.devName, name, BT_TEXT);
        _snwprintf(title, 160, L"ТЕЛЕФОН · %s", name);
    } else {
        wcscpy(title, L"ТЕЛЕФОН");
    }
    Frame(dc, title);
    Panel(dc, 16, 64, 356, 72, 12);
    int end = T(dc, 32, 112, s_dial, 28, kText, 700, AL_START, 270);
    R(dc, end + 4, 86, 3, 28, 0, kOrange);
    IconBtn(dc, IC_BKSP, 320, 81, 38, kMuted, A_DEL, K_DIAL);
    Panel(dc, 16, 148, 356, 196, 12);
    if (!proxy) {
        T(dc, 32, 190, L"Нет proxy Bluetooth", 20, kText, 700, AL_START, 324);
        T(dc, 32, 220, L"VestaHomeInstall, кнопка 2", 16, kMuted, 400, AL_START, 324);
    } else if (!on) {
        T(dc, 32, 190, L"Телефон не подключён", 20, kText, 700, AL_START, 324);
        T(dc, 32, 220, L"Подключение пока — в штатной", 16, kMuted, 400, AL_START, 324);
        T(dc, 32, 242, L"оболочке: Телефон → Bluetooth", 16, kMuted, 400, AL_START, 324);
    } else {
        T(dc, 32, 190, bt.devName[0] ? name : L"Телефон", 22, kText, 700, AL_START, 324);
        BtText(bt.operatorName, name, BT_TEXT);
        T(dc, 32, 220, name[0] ? name : L"оператор —", 17, kMuted, 600, AL_START, 324);
        s[0] = 0;
        if (bt.battery >= 0)
            _snwprintf(s, 120, L"батарея %d/5", bt.battery);
        if (bt.signal >= 0) {
            wchar_t t[40];
            _snwprintf(t, 40, L"%sсеть %d/5", s[0] ? L" · " : L"", bt.signal);
            wcscat(s, t);
        }
        T(dc, 32, 250, s, 17, kMuted, 400, AL_START, 324);
        T(dc, 32, 300, bt.av >= 1 ? L"музыка: подключена" : L"музыка: нет", 15, kDim, 400, AL_START, 324);
    }
    Button(dc, 16, 354, 172, 44, L"КОНТАКТЫ", B_N, IC_USERS, 15, 24, A_SCREEN, S_CONTACTS);
    Button(dc, 200, 354, 172, 44, L"ЖУРНАЛ", B_N, IC_CLOCK, 15, 24, A_SCREEN, S_CALLS);
    for (int i = 0; i < 12; i++) {   // keypad 3 x 4
        int x = 388 + (i % 3) * 136, y = 64 + (i / 3) * 62;
        wchar_t k[2] = { keys[i], 0 };
        bool d = Zone(x, y, 124, 56, A_KEY, K_DIAL * 65536 + keys[i]);
        R(dc, x, y, 124, 56, 10, d ? kOrange : kPanel, d ? CLR_INVALID : kLine);
        T(dc, x + 62, y + 39, k, 30, d ? kInk : kText, 700, AL_MID);
        if (keys[i] == L'0')   // held: "+"
            T(dc, x + 100, y + 38, L"+", 16, d ? kInk : kMuted, 700, AL_MID);
    }
    bool can = on && s_dial[0];
    bool d = Zone(388, 316, 192, 82, can ? A_PHONECALL : A_NONE);
    R(dc, 388, 316, 192, 82, 12, !can ? kPanel2 : d ? Mix(kOk, kInk, 160) : kOk);
    Ico(dc, IC_CALL, 462, 335, 44, can ? kInk : kDim);
    d = Zone(592, 316, 192, 82, s_dial[0] ? A_PHONECLR : A_NONE);
    R(dc, 592, 316, 192, 82, 12, d ? Mix(kStop, kInk, 160) : kStop);
    Ico(dc, IC_HANGUP, 666, 335, 44, kInk);
}

static void PhoneSoon(HDC dc, const wchar_t* what)   // the phone book and the call log: not yet
{
    Panel(dc, 192, 68, 592, 322);
    T(dc, 220, 140, what, 24, kText, 700, AL_START, 540);
    T(dc, 220, 180, L"Появится со следующим этапом: телефон отдаёт их по Bluetooth", 16, kMuted, 400, AL_START, 540);
    T(dc, 220, 204, L"(PBAP), proxy Bluetooth их уже видит, разбор — впереди.", 16, kMuted, 400, AL_START, 540);
    T(dc, 220, 246, L"Пока — набор номера и звонки работают.", 16, kMuted, 400, AL_START, 540);
}

static void ScrContacts(HDC dc)
{
    Frame(dc, L"ТЕЛЕФОН · КОНТАКТЫ");
    PhoneNav(dc, 1);
    PhoneSoon(dc, L"Телефонная книга");
}

static void ScrContact(HDC dc)
{
    ScrContacts(dc);
}

static void ScrCalls(HDC dc)
{
    Frame(dc, L"ТЕЛЕФОН · ЖУРНАЛ");
    PhoneNav(dc, 3);
    PhoneSoon(dc, L"Журнал звонков");
}

// A call: its number, its state as the stack reports it, the time talking; "end" - Hf_End.
static void ScrCall(HDC dc)
{
    BtState bt;
    BtGet(&bt, NULL);
    int c = bt.call[0];
    const wchar_t* st = c == BT_CALL_INCOMING ? L"ВХОДЯЩИЙ ВЫЗОВ" : c == BT_CALL_DIALING ? L"НАБОР НОМЕРА"
                      : c == BT_CALL_ALERTING ? L"ВЫЗОВ" : c == BT_CALL_ACTIVE ? L"РАЗГОВОР"
                      : c == BT_CALL_HELD ? L"НА УДЕРЖАНИИ" : c == BT_CALL_WAITING ? L"ОЖИДАЕТ" : L"ЗВОНОК";
    wchar_t num[40];
    if (bt.callNumber[0] && c != BT_CALL_DIALING && c != BT_CALL_ALERTING)
        BtText(bt.callNumber, num, 40);
    else
        wcscpy(num, s_callOut[0] ? s_callOut : L"—");
    Frame(dc, L"ТЕЛЕФОН");
    Disc(dc, 128, 168, 88, kPanel2);
    Ico(dc, IC_CALL, 96, 136, 64, c == BT_CALL_ACTIVE ? kOk : kMuted);
    T(dc, 256, 112, st, 14, kMuted, 700);
    T(dc, 256, 168, num, 40, kText, 700, AL_START, 528);
    if (c == BT_CALL_ACTIVE) {
        DWORD sec = (GetTickCount() - s_callTick) / 1000;
        wchar_t s[16];
        _snwprintf(s, 16, L"%02u:%02u", (unsigned)(sec / 60), (unsigned)(sec % 60));
        T(dc, 256, 236, s, 34, kText, 700);
    }
    bool d = Zone(216, 296, 368, 96, A_PHONEEND);
    R(dc, 216, 296, 368, 96, 12, d ? Mix(kStop, kInk, 160) : kStop);
    Ico(dc, IC_HANGUP, 376, 320, 48, kInk);
    Dock(dc);   // in a call the volume is the main control: the MCU's, as it is now
    IconBtn(dc, IC_VOL, 24, 417, 44, kMuted, A_SCREEN, S_VOLUME);
    int vol, mute, src;
    wchar_t v[16] = L"—";
    if (VolGet(&vol, &mute, &src))
        _snwprintf(v, 16, mute ? L"без звука" : L"%d", vol);
    T(dc, 88, 452, v, 22, kText, 700);
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

// Six rows of 48 px: the zone, the clock by hand (hours, minutes), its sync from GPS. The GPS port went to
// "Car" (it is the CAN module's port), "Factory settings" to "About the system".
static void ScrSetGeneral(HDC dc)
{
    static const wchar_t* fmt[2] = { L"12", L"24" };
    enum { H = 48 };
    SYSTEMTIME t;
    GetLocalTime(&t);
    wchar_t now[40], time[8], tz[16], hh[8], mm[8];
    Now(time, 8);
    _snwprintf(now, 40, L"сейчас %02d.%02d.%04d  %s", t.wDay, t.wMonth, t.wYear, time);
    int m = TzMinutes();
    if (m % 60)
        _snwprintf(tz, 16, L"UTC%c%d:%02d", m < 0 ? L'-' : L'+', abs(m) / 60, abs(m) % 60);
    else
        _snwprintf(tz, 16, L"UTC%c%d", m < 0 ? L'-' : L'+', abs(m) / 60);
    _snwprintf(hh, 8, L"%02d", t.wHour);
    _snwprintf(mm, 8, L"%02d", t.wMinute);
    Settings(dc, S_SETTINGS);
    SetRow(dc, 64, L"Часовой пояс", now, false, A_NONE, 0, H);
    Stepper(dc, 512, 68, 248, tz, 40, 19, A_TZ);
    SetRow(dc, 112, L"Часы", NULL, false, A_NONE, 0, H);
    Stepper(dc, 560, 116, 200, hh, 40, 19, A_HOUR);
    SetRow(dc, 160, L"Минуты", NULL, false, A_NONE, 0, H);
    Stepper(dc, 560, 164, 200, mm, 40, 19, A_MINUTE);
    SetRow(dc, 208, L"Синхронизация по GPS", s_on[T_GPS_TIME] ? L"вручную выставленное время поправит фикс" : NULL,
           false, A_NONE, 0, H);
    Toggle(dc, 688, 214, T_GPS_TIME);
    SetRow(dc, 256, L"Формат времени", NULL, false, A_NONE, 0, H);
    Seg(dc, 560, 260, 200, 40, fmt, 2, G_TIME_FMT);
    SetRow(dc, 304, L"Язык", NULL, false, A_NONE, 0, H);
    T(dc, 720, 335, L"Русский", 18, kMuted, 400, AL_END);
    Chevron(dc, 728, 312);
    SetRow(dc, 352, L"Виджеты главной", NULL, true, A_SCREEN, S_SET_HOME, H);
    Chevron(dc, 728, 360);
}

static void ScrSetSound(HDC dc);
static void ScrSetBalance(HDC dc);
static void ScrSetVolume(HDC dc);

static void ScrSetScreen(HDC dc);   // (after the MCU's part: brightness)

// Settings -> Radio: the MCU's tuner settings. The switches show what the MCU reports (Tuner.set); a tap sends the
// stock shell's key for on / off (extended keys, SPIPower.log 2026-10-18), the MCU's reply moves the switch.
// Opening the screen asks for all of them (extended key 0x90, as the shell's screen does).
static const int kRsetToggle[RSET_COUNT] = { T_NOISE, T_MW, T_RDS, T_AF, T_CT, T_TA, T_MS };
static const int kRsetKey[RSET_COUNT][2] = { { 0xA8, 0xA7 }, { 0xAA, 0xA9 }, { 0x87, 0x86 }, { 0x8B, 0x8A },
                                             { 0x8F, 0x8E }, { 0x89, 0x88 }, { 0xAC, 0xAB } };   // off, on

static int RsetOfToggle(int id)
{
    for (int i = 0; i < RSET_COUNT; i++)
        if (kRsetToggle[i] == id)
            return i;
    return -1;
}

static void ScrSetRadio(HDC dc)
{
    static const wchar_t* rows[RSET_COUNT] = { L"Шумоподавление", L"Диапазон СВ", L"RDS",
                                               L"AF · альтернативные частоты", L"CT · время по радио",
                                               L"TA · дорожные сообщения", L"MS · музыка / речь" };
    static DWORD s_asked;
    if (!s_asked || GetTickCount() - s_asked > 10000) {   // once on opening (the screen redraws often)
        s_asked = GetTickCount();
        McuKey(0x90, true);
    }
    Tuner t;
    TunerGet(&t);
    Settings(dc, S_SET_RADIO);
    for (int i = 0, k = 0; i < RSET_COUNT; i++) {
        if (i == RSET_MW)
            continue;   // СВ: turned on by its page of the radio (2026-10-20, the user)
        bool known = (t.setKnown >> i & 1) != 0;
        if (known)
            s_on[kRsetToggle[i]] = t.set[i] != 0;
        SetRow(dc, 64 + k * 48, rows[i], NULL, i == RSET_COUNT - 1, A_NONE, 0, 48);
        if (known)
            Toggle(dc, 688, 70 + k * 48, kRsetToggle[i]);
        else
            T(dc, 760, 96 + k * 48, L"нет данных MCU", 14, kDim, 400, AL_END);
        k++;
    }
}

// Settings -> Bluetooth (the phone, the OBD adapter): the stock shell's Bluetooth stack (FLC SDK in G3NavHMI) owns pairing and the
// connected phone; with our proxy of FLC_BLUETOOTH_SDK.dll (btshm.h) VestaHome shows the phone and its links.
// Pairing and the options (auto-connect, auto-answer) - the next step, through the same proxy.
static bool McuBtAddr(wchar_t* s, int n);

static void ScrSetBt(HDC dc)
{
    Settings(dc, S_SET_BT);
    wchar_t a[24];
    SetRow(dc, 64, L"Адрес Bluetooth ГУ", McuBtAddr(a, 24) ? a : L"MCU ещё не прислал (нужен proxy MCU)");
    int vol, mute, src;
    VolGet(&vol, &mute, &src);
    SetRow(dc, 120, L"Музыка с телефона", src == 9 ? L"играет сейчас" : L"другой источник");
    if (src != 9)
        Button(dc, 560, 126, 200, 44, L"ВКЛЮЧИТЬ", B_N, IC_BT, 15, 22, A_BTMUSIC);
    BtState bt;
    if (BtGet(&bt, NULL)) {
        wchar_t name[BT_TEXT], s[200];
        if (bt.devName[0]) {
            BtText(bt.devName, name, BT_TEXT);
            _snwprintf(s, 200, L"%s · %S", name, bt.devAddr);
        } else {
            wcscpy(s, bt.ready ? L"нет" : L"Bluetooth оболочки ещё не запущен");
        }
        SetRow(dc, 176, L"Телефон", s);
        const wchar_t* av = bt.av == 2 ? L"играет" : bt.av == 1 ? L"подключена" : bt.av == 0 ? L"нет" : L"—";
        const wchar_t* hf = bt.hf == 2 ? L"подключены" : bt.hf == 0 ? L"нет" : L"—";
        wchar_t bs[40] = L"", ss[40] = L"";
        if (bt.battery >= 0)
            _snwprintf(bs, 40, L" · батарея %d", bt.battery);
        if (bt.signal >= 0)
            _snwprintf(ss, 40, L" · сеть %d", bt.signal);
        _snwprintf(s, 200, L"музыка: %s · звонки: %s%s%s", av, hf, bs, ss);
        SetRow(dc, 232, L"Связь", s);
        s[0] = 0;   // the paired ones (0x08), by name
        for (int i = 0; i < bt.paired && i < BT_PAIRED && wcslen(s) < 150; i++) {
            wchar_t nm[48];
            BtText(bt.pairedName[i], nm, 48);
            if (s[0])
                wcscat(s, L", ");
            wcsncat(s, nm, 40);
        }
        SetRow(dc, 288, L"Сопряжённые", s[0] ? s : L"—", false, A_SCREEN, S_BT_PAIRED);
        SetRow(dc, 344, L"OBD-адаптер (проверка)", L"поиск, сопряжение и опрос ELM327", true, A_SCREEN, S_SET_OBD);
    } else {
        SetRow(dc, 176, L"Сопряжение и подключение", L"пока в штатной оболочке: Телефон → Bluetooth", true);
        T(dc, 264, 268, L"Подключённый телефон и его связь появятся здесь с proxy Bluetooth", 15, kMuted, 400,
          AL_START, 500);
        T(dc, 264, 290, L"(VestaHomeInstall, кнопка 2: ставится вместе с proxy MCU).", 15, kMuted, 400, AL_START, 500);
    }
}

// Settings -> Bluetooth -> paired: the stack's list (asked anew on opening: the proxy's BT_CMD_LISTPAIRED), 7 a
// page (the dots turn it); "УДАЛИТЬ" asks once more ("ТОЧНО?"), then Conn_Delete_Paired (BT_CMD_UNPAIR).
enum { kPairedRows = 7 };
static int s_pairedPage, s_pairedAsk = -1;   // s_pairedAsk: the row whose removal waits for the second press

static void ScrBtPaired(HDC dc)
{
    Settings(dc, S_BT_PAIRED);
    static DWORD s_seen;
    if (s_seen != s_screenTick) {
        s_seen = s_screenTick;
        s_pairedAsk = -1;
        BtCmd(BT_CMD_LISTPAIRED);
    }
    BtState bt;
    if (!BtGet(&bt, NULL)) {
        SetRow(dc, 64, L"Сопряжённые", L"нужен proxy Bluetooth (VestaHomeInstall, кнопка 2)", true);
        return;
    }
    int n = bt.paired < BT_PAIRED ? bt.paired : BT_PAIRED, pages = (n + kPairedRows - 1) / kPairedRows;
    if (s_pairedPage >= pages)
        s_pairedPage = 0;   // (the dots go round)
    if (!n)
        SetRow(dc, 64, L"Сопряжённых нет", L"телефон находит ГУ сам: штатная оболочка, Телефон → Bluetooth", true);
    for (int k = 0; k < kPairedRows; k++) {
        int i = s_pairedPage * kPairedRows + k, y = 64 + k * 44;
        if (i >= n)
            break;
        wchar_t nm[48], sub[64];
        BtText(bt.pairedName[i], nm, 48);
        bool on = !strcmp(bt.pairedAddr[i], bt.devAddr) && bt.hf >= 1;
        _snwprintf(sub, 64, L"%S%s", bt.pairedAddr[i], on ? L" · подключён" : L"");
        sub[63] = 0;
        T(dc, 264, y + 20, nm[0] ? nm : L"без имени", 17, kText, 700, AL_START, 360);
        T(dc, 264, y + 38, sub, 13, on ? kOrange : kMuted, 400, AL_START, 360);
        Button(dc, 640, y + 4, 128, 36, s_pairedAsk == i ? L"ТОЧНО?" : L"УДАЛИТЬ", s_pairedAsk == i ? B_SEL : B_N,
               IC_NONE, 14, 0, A_BTUNPAIR, i);
        if (k + 1 < kPairedRows && i + 1 < n)
            Sep(dc, 256, y + 43, 512);
    }
    PageDots(dc, s_pairedPage, pages, 512, 386);
}

static int PairedPages()
{
    BtState bt;
    int n = BtGet(&bt, NULL) ? (bt.paired < BT_PAIRED ? bt.paired : BT_PAIRED) : 0;
    return n ? (n + kPairedRows - 1) / kPairedRows : 1;
}

// Settings -> Bluetooth -> OBD (a test, 2026-10-20): an ELM327 adapter found by the stack's search (the proxy's
// BT_CMD_SEARCH; the devices come as events with their address at byte 4 and name at 0x16), paired with its PIN
// (BT_CMD_PAIR), then connected over the stack's serial port and asked by ELM327 commands (obd.cpp). Every step
// shows here and goes to the log.
enum { kObdFound = 6 };
struct ObdDev { char addr[18]; char name[48]; };
static ObdDev s_obdFound[kObdFound];
static int    s_obdFoundN, s_obdPin;   // s_obdPin: kObdPins
static wchar_t s_obdAddr[18];        // the adapter chosen (ini obd.addr), "" none
static LONG   s_obdEvSeen = -1;        // the proxy's events read so far; -1: no search asked
static DWORD  s_obdSearchTick;
static const char* const kObdPins[2] = { "1234", "0000" };

static bool ObdScanEvents()
{
    BtShm* b = BtShmGet();
    if (s_obdEvSeen < 0 || !b || b->magic != BT_SHM_MAGIC)
        return false;
    bool changed = false;
    LONG last = b->evSeq;
    if (last - s_obdEvSeen > BT_EV_SLOTS)
        s_obdEvSeen = last - BT_EV_SLOTS;
    for (LONG n = s_obdEvSeen + 1; n <= last; n++) {
        BtEvent e = b->ev[(n - 1) & (BT_EV_SLOTS - 1)];
        const char* a = (const char*)e.data;
        if (e.seq != n || e.code == 0x0A || a[2] != ':' || a[5] != ':' || a[14] != ':')
            continue;
        int i = 0;
        while (i < s_obdFoundN && strncmp(s_obdFound[i].addr, a, 17))
            i++;
        if (i == kObdFound)
            continue;
        if (i == s_obdFoundN)
            s_obdFoundN++;
        memcpy(s_obdFound[i].addr, a, 17);
        s_obdFound[i].addr[17] = 0;
        int k = 0;
        for (; k < 47 && 18 + k < BT_EV_DATA && a[18 + k]; k++)
            s_obdFound[i].name[k] = a[18 + k];
        s_obdFound[i].name[k] = 0;
        VLog(L"obd: ev 0x%02X %S %S", (unsigned)e.code, s_obdFound[i].addr, s_obdFound[i].name);
        changed = true;
    }
    s_obdEvSeen = last;
    return changed;
}

static void ScrSetObd(HDC dc)
{
    Settings(dc, S_SET_OBD);
    BtState bt;
    bool proxy = BtGet(&bt, NULL);
    ObdInfo o;
    ObdGet(&o);
    wchar_t s[160], nm[48];
    char addr[18];
    WideCharToMultiByte(CP_ACP, 0, s_obdAddr, -1, addr, 18, NULL, NULL);
    addr[17] = 0;
    wcscpy(s, L"не выбран: НАЙТИ, затем нажмите на адаптер");
    if (addr[0]) {
        _snwprintf(s, 160, L"%s", s_obdAddr);
        for (int i = 0; i < s_obdFoundN; i++)
            if (!strcmp(s_obdFound[i].addr, addr) && s_obdFound[i].name[0]) {
                BtText(s_obdFound[i].name, nm, 48);
                _snwprintf(s, 160, L"%s · %s", nm, s_obdAddr);
            }
    }
    s[159] = 0;
    SetRow(dc, 64, L"OBD-адаптер (ELM327)", s);
    bool searching = s_obdEvSeen >= 0 && GetTickCount() - s_obdSearchTick < 15000;
    Button(dc, 656, 70, 112, 44, searching ? L"ПОИСК…" : L"НАЙТИ", !proxy ? B_DIS : searching ? B_SEL : B_N, IC_NONE,
           15, 0, proxy ? A_OBDSCAN : A_NONE);
    for (int i = 0; i < s_obdFoundN; i++) {
        if (s_obdFound[i].name[0])
            BtText(s_obdFound[i].name, nm, 48);
        else
            MultiByteToWideChar(CP_ACP, 0, s_obdFound[i].addr, -1, nm, 48);
        Button(dc, 256 + (i % 2) * 260, 124 + (i / 2) * 46, 252, 40, nm,
               !strcmp(s_obdFound[i].addr, addr) ? B_SEL : B_N, IC_NONE, 15, 0, A_OBDPICK, i);
    }
    if (!s_obdFoundN)
        T(dc, 264, 168, !proxy ? L"нужен proxy Bluetooth (VestaHomeInstall, кнопка 2)"
                     : searching ? L"ищу устройства (около 10 с)…"
                     : L"адаптер в разъём OBD, зажигание включено, затем НАЙТИ", 15, kMuted, 400, AL_START, 500);
    T(dc, 264, 296, L"PIN", 15, kMuted, 700);
    for (int i = 0; i < 2; i++) {
        wchar_t p[8];
        MultiByteToWideChar(CP_ACP, 0, kObdPins[i], -1, p, 8);
        Button(dc, 304 + i * 88, 270, 80, 40, p, i == s_obdPin ? B_SEL : B_N, IC_NONE, 15, 0, A_OBDPIN, i);
    }
    bool can = addr[0] != 0;
    bool busy = o.state == OBD_CONNECTING || o.state == OBD_ON;
    Button(dc, 488, 270, 136, 40, L"СОПРЯЧЬ", can && proxy ? B_N : B_DIS, IC_NONE, 15, 0,
           can && proxy ? A_OBDPAIR : A_NONE);
    Button(dc, 632, 270, 136, 40, busy ? L"ОТКЛЮЧИТЬ" : L"ПОДКЛЮЧИТЬ", can || busy ? (busy ? B_SEL : B_N) : B_DIS,
           IC_NONE, 15, 0, can || busy ? A_OBDCONN : A_NONE);
    int y = 336, first = 1;   // four lines fit, or the values and three
    if (o.state == OBD_ON) {
        wchar_t v[3][16];
        _snwprintf(v[0], 16, o.rpm >= 0 ? L"%d" : L"—", o.rpm);
        _snwprintf(v[1], 16, o.speed >= 0 ? L"%d" : L"—", o.speed);
        _snwprintf(v[2], 16, o.coolant != -1 ? L"%d" : L"—", o.coolant);
        _snwprintf(s, 160, L"обороты %s · скорость %s км/ч · ОЖ %s °C · %s", v[0], v[1], v[2],
                   o.volt[0] ? o.volt : L"—");
        s[159] = 0;
        T(dc, 264, y, s, 17, kOrange, 700, AL_START, 504);
        y += 22;
        first = 2;
    }
    for (int i = first; i < kObdLines; i++, y += 18)
        if (o.lines[i][0])
            T(dc, 264, y, o.lines[i], 13, kMuted, 400, AL_START, 504);
}

// Settings -> Wi-Fi (wifi.cpp: Windows CE's WZC): the state, the networks in sight (strongest first, kWifiRows a
// page) with their signal and security; a tap connects (a new secured one asks its password on the keyboard),
// "Забыть" drops a known one. The known networks live in VestaHome.ini: WZC forgets them on every boot.
enum { kWifiRows = 3 };   // under them: the message and the access point's controls
static int     s_wifiPage;
static wchar_t s_wifiPass[64];     // typed on the keyboard
static wchar_t s_wifiTarget[33];   // the network it is for
static wchar_t s_wifiTitle[64];

static void WifiStateText(const WifiInfo& w, wchar_t* s, int n)
{
    switch (w.state) {
    case WIFI_NOWZC:      _snwprintf(s, n, L"%s", w.msg[0] ? w.msg : L"нет службы Wi-Fi"); break;
    case WIFI_NOADAPTER:  _snwprintf(s, n, L"адаптер Wi-Fi не найден"); break;
    case WIFI_OFF:        _snwprintf(s, n, L"выключено"); break;
    case WIFI_AP:         _snwprintf(s, n, L"точка ГУ «%s» · телефонов: %d", w.apSsid, w.apClients); break;
    case WIFI_CONNECTING: _snwprintf(s, n, L"подключение к %s…", w.ssid); break;
    case WIFI_CONNECTED:
        _snwprintf(s, n, L"%s · %S", w.ssid, w.ip[0] ? w.ip : "получение адреса…");
        break;
    default:              _snwprintf(s, n, L"не подключено"); break;
    }
    s[n - 1] = 0;
}

static void ScrSetWifi(HDC dc)
{
    static const wchar_t* kSec[4] = { L"открытая", L"WEP", L"WPA", L"WPA2" };
    WifiInfo w;
    WifiGet(&w);
    Settings(dc, S_SET_WIFI);
    wchar_t s[120];
    WifiStateText(w, s, 120);
    SetRow(dc, 64, L"Wi-Fi", s);
    Toggle(dc, 696, 74, T_WIFI);
    if (w.state >= WIFI_DISCONNECTED)
        Button(dc, 556, 70, 124, 44, w.scanning ? L"ПОИСК…" : L"НАЙТИ", w.scanning ? B_SEL : B_N, IC_NONE, 15, 0,
               A_WIFISCAN);
    if (w.state == WIFI_AP) {   // the unit is the access point: what to type on the phone
        Panel(dc, 256, 124, 512, 168, 12);
        T(dc, 276, 160, w.apAuto ? L"ТОЧКА ГУ ДЛЯ ТЕЛЕФОНА (АВТО)" : L"ТОЧКА ГУ ДЛЯ ТЕЛЕФОНА", 14, kMuted, 700);
        _snwprintf(s, 120, L"Сеть: %s", w.apSsid);
        T(dc, 276, 198, s, 22, kText, 700, AL_START, 470);
        _snwprintf(s, 120, L"Пароль: %s", w.apPass);
        T(dc, 276, 232, s, 22, kText, 700, AL_START, 470);
        _snwprintf(s, 120, L"Подключено телефонов: %d (2,4 ГГц, канал 6)", w.apClients < 0 ? 0 : w.apClients);
        T(dc, 276, 270, s, 15, w.apClients > 0 ? kOrange : kMuted, 600, AL_START, 470);
    }
    int pages = (w.count + kWifiRows - 1) / kWifiRows;
    if (s_wifiPage >= pages)
        s_wifiPage = pages ? pages - 1 : 0;
    for (int r = 0; r < kWifiRows; r++) {
        int i = s_wifiPage * kWifiRows + r;
        if (i >= w.count)
            break;
        const WifiNet& n = w.nets[i];
        int y = 124 + r * 56;
        bool cur = (w.state == WIFI_CONNECTED || w.state == WIFI_CONNECTING) && !wcscmp(n.ssid, w.ssid);
        bool down = Zone(256, y, 512, 52, A_WIFINET, i);
        bool hl = down || cur;
        R(dc, 256, y, 512, 52, 10, hl ? kOrange : kPanel2);
        int bars = WifiBars(n.rssi);
        Ico(dc, IC_WIFI, 268, y + 10, 32, hl ? kInk : bars >= 3 ? kText : bars >= 2 ? kMuted : kDim);
        T(dc, 312, y + 23, n.ssid, 18, hl ? kInk : kText, 700, AL_START, 280);
        if (cur && w.state == WIFI_CONNECTED)
            _snwprintf(s, 120, L"подключено · %d дБм", n.rssi);
        else if (cur)
            _snwprintf(s, 120, L"подключение…");
        else
            _snwprintf(s, 120, L"%s%s · %d дБм", n.known ? L"сохранена · " : L"", kSec[n.sec & 3], n.rssi);
        T(dc, 312, y + 44, s, 14, hl ? kInk : kMuted, 400, AL_START, 280);
        if (n.sec != WSEC_OPEN)
            Ico(dc, IC_LOCK, 724, y + 12, 28, hl ? kInk : kDim);
        if (n.known)   // drawn last: its button wins over the row's
            Button(dc, 604, y + 9, 108, 34, L"ЗАБЫТЬ", hl ? B_FILL : B_N, IC_NONE, 13, 0, A_WIFIFORGET, i);
    }
    if (!w.count && w.state >= WIFI_DISCONNECTED)
        T(dc, 272, 160, w.scanning ? L"Ищу сети…" : L"Сетей не видно — нажмите «НАЙТИ»", 16, kMuted, 400);
    if (w.msg[0] && w.state != WIFI_NOWZC)
        T(dc, 256, 312, w.msg, 14, kDim, 400, AL_START, 370);
    // the unit's own access point: by hand, or automatically when no known network is in sight
    if (w.state >= WIFI_AP) {
        bool apOn = w.state == WIFI_AP && !w.apAuto;
        Button(dc, 256, 352, 196, 40, apOn ? L"ТОЧКА ГУ: ВКЛ" : L"ТОЧКА ГУ", apOn ? B_SEL : B_N, IC_NONE, 14, 0,
               A_WIFIAP);
        T(dc, 466, 378, L"авто", 15, kMuted, 600);
        Toggle(dc, 520, 354, T_WIFI_AP_AUTO);
    }
    if (pages > 1) {
        _snwprintf(s, 16, L"%d/%d", s_wifiPage + 1, pages);
        Button(dc, 644, 298, 56, 40, NULL, s_wifiPage ? B_N : B_DIS, IC_BACK, 18, 26, A_WIFIPAGE, -1);
        T(dc, 638, 324, s, 14, kMuted, 600, AL_END);
        Button(dc, 708, 298, 56, 40, NULL, s_wifiPage + 1 < pages ? B_N : B_DIS, IC_NEXT, 18, 26, A_WIFIPAGE, 1);
    }
}

// The player's picture: kept in proportion (black bands) or stretched over the window; the picture while the
// car moves (off: only the sound, the rules of the road). Both kept in the ini.
static void ScrSetVideo(HDC dc)
{
    static const wchar_t* scale[2] = { L"ВПИСАТЬ", L"РАСТЯНУТЬ" };
    Settings(dc, S_SET_VIDEO);
    SetRow(dc, 64, L"Масштаб");
    Seg(dc, 512, 70, 248, 44, scale, 2, G_VIDEO_SCALE, 15);
    SetRow(dc, 120, L"Видео в движении", L"выключено: в движении видео на паузе", true);
    Toggle(dc, 688, 130, T_VIDEO_MOVING);
}

static void ScrSetPhoto(HDC dc);   // (with the photos, after the settings' data)

static void CanStatusText(wchar_t* s, int n);
static void Size(wchar_t* s, int n, unsigned __int64 bytes);

// "About the system", the last section of the settings: what the unit can tell without the MCU. The stock
// "Info" (SW 27.0.0.4120, MCU TR8005-4120, CAN 1.19, OS 27.0.0.4108, ID) gets its versions from the MCU; here the
// shell's version is known by the size of G3NavHMI.exe (4120: 4 077 056 B). Diagnostics open from here, as the
// stock engineering menu opens from "Info".
static void ScrInfo(HDC dc)
{
    wchar_t shell[48], os[48], can[64], sd[48];
    WIN32_FIND_DATA fd;
    HANDLE f = FindFirstFile(L"\\ResidentFlash\\navigation\\app\\G3NavHMI.exe", &fd);
    if (f == INVALID_HANDLE_VALUE)
        wcscpy(shell, L"нет G3NavHMI.exe");
    else {
        FindClose(f);
        if (fd.nFileSizeLow == 4077056)
            wcscpy(shell, L"27.0.0.4120");
        else
            _snwprintf(shell, 48, L"неизвестна (%u Б)", (unsigned)fd.nFileSizeLow);
    }
    OSVERSIONINFO vi;
    memset(&vi, 0, sizeof(vi));
    vi.dwOSVersionInfoSize = sizeof(vi);
#ifdef UNDER_CE
    const wchar_t* osName = L"Windows CE";
#else
    const wchar_t* osName = L"Windows (ПК)";
#endif
    if (GetVersionEx(&vi))
        _snwprintf(os, 48, L"%s %u.%u · сборка %u", osName, (unsigned)vi.dwMajorVersion, (unsigned)vi.dwMinorVersion,
                   (unsigned)vi.dwBuildNumber);
    else
        wcscpy(os, L"?");
    CanStatusText(can, 64);
    const wchar_t* rows[5][2] = { { L"Модель", L"LADA Vesta · MMC" }, { L"Оболочка G3", shell }, { L"ОС", os },
                                  { L"CAN-модуль", can }, { L"VestaHome", L"0.5 · 2026-10-18" } };
    Settings(dc, S_INFO);
    for (int i = 0; i < 5; i++) {
        int y = 64 + i * 44;
        T(dc, 264, y + 29, rows[i][0], 17, kMuted, 400);
        T(dc, 760, y + 29, rows[i][1], 17, kText, 700, AL_END, 360);
        Sep(dc, 256, y + 42, 512);
    }
#ifdef UNDER_CE
    const wchar_t* vol = L"\\SDMemory";
#else
    const wchar_t* vol = s_dir;
#endif
    ULARGE_INTEGER avail, size, freeAll;
    int pct = -1;
    if (GetDiskFreeSpaceEx(vol, &avail, &size, &freeAll) && size.QuadPart) {
        wchar_t used[24], total[24];
        Size(used, 24, size.QuadPart - freeAll.QuadPart);
        Size(total, 24, size.QuadPart);
        _snwprintf(sd, 48, L"%s из %s", used, total);
        pct = (int)((size.QuadPart - freeAll.QuadPart) * 100 / size.QuadPart);
    } else {
        wcscpy(sd, L"нет");
    }
    T(dc, 264, 304, L"SD-карта", 17, kMuted, 400);
    if (pct >= 0) {
        R(dc, 372, 294, 200, 10, 5, kLine);
        R(dc, 372, 294, pct * 2 > 10 ? pct * 2 : 10, 10, 5, pct > 90 ? kStop : kMuted);
    }
    T(dc, 760, 304, sd, 17, kText, 700, AL_END, 180);
    // what the stock "Info" and its engineering screens show; diagnostics; closing / minimizing VestaHome
    Button(dc, 256, 324, 200, 64, L"ПОДРОБНЕЕ", B_N, IC_INFO, 15, 26, A_SCREEN, S_INFO_MORE);
    Button(dc, 464, 324, 216, 64, L"ДИАГНОСТИКА", B_N, IC_SEARCH, 15, 26, A_SCREEN, S_DIAG);
    Button(dc, 688, 324, 80, 64, NULL, B_N, IC_POWER, 18, 36, A_SCREEN, S_EXIT);
}

// ---- diagnostics: the stock shell hides its engineering menu (CEngMainMenuDlg: CAN/UART, speakers, keys, fan,
// GPS, Wi-Fi MAC, touch, TFT calibration...) behind the four corners of its "Info" window; here it opens from
// "About the system". The tests that need the MCU protocol are grey until it is known.

static int s_testStep;            // the screen test's current fill
enum { kTestSteps = 7, kTouchMax = 256 };
static POINT s_touch[kTouchMax];  // the touch test's trail, oldest first
static int   s_touchCount;

struct Test { const wchar_t* label; Action a; int arg; };
static const Test kTests[8] = { { L"ЭКРАН", A_SCREEN, S_TEST_SCREEN }, { L"КАСАНИЯ", A_SCREEN, S_TEST_TOUCH },
                                { L"КАЛИБРОВКА", A_CALIBRATE, 0 }, { L"ПАМЯТЬ И ДИСКИ", A_SCREEN, S_TEST_MEM },
                                { L"КНОПКИ · MCU", A_SCREEN, S_TEST_MCU }, { L"ЗВУК И ВИДЕО", A_SCREEN, S_TEST_AV },
                                { L"CAN / UART · ПОТОК", A_SCREEN, S_TEST_CAN }, { L"GPS", A_SCREEN, S_TEST_CAN } };

// Diagnostics (from "About the system"): the tests 2 x 4, then closing and minimizing VestaHome.
static void ScrDiag(HDC dc)
{
    Frame(dc, L"ДИАГНОСТИКА");
    Panel(dc, 16, 68, 768, 396);
    for (int i = 0; i < 8; i++)
        Button(dc, 40 + (i % 2) * 368, 84 + (i / 2) * 64, 352, 52, kTests[i].label,
               kTests[i].a == A_SOON ? B_DIS : B_N, IC_NONE, 16, 0, kTests[i].a, kTests[i].arg);
    T(dc, 40, 362, L"«Кнопки» — монитор MCU (нажатия руля и панели приходят оттуда). GPS и CAN — один поток.",
      14, kDim, 400, AL_START, 720);
    T(dc, 40, 384, L"«Звук и видео» — писк поверх радио и тестовые файлы (папка TestMedia).", 14, kDim, 400,
      AL_START, 720);
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

// Explorer's desktop and taskbar for leaving VestaHome (minimize or exit): started when it does not run.
static void StartExplorer()
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
        for (int i = 0; i < 30 && !s_taskbar; i++) {   // its taskbar comes up within a few seconds
            Sleep(100);
            s_taskbar = FindWindow(L"HHTaskBar", NULL);
        }
    }
    if (s_taskbar)
        ShowWindow(s_taskbar, SW_SHOW);
#endif
}

// "Minimize": VestaHome stays open but goes behind everything (not hidden), Explorer comes up. Starting
// VestaHome.exe again, or any activation, brings it back on top (WM_ACTIVATE).
static bool s_minimized;

static void MinimizeHome()
{
#ifdef UNDER_CE
    s_minimized = true;
    SetWindowPos(s_wnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    SetWindowPos(s_wnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    StartExplorer();
    VLog(L"свернут (на задний план)");
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
    if (s_partial)   // the screen under the window has no buttons
        s_zoneIdx = 0;
    else
        kBtnCount = 0;
    Shade(0, 56, 800, 424, alpha, alpha);
}

// The power button of "About the system": exit or minimize VestaHome; both bring up Explorer. The stock shell
// and started programs keep running.
static void ScrExit(HDC dc)
{
    Under(dc, 184);
    Panel(dc, 110, 110, 580, 260, 16);
    Ico(dc, IC_POWER, 142, 140, 48, kText);
    T(dc, 206, 176, L"VESTAHOME", 24, kText, 700, AL_START, 440);
    T(dc, 142, 232, L"Свернуть — останется открытым позади, появится рабочий стол.", 17, kMuted, 400, AL_START,
      520);
    T(dc, 142, 256, L"Выход — закроется. Штатная оболочка продолжит работать.", 17, kMuted, 400, AL_START, 520);
    Button(dc, 142, 284, 160, 60, L"ОТМЕНА", B_N, IC_NONE, 17, 0, A_BACK);
    Button(dc, 320, 284, 160, 60, L"СВЕРНУТЬ", B_N, IC_NONE, 17, 0, A_MINIMIZE);
    Button(dc, 498, 284, 160, 60, L"ВЫХОД", B_PRI, IC_POWER, 17, 24, A_EXIT);
}

// The knob held 5 s (MCU 0x1102 18): the MCU reboots the unit 5 s later. The stock shell shows its own window
// with "Cancel" = key 0x5A (SPIPower.log 2026-10-18); this is it over VestaHome.
static DWORD s_rebootTick;

static void ScrReboot(HDC dc)
{
    Under(dc, 184);
    Panel(dc, 150, 130, 500, 220, 16);
    Ico(dc, IC_POWER, 182, 162, 48, kOrange);
    int left = 5 - (int)((GetTickCount() - s_rebootTick) / 1000);
    wchar_t s[48];
    _snwprintf(s, 48, L"ПЕРЕЗАГРУЗКА ЧЕРЕЗ %d С", left < 0 ? 0 : left);
    T(dc, 246, 198, s, 24, kText, 700, AL_START, 380);
    T(dc, 182, 250, L"Крутилка громкости удержана. Отменить?", 17, kMuted, 400, AL_START, 440);
    Button(dc, 250, 274, 300, 60, L"ОТМЕНА", B_PRI, IC_NONE, 18, 0, A_REBOOTNO);
}

static void ScrIncoming(HDC dc)
{
    BtState bt;
    BtGet(&bt, NULL);
    wchar_t num[40];
    BtText(bt.callNumber, num, 40);
    Under(dc, 184);
    Panel(dc, 100, 84, 600, 340, 16);
    Disc(dc, 196, 184, 64, kPanel2);
    Ico(dc, IC_CALL, 172, 160, 48, kOk);
    T(dc, 286, 150, L"ВХОДЯЩИЙ ВЫЗОВ", 14, kMuted, 700);
    T(dc, 286, 200, num[0] ? num : L"номер скрыт", 32, kText, 700, AL_START, 400);
    bool d = Zone(124, 300, 266, 92, A_PHONEANS);   // green and red: here the colour carries the meaning
    R(dc, 124, 300, 266, 92, 12, d ? Mix(kOk, kInk, 160) : kOk);
    Ico(dc, IC_CALL, 233, 322, 48, kInk);
    d = Zone(410, 300, 266, 92, A_PHONEEND);
    R(dc, 410, 300, 266, 92, 12, d ? Mix(kStop, kInk, 160) : kStop);
    Ico(dc, IC_HANGUP, 519, 322, 48, kInk);
}

enum { kVolTop = 30 };   // the shell's volume scale (user 2026-10-19)

static void ScrVolume(HDC dc)
{
    Under(dc, 153);
    Zone(0, 0, 800, 480, A_BACK);
    Panel(dc, 150, 150, 500, 180, 16);
    // the MCU's volume of the current source, 0..30 as the shell's (30 bars, one a step), as it comes (the knob,
    // the wheel)
    int vol, mute, src;
    bool have = VolGet(&vol, &mute, &src);
    const wchar_t* name = src == 0 ? L"ГРОМКОСТЬ · РАДИО" : src == 6 || src == 7 ? L"ГРОМКОСТЬ · МЕДИА"
                        : src == 9 ? L"ГРОМКОСТЬ · BLUETOOTH" : L"ГРОМКОСТЬ";
    Ico(dc, IC_VOL, 182, 182, 48, have && !mute ? kText : kDim);
    T(dc, 246, 216, name, 15, kMuted, 700);
    wchar_t s[16];
    if (!have)
        wcscpy(s, L"–");
    else if (mute)
        wcscpy(s, L"БЕЗ ЗВУКА");
    else
        _snwprintf(s, 16, L"%d", vol);
    T(dc, 618, 222, s, mute ? 28 : 48, mute ? kMuted : kText, 700, AL_END);
    int lit = have && !mute ? (vol < kVolTop ? vol : kVolTop) : 0;
    for (int i = 0; i < kVolTop; i++) {
        int h = 14 + i * 36 / (kVolTop - 1);
        R(dc, 182 + i * 15, 300 - h, 10, h, 2, i < lit ? kOrange : kLine);
    }
    if (!have)
        T(dc, 182, 316, L"MCU ещё не присылал громкость (нужен proxy)", 13, kDim, 400);
}

// The screen saver: the skin's picture (Skin\saver.png, skin.ini "saver": Lada's X-design, made by
// skin/make_saver.py), dimmed by the night theme; the clock, the date, the weather with its picture in the middle,
// what plays below. A tap goes back.
static void SaverWeather(HDC dc, int cx, int y);   // (after the weather's part)

static void ScrSaver(HDC dc)
{
    SYSTEMTIME t;
    GetLocalTime(&t);
    wchar_t clock[8], date[48], s[160];
    Now(clock, 8);
    _snwprintf(date, 48, L"%s, %d %s", kDays[t.wDayOfWeek % 7], t.wDay, kMonths[(t.wMonth + 11) % 12]);
    CharUpperBuff(date, 1);
    Zone(0, 0, 800, 480, A_BACK);
    SaverBackground(dc);
    T(dc, 400, 218, clock, 132, kText, 700, AL_MID);
    T(dc, 400, 270, date, 24, kMuted, 400, AL_MID);
    SaverWeather(dc, 400, 300);
    // what plays: the radio's station, Bluetooth's track
    s[0] = 0;
    int vol, mute, src;
    VolGet(&vol, &mute, &src);
    BtState bt;
    if (src == 9 && BtGet(&bt, NULL) && bt.title[0]) {
        BtText(bt.title, s, 160);
    } else if (src == 0) {
        Tuner tu;
        TunerGet(&tu);
        wchar_t f[16];
        FreqText(tu.f10, f, 16);
        const wchar_t* nm = tu.ps[0] ? tu.ps : RadioName(tu, tu.f10);
        if (tu.f10)
            _snwprintf(s, 160, L"%s  %s", f, nm);
    }
    if (s[0]) {
        int w = TextWidth(dc, s, 18, 700);
        w = w > 560 ? 560 : w;
        Ico(dc, src == 0 ? IC_RADIO : IC_MUSIC, 400 - (w + 40) / 2, 424, 30, kMuted);
        T(dc, 400 - (w + 40) / 2 + 40, 446, s, 18, kText, 700, AL_START, 560);
    }
}

static void Trim(wchar_t* s);

// ---------------------------------------------------------------- services: settings, GPS, weather, cameras

// ---- settings: VestaHome.ini next to the exe (UTF-8, key=value). The unit resets its registry on every boot;
// only the card keeps them.
enum { W_OMETEO, W_OWM, W_YANDEX, W_COUNT };
static const char* kProvKey[W_COUNT] = { "openmeteo", "owm", "yandex" };   // anything else (old wttr, gismeteo): Open-Meteo

// The main screen's widgets (DrawHome's table follows them; the ini names them).
enum { WG_MEDIA, WG_SPEED, WG_RADIO, WG_WEATHER, WG_APPS, WG_TRIP, WG_CLIMATE, WG_DATE, WG_PHONE, WG_CAR, WG_NAVI,
       WG_COUNT };
static const wchar_t* kWgKey[WG_COUNT] = { L"media", L"speed", L"radio", L"weather", L"apps", L"trip", L"climate",
                                           L"date", L"phone", L"car", L"navi" };

// The user's widgets (VestaWidgets\*.ini on the SD card, UserWidgetsScan): ids after the built-in ones, keys
// "u.<file name>". A script (MortScript) writes the data file, VestaHome only reads and draws it.
enum { kWgUser = 8, kWgMax = WG_COUNT + kWgUser };
struct UserWg {
    wchar_t  key[40], title[40], dir[MAX_PATH], data[MAX_PATH], tap[MAX_PATH], start[MAX_PATH];
    Icon     ic;
    bool     wide, have;
    int      refresh, state;        // refresh: seconds between reads; state: 0 none, 1 ok, 2 warn, 3 alert
    DWORD    readTick;
    FILETIME dataTime;              // the data file's write time last read
    wchar_t  value[24], unit[24], text[2][96];
    // HTTPS through VestaHome (UserWgFetch): url, every (s), the raw answer saved to resp, what to take from it
    // (fields 0 value, 1 unit, 2 text, 3 text2, 4 state: "start|end" markers) and the fixed parts ("%s" = taken)
    char     url[512];
    int      every;
    DWORD    fetchTick;
    wchar_t  resp[MAX_PATH], from[5][64], fixed[5][96];
};
static UserWg s_uw[kWgUser];
static int    s_uwCount;

static int WgCount() { return WG_COUNT + s_uwCount; }
static const wchar_t* WgKey(int id) { return id < WG_COUNT ? kWgKey[id] : s_uw[id - WG_COUNT].key; }

struct Cfg {
    int     weatherProv;
    wchar_t weatherKey[W_COUNT][64];   // API key per provider (Open-Meteo needs none)
    bool    weatherGps;                // the place where the car is; without a fix the home point
    wchar_t homeName[48];
    double  homeLat, homeLon;          // both 0 = not set
    wchar_t gpsPort[16];               // the shell's NMEA port, only read
    DWORD   gpsBaud;                   // 0 = leave the speed the shell set
    bool    gpsTime;                   // set the clock from GPS
    int     tzMin;                     // local time = UTC + this (minutes): the unit's own zone resets with the registry
    bool    climate;                   // the car has climate control: its widget is shown
    bool    camAlert, camSound;
    int     camOver;          // the beep: -1 always, else only above the camera's limit + camOver km/h
    int     beepDev;          // the beep's waveOut device, -1 = WAVE_MAPPER (2026-10-17: which one the MCU mixes?)
    int     camDist;                   // m, 0 = by speed
    wchar_t camFile[MAX_PATH];         // the camera file (iGO / Navitel), relative = from the exe's folder
    wchar_t camRegions[200];           // speedcamonline.ru regions to download: "63,73", "-1" = all of Russia
    wchar_t camKinds[100];             // its camera kinds (the site's filter numbers)
    bool    camWeekly;                 // download them again once a week
    int     camUpdated;                // yyyymmdd of the last download, 0 = never
    int     yaDay, yaCount;            // Yandex weather requests made on that day (yyyymmdd): its limit, see YaTake
    int     eq[3];                     // equalizer low, mid, high: -6..6 (what was last sent to the MCU)
    int     balance, fader;            // -8..8, 0 = centre (CAudioFieldDlg)
    wchar_t musicDir[MAX_PATH];        // the player's folder and track, to come back to
    wchar_t musicFile[MAX_PATH];
    bool    shuffle, repeat;
    bool    musicTree;                 // the playlist takes the subfolders too ("ИГРАТЬ ВСЁ")
    wchar_t videoDir[MAX_PATH];        // the video player's folder and file
    wchar_t videoFile[MAX_PATH];
    bool    videoStretch;              // fill the window instead of keeping the proportions
    bool    videoMoving;               // the picture also while the car moves
    bool    rvcOwn;                    // the rear view camera shown by VestaHome (rvc.cpp), not by the shell
    bool    wifiOn;                    // the Wi-Fi radio (wifi.cpp WifiEnable)
    int     saverMin;                  // the screen saver after this many minutes untouched, 0 = never
    int     themeMode, lightMode;      // our colours and maps / the unit's backlight: 0 by the sun, 1 day, 2 night
    int     homeOrder[kWgMax];         // the main screen's widgets (WG_*, then the user's) in their order
    wchar_t homeRaw[400];              // home.widgets as read: keeps the user widgets not found now (no card)
    int     homeHidden;                // bit 1 << WG_*: hidden
    wchar_t uiScreen[16], uiSource[8];   // the screen and the source last seen (Resume*): "home", "radio"...
    wchar_t btLast[18];                // the phone connected last, "A4:E2:87:98:D6:9F"
    wchar_t photoDir[MAX_PATH];        // the photos' folder last opened
    int     photoInterval;             // the slide show's seconds
    bool    photoFill, photoSlides, photoAutoRot;   // fill the screen / the slide show on opening / EXIF turn
    bool    wifiApAuto;                // the access point for a phone, automatically (wifi.cpp WifiApSetup)
    wchar_t wifiApSsid[33], wifiApPass[65];
    bool    rvcInvert;                 // the trajectory bends the other way (the CAN angle's sign)
    wchar_t radioStation[64];          // the internet radio's station (its name in radio.txt)
    int     wifiCount;                 // the known Wi-Fi networks: WZC forgets them on every boot (the registry)
    wchar_t wifiSsid[kWifiKnown][33];
    wchar_t wifiPass[kWifiKnown][64];  // open in memory; in the file sealed with the unit's key (secret.cpp)
    bool    wifiLocked[kWifiKnown];    // sealed by another unit (or damaged): kept as it was, not used
    wchar_t wifiSealed[kWifiKnown][300];
};
static Cfg s_cfg;
static const int kSaverMins[5] = { 0, 1, 3, 5, 10 };
static const int kCamDists[4] = { 0, 200, 400, 600 };   // 0 = by speed
// Camera bases live in their own folder next to the exe (kept out of git): downloads and the default file.
static const wchar_t* kCamFolder  = L"speedcam\\";
static const wchar_t* kCamDefault = L"speedcam\\speedcam.txt";

static void CfgDefaults()
{
    memset(&s_cfg, 0, sizeof(s_cfg));
    s_cfg.weatherProv = W_OMETEO;
    s_cfg.weatherGps = true;
    wcscpy(s_cfg.gpsPort, L"COM5:");
    s_cfg.gpsTime = true;
    s_cfg.tzMin = 180;                            // Moscow
    s_cfg.camAlert = s_cfg.camSound = true;
    s_cfg.camOver = -1;
    s_cfg.beepDev = 1;   // "Audio Output" 1: heard over the radio (HU 2026-10-18-2)
    s_cfg.wifiOn = true;
    s_cfg.saverMin = 5;
    s_cfg.photoInterval = 5;
    s_cfg.photoAutoRot = true;
    for (int i = 0; i < WG_COUNT; i++)
        s_cfg.homeOrder[i] = i;
    s_cfg.homeHidden = 0;
    wcscpy(s_cfg.wifiApSsid, L"VestaHome");
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
static void RadioCacheLoad(const wchar_t* k, const wchar_t* v);   // the tuner's last state, the stations' names
static int  RadioCacheText(wchar_t* out, int cap);

// "media,speed,-radio,u.fuel,...": the order, "-" hides; names not known now are skipped (and kept in homeRaw for
// when they come back), widgets not named go to the end. Again after the user widgets are scanned.
static void HomeWidgetsLoad(const wchar_t* v)
{
    if (v != s_cfg.homeRaw) {
        wcsncpy(s_cfg.homeRaw, v, 399);
        s_cfg.homeRaw[399] = 0;
    }
    int n = 0;
    bool seen[kWgMax] = { false };
    s_cfg.homeHidden = 0;
    while (*v) {
        while (*v == L' ' || *v == L',')
            v++;
        bool hide = *v == L'-';
        if (hide)
            v++;
        const wchar_t* e = v;
        while (*e && *e != L',' && *e != L' ')
            e++;
        for (int i = 0; i < WgCount(); i++)
            if (!seen[i] && (int)wcslen(WgKey(i)) == e - v && !wcsncmp(WgKey(i), v, e - v)) {
                seen[i] = true;
                s_cfg.homeOrder[n++] = i;
                if (hide)
                    s_cfg.homeHidden |= 1 << i;
            }
        v = e;
    }
    for (int i = 0; i < WgCount(); i++)
        if (!seen[i])
            s_cfg.homeOrder[n++] = i;
}

// The order to save: the widgets known now, then the user widgets of homeRaw not found now (the card out).
static void HomeWidgetsText(wchar_t* out, int cap)
{
    out[0] = 0;
    for (int i = 0; i < WgCount(); i++) {
        int id = s_cfg.homeOrder[i];
        wchar_t t[48];
        _snwprintf(t, 48, L"%s%s%s", i ? L"," : L"", s_cfg.homeHidden >> id & 1 ? L"-" : L"", WgKey(id));
        if ((int)(wcslen(out) + wcslen(t)) < cap)
            wcscat(out, t);
    }
    const wchar_t* v = s_cfg.homeRaw;
    while (*v) {
        while (*v == L' ' || *v == L',')
            v++;
        const wchar_t* b = v;
        const wchar_t* k = *v == L'-' ? v + 1 : v;
        const wchar_t* e = k;
        while (*e && *e != L',' && *e != L' ')
            e++;
        bool known = false;
        for (int i = 0; i < WgCount() && !known; i++)
            known = (int)wcslen(WgKey(i)) == e - k && !wcsncmp(WgKey(i), k, e - k);
        if (!known && e - k > 2 && !wcsncmp(k, L"u.", 2) && (int)(wcslen(out) + (e - b) + 2) < cap) {
            wcscat(out, L",");
            wcsncat(out, b, e - b);
        }
        v = e;
    }
    wcsncpy(s_cfg.homeRaw, out, 399);
    s_cfg.homeRaw[399] = 0;
}

static void LoadCfg()
{
    CfgDefaults();
    bool resave = false;   // a password written open by hand: sealed now
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
        } else if (!wcscmp(k, L"cam.over")) {
            s_cfg.camOver = _wtoi(v);
        } else if (!wcscmp(k, L"sound.beepdev")) {
            s_cfg.beepDev = _wtoi(v) < 0 ? 1 : _wtoi(v);   // AUTO was not heard over the radio
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
        } else if (!wcscmp(k, L"weather.yandex.day")) {
            s_cfg.yaDay = _wtoi(v);
        } else if (!wcscmp(k, L"weather.yandex.count")) {
            s_cfg.yaCount = _wtoi(v);
        } else if (!wcscmp(k, L"cam.file")) {
            wcsncpy(s_cfg.camFile, v, MAX_PATH - 1);
            s_cfg.camFile[MAX_PATH - 1] = 0;
        } else if (!wcscmp(k, L"audio.eq")) {
            swscanf(v, L"%d,%d,%d", &s_cfg.eq[0], &s_cfg.eq[1], &s_cfg.eq[2]);
            for (int i = 0; i < 3; i++)
                s_cfg.eq[i] = s_cfg.eq[i] < -6 ? -6 : s_cfg.eq[i] > 6 ? 6 : s_cfg.eq[i];
        } else if (!wcscmp(k, L"audio.balance")) {
            s_cfg.balance = _wtoi(v);
        } else if (!wcscmp(k, L"audio.fader")) {
            s_cfg.fader = _wtoi(v);
        } else if (!wcscmp(k, L"music.dir")) {
            wcsncpy(s_cfg.musicDir, v, MAX_PATH - 1);
            s_cfg.musicDir[MAX_PATH - 1] = 0;
        } else if (!wcscmp(k, L"music.file")) {
            wcsncpy(s_cfg.musicFile, v, MAX_PATH - 1);
            s_cfg.musicFile[MAX_PATH - 1] = 0;
        } else if (!wcscmp(k, L"music.tree")) {
            s_cfg.musicTree = CfgBool(v);
        } else if (!wcscmp(k, L"music.shuffle")) {
            s_cfg.shuffle = CfgBool(v);
        } else if (!wcscmp(k, L"music.repeat")) {
            s_cfg.repeat = CfgBool(v);
        } else if (!wcscmp(k, L"video.dir")) {
            wcsncpy(s_cfg.videoDir, v, MAX_PATH - 1);
            s_cfg.videoDir[MAX_PATH - 1] = 0;
        } else if (!wcscmp(k, L"video.file")) {
            wcsncpy(s_cfg.videoFile, v, MAX_PATH - 1);
            s_cfg.videoFile[MAX_PATH - 1] = 0;
        } else if (!wcscmp(k, L"video.stretch")) {
            s_cfg.videoStretch = CfgBool(v);
        } else if (!wcscmp(k, L"video.moving")) {
            s_cfg.videoMoving = CfgBool(v);
        } else if (!wcscmp(k, L"rvc.own")) {
            s_cfg.rvcOwn = CfgBool(v);
        } else if (!wcscmp(k, L"wifi.on")) {
            s_cfg.wifiOn = CfgBool(v);
        } else if (!wcscmp(k, L"ui.screen")) {
            wcsncpy(s_cfg.uiScreen, v, 15);
            s_cfg.uiScreen[15] = 0;
        } else if (!wcscmp(k, L"photo.dir")) {
            wcsncpy(s_cfg.photoDir, v, MAX_PATH - 1);
            s_cfg.photoDir[MAX_PATH - 1] = 0;
        } else if (!wcscmp(k, L"photo.interval")) {
            s_cfg.photoInterval = _wtoi(v) < 2 ? 2 : _wtoi(v) > 60 ? 60 : _wtoi(v);
        } else if (!wcscmp(k, L"photo.fill")) {
            s_cfg.photoFill = CfgBool(v);
        } else if (!wcscmp(k, L"photo.slides")) {
            s_cfg.photoSlides = CfgBool(v);
        } else if (!wcscmp(k, L"photo.autorotate")) {
            s_cfg.photoAutoRot = CfgBool(v);
        } else if (!wcscmp(k, L"bt.last")) {
            wcsncpy(s_cfg.btLast, v, 17);
            s_cfg.btLast[17] = 0;
        } else if (!wcscmp(k, L"obd.addr")) {
            wcsncpy(s_obdAddr, v, 17);
            s_obdAddr[17] = 0;
        } else if (!wcscmp(k, L"ui.source")) {
            wcsncpy(s_cfg.uiSource, v, 7);
            s_cfg.uiSource[7] = 0;
        } else if (!wcscmp(k, L"home.widgets")) {
            HomeWidgetsLoad(v);
        } else if (!wcscmp(k, L"screen.theme")) {
            s_cfg.themeMode = _wtoi(v) % 3;
        } else if (!wcscmp(k, L"screen.light")) {
            s_cfg.lightMode = _wtoi(v) % 3;
        } else if (!wcscmp(k, L"screen.saver")) {
            s_cfg.saverMin = _wtoi(v) < 0 ? 0 : _wtoi(v) > 60 ? 60 : _wtoi(v);
        } else if (!wcscmp(k, L"wifi.ap.auto")) {
            s_cfg.wifiApAuto = CfgBool(v);
        } else if (!wcscmp(k, L"wifi.ap.ssid") && v[0]) {
            wcsncpy(s_cfg.wifiApSsid, v, 32);
        } else if (!wcscmp(k, L"wifi.ap.pass") && wcslen(v) >= 8) {
            wcsncpy(s_cfg.wifiApPass, v, 63);
        } else if (!wcscmp(k, L"rvc.invert")) {
            s_cfg.rvcInvert = CfgBool(v);
        } else if (!wcsncmp(k, L"radio.c", 7) || !wcscmp(k, L"radio.names")) {
            RadioCacheLoad(k, v);
        } else if (!wcscmp(k, L"radio.station")) {
            wcsncpy(s_cfg.radioStation, v, 63);
            s_cfg.radioStation[63] = 0;
        } else if (!wcscmp(k, L"wifi.net") && s_cfg.wifiCount < kWifiKnown) {   // name|sealed password
            wchar_t* bar = wcschr(v, L'|');
            if (bar && bar > v) {
                *bar = 0;
                int i = s_cfg.wifiCount++;
                wcsncpy(s_cfg.wifiSsid[i], v, 32);
                s_cfg.wifiSsid[i][32] = 0;
                const wchar_t* pass = bar + 1;
                if (!SecretIsSealed(pass)) {   // written open by hand: taken, sealed at the next save
                    wcsncpy(s_cfg.wifiPass[i], pass, 63);
                    s_cfg.wifiPass[i][63] = 0;
                    if (pass[0])
                        resave = true;
                } else if (!SecretOpen(pass, s_cfg.wifiPass[i], 64)) {
                    s_cfg.wifiLocked[i] = true;
                    wcsncpy(s_cfg.wifiSealed[i], pass, 299);
                    s_cfg.wifiSealed[i][299] = 0;
                    VLog(L"wifi: пароль сети %s не расшифрован (другое ГУ?)", s_cfg.wifiSsid[i]);
                }
            }
        }
    }
    free(buf);
    if (resave)
        SaveCfg();
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
    wchar_t homeWg[200];
    HomeWidgetsText(homeWg, 200);
    static wchar_t text[12000];
    int n = _snwprintf(text, 12000,
        L"# VestaHome settings (changed from its screens; the file is rewritten whole).\r\n"
        L"weather.provider=%S\r\nweather.key.owm=%s\r\nweather.key.yandex=%s\r\n"
        L"# Yandex requests made that day (its free tariff: 30 a day)\r\nweather.yandex.day=%d\r\nweather.yandex.count=%d\r\n"
        L"weather.gps=%d\r\nhome.name=%s\r\nhome.lat=%.5f\r\nhome.lon=%.5f\r\n"
        L"# gps.baud: 0 = leave the port's speed as the shell set it\r\n"
        L"gps.port=%s\r\ngps.baud=%u\r\ngps.time=%d\r\n"
        L"# time.tz: local time = UTC + minutes (180 = Moscow)\r\ntime.tz=%d\r\ncar.climate=%d\r\n"
        L"# cam.file: the camera file (iGO or Navitel), a relative path is from the VestaHome folder;\r\n"
        L"# downloads go to speedcam\\speedcam.txt\r\n"
        L"# cam.over: the beep only above the camera's limit + this many km/h; -1 = always\r\n"
        L"cam.alert=%d\r\ncam.sound=%d\r\ncam.over=%d\r\nsound.beepdev=%d\r\ncam.distance=%d\r\ncam.file=%s\r\n"
        L"# cam.regions, cam.kinds: what to download from speedcamonline.ru (region codes, -1 = Russia; kinds)\r\n"
        L"cam.regions=%s\r\ncam.kinds=%s\r\ncam.weekly=%d\r\ncam.updated=%d\r\n"
        L"# audio: equalizer low,mid,high -6..6; balance/fader -8..8 (sent to the MCU when changed)\r\n"
        L"audio.eq=%d,%d,%d\r\naudio.balance=%d\r\naudio.fader=%d\r\n"
        L"# music.tree: the playlist is music.dir with all its subfolders (music.file a path inside it)\r\n"
        L"music.dir=%s\r\nmusic.file=%s\r\nmusic.shuffle=%d\r\nmusic.repeat=%d\r\nmusic.tree=%d\r\n"
        L"# video.moving: the video also while the car moves (0: paused)\r\n"
        L"video.dir=%s\r\nvideo.file=%s\r\nvideo.stretch=%d\r\nvideo.moving=%d\r\nradio.station=%s\r\n"
        L"# rvc.own: the rear view camera by VestaHome (without the stock shell); rvc.invert: the trajectory mirrored\r\n"
        L"rvc.own=%d\r\nrvc.invert=%d\r\nwifi.on=%d\r\n"
        L"# screen.saver: the clock after this many minutes untouched (0 = never)\r\n"
        L"screen.saver=%d\r\n"
        L"# screen.theme / screen.light: our colours and maps / the unit's backlight: 0 by sunrise and sunset (GPS, else\r\n"
        L"# home, else 7:00-20:00), 1 day, 2 night\r\n"
        L"screen.theme=%d\r\nscreen.light=%d\r\n"
        L"# home.widgets: the main screen's widgets in their order, \"-\" before a hidden one\r\n"
        L"home.widgets=%s\r\n"
        L"# ui.screen / ui.source: the screen and what played when last seen - both come back at the start\r\n"
        L"ui.screen=%s\r\nui.source=%s\r\n"
        L"# bt.last: the phone connected last (its address): connected again at the start when the source was BT\r\n"
        L"bt.last=%s\r\n"
        L"# obd.addr: the OBD-II Bluetooth adapter (ELM327; Settings -> Bluetooth -> OBD)\r\n"
        L"obd.addr=%s\r\n"
        L"# photo.*: the photos' folder, the slide show's seconds, fill the screen, the slide show on opening, EXIF turn\r\n"
        L"photo.dir=%s\r\nphoto.interval=%d\r\nphoto.fill=%d\r\nphoto.slides=%d\r\nphoto.autorotate=%d\r\n"
        L"# wifi.ap.*: the unit's own access point for a phone with its hotspot on 5 GHz (auto: when no known network)\r\n"
        L"wifi.ap.ssid=%s\r\nwifi.ap.pass=%s\r\nwifi.ap.auto=%d\r\n",
        kProvKey[s_cfg.weatherProv], s_cfg.weatherKey[W_OWM], s_cfg.weatherKey[W_YANDEX], s_cfg.yaDay, s_cfg.yaCount,
        s_cfg.weatherGps, s_cfg.homeName, s_cfg.homeLat, s_cfg.homeLon,
        s_cfg.gpsPort, (unsigned)s_cfg.gpsBaud, s_cfg.gpsTime, s_cfg.tzMin, s_cfg.climate, s_cfg.camAlert, s_cfg.camSound,
        s_cfg.camOver, s_cfg.beepDev, s_cfg.camDist, s_cfg.camFile, s_cfg.camRegions, s_cfg.camKinds, s_cfg.camWeekly, s_cfg.camUpdated,
        s_cfg.eq[0], s_cfg.eq[1], s_cfg.eq[2], s_cfg.balance, s_cfg.fader, s_cfg.musicDir, s_cfg.musicFile,
        s_cfg.shuffle, s_cfg.repeat, s_cfg.musicTree, s_cfg.videoDir, s_cfg.videoFile, s_cfg.videoStretch, s_cfg.videoMoving,
        s_cfg.radioStation, s_cfg.rvcOwn, s_cfg.rvcInvert, s_cfg.wifiOn, s_cfg.saverMin, s_cfg.themeMode, s_cfg.lightMode, homeWg, s_cfg.uiScreen, s_cfg.uiSource, s_cfg.btLast, s_obdAddr, s_cfg.photoDir, s_cfg.photoInterval, s_cfg.photoFill,
        s_cfg.photoSlides, s_cfg.photoAutoRot,
        s_cfg.wifiApSsid, s_cfg.wifiApPass, s_cfg.wifiApAuto);
    if (n < 0)
        return;
    n += RadioCacheText(text + n, 12000 - n);
    if (s_cfg.wifiCount) {
        int k = _snwprintf(text + n, 12000 - n,
                           L"# wifi.net: a known Wi-Fi network, name|password sealed with this unit's key\r\n");
        n += k > 0 ? k : 0;
    }
    for (int i = 0; i < s_cfg.wifiCount && n < 11500; i++) {
        wchar_t sealed[300];
        if (s_cfg.wifiLocked[i])
            wcscpy(sealed, s_cfg.wifiSealed[i]);
        else if (!SecretSeal(s_cfg.wifiPass[i], sealed, 300))
            continue;   // never written open
        int k = _snwprintf(text + n, 12000 - n, L"wifi.net=%s|%s\r\n", s_cfg.wifiSsid[i], sealed);
        n += k > 0 ? k : 0;
    }
    text[n < 12000 ? n : 11999] = 0;
    static char out[36000];
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

// ---- GPS: VestaCore (core/vgps.h) - the unit's NMEA port, shared with OSMapsCE: the first program reads it, the
// others take its data from shared memory ("VestaGPS"). The same stream carries the CAN module's '@' lines (car
// speed, steering angle, its version), parsed there too. Here: the monitor's sentence table and can.log, fed by
// the module's taps. The clock, the weather's place, the speed widget and the cameras use the state.
// PC test: gps.port=track.nmea plays a recording from the exe folder.
#include "vgps.h"
#include "vtask.h"   // VestaCore: the task manager's processes, windows and load
#define WM_APP_GPS (WM_APP + 10)
#define WM_APP_WX  (WM_APP + 11)
typedef GpsState Gps;

// The stream (G3NavHMI: rn2::CanMessageParser on COM4, a copy of COM3, 115200): '$' = NMEA, '@' = the CAN
// module ("@,<speed km/h>,<version>*hh", "@ANG,<steering angle | null>,*hh"). The monitor counts every type and
// can record the stream (can.log next to the exe): raw bytes while this program reads the port, the checked
// lines while another one does.
struct Sentence {
    char  name[12];              // "$GPRMC", "@", "@ANG"...
    DWORD count, tick;
    char  last[100];             // the last one, cut
};
enum { kSentMax = 24, kCanLogMax = 2 * 1024 * 1024 };
static Sentence s_sent[kSentMax];  // under s_gpsCs
static int      s_sentCount;
static DWORD    s_streamBin;                 // bytes outside printable ASCII and CR/LF (while we read the port)
static volatile DWORD s_rawTick;             // when the module last handed raw bytes (we read the port)
static HANDLE   s_canLog = INVALID_HANDLE_VALUE;   // the stream's record, opened by the monitor
static DWORD    s_canLogBytes;
static int      s_canMarks;
static CRITICAL_SECTION s_gpsCs;           // the table and can.log

static bool GpsFresh(const Gps& g, DWORD ms)
{
    return g.valid && GetTickCount() - g.fixTick < ms;
}

static bool GpsHasFix()
{
    Gps g;
    GpsGet(&g);
    return GpsFresh(g, 5000);
}

// Counts a sentence by its first field.
static void NoteSentence(const char* s)
{
    char name[12];
    int n = 0;
    while (s[n] && s[n] != ',' && s[n] != '*' && n < 11) {
        name[n] = (unsigned char)s[n] < 0x20 || (unsigned char)s[n] > 0x7E ? '?' : s[n];
        n++;
    }
    name[n] = 0;
    EnterCriticalSection(&s_gpsCs);
    int i = 0;
    while (i < s_sentCount && strcmp(s_sent[i].name, name))
        i++;
    if (i == s_sentCount && s_sentCount < kSentMax) {
        memset(&s_sent[i], 0, sizeof(Sentence));
        strcpy(s_sent[i].name, name);
        s_sentCount++;
    }
    if (i < s_sentCount) {
        s_sent[i].count++;
        s_sent[i].tick = GetTickCount();
        int k = 0;
        for (; s[k] && k < (int)sizeof(s_sent[i].last) - 1; k++)
            s_sent[i].last[k] = (unsigned char)s[k] < 0x20 || (unsigned char)s[k] > 0x7E ? '.' : s[k];
        s_sent[i].last[k] = 0;
    }
    LeaveCriticalSection(&s_gpsCs);
}

// The stream into can.log, up to 2 MB: the material for decoding what the CAN module sends. Only changes: a line
// the same as the last written one of its type (the text before the first comma) is left out - "@ANG" alone comes
// ~100 times a second. Marks ("#MARK n uptime") are put in from the monitors when something is done in the car.
struct CanLast {
    char key[12];
    char line[100];
};
static CanLast  s_canLast[kSentMax];   // under s_gpsCs
static int      s_canLastCount;
static char     s_canPart[128];        // a raw line not finished yet
static int      s_canPartLen;

static void CanLogWrite(const char* buf, DWORD len)
{
    EnterCriticalSection(&s_gpsCs);
    if (s_canLog != INVALID_HANDLE_VALUE) {
        DWORD w = 0;
        if (s_canLogBytes + len > kCanLogMax || !WriteFile(s_canLog, buf, len, &w, NULL)) {
            CloseHandle(s_canLog);
            s_canLog = INVALID_HANDLE_VALUE;
            VLog(L"can.log: закрыт, %u байт", (unsigned)s_canLogBytes);
        } else {
            s_canLogBytes += w;
        }
    }
    LeaveCriticalSection(&s_gpsCs);
}

// One line (no CR/LF) into can.log, unless it repeats the last one of its type.
static void CanLogLine(const char* s, int len)
{
    char key[12];
    int n = 0;
    while (n < len && s[n] != ',' && s[n] != '*' && n < 11) {
        key[n] = s[n];
        n++;
    }
    key[n] = 0;
    EnterCriticalSection(&s_gpsCs);
    if (s_canLog != INVALID_HANDLE_VALUE && len > 0) {
        int i = 0;
        while (i < s_canLastCount && strcmp(s_canLast[i].key, key))
            i++;
        if (i == s_canLastCount && s_canLastCount < kSentMax) {
            strcpy(s_canLast[i].key, key);
            s_canLast[i].line[0] = 0;
            s_canLastCount++;
        }
        bool same = false;
        if (i < s_canLastCount) {
            CanLast& c = s_canLast[i];
            same = len < (int)sizeof(c.line) && (int)strlen(c.line) == len && !memcmp(c.line, s, len);
            if (!same) {
                int k = len < (int)sizeof(c.line) - 1 ? len : (int)sizeof(c.line) - 1;
                memcpy(c.line, s, k);
                c.line[k] = 0;
            }
        }
        if (!same) {
            CanLogWrite(s, (DWORD)len);
            CanLogWrite("\r\n", 2);
        }
    }
    LeaveCriticalSection(&s_gpsCs);
}

// Raw bytes (we read the port): cut into lines for CanLogLine.
static void CanLogRaw(const char* buf, DWORD len)
{
    EnterCriticalSection(&s_gpsCs);
    if (s_canLog != INVALID_HANDLE_VALUE) {
        for (DWORD i = 0; i < len; i++) {
            char c = buf[i];
            if (c == '\n' || c == '\r') {
                CanLogLine(s_canPart, s_canPartLen);
                s_canPartLen = 0;
                continue;
            }
            if (s_canPartLen == (int)sizeof(s_canPart)) {   // too long for a line of the module: as it is
                CanLogLine(s_canPart, s_canPartLen);
                s_canPartLen = 0;
            }
            s_canPart[s_canPartLen++] = c;
        }
    }
    LeaveCriticalSection(&s_gpsCs);
}

static void CanLogToggle()
{
    EnterCriticalSection(&s_gpsCs);
    if (s_canLog != INVALID_HANDLE_VALUE) {
        CloseHandle(s_canLog);
        s_canLog = INVALID_HANDLE_VALUE;
        _snwprintf(s_status, 96, L"запись остановлена: can.log, %u КБ", (unsigned)(s_canLogBytes / 1024));
    } else {
        wchar_t path[MAX_PATH];
        if (!LogDir(path, s_dir)) {   // records go only to the SD card / USB disk
            wcscpy(s_status, L"нет SD-карты или USB-диска для записи");
            LeaveCriticalSection(&s_gpsCs);
            return;
        }
        wcscat(path, L"can.log");
        s_canLog = CreateFile(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        s_canLogBytes = 0;
        s_canMarks = 0;
        s_canLastCount = 0;
        s_canPartLen = 0;
        wcscpy(s_status, s_canLog != INVALID_HANDLE_VALUE ? L"запись потока в can.log" : L"can.log не создаётся");
    }
    LeaveCriticalSection(&s_gpsCs);
    VLog(L"can.log: %s", s_canLog != INVALID_HANDLE_VALUE ? L"запись" : L"стоп");
}

// The module's taps (its thread): raw bytes while this program reads the port, every checked line always.
static void GpsTapRaw(const char* buf, DWORD len)
{
    s_rawTick = GetTickCount();
    for (DWORD i = 0; i < len; i++) {
        unsigned char c = (unsigned char)buf[i];
        if (c > 0x7E || (c < 0x20 && c != '\r' && c != '\n'))
            s_streamBin++;
    }
    CanLogRaw(buf, len);
}

static void GpsTapLine(const char* line)
{
    NoteSentence(line);
    if (GetTickCount() - s_rawTick > 2000)   // another program reads the port: record its lines
        CanLogLine(line, (int)strlen(line));
}

static void GpsLogLine(const char* line)
{
    VLog(L"%S", line);
}

static void GpsStartCore()
{
    InitializeCriticalSection(&s_gpsCs);
    GpsConfig c;
    memset(&c, 0, sizeof(c));
    c.notify = s_wnd;
    c.msg = WM_APP_GPS;
    c.port = s_cfg.gpsPort;
    c.baud = s_cfg.gpsBaud;
    c.app = L"VestaHome";
    c.log = GpsLogLine;
    c.exeDir = s_dir;
    c.onLine = GpsTapLine;
    c.onRaw = GpsTapRaw;
    GpsStart(&c);
}

static void GpsReopen()
{
    GpsConfigure(s_cfg.gpsPort, s_cfg.gpsBaud);
}

static void GpsStopThread()
{
    GpsStop();
    if (s_canLog != INVALID_HANDLE_VALUE)
        CanLogToggle();   // close the record
}

// One line for the settings and the speed widget: what the port gives.
// brief: a short form for a row with a control on the right (no port name, no coordinates).
static void GpsStatusText(wchar_t* s, int n, bool brief)
{
    Gps g;
    GpsGet(&g);
    if (g.status == GPS_BUSY)
        _snwprintf(s, n, brief ? L"занят" : L"%s занят", g.port);
    else if (g.status != GPS_OPEN)
        _snwprintf(s, n, brief ? L"не открывается" : L"%s не открывается", g.port[0] ? g.port : s_cfg.gpsPort);
    else if (!g.lines || GetTickCount() - g.dataTick > 10000)
        wcsncpy(s, brief ? L"нет NMEA" : L"нет NMEA: выдачу включает навигация", n);
    else if (!GpsFresh(g, 5000))
        _snwprintf(s, n, brief ? L"нет фикса · спутников %d" : L"NMEA есть, нет фикса · спутников %d", g.sats);
    else if (brief)
        _snwprintf(s, n, L"фикс · спутников %d", g.sats);
    else
        _snwprintf(s, n, L"фикс · спутников %d · %.5f, %.5f", g.sats, g.lat, g.lon);
    s[n - 1] = 0;
    if (!g.owner && g.ownerApp[0]) {   // the port is read by another program (VestaCore shared memory)
        int l = (int)wcslen(s);
        _snwprintf(s + l, n - l, L" · через %s", g.ownerApp);
        s[n - 1] = 0;
    }
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

static bool McuSend6(const char* name, int a, int b, int c, int d, int e, int f);

// Sets the clock and gives it to the MCU too: at boot the unit's time comes from the MCU's clock, so a time set
// only with SetLocalTime was gone after a reboot (2026-10-17). As the stock shell does it (CSetupTimeDlg,
// Ghidra): SPApi_SetClockTime(hour, minute, second, year % 100, month, day) = MCU message 0x3C01.
static void ClockSet(const SYSTEMTIME& t)
{
    SetLocalTime(&t);
    McuSend6("SPApi_SetClockTime", t.wHour, t.wMinute, t.wSecond, t.wYear % 100, t.wMonth, t.wDay);
}

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
        ClockSet(gt);
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
        ClockSet(t);
    VLog(L"часовой пояс: %+d -> %+d мин", s_cfg.tzMin, m);
    s_cfg.tzMin = m;
    s_timeSynced = false;
    SaveCfg();
}

// The clock by hand ("General"): the arrows step the hour or the minute within the day, seconds to 0.
static void ClockStep(bool hour, int dir)
{
    SYSTEMTIME t;
    GetLocalTime(&t);
    if (hour)
        t.wHour = (WORD)((t.wHour + dir + 24) % 24);
    else
        t.wMinute = (WORD)((t.wMinute + dir + 60) % 60);
    t.wSecond = 0;
    t.wMilliseconds = 0;
    ClockSet(t);
    VLog(L"часы вручную: %02u:%02u", t.wHour, t.wMinute);
}

// ---- MCU: what the head unit's microcontroller sends the shell (climate, keys, versions...), seen through our
// SPIPowerMgr proxy (VestaHome/SPIPower): it copies every inbound message into a named shared ring. Layout =
// SPIPower/native/mcushm.h (keep both identical). Without the proxy the ring stays empty (magic 0).
#define WM_APP_MCU (WM_APP + 20)
#define MCU_SHM_NAME   L"VestaMCU2"   // v2: 40 data bytes
#define MCU_SHM_MAGIC  0x55434D56
#define MCU_SHM_SLOTS  256
#define MCU_SHM_DATA   40   // v2, as SPIPower/native/mcushm.h

struct McuMsg {
    volatile LONG  seq;
    DWORD          tick;
    unsigned short id;
    unsigned char  len, skel;
    unsigned char  data[MCU_SHM_DATA];
};
struct McuShm {
    DWORD         magic, version;
    volatile LONG seq, attach, wraps;
    DWORD         reserved[3];
    McuMsg        msg[MCU_SHM_SLOTS];
};

// One line per message id for the monitor.
struct McuStat {
    unsigned short id;
    unsigned char  len, skel;
    DWORD          count, tick;
    unsigned char  data[MCU_SHM_DATA];
};
enum { kMcuStatMax = 96, kMcuLogMax = 4 * 1024 * 1024 };

// The climate as G3NavHMI decodes it (CHvac::MsgDecode, PROJECT.md): value = payload[1]; 0x6109 = kind
// payload[1] (1 outside, 2 right zone, 3 driver) with the value in payload[3]. -1 = not seen yet.
struct Hvac {
    int   outer;          // °C (raw - 40)
    int   temp;           // the driver's knob, 0..16
    int   blower;         // 1..7
    int   mode;           // air direction 0..4
    int   ac, autoMode, rear, cycle, power;
    DWORD tick;           // the last climate message
};

static CRITICAL_SECTION s_mcuCs;
static McuStat  s_mcuStat[kMcuStatMax];
static int      s_mcuStatCount;
static DWORD    s_mcuTotal;              // messages taken from the ring since start
static McuShm*  s_shm;
static HANDLE   s_shmMap;
static HANDLE   s_mcuThread;
static volatile bool s_mcuStop;
static HANDLE   s_mcuLog = INVALID_HANDLE_VALUE;
static DWORD    s_mcuLogBytes;
static Hvac     s_hvac = { -999, -1, -1, -1, -1, -1, -1, -1, -1, 0 };
static volatile LONG s_mcuPosted;

// The sound as the MCU reports it (mcu.log 2026-10-17, PROJECT.md): 0x3101 00 00 <n> = the volume of the current
// source (the stock shell's scale is 0..30 - user 2026-10-19; up to 50 taken), 0x3109 00 <0/1> = mute, 0x4904 00 <n> = the
// source (00 radio, 06 and 07 the media player - USB, SD, video alike (2026-10-18), 09 Bluetooth). -1 = not seen yet. A change of the volume or the mute opens
// the volume window (the same values sent again - the tuner repeats them - do not).
enum { kVolMax = 50, WP_MCU_VOLUME = 1, WP_MCU_MARK = 2, WP_MCU_TUNER = 3, WP_MCU_REBOOT = 4, WP_MCU_SOUND = 5,
       WP_MCU_DAYNIGHT = 6 };
// The screen (2026-10-17 photos + Ghidra on the shell's CSetUpDisplayDlg 2026-10-18): 0x4105 00 <0 day, 1 night>,
// 0x4107 00 <backlight 1..10> (10 by day, 1 at night as set); the shell's day / night buttons are keys 0x64 / 0x65,
// its brightness slider SPApi_SysSendRadioBackLight(0..10) (the current mode's level).
// The sound settings as the MCU reports them (SPIPower.log 2026-10-18, also after the stock shell's changes):
// 0x3102 / 0x3103 / 0x3104 00 <0..12> = low / mid / high (6 = 0), 0x3105 00 <balance> <fader> (8 = the centre).
struct McuSound { int eq[3], bal, fad; };   // VestaHome's units (-6..6, -8..8); 99 = not seen
static McuSound s_mcuSnd = { { 99, 99, 99 }, 99, 99 };
static int      s_vol = -1, s_mute = -1, s_src = -1;
static volatile LONG s_volPosted;

static Tuner s_tun;

// "About the system -> Details" from the MCU (mcu.log, the engineering menu's photos; PROJECT.md): 0x7902 00 <00 SW |
// 03 HW> 00 00 <len> <text>, 0x7902 00 00 01 <CS hi lo>; 0x7922 00 06 <BT address>; the machine ID = 0x4118
// 00 <4 bytes> + 0x4111 00 20 <3 bytes> + 0x4112 00 <text>; System status 0x7905 00 <sub> <value> (5 bytes:
// one byte - 0A navigation board °C, 06 amplifier °C, 07 the tuner's field dB; 4 bytes: two - 03 / 0E the
// steering wheel buttons' voltages, as the shell shows them).
struct McuInfo {
    wchar_t sw[40], hw[16], id[16], idDate[8], idNum[12];
    int     cs;                 // -1 not seen
    BYTE    bt[6];
    bool    btHave;
    int     sys[16];            // by sub, -1 not seen
};
static McuInfo s_info;

static bool McuBtAddr(wchar_t* s, int n)   // the unit's Bluetooth address, "AA:BB:…"
{
    EnterCriticalSection(&s_mcuCs);
    bool have = s_info.btHave;
    if (have)
        _snwprintf(s, n, L"%02X:%02X:%02X:%02X:%02X:%02X", s_info.bt[0], s_info.bt[1], s_info.bt[2], s_info.bt[3],
                   s_info.bt[4], s_info.bt[5]);
    LeaveCriticalSection(&s_mcuCs);
    return have;
}
static void McuText(const unsigned char* p, int n, wchar_t* out, bool lead);

static void McuInfoTake(const McuMsg& m)   // under s_mcuCs
{
    const unsigned char* p = m.data;
    int len = m.len < MCU_SHM_DATA ? m.len : MCU_SHM_DATA;
    if (m.id == 0x7902 && len >= 5) {
        if (p[2] == 1)
            s_info.cs = p[3] << 8 | p[4];
        else if (len > 5) {
            wchar_t* out = p[1] == 3 ? s_info.hw : s_info.sw;
            int cap = p[1] == 3 ? 16 : 40, k = 0;
            for (int i = 5; i < len && k < cap - 1 && p[i] >= 0x20 && p[i] < 0x7F; i++)
                out[k++] = p[i];
            while (k > 0 && out[k - 1] == L'.')
                k--;
            out[k] = 0;
        }
    } else if (m.id == 0x7922 && len >= 8 && p[1] == 6) {
        memcpy(s_info.bt, p + 2, 6);
        s_info.btHave = true;
    } else if (m.id == 0x4118 && len >= 5) {
        _snwprintf(s_info.id, 16, L"%02X%02X%02X%02X", p[1], p[2], p[3], p[4]);
    } else if (m.id == 0x4111 && len >= 5) {
        _snwprintf(s_info.idDate, 8, L"%02X%02X%02X", p[2], p[3], p[4]);
    } else if (m.id == 0x4112 && len >= 3) {
        McuText(p + 1, len - 1 < 11 ? len - 1 : 11, s_info.idNum, true);
    } else if (m.id == 0x7905 && len >= 4 && p[1] < 16) {
        s_info.sys[p[1]] = len == 4 ? p[2] << 8 | p[3] : p[2];
    }
}

static bool s_mcuCsReady;   // McuStart has made s_mcuCs (SaveCfg may run before it)

// The radio's last state in the ini (the MCU sends it only on a change or when asked, HU 2026-10-18-2: VestaHome
// started after the shell showed nothing until the radio screen asked) and the stations' names: from the MCU's list
// and learned from RDS while listening - for the presets, which the MCU sends as frequencies only.
enum { kRadioNames = 48 };
static struct { int f10; wchar_t name[9]; } s_rnames[kRadioNames];
static int s_rnameCount;
static bool s_radioDirty;   // to be written (at most every 30 s)
static bool  s_cfgSoon;      // the ini to be written a minute after the first change not written (CfgSoon):
static DWORD s_cfgSoonTick;  // at most once a minute, and only when something changed

static void CfgSoon()
{
    if (!s_cfgSoon) {
        s_cfgSoon = true;
        s_cfgSoonTick = GetTickCount();
    }
}

static const wchar_t* RadioName(const Tuner& t, int f10)   // "" when not known
{
    for (int i = 0; i < t.listCount && i < kTunList; i++)
        if (t.list[i] == f10 && t.listName[i][0])
            return t.listName[i];
    for (int i = 0; i < s_rnameCount; i++)
        if (s_rnames[i].f10 == f10)
            return s_rnames[i].name;
    return L"";
}

static void RadioNameLearn(int f10, const wchar_t* ps)
{
    if (f10 <= 0 || !ps[0])
        return;
    int i = 0;
    while (i < s_rnameCount && s_rnames[i].f10 != f10)
        i++;
    if (i == s_rnameCount) {
        if (s_rnameCount == kRadioNames)
            i = s_rnameCount - 1;   // full: the last one goes
        else
            s_rnameCount++;
    } else if (!wcscmp(s_rnames[i].name, ps)) {
        return;
    }
    s_rnames[i].f10 = f10;
    wcsncpy(s_rnames[i].name, ps, 8);
    s_rnames[i].name[8] = 0;
    s_radioDirty = true;
}

static void RadioCacheLoad(const wchar_t* k, const wchar_t* v)
{
    if (!wcscmp(k, L"radio.cache")) {   // f10,band,cur,presets,p0..p23
        int a[28], n = 0;
        const wchar_t* q = v;
        while (n < 28 && *q) {
            a[n++] = _wtoi(q);
            while (*q && *q != L',')
                q++;
            if (*q)
                q++;
        }
        if (n >= 4) {
            s_tun.f10 = a[0];
            s_tun.band = a[1];
            s_tun.cur = a[2];
            s_tun.presets = a[3];
            for (int i = 0; i < 24 && 4 + i < n; i++)
                s_tun.preset[i] = a[4 + i];
        }
    } else if (!wcscmp(k, L"radio.cvol")) {   // volume, mute, source: the last ones the MCU sent
        int a[3] = { -1, -1, -1 }, n = 0;
        const wchar_t* q = v;
        while (n < 3 && *q) {
            a[n++] = _wtoi(q);
            while (*q && *q != L',')
                q++;
            if (*q)
                q++;
        }
        if (n == 3 && a[0] >= 0 && a[0] <= kVolMax) {
            s_vol = a[0];
            s_mute = a[1];
            s_src = a[2];
        }
    } else if (!wcscmp(k, L"radio.cset")) {   // known bits, value bits
        int known = _wtoi(v), val = 0;
        const wchar_t* c = wcschr(v, L',');
        if (c)
            val = _wtoi(c + 1);
        s_tun.setKnown = known;
        for (int i = 0; i < RSET_COUNT; i++)
            s_tun.set[i] = val >> i & 1;
    } else if (!wcscmp(k, L"radio.names")) {   // f10=NAME;...
        const wchar_t* q = v;
        while (*q && s_rnameCount < kRadioNames) {
            int f = _wtoi(q);
            const wchar_t* e = wcschr(q, L'=');
            if (!e)
                break;
            e++;
            int k2 = 0;
            wchar_t nm[9];
            while (e[k2] && e[k2] != L';' && k2 < 8) {
                nm[k2] = e[k2];
                k2++;
            }
            nm[k2] = 0;
            if (f > 0 && nm[0]) {
                s_rnames[s_rnameCount].f10 = f;
                wcscpy(s_rnames[s_rnameCount].name, nm);
                s_rnameCount++;
            }
            q = wcschr(e, L';');
            if (!q)
                break;
            q++;
        }
    }
}

static int RadioCacheText(wchar_t* out, int cap)
{
    Tuner t;
    if (s_mcuCsReady)
        EnterCriticalSection(&s_mcuCs);
    t = s_tun;
    if (s_mcuCsReady)
        LeaveCriticalSection(&s_mcuCs);
    int n = _snwprintf(out, cap, L"# radio.*: the tuner's last state and the stations' names (VestaHome's cache)\r\n"
                       L"radio.cache=%d,%d,%d,%d", t.f10, t.band, t.cur, t.presets);
    if (n < 0)
        return 0;
    for (int i = 0; i < 24 && n < cap - 16; i++)
        n += _snwprintf(out + n, cap - n, L",%d", t.preset[i]);
    int val = 0;
    for (int i = 0; i < RSET_COUNT; i++)
        val |= (t.set[i] ? 1 : 0) << i;
    int k = _snwprintf(out + n, cap - n, L"\r\nradio.cvol=%d,%d,%d\r\nradio.cset=%d,%d\r\nradio.names=", s_vol, s_mute,
                       s_src, t.setKnown, val);
    n += k > 0 ? k : 0;
    for (int i = 0; i < s_rnameCount && n < cap - 32; i++) {
        k = _snwprintf(out + n, cap - n, L"%d=%s;", s_rnames[i].f10, s_rnames[i].name);
        n += k > 0 ? k : 0;
    }
    k = _snwprintf(out + n, cap - n, L"\r\n");
    n += k > 0 ? k : 0;
    return n < cap ? n : cap - 1;
}

static void TunerSet(int i, int v)   // under s_mcuCs
{
    s_tun.set[i] = v != 0;
    s_tun.setKnown |= 1 << i;
}
static volatile LONG s_tunPosted;

// The MCU's ASCII, trailing spaces cut; lead = the leading ones too (the RDS name is centred in its 8 characters;
// a RadioText part is not: its second half may start with the space between two words).
static void McuText(const unsigned char* p, int n, wchar_t* out, bool lead)
{
    int k;
    while (lead && n > 0 && *p == ' ') {
        p++;
        n--;
    }
    for (k = 0; k < n; k++)
        out[k] = p[k] >= 0x20 && p[k] < 0x7F ? (wchar_t)p[k] : L' ';
    while (k > 0 && out[k - 1] == L' ')
        k--;
    out[k] = 0;
}

static void McuLogLine(const char* s)   // under s_mcuCs
{
    if (s_mcuLog == INVALID_HANDLE_VALUE)
        return;
    DWORD len = (DWORD)strlen(s), w = 0;
    if (s_mcuLogBytes + len > kMcuLogMax || !WriteFile(s_mcuLog, s, len, &w, NULL)) {
        CloseHandle(s_mcuLog);
        s_mcuLog = INVALID_HANDLE_VALUE;
        return;
    }
    s_mcuLogBytes += w;
}

// One message: the per-id table, the climate, the record ("<uptime s> <id> s<skeleton> <len> : hex |text|").
static void McuTake(const McuMsg& m)
{
    const unsigned char* p = m.data;
    bool hvac = false;
    EnterCriticalSection(&s_mcuCs);
    s_mcuTotal++;
    int i = 0;
    while (i < s_mcuStatCount && s_mcuStat[i].id != m.id)
        i++;
    if (i == s_mcuStatCount && s_mcuStatCount < kMcuStatMax) {
        memset(&s_mcuStat[i], 0, sizeof(McuStat));
        s_mcuStat[i].id = m.id;
        s_mcuStatCount++;
    }
    if (i < s_mcuStatCount) {
        McuStat& st = s_mcuStat[i];
        st.count++;
        st.tick = m.tick;
        st.len = m.len;
        st.skel = m.skel;
        memcpy(st.data, m.data, MCU_SHM_DATA);
    }
    if ((m.id >> 8) == 0x61) {
        hvac = true;
        switch (m.id) {
        case 0x6109:
            if (p[1] == 1) s_hvac.outer = p[3] - 40;
            else if (p[1] == 3) s_hvac.temp = p[3];
            break;
        case 0x610C: s_hvac.blower = p[1]; break;
        case 0x610D: s_hvac.mode = p[1]; break;
        case 0x610B: s_hvac.ac = p[1]; break;
        case 0x610A: s_hvac.autoMode = p[1]; break;
        case 0x6107: s_hvac.rear = p[1]; break;
        case 0x610F: s_hvac.cycle = p[1]; break;
        case 0x6106: s_hvac.power = p[1]; break;
        default: hvac = false;
        }
        if (hvac)
            s_hvac.tick = GetTickCount();
    }
    bool volume = false, mark = false;
    if (m.id == 0x3101 && p[1] == 0 && p[2] <= kVolMax) {
        volume = s_vol >= 0 && s_vol != p[2];
        if (s_vol != p[2])
            s_radioDirty = true;   // the cache in the ini
        s_vol = p[2];
    } else if (m.id == 0x3109) {
        volume = s_mute >= 0 && s_mute != (p[1] != 0);
        mark = s_mute == 0 && p[1] != 0;   // MUTE pressed: a mark in the records (the hardware key - an on-screen
                                           // MARK would change the screen being tested; 2026-10-17)
        s_mute = p[1] != 0;
    } else if (m.id == 0x4904) {
        s_src = p[1];
    }
    bool dayNight = false;
    if (m.id == 0x4105 && p[1] <= 1) {
        dayNight = s_mcuNight != p[1];
        s_mcuNight = p[1];
    } else if (m.id == 0x4107 && p[1] <= 10) {
        dayNight = s_bright != p[1];
        s_bright = p[1];
    }
    bool sound = false;
    if (m.id >= 0x3102 && m.id <= 0x3104 && p[1] <= 12) {
        sound = s_mcuSnd.eq[m.id - 0x3102] != p[1] - 6;
        s_mcuSnd.eq[m.id - 0x3102] = p[1] - 6;
    } else if (m.id == 0x3105 && p[1] <= 16 && p[2] <= 16) {
        sound = s_mcuSnd.bal != p[1] - 8 || s_mcuSnd.fad != p[2] - 8;
        s_mcuSnd.bal = p[1] - 8;
        s_mcuSnd.fad = 8 - p[2];   // (the MCU's fader is ours reversed: BalanceSend)
    }
    static bool s_infoInit;
    if (!s_infoInit) {
        s_infoInit = true;
        s_info.cs = -1;
        for (int k = 0; k < 16; k++)
            s_info.sys[k] = -1;
    }
    McuInfoTake(m);
    bool tuner = true;
    switch (m.id) {
    case 0x0901: {
        int f = p[2] << 8 | p[3];
        if (f != s_tun.f10) {   // another station: its RDS name and text are not the old one's
            s_tun.ps[0] = 0;
            s_tun.rt[0][0] = s_tun.rt[1][0] = 0;
            s_tun.fTick = GetTickCount();
        }
        s_tun.f10 = f;
        s_tun.band = p[1] <= 2 ? p[1] : 0;
        s_tunLive = true;
        s_tun0901++;
        break;
    }
    case 0x0904: if (p[1] < 8) s_tun.flags[p[1]] = p[2]; break;
    case 0x0930:   // station names, 4 a message
        for (int k = 0; k < 4 && p[1] + k <= p[2] && p[1] + k < kTunList; k++)
            McuText(p + 4 + k * 8, 8, s_tun.listName[p[1] + k], true);
        break;
    case 0x0907: s_tun.cur = p[2] == 0xFF ? -1 : p[2]; break;
    case 0x0908:
        if (p[1] == 0 && p[3] < kTunList && p[4] <= kTunList) {   // the station list
            s_tun.listCount = p[4];
            for (int k = 0; k < 8 && p[2] + k < kTunList; k++)
                s_tun.list[p[2] + k] = p[5 + 2 * k] << 8 | p[6 + 2 * k];
        }
        if (p[1] == 1 && p[3] < 24 && p[4] <= 24) {
            s_tun.presets = p[4];
            for (int k = 0; k < 8 && p[2] + k < 24; k++)
                s_tun.preset[p[2] + k] = p[5 + 2 * k] << 8 | p[6 + 2 * k];
        }
        break;
    case 0x0909:   // a preset saved: 00 01 <preset> <frequency BE> (no new 0x0908 follows - HU 2026-10-20)
        if (p[1] == 1 && p[2] < 24 && m.len >= 5)
            s_tun.preset[p[2]] = p[3] << 8 | p[4];
        break;
    case 0x090B: McuText(p + 3, 8, s_tun.ps, true); break;
    case 0x0931: if (p[3] < 2) McuText(p + 4, 32, s_tun.rt[p[3]], false); break;   // 2 x 32 = 64, RDS's RT
    // the radio's settings (SPIPower.log 2026-10-18: the reply to each settings key, and all of them to
    // the extended key 0x90): 0x0920 RDS, TA; 0x0921 -, AF; 0x0924 -, CT; 0x092D -, MS; 0x092E noise, MW
    case 0x0920: TunerSet(RSET_RDS, p[1]); TunerSet(RSET_TA, p[2]); break;
    case 0x0921: TunerSet(RSET_AF, p[2]); break;
    case 0x0924: TunerSet(RSET_CT, p[2]); break;
    case 0x092D: TunerSet(RSET_MS, p[2]); break;
    case 0x092E: TunerSet(RSET_NOISE, p[1]); TunerSet(RSET_MW, p[2]); break;
    default: tuner = false;
    }
    if (s_mcuLog != INVALID_HANDLE_VALUE) {
        char line[200];
        int n = _snprintf(line, 120, "%u.%03u %04X s%u %u :", (unsigned)(m.tick / 1000), (unsigned)(m.tick % 1000),
                          (unsigned)m.id, (unsigned)m.skel, (unsigned)m.len);
        int k, shown = m.len < MCU_SHM_DATA ? m.len : MCU_SHM_DATA;
        for (k = 0; k < shown && n < 180; k++)
            n += _snprintf(line + n, 200 - n, " %02X", p[k]);
        n += _snprintf(line + n, 200 - n, "  |");
        for (k = 0; k < shown && n < 196; k++)
            line[n++] = p[k] >= 0x20 && p[k] <= 0x7E ? (char)p[k] : '.';
        line[n++] = '|';
        line[n++] = '\r';
        line[n++] = '\n';
        line[n] = 0;
        McuLogLine(line);
    }
    LeaveCriticalSection(&s_mcuCs);
    if (hvac && !InterlockedExchange((LONG*)&s_mcuPosted, 1) && !PostMessage(s_wnd, WM_APP_MCU, 0, 0))
        s_mcuPosted = 0;
    if (volume && !InterlockedExchange((LONG*)&s_volPosted, 1) && !PostMessage(s_wnd, WM_APP_MCU, WP_MCU_VOLUME, 0))
        s_volPosted = 0;
    if (mark)
        PostMessage(s_wnd, WM_APP_MCU, WP_MCU_MARK, 0);
    if (sound)
        PostMessage(s_wnd, WM_APP_MCU, WP_MCU_SOUND, 0);
    if (dayNight)
        PostMessage(s_wnd, WM_APP_MCU, WP_MCU_DAYNIGHT, 0);
    if (m.id == 0x1102 && p[1] == 0x18)   // the knob held 5 s: the MCU reboots the unit in 5 s more (2026-10-18)
        PostMessage(s_wnd, WM_APP_MCU, WP_MCU_REBOOT, 0);
    if (tuner && !InterlockedExchange((LONG*)&s_tunPosted, 1) && !PostMessage(s_wnd, WM_APP_MCU, WP_MCU_TUNER, 0))
        s_tunPosted = 0;
}

static void TunerGet(Tuner* t)
{
    EnterCriticalSection(&s_mcuCs);
    *t = s_tun;
    LeaveCriticalSection(&s_mcuCs);
    s_tunPosted = 0;
}

// The volume, mute and source (see s_vol); false = no volume message yet.
static bool VolGet(int* vol, int* mute, int* src)
{
    EnterCriticalSection(&s_mcuCs);
    *vol = s_vol;
    *mute = s_mute > 0;
    *src = s_src;
    LeaveCriticalSection(&s_mcuCs);
    s_volPosted = 0;
    return *vol >= 0;
}

// Polls the ring 10 times a second. At the first look it takes what is there (up to 256 back): the climate and
// the versions sent at boot are in it.
static DWORD WINAPI McuThread(LPVOID)
{
    VLog(L"поток MCU: id %08X", (unsigned)GetCurrentThreadId());
    LONG seen = -1;
    DWORD lastTry = 0;
    while (!s_mcuStop) {
        if (!s_shm && GetTickCount() - lastTry > 2000) {
            lastTry = GetTickCount();
            s_shmMap = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(McuShm), MCU_SHM_NAME);
            if (s_shmMap)
                s_shm = (McuShm*)MapViewOfFile(s_shmMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(McuShm));
        }
        if (s_shm && s_shm->magic == MCU_SHM_MAGIC) {
            LONG seq = s_shm->seq;
            if (seen < 0 || seq < seen)
                seen = seq > MCU_SHM_SLOTS ? seq - MCU_SHM_SLOTS : 0;
            if (seq - seen > MCU_SHM_SLOTS)
                seen = seq - MCU_SHM_SLOTS;
            while (seen < seq) {
                LONG n = seen + 1;
                McuMsg m = s_shm->msg[(n - 1) & (MCU_SHM_SLOTS - 1)];
                if (m.seq == n)
                    McuTake(m);
                seen = n;
            }
        }
        Sleep(100);
    }
    return 0;
}

static void McuStart()
{
    InitializeCriticalSection(&s_mcuCs);
    s_mcuCsReady = true;
    s_mcuThread = CreateThread(NULL, 0, McuThread, NULL, 0, NULL);
}

static void McuStopThread()
{
    if (!s_mcuThread)
        return;
    s_mcuStop = true;
    WaitForSingleObject(s_mcuThread, 2000);
    EnterCriticalSection(&s_mcuCs);
    if (s_mcuLog != INVALID_HANDLE_VALUE) {
        CloseHandle(s_mcuLog);
        s_mcuLog = INVALID_HANDLE_VALUE;
    }
    LeaveCriticalSection(&s_mcuCs);
}

// 1 = the proxy is in and writes, 0 = the ring exists but no proxy ever wrote it, -1 = no ring.
static int McuState(LONG* attach, LONG* wraps)
{
    *attach = *wraps = 0;
    if (!s_shm)
        return -1;
    if (s_shm->magic != MCU_SHM_MAGIC)
        return 0;
    *attach = s_shm->attach;
    *wraps = s_shm->wraps;
    return 1;
}

static void McuLogToggle()
{
    EnterCriticalSection(&s_mcuCs);
    if (s_mcuLog != INVALID_HANDLE_VALUE) {
        CloseHandle(s_mcuLog);
        s_mcuLog = INVALID_HANDLE_VALUE;
        _snwprintf(s_status, 96, L"запись остановлена: mcu.log, %u КБ", (unsigned)(s_mcuLogBytes / 1024));
    } else {
        wchar_t path[MAX_PATH];
        if (!LogDir(path, s_dir)) {   // records go only to the SD card / USB disk
            wcscpy(s_status, L"нет SD-карты или USB-диска для записи");
            LeaveCriticalSection(&s_mcuCs);
            return;
        }
        wcscat(path, L"mcu.log");
        s_mcuLog = CreateFile(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        s_mcuLogBytes = 0;
        if (s_mcuLog != INVALID_HANDLE_VALUE) {
            const char* head = "# VestaHome mcu.log: <uptime s> <MCU id> s<skeleton> <len> : payload hex |text|\r\n";
            McuLogLine(head);
            // what is known so far: one line per id, so a short record still has every id in it
            for (int i = 0; i < s_mcuStatCount; i++) {
                char line[160];
                int n = _snprintf(line, 100, "# seen %04X x%u len %u :", (unsigned)s_mcuStat[i].id,
                                  (unsigned)s_mcuStat[i].count, (unsigned)s_mcuStat[i].len);
                for (int k = 0; k < 12 && k < s_mcuStat[i].len && n < 150; k++)
                    n += _snprintf(line + n, 160 - n, " %02X", s_mcuStat[i].data[k]);
                _snprintf(line + n, 160 - n, "\r\n");
                line[159] = 0;
                McuLogLine(line);
            }
        }
        wcscpy(s_status, s_mcuLog != INVALID_HANDLE_VALUE ? L"запись MCU в mcu.log" : L"mcu.log не создаётся");
    }
    LeaveCriticalSection(&s_mcuCs);
    VLog(L"mcu.log: %s", s_mcuLog != INVALID_HANDLE_VALUE ? L"запись" : L"стоп");
}

// One mark into every record that is on (can.log and mcu.log): what was just done in the car.
static int s_marks;

static void MarkLogs()
{
    bool can = s_canLog != INVALID_HANDLE_VALUE, mcu = s_mcuLog != INVALID_HANDLE_VALUE;
    if (!can && !mcu) {
        wcscpy(s_status, L"сначала включите запись");
        return;
    }
    char m[48];
    _snprintf(m, 48, "\r\n#MARK %d %u\r\n", ++s_marks, (unsigned)(GetTickCount() / 1000));
    m[47] = 0;
    if (can)
        CanLogWrite(m, (DWORD)strlen(m));
    if (mcu) {
        EnterCriticalSection(&s_mcuCs);
        McuLogLine(m + 2);
        LeaveCriticalSection(&s_mcuCs);
    }
    s_canMarks = s_marks;
    _snwprintf(s_status, 96, L"метка %d%s%s", s_marks, can ? L" · can.log" : L"", mcu ? L" · mcu.log" : L"");
}

static bool HvacGet(Hvac* h)
{
    EnterCriticalSection(&s_mcuCs);
    *h = s_hvac;
    LeaveCriticalSection(&s_mcuCs);
    s_mcuPosted = 0;
    return h->tick != 0;
}

// ---- MCU commands from this process, through SPIPowerMgr.dll as AppLauncher does it (it opens PIC1: for
// writing on the first send; reading stays with the shell). Arguments as G3NavHMI sends them (Ghidra, PROJECT.md):
// balance/fader = CAudioFieldDlg (x, y -8..8 -> x + 8, y + 8), equalizer = CAudioEffectDlg (band 0 high,
// 1 mid, 2 low; -6..6 -> + 6), product info / BT address = requests answered as MCU messages (to the monitor).
typedef void (*SpmFn0)(void);
typedef void (*SpmFn2)(unsigned int, unsigned int);
typedef void (*SpmFn6)(unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int);
static HMODULE s_spm;
static int     s_spmTried;

// The calls other than keys are made in the shell by our proxy (SPIPower/native/mcucmd.h, keep identical): from
// this process the audio ones never reached the MCU (no 0x3102.. replies, SPIPower.log 2026-10-17..20).
#define MCU_CMD_NAME   L"VestaMCUCmd"
#define MCU_CMD_EVENT  L"VestaMCUCmd_Ev"
#define MCU_CMD_MAGIC  0x44434D56
#define MCU_CMD_SLOTS  16
struct McuCall { char name[48]; unsigned int arg[6]; };
struct McuCmd {
    DWORD         magic;
    volatile LONG ready, seq, done;
    McuCall       call[MCU_CMD_SLOTS];
};

static bool McuViaShell(const char* name, int a, int b, int c, int d, int e, int f)
{
    static McuCmd* s_mc;
    static HANDLE  s_mcEv;
    if (!s_mc) {
        HANDLE m = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(McuCmd), MCU_CMD_NAME);
        if (m)
            s_mc = (McuCmd*)MapViewOfFile(m, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(McuCmd));
        s_mcEv = CreateEvent(NULL, FALSE, FALSE, MCU_CMD_EVENT);
    }
    if (!s_mc || s_mc->magic != MCU_CMD_MAGIC || !s_mc->ready || !s_mcEv)
        return false;   // an older proxy: from here, as before
    LONG n = s_mc->seq + 1;
    McuCall& k = s_mc->call[n % MCU_CMD_SLOTS];
    strncpy(k.name, name, sizeof(k.name) - 1);
    k.name[sizeof(k.name) - 1] = 0;
    unsigned int v[6] = { (unsigned)a & 0xFF, (unsigned)b & 0xFF, (unsigned)c & 0xFF, (unsigned)d & 0xFF,
                          (unsigned)e & 0xFF, (unsigned)f & 0xFF };
    memcpy(k.arg, v, sizeof(v));
    s_mc->seq = n;
    SetEvent(s_mcEv);
    VLog(L"mcu (через оболочку): %S(%d, %d, %d)", name, a, b, c);
    return true;
}

static bool McuIsKey(const char* name)
{
    return !strcmp(name, "SPApi_SendKeyEvent") || !strcmp(name, "SPApi_SendExtendKeyEvent");
}

static FARPROC SpmProc(const char* name)
{
#ifdef UNDER_CE
    if (!s_spmTried) {
        s_spmTried = 1;
        LoadLibrary(L"\\ResidentFlash\\navigation\\app\\NCL_BaseCom.dll");   // its dependency, by full path
        s_spm = LoadLibrary(L"\\ResidentFlash\\navigation\\app\\SPIPowerMgr.dll");
        VLog(L"mcu: SPIPowerMgr.dll %s", s_spm ? L"загружена" : L"не грузится");
    }
    if (!s_spm)
        return NULL;
    wchar_t w[64];
    MultiByteToWideChar(CP_ACP, 0, name, -1, w, 64);
    return GetProcAddress(s_spm, w);
#else
    (void)name;
    return NULL;
#endif
}

static bool McuSend2(const char* name, int a, int b)
{
    if (!McuIsKey(name) && McuViaShell(name, a, b, 0, 0, 0, 0))
        return true;
    SpmFn2 f = (SpmFn2)SpmProc(name);
    if (!f) {
        wcscpy(s_status, L"MCU: только на ГУ (нет SPIPowerMgr.dll)");
        return false;
    }
    f((unsigned)a & 0xFF, (unsigned)b & 0xFF);
    VLog(L"mcu: %S(%d, %d)", name, a, b);
    return true;
}

static bool McuSend3(const char* name, int a, int b, int c)
{
    if (!McuIsKey(name) && McuViaShell(name, a, b, c, 0, 0, 0))
        return true;
    SpmFn6 fn = (SpmFn6)SpmProc(name);   // three byte arguments in r0..r2; the rest is not read
    if (!fn)
        return false;
    fn(a & 0xFF, b & 0xFF, c & 0xFF, 0, 0, 0);
    VLog(L"mcu: %S(%d, %d, %d)", name, a, b, c);
    return true;
}

static bool McuSend6(const char* name, int a, int b, int c, int d, int e, int f)
{
    if (McuViaShell(name, a, b, c, d, e, f))
        return true;
    SpmFn6 fn = (SpmFn6)SpmProc(name);
    if (!fn)
        return false;   // the PC, or no SPIPowerMgr: only the local clock
    fn(a & 0xFF, b & 0xFF, c & 0xFF, d & 0xFF, e & 0xFF, f & 0xFF);
    VLog(L"mcu: %S(%d, %d, %d, %d, %d, %d)", name, a, b, c, d, e, f);
    return true;
}

static bool McuSend0(const char* name)
{
    SpmFn0 f = (SpmFn0)SpmProc(name);
    if (!f) {
        wcscpy(s_status, L"MCU: только на ГУ (нет SPIPowerMgr.dll)");
        return false;
    }
    f();
    VLog(L"mcu: %S()", name);
    return true;
}

static void Go(Screen s, bool push);
static void Back();

// ---- player: local music through BASS 2.4.12 (un4seen; bass.dll and its plugins for CE/ARM, from the
// SinglePlayer package - MAIN/SinglePlayer), loaded at run time from bass\ next to the exe, so the exe still
// imports COREDLL (+ WS2) only. The shell's GStreamer (GstProbe) had no AAC and no ICY; BASS plays MP3/MP2/OGG/WAV
// itself, AAC/M4A, FLAC, ALAC and MPC through plugins. The sound goes out through waveOut like any program's:
// switching the MCU's source radio <-> media is not done yet. Playlist = the audio files of the track's folder.
#define WM_APP_PLAYER (WM_APP + 21)
typedef unsigned __int64 QWORD;
typedef BOOL  (WINAPI *BassInitFn)(int, DWORD, DWORD, HWND, const void*);
typedef DWORD (WINAPI *BassPluginLoadFn)(const void*, DWORD);
typedef DWORD (WINAPI *BassStreamFileFn)(BOOL, const void*, QWORD, QWORD, DWORD);
typedef BOOL  (WINAPI *BassChanFn)(DWORD);
typedef BOOL  (WINAPI *BassPlayFn)(DWORD, BOOL);
typedef DWORD (WINAPI *BassActiveFn)(DWORD);
typedef QWORD (WINAPI *BassLenFn)(DWORD, DWORD);
typedef BOOL  (WINAPI *BassSetPosFn)(DWORD, QWORD, DWORD);
typedef double (WINAPI *BassB2SFn)(DWORD, QWORD);
typedef void  (CALLBACK *BassSyncProc)(DWORD, DWORD, DWORD, void*);
typedef DWORD (WINAPI *BassSyncFn)(DWORD, DWORD, QWORD, BassSyncProc, void*);
typedef int   (WINAPI *BassErrFn)(void);
typedef const char* (WINAPI *TagsReadFn)(DWORD, const char*);
// a "file" read by callbacks (the internet radio: BASS_StreamCreateFileUser, BASS_FILEPROCS)
typedef void  (CALLBACK *BassFileCloseProc)(void*);
typedef QWORD (CALLBACK *BassFileLenProc)(void*);
typedef DWORD (CALLBACK *BassFileReadProc)(void*, DWORD, void*);
typedef BOOL  (CALLBACK *BassFileSeekProc)(QWORD, void*);
struct BassFileProcs { BassFileCloseProc close; BassFileLenProc length; BassFileReadProc read; BassFileSeekProc seek; };
typedef DWORD (WINAPI *BassStreamUserFn)(DWORD, DWORD, const BassFileProcs*, void*);
enum { BASS_UNICODE_F = 0x80000000, BASS_SYNC_END_F = 2, BASS_SYNC_STALL_F = 6, BASS_ACTIVE_PLAYING_F = 1,
       BASS_ACTIVE_PAUSED_F = 3, BASS_STREAMFILE_BUFFER_F = 1, BASS_STREAM_BLOCK_F = 0x100000,
       kPlMax = 400, kMusRows = 5, kMusCols = 2, kMusMax = 400 };

static struct {
    int              state;   // 0 not loaded yet, 1 ready, -1 no bass.dll, -2 BASS_Init failed
    BassInitFn       init;
    BassPluginLoadFn pluginLoad;
    BassStreamFileFn streamFile;
    BassChanFn       streamFree, pause, stop;
    BassPlayFn       play;
    BassActiveFn     active;
    BassLenFn        getLength, getPos;
    BassSetPosFn     setPos;
    BassB2SFn        b2s;
    BassSyncFn       setSync;
    BassErrFn        error;
    TagsReadFn       tags;
    BassStreamUserFn streamUser;
} s_bass;

// Only one of the music, the internet radio and the video plays: each one starting stops / pauses the others.
static void NetStop();
static void VidPauseForAudio();
static void Redraw();
static void RedrawArea(int l, int t, int r, int b);

static DWORD    s_chan;                    // the stream playing (0 = none)
static wchar_t  s_plDir[MAX_PATH];         // the playlist's folder
static wchar_t* s_pl[kPlMax];              // its audio files (names), sorted
static int      s_plCount, s_plCur = -1;
static wchar_t  s_tTitle[96], s_tArtist[96], s_tAlbum[96];
// the folder browser of "Media -> list": "" = the volumes
static wchar_t  s_musDir[MAX_PATH];
static wchar_t* s_mus[kMusMax];            // entries: folders first ("\" at the end), then audio files
static int      s_musCount, s_musPage;

#ifdef UNDER_CE
#define BASS_PROC(name) GetProcAddress(lib, L##name)
#else
#define BASS_PROC(name) GetProcAddress(lib, name)
#endif

static bool BassLoad()
{
    if (s_bass.state)
        return s_bass.state > 0;
    wchar_t dir[MAX_PATH], path[MAX_PATH];
    _snwprintf(dir, MAX_PATH, L"%sbass\\", s_dir);
    _snwprintf(path, MAX_PATH, L"%sbass.dll", dir);
    HMODULE lib = LoadLibrary(path);
    if (!lib) {   // also right next to the exe
        wcscpy(dir, s_dir);
        _snwprintf(path, MAX_PATH, L"%sbass.dll", dir);
        lib = LoadLibrary(path);
    }
    if (!lib) {
        s_bass.state = -1;
        VLog(L"плеер: нет bass.dll (%sbass\\)", s_dir);
        return false;
    }
    s_bass.init = (BassInitFn)BASS_PROC("BASS_Init");
    s_bass.pluginLoad = (BassPluginLoadFn)BASS_PROC("BASS_PluginLoad");
    s_bass.streamFile = (BassStreamFileFn)BASS_PROC("BASS_StreamCreateFile");
    s_bass.streamFree = (BassChanFn)BASS_PROC("BASS_StreamFree");
    s_bass.pause = (BassChanFn)BASS_PROC("BASS_ChannelPause");
    s_bass.stop = (BassChanFn)BASS_PROC("BASS_ChannelStop");
    s_bass.play = (BassPlayFn)BASS_PROC("BASS_ChannelPlay");
    s_bass.active = (BassActiveFn)BASS_PROC("BASS_ChannelIsActive");
    s_bass.getLength = (BassLenFn)BASS_PROC("BASS_ChannelGetLength");
    s_bass.getPos = (BassLenFn)BASS_PROC("BASS_ChannelGetPosition");
    s_bass.setPos = (BassSetPosFn)BASS_PROC("BASS_ChannelSetPosition");
    s_bass.b2s = (BassB2SFn)BASS_PROC("BASS_ChannelBytes2Seconds");
    s_bass.setSync = (BassSyncFn)BASS_PROC("BASS_ChannelSetSync");
    s_bass.error = (BassErrFn)BASS_PROC("BASS_ErrorGetCode");
    s_bass.streamUser = (BassStreamUserFn)BASS_PROC("BASS_StreamCreateFileUser");
    if (!s_bass.init || !s_bass.streamFile || !s_bass.play || !s_bass.active || !s_bass.getLength ||
        !s_bass.getPos || !s_bass.setPos || !s_bass.b2s || !s_bass.streamFree || !s_bass.pause || !s_bass.error) {
        s_bass.state = -1;
        VLog(L"плеер: bass.dll без нужных функций");
        return false;
    }
    if (!s_bass.init(-1, 44100, 0, s_wnd, NULL)) {
        s_bass.state = -2;
        VLog(L"плеер: BASS_Init ошибка %d", s_bass.error());
        return false;
    }
    static const wchar_t* plugins[6] = { L"bass_aac.dll", L"bassflac.dll", L"bassalac.dll", L"bass_mpc.dll",
                                         L"bassopus.dll", L"basswv.dll" };
    int loaded = 0;
    for (int i = 0; i < 6 && s_bass.pluginLoad; i++) {
        _snwprintf(path, MAX_PATH, L"%s%s", dir, plugins[i]);
        if (GetFileAttributes(path) != 0xFFFFFFFF && s_bass.pluginLoad(path, BASS_UNICODE_F))
            loaded++;
    }
    _snwprintf(path, MAX_PATH, L"%stags.dll", dir);
    HMODULE tl = LoadLibrary(path);
    if (!tl && GetFileAttributes(path) != 0xFFFFFFFF)
        VLog(L"плеер: tags.dll не загрузилась, ошибка %u", (unsigned)GetLastError());
    if (tl) {
#ifdef UNDER_CE
        s_bass.tags = (TagsReadFn)GetProcAddress(tl, L"TAGS_Read");
#else
        s_bass.tags = (TagsReadFn)GetProcAddress(tl, "TAGS_Read");
#endif
    }
    s_bass.state = 1;
    VLog(L"плеер: BASS готов, плагинов %d, теги %s", loaded, s_bass.tags ? L"есть" : L"нет");
    return true;
}

static bool IsAudioName(const wchar_t* n)
{
    static const wchar_t* ext[14] = { L".mp3", L".mp2", L".mp1", L".ogg", L".wav", L".aac", L".m4a", L".mp4",
                                      L".flac", L".mpc", L".aif", L".aiff", L".opus", L".wv" };
    const wchar_t* dot = wcsrchr(n, L'.');
    if (!dot)
        return false;
    for (int i = 0; i < 14; i++)
        if (!_wcsicmp(dot, ext[i]))
            return true;
    return false;
}

// What the shell's GStreamer can open (its demuxers; the pictures by omxdecoder, see video.cpp).
static bool IsVideoName(const wchar_t* n)
{
    static const wchar_t* ext[14] = { L".mp4", L".m4v", L".mov", L".3gp", L".avi", L".mkv", L".flv", L".wmv",
                                      L".asf", L".mpg", L".mpeg", L".ts", L".vob", L".rmvb" };
    const wchar_t* dot = wcsrchr(n, L'.');
    if (!dot)
        return false;
    for (int i = 0; i < 14; i++)
        if (!_wcsicmp(dot, ext[i]))
            return true;
    return false;
}

static int CmpName(const void* a, const void* b)
{
    const wchar_t* x = *(const wchar_t* const*)a;
    const wchar_t* y = *(const wchar_t* const*)b;
    bool dx = x[wcslen(x) - 1] == L'\\', dy = y[wcslen(y) - 1] == L'\\';
    if (dx != dy)
        return dx ? -1 : 1;   // folders first
    return _wcsicmp(x, y);
}

static void FreeNames(wchar_t** v, int* n)
{
    for (int i = 0; i < *n; i++)
        free(v[i]);
    *n = 0;
}

// Folder `dir` ("X\" form): subfolders (with "\" appended, when folders) and audio (or video) files, sorted.
static int ReadMusicDir(const wchar_t* dir, wchar_t** v, int cap, bool folders, int kind = 0)   // 0 audio, 1 video,
                                                                                               // 2 photos
{
    wchar_t pat[MAX_PATH];
    _snwprintf(pat, MAX_PATH, L"%s*", dir);
    WIN32_FIND_DATA fd;
    HANDLE h = FindFirstFile(pat, &fd);
    int n = 0;
    if (h == INVALID_HANDLE_VALUE)
        return 0;
    do {
        bool isDir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if (fd.cFileName[0] == L'.' || (isDir && !folders) ||
            (!isDir && !(kind == 2 ? PicIsName(fd.cFileName) : kind ? IsVideoName(fd.cFileName)
                                                                      : IsAudioName(fd.cFileName))) ||
            (fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN))
            continue;
        size_t len = wcslen(fd.cFileName);
        wchar_t* s = (wchar_t*)malloc((len + 2) * sizeof(wchar_t));
        if (!s)
            break;
        wcscpy(s, fd.cFileName);
        if (isDir)
            wcscat(s, L"\\");
        v[n++] = s;
    } while (n < cap && FindNextFile(h, &fd));
    FindClose(h);
    qsort(v, n, sizeof(wchar_t*), CmpName);
    return n;
}

// Folder `dir` with all its subfolders (up to 6 deep): the audio files as paths inside it ("Artist\Album\01.mp3"),
// sorted by path - disc, artist, album, track. For "ИГРАТЬ ВСЁ".
static int ReadMusicTree(const wchar_t* dir, const wchar_t* sub, wchar_t** v, int n, int cap, int depth)
{
    wchar_t pat[MAX_PATH];
    _snwprintf(pat, MAX_PATH, L"%s%s*", dir, sub);
    pat[MAX_PATH - 1] = 0;
    WIN32_FIND_DATA fd;
    HANDLE h = FindFirstFile(pat, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return n;
    int first = n;
    do {
        bool isDir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if (fd.cFileName[0] == L'.' || (fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN))
            continue;
        if (isDir) {
            if (depth < 6) {
                wchar_t s2[MAX_PATH];
                _snwprintf(s2, MAX_PATH, L"%s%s\\", sub, fd.cFileName);
                s2[MAX_PATH - 1] = 0;
                n = ReadMusicTree(dir, s2, v, n, cap, depth + 1);
            }
        } else if (IsAudioName(fd.cFileName)) {
            size_t len = wcslen(sub) + wcslen(fd.cFileName);
            wchar_t* p = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
            if (!p)
                break;
            wcscpy(p, sub);
            wcscat(p, fd.cFileName);
            v[n++] = p;
        }
    } while (n < cap && FindNextFile(h, &fd));
    FindClose(h);
    if (!depth)
        qsort(v + first, n - first, sizeof(wchar_t*), CmpName);
    return n;
}

// The volumes that exist, as entries of a browser ("\SDMemory\" ...).
static int ReadVolumes(wchar_t** v)
{
#ifdef UNDER_CE
    static const wchar_t* vols[6] = { L"\\SDMemory\\", L"\\USBDisk\\", L"\\USBDevice\\", L"\\SDMMC\\",
                                      L"\\StorageFlash\\", L"\\ResidentFlash\\" };
#else
    static const wchar_t* vols[6] = { L"C:\\", L"D:\\", L"E:\\", L"F:\\", L"G:\\", L"H:\\" };
#endif
    int n = 0;
    for (int i = 0; i < 6; i++) {
        DWORD a = GetFileAttributes(vols[i]);
        if (a != 0xFFFFFFFF && (a & FILE_ATTRIBUTE_DIRECTORY)) {
            v[n] = (wchar_t*)malloc((wcslen(vols[i]) + 1) * sizeof(wchar_t));
            if (v[n])
                wcscpy(v[n++], vols[i]);
        }
    }
    return n;
}

static void MusicOpenDir(const wchar_t* dir)
{
    FreeNames(s_mus, &s_musCount);
    wcsncpy(s_musDir, dir, MAX_PATH - 1);
    s_musDir[MAX_PATH - 1] = 0;
    s_musPage = 0;
    s_musCount = s_musDir[0] ? ReadMusicDir(s_musDir, s_mus, kMusMax, true) : ReadVolumes(s_mus);
}

static void PlayerReadTags()
{
    s_tTitle[0] = s_tArtist[0] = s_tAlbum[0] = 0;
    if (s_bass.tags && s_chan) {
        const char* t = s_bass.tags(s_chan, "%UTF8(%TITL)");
        if (t && *t)
            MultiByteToWideChar(CP_UTF8, 0, t, -1, s_tTitle, 96);
        t = s_bass.tags(s_chan, "%UTF8(%ARTI)");
        if (t && *t)
            MultiByteToWideChar(CP_UTF8, 0, t, -1, s_tArtist, 96);
        t = s_bass.tags(s_chan, "%UTF8(%IFV2(%ALBM,%ALBM,)%IFV2(%YEAR, · %YEAR,))");
        if (t && *t)
            MultiByteToWideChar(CP_UTF8, 0, t, -1, s_tAlbum, 96);
    }
    if (!s_tTitle[0] && s_plCur >= 0) {   // the file name without its extension (and its folders, "ИГРАТЬ ВСЁ")
        const wchar_t* fn = wcsrchr(s_pl[s_plCur], L'\\');
        wcsncpy(s_tTitle, fn ? fn + 1 : s_pl[s_plCur], 95);
        s_tTitle[95] = 0;
        wchar_t* dot = wcsrchr(s_tTitle, L'.');
        if (dot)
            *dot = 0;
    }
}

static void CALLBACK PlayerEnd(DWORD, DWORD, DWORD, void*)
{
    PostMessage(s_wnd, WM_APP_PLAYER, 0, 0);   // from BASS's thread: the window takes the next track
}

static void PlayerStopStream()
{
    if (s_chan) {
        s_bass.streamFree(s_chan);
        s_chan = 0;
    }
}

static bool PlayerPlay(int i)
{
    if (!BassLoad() || i < 0 || i >= s_plCount)
        return false;
    NetStop();
    VidPauseForAudio();
    PlayerStopStream();
    wchar_t path[MAX_PATH];
    _snwprintf(path, MAX_PATH, L"%s%s", s_plDir, s_pl[i]);
    s_plCur = i;
    s_chan = s_bass.streamFile(FALSE, path, 0, 0, BASS_UNICODE_F);
    if (!s_chan) {
        int e = s_bass.error();
        _snwprintf(s_status, 96, L"не играет: %s (BASS %d)", s_pl[i], e);
        VLog(L"плеер: %s не открылся, BASS %d", path, e);
        return false;
    }
    if (s_bass.setSync)
        s_bass.setSync(s_chan, BASS_SYNC_END_F, 0, PlayerEnd, NULL);
    s_bass.play(s_chan, FALSE);
    MediaSource();
    PlayerReadTags();
    wcscpy(s_cfg.musicDir, s_plDir);
    wcsncpy(s_cfg.musicFile, s_pl[i], MAX_PATH - 1);
    s_cfg.musicFile[MAX_PATH - 1] = 0;
    CfgSoon();   // (not at once: every track would write the flash)
    return true;
}

// The playlist = the audio files of `dir` (tree: with all its subfolders), starting with `file` (or the first).
static bool PlayerLoadDir(const wchar_t* dir, const wchar_t* file, bool tree = false)
{
    FreeNames(s_pl, &s_plCount);
    wcsncpy(s_plDir, dir, MAX_PATH - 1);
    s_plDir[MAX_PATH - 1] = 0;
    s_cfg.musicTree = tree;
    s_plCount = tree ? ReadMusicTree(s_plDir, L"", s_pl, 0, kPlMax, 0) : ReadMusicDir(s_plDir, s_pl, kPlMax, false);
    s_plCur = -1;
    if (!s_plCount)
        return false;
    int start = 0;
    for (int i = 0; file && i < s_plCount; i++)
        if (!_wcsicmp(s_pl[i], file))
            start = i;
    return PlayerPlay(start);
}

static bool PlayerPlaying()
{
    return s_chan && s_bass.active(s_chan) == BASS_ACTIVE_PLAYING_F;
}

// Next / previous: shuffle picks any other track; past the end repeat goes round, otherwise it stops.
static void PlayerStep(int dir, bool automatic)
{
    if (!s_plCount)
        return;
    int i;
    if (s_cfg.shuffle && s_plCount > 1) {
        do {
            i = rand() % s_plCount;
        } while (i == s_plCur);
    } else {
        i = s_plCur + dir;
        if (i >= s_plCount || i < 0) {
            if (!s_cfg.repeat && automatic) {
                PlayerStopStream();
                return;
            }
            i = (i + s_plCount) % s_plCount;
        }
    }
    PlayerPlay(i);
}

static void PlayerPrev()
{
    // more than 3 s into the track: back to its start, like every player
    if (s_chan && s_bass.b2s(s_chan, s_bass.getPos(s_chan, 0)) > 3.0) {
        s_bass.setPos(s_chan, 0, 0);
        return;
    }
    PlayerStep(-1, false);
}

static void PlayerToggle()
{
    if (!BassLoad()) {
        wcscpy(s_status, s_bass.state == -1 ? L"нет bass.dll: папка bass рядом с VestaHome (см. «Медиа»)" : L"звук не запускается (BASS_Init)");
        return;
    }
    if (!s_chan) {   // nothing loaded: the last track, otherwise the list
        if (!s_cfg.musicDir[0] || !PlayerLoadDir(s_cfg.musicDir, s_cfg.musicFile, s_cfg.musicTree))
            Go(S_AUDIO_LIST, true);
        return;
    }
    DWORD a = s_bass.active(s_chan);
    if (a == BASS_ACTIVE_PLAYING_F) {
        s_bass.pause(s_chan);
    } else {
        NetStop();
        VidPauseForAudio();
        s_bass.play(s_chan, a == 0);   // stopped at the end: from the start
        MediaSource();
    }
}

static void PlayerSeek(int k)   // k of 20 along the bar
{
    if (!s_chan)
        return;
    QWORD len = s_bass.getLength(s_chan, 0);
    if (len && len != (QWORD)-1)
        s_bass.setPos(s_chan, len / 20 * k, 0);
}

// "1:42" of seconds.
static void Mmss(wchar_t* s, int n, double sec)
{
    int t = sec > 0 ? (int)sec : 0;
    _snwprintf(s, n, L"%d:%02d", t / 60, t % 60);
}

// The track for the screens: title, second line, position and length (s), false = nothing loaded.
static bool PlayerNow(wchar_t* title, wchar_t* sub, int n, double* pos, double* len)
{
    if (!s_chan)
        return false;
    wcsncpy(title, s_tTitle, n);
    title[n - 1] = 0;
    if (s_tArtist[0] && s_tAlbum[0])
        _snwprintf(sub, n, L"%s · %s", s_tArtist, s_tAlbum);
    else
        wcsncpy(sub, s_tArtist[0] ? s_tArtist : s_tAlbum, n);
    sub[n - 1] = 0;
    QWORD l = s_bass.getLength(s_chan, 0), p = s_bass.getPos(s_chan, 0);
    *len = l != (QWORD)-1 ? s_bass.b2s(s_chan, l) : 0;
    *pos = p != (QWORD)-1 ? s_bass.b2s(s_chan, p) : 0;
    return true;
}

// The orange play / pause circle with its action.
static void PlayBtn(HDC dc, int cx, int cy, int r, int isz)
{
    bool down = Zone(cx - r, cy - r, 2 * r, 2 * r, A_PLAY);
    Disc(dc, cx, cy, r, down ? kFacet : kOrange);
    Ico(dc, PlayerPlaying() ? IC_PAUSE : IC_PLAY, cx - isz / 2, cy - isz / 2, isz, kInk);
}

// Media -> music: the track, the bar (a tap seeks), shuffle, previous, play/pause, next, repeat.
static void ScrAudio(HDC dc)
{
    Frame(dc, L"МЕДИА · МУЗЫКА");
    MediaTabs(dc, S_AUDIO, IC_LIST, A_SCREEN, S_AUDIO_LIST, S_SET_SOUND);
    Cover(dc, IC_MUSIC, 16, 124, 232);
    wchar_t title[96], sub[200], s[160];
    double pos = 0, len = 0;
    bool have = PlayerNow(title, sub, 96, &pos, &len);
    if (have) {
        const wchar_t* folder = s_plDir;   // the folder's own name
        int l = (int)wcslen(s_plDir);
        for (int i = l - 2; i >= 0; i--)
            if (s_plDir[i] == L'\\') {
                folder = s_plDir + i + 1;
                break;
            }
        _snwprintf(s, 160, L"ТРЕК %d ИЗ %d · %s", s_plCur + 1, s_plCount, folder);
        if (s[wcslen(s) - 1] == L'\\')
            s[wcslen(s) - 1] = 0;
        T(dc, 272, 150, s, 14, kMuted, 600, AL_START, 512);
        T(dc, 272, 192, title, 34, kText, 700, AL_START, 512);
        T(dc, 272, 222, sub, 20, kMuted, 400, AL_START, 512);
    } else if (s_bass.state < 0) {
        T(dc, 272, 170, L"НЕТ БИБЛИОТЕКИ ЗВУКА", 22, kText, 700, AL_START, 512);
        if (s_bass.state == -1) {   // where it was looked for: the folder bass\ next to the exe
            wchar_t where[MAX_PATH + 16];
            _snwprintf(where, MAX_PATH + 16, L"нет %sbass\\bass.dll", s_dir);
            where[MAX_PATH + 15] = 0;
            T(dc, 272, 200, where, 15, kMuted, 400, AL_START, 512);
#ifdef UNDER_CE
            T(dc, 272, 224, L"Скопируйте папку bass из Tests\\SDMemory\\VestaHome", 15, kMuted, 400, AL_START, 512);
#else
            T(dc, 272, 224, L"ПК: BASS для Windows x86 (un4seen.com) в эту папку", 15, kMuted, 400, AL_START, 512);
#endif
        } else {
            T(dc, 272, 200, L"BASS_Init не удался — звук занят?", 16, kMuted, 400, AL_START, 512);
        }
    } else {
        T(dc, 272, 170, L"НИЧЕГО НЕ ИГРАЕТ", 22, kText, 700, AL_START, 512);
        T(dc, 272, 200, s_cfg.musicFile[0] ? L"Нажмите ▶ — продолжить последний трек" : L"Выберите музыку — кнопка списка вверху",
          16, kMuted, 400, AL_START, 512);
    }
    int k = len > 0 ? (int)(pos * 512 / len) : 0;
    R(dc, 272, 248, 512, 8, 4, kLine);
    if (have) {
        R(dc, 272, 248, k > 8 ? k : 8, 8, 4, kOrange);
        Disc(dc, 272 + k, 252, 11, kText);
        for (int z = 0; z < 20; z++)   // tap anywhere along the bar
            Zone(272 + z * 512 / 20, 232, 512 / 20 + 1, 40, A_SEEK, z);
        Mmss(s, 16, pos);
        T(dc, 272, 282, s, 17, kMuted, 600);
        Mmss(s, 16, len);
        T(dc, 784, 282, s, 17, kMuted, 600, AL_END);
    }
    IconBtn(dc, IC_SHUFFLE, 290, 316, 40, s_cfg.shuffle ? kOrange : kDim, A_SHUFFLE);
    IconBtn(dc, IC_PREV, 392, 312, 48, kText, A_PREVTRK);
    PlayBtn(dc, 528, 336, 42, 48);
    IconBtn(dc, IC_FWD, 616, 312, 48, kText, A_NEXTTRK);
    IconBtn(dc, IC_REPEAT, 726, 316, 40, s_cfg.repeat ? kOrange : kDim, A_REPEAT);
}

// The file lists' pages (music, video): rows a page less the "Наверх" row inside a folder.
static int MusPages()
{
    int per = kMusRows * kMusCols - (s_musDir[0] ? 1 : 0);   // "Наверх" takes the first cell of every page
    return s_musCount ? (s_musCount + per - 1) / per : 1;
}

// Media -> list: the volumes, then folders and audio files; a file plays its folder from it. Pages: a swipe and
// the dots, as everywhere (2026-10-20, the user: one way for every file list, no arrows).
static void ScrAudioList(HDC dc)
{
    if (!s_musCount && !s_musDir[0])
        MusicOpenDir(s_cfg.musicDir[0] && GetFileAttributes(s_cfg.musicDir) != 0xFFFFFFFF ? s_cfg.musicDir : L"");
    wchar_t s[MAX_PATH + 16], folder[MAX_PATH];
    wcscpy(folder, s_musDir);   // the folder's own name in the title, the whole path would not fit
    int fl = (int)wcslen(folder);
    if (fl && folder[fl - 1] == L'\\')
        folder[--fl] = 0;
    const wchar_t* fn = wcsrchr(folder, L'\\');
    _snwprintf(s, MAX_PATH + 16, L"МУЗЫКА · %s", !s_musDir[0] ? L"НОСИТЕЛИ" : fn && fn[1] ? fn + 1 : folder);
    Frame(dc, s);
    if (s_musPage >= MusPages())
        s_musPage = MusPages() - 1;
    int cell = 0;   // cells in columns: 5 down, then the second column
    if (s_musDir[0]) {
        ListRow(dc, 16, 66, 380, 56, L"Наверх", NULL, L_N, IC_UP, NULL, NULL, A_MUP);
        cell = 1;
    }
    int per = kMusRows * kMusCols - cell, first = s_musPage * per;
    for (int i = first; i < s_musCount && i < first + per; i++, cell++) {
        int x = 16 + cell / kMusRows * 388, y = 66 + cell % kMusRows * 62;
        const wchar_t* n = s_mus[i];
        bool isDir = n[wcslen(n) - 1] == L'\\';
        bool cur = !isDir && s_chan && s_plCur >= 0 && !_wcsicmp(s_plDir, s_musDir) && !_wcsicmp(n, s_pl[s_plCur]);
        wcsncpy(s, n, MAX_PATH);
        s[MAX_PATH] = 0;
        if (isDir && s[0] && s[wcslen(s) - 1] == L'\\')
            s[wcslen(s) - 1] = 0;
        ListRow(dc, x, y, 380, 56, s, NULL, cur ? L_CUR : L_N, isDir ? IC_FOLDER : IC_MUSIC, NULL, NULL, A_MFILE, i,
                isDir ? kOrange : CLR_INVALID);   // folders orange, tracks muted
    }
    if (!s_musCount)
        T(dc, 40, s_musDir[0] ? 176 : 114, s_musDir[0] ? L"Здесь нет папок и музыки" : L"Нет носителей", 18, kMuted,
          400);
    PageDots(dc, s_musPage, MusPages(), 400, 382);
    Dock(dc);
    Button(dc, 16, 410, 200, 58, L"НОСИТЕЛИ", B_N, IC_FOLDER, 16, 28, A_MUP, 1);
    if (s_musDir[0])   // this folder and everything under it, in path order
        Button(dc, 232, 410, 220, 58, L"ИГРАТЬ ВСЁ", B_N, IC_PLAY, 16, 26, A_MALL);
    IconBtn(dc, IC_MUSIC, 600, 417, 44, kText, A_BACK);
    IconBtn(dc, IC_GEAR, 712, 417, 44, kText, A_SCREEN, S_SET_SOUND);
}

// A browser's folder ("X\" form) -> its parent; the volume root ("\SDMemory\" or "C:\") -> "" (the volumes).
static void ParentDir(wchar_t* d)
{
    int l = (int)wcslen(d);
    if (l && d[l - 1] == L'\\')
        d[--l] = 0;
    wchar_t* slash = wcsrchr(d, L'\\');
    // the volume root ("\SDMemory\" or "C:\") goes back to the volume list
    if (!slash || slash == d || (slash == d + 2 && d[1] == L':' && !d[3]))
        d[0] = 0;
    else
        slash[1] = 0;
}

static void MusicUp(bool toVolumes)
{
    if (toVolumes || !s_musDir[0]) {
        MusicOpenDir(L"");
        return;
    }
    wchar_t d[MAX_PATH];
    wcscpy(d, s_musDir);
    ParentDir(d);
    MusicOpenDir(d);
}

static void MusicPick(int i)
{
    if (i < 0 || i >= s_musCount)
        return;
    const wchar_t* n = s_mus[i];
    wchar_t d[MAX_PATH];
    if (n[wcslen(n) - 1] == L'\\') {   // a folder (or a volume)
        _snwprintf(d, MAX_PATH, L"%s%s", s_musDir, n);
        MusicOpenDir(d);
        return;
    }
    wchar_t file[MAX_PATH];
    wcsncpy(file, n, MAX_PATH - 1);
    file[MAX_PATH - 1] = 0;
    wcscpy(d, s_musDir);
    if (!BassLoad()) {
        wcscpy(s_status, s_bass.state == -1 ? L"нет bass.dll: папка bass рядом с VestaHome (см. «Медиа»)" : L"звук не запускается (BASS_Init)");
        return;
    }
    if (PlayerLoadDir(d, file))
        Back();
}

// ---- internet radio (Media -> "Интернет"): the stations of radio.txt next to the exe ("name|address" lines),
// played by BASS from our own HTTP / HTTPS reader (https.cpp: BearSSL, redirects; the BASS for CE has no HTTPS):
// BASS_StreamCreateFileUser with a buffered "file" in blocks (nothing kept after it is played) whose read
// callback takes the ICY metadata (the title of the song) out of the audio. MP3 and OGG by BASS, AAC / AAC+ by
// bass_aac. The connection is made in a thread of its own (it takes seconds); a broken stream is opened again
// after 3, 6 ... 60 s while the radio is on.
#define WM_APP_NET (WM_APP + 22)
#define WM_APP_WIFI (WM_APP + 24)
enum { NET_READY = 1, NET_FAILED, NET_ENDED, NET_META, NET_STALL, kNetMax = 100, kNetRows = 5 };
enum { NR_OFF, NR_CONNECT, NR_PLAY, NR_STALL, NR_WAIT, NR_ERROR };

struct NetStation { wchar_t name[64]; char url[400]; };
struct NetFile {
    HttpStream* http;     // NULL once closed (under s_netCs)
    int         metaint;  // the ICY block size (0: no metadata)
    int         left;     // audio bytes before the next metadata block
    LONG        gen;
};
struct NetReady { LONG gen; DWORD chan; NetFile* file; };

static NetStation* s_stations;          // radio.txt, read when the screen is first opened
static int         s_netCount = -1, s_netScroll;   // the list scrolled by (px)
static int         s_netCur = -1;  // the station chosen
static bool        s_netOn;        // the radio should play: reconnects while on
static int         s_netState;     // NR_*
static DWORD       s_netChan;
static NetFile*    s_netFile;      // the playing stream's
static NetFile*    s_netOpening;   // the stream BASS_StreamCreateFileUser is reading now (under s_netCs)
static LONG       s_netGen;       // the attempt: an older thread's result is thrown away
static DWORD       s_netRetryAt;
static int         s_netRetries;
static bool        s_netFatal;     // the last failure was the format, not the network: no retry
static wchar_t     s_netMsg[96];   // the state or the error (under s_netCs while the thread writes it)
static wchar_t     s_netTitle[160]; // the song (ICY StreamTitle), under s_netCs
static wchar_t     s_netInfo[64];   // MP3 / AAC / OGG
static CRITICAL_SECTION s_netCs;

static void NetInit()
{
    InitializeCriticalSection(&s_netCs);
}

static void NetLoadList()
{
    if (s_netCount >= 0)
        return;
    s_netCount = 0;
    s_stations = (NetStation*)calloc(kNetMax, sizeof(NetStation));
    wchar_t path[MAX_PATH];
    _snwprintf(path, MAX_PATH, L"%sradio.txt", s_dir);
    DWORD size;
    char* buf = s_stations ? ReadAll(path, &size) : NULL;
    if (!buf)
        return;
    char* p = buf;
    if ((unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF)
        p += 3;
    while (*p && s_netCount < kNetMax) {
        char* end = strchr(p, '\n');
        if (end)
            *end = 0;
        int n = (int)strlen(p);
        while (n > 0 && (p[n - 1] == '\r' || p[n - 1] == ' ' || p[n - 1] == '\t'))
            p[--n] = 0;
        char* bar = strchr(p, '|');
        if (p[0] != '#' && bar && (!_strnicmp(bar + 1, "http://", 7) || !_strnicmp(bar + 1, "https://", 8))) {
            *bar = 0;
            NetStation& s = s_stations[s_netCount];
            MultiByteToWideChar(CP_UTF8, 0, p, -1, s.name, 64);
            s.name[63] = 0;
            Trim(s.name);
            strncpy(s.url, bar + 1, sizeof(s.url) - 1);
            if (s.name[0])
                s_netCount++;
        }
        p = end ? end + 1 : p + strlen(p);
    }
    free(buf);
    for (int i = 0; i < s_netCount; i++)
        if (!wcscmp(s_stations[i].name, s_cfg.radioStation))
            s_netCur = i;
}

static bool ValidUtf8(const unsigned char* s)
{
    while (*s) {
        int n = *s < 0x80 ? 0 : (*s & 0xE0) == 0xC0 ? 1 : (*s & 0xF0) == 0xE0 ? 2 : (*s & 0xF8) == 0xF0 ? 3 : -1;
        if (n < 0)
            return false;
        s++;
        for (; n > 0; n--, s++)
            if ((*s & 0xC0) != 0x80)
                return false;
    }
    return true;
}

// Russian stations send their titles in UTF-8 or in Windows-1251.
static void DecodeTitle(const char* s, wchar_t* w, int cap)
{
    if (ValidUtf8((const unsigned char*)s) && MultiByteToWideChar(CP_UTF8, 0, s, -1, w, cap))
        return;
    int i = 0;
    for (; s[i] && i < cap - 1; i++) {   // 1251 by hand: the unit may lack the code page
        unsigned char c = (unsigned char)s[i];
        w[i] = c >= 0xC0 ? (wchar_t)(0x410 + c - 0xC0) : c == 0xA8 ? 0x401 : c == 0xB8 ? 0x451 : c < 0x80 ? c : L'?';
    }
    w[i] = 0;
}

// A metadata block: "StreamTitle='Artist - Song';StreamUrl='';"
static void NetMeta(const char* m, LONG gen)
{
    const char* p = strstr(m, "StreamTitle='");
    if (!p)
        return;
    p += 13;
    const char* e = strstr(p, "';");
    if (!e)
        e = p + strlen(p);
    char t[320];
    int n = (int)(e - p) < 319 ? (int)(e - p) : 319;
    memcpy(t, p, n);
    t[n] = 0;
    wchar_t w[160];
    DecodeTitle(t, w, 160);
    Trim(w);
    EnterCriticalSection(&s_netCs);
    bool changed = wcscmp(w, s_netTitle) != 0;
    if (changed)
        wcscpy(s_netTitle, w);
    LeaveCriticalSection(&s_netCs);
    if (changed)
        PostMessage(s_wnd, WM_APP_NET, NET_META, gen);
}

static bool NetReadFull(NetFile* f, char* buf, int len)
{
    while (len > 0) {
        int k = HttpStreamRead(f->http, buf, len);
        if (k <= 0)
            return false;
        buf += k;
        len -= k;
    }
    return true;
}

// BASS's download thread: the audio bytes; the metadata blocks are taken out. 0 = the end of the "file".
static DWORD CALLBACK NetReadProc(void* buf, DWORD len, void* user)
{
    NetFile* f = (NetFile*)user;
    for (;;) {
        if (f->metaint && f->left == 0) {
            unsigned char l;
            if (!NetReadFull(f, (char*)&l, 1))
                return 0;
            if (l) {
                char m[16 * 255 + 1];
                if (!NetReadFull(f, m, l * 16))
                    return 0;
                m[l * 16] = 0;
                NetMeta(m, f->gen);
            }
            f->left = f->metaint;
            continue;
        }
        int want = (int)len;
        if (f->metaint && want > f->left)
            want = f->left;
        int k = HttpStreamRead(f->http, (char*)buf, want);
        if (k <= 0)
            return 0;   // broken or closed: BASS plays what it has, then the END sync
        if (f->metaint)
            f->left -= k;
        return (DWORD)k;
    }
}

static void CALLBACK NetCloseProc(void* user)
{
    NetFile* f = (NetFile*)user;
    EnterCriticalSection(&s_netCs);
    if (f->http)
        HttpStreamClose(f->http);
    f->http = NULL;
    LeaveCriticalSection(&s_netCs);
}

static QWORD CALLBACK NetLenProc(void*) { return 0; }      // unknown: endless
static BOOL CALLBACK NetSeekProc(QWORD, void*) { return FALSE; }

static const BassFileProcs kNetProcs = { NetCloseProc, NetLenProc, NetReadProc, NetSeekProc };

static void NetAbortFile(NetFile* f)   // the blocked read returns: BASS can free the stream at once
{
    EnterCriticalSection(&s_netCs);
    if (f && f->http)
        HttpStreamAbort(f->http);
    LeaveCriticalSection(&s_netCs);
}

static void CALLBACK NetEndSync(DWORD, DWORD, DWORD, void* user)
{
    PostMessage(s_wnd, WM_APP_NET, NET_ENDED, (LPARAM)user);
}

static void CALLBACK NetStallSync(DWORD, DWORD, DWORD data, void* user)
{
    PostMessage(s_wnd, WM_APP_NET, NET_STALL | (data << 8), (LPARAM)user);   // data 0 stalled, 1 going on
}

struct NetStart { LONG gen; char url[400]; };

static DWORD WINAPI NetThread(LPVOID p)
{
    VLog(L"поток радио-сети: id %08X", (unsigned)GetCurrentThreadId());
    NetStart* st = (NetStart*)p;
    int err = 0, metaint = 0;
    char name[96], type[64];
    HttpStream* h = HttpStreamOpen(st->url, &err, &metaint, name, sizeof(name), type, sizeof(type));
    NetFile* f = NULL;
    DWORD chan = 0;
    wchar_t msg[96] = L"";
    if (!h) {
        _snwprintf(msg, 96, L"%s", NetErrorText(err));
    } else {
        f = (NetFile*)calloc(1, sizeof(NetFile));
        if (f) {
            f->http = h;
            f->metaint = metaint;
            f->left = metaint;
            f->gen = st->gen;
            EnterCriticalSection(&s_netCs);
            s_netOpening = f;
            LeaveCriticalSection(&s_netCs);
            if (st->gen == s_netGen)   // reads the start of the stream to know its format (blocks)
                chan = s_bass.streamUser(BASS_STREAMFILE_BUFFER_F, BASS_STREAM_BLOCK_F, &kNetProcs, f);
            int berr = chan ? 0 : s_bass.error();
            EnterCriticalSection(&s_netCs);
            s_netOpening = NULL;
            if (!chan && f->http)
                HttpStreamClose(f->http);
            if (!chan)
                f->http = NULL;
            LeaveCriticalSection(&s_netCs);
            if (!chan) {
                free(f);
                f = NULL;
                _snwprintf(msg, 96, L"поток не играет (BASS %d)", berr);
            }
        } else {
            HttpStreamClose(h);
            wcscpy(msg, NetErrorText(NET_MEMORY));
        }
    }
    if (chan) {
        EnterCriticalSection(&s_netCs);
        wcscpy(s_netInfo, strstr(type, "aac") ? L"AAC" : strstr(type, "ogg") ? L"OGG" : strstr(type, "mpeg") ? L"MP3" : L"");
        LeaveCriticalSection(&s_netCs);
        NetReady* r = (NetReady*)malloc(sizeof(NetReady));
        if (r) {
            r->gen = st->gen;
            r->chan = chan;
            r->file = f;
        }
        if (!r || !PostMessage(s_wnd, WM_APP_NET, NET_READY, (LPARAM)r)) {
            NetAbortFile(f);
            s_bass.streamFree(chan);
            free(f);
            free(r);
        }
    } else if (st->gen == s_netGen) {   // (an older attempt, stopped or replaced, ends quietly)
        EnterCriticalSection(&s_netCs);
        wcscpy(s_netMsg, msg);
        s_netFatal = h != NULL;   // connected, but BASS doesn't take it
        LeaveCriticalSection(&s_netCs);
        VLog(L"радио: %S — %s", st->url, msg);
        PostMessage(s_wnd, WM_APP_NET, NET_FAILED, st->gen);
    }
    free(st);
    return 0;
}

static void NetFreeChan()
{
    if (s_netChan) {
        NetAbortFile(s_netFile);
        s_bass.streamFree(s_netChan);   // calls NetCloseProc
        s_netChan = 0;
    }
    free(s_netFile);
    s_netFile = NULL;
}

static void NetConnect()
{
    if (s_netCur < 0 || s_netCur >= s_netCount)
        return;
    NetStart* st = (NetStart*)malloc(sizeof(NetStart));
    if (!st)
        return;
    st->gen = InterlockedIncrement(&s_netGen);
    strcpy(st->url, s_stations[s_netCur].url);
    s_netState = NR_CONNECT;
    EnterCriticalSection(&s_netCs);
    wcscpy(s_netMsg, L"подключение…");
    s_netTitle[0] = s_netInfo[0] = 0;
    LeaveCriticalSection(&s_netCs);
    HANDLE t = CreateThread(NULL, 0, NetThread, st, 0, NULL);
    if (t)
        CloseHandle(t);
    else
        free(st);
}

static void NetStop()
{
    if (!s_netOn && !s_netChan)
        return;
    s_netOn = false;
    InterlockedIncrement(&s_netGen);   // a connection under way is thrown away when it comes
    EnterCriticalSection(&s_netCs);
    if (s_netOpening && s_netOpening->http)
        HttpStreamAbort(s_netOpening->http);
    LeaveCriticalSection(&s_netCs);
    NetFreeChan();
    s_netState = NR_OFF;
    s_netMsg[0] = 0;
}

static void NetPlay(int i)
{
    NetLoadList();
    if (i < 0 || i >= s_netCount)
        return;
    if (!BassLoad() || !s_bass.streamUser) {
        wcscpy(s_status, s_bass.state == -1 ? L"нет bass.dll: папка bass рядом с VestaHome" : L"звук не запускается (BASS)");
        return;
    }
    NetStop();
    PlayerStopStream();
    VidPauseForAudio();
    s_netCur = i;
    s_netOn = true;
    s_netRetries = 0;
    s_netFatal = false;
    wcscpy(s_cfg.radioStation, s_stations[i].name);
    SaveCfg();
    VLog(L"радио: %s", s_stations[i].name);
    MediaSource();
    NetConnect();
}

static void NetToggle()
{
    NetLoadList();
    if (s_netOn)
        NetStop();
    else
        NetPlay(s_netCur >= 0 ? s_netCur : 0);
}

static void NetStep(int dir)
{
    NetLoadList();
    if (s_netCount > 0)
        NetPlay(s_netCur < 0 ? 0 : (s_netCur + dir + s_netCount) % s_netCount);
}

// The thread's results and BASS's syncs (wp = what, lp = the attempt).
static void OnNetMessage(WPARAM wp, LPARAM lp)
{
    int what = (int)(wp & 0xFF);
    if (what == NET_READY) {
        NetReady* r = (NetReady*)lp;
        if (r->gen != s_netGen || !s_netOn) {   // stopped or another station meanwhile
            NetAbortFile(r->file);
            s_bass.streamFree(r->chan);
            free(r->file);
        } else {
            s_netChan = r->chan;
            s_netFile = r->file;
            if (s_bass.setSync) {
                s_bass.setSync(s_netChan, BASS_SYNC_END_F, 0, NetEndSync, (void*)r->gen);
                s_bass.setSync(s_netChan, BASS_SYNC_STALL_F, 0, NetStallSync, (void*)r->gen);
            }
            s_bass.play(s_netChan, FALSE);
            s_netState = NR_PLAY;
            s_netRetries = 0;
            s_netMsg[0] = 0;
        }
        free(r);
        return;
    }
    if ((LONG)lp != s_netGen || !s_netOn)
        return;
    if (what == NET_STALL) {
        if (s_netState == NR_PLAY || s_netState == NR_STALL)
            s_netState = (wp >> 8) ? NR_PLAY : NR_STALL;
    } else if (what == NET_ENDED || what == NET_FAILED) {
        NetFreeChan();
        if (what == NET_FAILED && s_netFatal) {
            s_netState = NR_ERROR;   // the message is the thread's
            return;
        }
        if (what == NET_ENDED)
            wcscpy(s_netMsg, L"связь прервалась");
        int delay = s_netRetries < 5 ? 3 << s_netRetries : 60;
        delay = delay > 60 ? 60 : delay;
        s_netRetries++;
        s_netRetryAt = GetTickCount() + delay * 1000;
        s_netState = NR_WAIT;
    }
}

// Once a second (the window's clock timer): the next attempt.
static void NetTick()
{
    if (s_netOn && s_netState == NR_WAIT && (int)(GetTickCount() - s_netRetryAt) >= 0)
        NetConnect();
}

static void NetStateText(wchar_t* s, int n)
{
    EnterCriticalSection(&s_netCs);
    switch (s_netState) {
    case NR_CONNECT: _snwprintf(s, n, L"%s", s_netMsg); break;
    case NR_PLAY:    _snwprintf(s, n, L"%s%sв эфире", s_netInfo, s_netInfo[0] ? L" · " : L""); break;
    case NR_STALL:   _snwprintf(s, n, L"буферизация…"); break;
    case NR_WAIT: {
        int left = (int)(s_netRetryAt - GetTickCount()) / 1000 + 1;
        _snwprintf(s, n, L"%s — снова через %d с", s_netMsg[0] ? s_netMsg : L"нет связи", left > 0 ? left : 1);
        break;
    }
    case NR_ERROR:   _snwprintf(s, n, L"%s", s_netMsg); break;
    default:         _snwprintf(s, n, L"выключено"); break;
    }
    s[n - 1] = 0;
    LeaveCriticalSection(&s_netCs);
}

static void NetPlayBtn(HDC dc, int cx, int cy, int r, int isz)
{
    bool down = Zone(cx - r, cy - r, 2 * r, 2 * r, A_NRPLAY);
    Disc(dc, cx, cy, r, down ? kFacet : kOrange);
    Ico(dc, s_netOn ? IC_PAUSE : IC_PLAY, cx - isz / 2, cy - isz / 2, isz, kInk);
}

// Media -> internet radio: the station, the song, the state; previous / on-off / next station.
static void ScrNetRadio(HDC dc)
{
    NetLoadList();
    Frame(dc, L"МЕДИА · ИНТЕРНЕТ-РАДИО");
    MediaTabs(dc, S_NETRADIO, IC_LIST, A_SCREEN, S_NETRADIO_LIST, S_SET_SOUND);
    Cover(dc, IC_RADIO, 16, 124, 232);
    wchar_t s[160], title[160];
    if (s_netCount <= 0) {
        T(dc, 272, 170, L"НЕТ СТАНЦИЙ", 22, kText, 700, AL_START, 512);
        _snwprintf(s, 160, L"нет %sradio.txt", s_dir);
        s[159] = 0;
        T(dc, 272, 200, s, 15, kMuted, 400, AL_START, 512);
        T(dc, 272, 224, L"Строка: название|адрес потока (http:// или https://)", 15, kMuted, 400, AL_START, 512);
        return;
    }
    if (s_netCur < 0) {
        T(dc, 272, 170, L"ВЫБЕРИТЕ СТАНЦИЮ", 22, kText, 700, AL_START, 512);
        T(dc, 272, 200, L"Кнопка списка вверху или ▶ — первая станция", 16, kMuted, 400, AL_START, 512);
    } else {
        _snwprintf(s, 160, L"СТАНЦИЯ %d ИЗ %d", s_netCur + 1, s_netCount);
        T(dc, 272, 150, s, 14, kMuted, 600, AL_START, 512);
        T(dc, 272, 192, s_stations[s_netCur].name, 34, kText, 700, AL_START, 512);
        EnterCriticalSection(&s_netCs);
        wcscpy(title, s_netTitle);
        LeaveCriticalSection(&s_netCs);
        if (s_netOn && title[0])
            T(dc, 272, 222, title, 20, kMuted, 400, AL_START, 512);
        NetStateText(s, 160);
        T(dc, 272, 282, s, 17, s_netState == NR_PLAY ? kOrange : kMuted, 600, AL_START, 512);
    }
    IconBtn(dc, IC_PREV, 392, 312, 48, kText, A_NRSTEP, -1);
    NetPlayBtn(dc, 528, 336, 42, 48);
    IconBtn(dc, IC_FWD, 616, 312, 48, kText, A_NRSTEP, 1);
}

// The stations of radio.txt, a list that scrolls (the finger, or the buttons by kNetRows rows); the current one
// orange. The rows are cut at the panel, their buttons too (a row half above it must not take the top bar's taps).
enum { kNetTop = 72, kNetBottom = 386, kNetPitch = 62 };

static int NetScrollMax()
{
    int content = s_netCount > 0 ? s_netCount * kNetPitch - 6 + 8 : 0;   // the rows, 4 px above and below
    return content > kNetBottom - kNetTop ? content - (kNetBottom - kNetTop) : 0;
}

static void NetScrollTo(int px)
{
    int m = NetScrollMax();
    s_netScroll = px < 0 ? 0 : px > m ? m : px;
}

static void ScrNetRadioList(HDC dc)
{
    NetLoadList();
    static DWORD s_seen;
    if (s_seen != s_screenTick) {   // opened: the station playing in view
        s_seen = s_screenTick;
        if (s_netCur >= 0 && (s_netCur * kNetPitch < s_netScroll ||
                              s_netCur * kNetPitch + kNetPitch > s_netScroll + kNetBottom - kNetTop))
            NetScrollTo((s_netCur - 2) * kNetPitch);
    }
    NetScrollTo(s_netScroll);   // (the list may have changed)
    Frame(dc, L"ИНТЕРНЕТ-РАДИО · СТАНЦИИ");
    Panel(dc, 16, 68, 704, 322);
    int saved = SaveDC(dc);
    IntersectClipRect(dc, 20, kNetTop, 716, kNetBottom);
    for (int i = 0; i < s_netCount; i++) {
        int y = kNetTop + 4 + i * kNetPitch - s_netScroll;
        if (y + 56 <= kNetTop || y >= kNetBottom)
            continue;
        int k0 = kBtnCount;
        ListRow(dc, 24, y, 680, 56, s_stations[i].name, NULL, i == s_netCur ? L_CUR : L_N, IC_RADIO, NULL, NULL,
                A_NRPICK, i);
        for (int k = k0; k < kBtnCount; k++) {   // its buttons cut at the panel too
            RECT& rc = s_btns[k].rc;
            rc.top = rc.top < kNetTop ? kNetTop : rc.top;
            rc.bottom = rc.bottom > kNetBottom ? kNetBottom : rc.bottom;
            if (rc.bottom < rc.top)
                rc.bottom = rc.top;
        }
    }
    RestoreDC(dc, saved);
    if (s_netCount <= 0)
        T(dc, 40, 114, L"Нет станций: radio.txt рядом с VestaHome", 18, kMuted, 400);
    int m = NetScrollMax();
    if (m > 0) {
        Button(dc, 728, 68, 56, 96, NULL, s_netScroll > 0 ? B_N : B_DIS, IC_UP, 18, 30, A_NRPAGE, -1);
        int view = kNetBottom - kNetTop, content = m + view, track = 114;   // the scroll bar between the buttons
        int th = track * view / content, ty = 172 + (track - th) * s_netScroll / m;
        th = th < 16 ? 16 : th;
        ty = ty + th > 172 + track ? 172 + track - th : ty;
        R(dc, 753, 172, 6, track, 3, kLine);
        R(dc, 753, ty, 6, th, 3, kMuted);
        Button(dc, 728, 294, 56, 96, NULL, s_netScroll < m ? B_N : B_DIS, IC_DOWN, 18, 30, A_NRPAGE, 1);
    }
    Dock(dc);
    IconBtn(dc, IC_RADIO, 600, 417, 44, kText, A_BACK);
    IconBtn(dc, IC_GEAR, 712, 417, 44, kText, A_SCREEN, S_SET_SOUND);
}

static void NetPage(int dir)   // the buttons: kNetRows rows up / down
{
    NetScrollTo(s_netScroll + dir * kNetRows * kNetPitch);
}

// ---- video (Media -> Video): a browser of the volumes' video files, then the player (video.cpp: the shell's
// GStreamer) drawing into a child window of ours over the screen's picture area; a tap on the picture: the whole
// screen and back. The picture pauses when another screen or program comes up, and while the car moves it is
// hidden (the sound goes on) unless the setting allows it. The playlist = the videos of the file's folder.
#define WM_APP_VIDTAP (WM_APP + 23)
#define VIDEO_CLASS L"VestaHome_Video"
enum { TIMER_VIDEO = 7, kVidMax = 400, kVidRows = 4, kVidCols = 2 };
static HWND     s_vidWnd;             // the picture
static wchar_t  s_vidDir[MAX_PATH];   // the browser's folder, "" = the volumes
static wchar_t* s_vids[kVidMax];      // its entries: folders ("\" at the end), then videos
static int      s_vidCount, s_vidPage;
static bool     s_vidRead;
static wchar_t  s_vpDir[MAX_PATH];    // the playing file's folder and its videos
static wchar_t* s_vp[kVidMax];
static int      s_vpCount, s_vpCur = -1;
static int      s_vidState;           // VID_* (video.h)
static bool     s_vidFull;            // the picture over the whole screen
static bool     s_vidMoving;          // the car moves and the picture is not allowed
static bool     s_vidParked;          // stopped by VidSync (see there), s_vidParkPos = where
static double   s_vidParkPos, s_vidSeekTo;
static wchar_t  s_vidMsg[160];        // why it doesn't play
static bool     s_vidShown;

static LRESULT CALLBACK VideoProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_ERASEBKGND: {   // black, as the stock player's window
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect((HDC)wp, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
        return 1;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        VideoExpose();   // the sink draws its frame again
        return 0;
    }
    case WM_LBUTTONUP:   // a tap on the picture: the whole screen and back
        PostMessage(s_wnd, WM_APP_VIDTAP, 0, 0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

static bool VidEnsureWnd()
{
    if (s_vidWnd)
        return true;
    WNDCLASS wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = VideoProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = VIDEO_CLASS;
    RegisterClass(&wc);
    s_vidWnd = CreateWindowEx(0, VIDEO_CLASS, L"", WS_CHILD, 0, 56, 800, 342, s_wnd, NULL, wc.hInstance, NULL);
    return s_vidWnd != NULL;
}

static bool VidPlay(int i);

// After every frame: the picture is shown only on the player's screen, with VestaHome in front, a video open and
// the car standing (or allowed). The picture is a hardware overlay: hiding our window leaves it on top of
// everything (HU 2026-10-18), so leaving the player (or the car moving) "parks" the video - stopped, the overlay
// gone, the position kept - and coming back opens it again from there.
static void VidSync()
{
    if (!s_vidWnd)
        return;
    bool open = s_vidState == VID_PLAYING || s_vidState == VID_PAUSED || s_vidState == VID_ENDED;
    bool front = IsWindowVisible(s_wnd) && GetForegroundWindow() == s_wnd;
    bool show = open && front && s_screen == S_VIDEO_PLAY && !s_vidMoving;
    if (open && !show && !s_vidParked) {
        double pos, len;
        VideoTimes(&pos, &len);
        VideoStop();
        s_vidParked = true;
        s_vidParkPos = s_vidState == VID_ENDED ? 0 : pos;
        s_vidState = VID_PAUSED;
    } else if (show && s_vidParked) {
        double pos = s_vidParkPos;
        if (VidPlay(s_vpCur))
            s_vidSeekTo = pos;   // VidTick seeks once the file is open
        return;
    }
    if (show) {
        if (s_vidFull)
            SetWindowPos(s_vidWnd, HWND_TOP, 0, 0, 800, 480, SWP_SHOWWINDOW);
        else
            SetWindowPos(s_vidWnd, HWND_TOP, 0, 56, 800, 342, SWP_SHOWWINDOW);
        if (!s_vidShown)
            VideoExpose();
    } else if (s_vidShown) {
        ShowWindow(s_vidWnd, SW_HIDE);
    }
    s_vidShown = show;
}

static void VidPauseForAudio()
{
    if (s_vidState == VID_PLAYING) {
        VideoPause(true);
        s_vidState = VID_PAUSED;
    }
}

static void VidOpenDir(const wchar_t* dir)
{
    FreeNames(s_vids, &s_vidCount);
    wcsncpy(s_vidDir, dir, MAX_PATH - 1);
    s_vidDir[MAX_PATH - 1] = 0;
    s_vidPage = 0;
    s_vidRead = true;
    s_vidCount = s_vidDir[0] ? ReadMusicDir(s_vidDir, s_vids, kVidMax, true, true) : ReadVolumes(s_vids);
}

static bool VidPlay(int i)
{
    if (i < 0 || i >= s_vpCount)
        return false;
    s_vpCur = i;
    s_vidFull = false;
    if (!VideoLoad(VLog, s_vidMsg, 160) || !VidEnsureWnd()) {
        if (!s_vidMsg[0])
            wcscpy(s_vidMsg, L"нет окна для видео");
        s_vidState = VID_ERROR;
        return false;
    }
    NetStop();
    if (PlayerPlaying())
        s_bass.pause(s_chan);
    SetWindowPos(s_vidWnd, HWND_TOP, 0, 56, 800, 342, SWP_SHOWWINDOW);   // the sink takes the window as it is
    s_vidShown = true;
    wchar_t path[MAX_PATH];
    _snwprintf(path, MAX_PATH, L"%s%s", s_vpDir, s_vp[i]);
    path[MAX_PATH - 1] = 0;
    s_vidMsg[0] = 0;
    s_vidParked = false;
    s_vidSeekTo = 0;
    if (!VideoOpen(path, s_vidWnd, s_cfg.videoStretch, s_vidMsg, 160)) {
        VLog(L"видео: %s не открылось: %s", path, s_vidMsg);
        s_vidState = VID_ERROR;
        return false;
    }
    VLog(L"видео: %s", path);
    s_vidState = VID_PLAYING;
    MediaSource();
    SetTimer(s_wnd, TIMER_VIDEO, 250, NULL);
    wcscpy(s_cfg.videoDir, s_vpDir);
    wcsncpy(s_cfg.videoFile, s_vp[i], MAX_PATH - 1);
    s_cfg.videoFile[MAX_PATH - 1] = 0;
    SaveCfg();
    return true;
}

// The playlist = the videos of `dir`, starting with `file`.
static bool VidLoadDir(const wchar_t* dir, const wchar_t* file)
{
    FreeNames(s_vp, &s_vpCount);
    wcsncpy(s_vpDir, dir, MAX_PATH - 1);
    s_vpDir[MAX_PATH - 1] = 0;
    s_vpCount = ReadMusicDir(s_vpDir, s_vp, kVidMax, false, true);
    s_vpCur = -1;
    int start = 0;
    for (int i = 0; file && i < s_vpCount; i++)
        if (!_wcsicmp(s_vp[i], file))
            start = i;
    return s_vpCount > 0 && VidPlay(start);
}

static void VidPick(int i)
{
    if (i < 0 || i >= s_vidCount)
        return;
    const wchar_t* n = s_vids[i];
    wchar_t d[MAX_PATH];
    if (n[wcslen(n) - 1] == L'\\') {   // a folder (or a volume)
        _snwprintf(d, MAX_PATH, L"%s%s", s_vidDir, n);
        d[MAX_PATH - 1] = 0;
        VidOpenDir(d);
        return;
    }
    wchar_t file[MAX_PATH];
    wcscpy(file, n);
    wcscpy(d, s_vidDir);
    if (s_vidParked && s_vpCur >= 0 && s_vpCur < s_vpCount && !_wcsicmp(d, s_vpDir) && !_wcsicmp(s_vp[s_vpCur], file)) {
        Go(S_VIDEO_PLAY, true);   // the video left a moment ago: on from where it was (VidSync)
        return;
    }
    VidLoadDir(d, file);
    Go(S_VIDEO_PLAY, true);
}

static void VidUp(bool toVolumes)
{
    wchar_t d[MAX_PATH];
    wcscpy(d, s_vidDir);
    if (toVolumes)
        d[0] = 0;
    else
        ParentDir(d);
    VidOpenDir(d);
}

static void VidToggle()
{
    if (s_vidParked)
        return;   // parked on this screen = the car moves
    if (s_vidState == VID_PLAYING) {
        VideoPause(true);
        s_vidState = VID_PAUSED;
    } else if (s_vidState == VID_PAUSED || s_vidState == VID_ENDED) {
        PlayerStopStream();
        NetStop();
        VideoPause(false);
        s_vidState = VID_PLAYING;
        MediaSource();
    } else if (s_cfg.videoDir[0]) {   // nothing open: the last video
        VidLoadDir(s_cfg.videoDir, s_cfg.videoFile);
    }
}

// ---- one source at a time (HU 2026-10-20: the radio stayed on under our music, Bluetooth played with it). The
// radio and Bluetooth music are the MCU's own sources; ours (music, internet radio, video) sound through waveOut
// only while the MCU's source is "media" (0x4904 06/07) - turned on as the shell's "USB music" does it:
// SendMediaSourceSelect(5, 3) after its 0x30.
static void OursQuiet()   // the radio or Bluetooth takes over: our players stop (pause)
{
    NetStop();
    if (PlayerPlaying())
        s_bass.pause(s_chan);
    VidPauseForAudio();
}

static void BtQuiet()   // the phone's music paused (inaudible under another source, it would still stream)
{
    BtState bt;
    if (BtGet(&bt, NULL) && bt.av == 2)
        BtCmd(BT_CMD_PAUSE);
}

static void MediaSource()   // one of our players starts
{
    BtQuiet();
    int vol, mute, src;
    VolGet(&vol, &mute, &src);
    if (src == 6 || src == 7 || src == 9)
        return;   // media, or Bluetooth's path: ours sound
    // Straight to Bluetooth's path: SendMediaSourceSelect never made it "media" by itself, and it woke the shell's
    // media player - with it our video stayed black, sound only (HU 2026-10-20-3; the picture was there on the
    // 18th, before the call): the hardware overlay is one.
    VLog(L"источник: путь Bluetooth для нашего звука (было %d)", src);
    McuKey(0x84, false);
    SetTimer(s_wnd, TIMER_BTSRC, 800, NULL);
    s_srcByUs = s_btPathTick = GetTickCount();
}


static void VidStep(int dir)
{
    if (s_vpCount > 0)
        VidPlay((s_vpCur + dir + s_vpCount) % s_vpCount);
}

static void VidSeek(int k)   // k of 20 along the bar
{
    double pos, len;
    if (VideoTimes(&pos, &len) && len > 0)
        VideoSeek(len * k / 20);
}

static int CarKmh()
{
    Gps g;
    GpsGet(&g);
    if (g.carTick && GetTickCount() - g.carTick < 3000)
        return g.carSpeed;
    return GpsFresh(g, 5000) ? (int)g.speedKmh : 0;
}

// Every 250 ms while a video is open: its bus, the end (the next file of the folder), the car's speed; the
// player's bar once a second.
static void VidTick()
{
    if (s_vidState == VID_NONE || s_vidState == VID_ERROR) {
        KillTimer(s_wnd, TIMER_VIDEO);
        return;
    }
    int was = s_vidState;
    if (!s_vidParked)   // parked: no pipeline, only the speed is watched (to come back when the car stops)
        s_vidState = VideoPoll(s_vidMsg, 160);
    if (s_vidSeekTo > 0 && s_vidState == VID_PLAYING) {   // back from parking: the position once it can seek
        double pos, len;
        if (VideoTimes(&pos, &len) && len > 0) {
            VideoSeek(s_vidSeekTo);
            s_vidSeekTo = 0;
        }
    }
    if (s_vidState == VID_ENDED && was != VID_ENDED && s_vpCur + 1 < s_vpCount) {
        VidPlay(s_vpCur + 1);
        return;
    }
    int kmh = CarKmh();
    bool moving = !s_cfg.videoMoving && (s_vidMoving ? kmh >= 3 : kmh >= 8);   // a margin against flicker
    bool redraw = moving != s_vidMoving || s_vidState != was;
    s_vidMoving = moving;
    static DWORD s_barTick;
    bool bar = false;
    if (s_screen == S_VIDEO_PLAY && GetTickCount() - s_barTick >= 1000) {
        s_barTick = GetTickCount();
        bar = true;
    }
    if (IsWindowVisible(s_wnd) && GetForegroundWindow() == s_wnd) {
        if (redraw)
            Redraw();
        else if (bar && !s_vidFull)
            RedrawArea(0, 398, 800, 480);   // the bar and the times
    }
}

static int VidPages()
{
    int per = kVidRows * kVidCols - (s_vidDir[0] ? 1 : 0);
    return s_vidCount ? (s_vidCount + per - 1) / per : 1;
}

// Media -> video: the volumes, then folders and videos; a file plays its folder from it. Pages: a swipe, the dots.
static void ScrVideo(HDC dc)
{
    if (!s_vidRead)
        VidOpenDir(s_cfg.videoDir[0] && GetFileAttributes(s_cfg.videoDir) != 0xFFFFFFFF ? s_cfg.videoDir : L"");
    wchar_t s[MAX_PATH + 16], folder[MAX_PATH];
    wcscpy(folder, s_vidDir);
    int fl = (int)wcslen(folder);
    if (fl && folder[fl - 1] == L'\\')
        folder[--fl] = 0;
    const wchar_t* fn = wcsrchr(folder, L'\\');
    _snwprintf(s, MAX_PATH + 16, L"МЕДИА · ВИДЕО · %s", !s_vidDir[0] ? L"НОСИТЕЛИ" : fn && fn[1] ? fn + 1 : folder);
    Frame(dc, s);
    MediaTabs(dc, S_VIDEO, IC_PLAY, A_SCREEN, S_VIDEO_PLAY, S_SET_VIDEO);
    if (s_vidPage >= VidPages())
        s_vidPage = VidPages() - 1;
    int cell = 0;
    if (s_vidDir[0]) {
        ListRow(dc, 16, 120, 380, 60, L"Наверх", NULL, L_N, IC_UP, NULL, NULL, A_VUP);
        cell = 1;
    }
    int per = kVidRows * kVidCols - cell, first = s_vidPage * per;
    for (int i = first; i < s_vidCount && i < first + per; i++, cell++) {
        int x = 16 + cell / kVidRows * 388, y = 120 + cell % kVidRows * 66;
        const wchar_t* n = s_vids[i];
        bool isDir = n[wcslen(n) - 1] == L'\\';
        bool cur = !isDir && s_vpCur >= 0 && !_wcsicmp(s_vpDir, s_vidDir) && !_wcsicmp(n, s_vp[s_vpCur]);
        wcsncpy(s, n, MAX_PATH);
        s[MAX_PATH] = 0;
        if (isDir && s[0] && s[wcslen(s) - 1] == L'\\')
            s[wcslen(s) - 1] = 0;
        ListRow(dc, x, y, 380, 60, s, NULL, cur ? L_CUR : L_N, isDir ? IC_FOLDER : IC_VIDEO, NULL, NULL, A_VFILE, i,
                isDir ? kOrange : CLR_INVALID);
    }
    if (!s_vidCount)
        T(dc, 40, s_vidDir[0] ? 236 : 168, s_vidDir[0] ? L"Здесь нет папок и видео" : L"Нет носителей", 18, kMuted, 400);
    PageDots(dc, s_vidPage, VidPages());
}

// The player: the picture (our child window) between the top bar and the dock: the bar (a tap seeks), the
// list, previous, play / pause, next, the whole screen, the settings.
static void ScrVideoPlay(HDC dc)
{
    const wchar_t* name = s_vpCur >= 0 ? s_vp[s_vpCur] : NULL;
    wchar_t s[MAX_PATH + 16];
    _snwprintf(s, MAX_PATH + 16, L"ВИДЕО · %s", name ? name : L"НЕТ ФАЙЛА");
    s[MAX_PATH + 15] = 0;
    Frame(dc, s);
    Fill(dc, 0, 56, 800, 398, RGB(0, 0, 0));
    if (s_vidState == VID_ERROR || s_vidState == VID_NONE) {
        T(dc, 400, 210, s_vidState == VID_ERROR ? L"ВИДЕО НЕ ИГРАЕТ" : L"НИЧЕГО НЕ ОТКРЫТО", 22, kText, 700, AL_MID, 720);
        T(dc, 400, 244, s_vidState == VID_ERROR ? s_vidMsg : L"Выберите файл в «Медиа → Видео»", 16, kMuted, 400,
          AL_MID, 720);
    } else if (s_vidMoving) {
        T(dc, 400, 210, L"В ДВИЖЕНИИ — ПАУЗА", 22, kText, 700, AL_MID, 720);
        T(dc, 400, 244, L"Картинка вернётся на остановке (или включите её в настройках видео)", 16, kMuted, 400,
          AL_MID, 720);
    }
    Dock(dc);
    double pos = 0, len = 0;
    bool have = (s_vidState == VID_PLAYING || s_vidState == VID_PAUSED || s_vidState == VID_ENDED) &&
                VideoTimes(&pos, &len);
    int k = len > 0 ? (int)(pos * 640 / len) : 0;
    R(dc, 80, 404, 640, 6, 3, kLine);
    if (have) {
        R(dc, 80, 404, k > 6 ? k : 6, 6, 3, kOrange);
        Disc(dc, 80 + k, 407, 8, kText);
        for (int z = 0; z < 20; z++)
            Zone(80 + z * 32, 392, 33, 28, A_VSEEK, z);
    }
    Mmss(s, 16, pos);
    T(dc, 16, 414, s, 15, kMuted, 600);
    Mmss(s, 16, len);
    T(dc, 784, 414, s, 15, kMuted, 600, AL_END);
    IconBtn(dc, IC_LIST, 24, 430, 40, kText, A_BACK);
    IconBtn(dc, IC_HOME, 124, 430, 40, kText, A_NAV, S_HOME);
    IconBtn(dc, IC_PREV, 236, 430, 40, kText, A_VPREV);
    bool down = Zone(376, 426, 48, 48, A_VPLAY);
    Disc(dc, 400, 450, 24, down ? kFacet : kOrange);
    Ico(dc, s_vidState == VID_PLAYING ? IC_PAUSE : IC_PLAY, 384, 434, 32, kInk);
    IconBtn(dc, IC_FWD, 524, 430, 40, kText, A_VNEXT);
    IconBtn(dc, IC_VIDEO, 632, 430, 40, s_vidFull ? kOrange : kText, A_VFULL);
    IconBtn(dc, IC_GEAR, 736, 430, 40, kText, A_SCREEN, S_SET_VIDEO);
}

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

// ---- weather: Open-Meteo (open, no key) or OpenWeatherMap (free key), both over HTTP; Yandex over HTTPS with the
// user's own key of "Yandex Weather for smart home" (free, personal use only; its key opens REST /v2/forecast, not
// GraphQL). wttr.in and Gismeteo dropped by the user's decision 2026-10-15. Needs a place: GPS or the home point.
// Refreshed every 30 minutes, on a tap on the widget and after a change of the settings.
enum { WX_SUN, WX_PART, WX_CLOUD, WX_RAIN, WX_SNOW, WX_STORM, WX_FOG };
struct Weather {
    int     ok;                    // 1 data, 0 none yet, -1 the last try failed (old data may stay)
    bool    have;                  // the values below are from some successful fetch
    int     temp, tmax, tmin, wind, kind;
    wchar_t desc[48], place[48], err[80], rise[8], set[8];
    SYSTEMTIME at;                 // local time of the fetch
    DWORD   tick;
    int     prov;                  // W_*: where the data came from (Open-Meteo: its credit, CC BY 4.0)
};
static CRITICAL_SECTION s_wxCs;
static Weather          s_wx;
static volatile LONG    s_wxBusy;
static DWORD            s_wxTry;      // tick of the last attempt
static int              s_wxProv;     // the request's source: the chosen one, Open-Meteo when Yandex's are used up
// Yandex "weather for smart home": 30 requests a day, 1000 a month (the user's tariff, 2026-10-15); 30 x 31 = 930,
// so the daily limit keeps the monthly one. Used up: Open-Meteo until the next day.
enum { kYaPerDay = 30 };

// WMO weather code (Open-Meteo): its picture, and its text into desc (48).
static int OmKind(int code, wchar_t* desc)
{
    static const struct { int upTo, kind; const wchar_t* text; } k[] = {
        { 0, WX_SUN, L"ясно" }, { 1, WX_PART, L"преимущественно ясно" }, { 2, WX_PART, L"переменная облачность" },
        { 3, WX_CLOUD, L"пасмурно" }, { 48, WX_FOG, L"туман" }, { 57, WX_RAIN, L"морось" }, { 67, WX_RAIN, L"дождь" },
        { 77, WX_SNOW, L"снег" }, { 82, WX_RAIN, L"ливень" }, { 86, WX_SNOW, L"снегопад" }, { 99, WX_STORM, L"гроза" } };
    for (int i = 0; i < (int)(sizeof(k) / sizeof(k[0])); i++)
        if (code <= k[i].upTo) {
            wcscpy(desc, k[i].text);
            return k[i].kind;
        }
    desc[0] = 0;
    return WX_CLOUD;
}

// Yandex's condition ("partly-cloudy"): its picture, and its text into desc (48).
static int YaKind(const wchar_t* cond, wchar_t* desc)
{
    static const struct { const wchar_t* key; int kind; const wchar_t* text; } k[] = {
        { L"clear", WX_SUN, L"ясно" }, { L"partly-cloudy", WX_PART, L"малооблачно" },
        { L"cloudy", WX_CLOUD, L"облачно с прояснениями" }, { L"overcast", WX_CLOUD, L"пасмурно" },
        { L"fog", WX_FOG, L"туман" }, { L"drizzle", WX_RAIN, L"морось" }, { L"light-rain", WX_RAIN, L"небольшой дождь" },
        { L"rain", WX_RAIN, L"дождь" }, { L"moderate-rain", WX_RAIN, L"умеренный дождь" },
        { L"heavy-rain", WX_RAIN, L"сильный дождь" }, { L"continuous-heavy-rain", WX_RAIN, L"затяжной дождь" },
        { L"showers", WX_RAIN, L"ливень" }, { L"wet-snow", WX_SNOW, L"дождь со снегом" },
        { L"light-snow", WX_SNOW, L"небольшой снег" }, { L"snow", WX_SNOW, L"снег" },
        { L"snow-showers", WX_SNOW, L"снегопад" }, { L"hail", WX_STORM, L"град" },
        { L"thunderstorm", WX_STORM, L"гроза" }, { L"thunderstorm-with-rain", WX_STORM, L"дождь с грозой" },
        { L"thunderstorm-with-hail", WX_STORM, L"гроза с градом" } };
    for (int i = 0; i < (int)(sizeof(k) / sizeof(k[0])); i++)
        if (!wcscmp(cond, k[i].key)) {
            wcscpy(desc, k[i].text);
            return k[i].kind;
        }
    desc[0] = 0;
    return WX_CLOUD;
}

// The first number of an array value ("key":[21.7]).
static double JsonFirst(const char* p, const char* key, double def)
{
    p = JsonAt(p, key);
    return p && *p == '[' ? atof(p + 1) : def;
}

// The first time of an array value ("key":["2026-10-15T07:05"]) -> "7:05".
static void IsoClock(const char* p, const char* key, wchar_t* out)
{
    p = JsonAt(p, key);
    const char* t = p && *p == '[' ? strchr(p, 'T') : NULL;
    if (t && t - p < 16)
        _snwprintf(out, 8, L"%d:%02d", atoi(t + 1), atoi(t + 4));
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

static void UnixClock(double t, double tz, wchar_t* out)
{
    long s = (long)fmod(t + tz, 86400.0);
    _snwprintf(out, 8, L"%ld:%02ld", s / 3600, s / 60 % 60);
}

static DWORD WINAPI WeatherThread(LPVOID)
{
    VLog(L"поток погоды: id %08X", (unsigned)GetCurrentThreadId());
    Weather w;
    memset(&w, 0, sizeof(w));
    int prov = s_wxProv;
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
    if (prov == W_OMETEO) {
        // api.open-meteo.com does not answer from some Russian networks (2026-10-15: a timeout by HTTP and HTTPS,
        // its site works); the same service under historical-forecast-api.open-meteo.com does. The host that
        // answered last is tried first.
        static const char* hosts[2] = { "api.open-meteo.com", "historical-forecast-api.open-meteo.com" };
        static int s_omHost;
        if (!here && !home) {
            wcscpy(w.err, L"нет места: GPS или домашняя точка");
        } else {
            _snprintf(path, sizeof(path), "/v1/forecast?latitude=%.3f&longitude=%.3f&current=temperature_2m,"
                      "weather_code,wind_speed_10m&daily=temperature_2m_max,temperature_2m_min,sunrise,sunset"
                      "&wind_speed_unit=ms&timezone=auto&forecast_days=1", lat, lon);
            for (int i = 0; i < 2 && !body; i++) {
                int h = (s_omHost + i) % 2;
                body = Fetch(hosts[h], false, path, NULL, w.err, 80);
                if (body)
                    s_omHost = h;
            }
            if (body) {
                const char* cur = JsonAt(body, "current");   // after "current_units", which has the same keys
                const char* day = JsonAt(body, "daily");
                w.temp = (int)floor(JsonNum(cur, "temperature_2m", 0) + 0.5);
                w.wind = (int)(JsonNum(cur, "wind_speed_10m", 0) + 0.5);
                w.kind = OmKind((int)JsonNum(cur, "weather_code", 0), w.desc);
                w.tmax = (int)floor(JsonFirst(day, "temperature_2m_max", w.temp) + 0.5);
                w.tmin = (int)floor(JsonFirst(day, "temperature_2m_min", w.temp) + 0.5);
                IsoClock(day, "sunrise", w.rise);
                IsoClock(day, "sunset", w.set);
                if (here)
                    wcscpy(w.place, L"по GPS");   // Open-Meteo gives no place name
                w.ok = 1;
            }
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
    } else if (!key[0]) {
        wcscpy(w.err, L"нужен ключ Яндекс Погоды");
    } else if (!here && !home) {
        wcscpy(w.err, L"нет места: GPS или домашняя точка");
    } else {
        char hdr[120];
        _snprintf(hdr, sizeof(hdr), "X-Yandex-Weather-Key: %s\r\n", key);
        _snprintf(path, sizeof(path), "/v2/forecast?lat=%.3f&lon=%.3f&limit=1&hours=false&extra=false", lat, lon);
        body = Fetch("api.weather.yandex.ru", true, path, hdr, w.err, 80);
        if (body) {
            const char* fact = JsonAt(body, "fact");
            const char* fc = JsonAt(body, "forecasts");
            const char* ds = JsonAt(fc, "day_short");
            w.temp = (int)floor(JsonNum(fact, "temp", 0) + 0.5);
            w.wind = (int)(JsonNum(fact, "wind_speed", 0) + 0.5);
            wchar_t cond[32] = L"";
            JsonStr(fact, "condition", cond, 32);
            w.kind = YaKind(cond, w.desc);
            w.tmax = (int)floor(JsonNum(ds, "temp", w.temp) + 0.5);
            w.tmin = (int)floor(JsonNum(ds, "temp_min", w.temp) + 0.5);
            JsonStr(fc, "sunrise", w.rise, 8);
            JsonStr(fc, "sunset", w.set, 8);
            if (w.rise[0] == L'0')   // "07:05" -> "7:05", like the other sources
                memmove(w.rise, w.rise + 1, 7 * sizeof(wchar_t));
            if (w.set[0] == L'0')
                memmove(w.set, w.set + 1, 7 * sizeof(wchar_t));
            if (here)
                wcscpy(w.place, L"по GPS");   // no place name without the paid "extra" data
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
        w.prov = prov;
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

// One of today's Yandex requests, counted in the ini (a restart doesn't reset it); false = today's are used up,
// or the clock is not set yet (it resets on boot until GPS sets it: the count's day is unknown).
static bool YaTake()
{
    SYSTEMTIME t;
    GetLocalTime(&t);
    if (t.wYear < 2025)
        return false;
    int day = t.wYear * 10000 + t.wMonth * 100 + t.wDay;
    if (s_cfg.yaDay != day) {
        s_cfg.yaDay = day;
        s_cfg.yaCount = 0;
    }
    if (s_cfg.yaCount >= kYaPerDay)
        return false;
    s_cfg.yaCount++;
    SaveCfg();
    return true;
}

static void WeatherRefresh()
{
    if (InterlockedExchange((LONG*)&s_wxBusy, 1))
        return;
    s_wxTry = GetTickCount();
    s_wxProv = s_cfg.weatherProv;
    if (s_wxProv == W_YANDEX && s_cfg.weatherKey[W_YANDEX][0] && !YaTake()) {
        VLog(L"погода: запросы Яндекса на сегодня кончились (%d из %d) или часы не выставлены — Open-Meteo",
             s_cfg.yaCount, kYaPerDay);
        s_wxProv = W_OMETEO;
    }
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

// A sound over the radio, as an external navigator says its prompts (Ghidra, PROJECT.md): WM_NAVIGATION_VOICE
// (0x101D, wParam 1 = a prompt starts, 0 = ended) to the stock shell's main window "RadioCommWindow"; the shell
// tells the MCU (SPApi_SendRadioNaviVoiceGuidance), which turns the radio down and lets our waveOut through.
// The sound starts kVoiceLeadMs after "1" (the MCU turns the radio down meanwhile), "0" goes kVoiceTailMs after
// it ends. No shell window: played at once. A device that never says "done" (Bluetooth HF without a phone, HU
// 2026-10-18: the "1" stayed on, the knob then set the navigation's volume) is cut after kVoiceMaxMs.
enum { TIMER_VOICE = 9, WM_NAVIGATION_VOICE = 0x101D, kVoiceLeadMs = 1000, kVoiceTailMs = 800, kVoiceMaxMs = 1500 };
static int s_voice;   // 0 idle, 1 waiting to play, 2 playing, 3 waiting to end

static bool NaviVoice(bool on)
{
    HWND h = FindWindow(NULL, L"RadioCommWindow");
    if (h)
        PostMessage(h, WM_NAVIGATION_VOICE, on ? 1 : 0, 0);
    static int s_logged = -1;
    if (s_logged != (h != NULL)) {
        s_logged = h != NULL;
        VLog(L"звук поверх радио: %s", h ? L"через оболочку (RadioCommWindow, 0x101D)" : L"окна оболочки нет — сразу");
    }
    return h != NULL;
}

static HWAVEOUT s_beepOut;
static WAVEHDR  s_beepHdr;
static short    s_beep[11025 / 2];   // 2 x 120 ms at 1.2 kHz with a gap, 11 kHz mono

static void BeepPlay();

static void Beep2()
{
    if (s_voice)
        return;   // one is on its way
    if (NaviVoice(true)) {
        s_voice = 1;
        SetTimer(s_wnd, TIMER_VOICE, kVoiceLeadMs, NULL);
    } else {
        BeepPlay();
    }
}

// TIMER_VOICE: the lead is over (play) or the tail (the prompt has ended).
static void VoiceTimer()
{
    KillTimer(s_wnd, TIMER_VOICE);
    if (s_voice == 1) {
        s_voice = 2;
        SetTimer(s_wnd, TIMER_VOICE, kVoiceMaxMs, NULL);   // the guard (BeepPlay may end it at once: VoiceDone)
        BeepPlay();
    } else if (s_voice == 2) {   // no "done" from the device
        if (s_beepOut)
            waveOutReset(s_beepOut);
        s_voice = 3;
        SetTimer(s_wnd, TIMER_VOICE, kVoiceTailMs, NULL);
    } else if (s_voice == 3) {
        s_voice = 0;
        NaviVoice(false);
    }
}

// MM_WOM_DONE: the sound has been played.
static void VoiceDone()
{
    if (s_voice == 2) {
        s_voice = 3;
        SetTimer(s_wnd, TIMER_VOICE, kVoiceTailMs, NULL);
    }
}

static void BeepPlay()
{
    if (s_beepOut) {
        if (!(s_beepHdr.dwFlags & WHDR_DONE))
            waveOutReset(s_beepOut);   // the last one hung: dropped
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
    UINT dev = s_cfg.beepDev < 0 ? WAVE_MAPPER : (UINT)s_cfg.beepDev;
    if (waveOutOpen(&s_beepOut, dev, &wf, (DWORD)s_wnd, 0, CALLBACK_WINDOW) != MMSYSERR_NOERROR) {
        s_beepOut = NULL;
        VoiceDone();
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

// The keypad's tones (DTMF, 120 ms) on "Audio Output" 1 - the user's choice (2026-10-20), as the beep's device.
static HWAVEOUT s_dtmfOut;
static WAVEHDR  s_dtmfHdr;
static short    s_dtmf[11025 * 12 / 100];

static void DtmfPlay(wchar_t k)
{
    static const wchar_t* keys = L"123456789*0#";
    static const int row[4] = { 697, 770, 852, 941 }, col[3] = { 1209, 1336, 1477 };
    const wchar_t* at = wcschr(keys, k == L'+' ? L'0' : k);
    if (!at || !k)
        return;
    int i = (int)(at - keys), f1 = row[i / 3], f2 = col[i % 3];
    if (s_dtmfOut) {
        waveOutReset(s_dtmfOut);
        waveOutUnprepareHeader(s_dtmfOut, &s_dtmfHdr, sizeof(s_dtmfHdr));
        waveOutClose(s_dtmfOut);
        s_dtmfOut = NULL;
    }
    WAVEFORMATEX wf;
    memset(&wf, 0, sizeof(wf));
    wf.wFormatTag = WAVE_FORMAT_PCM;
    wf.nChannels = 1;
    wf.nSamplesPerSec = 11025;
    wf.wBitsPerSample = 16;
    wf.nBlockAlign = 2;
    wf.nAvgBytesPerSec = 22050;
    if (waveOutOpen(&s_dtmfOut, 1, &wf, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        s_dtmfOut = NULL;
        return;
    }
    int n = sizeof(s_dtmf) / sizeof(s_dtmf[0]);
    for (int j = 0; j < n; j++) {
        double a = j < 110 ? j / 110.0 : j > n - 110 ? (n - j) / 110.0 : 1.0;   // 10 ms in and out: no clicks
        s_dtmf[j] = (short)(a * 5000 * (sin(j * 2 * 3.14159265 * f1 / 11025) + sin(j * 2 * 3.14159265 * f2 / 11025)));
    }
    memset(&s_dtmfHdr, 0, sizeof(s_dtmfHdr));
    s_dtmfHdr.lpData = (LPSTR)s_dtmf;
    s_dtmfHdr.dwBufferLength = sizeof(s_dtmf);
    waveOutPrepareHeader(s_dtmfOut, &s_dtmfHdr, sizeof(s_dtmfHdr));
    waveOutWrite(s_dtmfOut, &s_dtmfHdr, sizeof(s_dtmfHdr));
}

// Diagnostics -> "Sound and video": the beep over the radio and short test files (VestaHome/tools/
// make_testmedia.py -> Tests\SDMemory\TestMedia) played by our own players. The folder: TestMedia on the SD card,
// a USB disk or next to the exe.
struct AvTest { const wchar_t* label; const wchar_t* file; bool video; };
static const AvTest kAvTests[10] = {
    { L"ПИСК ПОВЕРХ РАДИО", NULL, false },     { L"ЛЕВО → ПРАВО (MP3)", L"lr.mp3", false },
    { L"СВИП 40 Гц – 12 кГц", L"sweep.mp3", false }, { L"MP3 + теги", L"test.mp3", false },
    { L"M4A (AAC)", L"test.m4a", false },      { L"FLAC", L"test.flac", false },
    { L"WAV", L"test.wav", false },            { L"ВИДЕО MP4 (H.264)", L"test.mp4", true },
    { L"ВИДЕО AVI (XviD)", L"test.avi", true }, { L"ВИДЕО MKV (H.264)", L"test.mkv", true } };

// The TestMedia folder with a trailing '\', or "" when there is none.
static const wchar_t* AvDir()
{
    static wchar_t d[MAX_PATH];
    const wchar_t* roots[4] = { L"\\SDMemory\\", L"\\USBDisk\\", L"\\USBDevice\\", s_dir };
    for (int i = 0; i < 4; i++) {
        _snwprintf(d, MAX_PATH, L"%sTestMedia\\", roots[i]);
        d[MAX_PATH - 1] = 0;
        if (GetFileAttributes(d) != 0xFFFFFFFF)
            return d;
    }
    d[0] = 0;
    return d;
}

static bool AvHas(const wchar_t* dir, const wchar_t* file)
{
    wchar_t p[MAX_PATH];
    _snwprintf(p, MAX_PATH, L"%s%s", dir, file);
    p[MAX_PATH - 1] = 0;
    return dir[0] && GetFileAttributes(p) != 0xFFFFFFFF;
}

// waveOut devices: "АВТО" (WAVE_MAPPER) or "<n> · <its name>". The unit has more than one (wav_proxy.dll: DSP and
// ONCHIP, Bluetooth, iPod): the navigation prompt may need another one than the default.
static void WaveDevName(int dev, wchar_t* s, int n)
{
    WAVEOUTCAPS c;
    if (dev < 0)
        _snwprintf(s, n, L"АВТО (из %u)", (unsigned)waveOutGetNumDevs());
    else if (waveOutGetDevCaps((UINT)dev, &c, sizeof(c)) == MMSYSERR_NOERROR)
        _snwprintf(s, n, L"%d · %s", dev, c.szPname);
    else
        _snwprintf(s, n, L"%d", dev);
    s[n - 1] = 0;
}

static void WaveDevsLog()
{
    UINT n = waveOutGetNumDevs();
    for (UINT i = 0; i < n; i++) {
        wchar_t s[64];
        WaveDevName((int)i, s, 64);
        VLog(L"звук: устройство %s", s);
    }
    if (!n)
        VLog(L"звук: устройств waveOut нет");
}

static void ScrTestAv(HDC dc)
{
    Frame(dc, L"ЗВУК И ВИДЕО");
    Panel(dc, 16, 68, 768, 396);
    const wchar_t* dir = AvDir();
    for (int i = 0; i < 10; i++) {
        bool ok = !kAvTests[i].file || AvHas(dir, kAvTests[i].file);
        Button(dc, 40 + (i % 2) * 368, 84 + (i / 2) * 54, 352, 46, kAvTests[i].label, ok ? B_N : B_DIS, IC_NONE,
               16, 0, ok ? A_AVTEST : A_NONE, i);
    }
    wchar_t s[160];
    WaveDevName(s_cfg.beepDev, s, 64);
    wchar_t b[96];
    _snwprintf(b, 96, L"УСТРОЙСТВО ПИСКА: %s", s);
    Button(dc, 40, 356, 720, 42, b, B_N, IC_NONE, 15, 0, A_AVDEV);   // the next device and a beep on it
    if (dir[0])
        _snwprintf(s, 160, L"Файлы: %s. Писк — через канал голоса навигации (радио тише).", dir);
    else
        wcscpy(s, L"Нет папки TestMedia (на SD-карте, USB или рядом с VestaHome) — работает только писк.");
    T(dc, 40, 420, s, 14, kDim, 400, AL_START, 720);
    T(dc, 40, 444, L"Лево → право: 3 с слева, 3 с справа, 3 с оба. Свип — для проверки эквалайзера.", 14, kDim,
      400, AL_START, 720);
}

static void AvDevNext()
{
    int n = (int)waveOutGetNumDevs();
    s_cfg.beepDev = s_cfg.beepDev + 1 >= n ? -1 : s_cfg.beepDev + 1;
    SaveCfg();
    wchar_t s[64];
    WaveDevName(s_cfg.beepDev, s, 64);
    VLog(L"звук: писк на устройство %s", s);
    Beep2();
}

static void AvTestRun(int i)
{
    if (i < 0 || i >= 10)
        return;
    if (!kAvTests[i].file) {
        Beep2();
        return;
    }
    const wchar_t* dir = AvDir();
    if (!AvHas(dir, kAvTests[i].file))
        return;
    wchar_t d[MAX_PATH];
    wcscpy(d, dir);
    if (kAvTests[i].video) {
        VidLoadDir(d, kAvTests[i].file);
        Go(S_VIDEO_PLAY, true);
        return;
    }
    if (!BassLoad()) {
        wcscpy(s_status, s_bass.state == -1 ? L"нет bass.dll: папка bass рядом с VestaHome" : L"звук не запускается (BASS_Init)");
        return;
    }
    if (PlayerLoadDir(d, kAvTests[i].file))
        Go(S_AUDIO, true);
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

// The beep for the camera ahead, once per camera: at once, or (cam.over) only while the car goes faster than
// the camera's limit + cam.over km/h - then it comes when the speed gets there, while the camera is still ahead.
// A camera without a known limit beeps only in the "always" mode.
static bool s_camBeeped;

static void CamSound()
{
    if (s_camNear < 0 || s_camBeeped || !s_cfg.camSound)
        return;
    int lim = s_cams[s_camNear].speed;
    if (s_cfg.camOver >= 0 && (!lim || CarKmh() <= lim + s_cfg.camOver))
        return;   // not over the limit (yet)
    s_camBeeped = true;
    Beep2();
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
            CamSound();   // the speed may have gone over the limit since
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
    if (s_camNear != was)
        s_camBeeped = false;   // a new camera: its own beep
    CamSound();
    return s_camNear != was || s_camNearDist / 50 != wasDist;
}

static bool OutsideTemp(int* t)
{
    Hvac h;
    if (HvacGet(&h) && h.outer > -100) {   // the car's own sensor
        *t = h.outer;
        return true;
    }
    Weather w;
    WxGet(&w);
    *t = w.temp;
    return w.have;
}

// "КЛИМАТ 7/16 · ВЕНТ 3 · A/C" for the plate.
static bool HvacChip(wchar_t* s, int n)
{
    Hvac h;
    if (!HvacGet(&h))
        return false;
    wchar_t t[16] = L"", b[16] = L"";
    if (h.temp >= 0)
        _snwprintf(t, 16, L" %d/16", h.temp);
    if (h.blower >= 0)
        _snwprintf(b, 16, L" · ВЕНТ %d", h.blower);
    _snwprintf(s, n, L"КЛИМАТ%s%s%s%s", t, b, h.ac == 1 ? L" · A/C" : L"", h.autoMode == 2 ? L" · АВТО" : L"");
    s[n - 1] = 0;
    return true;
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
enum { TIMER_LONG = 6, kLongMs = 600 };   // a long press (the keyboard's text field)
static DWORD s_waitPid;
static int   s_waitApp = -1;   // -2: a program not in the list (a widget's tap=), found by s_waitPid
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
static bool    s_clipMenu;     // "Copy" / "Paste" shown in place of the text field

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
    static const App none = App();
    const App& a = s_waitApp >= 0 ? s_apps[s_waitApp] : none;
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

// A program's window on top of everything, guarded while its program runs.
static void RaiseWindow(HWND h)
{
    SetWindowPos(h, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    SetForegroundWindow(h);
    if (s_raisedCount < kMaxRaised)
        s_raised[s_raisedCount++] = h;
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    StartGuard(pid);
}

static void OnWaitTimer()
{
    if (s_waitApp == -1) {
        KillTimer(s_wnd, TIMER_WAIT);
        return;
    }
    FindApp f = { NULL };
    EnumWindows(EnumApp, (LPARAM)&f);
    if (f.found) {
        KillTimer(s_wnd, TIMER_WAIT);
        RaiseWindow(f.found);
        s_status[0] = 0;
        s_waitApp = -1;
    } else if (GetTickCount() - s_waitTick > kWaitMs) {
        KillTimer(s_wnd, TIMER_WAIT);
        _snwprintf(s_status, 96, L"окно не найдено: %s", s_waitApp >= 0 ? s_apps[s_waitApp].name : L"программа");
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
    s_clipMenu = false;
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

// ---- the clipboard on the keyboard screen: a long press on the text field (TIMER_LONG) shows "Copy",
// "Paste" and "Clear" in its place. The text is copied whole; pasting adds to its end (the cursor is always there),
// without line breaks and tabs, up to the field's length.

static void ClipCopy()
{
    int n = (int)wcslen(s_keysText) + 1;
    bool ok = false;
    if (OpenClipboard(s_wnd)) {
        EmptyClipboard();
#ifdef UNDER_CE
        HANDLE h = LocalAlloc(LMEM_FIXED, n * sizeof(wchar_t));   // CE: the clipboard takes local memory as is
        if (h)
            memcpy(h, s_keysText, n * sizeof(wchar_t));
#else
        HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, n * sizeof(wchar_t));
        if (h) {
            memcpy(GlobalLock(h), s_keysText, n * sizeof(wchar_t));
            GlobalUnlock(h);
        }
#endif
        ok = h && SetClipboardData(CF_UNICODETEXT, h);
        CloseClipboard();
    }
    wcscpy(s_status, ok ? L"скопировано" : L"не скопировано");
}

static void ClipPaste()
{
    int n = (int)wcslen(s_keysText), cap = s_keysCap;
    bool got = false;
    if (OpenClipboard(s_wnd)) {
        HANDLE h = GetClipboardData(CF_UNICODETEXT);
#ifdef UNDER_CE
        const wchar_t* p = (const wchar_t*)h;
#else
        const wchar_t* p = h ? (const wchar_t*)GlobalLock(h) : NULL;
#endif
        for (; p && *p && n < cap; p++)
            if (*p >= L' ') {
                s_keysText[n++] = *p;
                got = true;
            }
        s_keysText[n] = 0;
#ifndef UNDER_CE
        if (h)
            GlobalUnlock(h);
#endif
        CloseClipboard();
    }
    wcscpy(s_status, got ? L"вставлено" : L"в буфере нет текста");
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
    Frame(dc, s_editMode ? L"ПРИЛОЖЕНИЯ · ИЗМЕНЕНИЕ" : L"ПРИЛОЖЕНИЯ", A_BACK, IC_EDIT, A_EDITMODE, s_editMode,
          IC_TASKS, A_SCREEN, S_TASKS);
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
    if (s_clipMenu) {   // after a long press on the field, in its place; any other tap closes it
        Button(dc, 16, 68, 194, 60, L"КОПИРОВАТЬ", s_keysText[0] ? B_FILL : B_DIS, IC_NONE, 17, 0,
               s_keysText[0] ? A_CLIPCOPY : A_NONE);
        Button(dc, 219, 68, 194, 60, L"ВСТАВИТЬ", B_FILL, IC_NONE, 17, 0, A_CLIPPASTE);
        Button(dc, 422, 68, 194, 60, L"ОЧИСТИТЬ", s_keysText[0] ? B_FILL : B_DIS, IC_NONE, 17, 0,
               s_keysText[0] ? A_CLIPCLEAR : A_NONE);
    } else {
        Zone(16, 68, 600, 60, A_KEYFIELD);   // a long press: copy / paste (TIMER_LONG)
        R(dc, 16, 68, 600, 60, 10, kPanel, kLine);
        int end = TTail(dc, 32, 106, s_keysText, 26, kText, 700, 560);
        R(dc, end + 4, 84, 3, 28, 0, kOrange);
    }
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
        int x = 16 + (k / 5) * 388, y = 96 + (k % 5) * 56;
        bool d = Zone(x, y, 380, 50, A_FILE, first + k);
        R(dc, x, y, 380, 50, 10, d ? kOrange : kPanel);
        Ico(dc, f.dir ? IC_FOLDER : IC_FILE, x + 14, y + 10, 30, d ? kInk : f.dir ? kOrange : kMuted);   // folders stand out
        T(dc, x + 58, y + 32, f.name, 18, d ? kInk : kText, 700, AL_START, 310);
    }
    if (s_fileCount == 0)
        T(dc, 400, 240, s_filesFor == F_ICON ? L"Здесь нет папок и иконок" : s_filesFor == F_CAMS ?
          L"Здесь нет папок и .txt" : L"Здесь нет папок и программ", 22, kText, 700, AL_MID);
    PageDots(dc, s_filePage, FilePages(), 400, 382);
    Dock(dc);
    Button(dc, 16, 410, 120, 58, NULL, B_N, IC_UP, 18, 0, A_UP);
    Button(dc, 664, 410, 120, 58, L"ОТМЕНА", B_N, IC_NONE, 18, 0, A_CANCEL);
}

// ---------------------------------------------------------------- the main screen: widgets
// Two pages of five widgets (3 x 2 grid of 248 x 158 cells, the first of each page two cells wide), swiped or
// turned with the dots. A widget opens its screen; small buttons inside it act on their own; a caption with a gear
// opens the widget's settings. Real: the date, the programs, the weather, the speed (GPS) and the cameras; the
// rest are demo values until the car's bus through the MCU is known.

// ---- task manager (Apps -> the button in the top bar): processes and windows (VestaCore vtask), the CPU
// and memory graphs over the last 2 minutes. Sampled every 2 s while the screen is in front, never in the
// background. A process ends on the second press of its button; system ones and VestaHome itself can't.

enum { kTaskRows = 5 };
static TaskInfo s_task;
static int      s_taskTab;       // 0 processes, 1 windows
static int      s_taskPage;
static DWORD    s_taskArm;       // the process whose "End" was pressed once
static int      s_taskTicks;

static void TasksOpen()
{
    if (!s_task.procCount && !s_task.histCount) {   // the first time: the busiest first
        s_task.sortKey = TS_CPU;
        s_task.sortDesc = true;
    }
    s_taskPage = 0;
    s_taskArm = 0;
    TaskSample(&s_task, s_wnd);
}

static int TaskPages()
{
    int n = s_taskTab ? s_task.winCount : s_task.procCount;
    return n ? (n + kTaskRows - 1) / kTaskRows : 1;
}

static void MemText(wchar_t* s, int n, DWORD kb)
{
    if (kb < 1024)
        _snwprintf(s, n, L"%u КБ", (unsigned)kb);
    else
        _snwprintf(s, n, L"%u,%u МБ", (unsigned)(kb / 1024), (unsigned)(kb % 1024 * 10 / 1024));
    s[n - 1] = 0;
}

static void TaskEnd(DWORD pid)
{
    int i = 0;
    while (i < s_task.procCount && s_task.proc[i].pid != pid)
        i++;
    if (i == s_task.procCount)
        return;
    wchar_t name[40];
    wcscpy(name, s_task.proc[i].name);
    if (s_taskArm != pid) {
        s_taskArm = pid;
        _snwprintf(s_status, 96, L"ещё раз, чтобы завершить %s", name);
        return;
    }
    s_taskArm = 0;
    if (TaskKill(pid, name)) {
        _snwprintf(s_status, 96, L"завершён: %s", name);
        memmove(&s_task.proc[i], &s_task.proc[i + 1], (s_task.procCount - i - 1) * sizeof(TaskProc));
        s_task.procCount--;
    } else {
        _snwprintf(s_status, 96, L"не завершается: %s", name);
    }
}

static int TaskWinIndex(HWND h)
{
    for (int i = 0; i < s_task.winCount; i++)
        if (s_task.win[i].h == h)
            return i;
    return -1;
}

static void TaskSwitch(HWND h)
{
    int i = TaskWinIndex(h);
    if (i < 0 || !IsWindow(h)) {
        wcscpy(s_status, L"окна уже нет");
        return;
    }
    s_status[0] = 0;
    RaiseWindow(h);
}

static void TaskClose(HWND h)
{
    int i = TaskWinIndex(h);
    if (i < 0 || !IsWindow(h)) {
        wcscpy(s_status, L"окна уже нет");
        return;
    }
    PostMessage(h, WM_CLOSE, 0, 0);
    _snwprintf(s_status, 96, L"закрывается: %s", s_task.win[i].title);
    memmove(&s_task.win[i], &s_task.win[i + 1], (s_task.winCount - i - 1) * sizeof(TaskWin));
    s_task.winCount--;
}

// A graph panel: caption, value, the history (percent, newest at the right edge), a line at 50 %.
static void TaskGraph(HDC dc, int x, int y, int w, const wchar_t* caption, const wchar_t* value,
                      const unsigned char* hist, COLORREF c)
{
    R(dc, x, y, w, 104, 14, kPanel, kLine);
    T(dc, x + 14, y + 26, caption, 15, kMuted, 700);
    T(dc, x + w - 14, y + 26, value, 15, kText, 700, AL_END, w - 120);
    int gx = x + 14, gy = y + 36, gw = w - 28, gh = 56;
    Fill(dc, gx, gy + gh / 2, gx + gw, gy + gh / 2 + 1, kLine);
    Fill(dc, gx, gy + gh, gx + gw, gy + gh + 2, kLine);
    int n = s_task.histCount;
    if (n < 2)
        return;
    POINT p[TASK_HIST];
    for (int i = 0; i < n; i++) {
        p[i].x = gx + (TASK_HIST - n + i) * gw / (TASK_HIST - 1);
        p[i].y = gy + gh - hist[i] * gh / 100;
    }
    HPEN pen = CreatePen(PS_SOLID, 2, C(c));
    HGDIOBJ old = SelectObject(dc, pen);
    Polyline(dc, p, n);
    SelectObject(dc, old);
    DeleteObject(pen);
}

// A sort button; the current one is orange with a small triangle: down = the biggest first.
static void TaskSortBtn(HDC dc, int x, const wchar_t* label, int key)
{
    bool cur = s_task.sortKey == key;
    Button(dc, x, 176, 104, 48, label, cur ? B_SEL : B_N, IC_NONE, 16, 0, A_TASKSORT, key);
    if (!cur)
        return;
    int cx = x + 92, cy = 200, d = s_task.sortDesc ? 1 : -1;
    POINT t[3] = { { cx - 5, cy - 3 * d }, { cx + 5, cy - 3 * d }, { cx, cy + 3 * d } };
    Poly(dc, t, 3, kInk);
}

static void DrawTasks(HDC dc)
{
    wchar_t s[96], a[24], b[24];
    Frame(dc, L"ДИСПЕТЧЕР ЗАДАЧ");
    if (s_task.cpu10 >= 0)
        _snwprintf(s, 96, L"%d %%", (s_task.cpu10 + 5) / 10);
    else
        wcscpy(s, L"...");
    TaskGraph(dc, 16, 64, 380, L"ПРОЦЕССОР", s, s_task.cpuHist, kOrange);
    DWORD total = s_task.memTotalKB, used = total - s_task.memAvailKB;
    MemText(a, 24, used);
    MemText(b, 24, total);
    _snwprintf(s, 96, L"%s из %s · %u %%", a, b, total ? (unsigned)(used * 100 / total) : 0);
    TaskGraph(dc, 404, 64, 380, L"ПАМЯТЬ", s, s_task.memHist, kText);

    _snwprintf(s, 96, L"ПРОЦЕССЫ %d", s_task.procCount);
    Button(dc, 16, 176, 172, 48, s, s_taskTab == 0 ? B_SEL : B_N, IC_NONE, 16, 0, A_TASKTAB, 0);
    _snwprintf(s, 96, L"ОКНА %d", s_task.winCount);
    Button(dc, 196, 176, 128, 48, s, s_taskTab == 1 ? B_SEL : B_N, IC_NONE, 16, 0, A_TASKTAB, 1);
    if (s_taskTab == 0) {
        TaskSortBtn(dc, 336, L"ИМЯ", TS_NAME);
        TaskSortBtn(dc, 446, L"ЦП", TS_CPU);
        TaskSortBtn(dc, 556, L"ПАМЯТЬ", TS_MEM);
    }
    int pages = TaskPages();
    if (s_taskPage >= pages)
        s_taskPage = pages - 1;
    Button(dc, 668, 176, 40, 48, NULL, B_N, IC_BACK, 18, 24, A_TASKPAGE, -1);
    _snwprintf(s, 96, L"%d/%d", s_taskPage + 1, pages);
    T(dc, 726, 207, s, 15, kMuted, 600, AL_MID);
    Button(dc, 744, 176, 40, 48, NULL, B_N, IC_NEXT, 18, 24, A_TASKPAGE, 1);

    if (s_taskTab == 0 && !s_task.procOk) {
        T(dc, 400, 350, L"Список процессов недоступен: нет toolhelp.dll", 18, kMuted, 600, AL_MID, 760);
        return;
    }
    for (int k = 0; k < kTaskRows; k++) {
        int i = s_taskPage * kTaskRows + k, y = 232 + k * 48;
        if (s_taskTab == 0) {
            if (i >= s_task.procCount)
                break;
            const TaskProc& p = s_task.proc[i];
            bool self = p.pid == GetCurrentProcessId(), sys = TaskIsSystem(p.name);
            R(dc, 16, y, 768, 44, 10, kPanel);
            T(dc, 32, y + 29, p.name, 18, self ? kOrange : kText, 700, AL_START, 250);
            _snwprintf(s, 96, L"%d пот.", p.threads);
            T(dc, 296, y + 28, s, 15, kMuted, 400);
            if (p.cpu10 >= 0)
                _snwprintf(s, 96, L"%d,%d %%", p.cpu10 / 10, p.cpu10 % 10);
            else
                wcscpy(s, L"...");
            T(dc, 470, y + 29, s, 17, p.cpu10 >= 200 ? kOrange : kText, 700, AL_END);
            if (p.memKB)
                MemText(s, 96, p.memKB);
            else
                wcscpy(s, L"-");
            T(dc, 604, y + 29, s, 17, kText, 700, AL_END);
            if (sys || self)
                T(dc, 700, y + 28, sys ? L"система" : L"эта программа", 14, kDim, 600, AL_MID);
            else
                Button(dc, 624, y + 4, 152, 36, s_taskArm == p.pid ? L"ТОЧНО?" : L"ЗАВЕРШИТЬ",
                       s_taskArm == p.pid ? B_PRI : B_N, IC_NONE, 15, 0, A_TASKKILL, (int)p.pid);
        } else {
            if (i >= s_task.winCount)
                break;
            const TaskWin& w = s_task.win[i];
            R(dc, 16, y, 768, 44, 10, kPanel);
            T(dc, 32, y + 29, w.title, 18, kText, 700, AL_START, 300);
            T(dc, 346, y + 28, w.proc, 15, kMuted, 400, AL_START, 190);
            Button(dc, 544, y + 4, 112, 36, L"ПЕРЕЙТИ", B_N, IC_NONE, 15, 0, A_TASKSWITCH, (int)(INT_PTR)w.h);
            Button(dc, 664, y + 4, 112, 36, L"ЗАКРЫТЬ", B_N, IC_NONE, 15, 0, A_TASKCLOSE, (int)(INT_PTR)w.h);
        }
    }
    if (s_taskTab == 1 && !s_task.winCount)
        T(dc, 400, 350, L"Других окон нет", 18, kMuted, 600, AL_MID);
}

static int s_homePage;
// The widgets' grid: 3 x 2 on a page, a single 248 x 158, a double two singles and the gap.
enum { kWgW = 248, kWgH = 158, kWgGap = 12, kWgW2 = kWgW * 2 + kWgGap, kWgPages = 6 };

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

// The player's track (or "nothing plays" with play = the last track / the list); the progress updates with the
// minute redraw, the player screen itself updates every second.
static void WMedia(HDC dc, int x, int y)
{
    if (s_netOn && s_netCur >= 0) {   // the internet radio: the station, the song or its state
        bool d = Widget(dc, x, y, kWgW2, kWgH, L"СЕЙЧАС ИГРАЕТ · ИНТЕРНЕТ-РАДИО", IC_RADIO, A_NAV, S_NETRADIO);
        TriFill(x + 16, y + 42, 100, 100, 12);
        Ico(dc, IC_RADIO, x + 40, y + 66, 52, Mix(kInk, kTriOr[1], 140));
        wchar_t s[160];
        EnterCriticalSection(&s_netCs);
        wcscpy(s, s_netTitle);
        LeaveCriticalSection(&s_netCs);
        if (!s[0] || s_netState != NR_PLAY)
            NetStateText(s, 160);
        T(dc, x + 136, y + 70, s_stations[s_netCur].name, 26, d ? kInk : kText, 700, AL_START, 352);
        TWrap(dc, x + 136, y + 94, s, 16, d ? kInk : kMuted, 400, 352, 1, 0);
        IconBtn(dc, IC_PREV, x + 196, y + 122, 28, d ? kInk : kText, A_NRSTEP, -1);
        NetPlayBtn(dc, x + 312, y + 136, 20, 22);
        IconBtn(dc, IC_FWD, x + 400, y + 122, 28, d ? kInk : kText, A_NRSTEP, 1);
        return;
    }
    bool d = Widget(dc, x, y, kWgW2, kWgH, L"СЕЙЧАС ИГРАЕТ · МУЗЫКА", IC_MUSIC, A_NAV, S_AUDIO);
    COLORREF fg = d ? kInk : kText, mg = d ? kInk : kMuted;
    TriFill(x + 16, y + 42, 100, 100, 12);
    Ico(dc, IC_MUSIC, x + 40, y + 66, 52, Mix(kInk, kTriOr[1], 140));
    int tx = x + 136;
    wchar_t title[96], sub[200], s[48], a[16], b[16];
    double pos = 0, len = 0;
    if (PlayerNow(title, sub, 96, &pos, &len)) {
        T(dc, tx, y + 70, title, 26, fg, 700, AL_START, 352);
        _snwprintf(s, 48, L"трек %d из %d", s_plCur + 1, s_plCount);
        TWrap(dc, tx, y + 94, sub[0] ? sub : s, 16, mg, 400, 260, 1, 0);
        Mmss(a, 16, pos);
        Mmss(b, 16, len);
        _snwprintf(s, 48, L"%s / %s", a, b);
        T(dc, x + kWgW2 - 16, y + 94, s, 15, mg, 600, AL_END);
        int k = len > 0 ? (int)(pos * 352 / len) : 0;
        R(dc, tx, y + 106, 352, 6, 3, d ? kInk : kLine);
        R(dc, tx, y + 106, k > 6 ? k : 6, 6, 3, d ? kInk : kOrange);
    } else {   // nothing loaded: the last track (dim) to go on with, or a dash
        wchar_t last[MAX_PATH];
        const wchar_t* fn = wcsrchr(s_cfg.musicFile, L'\\');
        wcscpy(last, fn ? fn + 1 : s_cfg.musicFile);
        wchar_t* dot = wcsrchr(last, L'.');
        if (dot)
            *dot = 0;
        T(dc, tx, y + 70, last[0] ? last : L"—", 26, last[0] ? mg : fg, 700, AL_START, 352);
        TWrap(dc, tx, y + 94, last[0] ? L"последний трек · ▶ — продолжить" : L"выберите музыку в «Медиа»", 16, mg,
              400, 352, 1, 0);
        R(dc, tx, y + 106, 352, 6, 3, d ? kInk : kLine);
    }
    IconBtn(dc, IC_PREV, tx + 60, y + 122, 28, fg, A_PREVTRK);
    PlayBtn(dc, tx + 176, y + 136, 20, 22);
    IconBtn(dc, IC_FWD, tx + 264, y + 122, 28, fg, A_NEXTTRK);
}

static const wchar_t* kHeadings[8] = { L"С", L"СВ", L"В", L"ЮВ", L"Ю", L"ЮЗ", L"З", L"СЗ" };

// Speed: the car's own from the CAN module ("@,<km/h>") when it comes, otherwise GPS; a camera ahead shows as its
// speed sign ("КАМ" without a limit) with the distance to it.
static void WSpeed(HDC dc, int x, int y)
{
    Gps g;
    GpsGet(&g);
    bool fix = GpsFresh(g, 5000);
    bool car = g.carTick && GetTickCount() - g.carTick < 3000;
    Widget(dc, x, y, 248, 158, car ? L"СКОРОСТЬ · CAN" : L"СКОРОСТЬ · GPS", IC_NAV, A_NONE, 0, S_WSET_SPEED);
    wchar_t v[64];
    if (car)
        _snwprintf(v, 8, L"%d", g.carSpeed);
    else if (fix)
        _snwprintf(v, 8, L"%d", (int)(g.speedKmh + 0.5));
    else
        wcscpy(v, L"—");
    int end = T(dc, x + 16, y + 112, v, 72, fix || car ? kText : kDim, 700);
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
    TWrap(dc, x + 16, y + 144, v, 14, fix ? kMuted : kDim, 400, 216, 1, 0);
}

// The radio widget: the MCU's frequency and RDS name; the arrows step the presets of the page playing.
static void WRadio(HDC dc, int x, int y)
{
    Tuner t;
    TunerGet(&t);
    wchar_t f[16];
    FreqText(t.f10, f, 16);
    bool d = Widget(dc, x, y, 248, 158, t.band == 2 || (t.f10 > 0 && t.f10 < 3000) ? L"РАДИО · СВ"
                    : t.band == 1 || (t.f10 >= 6500 && t.f10 < 8750) ? L"РАДИО · УКВ" : L"РАДИО · FM", IC_RADIO, A_NAV,
                    S_RADIO);
    T(dc, x + 16, y + 88, f, 44, d ? kInk : t.f10 ? kText : kDim, 700);
    // RDS's name, else the one known for the frequency
    TWrap(dc, x + 16, y + 114, t.ps[0] ? t.ps : RadioName(t, t.f10), 17, d ? kInk : kMuted, 600, 216, 1, 0);
    if (t.presets > 0) {   // previous / next preset (none playing: from the first)
        int cur = t.cur >= 0 && t.cur < 24 ? t.cur : 0, page = cur / 8 * 8, per = t.presets < 8 ? t.presets : 8;
        IconBtn(dc, IC_PREV, x + 24, y + 124, 26, d ? kInk : kText, A_PRESET, page + (cur - page + per - 1) % per);
        IconBtn(dc, IC_FWD, x + 82, y + 124, 26, d ? kInk : kText, A_PRESET,
                t.cur >= 0 ? page + (cur - page + 1) % per : cur);
    }
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
        T(dc, x + 16, y + 84, s_wxBusy ? L"Загрузка…" : L"Нет данных", 22, fg, 700, AL_START, 216);
        TWrap(dc, x + 16, y + 108, w.ok < 0 ? w.err : L"нажмите, чтобы обновить", 14, mg, 400, 216, 3, 18);
        return;
    }
    WxIcon(dc, w.kind, x + 16, y + 44, fg);
    _snwprintf(s, 16, L"%+d°", w.temp);
    T(dc, x + 96, y + 96, s, 44, fg, 700);
    _snwprintf(s, 100, L"%s, ветер %d м/с", w.desc, w.wind);
    CharUpperBuff(s, 1);
    TWrap(dc, x + 16, y + 126, s, 15, mg, 400, 216, 1, 0);
    if (w.ok < 0)
        _snwprintf(s, 100, L"не обновилась: %s", w.err);
    else
        _snwprintf(s, 100, L"днём %+d°, ночью %+d° · %02d:%02d", w.tmax, w.tmin, w.at.wHour, w.at.wMinute);
    TWrap(dc, x + 16, y + 146, s, 14, d ? kInk : kDim, 400, 216, 1, 0);
}

// The screen saver's weather, centred on cx: the picture and the temperature (the car's sensor first), below them
// the sky's text.
static void SaverWeather(HDC dc, int cx, int y)
{
    Weather w;
    WxGet(&w);
    int temp;
    if (!OutsideTemp(&temp))
        return;
    wchar_t ts[16];
    _snwprintf(ts, 16, L"%+d°", temp);
    int tw = TextWidth(dc, ts, 40, 700), iw = w.have ? 72 : 0, x = cx - (iw + tw) / 2;
    if (w.have)
        WxIcon(dc, w.kind, x, y, kText);
    T(dc, x + iw, y + 48, ts, 40, kText, 700);
    if (w.have && w.desc[0])
        T(dc, cx, y + 88, w.desc, 18, kMuted, 400, AL_MID, 400);
}

static void WCar(HDC dc, int x, int y)
{
    static const wchar_t* rows[3][2] = { { L"Двигатель", L"90 °C" }, { L"Аккумулятор", L"14,2 В" },
                                         { L"ТО через", L"3 200 км" } };
    bool d = Widget(dc, x, y, 248, 158, L"АВТОМОБИЛЬ", IC_CAR, A_SCREEN, S_CAR, S_WSET_CAR);
    Disc(dc, x + 24, y + 58, 7, d ? kInk : kOk);
    T(dc, x + 40, y + 65, L"Ошибок нет", 19, d ? kInk : kText, 700);
    for (int i = 0; i < 3; i++) {
        T(dc, x + 16, y + 94 + i * 24, rows[i][0], 15, d ? kInk : kMuted, 400, AL_START, 140);
        T(dc, x + 232, y + 94 + i * 24, rows[i][1], 15, d ? kInk : kText, 700, AL_END);
    }
}

static void WTrip(HDC dc, int x, int y)
{
    static const wchar_t* cells[4][3] = { { L"РАСХОД", L"7,4", L"л/100 км" }, { L"ЗАПАС ХОДА", L"420", L"км" },
                                          { L"ПРОБЕГ", L"34,5", L"км" }, { L"СРЕДНЯЯ", L"49", L"км/ч" } };
    bool d = Widget(dc, x, y, kWgW2, kWgH, L"БОРТОВОЙ КОМПЬЮТЕР · ПОЕЗДКА", IC_CLOCK, A_SCREEN, S_CAR);
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
    TWrap(dc, x + 16, y + 144, s, 14, d ? kInk : kDim, 400, 216, 1, 0);
}

// Climate control, only when the car has it (the car widget's settings); demo until the MCU protocol is known.
// The climate: live from the MCU (through the proxy) when it has sent anything since start - the knob's
// position (0..16: the Vesta's climate reports a knob, not degrees), the blower 1..7, A/C, auto, the rear window
// heating and the outside temperature; otherwise the demo.
static void WClimate(HDC dc, int x, int y)
{
    Hvac h;
    if (HvacGet(&h)) {
        Widget(dc, x, y, 248, 158, L"КЛИМАТ", IC_CAR, A_SCREEN, S_TEST_MCU, S_WSET_CAR);
        wchar_t s[24];
        if (h.temp >= 0) {
            for (int i = 0; i < 16; i++) {   // the knob: blue to red, the set part lit
                COLORREF c = i < h.temp ? (i < 8 ? RGB(0x4a, 0x9e, 0xff) : kOrange) : kLine;
                R(dc, x + 16 + i * 9, y + 70, 7, 18, 2, c);
            }
            _snwprintf(s, 24, L"%d/16", h.temp);
            T(dc, x + 168, y + 86, s, 18, kText, 700);
        }
        if (h.blower >= 0) {
            for (int i = 0; i < 7; i++)
                R(dc, x + 16 + i * 13, y + 112 - (6 + i * 3), 9, 6 + i * 3, 2, i < h.blower ? kText : kLine);
            _snwprintf(s, 24, L"вент %d", h.blower);
            T(dc, x + 112, y + 112, s, 14, kMuted, 600);
        }
        R(dc, x + 16, y + 120, 56, 28, 8, h.ac == 1 ? kOrange : CLR_INVALID, h.ac == 1 ? CLR_INVALID : kLine);
        T(dc, x + 44, y + 140, L"A/C", 14, h.ac == 1 ? kInk : kMuted, 700, AL_MID);
        R(dc, x + 80, y + 120, 64, 28, 8, h.autoMode == 2 ? kOrange : CLR_INVALID, h.autoMode == 2 ? CLR_INVALID : kLine);
        T(dc, x + 112, y + 140, L"АВТО", 13, h.autoMode == 2 ? kInk : kMuted, 700, AL_MID);
        R(dc, x + 152, y + 120, 80, 28, 8, h.rear == 1 ? kOrange : CLR_INVALID, h.rear == 1 ? CLR_INVALID : kLine);
        T(dc, x + 192, y + 140, L"ОБОГРЕВ", 12, h.rear == 1 ? kInk : kMuted, 700, AL_MID);
        if (h.outer > -100) {
            _snwprintf(s, 24, L"за бортом %+d°", h.outer);
            T(dc, x + 232, y + 54, s, 13, kMuted, 600, AL_END);
        }
        return;
    }
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
    bool d = Widget(dc, x, y, 248, 158, L"ПРИЛОЖЕНИЯ", IC_APPS, A_NAV, S_APPS);
    if (s_appCount == 0) {
        TWrap(dc, x + 16, y + 92, L"Добавьте программы в «Приложениях»", 16, d ? kInk : kMuted, 400, 216, 2, 22);
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
// OSMapsCE among the programs: the first whose path has "OSMaps" (OSMapsCE.exe; OSMapsPC.exe on the PC); -1 none.
static int NaviApp()
{
    for (int i = 0; i < s_appCount; i++) {
        wchar_t lp[MAX_PATH];
        wcscpy(lp, s_apps[i].path);
        CharLowerBuff(lp, (DWORD)wcslen(lp));
        if (wcsstr(lp, L"osmaps"))
            return i;
    }
    return -1;
}

static void WNavi(HDC dc, int x, int y)
{
    int app = NaviApp();
    const wchar_t* name = app >= 0 ? s_apps[app].name : L"OSMapsCE не найден";
    bool d = Widget(dc, x, y, 248, 158, L"НАВИГАЦИЯ", IC_NAV, app >= 0 ? A_RUN : A_NAV, app >= 0 ? app : S_APPS);
    Ico(dc, IC_MAP, x + 16, y + 52, 48, d ? kInk : kText);
    T(dc, x + 78, y + 74, name, 19, d ? kInk : kText, 700, AL_START, 154);
    TWrap(dc, x + 78, y + 96, L"маршрут не задан", 15, d ? kInk : kMuted, 400, 154, 1, 0);
    TWrap(dc, x + 16, y + 144, app >= 0 ? L"нажмите, чтобы открыть карты" : L"добавьте его в «Приложениях»", 14,
          d ? kInk : kDim, 400, 216, 1, 0);
}

// ---- the user's widgets: VestaWidgets\*.ini on the SD card (the PC: next to the exe), one a widget:
// (and the HTTPS that VestaHome lends them: url= every= save=, value.from= ... - see UserWgFetch)
//     title=FUEL  icon=car  wide=0  data=fuel.txt  refresh=10  start=fuel.mscr  tap=fuel.mscr
// data (default <name>.txt beside the ini) is written by a script (MortScript); VestaHome reads it every refresh
// seconds when its time changed:  value=7,4  unit=л/100 км  text=...  text2=...  state=ok|warn|alert
// start= runs once when VestaHome starts (a script that keeps the data fresh), tap= on a tap; .mscr through
// MortScript.exe, .exe as it is; a relative path is from the ini's folder. UTF-8 or cp1251 (MortScript writes
// ANSI). A broken file can't harm VestaHome: unknown lines are skipped, no file - "no data".
static const struct { const wchar_t* name; Icon ic; } kIconNames[] = {
    { L"music", IC_MUSIC }, { L"radio", IC_RADIO }, { L"phone", IC_PHONE }, { L"gear", IC_GEAR },
    { L"home", IC_HOME }, { L"car", IC_CAR }, { L"cloud", IC_CLOUD }, { L"nav", IC_NAV }, { L"map", IC_MAP },
    { L"sd", IC_SD }, { L"usb", IC_USB }, { L"wifi", IC_WIFI }, { L"bt", IC_BT }, { L"info", IC_INFO },
    { L"clock", IC_CLOCK }, { L"photo", IC_PHOTO }, { L"video", IC_VIDEO }, { L"apps", IC_APPS },
    { L"warn", IC_WARN }, { L"eye", IC_EYE }, { L"power", IC_POWER }, { L"folder", IC_FOLDER },
    { L"tasks", IC_TASKS }, { L"star", IC_STAR }, { L"lock", IC_LOCK }, { L"file", IC_FILE } };

static void UserWgDir(wchar_t* dir)
{
#ifdef UNDER_CE
    wcscpy(dir, L"\\SDMemory\\VestaWidgets\\");
#else
    _snwprintf(dir, MAX_PATH, L"%sVestaWidgets\\", s_dir);
#endif
}

static bool IsUtf8(const unsigned char* p, int n)
{
    for (int i = 0; i < n;) {
        int k = p[i] < 0x80 ? 0 : (p[i] & 0xE0) == 0xC0 ? 1 : (p[i] & 0xF0) == 0xE0 ? 2 : (p[i] & 0xF8) == 0xF0 ? 3 : -1;
        if (k < 0 || i + k >= n + (k ? 0 : 1))
            return false;
        for (int j = 1; j <= k; j++)
            if ((p[i + j] & 0xC0) != 0x80)
                return false;
        i += k + 1;
    }
    return true;
}

// A key=value text file (UTF-8 with or without BOM, else cp1251) into wide text; NULL when missing. free() it.
static wchar_t* ReadText(const wchar_t* path)
{
    DWORD size;
    char* buf = ReadAll(path, &size);
    if (!buf)
        return NULL;
    const unsigned char* b = (const unsigned char*)buf;
    int n = (int)size, skip = 0;
    if (n >= 3 && b[0] == 0xEF && b[1] == 0xBB && b[2] == 0xBF)
        skip = 3;
    UINT cp = IsUtf8(b + skip, n - skip) ? CP_UTF8 : 1251;
    wchar_t* w = (wchar_t*)malloc((n + 1) * sizeof(wchar_t));
    if (w) {
        int k = MultiByteToWideChar(cp, 0, buf + skip, n - skip, w, n);
        if (k <= 0 && cp == 1251)
            k = MultiByteToWideChar(CP_ACP, 0, buf + skip, n - skip, w, n);
        w[k > 0 ? k : 0] = 0;
    }
    free(buf);
    return w;
}

// The next "key=value" line of text (comments '#' / ';' and blank lines skipped); false at the end.
static bool NextKeyVal(wchar_t** pos, wchar_t** k, wchar_t** v)
{
    while (**pos) {
        wchar_t* line = *pos;
        wchar_t* end = line;
        while (*end && *end != L'\n' && *end != L'\r')
            end++;
        *pos = *end ? end + 1 : end;
        *end = 0;
        Trim(line);
        if (!line[0] || line[0] == L'#' || line[0] == L';')
            continue;
        wchar_t* eq = wcschr(line, L'=');
        if (!eq)
            continue;
        *eq = 0;
        *k = line;
        *v = eq + 1;
        Trim(*k);
        Trim(*v);
        return true;
    }
    return false;
}

// A path of the widget's ini: as it is when absolute, else from the ini's folder.
static void UserWgPath(const UserWg& w, const wchar_t* v, wchar_t* out)
{
    if (v[0] == L'\\' || (v[0] && v[1] == L':'))
        wcsncpy(out, v, MAX_PATH - 1);
    else
        _snwprintf(out, MAX_PATH, L"%s%s", w.dir, v);
    out[MAX_PATH - 1] = 0;
}

static bool UserWgLoad(const wchar_t* dir, const wchar_t* file, UserWg* w)
{
    wchar_t path[MAX_PATH];
    _snwprintf(path, MAX_PATH, L"%s%s", dir, file);
    wchar_t* text = ReadText(path);
    if (!text)
        return false;
    memset(w, 0, sizeof(*w));
    wcsncpy(w->dir, dir, MAX_PATH - 1);
    wchar_t name[40];
    wcsncpy(name, file, 39);
    name[39] = 0;
    wchar_t* dot = wcsrchr(name, L'.');
    if (dot)
        *dot = 0;
    _snwprintf(w->key, 40, L"u.%s", name);
    for (wchar_t* c = w->key; *c; c++)   // the key goes into a comma list
        if (*c == L',' || *c == L' ')
            *c = L'_';
    wcsncpy(w->title, name, 39);
    w->ic = IC_INFO;
    w->refresh = 10;
    wchar_t dataName[MAX_PATH];
    _snwprintf(dataName, MAX_PATH, L"%s.txt", name);
    UserWgPath(*w, dataName, w->data);
    wchar_t* pos = text;
    wchar_t *k, *v;
    while (NextKeyVal(&pos, &k, &v)) {
        if (!_wcsicmp(k, L"title")) {
            wcsncpy(w->title, v, 39);
            w->title[39] = 0;
        } else if (!_wcsicmp(k, L"icon")) {
            for (int i = 0; i < (int)(sizeof(kIconNames) / sizeof(kIconNames[0])); i++)
                if (!_wcsicmp(v, kIconNames[i].name))
                    w->ic = kIconNames[i].ic;
        } else if (!_wcsicmp(k, L"wide")) {
            w->wide = CfgBool(v);
        } else if (!_wcsicmp(k, L"data")) {
            UserWgPath(*w, v, w->data);
        } else if (!_wcsicmp(k, L"refresh")) {
            w->refresh = _wtoi(v) < 1 ? 1 : _wtoi(v) > 3600 ? 3600 : _wtoi(v);
        } else if (!_wcsicmp(k, L"tap")) {
            if (v[0])
                UserWgPath(*w, v, w->tap);
        } else if (!_wcsicmp(k, L"start")) {
            if (v[0])
                UserWgPath(*w, v, w->start);
        } else if (!_wcsicmp(k, L"url")) {
            WideCharToMultiByte(CP_UTF8, 0, v, -1, w->url, sizeof(w->url), NULL, NULL);
        } else if (!_wcsicmp(k, L"every")) {
            w->every = _wtoi(v) < 60 ? 60 : _wtoi(v);   // not more often than once a minute
        } else if (!_wcsicmp(k, L"save")) {
            if (v[0])
                UserWgPath(*w, v, w->resp);
        } else {
            static const wchar_t* fields[5] = { L"value", L"unit", L"text", L"text2", L"state" };
            for (int i = 0; i < 5; i++) {
                size_t n = wcslen(fields[i]);
                if (!_wcsnicmp(k, fields[i], n) && !_wcsicmp(k + n, L".from")) {
                    wcsncpy(w->from[i], v, 63);
                    w->from[i][63] = 0;
                } else if (!_wcsicmp(k, fields[i])) {
                    wcsncpy(w->fixed[i], v, 95);
                    w->fixed[i][95] = 0;
                }
            }
        }
    }
    if (w->url[0]) {
        if (!w->every)
            w->every = 600;
        if (!w->resp[0]) {
            wchar_t respName[MAX_PATH];
            _snwprintf(respName, MAX_PATH, L"%s.resp", name);
            UserWgPath(*w, respName, w->resp);
        }
    }
    CharUpperBuff(w->title, (DWORD)wcslen(w->title));
    free(text);
    return true;
}

// The data file, when its write time changed; true: something to draw anew.
static bool UserWgRead(UserWg& w)
{
    WIN32_FIND_DATA fd;
    HANDLE f = FindFirstFile(w.data, &fd);
    if (f == INVALID_HANDLE_VALUE) {
        bool was = w.have;
        w.have = false;
        return was;
    }
    FindClose(f);
    if (w.have && !CompareFileTime(&fd.ftLastWriteTime, &w.dataTime))
        return false;
    wchar_t* text = ReadText(w.data);
    if (!text)
        return false;
    w.dataTime = fd.ftLastWriteTime;
    w.have = true;
    w.value[0] = w.unit[0] = w.text[0][0] = w.text[1][0] = 0;
    w.state = 0;
    wchar_t* pos = text;
    wchar_t *k, *v;
    while (NextKeyVal(&pos, &k, &v)) {
        if (!_wcsicmp(k, L"value"))
            wcsncpy(w.value, v, 23);
        else if (!_wcsicmp(k, L"unit"))
            wcsncpy(w.unit, v, 23);
        else if (!_wcsicmp(k, L"text"))
            wcsncpy(w.text[0], v, 95);
        else if (!_wcsicmp(k, L"text2"))
            wcsncpy(w.text[1], v, 95);
        else if (!_wcsicmp(k, L"state"))
            w.state = !_wcsicmp(v, L"ok") ? 1 : !_wcsicmp(v, L"warn") ? 2 : !_wcsicmp(v, L"alert") ? 3 : 0;
    }
    w.value[23] = w.unit[23] = w.text[0][95] = w.text[1][95] = 0;
    free(text);
    return true;
}

// Runs a widget's command: a .mscr through MortScript.exe (looked for beside the widget, on the card, next to
// VestaHome, in the usual folders), anything else as a program.
static void UserWgRun(const UserWg& w, const wchar_t* cmd, const wchar_t* why)
{
    wchar_t exe[MAX_PATH], args[MAX_PATH + 4];
    const wchar_t* dot = wcsrchr(cmd, L'.');
    if (dot && !_wcsicmp(dot, L".mscr")) {
        wchar_t cand[7][MAX_PATH];
        _snwprintf(cand[0], MAX_PATH, L"%sMortScript.exe", w.dir);
        wcscpy(cand[1], L"\\SDMemory\\MortScript\\MortScript.exe");
        wcscpy(cand[2], L"\\SDMemory\\MortScript.exe");
        _snwprintf(cand[3], MAX_PATH, L"%sMortScript.exe", s_dir);
        wcscpy(cand[4], L"\\SDMMC\\MortScript\\MortScript.exe");
        wcscpy(cand[5], L"\\StorageFlash\\MortScript\\MortScript.exe");
        wcscpy(cand[6], L"\\ResidentFlash\\MortScript\\MortScript.exe");
        exe[0] = 0;
        for (int i = 0; i < 7 && !exe[0]; i++)
            if (GetFileAttributes(cand[i]) != 0xFFFFFFFF)
                wcscpy(exe, cand[i]);
        if (!exe[0]) {
            VLog(L"виджет %s: %s - нет MortScript.exe (рядом с виджетом, \\SDMemory\\MortScript\\)", w.key, why);
            return;
        }
        _snwprintf(args, MAX_PATH + 4, L"\"%s\"", cmd);
    } else {
        wcsncpy(exe, cmd, MAX_PATH - 1);
        exe[MAX_PATH - 1] = 0;
        args[0] = 0;
        // A program: as from "Приложения" (VestaHome waits for its window and brings it up, otherwise VestaHome
        // stayed over it - HU 2026-10-20). Its path wrong (an example's ini): the one of that file name there.
        const wchar_t* fn = wcsrchr(exe, L'\\');
        fn = fn ? fn + 1 : exe;
        bool there = GetFileAttributes(exe) != 0xFFFFFFFF;
        int app = -1;
        for (int i = 0; i < s_appCount && app < 0; i++) {
            const wchar_t* an = wcsrchr(s_apps[i].path, L'\\');
            an = an ? an + 1 : s_apps[i].path;
            if (!_wcsicmp(s_apps[i].path, exe) || (!there && !_wcsicmp(an, fn)))
                app = i;
        }
        if (app >= 0) {
            VLog(L"виджет %s: %s - %s (из «Приложений»)", w.key, why, s_apps[app].path);
            RunApp(app);
            return;
        }
        if (!there) {
            VLog(L"виджет %s: %s - нет %s (и в «Приложениях» нет %s)", w.key, why, exe, fn);
            _snwprintf(s_status, 96, L"нет %s: добавьте в «Приложения»", fn);
            return;
        }
    }
    PROCESS_INFORMATION pi;
#ifdef UNDER_CE
    BOOL ok = CreateProcess(exe, args[0] ? args : NULL, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi);
#else
    STARTUPINFO si;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    wchar_t line[2 * MAX_PATH + 8];
    _snwprintf(line, 2 * MAX_PATH + 8, L"\"%s\" %s", exe, args);
    BOOL ok = CreateProcess(exe, line, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
#endif
    VLog(L"виджет %s: %s - %s %s", w.key, why, cmd, ok ? L"запущен" : L"не запустился");
    if (ok) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        if (!args[0]) {   // a program (not a script): its window brought up when it comes
            s_waitApp = -2;
            s_waitPid = pi.dwProcessId;
            s_waitTick = GetTickCount();
            SetTimer(s_wnd, TIMER_WAIT, 300, NULL);
        }
    }
}

// ---- HTTPS for the widgets: a widget with url= is fetched by VestaHome every every= seconds (one at a time, in a
// thread of its own; at most 256 KB, 60 s at least between two of the same widget). The answer goes to resp
// (for a script), and when the widget asks for fields (value.from= "start|end" ...) VestaHome takes them out and
// writes the data file itself - no script needed. A failure keeps the last data and goes to the log.
#define WM_APP_UWG (WM_APP + 26)
enum { kUwgMaxBody = 256 * 1024 };
static CRITICAL_SECTION s_uwCs;   // s_uw between the scan (the window) and the fetch thread
static HANDLE s_uwThread;

static void WriteUtf8(const wchar_t* path, const wchar_t* text)
{
    int n = WideCharToMultiByte(CP_UTF8, 0, text, -1, NULL, 0, NULL, NULL);
    char* b = (char*)malloc(n > 0 ? n : 1);
    if (!b)
        return;
    WideCharToMultiByte(CP_UTF8, 0, text, -1, b, n, NULL, NULL);
    HANDLE f = CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (f != INVALID_HANDLE_VALUE) {
        DWORD w;
        WriteFile(f, b, n > 0 ? n - 1 : 0, &w, NULL);
        CloseHandle(f);
    }
    free(b);
}

// The text between the markers "start|end" of rule in body (the first match), entities &amp; &lt; &gt; &quot;
// &apos; decoded, trimmed; false: not found.
static bool TakeBetween(const wchar_t* body, const wchar_t* rule, wchar_t* out, int cap)
{
    const wchar_t* bar = wcschr(rule, L'|');
    if (!bar)
        return false;
    wchar_t a[64], b[64];
    int la = (int)(bar - rule);
    wcsncpy(a, rule, la);
    a[la] = 0;
    wcsncpy(b, bar + 1, 63);
    b[63] = 0;
    const wchar_t* p = a[0] ? wcsstr(body, a) : body;
    if (!p)
        return false;
    p += la;
    const wchar_t* e = b[0] ? wcsstr(p, b) : p + wcslen(p);
    if (!e)
        return false;
    int n = 0;
    for (const wchar_t* q = p; q < e && n < cap - 1; q++) {
        static const struct { const wchar_t* ent; wchar_t c; } ents[5] = {
            { L"&amp;", L'&' }, { L"&lt;", L'<' }, { L"&gt;", L'>' }, { L"&quot;", L'"' }, { L"&apos;", L'\'' } };
        bool done = false;
        for (int i = 0; i < 5 && !done; i++)
            if (!wcsncmp(q, ents[i].ent, wcslen(ents[i].ent))) {
                out[n++] = ents[i].c;
                q += wcslen(ents[i].ent) - 1;
                done = true;
            }
        if (!done)
            out[n++] = *q == L'\r' || *q == L'\n' || *q == L'\t' ? L' ' : *q;
    }
    out[n] = 0;
    Trim(out);
    return true;
}

struct UwgJob { wchar_t key[40], resp[MAX_PATH], data[MAX_PATH], from[5][64], fixed[5][96]; char url[512]; };

static void UserWgFetchOne(const UwgJob& j)
{
    const char* u = j.url;
    bool tls = !_strnicmp(u, "https://", 8);
    if (!tls && _strnicmp(u, "http://", 7)) {
        VLog(L"виджет %s: url не http:// и не https://", j.key);
        return;
    }
    u += tls ? 8 : 7;
    char host[128];
    const char* slash = strchr(u, '/');
    int hl = slash ? (int)(slash - u) : (int)strlen(u);
    if (hl <= 0 || hl >= (int)sizeof(host))
        return;
    memcpy(host, u, hl);
    host[hl] = 0;
    const char* path = slash ? slash : "/";
    DWORD t0 = GetTickCount();
    unsigned char* body = NULL;
    int len = 0, status = 0;
    int r = HttpGet(host, tls, path, "User-Agent: Mozilla/5.0 (VestaHome)\r\n", &body, &len, &status);
    if (r != NET_OK || status != 200 || !body) {
        VLog(L"виджет %s: загрузка %S - %s", j.key, host, r != NET_OK ? NetErrorText(r) : L"ответ не 200");
        if (r == NET_OK)
            VLog(L"виджет %s: HTTP %d", j.key, status);
        free(body);
        return;
    }
    if (len > kUwgMaxBody) {
        VLog(L"виджет %s: ответ %d байт - больше 256 КБ, отброшен", j.key, len);
        free(body);
        return;
    }
    HANDLE f = CreateFile(j.resp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);   // the raw answer for a script
    if (f != INVALID_HANDLE_VALUE) {
        DWORD w;
        WriteFile(f, body, len, &w, NULL);
        CloseHandle(f);
    }
    bool rules = false;
    for (int i = 0; i < 5; i++)
        rules = rules || j.from[i][0];
    if (rules) {   // the fields taken out: the data file
        UINT cp = IsUtf8(body, len) ? CP_UTF8 : 1251;
        wchar_t* w = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
        if (w) {
            int k = MultiByteToWideChar(cp, 0, (const char*)body, len, w, len);
            w[k > 0 ? k : 0] = 0;
            static const wchar_t* keys[5] = { L"value", L"unit", L"text", L"text2", L"state" };
            wchar_t data[1024] = L"# written by VestaHome from the widget's url\r\n", got[96], line[256];
            for (int i = 0; i < 5; i++) {
                bool have = j.from[i][0] && TakeBetween(w, j.from[i], got, 96);
                if (i == 4 && have)   // Yandex's colours
                    wcscpy(got, !_wcsicmp(got, L"green") ? L"ok" : !_wcsicmp(got, L"yellow") ? L"warn"
                                : !_wcsicmp(got, L"red") ? L"alert" : got);
                if (j.fixed[i][0] && wcsstr(j.fixed[i], L"%s"))
                    _snwprintf(line, 256, j.fixed[i], have ? got : L"—");
                else if (have)
                    wcscpy(line, got);
                else
                    wcsncpy(line, j.fixed[i], 255);
                line[255] = 0;
                if (line[0] && wcslen(data) + wcslen(keys[i]) + wcslen(line) + 4 < 1024) {
                    wcscat(data, keys[i]);
                    wcscat(data, L"=");
                    wcscat(data, line);
                    wcscat(data, L"\r\n");
                }
            }
            WriteUtf8(j.data, data);
            free(w);
        }
    }
    VLog(L"виджет %s: загрузка %S - %d байт за %u мс", j.key, host, len, (unsigned)(GetTickCount() - t0));
    free(body);
}

static DWORD WINAPI UserWgThread(LPVOID)
{
    for (;;) {
        Sleep(1000);
        UwgJob j;
        bool due = false;
        EnterCriticalSection(&s_uwCs);
        for (int i = 0; i < s_uwCount && !due; i++) {
            UserWg& w = s_uw[i];
            if (!w.url[0] || (w.fetchTick && GetTickCount() - w.fetchTick < (DWORD)w.every * 1000))
                continue;
            w.fetchTick = GetTickCount();
            if (!w.fetchTick)
                w.fetchTick = 1;
            wcscpy(j.key, w.key);
            wcscpy(j.resp, w.resp);
            wcscpy(j.data, w.data);
            memcpy(j.from, w.from, sizeof(j.from));
            memcpy(j.fixed, w.fixed, sizeof(j.fixed));
            strcpy(j.url, w.url);
            due = true;
        }
        LeaveCriticalSection(&s_uwCs);
        if (due) {
            UserWgFetchOne(j);
            PostMessage(s_wnd, WM_APP_UWG, 0, 0);
        }
    }
    return 0;
}

static int CmpUserWg(const void* a, const void* b) { return wcscmp(((const UserWg*)a)->key, ((const UserWg*)b)->key); }

// The widgets' folder read (at the start and when a card comes or goes); a widget seen the first time since
// VestaHome started runs its start= script. The main screen's order is applied again with them.
static void UserWidgetsScan()
{
    static wchar_t s_started[32][40];
    static int s_startedCount;
    wchar_t dir[MAX_PATH], pat[MAX_PATH];
    UserWgDir(dir);
    _snwprintf(pat, MAX_PATH, L"%s*.ini", dir);
    if (!s_uwThread)
        InitializeCriticalSection(&s_uwCs);
    EnterCriticalSection(&s_uwCs);
    s_uwCount = 0;
    WIN32_FIND_DATA fd;
    HANDLE f = FindFirstFile(pat, &fd);
    if (f != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && UserWgLoad(dir, fd.cFileName, &s_uw[s_uwCount]))
                s_uwCount++;
        } while (s_uwCount < kWgUser && FindNextFile(f, &fd));
        FindClose(f);
    }
    qsort(s_uw, s_uwCount, sizeof(UserWg), CmpUserWg);
    bool net = false;
    for (int i = 0; i < s_uwCount; i++)
        net = net || s_uw[i].url[0];
    LeaveCriticalSection(&s_uwCs);
    if (net && !s_uwThread)
        s_uwThread = CreateThread(NULL, 0, UserWgThread, NULL, 0, NULL);
    for (int i = 0; i < s_uwCount; i++) {
        UserWg& w = s_uw[i];
        UserWgRead(w);
        bool started = false;
        for (int k = 0; k < s_startedCount && !started; k++)
            started = !wcscmp(s_started[k], w.key);
        if (!started && w.start[0]) {
            UserWgRun(w, w.start, L"start");
            if (s_startedCount < 32)
                wcscpy(s_started[s_startedCount++], w.key);
        }
    }
    if (s_uwCount)
        VLog(L"виджеты пользователя: %d (%s)", s_uwCount, dir);
    HomeWidgetsLoad(s_cfg.homeRaw);
}

static void WUser(HDC dc, int x, int y, int u)
{
    const UserWg& w = s_uw[u];
    int ww = w.wide ? kWgW2 : kWgW;
    bool d = Widget(dc, x, y, ww, kWgH, w.title, w.ic, w.tap[0] ? A_USERWG : A_NONE, u);
    COLORREF fg = d ? kInk : kText, mg = d ? kInk : kMuted;
    if (w.state) {
        static const COLORREF dot[4] = { 0, kOk, kOrange, kStop };
        Disc(dc, x + ww - 24, y + 21, 7, d ? kInk : dot[w.state]);
    }
    if (!w.have) {
        T(dc, x + 16, y + 84, L"Нет данных", 22, fg, 700, AL_START, ww - 32);
        const wchar_t* fn = wcsrchr(w.data, L'\\');
        wchar_t s[MAX_PATH + 32];
        _snwprintf(s, MAX_PATH + 32, L"скрипт ещё не записал %s", fn ? fn + 1 : w.data);
        TWrap(dc, x + 16, y + 108, s, 14, mg, 400, ww - 32, 3, 18);
        return;
    }
    if (w.value[0]) {
        int end = T(dc, x + 16, y + 96, w.value, 44, fg, 700, AL_START, ww - 32);
        if (w.unit[0] && end + 8 < x + ww - 16)
            T(dc, end + 8, y + 96, w.unit, 18, mg, 600, AL_START, x + ww - 16 - end - 8);
        TWrap(dc, x + 16, y + 124, w.text[0], 15, mg, 400, ww - 32, 1, 0);
        TWrap(dc, x + 16, y + 146, w.text[1], 14, d ? kInk : kDim, 400, ww - 32, 1, 0);
    } else {
        int n = TWrap(dc, x + 16, y + 80, w.text[0], 19, fg, 700, ww - 32, 2, 24);
        TWrap(dc, x + 16, y + 80 + n * 24 + 8, w.text[1], 15, mg, 400, ww - 32, 2, 19);
    }
}

struct WidgetDef { const wchar_t* name; bool wide; Icon ic; void (*draw)(HDC dc, int x, int y); };
static const WidgetDef kWidgets[WG_COUNT] = {
    { L"Сейчас играет", true, IC_MUSIC, WMedia }, { L"Скорость", false, IC_NAV, WSpeed },
    { L"Радио", false, IC_RADIO, WRadio }, { L"Погода", false, IC_CLOUD, WWeather },
    { L"Приложения", false, IC_APPS, WFavorites }, { L"Поездка", true, IC_CLOCK, WTrip },
    { L"Климат", false, IC_CAR, WClimate }, { L"Календарь", false, IC_CLOCK, WDate },
    { L"Телефон", false, IC_PHONE, WPhone }, { L"Автомобиль", false, IC_CAR, WCar },
    { L"Навигация", false, IC_NAV, WNavi } };

// A widget by its id (built-in or the user's): the only way to it for the layout, the settings and the screen.
static const wchar_t* WgName(int id) { return id < WG_COUNT ? kWidgets[id].name : s_uw[id - WG_COUNT].title; }
static bool WgWide(int id) { return id < WG_COUNT ? kWidgets[id].wide : s_uw[id - WG_COUNT].wide; }
static Icon WgIcon(int id) { return id < WG_COUNT ? kWidgets[id].ic : s_uw[id - WG_COUNT].ic; }
static void WgDraw(int id, HDC dc, int x, int y)
{
    if (id < WG_COUNT)
        kWidgets[id].draw(dc, x, y);
    else
        WUser(dc, x, y, id - WG_COUNT);
}

// Shown: not hidden, and the climate only when the car has it (Settings -> Car).
static bool WgShown(int id)
{
    return !(s_cfg.homeHidden >> id & 1) && (id != WG_CLIMATE || s_cfg.climate);
}

// Where each shown widget goes: in their order, each into the first free place (a double needs two free cells
// side by side in a row), so the later ones close up the gaps. page < 0: not placed. Returns the pages.
struct WgPlace { int page, x, y; };
static int HomeLayout(WgPlace* at)
{
    bool used[kWgPages][6];
    memset(used, 0, sizeof(used));
    int pages = 1;
    for (int i = 0; i < WgCount(); i++) {
        int id = s_cfg.homeOrder[i];
        at[id].page = -1;
        if (!WgShown(id))
            continue;
        bool wide = WgWide(id);
        for (int k = 0; k < kWgPages * 6 && at[id].page < 0; k++) {
            int pg = k / 6, c = k % 6;
            if (used[pg][c] || (wide && (c % 3 == 2 || used[pg][c + 1])))
                continue;
            used[pg][c] = true;
            if (wide)
                used[pg][c + 1] = true;
            at[id].page = pg;
            at[id].x = 16 + c % 3 * (kWgW + kWgGap);
            at[id].y = 64 + c / 3 * (kWgH + kWgGap);
            if (pg + 1 > pages)
                pages = pg + 1;
        }
    }
    return pages;
}

static int HomePages()
{
    WgPlace at[kWgMax];
    return HomeLayout(at);
}

// A widget's rectangle when it is on the page shown (for its own redraws).
static bool HomeRect(int id, RECT* rc)
{
    WgPlace at[kWgMax];
    HomeLayout(at);
    if (at[id].page != s_homePage)
        return false;
    rc->left = at[id].x;
    rc->top = at[id].y;
    rc->right = at[id].x + (WgWide(id) ? kWgW2 : kWgW);
    rc->bottom = at[id].y + kWgH;
    return true;
}

// The main screen: the date as the title, the widgets of the page, the page dots.
static void DrawHome(HDC dc)
{
    SYSTEMTIME t;
    GetLocalTime(&t);
    wchar_t s[48];
    _snwprintf(s, 48, L"%s, %d %s", kDays[t.wDayOfWeek % 7], t.wDay, kMonths[(t.wMonth + 11) % 12]);
    CharUpperBuff(s, 1);
    Frame(dc, s);
    WgPlace at[kWgMax];
    int pages = HomeLayout(at);
    if (s_homePage >= pages)
        s_homePage = pages - 1;
    for (int id = 0; id < WgCount(); id++)
        if (at[id].page == s_homePage)
            WgDraw(id, dc, at[id].x, at[id].y);
    bool any = false;
    for (int id = 0; id < WgCount(); id++)
        any = any || at[id].page >= 0;
    if (!any)
        T(dc, 400, 230, L"Все виджеты скрыты: «Настройки → Общие → Виджеты главной»", 16, kMuted, 400, AL_MID, 740);
    PageDots(dc, s_homePage, pages);
}

// Settings -> General -> the main screen's widgets: in their order (down the left column, then the right, 12 a
// page), each with "show" (the eye) and up / down (earlier / later); the user's widgets with their file.
static int s_setHomePage;

static void ScrSetHome(HDC dc)
{
    int pages = (WgCount() + 11) / 12;
    s_setHomePage = s_setHomePage >= pages ? pages - 1 : s_setHomePage;
    Frame(dc, L"ГЛАВНАЯ · ВИДЖЕТЫ");
    Panel(dc, 16, 64, 768, 336);
    for (int i = s_setHomePage * 12; i < WgCount() && i < s_setHomePage * 12 + 12; i++) {
        int id = s_cfg.homeOrder[i], j = i % 12, x = 24 + j / 6 * 380, y = 70 + j % 6 * 54;
        bool shown = !(s_cfg.homeHidden >> id & 1);
        if (j % 6)
            Sep(dc, x, y - 2, 368);
        Ico(dc, WgIcon(id), x + 8, y + 12, 26, shown ? kOrange : kDim);
        const wchar_t* sub = WgWide(id) ? L"двойной" : NULL;
        wchar_t usub[48];
        if (id >= WG_COUNT) {
            _snwprintf(usub, 48, L"свой · %s.ini%s", s_uw[id - WG_COUNT].key + 2, WgWide(id) ? L" · двойной" : L"");
            sub = usub;
        }
        if (id == WG_CLIMATE && !s_cfg.climate)
            sub = L"нет в машине (Настройки → Автомобиль)";
        T(dc, x + 46, y + (sub ? 24 : 33), WgName(id), 18, shown ? kText : kDim, 700, AL_START, 172);
        if (sub)
            T(dc, x + 46, y + 43, sub, 13, kMuted, 400, AL_START, 172);
        Button(dc, x + 222, y + 7, 44, 40, NULL, i ? B_N : B_DIS, IC_UP, 15, 24, A_WGMOVE, id * 2);
        Button(dc, x + 270, y + 7, 44, 40, NULL, i < WgCount() - 1 ? B_N : B_DIS, IC_DOWN, 15, 24, A_WGMOVE,
               id * 2 + 1);
        Button(dc, x + 318, y + 7, 48, 40, NULL, shown ? B_SEL : B_N, IC_EYE, 15, 24, A_WGVIS, id);
    }
    T(dc, 24, 440, L"вверх / вниз — раньше / позже; следующие занимают место скрытых. Свои виджеты — "
                   L"\\SDMemory\\VestaWidgets\\*.ini", 13, kDim, 400, AL_START, 560);
    if (pages > 1) {
        wchar_t pg[16];
        _snwprintf(pg, 16, L"%d / %d", s_setHomePage + 1, pages);
        Button(dc, 600, 412, 56, 52, NULL, s_setHomePage ? B_N : B_DIS, IC_BACK, 15, 26, A_SHPAGE, -1);
        T(dc, 692, 444, pg, 15, kMuted, 600, AL_MID);
        Button(dc, 728, 412, 56, 52, NULL, s_setHomePage + 1 < pages ? B_N : B_DIS, IC_NEXT, 15, 26, A_SHPAGE, 1);
    }
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
    static const wchar_t* prov[W_COUNT] = { L"OPEN-METEO", L"OPENWEATHER", L"ЯНДЕКС" };
    Frame(dc, L"ПОГОДА · НАСТРОЙКИ");
    Panel(dc, 16, 64, 768, 400);
    wchar_t s[120];
    int p = s_cfg.weatherProv;
    WRow(dc, 68, L"Источник", p == W_OMETEO ? L"открытый, без ключа" : p == W_OWM ? L"бесплатный ключ на openweathermap.org"
         : L"свой ключ «Погоды для умного дома»", A_NONE, 0, 300);
    Seg(dc, 300, 74, 468, 44, prov, W_COUNT, G_WPROV, 14);
    const wchar_t* key = s_cfg.weatherKey[p];
    WRow(dc, 124, L"Ключ API", p == W_OMETEO ? L"не нужен" : key[0] ? key : L"не задан — нажмите, чтобы ввести",
         p == W_OMETEO ? A_NONE : A_KEYS, K_WKEY, 776);
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
    // Open-Meteo's data is CC BY 4.0: its credit wherever it is shown in detail (here; also "About the system")
    bool om = p == W_OMETEO || (w.have && w.prov == W_OMETEO);
    T(dc, 36, 440, om ? L"Обновляется раз в 30 мин. Weather data by Open-Meteo.com (CC BY 4.0)"
                      : L"Обновляется раз в 30 мин.", 13, kDim, 400, AL_START, 728);
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
    static const wchar_t* snd[5] = { L"ВЫКЛ", L"ВСЕГДА", L"> ЛИМИТ", L"+10", L"+20" };
    WRow(dc, 124, L"Звуковой сигнал", L"всегда или если скорость выше лимита камеры", A_NONE, 0, 340);
    Seg(dc, 352, 130, 416, 44, snd, 5, G_CAMSND, 14);
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
    WRow(dc, 292, label, s, A_SCREEN, S_SET_CAR, 776);   // set on "Car" for the whole shell
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
    WRow(dc, 68, L"Климат-контроль в машине", L"виджет «Климат» на главной", A_NONE, 0, 640, true);
    Toggle(dc, 688, 78, T_CLIMATE);
    T(dc, 36, 160, L"Ошибки, температуры, климат и датчик за бортом придут из MCU, когда будет", 15, kMuted, 400,
      AL_START, 728);
    T(dc, 36, 184, L"разобран его протокол. Сейчас значения на виджетах — демо.", 15, kMuted, 400, AL_START, 728);
}

// One line on the CAN module's stream: the car's speed and the steering angle when they come.
static void CanStatusText(wchar_t* s, int n)
{
    Gps g;
    GpsGet(&g);
    DWORD now = GetTickCount();
    bool spd = g.carTick && now - g.carTick < 5000, ang = g.angTick && now - g.angTick < 5000;
    if (g.status != GPS_OPEN)
        _snwprintf(s, n, L"порт %s не открыт", g.port[0] ? g.port : s_cfg.gpsPort);
    else if (spd && ang)
        _snwprintf(s, n, L"v%S · %d км/ч · руль %c%d,%d°", g.canVer, g.carSpeed, g.angle10 < 0 ? L'-' : L'+',
                   abs(g.angle10) / 10, abs(g.angle10) % 10);
    else if (spd)
        _snwprintf(s, n, L"v%S · %d км/ч", g.canVer, g.carSpeed);
    else if (g.carTick)
        wcsncpy(s, L"данные были, сейчас нет", n);
    else
        wcsncpy(s, L"нет строк «@» в потоке", n);
    s[n - 1] = 0;
}

// Settings -> "Car": the data port (the shell's NMEA port is the CAN module's stream), the climate widget, what
// the CAN stream gives, its monitor. CAN settings gather here as they are decoded.
static void ScrSetCar(HDC dc)
{
    enum { H = 48 };
    wchar_t port[16], baud[16], gps[160], can[64];
    GpsPortText(port, 16);
    GpsBaudText(baud, 16);
    GpsStatusText(gps, 160, true);
    CanStatusText(can, 64);
    Settings(dc, S_SET_CAR);
    SetRow(dc, 64, L"Порт GPS · CAN", gps, false, A_NONE, 0, H);
    Stepper(dc, 512, 68, 248, port, 40, 19, A_GPSPORT);
    SetRow(dc, 112, L"Скорость порта", L"или как у оболочки", false, A_NONE, 0, H);
    Stepper(dc, 512, 116, 248, baud, 40, 19, A_GPSBAUD);
    SetRow(dc, 160, L"Данные CAN", can, false, A_SCREEN, S_TEST_CAN, H);
    Chevron(dc, 728, 168);
    wchar_t mcu[80];
    LONG attach, wraps;
    int st = McuState(&attach, &wraps);
    if (st < 0)
        wcscpy(mcu, L"нет общей памяти");
    else if (!st)
        wcscpy(mcu, L"proxy SPIPowerMgr не установлен или старый");
    else
        _snwprintf(mcu, 80, L"proxy работает · сообщений %u", (unsigned)s_mcuTotal);
    SetRow(dc, 208, L"Данные MCU", mcu, false, A_SCREEN, S_TEST_MCU, H);
    Chevron(dc, 728, 216);
    SetRow(dc, 256, L"Климат-контроль в машине", L"виджет «Климат» вместо календаря", false, A_NONE, 0, H);
    Toggle(dc, 688, 262, T_CLIMATE);
    SetRow(dc, 304, L"Камера заднего хода — своя", L"видео и разметка VestaHome", true, A_NONE, 0, H);
    Button(dc, 552, 310, 124, 40, L"ПОКАЗАТЬ", B_N, IC_NONE, 14, 0, A_RVCVIEW);   // by hand; a tap closes it
    Toggle(dc, 688, 310, T_RVC_OWN);
    T(dc, 264, 372, L"CAN-модуль даёт скорость и угол руля; климат, наружная температура,", 14, kDim, 400,
      AL_START, 504);
    T(dc, 264, 392, L"кнопки и версии приходят от MCU — через proxy SPIPowerMgr.", 14, kDim, 400, AL_START, 504);
}

// MCU monitor ("Car -> MCU data", "Diagnostics -> keys"): the proxy's state, every message id with its count,
// age and payload (the latest first), the climate decoded; recording into mcu.log with marks, the same МЕТКА
// goes into can.log too when that one records.
static void ScrTestMcu(HDC dc)
{
    McuStat st[kMcuStatMax];
    int count;
    DWORD total, logBytes;
    bool rec;
    EnterCriticalSection(&s_mcuCs);
    count = s_mcuStatCount;
    memcpy(st, s_mcuStat, count * sizeof(McuStat));
    total = s_mcuTotal;
    rec = s_mcuLog != INVALID_HANDLE_VALUE;
    logBytes = s_mcuLogBytes;
    LeaveCriticalSection(&s_mcuCs);
    // the latest first: what changed when something was done in the car is on top
    for (int i = 1; i < count; i++)
        for (int j = i; j > 0 && (int)(st[j].tick - st[j - 1].tick) > 0; j--) {
            McuStat t = st[j];
            st[j] = st[j - 1];
            st[j - 1] = t;
        }

    wchar_t s[160];
    Frame(dc, L"ДАННЫЕ MCU");
    Panel(dc, 16, 68, 768, 396);
    LONG attach, wraps;
    int state = McuState(&attach, &wraps);
    if (state < 0)
        wcscpy(s, L"Общей памяти нет — VestaHome не смог её создать");
    else if (!state)
        wcscpy(s, L"Proxy не пишет: не установлен или старая версия — установить новую (VestaHomeInstall)");
    else
        _snwprintf(s, 160, L"Proxy: загрузок %d, обработчиков %d · сообщений %u, разных %d", (int)attach, (int)wraps,
                   (unsigned)total, count);
    T(dc, 32, 96, s, 16, state > 0 ? kText : kOrange, 700, AL_START, 736);
    Hvac h;
    if (HvacGet(&h) && HvacChip(s, 160)) {
        if (h.outer > -100) {
            wchar_t o[24];
            _snwprintf(o, 24, L" · за бортом %+d°", h.outer);
            wcscat(s, o);
        }
    } else {
        wcscpy(s, L"Климат: ещё не приходил (0x61xx)");
    }
    T(dc, 32, 122, s, 15, kMuted, 600, AL_START, 736);
    Sep(dc, 32, 134, 736);
    DWORD now = GetTickCount();
    for (int i = 0; i < count && i < 7; i++) {
        int y = 140 + i * 34;
        wchar_t name[16], meta[32], hex[128];
        _snwprintf(name, 16, L"%04X", (unsigned)st[i].id);
        _snwprintf(meta, 32, L"%u · %u с", (unsigned)st[i].count, (unsigned)((now - st[i].tick) / 1000));
        int n = 0, shown = st[i].len < 14 ? st[i].len : 14;
        for (int k = 0; k < shown && n < 120; k++)
            n += _snwprintf(hex + n, 128 - n, L"%02X ", st[i].data[k]);
        hex[n] = 0;
        if (n + 4 < 128 && shown) {   // and as text when it looks like text (versions, names)
            int printable = 0;
            for (int k = 0; k < shown; k++)
                printable += st[i].data[k] >= 0x20 && st[i].data[k] <= 0x7E;
            if (printable >= 4 && printable * 2 >= shown) {
                wcscat(hex, L" «");
                n = (int)wcslen(hex);
                for (int k = 0; k < shown && n < 124; k++)
                    hex[n++] = st[i].data[k] >= 0x20 && st[i].data[k] <= 0x7E ? (wchar_t)st[i].data[k] : L'.';
                hex[n++] = L'»';
                hex[n] = 0;
            }
        }
        DWORD age = (now - st[i].tick) / 1000;
        T(dc, 32, y + 24, name, 15, age < 3 ? kOrange : kMuted, 700, AL_START, 60);
        T(dc, 96, y + 24, meta, 14, kMuted, 400, AL_START, 110);
        T(dc, 212, y + 24, hex, 14, kText, 400, AL_START, 556);
    }
    if (!count)
        T(dc, 32, 170, L"Сообщений ещё не было", 16, kMuted, 400);
    if (rec)
        _snwprintf(s, 160, L"запись: mcu.log · %u КБ · меток %d", (unsigned)(logBytes / 1024), s_marks);
    else
        wcscpy(s, L"запись выключена");
    T(dc, 32, 432, s, 14, rec ? kOrange : kDim, 400, AL_START, 280);
    Button(dc, 320, 400, 120, 52, L"ЗАПРОС", state > 0 ? B_N : B_DIS, IC_NONE, 14, 0, A_MCUREQ);
    Button(dc, 448, 400, 152, 52, rec ? L"СТОП ЗАПИСИ" : L"ЗАПИСЬ", rec ? B_SEL : B_N, IC_NONE, 15, 0, A_MCULOG);
    Button(dc, 608, 400, 160, 52, L"МЕТКА", rec ? B_N : B_DIS, IC_NONE, 15, 0, A_CANMARK);
}

// "About the system -> Details": what the stock "Info" and its engineering "Software" / "System status" show
// (photos MAIN/Foto/info: SW / MCU / CAN / OS versions, ID, BT, HW, temperatures, steering-wheel keys voltages,
// tuner). Here: what VestaHome knows itself, and the MCU's answers that look like text (versions, names) - asked
// with "Request" (product info, BT address). Which message carries which field is decoded from mcu.log later.
static void McuRequest();

static void ScrInfoMore(HDC dc)
{
    wchar_t s[160], can[64];
    Frame(dc, L"О СИСТЕМЕ · ПОДРОБНЕЕ");
    Panel(dc, 16, 68, 444, 396);
    Panel(dc, 472, 68, 312, 396);
    T(dc, 32, 96, L"ВЕРСИИ", 13, kMuted, 700);
    WIN32_FIND_DATA fd;
    HANDLE f = FindFirstFile(L"\\ResidentFlash\\navigation\\app\\G3NavHMI.exe", &fd);
    if (f != INVALID_HANDLE_VALUE) {
        FindClose(f);
        _snwprintf(s, 160, L"%s", fd.nFileSizeLow == 4077056 ? L"27.0.0.4120" : L"неизвестна");
    } else {
        wcscpy(s, L"—");
    }
    OSVERSIONINFO vi;
    memset(&vi, 0, sizeof(vi));
    vi.dwOSVersionInfoSize = sizeof(vi);
    wchar_t os[40] = L"—";
    if (GetVersionEx(&vi))
        _snwprintf(os, 40, L"%u.%u.%u", (unsigned)vi.dwMajorVersion, (unsigned)vi.dwMinorVersion,
                   (unsigned)vi.dwBuildNumber);
    Gps g;
    GpsGet(&g);
    wchar_t canv[16] = L"—";
    if (g.canVer[0])
        _snwprintf(canv, 16, L"%S", g.canVer);
    LONG attach, wraps;
    int st = McuState(&attach, &wraps);
    static DWORD s_asked;   // the MCU's versions and BT address, asked for once on opening (as the stock "Info")
    if (st > 0 && (!s_asked || GetTickCount() - s_asked > 30000)) {
        s_asked = GetTickCount();
        McuRequest();
    }
    EnterCriticalSection(&s_mcuCs);
    McuInfo inf = s_info;
    LeaveCriticalSection(&s_mcuCs);
    wchar_t mcu[48], hw[24], bt[24], id[40];
    if (inf.sw[0] && inf.cs >= 0)
        _snwprintf(mcu, 48, L"%s · CS %04X", inf.sw, inf.cs);
    else
        wcscpy(mcu, inf.sw[0] ? inf.sw : L"—");
    wcscpy(hw, inf.hw[0] ? inf.hw : L"—");
    if (inf.btHave)
        _snwprintf(bt, 24, L"%02X:%02X:%02X:%02X:%02X:%02X", inf.bt[0], inf.bt[1], inf.bt[2], inf.bt[3], inf.bt[4],
                   inf.bt[5]);
    else
        wcscpy(bt, L"—");
    if (inf.id[0])
        _snwprintf(id, 40, L"%s %s %s", inf.id, inf.idDate, inf.idNum);
    else
        wcscpy(id, L"—");
    const wchar_t* rows[9][2] = { { L"Оболочка G3 (SW)", s }, { L"Windows CE", os }, { L"MCU (радио)", mcu },
                                  { L"Железо (HW)", hw }, { L"CAN-модуль", canv }, { L"Bluetooth-адрес", bt },
                                  { L"ID устройства", id }, { L"Proxy MCU", st > 0 ? L"работает" : L"нет" },
                                  { L"VestaHome", L"0.5 · 2026-10-18" } };
    for (int i = 0; i < 9; i++) {
        int y = 100 + i * 33;
        T(dc, 32, y + 22, rows[i][0], 15, kMuted, 400);
        T(dc, 444, y + 22, rows[i][1], 15, kText, 700, AL_END, 260);
    }
    Button(dc, 32, 412, 200, 44, L"ЗАПРОСИТЬ", st > 0 ? B_N : B_DIS, IC_REFRESH, 14, 22, A_MCUREQ);
    Button(dc, 244, 412, 200, 44, L"ДАННЫЕ MCU", B_N, IC_NONE, 14, 0, A_SCREEN, S_TEST_MCU);

    T(dc, 488, 96, L"СОСТОЯНИЕ", 13, kMuted, 700);
    MEMORYSTATUS ms;
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatus(&ms);
    wchar_t ram[40], up[24], temp[32], sw[32], field[16], gps[40];
    _snwprintf(ram, 40, L"%u из %u МБ", (unsigned)(ms.dwAvailPhys >> 20), (unsigned)(ms.dwTotalPhys >> 20));
    DWORD m = GetTickCount() / 60000;
    _snwprintf(up, 24, L"%u ч %02u мин", (unsigned)(m / 60), (unsigned)(m % 60));
    // System status (0x7905; the MCU sends it every 2 s for a while after start, not always)
    if (inf.sys[0x0A] >= 0 || inf.sys[0x06] >= 0)
        _snwprintf(temp, 32, L"плата %d °C · усилитель %d °C", inf.sys[0x0A], inf.sys[0x06]);
    else
        wcscpy(temp, L"—");
    if (inf.sys[0x03] >= 0)
        _snwprintf(sw, 32, L"%d · %d", inf.sys[0x03], inf.sys[0x0E]);
    else
        wcscpy(sw, L"—");
    if (inf.sys[0x07] >= 0)
        _snwprintf(field, 16, L"%d дБ", inf.sys[0x07]);
    else
        wcscpy(field, L"—");
    _snwprintf(gps, 40, L"%s · %d спутн.", GpsFresh(g, 5000) ? L"фикс" : L"нет фикса", g.sats);
    CanStatusText(can, 64);
    const wchar_t* right[7][2] = { { L"Память свободно", ram }, { L"Работает", up }, { L"GPS", gps }, { L"CAN", can },
                                   { L"Температура", temp }, { L"Кнопки руля (покой)", sw },
                                   { L"Поле тюнера", field } };
    for (int i = 0; i < 7; i++) {
        int y = 100 + i * 41;
        T(dc, 488, y + 18, right[i][0], 13, kMuted, 400);
        T(dc, 488, y + 37, right[i][1], 15, kText, 700, AL_START, 284);
    }
    Button(dc, 488, 412, 280, 44, L"ЛИЦЕНЗИИ И СЕРВИСЫ", B_N, IC_INFO, 13, 22, A_SCREEN, S_LICENSES);
}

// "Details -> Licenses and services": what VestaHome brings that the unit's firmware doesn't have - the libraries
// and their licenses - and the internet services it uses (Open-Meteo's CC BY 4.0 credit among them). A window
// over the screen; the text scrolls with the finger or the arrows.
struct LicItem { const wchar_t* title; const wchar_t* text; };
static const LicItem kLicenses[] = {
    { L"БИБЛИОТЕКИ (В СОСТАВЕ VESTAHOME)", NULL },
    { L"BearSSL 0.6", L"Thomas Pornin · лицензия MIT · bearssl.org. HTTPS: погода Яндекса, база камер." },
    { L"Корневые сертификаты Mozilla", L"Mozilla CA Certificate Store · MPL 2.0. Проверка серверов HTTPS "
                                      L"(часть сертификатов, встроена в программу)." },
    { L"stb_image 2.30", L"Sean Barrett · public domain (или MIT) · github.com/nothings/stb. Значки скина PNG, "
                         L"фото PNG, BMP и прогрессивные JPEG." },
    { L"TJpgDec R0.03", L"ChaN · свободная лицензия (с сохранением уведомления об авторстве) · elm-chan.org. "
                        L"Фото JPEG: уменьшение при чтении." },
    { L"BASS 2.4.17 (Windows CE)", L"Un4seen Developments · бесплатно для некоммерческого использования · "
                                   L"un4seen.com. Музыка и интернет-радио." },
    { L"BASSFLAC 2.4.4, BASSALAC 2.4, BASSOPUS 2.4.2, BASSWV 2.4.7", L"Un4seen Developments · дополнения к BASS. "
                                                                    L"FLAC, Apple Lossless, Opus, WavPack." },
    { L"BASS_AAC 2.4.5, BASS_MPC 2.4.1.3", L"MaresWEB · дополнения к BASS. AAC и M4A, Musepack." },
    { L"tags.dll (18)", L"Дополнение к BASS. Названия, исполнители и альбомы из тегов треков." },
    { L"СЕРВИСЫ", NULL },
    { L"Open-Meteo.com", L"Weather data by Open-Meteo.com, CC BY 4.0. Погода по умолчанию, без ключа." },
    { L"OpenWeatherMap", L"openweathermap.org. Погода, если выбрана и задан свой ключ." },
    { L"Яндекс Погода", L"yandex.ru/pogoda. Погода, если выбрана и задан свой ключ (тариф «погода для умного "
                        L"дома», до 30 запросов в день)." },
    { L"speedcamonline.ru", L"База камер скорости: скачивается по кнопке в настройках камер." },
    { L"Интернет-радио", L"Потоки станций из radio.txt принадлежат самим станциям." },
    { L"Свои виджеты", L"Загружают адреса из своих .ini (url=) через HTTPS VestaHome; данные принадлежат их источникам. "
                       L"Пример probki.ini - баллы Яндекс Пробок (неофициальный адрес export.yandex.ru)." },
    { L"ИЗ ПРОШИВКИ ГУ (НЕ ВХОДИТ В VESTAHOME)", NULL },
    { NULL, L"Видео — GStreamer штатного плеера; Bluetooth — стек FLC оболочки; радио, звук и кнопки — MCU через "
            L"SPIPowerMgr; шрифт Tahoma; Windows CE 6." },
};
enum { kLicTop = 104, kLicBottom = 420 };
static int s_licScroll, s_licHeight;   // the text scrolled by / its height (known after a frame)

static int LicScrollMax()
{
    int view = kLicBottom - kLicTop;
    return s_licHeight > view ? s_licHeight - view : 0;
}

static void ScrLicenses(HDC dc)
{
    Under(dc, 153);
    Zone(0, 0, 800, 480, A_BACK);   // a tap beside the window closes it
    Zone(40, 40, 720, 400, A_NONE);
    Panel(dc, 40, 40, 720, 400, 16);
    T(dc, 64, 82, L"ЛИЦЕНЗИИ И СЕРВИСЫ", 17, kText, 700);
    Button(dc, 696, 52, 52, 44, NULL, B_N, IC_CLOSE, 15, 24, A_BACK);
    Sep(dc, 56, kLicTop - 6, 688);
    s_licScroll = s_licScroll < 0 ? 0 : s_licScroll > LicScrollMax() ? LicScrollMax() : s_licScroll;
    int saved = SaveDC(dc);
    IntersectClipRect(dc, 56, kLicTop, 704, kLicBottom);
    int y = kLicTop + 8 - s_licScroll;
    for (int i = 0; i < (int)(sizeof(kLicenses) / sizeof(kLicenses[0])); i++) {
        const LicItem& it = kLicenses[i];
        if (!it.text) {   // a section's heading
            y += i ? 14 : 0;
            T(dc, 64, y + 14, it.title, 13, kOrange, 700);
            y += 24;
            continue;
        }
        if (it.title) {
            T(dc, 64, y + 18, it.title, 17, kText, 700, AL_START, 620);
            y += 24;
        }
        y += TWrap(dc, 64, y + 16, it.text, 14, kMuted, 400, 620, 6, 19) * 19 + 8;
    }
    s_licHeight = y + s_licScroll - kLicTop;
    RestoreDC(dc, saved);
    if (LicScrollMax() > 0) {   // the arrows and the bar
        int m = LicScrollMax(), view = kLicBottom - kLicTop;
        Button(dc, 712, kLicTop, 40, 60, NULL, s_licScroll > 0 ? B_N : B_DIS, IC_UP, 14, 22, A_LICSCROLL, -1);
        Button(dc, 712, kLicBottom - 60, 40, 60, NULL, s_licScroll < m ? B_N : B_DIS, IC_DOWN, 14, 22, A_LICSCROLL,
               1);
        int track = view - 136, th = track * view / s_licHeight, ty = kLicTop + 68 + (track - th) * s_licScroll / m;
        th = th < 16 ? 16 : th;
        R(dc, 729, kLicTop + 68, 6, track, 3, kLine);
        R(dc, 729, ty, 6, th, 3, kMuted);
    }
}

// "Request": product info and the BT address, as the stock "Info" asks for them (FUN_00679190); the answers come
// back as MCU messages and show up in the monitor and on "Details".
static void McuRequest()
{
    if (McuSend0("SPApi_SendRadioRequestProductInfo")) {
        McuSend0("SPApi_SendRadioRequestBTAddress");
        wcscpy(s_status, L"запрос сведений → MCU");
    }
}

// ---- sound: the stock shell's real controls (PROJECT.md): a 3-band equalizer -6..6 (CAudioEffectDlg) and
// balance/fader -8..8 (CAudioFieldDlg), sent to the MCU as G3NavHMI sends them. The MCU keeps them itself; what
// VestaHome shows is what it last sent (VestaHome.ini).
struct EqPreset { const wchar_t* name; int v[3]; };
static const EqPreset kEqPresets[5] = { { L"РОВНО", { 0, 0, 0 } }, { L"ПОП", { -1, 2, 1 } },
                                        { L"РОК", { 4, -1, 3 } }, { L"ДЖАЗ", { 2, 1, 3 } },
                                        { L"ГОЛОС", { -2, 3, 1 } } };
static const int kEqBand[3] = { 2, 1, 0 };   // low, mid, high -> the MCU's band (0 = high, OnTimerHighSlot)

static DWORD s_sndSentTick;   // our last EQ / balance: the MCU's echoes of older values are not taken back meanwhile

static void EqSend(int i)
{
    s_sndSentTick = GetTickCount();
    if (McuSend2("SPApi_SendRadioAudioHMLData", kEqBand[i], s_cfg.eq[i] + 6))
        wcscpy(s_status, L"эквалайзер → MCU");
}

static void EqSet(int arg)   // band * 16 + value + 6
{
    int band = arg / 16, v = arg % 16 - 6;
    if (band < 0 || band > 2 || s_cfg.eq[band] == v)
        return;
    s_cfg.eq[band] = v;
    SaveCfg();
    EqSend(band);
}

static void EqPresetSet(int p)
{
    for (int i = 0; i < 3; i++)
        if (s_cfg.eq[i] != kEqPresets[p].v[i]) {
            s_cfg.eq[i] = kEqPresets[p].v[i];
            EqSend(i);
        }
    SaveCfg();
}

static void BalanceSend()
{
    s_sndSentTick = GetTickCount();
    // the MCU's fader runs the other way from our screen's (HU 2026-10-20: front and rear were swapped)
    if (McuSend2("SPApi_SendRadioAudioBalanceFaderData", s_cfg.balance + 8, 8 - s_cfg.fader))
        wcscpy(s_status, L"баланс → MCU");
}

static void BalanceStep(bool fader, int dir)   // dir 0 = both to the centre
{
    int* v = fader ? &s_cfg.fader : &s_cfg.balance;
    if (!dir) {
        s_cfg.balance = s_cfg.fader = 0;
    } else {
        int n = *v + dir;
        if (n < -8 || n > 8)
            return;
        *v = n;
    }
    SaveCfg();
    BalanceSend();
}

// The car from above on the balance screen: a tap or a drag puts the point there (as in the stock shell's
// CAudioFieldDlg). The point's place: x = kBalX + balance * kBalDX, y = kBalY + fader * kBalDY.
enum { kBalX = 376, kBalY = 230, kBalDX = 10, kBalDY = 18 };

static int BalRound(int d, int step)
{
    int v = (d + (d < 0 ? -step / 2 : step / 2)) / step;
    return v < -8 ? -8 : v > 8 ? 8 : v;
}

// A point of the screen -> balance and fader; sent when they change. save: the finger is up.
static void BalanceAt(int x, int y, bool save)
{
    int b = BalRound(x - kBalX, kBalDX), f = BalRound(y - kBalY, kBalDY);
    if (b != s_cfg.balance || f != s_cfg.fader) {
        s_cfg.balance = b;
        s_cfg.fader = f;
        BalanceSend();
        Redraw();
    }
    if (save)
        SaveCfg();
}

// Equalizer: presets, three sliders (a tap on a slider sets the value there), balance and volumes as buttons.
static void ScrSetSound(HDC dc)
{
    static const wchar_t* bands[3] = { L"НИЗКИЕ", L"СРЕДНИЕ", L"ВЫСОКИЕ" };
    static DWORD s_seen;   // opened: the shell's key of its equalizer screen - the MCU takes the changes only after it
    if (s_seen != s_screenTick) {   // (HU 2026-10-18-2: ours alone had no answer, the shell's 0xE6 first had)
        s_seen = s_screenTick;
        McuKey(0xE6, false);
    }
    Settings(dc, S_SET_SOUND);
    T(dc, 264, 92, L"Эквалайзер (MCU)", 19, kText, 700);
    for (int p = 0; p < 5; p++) {
        bool cur = s_cfg.eq[0] == kEqPresets[p].v[0] && s_cfg.eq[1] == kEqPresets[p].v[1] &&
                   s_cfg.eq[2] == kEqPresets[p].v[2];
        Button(dc, 256 + p * 84, 104, 78, 40, kEqPresets[p].name, cur ? B_SEL : B_N, IC_NONE, 13, 0, A_EQPRESET, p);
    }
    enum { Y = 186, H = 132 };
    for (int i = 0; i < 3; i++) {
        int x = 304 + i * 104;
        wchar_t v[8];
        _snwprintf(v, 8, L"%+d", s_cfg.eq[i]);
        VSlider(dc, x, Y, H, (s_cfg.eq[i] + 6) * 100 / 12, bands[i], s_cfg.eq[i] ? v : L"0");
        for (int k = 0; k <= 12; k++)   // 13 tap zones down the slider, top = +6
            Zone(x - 30, Y - 6 + (12 - k) * H / 12, 60, H / 12 + 1, A_EQ, i * 16 + k);
    }
    Button(dc, 600, 160, 168, 64, L"БАЛАНС", B_N, IC_EQ, 15, 28, A_SCREEN, S_SET_BALANCE);
    Button(dc, 600, 236, 168, 64, L"ГРОМКОСТЬ", B_N, IC_VOL, 15, 28, A_SCREEN, S_SET_VOLUME);
    Sep(dc, 256, 350, 512);
    T(dc, 264, 384, L"Тонкомпенсация (демо)", 19, kText, 700);
    Toggle(dc, 688, 358, T_LOUDNESS);
}

static void ScrSetBalance(HDC dc)
{
    static const wchar_t* speed[4] = { L"ВЫКЛ", L"1", L"2", L"3" };
    static DWORD s_seen;   // opened: the shell's key of its balance screen (as 0xE6 for the equalizer)
    if (s_seen != s_screenTick) {
        s_seen = s_screenTick;
        McuKey(0xE4, false);
    }
    Settings(dc, S_SET_BALANCE);
    R(dc, 276, 76, 200, 316, 40, kPanel2);   // the car from above, four seats
    R(dc, 296, 126, 70, 76, 10, kInk);
    R(dc, 386, 126, 70, 76, 10, kInk);
    R(dc, 296, 256, 70, 76, 10, kInk);
    R(dc, 386, 256, 70, 76, 10, kInk);
    Dash(dc, 376, 84, 376, 384, 4, 6, kDim);
    Dash(dc, 284, 230, 468, 230, 4, 6, kDim);
    Zone(276, 76, 200, 316, A_BALXY);   // the whole car: tap or drag
    int cx = kBalX + s_cfg.balance * kBalDX, cy = kBalY + s_cfg.fader * kBalDY;   // + fader = to the rear (to verify)
    Disc(dc, cx, cy, 18, kOrange);
    Disc(dc, cx, cy, 6, kInk);
    wchar_t b[24], f[24];
    if (s_cfg.balance)
        _snwprintf(b, 24, L"%s %d", s_cfg.balance < 0 ? L"ЛЕВО" : L"ПРАВО", abs(s_cfg.balance));
    else
        wcscpy(b, L"ЦЕНТР");
    if (s_cfg.fader)
        _snwprintf(f, 24, L"%s %d", s_cfg.fader < 0 ? L"ПЕРЕД" : L"ЗАД", abs(s_cfg.fader));
    else
        wcscpy(f, L"ЦЕНТР");
    T(dc, 504, 96, L"Баланс", 19, kText, 700);
    Stepper(dc, 504, 108, 256, b, 48, 20, A_BAL);
    T(dc, 504, 184, L"Фейдер", 19, kText, 700);
    Stepper(dc, 504, 196, 256, f, 48, 20, A_FADE);
    T(dc, 504, 272, L"Громкость от скорости (демо)", 19, kText, 700, AL_START, 260);
    Seg(dc, 504, 284, 256, 44, speed, 4, G_SPEED_VOL, 15);
    Button(dc, 504, 344, 256, 48, L"В ЦЕНТР", B_N, IC_NONE, 16, 0, A_BALCENTER);
}

// Volumes of the sources (CAudioVolumeDlg). Demo: of SPApi_SendRadioAudioVolData(kind, value) only kind 5
// (traffic announcements, CRDSSetupDlg) is known, so nothing is sent from here yet.
static int s_sndVol[4] = { 12, 18, 15, 10 };

static void ScrSetVolume(HDC dc)
{
    static const wchar_t* rows[4][2] = { { L"Навигация", L"подсказки навигатора" },
                                         { L"Телефон", L"разговор по Bluetooth" },
                                         { L"Дорожные сообщения", L"RDS TA" },
                                         { L"При включении", L"громкость после запуска" } };
    Settings(dc, S_SET_SOUND);
    for (int i = 0; i < 4; i++) {
        wchar_t v[8];
        _snwprintf(v, 8, L"%d", s_sndVol[i]);
        SetRow(dc, 64 + i * 56, rows[i][0], rows[i][1], i == 3);
        Stepper(dc, 560, 68 + i * 56, 200, v, 48, 20, A_SNDVOL);
        // the stepper sends -1/+1: which row is in the high bits
        s_btns[kBtnCount - 2].arg = i * 4 + 1;
        s_btns[kBtnCount - 1].arg = i * 4 + 3;
    }
    T(dc, 264, 312, L"Демо: из громкостей MCU разобрана только «дорожные сообщения» (вид 5),", 14, kDim, 400,
      AL_START, 504);
    T(dc, 264, 332, L"поэтому отсюда в MCU пока ничего не уходит.", 14, kDim, 400, AL_START, 504);
}

// The stream monitor ("Diagnostics -> CAN / UART", "Car -> Data stream"): the port, bytes, the car's values and
// every sentence type with its count, age and last line; recording into can.log with marks for decoding.
static void ScrTestCan(HDC dc)
{
    Gps g;
    GpsGet(&g);
    Sentence sent[kSentMax];
    int count;
    DWORD bytes, bin, bad, logBytes;
    bool rec;
    EnterCriticalSection(&s_gpsCs);
    count = s_sentCount;
    memcpy(sent, s_sent, count * sizeof(Sentence));
    bytes = g.bytes;          // of the program that reads the port (VestaCore)
    bin = s_streamBin;        // seen only while this program reads it
    bad = (DWORD)g.badLines;
    rec = s_canLog != INVALID_HANDLE_VALUE;
    logBytes = s_canLogBytes;
    LeaveCriticalSection(&s_gpsCs);

    wchar_t s[160], gps[160], can[64];
    _snwprintf(s, 160, L"ПОТОК ДАННЫХ · %s", s_cfg.gpsPort);
    Frame(dc, s);
    Panel(dc, 16, 68, 768, 396);
    GpsStatusText(gps, 160);
    CanStatusText(can, 64);
    _snwprintf(s, 160, L"GPS: %s", gps);
    T(dc, 32, 96, s, 16, kText, 700, AL_START, 736);
    _snwprintf(s, 160, L"CAN: %s · принято %u КБ, не текст %u Б, битых строк %u", can, (unsigned)(bytes / 1024),
               (unsigned)bin, (unsigned)bad);
    T(dc, 32, 122, s, 16, kText, 700, AL_START, 736);
    Sep(dc, 32, 134, 736);
    // the busiest types first would jump around; keep the order they first came in, 7 rows
    DWORD now = GetTickCount();
    if (!count)
        T(dc, 32, 170, L"Строк «$» и «@» ещё не было", 16, kMuted, 400);
    for (int i = 0; i < count && i < 7; i++) {
        int y = 140 + i * 34;
        wchar_t name[16], last[104], meta[32];
        MultiByteToWideChar(CP_ACP, 0, sent[i].name[0] ? sent[i].name : "?", -1, name, 16);
        MultiByteToWideChar(CP_ACP, 0, sent[i].last, -1, last, 104);
        DWORD age = (now - sent[i].tick) / 1000;
        _snwprintf(meta, 32, L"%u · %u с", (unsigned)sent[i].count, (unsigned)age);
        T(dc, 32, y + 24, name, 15, age < 5 ? kOrange : kMuted, 700, AL_START, 90);
        T(dc, 128, y + 24, meta, 14, kMuted, 400, AL_START, 100);
        T(dc, 236, y + 24, last, 14, kText, 400, AL_START, 532);
    }
    if (count > 7) {
        _snwprintf(s, 160, L"ещё типов: %d", count - 7);
        T(dc, 32, 392, s, 14, kDim, 400);
    }
    if (rec)
        _snwprintf(s, 160, L"запись: can.log · %u КБ из %u · меток %d", (unsigned)(logBytes / 1024),
                   (unsigned)(kCanLogMax / 1024), s_canMarks);
    else
        wcscpy(s, L"запись выключена: включите и делайте действия в машине, отмечая их МЕТКОЙ");
    T(dc, 32, 412, s, 14, rec ? kOrange : kDim, 400, AL_START, 400);
    Button(dc, 440, 400, 160, 52, rec ? L"СТОП ЗАПИСИ" : L"ЗАПИСЬ", rec ? B_SEL : B_N, IC_NONE, 15, 0, A_CANLOG);
    Button(dc, 608, 400, 160, 52, L"МЕТКА", rec ? B_N : B_DIS, IC_NONE, 15, 0, A_CANMARK);
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

// A known network (name + password) into the settings and to the Wi-Fi thread; pass NULL = forget it.
static void WifiKnownSet(const wchar_t* ssid, const wchar_t* pass)
{
    int k = 0;
    while (k < s_cfg.wifiCount && wcscmp(s_cfg.wifiSsid[k], ssid))
        k++;
    if (!pass) {
        if (k < s_cfg.wifiCount) {
            int last = --s_cfg.wifiCount;
            wcscpy(s_cfg.wifiSsid[k], s_cfg.wifiSsid[last]);
            wcscpy(s_cfg.wifiPass[k], s_cfg.wifiPass[last]);
            wcscpy(s_cfg.wifiSealed[k], s_cfg.wifiSealed[last]);
            s_cfg.wifiLocked[k] = s_cfg.wifiLocked[last];
        }
        WifiForget(ssid);
    } else {
        if (k == kWifiKnown)
            return;
        wcsncpy(s_cfg.wifiSsid[k], ssid, 32);
        s_cfg.wifiSsid[k][32] = 0;
        wcsncpy(s_cfg.wifiPass[k], pass, 63);
        s_cfg.wifiPass[k][63] = 0;
        s_cfg.wifiLocked[k] = false;
        if (k == s_cfg.wifiCount)
            s_cfg.wifiCount++;
        WifiRemember(ssid, pass);
    }
    SaveCfg();
}

// A tap on a network: a known or open one connects at once, a new secured one asks its password first.
static void WifiTap(int i)
{
    WifiInfo w;
    WifiGet(&w);
    if (i < 0 || i >= w.count)
        return;
    const WifiNet& n = w.nets[i];
    if (w.state == WIFI_CONNECTED && !wcscmp(w.ssid, n.ssid)) {
        wcscpy(s_status, L"уже подключено");
        return;
    }
    for (int k = 0; k < s_cfg.wifiCount; k++)
        if (!s_cfg.wifiLocked[k] && !wcscmp(s_cfg.wifiSsid[k], n.ssid)) {
            WifiConnect(n.ssid, s_cfg.wifiPass[k]);
            return;
        }
    if (n.sec == WSEC_OPEN) {
        WifiKnownSet(n.ssid, L"");
        WifiConnect(n.ssid, L"");
        return;
    }
    wcscpy(s_wifiTarget, n.ssid);
    s_wifiPass[0] = 0;
    _snwprintf(s_wifiTitle, 64, L"ПАРОЛЬ · %s", n.ssid);
    s_wifiTitle[63] = 0;
    OpenKeysFor(s_wifiPass, 63, s_wifiTitle, S_SET_WIFI);
}

static void WifiForgetTap(int i)
{
    WifiInfo w;
    WifiGet(&w);
    if (i >= 0 && i < w.count)
        WifiKnownSet(w.nets[i].ssid, NULL);
}

// Typed into a setting: coordinates are parsed, the file written, what depends on it restarted.
static void CfgKeysDone()
{
    if (s_keysTarget == s_wifiPass) {   // the password of a new network: kept and connected with
        if (s_wifiPass[0]) {
            WifiKnownSet(s_wifiTarget, s_wifiPass);
            WifiConnect(s_wifiTarget, s_wifiPass);
        }
        return;
    }
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
    s_seg[G_CAMSND] = !s_cfg.camSound ? 0 : s_cfg.camOver < 0 ? 1 : s_cfg.camOver < 10 ? 2 : s_cfg.camOver < 20 ? 3 : 4;
    s_on[T_CAM_WEEKLY] = s_cfg.camWeekly;
    s_on[T_VIDEO_MOVING] = s_cfg.videoMoving;
    s_on[T_RVC_OWN] = s_cfg.rvcOwn;
    s_on[T_WIFI] = s_cfg.wifiOn;
    s_on[T_WIFI_AP_AUTO] = s_cfg.wifiApAuto;
    s_on[T_SLIDES_OPEN] = s_cfg.photoSlides;
    s_on[T_AUTOROTATE] = s_cfg.photoAutoRot;
    s_seg[G_PHOTO_SCALE] = s_cfg.photoFill ? 1 : 0;
    s_seg[G_VIDEO_SCALE] = s_cfg.videoStretch ? 1 : 0;
    s_seg[G_WPROV] = s_cfg.weatherProv;
    s_seg[G_CAMDIST] = 0;
    for (int i = 0; i < 4; i++)
        if (kCamDists[i] == s_cfg.camDist)
            s_seg[G_CAMDIST] = i;
}

static bool IsCfgToggle(int id)
{
    return id == T_GPS_TIME || id == T_W_GPS || id == T_CLIMATE || id == T_CAM_ALERT ||
           id == T_CAM_WEEKLY || id == T_VIDEO_MOVING || id == T_RVC_OWN || id == T_WIFI ||
           id == T_WIFI_AP_AUTO || id == T_SLIDES_OPEN || id == T_AUTOROTATE;
}

static void ControlsToCfg()
{
    bool weather = s_cfg.weatherGps != s_on[T_W_GPS] || s_cfg.weatherProv != s_seg[G_WPROV];
    s_cfg.gpsTime = s_on[T_GPS_TIME];
    s_cfg.weatherGps = s_on[T_W_GPS];
    s_cfg.climate = s_on[T_CLIMATE];
    s_cfg.camAlert = s_on[T_CAM_ALERT];
    {   // the beep: off / always / above the camera's limit by 0, 10, 20 km/h
        static const int over[5] = { -1, -1, 0, 10, 20 };
        s_cfg.camSound = s_seg[G_CAMSND] > 0;
        s_cfg.camOver = over[s_seg[G_CAMSND] % 5];
    }
    s_cfg.camWeekly = s_on[T_CAM_WEEKLY];
    s_cfg.videoMoving = s_on[T_VIDEO_MOVING];
    s_cfg.photoSlides = s_on[T_SLIDES_OPEN];
    s_cfg.photoAutoRot = s_on[T_AUTOROTATE];
    s_cfg.rvcOwn = s_on[T_RVC_OWN];
    if (s_cfg.wifiOn != s_on[T_WIFI]) {
        s_cfg.wifiOn = s_on[T_WIFI];
        WifiEnable(s_cfg.wifiOn);
    }
    if (s_cfg.wifiApAuto != s_on[T_WIFI_AP_AUTO]) {
        s_cfg.wifiApAuto = s_on[T_WIFI_AP_AUTO];
        WifiApSetup(s_cfg.wifiApSsid, s_cfg.wifiApPass, s_cfg.wifiApAuto);
    }
    if (s_cfg.videoStretch != (s_seg[G_VIDEO_SCALE] == 1)) {
        s_cfg.videoStretch = s_seg[G_VIDEO_SCALE] == 1;
        VideoSetStretch(s_cfg.videoStretch);
    }
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
    VLog(L"поток базы камер: id %08X", (unsigned)GetCurrentThreadId());
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
    case S_SET_OBD:     ScrSetObd(dc); break;
    case S_BT_PAIRED:   ScrBtPaired(dc); break;
    case S_SET_VIDEO:   ScrSetVideo(dc); break;
    case S_SET_PHOTO:   ScrSetPhoto(dc); break;
    case S_INFO:        ScrInfo(dc); break;
    case S_WARNING:     ScrWarning(dc); break;
    case S_VOLUME:      ScrVolume(dc); break;
    case S_SAVER:       ScrSaver(dc); break;
    case S_EXIT:        ScrExit(dc); break;
    case S_REBOOT:      ScrReboot(dc); break;
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
    case S_SET_HOME:    ScrSetHome(dc); break;
    case S_SET_CAR:     ScrSetCar(dc); break;
    case S_TEST_CAN:    ScrTestCan(dc); break;
    case S_TEST_MCU:    ScrTestMcu(dc); break;
    case S_TEST_AV:     ScrTestAv(dc); break;
    case S_INFO_MORE:   ScrInfoMore(dc); break;
    case S_LICENSES:    ScrLicenses(dc); break;
    case S_SET_VOLUME:  ScrSetVolume(dc); break;
    case S_CAMDL:       ScrCamDl(dc); break;
    case S_CAMREG:      ScrCamReg(dc); break;
    case S_TASKS:       DrawTasks(dc); break;
    case S_NETRADIO:    ScrNetRadio(dc); break;
    case S_NETRADIO_LIST: ScrNetRadioList(dc); break;
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
    if (s_partial)
        s_zoneIdx = 0;
    else
        kBtnCount = 0;
    DrawScreen(dc, s_screen);
    if (Section(s_screen) >= 0)
        NavBar(dc, Section(s_screen));
}

static int s_drawnMinute = -1;   // the minute on the clock of the last frame
static DWORD s_drawCount, s_drawMs, s_drawMax;   // the frames since the last load line, their time
static DWORD s_areaCount, s_areaMs, s_areaShown;  // the partial ones: drawn, their time, how many went out

static void Redraw()
{
    if (s_drag)
        return;
    SYSTEMTIME t;
    GetLocalTime(&t);
    s_drawnMinute = t.wMinute;
    DWORD t0 = GetTickCount();
    HDC hdc = GetDC(s_wnd);
    HDC dc = CreateCompatibleDC(hdc);
    HGDIOBJ ob = SelectObject(dc, s_back);
    DrawFrame(dc);
    BitBlt(hdc, 0, 0, 800, 480, dc, 0, 0, SRCCOPY);   // the video's window is left out (WS_CLIPCHILDREN)
    SelectObject(dc, ob);
    DeleteDC(dc);
    ReleaseDC(s_wnd, hdc);
    VidSync();
    DWORD ms = GetTickCount() - t0;   // for the load line of the log (once a minute)
    s_drawCount++;
    s_drawMs += ms;
    if (ms > s_drawMax)
        s_drawMax = ms;
}

// Only what changed: the frame drawn again clipped to [l, t, r, b) (the layout and the buttons stay those of the
// last full frame, so it is for values that change in place: the speed, the track's position, a song title),
// and only if its pixels are not the same as before does the area go to the screen.
static void RedrawArea(int l, int t, int r, int b)
{
    if (!s_back || l >= r || t >= b || s_drag)
        return;
    DWORD t0 = GetTickCount();
    int w = r - l, h = b - t;
    unsigned short* was = (unsigned short*)malloc(w * h * 2);
    if (!was) {
        Redraw();
        return;
    }
    for (int y = t; y < b; y++)
        memcpy(was + (y - t) * w, s_backBits + (479 - y) * 800 + l, w * 2);
    s_partial = true;
    s_zoneIdx = 0;
    s_clip.left = l;
    s_clip.top = t;
    s_clip.right = r;
    s_clip.bottom = b;
    HDC hdc = GetDC(s_wnd);
    HDC dc = CreateCompatibleDC(hdc);
    HGDIOBJ ob = SelectObject(dc, s_back);
    IntersectClipRect(dc, l, t, r, b);
    DrawFrame(dc);
#ifndef UNDER_CE
    GdiFlush();
#endif
    bool changed = false;
    for (int y = t; y < b && !changed; y++)
        changed = memcmp(was + (y - t) * w, s_backBits + (479 - y) * 800 + l, w * 2) != 0;
    if (changed)
        BitBlt(hdc, l, t, w, h, dc, l, t, SRCCOPY);
    SelectObject(dc, ob);
    DeleteDC(dc);
    ReleaseDC(s_wnd, hdc);
    free(was);
    s_partial = false;
    s_clip.left = s_clip.top = 0;
    s_clip.right = 800;
    s_clip.bottom = 480;
    DWORD ms = GetTickCount() - t0;
    s_areaCount++;
    s_areaMs += ms;
    if (changed)
        s_areaShown++;
}

// Our own threads' share of the CPU since the last call, the 4 busiest: "<id> NN%" (2026-10-18: VestaHome showed
// 82-92 % with no frames drawn and minimized - which thread; the threads log their ids as they start).
static void OwnThreadsLoad(wchar_t* out, int n)
{
    out[0] = 0;
    struct Th { DWORD id; unsigned __int64 t; };
    static Th s_prev[48];
    static int s_prevCount;
    static DWORD s_prevTick;
    Th cur[48];
    int count = 0;
#ifdef UNDER_CE
    typedef HANDLE (WINAPI *SnapFn)(DWORD, DWORD);
    typedef BOOL (WINAPI *ThFn)(HANDLE, THREADENTRY32*);
    typedef BOOL (WINAPI *CloseFn)(HANDLE);
    typedef HANDLE (WINAPI *OpenFn)(DWORD, BOOL, DWORD);
    static HMODULE s_th;
    if (!s_th)
        s_th = LoadLibrary(L"toolhelp.dll");
    SnapFn snap = s_th ? (SnapFn)GetProcAddress(s_th, L"CreateToolhelp32Snapshot") : NULL;
    ThFn first = s_th ? (ThFn)GetProcAddress(s_th, L"Thread32First") : NULL;
    ThFn next = s_th ? (ThFn)GetProcAddress(s_th, L"Thread32Next") : NULL;
    CloseFn close = s_th ? (CloseFn)GetProcAddress(s_th, L"CloseToolhelp32Snapshot") : NULL;
    OpenFn open = (OpenFn)GetProcAddress(GetModuleHandle(L"coredll.dll"), L"OpenThread");
    if (!snap || !first || !next || !close) {
        wcscpy(out, L"нет toolhelp");
        return;
    }
    HANDLE h = snap(TH32CS_SNAPTHREAD, 0);
    if (h == INVALID_HANDLE_VALUE)
        return;
    THREADENTRY32 te;
    te.dwSize = sizeof(te);
    DWORD pid = GetCurrentProcessId();
    for (BOOL ok = first(h, &te); ok && count < 48; ok = next(h, &te)) {
        if (te.th32OwnerProcessID == pid) {
            HANDLE th = open ? open(THREAD_QUERY_INFORMATION, FALSE, te.th32ThreadID) : NULL;
            FILETIME c, e, k, u;
            if (GetThreadTimes(th ? th : (HANDLE)te.th32ThreadID, &c, &e, &k, &u)) {
                cur[count].id = te.th32ThreadID;
                cur[count].t = ((unsigned __int64)k.dwHighDateTime << 32 | k.dwLowDateTime) +
                               ((unsigned __int64)u.dwHighDateTime << 32 | u.dwLowDateTime);
                count++;
            }
            if (th)
                CloseHandle(th);
        }
        te.dwSize = sizeof(te);
    }
    close(h);
#else
    wcscpy(out, L"только на ГУ");
    return;
#endif
    DWORD now = GetTickCount(), wall = now - s_prevTick;
    int pct[48];
    for (int i = 0; i < count; i++) {
        pct[i] = -1;
        for (int j = 0; j < s_prevCount; j++)
            if (s_prev[j].id == cur[i].id && s_prevTick && wall)
                pct[i] = (int)((cur[i].t - s_prev[j].t) / 10000 * 100 / wall);   // 100 ns -> ms -> %
    }
    for (int k = 0; k < 4; k++) {   // the 4 busiest
        int best = -1;
        for (int i = 0; i < count; i++)
            if (pct[i] >= 0 && (best < 0 || pct[i] > pct[best]))
                best = i;
        if (best < 0)
            break;
        wchar_t s[32];
        _snwprintf(s, 32, L"%s%08X %d%%", k ? L", " : L"", (unsigned)cur[best].id, pct[best]);
        s[31] = 0;
        if ((int)(wcslen(out) + wcslen(s)) < n - 1)
            wcscat(out, s);
        pct[best] = -1;
    }
    if (!out[0])
        _snwprintf(out, n, L"%d, замер с этой минуты", count);
    memcpy(s_prev, cur, count * sizeof(Th));
    s_prevCount = count;
    s_prevTick = now;
}

// Once a minute into the log: the CPU over that minute (the whole and the busiest processes, VestaCore vtask)
// and what our frames cost - to find what loads the unit.
static void LogLoad(HWND hwnd)
{
    if (s_screen != S_TASKS)   // (its screen samples every 2 s itself)
        TaskSample(&s_task, hwnd);
    int idx[4] = { -1, -1, -1, -1 };
    for (int i = 0; i < s_task.procCount; i++) {
        int c = s_task.proc[i].cpu10;
        for (int k = 0; k < 4; k++)
            if (idx[k] < 0 || c > s_task.proc[idx[k]].cpu10) {
                for (int m = 3; m > k; m--)
                    idx[m] = idx[m - 1];
                idx[k] = i;
                break;
            }
    }
    wchar_t top[200] = L"";
    for (int k = 0; k < 4 && idx[k] >= 0 && s_task.proc[idx[k]].cpu10 > 0; k++) {
        wchar_t s[64];
        _snwprintf(s, 64, L"%s%s %d%%", k ? L", " : L"", s_task.proc[idx[k]].name, (s_task.proc[idx[k]].cpu10 + 5) / 10);
        s[63] = 0;
        if (wcslen(top) + wcslen(s) < 199)
            wcscat(top, s);
    }
    wchar_t thr[160];
    OwnThreadsLoad(thr, 160);
    VLog(L"нагрузка: ЦП %d%%; %s; кадров %u, в среднем %u мс, макс %u мс; частичных %u (%u на экран), в среднем %u мс; "
         L"потоки VH: %s",
         s_task.cpu10 >= 0 ? (s_task.cpu10 + 5) / 10 : -1, top[0] ? top : L"процессы не видны", (unsigned)s_drawCount,
         (unsigned)(s_drawCount ? s_drawMs / s_drawCount : 0), (unsigned)s_drawMax, (unsigned)s_areaCount,
         (unsigned)s_areaShown, (unsigned)(s_areaCount ? s_areaMs / s_areaCount : 0), thr);
    s_drawCount = s_drawMs = s_drawMax = s_areaCount = s_areaMs = s_areaShown = 0;
}

// The band that slides on a page turn: under the top bar, above the bottom bar and above the page dots where
// they are (they stay in place, the new page's show when it has turned); on the radio only the presets.
static int SlideTop()   // under what stays: the radio's frequency panel, the media tabs
{
    return s_screen == S_RADIO ? 258 : s_screen == S_PHOTO || s_screen == S_VIDEO ? 116 : 56;
}

static int SlideLeft()   // right of what stays: the settings' tiles
{
    return s_screen == S_BT_PAIRED ? 244 : 0;
}

static int SlideBottom()
{
    if (s_screen == S_AUDIO_LIST || s_screen == S_FILES)
        return 378;   // over their dots and dock
    if (s_screen == S_BT_PAIRED)
        return 382;
    if (Section(s_screen) < 0)
        return 480;
    return s_screen == S_HOME || s_screen == S_RADIO || s_screen == S_APPS || s_screen == S_PHOTO ||
           s_screen == S_VIDEO ? 392 : 408;   // the dots stay
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
    int top = SlideTop(), bottom = SlideBottom(), h = bottom - top, l = SlideLeft(), sw = 800 - l;
    BitBlt(hdc, 0, 0, 800, top, dc, 0, 0, SRCCOPY);
    BitBlt(hdc, 0, bottom, 800, 480 - bottom, dc, 0, bottom, SRCCOPY);
    BitBlt(hdc, 0, top, l, h, dc, 0, top, SRCCOPY);
    const DWORD kMs = 220;
    DWORD t0 = GetTickCount();
    for (;;) {
        DWORD el = GetTickCount() - t0;
        double p = el >= kMs ? 1.0 : (double)el / kMs;
        p = 1 - (1 - p) * (1 - p) * (1 - p);   // ease-out
        int off = (int)(sw * p + 0.5);
        if (dir > 0) {
            BitBlt(hdc, l, top, sw - off, h, odc, l + off, top, SRCCOPY);
            BitBlt(hdc, 800 - off, top, off, h, dc, l, top, SRCCOPY);
        } else {
            BitBlt(hdc, l + off, top, sw - off, h, odc, l, top, SRCCOPY);
            BitBlt(hdc, l, top, off, h, dc, 800 - off, top, SRCCOPY);
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
    VidSync();
}

static int s_slideDir;   // set by an action that turned a page: the next frame slides in

// ---- a page dragged with the finger: once the finger has moved sideways, the page goes with it and its
// neighbour comes in beside it; let go far enough it turns (the rest slides on), otherwise it slides back. The
// current frame is copied aside (768 KB, only while dragging), the neighbour is drawn into s_back.
static bool Swipe(int dir);
static void BuildButtons();
static int  s_phPage;   // the photos' page (the photos' part is below)
static int  PhotoPages();
static void PhotoStep(int dir);
static int     s_dragNb, s_dragSide, s_dragX;   // the neighbour in s_back (1 next, -1 previous, 0 none: an end)
static HBITMAP s_dragOld;
static unsigned short* s_dragOldBits;

static bool CanSwipe()
{
    return s_screen == S_HOME || s_screen == S_APPS || s_screen == S_FILES || s_screen == S_RADIO ||
           s_screen == S_PHOTO || s_screen == S_AUDIO_LIST || s_screen == S_VIDEO || s_screen == S_BT_PAIRED;
}

// The page beside (dir) drawn into s_back without turning to it (no side effects: the radio's band is not
// switched); false at an end that does not go round.
static bool DrawNeighbour(int dir)
{
    int hp = s_homePage, rp = s_radioPage, ap = s_page, fp = s_filePage, pp = s_phPage, mp = s_musPage,
        vp = s_vidPage, bp = s_pairedPage;
    bool ok = true;
    if (s_screen == S_BT_PAIRED) {
        ok = PairedPages() > 1;
        s_pairedPage = (bp + dir + PairedPages()) % PairedPages();
    } else if (s_screen == S_PHOTO) {
        ok = pp + dir >= 0 && pp + dir < PhotoPages();
        s_phPage += dir;
    } else if (s_screen == S_AUDIO_LIST) {
        ok = mp + dir >= 0 && mp + dir < MusPages();
        s_musPage += dir;
    } else if (s_screen == S_VIDEO) {
        ok = vp + dir >= 0 && vp + dir < VidPages();
        s_vidPage += dir;
    } else if (s_screen == S_HOME) {
        int n = HomePages();
        ok = n > 1;
        s_homePage = (hp + dir + n) % n;
    } else if (s_screen == S_RADIO) {
        s_radioPage = (rp + dir + kRadioPages) % kRadioPages;
    } else if (s_screen == S_APPS) {
        ok = ap + dir >= 0 && ap + dir < AppsPages();
        s_page += dir;
    } else {
        ok = fp + dir >= 0 && fp + dir < FilePages();
        s_filePage += dir;
    }
    if (ok) {
        HDC hdc = GetDC(s_wnd);
        HDC dc = CreateCompatibleDC(hdc);
        HGDIOBJ ob = SelectObject(dc, s_back);
        DrawFrame(dc);
        SelectObject(dc, ob);
        DeleteDC(dc);
        ReleaseDC(s_wnd, hdc);
    }
    s_homePage = hp;
    s_radioPage = rp;
    s_page = ap;
    s_filePage = fp;
    s_phPage = pp;
    s_musPage = mp;
    s_vidPage = vp;
    s_pairedPage = bp;
    return ok;
}

// The current page at x in the band between the bars, its neighbour beside it (or the background at an end).
static void DragShow(int x)
{
    HDC hdc = GetDC(s_wnd);
    HDC dc = CreateCompatibleDC(hdc), odc = CreateCompatibleDC(hdc);
    HGDIOBJ ob = SelectObject(dc, s_back), oo = SelectObject(odc, s_dragOld);
    int top = SlideTop(), bottom = SlideBottom(), h = bottom - top, l = SlideLeft(), w = 800 - l;
    if (x > 0)
        BitBlt(hdc, l + x, top, w - x, h, odc, l, top, SRCCOPY);
    else
        BitBlt(hdc, l, top, w + x, h, odc, l - x, top, SRCCOPY);
    if (s_dragNb && x > 0)
        BitBlt(hdc, l, top, x, h, dc, 800 - x, top, SRCCOPY);
    else if (s_dragNb && x < 0)
        BitBlt(hdc, 800 + x, top, -x, h, dc, l, top, SRCCOPY);
    else if (x) {
        RECT rc = { x > 0 ? l : 800 + x, top, x > 0 ? l + x : 800, bottom };
        HBRUSH b = CreateSolidBrush(C(kInk));
        FillRect(hdc, &rc, b);
        DeleteObject(b);
    }
    SelectObject(odc, oo);
    SelectObject(dc, ob);
    DeleteDC(odc);
    DeleteDC(dc);
    ReleaseDC(s_wnd, hdc);
}

static void DragStart()
{
    KillTimer(s_wnd, TIMER_LONG);
    s_pressed = -1;
    Redraw();   // the page without the pressed look
    s_dragOld = MakeDib(800, 480, &s_dragOldBits);
    if (!s_dragOld || !s_dragOldBits) {   // no memory: the swipe as before (on letting go)
        if (s_dragOld)
            DeleteObject(s_dragOld);
        s_dragOld = NULL;
        return;
    }
    memcpy(s_dragOldBits, s_backBits, 800 * 480 * 2);
    s_drag = true;
    s_dragNb = s_dragSide = s_dragX = 0;
}

static void DragMove(int dx)
{
    int side = dx < 0 ? 1 : -1;
    if (side != s_dragSide) {
        s_dragSide = side;
        s_dragNb = DrawNeighbour(side) ? side : 0;
    }
    s_dragX = s_dragNb ? dx : dx / 3;   // at an end: held back
    DragShow(s_dragX);
}

// Let go: the page turns (dx past 100 px towards a neighbour) or slides back.
static void DragEnd(int dx)
{
    bool turn = s_dragNb && s_dragNb == (dx < 0 ? 1 : -1) && (dx > 100 || dx < -100);
    int w = 800 - SlideLeft(), from = s_dragX, to = turn ? (s_dragNb > 0 ? -w : w) : 0;
    const DWORD kMs = 160;
    DWORD t0 = GetTickCount();
    for (;;) {
        DWORD el = GetTickCount() - t0;
        double p = el >= kMs ? 1.0 : (double)el / kMs;
        p = 1 - (1 - p) * (1 - p) * (1 - p);   // ease-out
        DragShow(from + (int)((to - from) * p));
        if (el >= kMs)
            break;
        Sleep(10);
    }
    s_drag = false;
    DeleteObject(s_dragOld);
    s_dragOld = NULL;
    if (turn)
        Swipe(s_dragNb);   // the page for real (the radio: its band too)
    BuildButtons();
    Redraw();
}

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
    if (s == S_TASKS)
        TasksOpen();
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
static void RadioOn();

// ---- the screen and the source come back after a restart (the unit off and on): the screen as it was (its
// section's main one: a settings page -> Settings), and what played - the radio (the MCU's source), our music
// (the last track), Bluetooth (the phone: when it connects, its music becomes the source and plays). Remembered
// in the ini at most once a minute (CfgSoon), so going round the screens doesn't wear the flash.
static const struct { Screen s; const wchar_t* key; } kUiScreens[] = {
    { S_HOME, L"home" }, { S_RADIO, L"radio" }, { S_AUDIO, L"music" }, { S_NETRADIO, L"netradio" },
    { S_AUDIO_BT, L"bt" }, { S_AUX, L"aux" }, { S_VIDEO, L"video" }, { S_PHOTO, L"photo" }, { S_PHONE, L"phone" },
    { S_APPS, L"apps" }, { S_SETTINGS, L"settings" } };
static DWORD s_bootTick;
static bool  s_resumed;   // the source has been brought back (or given up)

static const wchar_t* UiScreenKey()
{
    Screen root = s_histLen ? s_hist[0] : s_screen;
    if (Section(root) == 5)
        root = S_SETTINGS;
    for (int i = 0; i < (int)(sizeof(kUiScreens) / sizeof(kUiScreens[0])); i++)
        if (kUiScreens[i].s == root)
            return kUiScreens[i].key;
    return NULL;   // not a main screen (a player, a call): the last main one stays
}

// What plays now: "music" (ours), "radio", "bt", "" (another source: the shell's); NULL - not known yet.
static const wchar_t* UiSourceKey()
{
    if (PlayerPlaying())
        return L"music";
    int vol, mute, src;
    if (!VolGet(&vol, &mute, &src) || src < 0)
        return NULL;
    return src == 0 ? L"radio" : src == 9 ? L"bt" : L"";
}

static void ResumeScreen()   // at the start
{
    for (int i = 0; i < (int)(sizeof(kUiScreens) / sizeof(kUiScreens[0])); i++)
        if (!wcscmp(kUiScreens[i].key, s_cfg.uiScreen)) {
            s_screen = kUiScreens[i].s;
            s_screenTick = GetTickCount();
            if (Section(s_screen) == 2)
                s_media = s_screen;
            VLog(L"запуск: экран «%s»", s_cfg.uiScreen);
        }
}

// The volume at the start: no louder than 10 (the user, 2026-10-19: the shell's scale 0..30, comfortable 5..15).
// The shell's volume window sets its "default" volume with SPApi_SendRadioAudioVolData(0, v) (CAudioVolumeDlg::
// OnTimerDefaultSlot, Ghidra 2026-10-19; beside it UpdateStartedVol - the volume at power-on); whether it moves
// the volume playing now or only the next start's is to be seen on the unit: the volume the MCU reports after it
// goes to the log. Once, 10 s after the start (the MCU's link up); then again at 15 s only to log the result.
enum { kStartVolMax = 10 };

static void StartVolumeTick()
{
    static int s_step;   // 0 waiting, 1 sent, 2 done
    DWORD el = GetTickCount() - s_bootTick;
    int vol, mute, src;
    if (s_step == 0 && el >= 10000) {
        s_step = 2;
        if (VolGet(&vol, &mute, &src) && vol > kStartVolMax) {
            McuSend3("SPApi_SendRadioAudioVolData", 0, kStartVolMax, 0);
            VLog(L"запуск: громкость %d > %d - SendRadioAudioVolData(0, %d)", vol, kStartVolMax, kStartVolMax);
            s_step = 1;
        }
    } else if (s_step == 1 && el >= 15000) {
        s_step = 2;
        bool have = VolGet(&vol, &mute, &src);
        VLog(L"запуск: громкость после команды - %d (MCU %s)", have ? vol : -1,
             have && vol <= kStartVolMax ? L"убавил сейчас" : L"сейчас не убавил: возможно, только при следующем включении");
    }
}

// Once a second from the start: 10 s on (the MCU's link up) the source comes back. Bluetooth: the stack itself
// connects the last phone at power-on when the shell allows it; not connected after 15 s (and again at 45 s) we
// connect the one remembered (bt.last) as the shell does (Hf_Connect + Av_Connect, the proxy's BT_CMD_CONNECT),
// the phone only (no music link) - its music link (Av_Connect); then its music becomes the source (BtMusicOn) and
// plays (Av_Mute(0) + Av_Play, as the shell's play). Given up after 2 min.
static void ResumeSourceTick()
{
    DWORD el = GetTickCount() - s_bootTick;
    if (s_resumed || el < 10000)
        return;
    int vol, mute, src;
    VolGet(&vol, &mute, &src);
    if (!wcscmp(s_cfg.uiSource, L"radio")) {
        VLog(L"запуск: радио (источник MCU %d)", src);
        RadioOn();
    } else if (!wcscmp(s_cfg.uiSource, L"music")) {
        VLog(L"запуск: музыка «%s»", s_cfg.musicFile);
        if (!s_chan && s_cfg.musicDir[0])
            PlayerLoadDir(s_cfg.musicDir, s_cfg.musicFile, s_cfg.musicTree);
    } else if (!wcscmp(s_cfg.uiSource, L"bt")) {
        static DWORD s_srcAsked;
        static int s_tries;
        BtState bt;
        bool proxy = BtGet(&bt, NULL);
        bool phone = proxy && bt.av >= 1;   // A2DP connected
        if (!phone) {
            if (proxy && ((s_tries == 0 && el >= 15000) || (s_tries == 1 && el >= 45000))) {
                s_tries++;
                if (bt.hf >= 1) {   // the phone is there, its music not
                    VLog(L"запуск: Bluetooth - телефон есть, подключаю музыку (A2DP)");
                    BtCmd(BT_CMD_AVCONNECT);
                } else if (s_cfg.btLast[0]) {
                    VLog(L"запуск: Bluetooth - подключаю телефон %s", s_cfg.btLast);
                    BtCmdText(BT_CMD_CONNECT, s_cfg.btLast);
                } else {
                    VLog(L"запуск: Bluetooth - последний телефон не известен, жду подключения");
                }
            }
            if (el < 120000)
                return;
            VLog(L"запуск: Bluetooth - телефон не подключился за 2 мин");
        } else if (src != 9 && !s_srcAsked) {
            VLog(L"запуск: Bluetooth - телефон подключён, источник BT");
            BtMusicOn();
            s_srcAsked = GetTickCount();
            return;
        } else if (src != 9 && GetTickCount() - s_srcAsked < 4000) {
            return;   // the source still switching
        } else {
            VLog(L"запуск: Bluetooth - играть (источник MCU %d)", src);
            BtCmd(BT_CMD_PLAYON);
        }
    }
    s_resumed = true;
}

// Once a second: the screen and the source to remember (after the start's resume, not before it), and the phone
// connected (bt.last).
static void UiRememberTick()
{
    BtState bt;
    if (BtGet(&bt, NULL) && bt.hf >= 1 && bt.devAddr[0]) {
        wchar_t a[18];
        BtText(bt.devAddr, a, 18);
        if (wcscmp(a, s_cfg.btLast)) {
            wcscpy(s_cfg.btLast, a);
            VLog(L"запомнить: телефон %s", a);
            CfgSoon();
        }
    }
    if (!s_resumed)
        return;
    const wchar_t* sc = UiScreenKey();
    const wchar_t* so = UiSourceKey();
    if ((sc && wcscmp(sc, s_cfg.uiScreen)) || (so && wcscmp(so, s_cfg.uiSource))) {
        if (sc)
            wcscpy(s_cfg.uiScreen, sc);
        if (so)
            wcscpy(s_cfg.uiSource, so);
        VLog(L"запомнить: экран «%s», источник «%s»", s_cfg.uiScreen, s_cfg.uiSource);
        CfgSoon();
    }
}

static void Nav(Screen s)
{
    if (s == S_RADIO)
        RadioOn();
    s_histLen = 0;
    s_status[0] = 0;
    s_editMode = false;
    s_screen = s;
    s_screenTick = GetTickCount();
    if (Section(s) == 2)
        s_media = s;
}

// A swipe turns the page of the main screen, the radio, the programs or the file browser; the main screen and
// the radio go round (past the last page the first), the others stop: false at the first / last page.
static bool Swipe(int dir)
{
    int was = s_homePage + s_page * 100 + s_filePage * 10000;
    if (s_screen == S_HOME) {
        int n = HomePages();
        s_homePage = (s_homePage + dir + n) % n;
    } else if (s_screen == S_APPS) {
        s_page += dir;
        s_page = s_page < 0 ? 0 : s_page >= AppsPages() ? AppsPages() - 1 : s_page;
    } else if (s_screen == S_RADIO) {
        RadioPageTo(s_radioPage + dir);   // (round)
        return true;
    } else if (s_screen == S_PHOTO) {
        int to = s_phPage + dir;
        if (to < 0 || to >= PhotoPages())
            return false;
        s_phPage = to;
        return true;
    } else if (s_screen == S_BT_PAIRED) {
        if (PairedPages() < 2)
            return false;
        s_pairedPage = (s_pairedPage + dir + PairedPages()) % PairedPages();
        return true;
    } else if (s_screen == S_AUDIO_LIST || s_screen == S_VIDEO) {
        int& pg = s_screen == S_VIDEO ? s_vidPage : s_musPage;
        int to = pg + dir;
        if (to < 0 || to >= (s_screen == S_VIDEO ? VidPages() : MusPages()))
            return false;
        pg = to;
        return true;
    } else if (s_screen == S_PHOTO_VIEW) {
        PhotoStep(dir);
        return true;
    } else if (s_screen == S_FILES) {
        s_filePage += dir;
        s_filePage = s_filePage < 0 ? 0 : s_filePage >= FilePages() ? FilePages() - 1 : s_filePage;
    }
    return s_homePage + s_page * 100 + s_filePage * 10000 != was;
}

// Day / night, two things set apart: the theme - VestaHome's colours a third darker, OSMapsCE's night map (VestaCore
// vnight) - and the unit's backlight (the shell's key: the MCU switches it and reports 0x4105, its own brightness
// for each).
static void SetTheme(bool night)
{
    s_night = night;
    NightShare(night);
}

static void SetLight(bool night)
{
    if (s_mcuNight != (night ? 1 : 0))
        McuKey(night ? 0x65 : 0x64, false);
    s_mcuNight = night ? 1 : 0;   // at once in the settings; the MCU's 0x4105 confirms
}

static DWORD s_lastTouch;   // the last tap: the screen saver counts from it

// Sunrise or sunset for the day of the year, minutes after local midnight; -1: the sun stays down, -2: up.
// The almanac's algorithm (zenith 90.833 deg), good to a couple of minutes.
static int SunEvent(int doy, double lat, double lon, bool rise)
{
    const double k = 3.14159265358979 / 180;
    double lh = lon / 15, t = doy + ((rise ? 6 : 18) - lh) / 24;
    double m = 0.9856 * t - 3.289;
    double l = fmod(m + 1.916 * sin(m * k) + 0.020 * sin(2 * m * k) + 282.634 + 720, 360);
    double ra = fmod(atan(0.91764 * tan(l * k)) / k + 720, 360);
    ra = (ra + floor(l / 90) * 90 - floor(ra / 90) * 90) / 15;
    double sd = 0.39782 * sin(l * k), cd = cos(asin(sd));
    double ch = (cos(90.833 * k) - sd * sin(lat * k)) / (cd * cos(lat * k));
    if (ch > 1)
        return -1;
    if (ch < -1)
        return -2;
    double h = (rise ? 360 - acos(ch) / k : acos(ch) / k) / 15;
    double ut = h + ra - 0.06571 * t - 6.622 - lh;
    return ((int)floor(ut * 60 + 0.5) + s_cfg.tzMin + 2880) % 1440;
}

// The automatic mode's answer for now: night between sunset and sunrise (the place: the GPS's last fix, else home,
// else 7:00-20:00). rise / set for the settings' line.
static bool SunNight(int* rise, int* set)
{
    Gps g;
    GpsGet(&g);
    SYSTEMTIME t;
    GetLocalTime(&t);
    static const int kDays0[12] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
    int doy = kDays0[(t.wMonth + 11) % 12] + t.wDay + (t.wMonth > 2 && t.wYear % 4 == 0 ? 1 : 0);
    int now = t.wHour * 60 + t.wMinute;
    bool gps = g.fixTick && (g.lat || g.lon), home = s_cfg.homeLat || s_cfg.homeLon;
    int a = 7 * 60, b = 20 * 60;
    if (gps || home) {
        double lat = gps ? g.lat : s_cfg.homeLat, lon = gps ? g.lon : s_cfg.homeLon;
        a = SunEvent(doy, lat, lon, true);
        b = SunEvent(doy, lat, lon, false);
    }
    *rise = a;
    *set = b;
    if (a == -1 || b == -1)
        return true;    // polar night
    if (a == -2 || b == -2)
        return false;   // polar day
    return a < b ? now < a || now >= b : now >= b && now < a;
}

// Applies the theme's and the backlight's modes. "By the sun" switches when the sun's answer changes (a switch by
// hand - the bar, the shell, OSMapsCE - holds until the next sunrise / sunset); force: apply all now (the start,
// a choice in the settings).
static void NightAuto(bool force)
{
    static int s_last = -1;
    int a, b;
    int n = SunNight(&a, &b) ? 1 : 0;
    bool edge = n != s_last || force;
    if (n != s_last)
        VLog(L"экран: по солнцу %s", n ? L"ночь" : L"день");
    s_last = n;
    if (s_cfg.themeMode == 0 ? edge : force)
        SetTheme(s_cfg.themeMode == 0 ? n != 0 : s_cfg.themeMode == 2);
    if (s_cfg.lightMode == 0 ? edge : force)
        SetLight(s_cfg.lightMode == 0 ? n != 0 : s_cfg.lightMode == 2);
}

// Settings -> Screen: the theme's and the backlight's day / night, the screen saver, the touch calibration. (No
// brightness: the display takes only the day and the night levels - HU 2026-10-20, the user.)
static void ScrSetScreen(HDC dc)
{
    static const wchar_t* saver[5] = { L"ВЫКЛ", L"1", L"3", L"5", L"10" };
    Settings(dc, S_SET_SCREEN);
    wchar_t sub[48];
    int a, b;
    SunNight(&a, &b);
    if (a >= 0 && b >= 0)   // "auto": the sun's times
        _snwprintf(sub, 48, L"восход %d:%02d · закат %d:%02d", a / 60, a % 60, b / 60, b % 60);
    else
        wcscpy(sub, a == -1 ? L"полярная ночь" : L"полярный день");
    int light = s_mcuNight;
    for (int row = 0; row < 2; row++) {   // the theme, the backlight: АВТО / ДЕНЬ / НОЧЬ
        int y = 64 + row * 56, mode = row ? s_cfg.lightMode : s_cfg.themeMode;
        Action act = row ? A_LIGHT : A_THEME;
        SetRow(dc, y, row ? L"Подсветка" : L"Тема",
               row ? (light < 0 ? L"нет данных от MCU" : light ? L"сейчас ночная" : L"сейчас дневная") : sub);
        Button(dc, 456, y + 6, 96, 44, L"АВТО", mode == 0 ? B_SEL : B_N, IC_NONE, 15, 0, act, 0);
        Button(dc, 560, y + 6, 96, 44, L"ДЕНЬ", mode == 1 ? B_SEL : B_N, IC_SUN, 15, 22, act, 1);
        Button(dc, 664, y + 6, 104, 44, L"НОЧЬ", mode == 2 ? B_SEL : B_N, IC_MOON, 15, 22, act, 2);
    }
    SetRow(dc, 176, L"Заставка, мин", L"часы и дата без касаний");
    for (int i = 0; i < 5; i++)
        if (kSaverMins[i] == s_cfg.saverMin)
            s_seg[G_SAVER] = i;
    Seg(dc, 456, 182, 312, 44, saver, 5, G_SAVER, 14);
    SetRow(dc, 232, L"Калибровка экрана", L"системные перекрестья", true, A_CALIBRATE);
    Chevron(dc, 736, 244);
}

// ---- photos (Media -> Photo): the volumes, folders and pictures as tiles with thumbnails (4 x 2 a page, a swipe
// turns it), the viewer between the top bar and the dock: previous / next (and a swipe), a quarter turn either
// way, the slide show. The pictures are decoded by photo.cpp in a thread of their own (a photo takes a second or
// two on the unit): the viewed one first, then the thumbnails of the page shown; a result posts WM_APP_PHOTO.
#define WM_APP_PHOTO (WM_APP + 25)
#define WM_APP_OBD (WM_APP + 27)
enum { TIMER_PHOTO = 13, kPhMax = 1000, kPhTiles = 8, kThW = 172, kThH = 92, kPhBoxW = 800, kPhBoxH = 342 };
static wchar_t  s_phDir[MAX_PATH];   // "" = the volumes
static wchar_t* s_phs[kPhMax];       // folders ("\" at the end), then pictures
static int      s_phCount, s_phCur = -1;   // s_phCur: the picture viewed (index in s_phs)
static bool     s_phRead, s_phShow;  // s_phShow: the slide show runs
static int      s_phRot;             // the viewer's own quarter turns
struct PhThumb { wchar_t path[MAX_PATH]; Pic pic; int state; };   // state: 0 wanted, 1 working, 2 done, 3 failed
static PhThumb  s_th[kPhTiles];
static wchar_t  s_pvPath[MAX_PATH];  // the viewer's request
static int      s_pvRot, s_pvState, s_pvErr, s_pvW, s_pvH;   // state: 0 none, 1 wanted, 2 working, 3 done
static bool     s_pvFill, s_pvAuto;
static Pic      s_pv;
static CRITICAL_SECTION s_phCs;
static HANDLE   s_phEv, s_phThread;

static DWORD WINAPI PhotoThread(LPVOID)
{
    for (;;) {
        WaitForSingleObject(s_phEv, INFINITE);
        for (;;) {
            wchar_t path[MAX_PATH];
            int slot = -1, rot = 0;
            bool view = false, fill = false, exif = false;
            EnterCriticalSection(&s_phCs);
            if (s_pvState == 1) {   // the viewed picture first
                view = true;
                wcscpy(path, s_pvPath);
                rot = s_pvRot;
                fill = s_pvFill;
                exif = s_pvAuto;
                s_pvState = 2;
            } else {
                for (int i = 0; i < kPhTiles && slot < 0; i++)
                    if (s_th[i].path[0] && s_th[i].state == 0) {
                        slot = i;
                        wcscpy(path, s_th[i].path);
                        s_th[i].state = 1;
                        exif = s_cfg.photoAutoRot;
                    }
            }
            LeaveCriticalSection(&s_phCs);
            if (!view && slot < 0)
                break;
            Pic pic = { 0, 0, NULL };
            int w = 0, h = 0;
            int res = view ? PicLoad(path, kPhBoxW, kPhBoxH, fill, rot, exif, &pic, &w, &h)
                           : PicThumb(path, kThW, kThH, exif, &pic);
            EnterCriticalSection(&s_phCs);
            if (view && s_pvState == 2 && !wcscmp(path, s_pvPath) && rot == s_pvRot && fill == s_pvFill) {
                PicFree(&s_pv);
                s_pv = pic;
                s_pvErr = res;
                s_pvW = w;
                s_pvH = h;
                s_pvState = 3;
                pic.px = NULL;
            } else if (!view && s_th[slot].state == 1 && !wcscmp(path, s_th[slot].path)) {
                s_th[slot].pic = pic;
                s_th[slot].state = res == PIC_OK ? 2 : 3;
                pic.px = NULL;
            }
            LeaveCriticalSection(&s_phCs);
            PicFree(&pic);   // (a request changed meanwhile: not wanted any more)
            PostMessage(s_wnd, WM_APP_PHOTO, 0, 0);
        }
    }
    return 0;
}

static void PhotoWake()
{
    if (!s_phThread) {
        InitializeCriticalSection(&s_phCs);
        s_phEv = CreateEvent(NULL, FALSE, FALSE, NULL);
        s_phThread = CreateThread(NULL, 0, PhotoThread, NULL, 0, NULL);
        VLog(L"поток фото: %s", s_phThread ? L"запущен" : L"не запустился");
    }
    if (s_phEv)
        SetEvent(s_phEv);
}

static bool PhIsDir(int i)
{
    return i >= 0 && i < s_phCount && s_phs[i][wcslen(s_phs[i]) - 1] == L'\\';
}

static void PhotoOpenDir(const wchar_t* dir)
{
    FreeNames(s_phs, &s_phCount);
    wcsncpy(s_phDir, dir, MAX_PATH - 1);
    s_phDir[MAX_PATH - 1] = 0;
    s_phPage = 0;
    s_phCur = -1;
    s_phRead = true;
    s_phCount = s_phDir[0] ? ReadMusicDir(s_phDir, s_phs, kPhMax, true, 2) : ReadVolumes(s_phs);
    if (s_phDir[0] && wcscmp(s_cfg.photoDir, s_phDir)) {
        wcscpy(s_cfg.photoDir, s_phDir);
        CfgSoon();
    }
}

static int PhotoTiles()   // the tiles of the folder: "up" first in a folder
{
    return s_phCount + (s_phDir[0] ? 1 : 0);
}

static int PhotoPages()
{
    int n = PhotoTiles();
    return n > 0 ? (n + kPhTiles - 1) / kPhTiles : 1;
}

// The picture into the frame at x, y (cut at the frame and at the partial redraw's area).
static void BlitPic(const Pic& p, int x, int y)
{
    if (!p.px)
        return;
#ifndef UNDER_CE
    GdiFlush();
#endif
    for (int r = 0; r < p.h; r++) {
        int fy = y + r;
        if (fy < s_clip.top || fy >= s_clip.bottom)
            continue;
        int x0 = x < s_clip.left ? s_clip.left : x, x1 = x + p.w > s_clip.right ? s_clip.right : x + p.w;
        if (x1 > x0)
            memcpy(s_backBits + (479 - fy) * 800 + x0, p.px + r * p.w + (x0 - x), (x1 - x0) * 2);
    }
}

// The page's thumbnails asked for (the worker makes them); a tile no longer shown lets go of its picture.
static void PhotoWantThumbs()
{
    if (!s_phThread)
        PhotoWake();
    bool any = false;
    EnterCriticalSection(&s_phCs);
    for (int t = 0; t < kPhTiles; t++) {
        int k = s_phPage * kPhTiles + t - (s_phDir[0] ? 1 : 0);
        wchar_t path[MAX_PATH] = L"";
        if (k >= 0 && k < s_phCount && !PhIsDir(k))
            _snwprintf(path, MAX_PATH, L"%s%s", s_phDir, s_phs[k]);
        if (wcscmp(path, s_th[t].path)) {
            PicFree(&s_th[t].pic);
            wcscpy(s_th[t].path, path);
            s_th[t].state = 0;
            any = any || path[0];
        }
    }
    LeaveCriticalSection(&s_phCs);
    if (any)
        PhotoWake();
}

static void ScrPhoto(HDC dc)
{
    if (!s_phRead)
        PhotoOpenDir(s_cfg.photoDir[0] && GetFileAttributes(s_cfg.photoDir) != 0xFFFFFFFF ? s_cfg.photoDir : L"");
    if (s_phPage >= PhotoPages())
        s_phPage = PhotoPages() - 1;
    wchar_t s[MAX_PATH + 16], folder[MAX_PATH];
    wcscpy(folder, s_phDir);
    int fl = (int)wcslen(folder);
    if (fl && folder[fl - 1] == L'\\')
        folder[--fl] = 0;
    const wchar_t* fn = wcsrchr(folder, L'\\');
    _snwprintf(s, MAX_PATH + 16, L"МЕДИА · ФОТО · %s", !s_phDir[0] ? L"НОСИТЕЛИ" : fn && fn[1] ? fn + 1 : folder);
    Frame(dc, s);
    MediaTabs(dc, S_PHOTO, IC_PLAY, A_PHPLAY, 0, S_SET_PHOTO);
    PhotoWantThumbs();
    int up = s_phDir[0] ? 1 : 0;
    for (int t = 0; t < kPhTiles; t++) {
        int k = s_phPage * kPhTiles + t - up, x = 16 + (t % 4) * 194, y = 124 + (t / 4) * 138;
        if (k >= s_phCount)
            break;
        bool down = Zone(x, y, 184, 128, A_PHFILE, k);   // k -1: up
        bool dir = k < 0 || PhIsDir(k);
        bool cur = !dir && k == s_phCur;
        R(dc, x, y, 184, 128, 12, down || cur ? kOrange : kPanel, down || cur ? CLR_INVALID : kLine);
        wchar_t name[MAX_PATH];
        if (k < 0)
            wcscpy(name, L"НАВЕРХ");
        else {
            wcscpy(name, s_phs[k]);
            if (dir && name[0] && name[wcslen(name) - 1] == L'\\')
                name[wcslen(name) - 1] = 0;
        }
        if (dir) {
            Ico(dc, k < 0 ? IC_UP : IC_FOLDER, x + 64, y + 22, 56, down ? kInk : k < 0 ? kMuted : kOrange);
        } else {
            R(dc, x + 6, y + 6, kThW, kThH, 0, kMargin);
            EnterCriticalSection(&s_phCs);
            if (s_th[t].state == 2)
                BlitPic(s_th[t].pic, x + 6, y + 6);
            else
                Ico(dc, IC_PHOTO, x + 6 + kThW / 2 - 22, y + 6 + kThH / 2 - 22, 44, s_th[t].state == 3 ? kOff : kDim);
            LeaveCriticalSection(&s_phCs);
        }
        T(dc, x + 92, y + 118, name, 14, down || cur ? kInk : kMuted, 700, AL_MID, 172);
    }
    if (!s_phCount)
        T(dc, 400, 250, s_phDir[0] ? L"Здесь нет папок и фото (JPG, PNG, BMP)" : L"Нет носителей", 18, kMuted, 400,
          AL_MID);
    PageDots(dc, s_phPage, PhotoPages());
}

// The viewer: picture i of the folder asked for (its own turn reset), the slide show's timer as set.
static void PhotoView(int i)
{
    if (i < 0 || i >= s_phCount || PhIsDir(i))
        return;
    if (i != s_phCur)
        s_phRot = 0;
    s_phCur = i;
    s_phPage = (i + (s_phDir[0] ? 1 : 0)) / kPhTiles;
    wchar_t path[MAX_PATH];
    _snwprintf(path, MAX_PATH, L"%s%s", s_phDir, s_phs[i]);
    if (!s_phThread)
        PhotoWake();
    EnterCriticalSection(&s_phCs);
    wcscpy(s_pvPath, path);
    s_pvRot = s_phRot;
    s_pvFill = s_cfg.photoFill;
    s_pvAuto = s_cfg.photoAutoRot;
    s_pvState = 1;
    LeaveCriticalSection(&s_phCs);
    PhotoWake();
    if (s_phShow)
        SetTimer(s_wnd, TIMER_PHOTO, s_cfg.photoInterval * 1000, NULL);
}

static void PhotoStep(int dir)   // the next / previous picture of the folder, round
{
    if (s_phCount <= 0)
        return;
    int i = s_phCur < 0 ? (dir > 0 ? -1 : s_phCount) : s_phCur;
    for (int n = 0; n < s_phCount; n++) {
        i = (i + dir + s_phCount) % s_phCount;
        if (!PhIsDir(i)) {
            PhotoView(i);
            return;
        }
    }
}

static void PhotoShow(bool on)   // the slide show
{
    s_phShow = on;
    if (on)
        SetTimer(s_wnd, TIMER_PHOTO, s_cfg.photoInterval * 1000, NULL);
    else
        KillTimer(s_wnd, TIMER_PHOTO);
}

static void ScrPhotoView(HDC dc)
{
    int n = 0, at = 0;
    for (int i = 0; i < s_phCount; i++)
        if (!PhIsDir(i)) {
            n++;
            if (i == s_phCur)
                at = n;
        }
    wchar_t s[MAX_PATH + 32];
    _snwprintf(s, MAX_PATH + 32, L"ФОТО · %d / %d · %s", at, n, s_phCur >= 0 ? s_phs[s_phCur] : L"");
    Frame(dc, s);
    R(dc, 0, 56, 800, 342, 0, kMargin);
    EnterCriticalSection(&s_phCs);
    int state = s_pvState, err = s_pvErr;
    if (state == 3 && err == PIC_OK)
        BlitPic(s_pv, (800 - s_pv.w) / 2, 56 + (342 - s_pv.h) / 2);
    LeaveCriticalSection(&s_phCs);
    if (state == 1 || state == 2) {
        T(dc, 400, 236, L"Загрузка…", 20, kMuted, 600, AL_MID);
    } else if (state == 3 && err != PIC_OK) {
        static const wchar_t* why[5] = { L"", L"ФОРМАТ НЕ ПОДДЕРЖИВАЕТСЯ", L"СЛИШКОМ БОЛЬШОЕ ИЗОБРАЖЕНИЕ",
                                         L"ФАЙЛ НЕ ЧИТАЕТСЯ", L"НЕ ХВАТАЕТ ПАМЯТИ" };
        Panel(dc, 220, 110, 360, 230);
        Ico(dc, IC_PHOTO, 360, 140, 80, kOff);
        Line(dc, 352, 140, 448, 220, 4, kMuted);
        T(dc, 400, 262, why[err % 5], 18, kText, 700, AL_MID, 336);
        T(dc, 400, 292, s_phCur >= 0 ? s_phs[s_phCur] : L"", 17, kMuted, 400, AL_MID, 336);
        T(dc, 400, 318, err == PIC_FORMAT ? L"JPG (не прогрессивный), PNG, BMP" : L"Пропустить можно стрелками внизу",
          15, kDim, 400, AL_MID, 336);
    }
    Dock(dc);
    IconBtn(dc, IC_GALLERY, 24, 417, 44, kText, A_BACK);
    IconBtn(dc, IC_PREV, 132, 417, 44, kText, A_PHSTEP, -1);
    IconBtn(dc, IC_ROT_L, 244, 417, 44, kText, A_PHROT, -1);
    bool d = Zone(330, 410, 140, 58, A_PHSHOW);   // the slide show: orange while it runs
    R(dc, 330, 410, 140, 58, 10, d || s_phShow ? kOrange : kPanel2);
    Ico(dc, s_phShow ? IC_PAUSE : IC_PLAY, 378, 417, 44, d || s_phShow ? kInk : kOrange);
    IconBtn(dc, IC_ROT_R, 512, 417, 44, kText, A_PHROT, 1);
    IconBtn(dc, IC_FWD, 624, 417, 44, kText, A_PHSTEP, 1);
    IconBtn(dc, IC_GEAR, 732, 417, 44, kText, A_SCREEN, S_SET_PHOTO);
}

static void ScrSetPhoto(HDC dc)
{
    static const wchar_t* scale[2] = { L"ВПИСАТЬ", L"ЗАПОЛН." };
    Settings(dc, S_SET_PHOTO);
    SetRow(dc, 64, L"Интервал слайд-шоу");
    wchar_t iv[16];
    _snwprintf(iv, 16, L"%d С", s_cfg.photoInterval);
    Stepper(dc, 560, 68, 200, iv, 48, 18, A_PHINT);
    SetRow(dc, 120, L"Масштаб");
    Seg(dc, 512, 126, 248, 44, scale, 2, G_PHOTO_SCALE, 15);
    SetRow(dc, 176, L"Слайд-шоу при открытии");
    Toggle(dc, 688, 186, T_SLIDES_OPEN);
    SetRow(dc, 232, L"Автоповорот", NULL, true);
    Toggle(dc, 688, 242, T_AUTOROTATE);
}

static void DoAction(const Btn& b)
{
    if (b.action != A_DELETE)
        s_delArmed = false;
    if (b.action != A_TASKKILL)
        s_taskArm = 0;
    s_clipMenu = false;
    switch (b.action) {
    case A_TASKTAB:    s_taskTab = b.arg; s_taskPage = 0; s_status[0] = 0; break;
    case A_TASKSORT:   TaskSort(&s_task, b.arg); s_taskPage = 0; break;
    case A_TASKPAGE:
        s_taskPage += b.arg;
        s_taskPage = s_taskPage < 0 ? 0 : s_taskPage >= TaskPages() ? TaskPages() - 1 : s_taskPage;
        break;
    case A_TASKKILL:   TaskEnd((DWORD)b.arg); break;
    case A_TASKSWITCH: TaskSwitch((HWND)(INT_PTR)b.arg); break;
    case A_TASKCLOSE:  TaskClose((HWND)(INT_PTR)b.arg); break;
    case A_CLIPCOPY:   ClipCopy(); break;
    case A_CLIPPASTE:  ClipPaste(); break;
    case A_CLIPCLEAR:  s_keysText[0] = 0; break;
    case A_NAV:      Nav((Screen)b.arg); break;
    case A_NIGHT:    SetTheme(!s_night); break;   // the bar: the theme (the backlight: in the settings)
    case A_PHFILE:   // a tile: up, a folder, a picture (the viewer)
        if (b.arg < 0) {
            wchar_t d[MAX_PATH];
            wcscpy(d, s_phDir);
            ParentDir(d);
            PhotoOpenDir(d);
        } else if (PhIsDir(b.arg)) {
            wchar_t d[MAX_PATH];
            _snwprintf(d, MAX_PATH, L"%s%s", s_phDir, s_phs[b.arg]);
            PhotoOpenDir(d);
        } else {
            PhotoView(b.arg);
            Go(S_PHOTO_VIEW, true);
            PhotoShow(s_cfg.photoSlides);
        }
        break;
    case A_PHPLAY:   // the tab's play: the slide show from the picture viewed (or the first)
        if (s_phCur < 0)
            PhotoStep(1);
        else
            PhotoView(s_phCur);
        if (s_phCur >= 0) {
            Go(S_PHOTO_VIEW, true);
            PhotoShow(true);
        }
        break;
    case A_PHSTEP:   PhotoStep(b.arg); break;
    case A_LICSCROLL: s_licScroll += b.arg * 160; break;   // (ScrLicenses keeps it in range)
    case A_PHROT:    s_phRot = (s_phRot + b.arg + 4) % 4; PhotoView(s_phCur); break;
    case A_PHSHOW:   PhotoShow(!s_phShow); break;
    case A_PHINT: {   // the slide show's interval: 2, 3, 5, 10, 15, 30, 60 s
        static const int iv[7] = { 2, 3, 5, 10, 15, 30, 60 };
        int k = 0;
        while (k < 6 && iv[k] < s_cfg.photoInterval)
            k++;
        k = k + b.arg < 0 ? 0 : k + b.arg > 6 ? 6 : k + b.arg;
        s_cfg.photoInterval = iv[k];
        SaveCfg();
        break;
    }
    case A_USERWG:   // a user widget's tap
        if (b.arg >= 0 && b.arg < s_uwCount && s_uw[b.arg].tap[0])
            UserWgRun(s_uw[b.arg], s_uw[b.arg].tap, L"нажатие");
        break;
    case A_SHPAGE:   s_setHomePage += b.arg; break;
    case A_WGVIS:
        s_cfg.homeHidden ^= 1 << b.arg;
        SaveCfg();
        break;
    case A_WGMOVE: {   // arg: id * 2 + (1 later, 0 earlier)
        int i = 0;
        while (i < WgCount() && s_cfg.homeOrder[i] != b.arg / 2)
            i++;
        int j = b.arg % 2 ? i + 1 : i - 1;
        if (i < WgCount() && j >= 0 && j < WgCount()) {
            int t = s_cfg.homeOrder[i];
            s_cfg.homeOrder[i] = s_cfg.homeOrder[j];
            s_cfg.homeOrder[j] = t;
            SaveCfg();
        }
        break;
    }
    case A_THEME:
    case A_LIGHT:
        (b.action == A_THEME ? s_cfg.themeMode : s_cfg.lightMode) = b.arg;
        SaveCfg();
        NightAuto(true);
        break;
    case A_SCREEN:   Go((Screen)b.arg, true); break;
    case A_TAB:      Go((Screen)b.arg, false); break;
    case A_BACK:     Back(); break;
    case A_TOGGLE:
        s_on[b.arg] = !s_on[b.arg];
        if (RsetOfToggle(b.arg) >= 0)   // a radio setting: the MCU's key, its reply sets the switch
            McuKey(kRsetKey[RsetOfToggle(b.arg)][s_on[b.arg] ? 1 : 0], true);
        if (IsCfgToggle(b.arg))
            ControlsToCfg();
        break;
    case A_SEG:
        s_seg[b.arg / 16] = b.arg % 16;
        if (b.arg / 16 == G_PHOTO_SCALE) {   // fit / fill: the picture viewed again
            s_cfg.photoFill = b.arg % 16 == 1;
            SaveCfg();
        }
        if (b.arg / 16 == G_SAVER) {
            s_cfg.saverMin = kSaverMins[b.arg % 16 % 5];
            SaveCfg();
        }
        if (b.arg / 16 == G_WPROV || b.arg / 16 == G_CAMDIST || b.arg / 16 == G_VIDEO_SCALE || b.arg / 16 == G_CAMSND)
            ControlsToCfg();
        if (b.arg / 16 == G_CAMSND && b.arg % 16 > 0)
            Beep2();   // a sample: how it sounds over the radio
        break;
    case A_KEYS:     OpenCfgKeys(b.arg); break;
    case A_HOMEGPS:  HomeFromGps(); break;
    case A_TZ:       TzStep(b.arg); break;
    case A_GPSPORT:  GpsPortStep(b.arg); break;
    case A_GPSBAUD:  GpsBaudStep(b.arg); break;
    case A_HOUR:     ClockStep(true, b.arg); break;
    case A_MINUTE:   ClockStep(false, b.arg); break;
    case A_CANLOG:   CanLogToggle(); break;
    case A_CANMARK:  MarkLogs(); break;
    case A_MCULOG:   McuLogToggle(); break;
    case A_PLAY:     PlayerToggle(); break;
    case A_PREVTRK:  PlayerPrev(); break;
    case A_NEXTTRK:  PlayerStep(1, false); break;
    case A_SEEK:     PlayerSeek(b.arg); break;
    case A_SHUFFLE:  s_cfg.shuffle = !s_cfg.shuffle; SaveCfg(); break;
    case A_REPEAT:   s_cfg.repeat = !s_cfg.repeat; SaveCfg(); break;
    case A_AVTEST:   AvTestRun(b.arg); break;
    case A_AVDEV:    AvDevNext(); break;
    case A_PRESET:   RadioOn(); RadioPreset(b.arg); break;
    case A_RKEY:     RadioKey(b.arg); break;
    case A_RPAGE:    RadioPageTo(s_radioPage + 1); break;   // the band button: the next page, round
    case A_RLIST:    McuSend3("SPApi_SendListItemControl", 0, b.arg, 0); break;
    case A_RLISTPAGE: s_rlistPage += b.arg; if (s_rlistPage < 0) s_rlistPage = 0; break;
    case A_RSCAN:    RadioScan(); break;
    case A_BTMUSIC:  BtMusicOn(); break;
    case A_BTCMD:
        if (b.arg == BT_CMD_PLAYPAUSE || b.arg == BT_CMD_PLAY) {   // the phone's music: the source too, ours stop
            int vol, mute, src;
            VolGet(&vol, &mute, &src);
            OursQuiet();
            if (src != 9)
                BtMusicOn();
        }
        BtCmd(b.arg);
        break;
    case A_REBOOTNO: McuKey(0x5A, false); Back(); break;
    case A_MFILE:    MusicPick(b.arg); break;
    case A_PHONECALL:
        wcscpy(s_callOut, s_dial);
        BtCmdText(BT_CMD_DIAL, s_dial);
        Go(S_CALL, true);
        break;
    case A_PHONECLR: s_dial[0] = 0; break;
    case A_PHONEANS:
        BtCmd(BT_CMD_ANSWER);
        Back();
        Go(S_CALL, true);
        break;
    case A_PHONEEND:
        BtCmd(BT_CMD_END);
        if (s_screen == S_INCOMING || s_screen == S_CALL)
            Back();
        break;
    case A_MALL:
        if (!BassLoad())
            wcscpy(s_status, L"звук не запускается (BASS)");
        else if (PlayerLoadDir(s_musDir, NULL, true))
            Back();
        else
            wcscpy(s_status, L"здесь и глубже нет музыки");
        break;
    case A_MUP:      MusicUp(b.arg != 0); break;
    case A_VFILE:    VidPick(b.arg); break;
    case A_VUP:      VidUp(b.arg != 0); break;
    case A_VPLAY:    VidToggle(); break;
    case A_VPREV:    VidStep(-1); break;
    case A_VNEXT:    VidStep(1); break;
    case A_VSEEK:    VidSeek(b.arg); break;
    case A_VFULL:    s_vidFull = !s_vidFull; break;
    case A_NRPLAY:   NetToggle(); break;
    case A_NRSTEP:   NetStep(b.arg); break;
    case A_NRPICK:   NetPlay(b.arg); Back(); break;
    case A_NRPAGE:   NetPage(b.arg); break;
    case A_WIFINET:  WifiTap(b.arg); break;
    case A_WIFIFORGET: WifiForgetTap(b.arg); break;
    case A_WIFISCAN: WifiScan(); break;
    case A_OBDSCAN:   // the stack's search; its devices are read from the proxy's events from now on
        s_obdFoundN = 0;
        s_obdEvSeen = BtShmGet() ? BtShmGet()->evSeq : -1;
        s_obdSearchTick = GetTickCount();
        VLog(L"obd: поиск устройств");
        BtCmd(BT_CMD_SEARCH);
        Redraw();
        break;
    case A_OBDPICK:
        if (b.arg >= 0 && b.arg < s_obdFoundN) {
            MultiByteToWideChar(CP_ACP, 0, s_obdFound[b.arg].addr, -1, s_obdAddr, 18);
            s_obdAddr[17] = 0;
            SaveCfg();
        }
        Redraw();
        break;
    case A_OBDPIN: s_obdPin = b.arg; Redraw(); break;
    case A_RVCVIEW:   // the camera's picture now, whatever the gear (powered only in reverse? - to be seen)
        s_rvcManual = true;
        VLog(L"камера: показать вручную");
        RvcTick();
        break;
    case A_BTUNPAIR: {   // the first press asks, the second removes
        BtState bt;
        if (b.arg != s_pairedAsk) {
            s_pairedAsk = b.arg;
        } else if (BtGet(&bt, NULL) && b.arg < bt.paired && b.arg < BT_PAIRED) {
            wchar_t a[18];
            BtText(bt.pairedAddr[b.arg], a, 18);
            VLog(L"bt: удалить сопряжение %s", a);
            BtCmdText(BT_CMD_UNPAIR, a);
            s_pairedAsk = -1;
        }
        Redraw();
        break;
    }
    case A_OBDPAIR: {
        wchar_t t[40];
        _snwprintf(t, 40, L"%s %S", s_obdAddr, kObdPins[s_obdPin]);
        t[39] = 0;
        VLog(L"obd: сопряжение %s", t);
        BtCmdText(BT_CMD_PAIR, t);
        break;
    }
    case A_OBDCONN: {
        ObdInfo o;
        ObdGet(&o);
        if (o.state == OBD_CONNECTING || o.state == OBD_ON) {
            ObdStop();
        } else {
            char a[18];
            WideCharToMultiByte(CP_ACP, 0, s_obdAddr, -1, a, 18, NULL, NULL);
            a[17] = 0;
            ObdConnect(a);
        }
        Redraw();
        break;
    }
    case A_WIFIAP: {   // the access point by hand: on until pressed again
        WifiInfo w;
        WifiGet(&w);
        WifiApManual(!(w.state == WIFI_AP && !w.apAuto));
        break;
    }
    case A_WIFIPAGE: s_wifiPage += b.arg; if (s_wifiPage < 0) s_wifiPage = 0; break;
    case A_MCUREQ:   McuRequest(); break;
    case A_EQ:       EqSet(b.arg); break;
    case A_EQPRESET: EqPresetSet(b.arg); break;
    case A_BAL:      BalanceStep(false, b.arg); break;
    case A_FADE:     BalanceStep(true, b.arg); break;
    case A_BALCENTER: BalanceStep(false, 0); break;
    case A_SNDVOL: {   // demo volumes: row * 4 + 1 (down) / + 3 (up)
        int row = b.arg / 4, v = s_sndVol[row] + (b.arg % 4) - 2;
        if (row >= 0 && row < 4 && v >= 0 && v <= 30)
            s_sndVol[row] = v;
        break;
    }
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
            if (field == K_DIAL)
                DtmfPlay(text[n]);
        }
        break;
    }
    case A_PAGE:     // the dots go round
        if (s_screen == S_HOME)
            s_homePage = (s_homePage + 1) % HomePages();
        else if (s_screen == S_RADIO)
            RadioPageTo(s_radioPage + 1);
        else if (s_screen == S_AUDIO_LIST)
            s_musPage = (s_musPage + 1) % MusPages();
        else if (s_screen == S_VIDEO)
            s_vidPage = (s_vidPage + 1) % VidPages();
        else if (s_screen == S_PHOTO)
            s_phPage = (s_phPage + 1) % PhotoPages();
        else if (s_screen == S_FILES)
            s_filePage = (s_filePage + 1) % FilePages();
        else if (s_screen == S_BT_PAIRED)
            s_pairedPage++;   // (ScrBtPaired wraps it)
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
    case A_MINIMIZE: MinimizeHome(); break;
    case A_CALIBRATE: Calibrate(); break;
    case A_SOON:     wcscpy(s_status, b.arg ? L"нужно чтение GPS (NMEA)" : L"нужен протокол MCU"); break;
    case A_TEST:     if (++s_testStep >= kTestSteps) Back(); break;
    case A_EXIT:     StartExplorer(); DestroyWindow(s_wnd); return;
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
        if (!_wcsicmp(line, L"saver")) {
            wcscpy(s_skinSaver, file);
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
               ";   background = an 800 x 480 picture for the main menu instead of the built-in mosaic,\n"
               ";   saver = the screen saver's 800 x 480 picture (kept only while it shows).\n"
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

// The list the press started on, which a finger moved up / down scrolls: 1 the internet radio's stations, 2 the
// licenses' text, 0 none.
static int ListUnderFinger()
{
    if (s_screen == S_NETRADIO_LIST && s_downX < 720 && s_downY >= kNetTop && s_downY < kNetBottom)
        return 1;
    if (s_screen == S_LICENSES && s_downX >= 56 && s_downX < 704 && s_downY >= kLicTop && s_downY < kLicBottom)
        return 2;
    return 0;
}

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
        {
            static const wchar_t* kSrc[4] = { L"нет", L"ID устройства", L"MAC адаптеров", L"ПК" };
            int src = SecretInit();   // the key that seals the passwords of the settings
            VLog(L"ключ паролей: %s", kSrc[src & 3]);
        }
        LoadCfg();
        CfgToControls();
        s_bootTick = GetTickCount();
        UserWidgetsScan();   // (after the ini: their places in the order)
        ResumeScreen();
        WaveDevsLog();
        {
            WSADATA wsa;
            WSAStartup(MAKEWORD(2, 2), &wsa);
        }
        InitializeCriticalSection(&s_wxCs);
        NetInit();
        HttpInit();
        GpsStartCore();
        McuStart();
        VLog(L"поток главный (окно): id %08X", (unsigned)GetCurrentThreadId());
        if (!s_cfg.wifiApPass[0]) {   // the access point's password: 8 digits, made once
            srand(GetTickCount() ^ (DWORD)(INT_PTR)hwnd);
            for (int i = 0; i < 8; i++)
                s_cfg.wifiApPass[i] = (wchar_t)(L'0' + (rand() ^ (GetTickCount() >> i)) % 10);
            s_cfg.wifiApPass[8] = 0;
            SaveCfg();
        }
        WifiEnable(s_cfg.wifiOn);
        WifiApSetup(s_cfg.wifiApSsid, s_cfg.wifiApPass, s_cfg.wifiApAuto);
        WifiStart(hwnd, WM_APP_WIFI, VLog);
        ObdInit(hwnd, WM_APP_OBD, VLog);
        for (int i = 0; i < s_cfg.wifiCount; i++)
            if (!s_cfg.wifiLocked[i])
                WifiRemember(s_cfg.wifiSsid[i], s_cfg.wifiPass[i]);
        s_mediaMask = MediaMask();
        RvcInit(VLog);
        SetTimer(hwnd, TIMER_RVC, kRvcMs, NULL);   // the reverse gear (rvc.cpp), cheap: one GPIO read
        WeatherRefresh();
        BuildButtons();
        SetTimer(hwnd, 1, 1000, NULL);
        return 0;
    }
    case WM_TIMER: {
        if (wp == TIMER_LONG) {   // held on the keyboard's text field: copy / paste
            KillTimer(hwnd, TIMER_LONG);
            if (s_pressed >= 0 && s_btns[s_pressed].action == A_BTCMD) {   // held on ⏮ / ⏭: rewind / fast forward
                s_btHold = s_btns[s_pressed].arg == BT_CMD_NEXT ? BT_CMD_FF_RELEASE : BT_CMD_REW_RELEASE;
                BtCmd(s_btns[s_pressed].arg == BT_CMD_NEXT ? BT_CMD_FF_PRESS : BT_CMD_REW_PRESS);
                return 0;   // the button stays pressed until the finger goes up
            }
            if (s_pressed >= 0 && s_btns[s_pressed].action == A_KEY) {   // "0" held on the keypad: "+"
                int n = (int)wcslen(s_dial);
                if (n < 19) {
                    s_dial[n] = L'+';
                    s_dial[n + 1] = 0;
                    DtmfPlay(L'0');
                }
                s_pressed = -1;   // the release does not add the "0"
                Redraw();
                return 0;
            }
            if (s_pressed >= 0 && s_btns[s_pressed].action == A_PRESET) {   // held on a preset: save it
                RadioPresetSave(s_btns[s_pressed].arg);
                s_pressed = -1;
                Redraw();
                return 0;
            }
            if (s_pressed >= 0 && s_btns[s_pressed].action == A_KEYFIELD) {
                s_pressed = -1;   // the release does nothing
                s_clipMenu = true;
                Redraw();
            }
            return 0;
        }
        if (wp == TIMER_GUARD) {
            OnGuardTimer();
            return 0;
        }
        if (wp == TIMER_VIDEO) {
            VidTick();
            return 0;
        }
        if (wp == TIMER_RVC) {
            RvcTick();
            return 0;
        }
        if (wp == TIMER_MARQ) {   // the running lines of the radio screen: only its text strip is drawn again
            if (s_screen != S_RADIO || !s_marqOn || !IsWindowVisible(hwnd) || GetForegroundWindow() != hwnd) {
                KillTimer(hwnd, TIMER_MARQ);
                return 0;
            }
            s_marq += kMarqStep;
            s_marqOn = false;   // set again by a line that still does not fit
            RedrawArea(300, 100, 764, 164);
            return 0;
        }
        if (wp == TIMER_VOICE) {
            VoiceTimer();
            return 0;
        }
        if (wp == TIMER_PHOTO) {   // the slide show: the next picture once the one shown is there
            if (s_screen != S_PHOTO_VIEW) {
                PhotoShow(false);
                return 0;
            }
            if (s_pvState == 3)
                PhotoStep(1);
            if (IsWindowVisible(hwnd))
                Redraw();
            return 0;
        }
        if (wp == TIMER_BTSRC) {   // BtMusicOn's second key
            KillTimer(hwnd, TIMER_BTSRC);
            McuKey(0x54, false);
            return 0;
        }
        if (wp == 1 && s_screen == S_RADIO_LIST && s_scanStart && IsWindowVisible(hwnd)) {
            RadioScanTick();
            Redraw();
        }
        if (wp == 1) {   // day / night: all modes 10 s after the start (the MCU's link up), then by the sun
            static int s_ticks;
            if (++s_ticks >= 10)
                NightAuto(s_ticks == 10);
        }
        if (wp == 1) {   // the night mode switched in OSMapsCE: follow it
            int n = NightShared();
            if (n >= 0 && (n != 0) != s_night) {   // OSMapsCE switched it: our colours, not the backlight
                SetTheme(n != 0);
                if (IsWindowVisible(hwnd))
                    Redraw();
            }
        }
        if (wp == 1) {   // calls: an incoming one opens its window, a taken one the call's, the end closes them
            BtState bt;
            static int s_lastCall;
            if (BtGet(&bt, NULL) && bt.call[0] != s_lastCall) {
                int c = bt.call[0];
                s_lastCall = c;
                bool front = IsWindowVisible(hwnd) && GetForegroundWindow() == hwnd;
                if (c == BT_CALL_ACTIVE)
                    s_callTick = GetTickCount();
                if (front && c == BT_CALL_INCOMING && s_screen != S_INCOMING) {
                    Go(S_INCOMING, true);
                } else if (front && s_screen == S_INCOMING && c >= BT_CALL_DIALING) {
                    Back();
                    Go(S_CALL, true);
                } else if (c == BT_CALL_NONE && (s_screen == S_INCOMING || s_screen == S_CALL)) {
                    Back();
                }
                if (IsWindowVisible(hwnd))
                    Redraw();
            } else if (s_screen == S_CALL && IsWindowVisible(hwnd)) {
                Redraw();   // the time talking
            }
        }
        if (wp == 1) {   // the screen saver: untouched for its minutes, VestaHome in front, nothing to watch (video,
                         // photos, a call)
            DWORD now = GetTickCount();
            bool front = IsWindowVisible(hwnd) && GetForegroundWindow() == hwnd;
            if (!s_lastTouch)
                s_lastTouch = now;
            if (s_screen != S_SAVER)
                SaverFree();
            if (s_screen == S_SAVER) {
                static int s_min = -1;
                SYSTEMTIME t;
                GetLocalTime(&t);
                if (t.wMinute != s_min && front) {   // the clock
                    s_min = t.wMinute;
                    Redraw();
                }
            } else if (s_cfg.saverMin > 0 && front && now - s_lastTouch > (DWORD)s_cfg.saverMin * 60000 &&
                       s_screen != S_VIDEO_PLAY && s_screen != S_PHOTO_VIEW && s_screen != S_CALL &&
                       s_screen != S_INCOMING &&
                       s_screen != S_REBOOT && s_screen != S_KEYS) {
                Go(S_SAVER, true);
                Redraw();
            }
        }
        if (wp == 1) {   // the screen and the source: back at the start, then remembered
            StartVolumeTick();
            ResumeSourceTick();
            UiRememberTick();
            for (int u = 0; u < s_uwCount; u++) {   // the user widgets' data: re-read as often as each asks
                UserWg& w = s_uw[u];
                if (GetTickCount() - w.readTick < (DWORD)w.refresh * 1000)
                    continue;
                w.readTick = GetTickCount();
                RECT wr;
                if (UserWgRead(w) && s_screen == S_HOME && IsWindowVisible(hwnd) && HomeRect(WG_COUNT + u, &wr))
                    RedrawArea(wr.left, wr.top, wr.right, wr.bottom);
            }
            if (s_cfgSoon && GetTickCount() - s_cfgSoonTick > 60000) {
                s_cfgSoon = false;
                SaveCfg();
            }
        }
        if (wp == 1 && s_radioDirty) {   // the radio's cache, at most every 30 s
            static DWORD s_saved;
            if (GetTickCount() - s_saved > 30000) {
                s_saved = GetTickCount();
                s_radioDirty = false;
                SaveCfg();
            }
        }
        if (wp == 1 && s_radioAsk) {   // the tuner silent since the start: asked again (s_tunLive)
            static DWORD s_asked;
            DWORD now = GetTickCount();
            if (s_tunLive || now - s_radioAsk > 60000) {
                VLog(L"радио: тюнер %s через %lu с", s_tunLive ? L"ответил" : L"не ответил", (now - s_radioAsk) / 1000);
                s_radioAsk = 0;
            } else if (now - s_asked >= 3000) {
                static bool s_woken;
                if (!s_woken) {   // as the shell at its start
                    s_woken = true;
                    McuSend3("SPApi_SendRadioNaviVoiceGuidance", 0, 0, 0);
                    McuSend3("SPApi_SendRadioIsNaviSourceCouldIn", 0, 0, 0);
                }
                s_asked = now;
                McuKey(0x30, false);
            }
        }
        if (wp == 1) {   // the source: our switches checked; changed elsewhere (MODE, the shell) - ours stop
            static int s_srcSeen = -2;
            int vol, mute, src;
            VolGet(&vol, &mute, &src);
            DWORD now = GetTickCount();
            if (s_radioSrcAsk && now - s_radioSrcAsk > 1500) {
                s_radioSrcAsk = 0;
                if (src != 0) {   // the keys did not do it: a preset does (the one playing, else the first)
                    Tuner t;
                    TunerGet(&t);
                    int n = t.cur >= 0 && t.cur < 24 ? t.cur : 0;
                    VLog(L"радио: клавиши не переключили источник (%d) - пресет %d", src, n + 1);
                    McuSend3("SPApi_SendListItemControl", 1, n, 0);
                    s_srcByUs = now;
                }
            }
            if (s_btPathTick && now - s_btPathTick > 3000) {   // the BT key may start the phone: it is paused
                s_btPathTick = 0;
                BtQuiet();
            }
            if (src != s_srcSeen) {
                if (s_srcSeen != -2 && (src == 0 || src == 9) && now - s_srcByUs > 6000) {
                    VLog(L"источник: %s - наши плееры на паузу", src ? L"Bluetooth" : L"радио");
                    OursQuiet();
                }
                s_srcSeen = src;
            }
        }
        if (wp == 1 && s_screen == S_SET_OBD && IsWindowVisible(hwnd) && ObdScanEvents())
            Redraw();   // the search's devices
        if (wp == 1 && (s_screen == S_AUDIO_BT || s_screen == S_SET_BT || s_screen == S_BT_PAIRED) &&
            IsWindowVisible(hwnd)) {
            BtState bt;   // the Bluetooth screens follow the proxy's state
            LONG seq = 0;
            static LONG s_btSeen;
            if (BtGet(&bt, &seq) && seq != s_btSeen) {
                s_btSeen = seq;
                Redraw();
            }
        }
        if (wp == TIMER_WAIT) {
            OnWaitTimer();
            if (IsWindowVisible(hwnd))
                Redraw();   // the status line may have changed
            return 0;
        }
        // the volume window closes by itself after 2.5 s, the warning after 10 s
        DWORD open = GetTickCount() - s_screenTick;
        if (s_screen == S_REBOOT && open <= 6000) {   // the countdown
            Redraw();
            return 0;
        }
        if ((s_screen == S_VOLUME && open > 2500) || (s_screen == S_WARNING && open > 10000) ||
            s_screen == S_REBOOT) {
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
        NetTick();
        {
            static DWORD s_loadTick;
            if (!s_loadTick) {   // the first sample: the minute is counted from it
                s_loadTick = GetTickCount();
                TaskSample(&s_task, hwnd);
            } else if (GetTickCount() - s_loadTick >= 60000) {
                s_loadTick = GetTickCount();
                LogLoad(hwnd);
            }
        }
        {   // an SD card or a USB disk in or out: the marks of the top bar
            int m = MediaMask();
            if (m != s_mediaMask) {
                s_mediaMask = m;
                UserWidgetsScan();   // the card came or went: its widgets
                if (IsWindowVisible(hwnd) && GetForegroundWindow() == hwnd)
                    Redraw();
            }
        }
        if (s_hvacShowTick && GetTickCount() - s_hvacShowTick > 4000) {   // the climate plate goes away
            s_hvacShowTick = 0;
            if (IsWindowVisible(hwnd) && GetForegroundWindow() == hwnd)
                Redraw();
        }
        // the task manager: a new sample every 2 s, only while it is in front
        bool tasks = s_screen == S_TASKS && (++s_taskTicks & 1) == 0 && IsWindowVisible(hwnd) &&
                     GetForegroundWindow() == hwnd;
        if (tasks)
            TaskSample(&s_task, hwnd);
        // only what changed, only when in front: the whole frame for a new minute (the clock, the date) and the
        // screens that are lists of live data; otherwise just the area of the value that moves
        SYSTEMTIME t;
        GetLocalTime(&t);
        RECT wr;
        if (IsWindowVisible(hwnd) && GetForegroundWindow() == hwnd) {
            if (t.wMinute != s_drawnMinute || tasks || s_screen == S_CALL || s_screen == S_TEST_CAN ||
                s_screen == S_TEST_MCU)
                Redraw();
            else if (s_screen == S_AUDIO && PlayerPlaying())
                RedrawArea(272, 130, 800, 300);   // the track's position
            else if (s_screen == S_HOME && HomeRect(WG_MEDIA, &wr) && PlayerPlaying())
                RedrawArea(wr.left, wr.top, wr.right, wr.bottom);   // the media widget
            else if (s_screen == S_NETRADIO && s_netState == NR_WAIT)
                RedrawArea(272, 262, 800, 300);   // "again in N s"
        }
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
        if (LOWORD(wp) == WA_INACTIVE)
            VidSync();   // another program in front: the video pauses, its picture goes
        if (LOWORD(wp) != WA_INACTIVE) {
#ifdef UNDER_CE
            if (s_minimized) {   // after "Minimize": on top again, the taskbar away
                s_minimized = false;
                SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
                s_taskbar = FindWindow(L"HHTaskBar", NULL);
                if (s_taskbar)
                    ShowWindow(s_taskbar, SW_HIDE);
            }
#endif
            Redraw();
        }
        return 0;
    case WM_VESTA_NIGHT:
        return s_night ? 1 : 0;
    case WM_APP_PLAYER:   // the track ended (BASS's sync): the next one
        PlayerStep(1, true);
        if (IsWindowVisible(hwnd) && GetForegroundWindow() == hwnd)
            Redraw();
        return 0;
    case WM_APP_UWG:   // a widget's url fetched: its data read at the next second
        for (int u = 0; u < s_uwCount; u++)
            s_uw[u].readTick = 0;
        return 0;
    case WM_APP_PHOTO:   // a picture or a thumbnail decoded
        if ((s_screen == S_PHOTO || s_screen == S_PHOTO_VIEW) && IsWindowVisible(hwnd) && !s_drag)
            Redraw();
        return 0;
    case WM_APP_NET:   // the internet radio: connected, failed, broken, a new song title
        OnNetMessage(wp, lp);
        if (IsWindowVisible(hwnd) && GetForegroundWindow() == hwnd) {
            RECT wr;
            if (s_screen == S_NETRADIO)
                RedrawArea(272, 130, 800, 390);   // the station's lines and its buttons
            else if (s_screen == S_HOME && HomeRect(WG_MEDIA, &wr))
                RedrawArea(wr.left, wr.top, wr.right, wr.bottom);   // the media widget
            else if (s_screen == S_NETRADIO_LIST)
                Redraw();
        }
        return 0;
    case WM_APP_WIFI: {   // the Wi-Fi thread saw a change: its screen, or the top bar's mark when the state moved
        static int s_lastState = -1;
        WifiInfo w;
        WifiGet(&w);
        bool mark = w.state != s_lastState;
        s_lastState = w.state;
        if (IsWindowVisible(hwnd) && GetForegroundWindow() == hwnd && (mark || s_screen == S_SET_WIFI))
            Redraw();
        return 0;
    }
    case WM_APP_OBD:   // the OBD thread's step or values
        if (s_screen == S_SET_OBD && IsWindowVisible(hwnd))
            Redraw();
        return 0;
    case WM_APP_VIDTAP:   // a tap on the picture: the whole screen and back
        s_vidFull = !s_vidFull;
        Redraw();
        return 0;
    case WM_APP_MCU: {   // a climate change: its plate in the top bar for 4 s, the screens that show it
        if (wp == WP_MCU_TUNER) {   // the tuner changed: the radio screen and the home widget
            Tuner t;
            TunerGet(&t);   // (clears s_tunPosted)
            RadioBandStep(t);
            RadioNameLearn(t.f10, t.ps);
            s_radioDirty = true;
            if (IsWindowVisible(hwnd) && (s_screen == S_RADIO || s_screen == S_HOME || s_screen == S_SET_RADIO ||
                                          s_screen == S_RADIO_LIST))
                Redraw();
            return 0;
        }
        if (wp == WP_MCU_DAYNIGHT) {   // the backlight's day / night and brightness: the settings' lines
            if (IsWindowVisible(hwnd))
                Redraw();
            return 0;
        }
        if (wp == WP_MCU_SOUND) {   // the MCU's EQ / balance (the shell's too): ours follow
            if (GetTickCount() - s_sndSentTick < 1500)
                return 0;
            EnterCriticalSection(&s_mcuCs);
            McuSound m = s_mcuSnd;
            LeaveCriticalSection(&s_mcuCs);
            bool changed = false;
            for (int i = 0; i < 3; i++)
                if (m.eq[i] != 99 && m.eq[i] != s_cfg.eq[i]) {
                    s_cfg.eq[i] = m.eq[i];
                    changed = true;
                }
            if (m.bal != 99 && (m.bal != s_cfg.balance || m.fad != s_cfg.fader)) {
                s_cfg.balance = m.bal;
                s_cfg.fader = m.fad;
                changed = true;
            }
            if (changed) {
                SaveCfg();
                if (IsWindowVisible(hwnd) && (s_screen == S_SET_SOUND || s_screen == S_SET_BALANCE))
                    Redraw();
            }
            return 0;
        }
        if (wp == WP_MCU_REBOOT) {
            VLog(L"mcu: крутилка удержана — перезагрузка через 5 с");
            s_rebootTick = GetTickCount();
            if (IsWindowVisible(hwnd) && GetForegroundWindow() == hwnd) {
                if (s_screen != S_REBOOT)
                    Go(S_REBOOT, true);
                BuildButtons();
                Redraw();
            }
            return 0;
        }
        if (wp == WP_MCU_MARK) {   // MUTE on while recording: one mark
            if (s_canLog != INVALID_HANDLE_VALUE || s_mcuLog != INVALID_HANDLE_VALUE)
                MarkLogs();
            return 0;
        }
        if (wp == WP_MCU_VOLUME) {   // the volume or the mute changed: the volume window for 2.5 s
            int v, mu, src;
            VolGet(&v, &mu, &src);
            if (!IsWindowVisible(hwnd))
                return 0;
            if (GetForegroundWindow() == hwnd && s_screen != S_VOLUME)
                Go(S_VOLUME, true);
            if (s_screen == S_VOLUME)
                s_screenTick = GetTickCount();   // kept open while it keeps changing
            BuildButtons();
            Redraw();   // the window, or the level in the top bar
            return 0;
        }
        Hvac h;
        HvacGet(&h);
        s_hvacShowTick = GetTickCount();
        if (IsWindowVisible(hwnd) && GetForegroundWindow() == hwnd)
            Redraw();
        return 0;
    }
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
        RECT sr;
        bool spd = s_screen == S_HOME && HomeRect(WG_SPEED, &sr);
        bool shown = spd || s_screen == S_WSET_SPEED || s_screen == S_SET_CAR ||
                     s_screen == S_TEST_CAN || s_screen == S_INFO;
        if (IsWindowVisible(hwnd) && GetForegroundWindow() == hwnd && (cam || (shown && GetTickCount() - s_drawTick >= 1000))) {
            s_drawTick = GetTickCount();
            if (!cam && spd) {   // the speed widget and the top bar's marks
                RedrawArea(sr.left, sr.top, sr.right, sr.bottom);
                RedrawArea(0, 0, 800, 56);
            } else {
                Redraw();
            }
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
        s_lastTouch = GetTickCount();
        s_downX = (short)LOWORD(lp);
        s_downY = (short)HIWORD(lp);
        s_pressed = HitTest((short)LOWORD(lp), (short)HIWORD(lp));
        if (s_screen == S_TEST_TOUCH && s_pressed < 0)
            AddTouch((short)LOWORD(lp), (short)HIWORD(lp));
        if (s_pressed >= 0 && (s_btns[s_pressed].action == A_KEYFIELD || s_btns[s_pressed].action == A_PRESET ||
                               (s_btns[s_pressed].action == A_KEY && s_btns[s_pressed].arg == K_DIAL * 65536 + L'0') ||
                               (s_btns[s_pressed].action == A_BTCMD &&
                                (s_btns[s_pressed].arg == BT_CMD_NEXT || s_btns[s_pressed].arg == BT_CMD_PREV))))
            SetTimer(hwnd, TIMER_LONG, s_btns[s_pressed].action == A_PRESET ? 1200 : kLongMs, NULL);
        SetCapture(hwnd);
        Redraw();
        return 0;
    case WM_MOUSEMOVE:   // the touch test and the balance point follow the finger, a swiped page, a scrolled list
        if ((wp & MK_LBUTTON) && ListUnderFinger()) {
            int dy = (short)HIWORD(lp) - s_downY;
            if (!s_listDrag && (dy > 12 || dy < -12)) {
                s_listDrag = true;
                s_listDrag0 = ListUnderFinger() == 1 ? s_netScroll : s_licScroll;
                KillTimer(hwnd, TIMER_LONG);
                s_pressed = -1;
            }
            if (s_listDrag) {
                if (ListUnderFinger() == 1) {
                    NetScrollTo(s_listDrag0 - dy);
                    RedrawArea(16, 64, 784, 392);
                } else {
                    s_licScroll = s_listDrag0 - dy;
                    RedrawArea(40, 40, 760, 440);
                }
                return 0;
            }
        }
        if ((wp & MK_LBUTTON) && CanSwipe() && !s_btHold &&
            !(s_pressed >= 0 && s_btns[s_pressed].action == A_BALXY)) {
            int dx = (short)LOWORD(lp) - s_downX, dy = (short)HIWORD(lp) - s_downY;
            if (!s_drag && (dx > 24 || dx < -24) && abs(dx) > abs(dy))
                DragStart();
            if (s_drag) {
                DragMove(dx);
                return 0;
            }
        }
        if (s_screen == S_TEST_TOUCH && (wp & MK_LBUTTON) && s_pressed < 0) {
            AddTouch((short)LOWORD(lp), (short)HIWORD(lp));
            Redraw();
        }
        if ((wp & MK_LBUTTON) && s_pressed >= 0 && s_btns[s_pressed].action == A_BALXY)
            BalanceAt((short)LOWORD(lp), (short)HIWORD(lp), false);
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
        KillTimer(hwnd, TIMER_LONG);
        if (s_btHold) {   // rewind / fast forward ends with the finger
            BtCmd(s_btHold);
            s_btHold = 0;
            s_pressed = -1;
            ReleaseCapture();
            Redraw();
            return 0;
        }
        ReleaseCapture();
        int x = (short)LOWORD(lp), dx = x - s_downX;
        if (s_listDrag) {   // the list scrolled: no tap; the buttons of the new positions
            s_listDrag = false;
            s_pressed = -1;
            Redraw();
            return 0;
        }
        if (s_drag) {
            s_pressed = -1;
            DragEnd(dx);
            return 0;
        }
        int i = HitTest(x, (short)HIWORD(lp));
        int was = s_pressed;
        s_pressed = -1;
        if (was >= 0 && s_btns[was].action == A_BALXY) {   // the finger may have left the car: still the point
            BalanceAt(x, (short)HIWORD(lp), true);
            Redraw();
            return 0;
        }
        if ((s_screen == S_HOME || s_screen == S_APPS || s_screen == S_FILES || s_screen == S_RADIO ||
             s_screen == S_PHOTO_VIEW) && (dx > 120 || dx < -120)) {
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
            SetTheme(!s_night);
            Redraw();
        } else if (wp == 'R') {   // a "reverse" to try the camera window
            s_rvcFake = !s_rvcFake;
        } else if (wp == VK_ESCAPE) {
            DestroyWindow(hwnd);
        }
        return 0;
#endif
    case MM_WOM_DONE:   // the camera beep has been played
        VoiceDone();
        return 0;
    case WM_DESTROY:
        UiRememberTick();   // the screen and the source as they are now, and what waits for the minute's save
        if (s_cfgSoon) {
            s_cfgSoon = false;
            SaveCfg();
        }
        if (s_voice)
            NaviVoice(false);
        if (s_rvcWnd)
            RvcHide();
        VideoStop();
        NetStop();
        WifiStop();
        ObdStop();
        GpsStopThread();
        McuStopThread();
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
    DWORD style = WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN;   // children: the video's picture
    int x = 0, y = 0, w = 800, h = 480;
#else
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE | WS_CLIPCHILDREN;
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
