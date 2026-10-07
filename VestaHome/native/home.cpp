// VestaHome - test main menu for the Lada Vesta MMC (Windows CE 6.0, 800x480), style of the skin concept v0.1.
// Started by the stock shell G3NavHMI from Config\ExternNavi.cfg (the "Navigation" button), like AppLauncher.
// The "Apps" screen lists programs from apps.txt next to the exe; shortcuts are added, changed and removed
// right on that screen (own on-screen keyboard and file browser: the unit has neither).
// Colours and pictures come from Skin\skin.ini; whatever it doesn't give is drawn by the built-in code.
// Also builds as a desktop Win32 app for testing on a PC; "VestaHomePC.exe /export" writes the built-in
// graphics to Skin\ as PNG files plus a default skin.ini.
#include <windows.h>
#include <shellapi.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

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

// ---------------------------------------------------------------- layout

enum Icon { IC_NONE, IC_MAP, IC_NAV, IC_RADIO, IC_MUSIC, IC_PHONE, IC_GEAR, IC_HOME, IC_MOON, IC_BACK, IC_SUN,
            IC_APPS, IC_VIDEO, IC_NEXT, IC_CLOSE, IC_EDIT, IC_ADD, IC_FOLDER, IC_FILE, IC_UP, IC_SHIFT, IC_BKSP,
            IC_AUX, IC_PHOTO, IC_INFO, IC_PREV, IC_FWD, IC_PAUSE, IC_COUNT };

// Picture names in skin.ini (and Skin\<name>.png), by icon; NULL = icon not used, nothing to skin.
static const wchar_t* kIconKey[IC_COUNT] = { NULL, NULL, NULL, L"radio", L"music", L"phone", L"settings", L"home",
    L"night", L"prev", L"day", L"apps", L"video", L"next", L"exit", L"edit", L"add", L"folder", L"file", L"up",
    L"shift", L"backspace", L"aux", L"photo", L"info", L"rew", L"fwd", L"pause" };

enum Action { A_NONE, A_SHELL, A_HOME, A_APPS, A_RUN, A_PREV, A_NEXT, A_NIGHT, A_EXIT,
              A_ADD, A_EDITMODE, A_FIELD, A_SAVE, A_DELETE, A_CANCEL, A_OK, A_KEY, A_SHIFT, A_LANG, A_BKSP,
              A_FILE, A_UP, A_SCREEN, A_LAUNCH };

// S_EDIT: one shortcut; S_KEYS: on-screen keyboard for its name/parameters; S_FILES: picking its exe.
enum Screen { S_HOME, S_APPS, S_EDIT, S_KEYS, S_FILES, S_RADIO, S_AUDIO, S_PHONE, S_VIDEO, S_PHOTO, S_SETTINGS, S_AUX, S_INFO };

struct Btn {
    RECT        rc;
    Icon        icon;
    const wchar_t* label;
    Action      action;
    bool        tile;      // big tile with a facet corner
    int         arg;       // A_RUN: index in s_apps; A_FIELD: F_*; A_KEY: character; A_FILE: index in s_files
};

static const Btn kHome[] = {
    // tiles 3 x 2, right part (concept v0.1): radio, audio, phone / programs, video, settings
    { { 316,  72, 464, 222 }, IC_RADIO, L"РАДИО",      A_SCREEN, true, S_RADIO },
    { { 476,  72, 624, 222 }, IC_MUSIC, L"АУДИО",      A_SCREEN, true, S_AUDIO },
    { { 636,  72, 784, 222 }, IC_PHONE, L"ТЕЛЕФОН",    A_SCREEN, true, S_PHONE },
    { { 316, 234, 464, 384 }, IC_APPS,  L"ПРИЛОЖЕНИЯ", A_LAUNCH, true },
    { { 476, 234, 624, 384 }, IC_VIDEO, L"ВИДЕО",      A_SCREEN, true, S_VIDEO },
    { { 636, 234, 784, 384 }, IC_GEAR,  L"НАСТРОЙКИ",  A_SCREEN, true, S_SETTINGS },
    // dock: home pill (120 px), then info, settings, day/night, photo, aux, radio, exit
    { {  16, 410, 136, 468 }, IC_HOME,  NULL, A_HOME,   false },
    { { 148, 410, 232, 468 }, IC_INFO,  NULL, A_SCREEN, false, S_INFO },
    { { 240, 410, 324, 468 }, IC_GEAR,  NULL, A_SCREEN, false, S_SETTINGS },
    { { 332, 410, 416, 468 }, IC_MOON,  NULL, A_NIGHT,  false },
    { { 424, 410, 508, 468 }, IC_PHOTO, NULL, A_SCREEN, false, S_PHOTO },
    { { 516, 410, 600, 468 }, IC_AUX,   NULL, A_SCREEN, false, S_AUX },
    { { 608, 410, 692, 468 }, IC_RADIO, NULL, A_SCREEN, false, S_RADIO },
    { { 700, 410, 784, 468 }, IC_CLOSE, NULL, A_EXIT,   false },
};

// Dock for every demo screen: back to the main menu + exit.
static const Btn kScreenDock[] = {
    { {  16, 410, 116, 468 }, IC_HOME,  NULL, A_HOME,  false },
    { { 676, 410, 784, 468 }, IC_CLOSE, NULL, A_EXIT, false },
};

static const Btn kAppsDock[] = {
    { {  16, 410, 116, 468 }, IC_HOME,  NULL, A_HOME,     false },
    { { 128, 410, 228, 468 }, IC_BACK,  NULL, A_PREV,     false },
    { { 340, 410, 440, 468 }, IC_NEXT,  NULL, A_NEXT,     false },
    { { 452, 410, 552, 468 }, IC_EDIT,  NULL, A_EDITMODE, false },
    { { 564, 410, 664, 468 }, IC_MOON,  NULL, A_NIGHT,    false },
    { { 676, 410, 784, 468 }, IC_CLOSE, NULL, A_EXIT,     false },
};

enum { F_NAME, F_PATH, F_PARAMS };   // fields of the shortcut editor

static const Btn kEdit[] = {
    { {  16,  86, 784, 156 }, IC_NONE, NULL, A_FIELD, false, F_NAME },
    { {  16, 166, 784, 236 }, IC_NONE, NULL, A_FIELD, false, F_PATH },
    { {  16, 246, 784, 316 }, IC_NONE, NULL, A_FIELD, false, F_PARAMS },
    { {  16, 410, 268, 468 }, IC_NONE, L"ОТМЕНА",    A_CANCEL, false },
    { { 544, 410, 784, 468 }, IC_NONE, L"СОХРАНИТЬ", A_SAVE,   false },
    { { 280, 410, 532, 468 }, IC_NONE, L"УДАЛИТЬ",   A_DELETE, false },   // last: only for an existing shortcut
};

static const Btn kFilesDock[] = {
    { {  16, 410, 136, 468 }, IC_UP,   NULL, A_UP,   false },
    { { 148, 410, 268, 468 }, IC_BACK, NULL, A_PREV, false },
    { { 412, 410, 532, 468 }, IC_NEXT, NULL, A_NEXT, false },
    { { 544, 410, 784, 468 }, IC_NONE, L"ОТМЕНА", A_CANCEL, false },
};

// On-screen keyboard: the text field with these two buttons on top, then 4 rows of 12 keys and a row of
// Shift, language, space, backspace.
static const Btn kKeysTop[] = {
    { { 572, 82, 677, 136 }, IC_NONE, L"ОТМЕНА", A_CANCEL, false },
    { { 682, 82, 787, 136 }, IC_NONE, L"ГОТОВО", A_OK,     false },
};
static const wchar_t* kKeysRu[4] = { L"1234567890-_", L"йцукенгшщзхъ", L"фывапролджэё", L"ячсмитьбю.,!" };
static const wchar_t* kKeysEn[4] = { L"1234567890-_", L"qwertyuiop()", L"asdfghjkl:;\"", L"zxcvbnm.,/\\=" };

// Programs of the "Apps" screen, from apps.txt: "Name | \path\to\program.exe | class!title | parameters".
struct App {
    wchar_t name[48];
    wchar_t path[MAX_PATH];
    wchar_t wclass[64];    // optional window to raise, "class!title" like AppLauncher; empty = any window of the process
    wchar_t wtitle[96];
    wchar_t params[200];   // command line passed to the program, may be empty
    HICON   icon;          // the program's own icon, NULL if it has none
    bool    exists;
};

// The stock shell stays topmost, so a started program must be raised above it explicitly.
// After the start its window is looked for every 300 ms for up to kWaitMs.
enum { TIMER_CLOCK = 1, TIMER_WAIT = 2, kWaitMs = 20000, kMaxRaised = 16 };
static DWORD s_waitPid;
static int   s_waitApp = -1;
static DWORD s_waitTick;
static HWND  s_raised[kMaxRaised];   // windows we made topmost; dropped again when the shell is shown
static int   s_raisedCount;
enum { kMaxApps = 64, kAppsPerPage = 8 };
static App   s_apps[kMaxApps];
static int   s_appCount;
static int   s_page;
static Screen s_screen = S_HOME;

// Shortcut editor: works on a copy, written to apps.txt on "Save".
static bool  s_editMode;          // apps screen: a tile opens its editor instead of starting the program
static int   s_editIdx;           // index in s_apps, -1 = new shortcut
static App   s_edit;
static bool  s_delArmed;          // "Delete" was pressed once; the second press deletes

static int     s_keysField;       // F_NAME or F_PARAMS
static wchar_t s_keysText[200];
static bool    s_keysEn, s_keysShift;

// File browser for the program path: folders and .exe files of s_fdir.
struct FileEnt { wchar_t name[100]; bool dir; };
enum { kMaxFiles = 256, kFilesPerPage = 10 };
static FileEnt s_files[kMaxFiles];
static int     s_fileCount, s_filePage;
static wchar_t s_fdir[MAX_PATH];  // ends with a backslash

static Btn   s_btns[64];          // buttons of the current screen
static int   kBtnCount;

// Skin pictures, premultiplied RGBA; px == NULL = the built-in drawing is used.
struct Img { int w, h; unsigned char* px; };
static Img     s_icons[IC_COUNT][2];   // [1] = pressed variant
static wchar_t s_skinBg[MAX_PATH];     // background picture; read again on every rebuild (night toggle)

static HWND    s_wnd;
static HBITMAP s_back;          // whole frame, drawn off-screen
static HBITMAP s_bg;            // triangle pattern + static parts, drawn once
static unsigned short* s_backBits;   // their pixels: RGB565, bottom-up, 800 per row
static unsigned short* s_bgBits;
static HFONT   s_fClock, s_fBig, s_fMid, s_fSmall, s_fTiny;
static int     s_pressed = -1;
static bool    s_night;
static wchar_t s_dir[MAX_PATH];
static wchar_t s_status[96];    // last action result, shown in the clock widget
#ifdef UNDER_CE
static HWND    s_taskbar;
#endif

static const wchar_t* kMonths[12] = { L"января", L"февраля", L"марта", L"апреля", L"мая", L"июня",
                                      L"июля", L"августа", L"сентября", L"октября", L"ноября", L"декабря" };
static const wchar_t* kDays[7] = { L"воскресенье", L"понедельник", L"вторник", L"среда", L"четверг",
                                   L"пятница", L"суббота" };

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

static void Round(HDC dc, const RECT& rc, int radius, COLORREF fill, COLORREF border)
{
    HBRUSH br = CreateSolidBrush(C(fill));
    HPEN pen = border == CLR_INVALID ? CreatePen(PS_NULL, 0, 0) : CreatePen(PS_SOLID, 2, C(border));
    HGDIOBJ ob = SelectObject(dc, br), op = SelectObject(dc, pen);
    RoundRect(dc, rc.left, rc.top, rc.right, rc.bottom, radius, radius);
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(br);
    DeleteObject(pen);
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

static void Text(HDC dc, HFONT f, COLORREF c, int l, int t, int r, int b, const wchar_t* s, UINT fmt)
{
    RECT rc = { l, t, r, b };
    HGDIOBJ of = SelectObject(dc, f);
    SetTextColor(dc, C(c));
    SetBkMode(dc, TRANSPARENT);
    DrawText(dc, s, -1, &rc, fmt | DT_NOPREFIX);
    SelectObject(dc, of);
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
    case IC_GEAR:
        p.Circle(24, 24, 7);
        p.Circle(24, 24, 13);
        p.L2(24, 5, 24, 11);
        p.L2(24, 37, 24, 43);
        p.L2(5, 24, 11, 24);
        p.L2(37, 24, 43, 24);
        p.L2(10.6, 10.6, 14.8, 14.8);
        p.L2(33.2, 33.2, 37.4, 37.4);
        p.L2(10.6, 37.4, 14.8, 33.2);
        p.L2(33.2, 14.8, 37.4, 10.6);
        break;
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
    case IC_EDIT: {   // pencil
        double v[] = { 9, 39, 11, 29, 31, 9, 39, 17, 19, 37, 9, 39 };
        p.Line(v, 6);
        p.L2(27, 13, 35, 21);
        break;
    }
    case IC_ADD:
        p.L2(24, 8, 24, 40);
        p.L2(8, 24, 40, 24);
        break;
    case IC_FOLDER: {
        double v[] = { 5, 11, 18, 11, 22, 16, 43, 16, 43, 39, 5, 39, 5, 11 };
        p.Line(v, 7);
        break;
    }
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
    default:
        break;
    }
}

// ---------------------------------------------------------------- skin pictures

#define RGB565(r, g, b) ((unsigned short)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))

// 800 x 480 RGB565 bottom-up DIB section: the unit's screen format, with pixels reachable for blending.
static HBITMAP MakeDib(unsigned short** bits)
{
    struct { BITMAPINFOHEADER h; DWORD masks[3]; } bi;
    memset(&bi, 0, sizeof(bi));
    bi.h.biSize = sizeof(BITMAPINFOHEADER);
    bi.h.biWidth = 800;
    bi.h.biHeight = 480;
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
static void DrawImg(const Img& im, int x, int y, int w, int h)
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

// The skin's picture of the icon if it has one, otherwise the built-in line drawing.
static void DrawGlyph(HDC dc, Icon ic, int x, int y, int size, bool down)
{
    const Img& im = s_icons[ic][down && s_icons[ic][1].px ? 1 : 0];
    if (im.px)
        DrawImg(im, x, y, size, size);
    else
        PaintIcon(dc, ic, x, y, size, down ? kInk : kText);
}

// ---------------------------------------------------------------- frame

static HFONT MakeFont(int height, int weight)
{
    LOGFONT lf;
    memset(&lf, 0, sizeof(lf));
    lf.lfHeight = -height;
    lf.lfWeight = weight;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = ANTIALIASED_QUALITY;
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

// Static background: graphite with the triangle mosaic, top bar line.
static void PaintBackground(HDC dc)
{
    Fill(dc, 0, 0, 800, 480, kInk);
    Triangles(dc, 0, 70, 800, 398, kTri, 3);
    Fill(dc, 83, 68, 717, 70, kLine);
    Fill(dc, 0, 398, 800, 400, kLine);
}

// The skin's 800 x 480 background picture, dimmed at night like everything else.
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
            c = C(c);
            dst[x] = RGB565(GetRValue(c), GetGValue(c), GetBValue(c));
        }
    }
    stbi_image_free(rgb);
    return true;
}

static void BuildBackground()
{
    if (!s_bg)
        s_bg = MakeDib(&s_bgBits);
    if (!s_bg || (s_skinBg[0] && LoadBackground()))
        return;
    HDC dc = CreateCompatibleDC(NULL);
    HGDIOBJ ob = SelectObject(dc, s_bg);
    PaintBackground(dc);
    SelectObject(dc, ob);
    DeleteDC(dc);
}

// Single line in the box; a line too long for it shows its end (paths, typed text). Returns where it ends.
static int TextFit(HDC dc, HFONT f, COLORREF c, int l, int t, int r, int b, const wchar_t* s)
{
    SIZE sz;
    HGDIOBJ of = SelectObject(dc, f);
    GetTextExtentPoint32(dc, s, (int)wcslen(s), &sz);
    SelectObject(dc, of);
    bool tail = sz.cx > r - l;
    Text(dc, f, c, l, t, r, b, s, (tail ? DT_RIGHT : DT_LEFT) | DT_SINGLELINE | DT_VCENTER);
    return tail ? r : l + sz.cx;
}

static const wchar_t* kFieldCaption[3] = { L"НАЗВАНИЕ", L"ПРОГРАММА", L"ПАРАМЕТРЫ ЗАПУСКА" };
static const wchar_t* kFieldEmpty[3] = { L"нажмите, чтобы ввести", L"нажмите, чтобы выбрать exe", L"нет" };

static void DrawButton(HDC dc, int i)
{
    const Btn& b = s_btns[i];
    bool down = i == s_pressed;
    if (b.tile) {
        bool editing = s_editMode && b.action == A_RUN;
        Round(dc, b.rc, 28, down ? kOrange : kPanel, down ? CLR_INVALID : (editing ? kOrange : kLine));
        if (!down) {   // orange facet in the top right corner, following the border's rounded corner (radius 14)
            POINT f[12];
            int n = 0;
            f[n].x = b.rc.right - 38; f[n].y = b.rc.top + 1; n++;
            for (int k = 0; k <= 8; k++, n++) {
                double a = (-90 + k * 90 / 8.0) * 3.14159265 / 180;
                f[n].x = b.rc.right - 14 + (int)(13 * cos(a) + 0.5);
                f[n].y = b.rc.top + 14 + (int)(13 * sin(a) + 0.5);
            }
            f[n].x = b.rc.right - 1; f[n].y = b.rc.top + 38; n++;
            Poly(dc, f, n, kFacet);
        }
        int cx = (b.rc.left + b.rc.right) / 2;
        if (b.action == A_RUN) {
            const App& a = s_apps[b.arg];
            if (a.icon) {
                DrawIconEx(dc, cx - 32, b.rc.top + 22, a.icon, 64, 64, 0, NULL, DI_NORMAL);
            } else {   // no icon in the program: its first letter in a ring
                wchar_t first[2] = { a.name[0], 0 };
                Pen ring(dc, cx - 32, b.rc.top + 22, 64, down ? kInk : kText);
                ring.Circle(24, 24, 20);
                Text(dc, s_fMid, down ? kInk : kText, cx - 32, b.rc.top + 22, cx + 32, b.rc.top + 86, first,
                     DT_CENTER | DT_SINGLELINE | DT_VCENTER);
            }
            Text(dc, s_fSmall, down ? kInk : (a.exists ? kMuted : kDim), b.rc.left + 6, b.rc.bottom - 44,
                 b.rc.right - 6, b.rc.bottom - 10, a.exists ? a.name : L"нет файла", DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        } else {
            DrawGlyph(dc, b.icon, cx - 30, b.rc.top + 24, 60, down);
            Text(dc, s_fSmall, down ? kInk : kMuted, b.rc.left, b.rc.bottom - 42, b.rc.right, b.rc.bottom - 12, b.label,
                 DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        }
    } else if (b.action == A_FIELD) {   // editor field: caption and value
        const wchar_t* v = b.arg == F_NAME ? s_edit.name : b.arg == F_PATH ? s_edit.path : s_edit.params;
        Round(dc, b.rc, 20, down ? kOrange : kPanel, down ? CLR_INVALID : kLine);
        Text(dc, s_fTiny, down ? kInk : kOrange, b.rc.left + 20, b.rc.top + 8, b.rc.right - 20, b.rc.top + 28,
             kFieldCaption[b.arg], DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        if (v[0])
            TextFit(dc, s_fMid, down ? kInk : kText, b.rc.left + 20, b.rc.top + 28, b.rc.right - 20, b.rc.bottom - 6, v);
        else
            Text(dc, s_fSmall, down ? kInk : kDim, b.rc.left + 20, b.rc.top + 28, b.rc.right - 20, b.rc.bottom - 6,
                 kFieldEmpty[b.arg], DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    } else if (b.action == A_FILE) {    // file browser row
        const FileEnt& f = s_files[b.arg];
        int cy = (b.rc.top + b.rc.bottom) / 2;
        Round(dc, b.rc, 16, down ? kOrange : kPanel, down ? CLR_INVALID : kLine);
        DrawGlyph(dc, f.dir ? IC_FOLDER : IC_FILE, b.rc.left + 12, cy - 16, 32, down);
        Text(dc, s_fSmall, down ? kInk : kText, b.rc.left + 56, b.rc.top, b.rc.right - 12, b.rc.bottom, f.name,
             DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    } else {
        // the "home" slot of the main menu lights up like the concept's dock pill
        bool home = b.action == A_HOME && s_screen == S_HOME;
        bool on = (b.action == A_EDITMODE && s_editMode) || (b.action == A_SHIFT && s_keysShift);
        Round(dc, b.rc, 20, down || home ? kOrange : kPanel, down || home ? CLR_INVALID : (on ? kOrange : kLine));
        int cx = (b.rc.left + b.rc.right) / 2, cy = (b.rc.top + b.rc.bottom) / 2;
        wchar_t key[2] = { (wchar_t)b.arg, 0 };
        const wchar_t* label = b.label;
        if (b.action == A_DELETE && s_delArmed)
            label = L"ТОЧНО УДАЛИТЬ?";
        else if (b.action == A_LANG)
            label = s_keysEn ? L"ENG" : L"РУС";
        else if (b.action == A_KEY && !label) {
            if (s_keysShift)
                CharUpperBuff(key, 1);
            label = key;
        }
        if (label) {
            COLORREF c = down ? kInk : (b.action == A_DELETE && s_delArmed) ? kOrange : kText;
            Text(dc, label == key ? s_fMid : s_fSmall, c, b.rc.left, b.rc.top, b.rc.right, b.rc.bottom, label,
                 DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        } else {
            Icon ic = (b.action == A_NIGHT && s_night) ? IC_SUN : b.icon;
            DrawGlyph(dc, ic, cx - 20, cy - 20, 40, down || home);
        }
    }
}

// Demo versions of the stock shell's other screens, drawn in the v0.1 style.
// Values are placeholders; layouts follow the measurements recorded in PROJECT.md.
static void DrawDemoScreen(HDC dc)
{
    switch (s_screen) {
    case S_RADIO: {
        RECT p = { 12, 82, 788, 352 };
        Round(dc, p, 28, kPanel, kLine);
        Text(dc, s_fTiny, kMuted, 36, 100, 300, 124, L"РАДИО · FM", DT_LEFT | DT_SINGLELINE);
        Text(dc, s_fBig, kText, 32, 130, 388, 210, L"106.2", DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        for (int r = 0; r < 2; r++)
            for (int c = 0; c < 4; c++) {
                RECT b = { 408 + c * 96, 96 + r * 132, 408 + c * 96 + 88, 96 + r * 132 + 112 };
                Round(dc, b, 20, kPanel2, kLine);
                Text(dc, s_fSmall, kText, b.left, b.top + 20, b.right, b.bottom - 20,
                     (r == 0 && c == 0) ? L"ДОРОЖНОЕ" : L"—", DT_CENTER | DT_SINGLELINE | DT_VCENTER);
            }
        break;
    }
    case S_AUDIO: {
        RECT cover = { 24, 90, 152, 218 };
        Round(dc, cover, 16, kFacet, CLR_INVALID);
        Triangles(dc, cover.left + 1, cover.top + 1, cover.right - 1, cover.bottom - 1, kTriOr, 4);
        Text(dc, s_fTiny, kOrange, 176, 96, 500, 118, L"СЕЙЧАС ИГРАЕТ", DT_LEFT | DT_SINGLELINE);
        Text(dc, s_fMid, kText, 174, 120, 780, 154, L"—", DT_LEFT | DT_SINGLELINE);
        Text(dc, s_fSmall, kMuted, 176, 158, 780, 182, L"нет данных", DT_LEFT | DT_SINGLELINE);
        Fill(dc, 176, 200, 772, 206, kLine);
        Fill(dc, 176, 200, 300, 206, kOrange);
        RECT list = { 24, 232, 780, 390 };
        Round(dc, list, 20, kPanel, kLine);
        for (int i = 0; i < 4; i++)
            Text(dc, s_fSmall, i == 0 ? kText : kMuted, 40, 244 + i * 36, 764, 244 + i * 36 + 32,
                 i == 0 ? L"Трек 1" : L"—", DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        break;
    }
    case S_PHONE: {
        Text(dc, s_fBig, kText, 24, 96, 460, 150, L"—", DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        Text(dc, s_fSmall, kMuted, 24, 156, 460, 184, L"последние вызовы", DT_LEFT | DT_SINGLELINE);
        const wchar_t* keys[12] = { L"1", L"2", L"3", L"4", L"5", L"6", L"7", L"8", L"9", L"*", L"0", L"#" };
        for (int i = 0; i < 12; i++) {
            int c = i % 3, r = i / 3;
            RECT b = { 24 + c * 112, 200 + r * 68, 24 + c * 112 + 104, 200 + r * 68 + 60 };
            Round(dc, b, 16, kPanel, kLine);
            Text(dc, s_fMid, kText, b.left, b.top, b.right, b.bottom, keys[i], DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        }
        break;
    }
    case S_VIDEO: {
        RECT p = { 24, 90, 776, 330 };
        Round(dc, p, 20, kPanel, kLine);
        Text(dc, s_fMid, kMuted, p.left, p.top, p.right, p.bottom, L"ВИДЕО", DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        break;
    }
    case S_PHOTO: {
        RECT p = { 24, 90, 776, 330 };
        Round(dc, p, 20, kPanel, kLine);
        Text(dc, s_fMid, kMuted, p.left, p.top, p.right, p.bottom, L"ФОТО", DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        break;
    }
    case S_SETTINGS: {
        const wchar_t* sec[4] = { L"Экран", L"Звук", L"Связь", L"Система" };
        for (int i = 0; i < 4; i++) {
            RECT b = { 24, 90 + i * 60, 260, 90 + i * 60 + 50 };
            Round(dc, b, 16, i == 0 ? kOrange : kPanel, i == 0 ? CLR_INVALID : kLine);
            Text(dc, s_fSmall, i == 0 ? kInk : kText, b.left, b.top, b.right, b.bottom,
                 sec[i], DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        }
        RECT p = { 284, 90, 776, 390 };
        Round(dc, p, 20, kPanel, kLine);
        Text(dc, s_fSmall, kMuted, 304, 106, 756, 130, L"Яркость", DT_LEFT | DT_SINGLELINE);
        Fill(dc, 304, 140, 756, 146, kLine);
        Fill(dc, 304, 140, 520, 146, kOrange);
        break;
    }
    case S_AUX: {
        RECT p = { 24, 90, 776, 330 };
        Round(dc, p, 20, kPanel, kLine);
        Text(dc, s_fMid, kMuted, p.left, p.top, p.right, p.bottom, L"AUX", DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        break;
    }
    case S_INFO: {
        RECT p = { 24, 90, 776, 330 };
        Round(dc, p, 20, kPanel, kLine);
        Text(dc, s_fMid, kMuted, p.left, p.top, p.right, p.bottom, L"ИНФОРМАЦИЯ", DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        break;
    }
    default:
        break;
    }
}

static void DrawFrame(HDC dc)
{
    HDC bg = CreateCompatibleDC(dc);
    HGDIOBJ ob = SelectObject(bg, s_bg);
    BitBlt(dc, 0, 0, 800, 480, bg, 0, 0, SRCCOPY);
    SelectObject(bg, ob);
    DeleteDC(bg);

    SYSTEMTIME t;
    GetLocalTime(&t);
    wchar_t s[64];

    // top bar: clock in the centre, like the stock shell
    _snwprintf(s, 64, L"%02d:%02d", t.wHour, t.wMinute);
    Text(dc, s_fBig, kText, 0, 8, 800, 62, s, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
    const wchar_t* title = L"VESTA HOME · ТЕСТ 0.3";
    switch (s_screen) {
    case S_APPS:  title = s_editMode ? L"ПРИЛОЖЕНИЯ · ИЗМЕНЕНИЕ" : L"ПРИЛОЖЕНИЯ"; break;
    case S_EDIT:  title = s_editIdx < 0 ? L"НОВЫЙ ЯРЛЫК" : L"ИЗМЕНИТЬ ЯРЛЫК"; break;
    case S_KEYS:  title = s_keysField == F_NAME ? L"НАЗВАНИЕ" : L"ПАРАМЕТРЫ ЗАПУСКА"; break;
    case S_FILES: title = L"ВЫБОР ПРОГРАММЫ"; break;
    case S_RADIO:    title = L"РАДИО"; break;
    case S_AUDIO:    title = L"АУДИО"; break;
    case S_PHONE:    title = L"ТЕЛЕФОН"; break;
    case S_VIDEO:    title = L"ВИДЕО"; break;
    case S_PHOTO:    title = L"ФОТО"; break;
    case S_SETTINGS: title = L"НАСТРОЙКИ"; break;
    case S_AUX:      title = L"AUX"; break;
    case S_INFO:     title = L"ИНФОРМАЦИЯ"; break;
    default:      break;
    }
    Text(dc, s_fTiny, kOrange, 20, 8, 300, 62, title, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    if (s_screen >= S_RADIO) {   // demo screens of the stock shell, redrawn in the v0.1 style
        DrawDemoScreen(dc);
        for (int i = 0; i < kBtnCount; i++)
            DrawButton(dc, i);
        return;
    }

    if (s_screen != S_HOME) {
        if (s_screen == S_APPS) {
            if (s_appCount == 0) {
                Text(dc, s_fMid, kText, 16, 260, 784, 300, L"Список приложений пуст", DT_CENTER | DT_SINGLELINE);
                Text(dc, s_fSmall, kMuted, 16, 310, 784, 340, L"Нажмите «Добавить» или впишите программы в apps.txt",
                     DT_CENTER | DT_SINGLELINE);
            }
            _snwprintf(s, 64, L"%d / %d", s_page + 1, AppsPages());
            Text(dc, s_fMid, kText, 228, 410, 340, 468, s, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        } else if (s_screen == S_FILES) {
            TextFit(dc, s_fSmall, kMuted, 20, 76, 780, 108, s_fdir);
            if (s_fileCount == 0)
                Text(dc, s_fMid, kText, 16, 200, 784, 240, L"Здесь нет папок и программ", DT_CENTER | DT_SINGLELINE);
            _snwprintf(s, 64, L"%d / %d", s_filePage + 1, FilePages());
            Text(dc, s_fMid, kText, 268, 410, 412, 468, s, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        } else if (s_screen == S_KEYS) {
            Fill(dc, 0, 70, 800, 480, kInk);   // plain plate under the keys
            RECT f = { 12, 82, 566, 136 };
            Round(dc, f, 20, kPanel, kOrange);
            int end = TextFit(dc, s_fMid, kText, 28, 82, 550, 136, s_keysText);
            Fill(dc, end + 2, 94, end + 5, 124, kOrange);   // caret
        }
        if (s_status[0])
            Text(dc, s_fTiny, kMuted, 300, 8, 780, 62, s_status, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
        for (int i = 0; i < kBtnCount; i++)
            DrawButton(dc, i);
        return;
    }

    // widget 1: "now playing" (concept v0.1: album art, track, progress, transport).
    // The stock shell draws its own player, so the values here are placeholders until we read them.
    RECT w1 = { 16, 72, 300, 268 };
    Round(dc, w1, 28, kPanel, kLine);
    RECT art = { 32, 88, 120, 176 };
    Round(dc, art, 16, kFacet, CLR_INVALID);
    Triangles(dc, art.left + 1, art.top + 1, art.right - 1, art.bottom - 1, kTriOr, 4);   // orange mosaic cover
    Text(dc, s_fTiny, kOrange, 136, 90, 292, 112, L"СЕЙЧАС ИГРАЕТ", DT_LEFT | DT_SINGLELINE);
    Text(dc, s_fMid, kText, 134, 112, 292, 146, L"—", DT_LEFT | DT_SINGLELINE);
    Text(dc, s_fSmall, kMuted, 136, 148, 292, 172, L"нет данных", DT_LEFT | DT_SINGLELINE);
    Fill(dc, 32, 196, 284, 202, kLine);   // progress track
    Fill(dc, 32, 196, 124, 202, kOrange); // played part
    DrawGlyph(dc, IC_PREV, 52, 216, 36, false);
    RECT play = { 134, 210, 182, 258 };
    Round(dc, play, 48, kOrange, CLR_INVALID);   // play/pause circle
    const Img& pim = s_icons[IC_PAUSE][0];
    if (pim.px)
        DrawImg(pim, 141, 217, 34, 34);
    else
        PaintIcon(dc, IC_PAUSE, 141, 217, 34, kInk);
    DrawGlyph(dc, IC_FWD, 228, 216, 36, false);

    // widget 2: radio (concept v0.1)
    RECT w2 = { 16, 280, 300, 384 };
    Round(dc, w2, 28, kPanel, kLine);
    Text(dc, s_fTiny, kMuted, 32, 292, 290, 314, L"РАДИО · FM", DT_LEFT | DT_SINGLELINE);
    Text(dc, s_fBig, kText, 30, 312, 210, 372, L"—.—", DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    Text(dc, s_fSmall, kMuted, 150, 336, 292, 366, L"нет данных", DT_RIGHT | DT_SINGLELINE | DT_VCENTER);

    // date (or the last action) in the free right corner of the top bar
    if (s_status[0]) {
        Text(dc, s_fSmall, kMuted, 456, 8, 784, 62, s_status, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
    } else {
        _snwprintf(s, 64, L"%d %s · %s", t.wDay, kMonths[(t.wMonth + 11) % 12], kDays[t.wDayOfWeek % 7]);
        Text(dc, s_fSmall, kMuted, 456, 8, 784, 62, s, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
    }

    for (int i = 0; i < kBtnCount; i++)
        DrawButton(dc, i);
}

static void Redraw()
{
    HDC hdc = GetDC(s_wnd);
    HDC dc = CreateCompatibleDC(hdc);
    HGDIOBJ ob = SelectObject(dc, s_back);
    DrawFrame(dc);
    BitBlt(hdc, 0, 0, 800, 480, dc, 0, 0, SRCCOPY);
    SelectObject(dc, ob);
    DeleteDC(dc);
    ReleaseDC(s_wnd, hdc);
}

// ---------------------------------------------------------------- actions

static bool Exists(const wchar_t* path)
{
    return GetFileAttributes(path) != 0xFFFFFFFF;
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

#define APPS_HEADER \
    "# Programs of the \"Apps\" screen: one per line, \"Name | full path to the exe\".\r\n" \
    "# Lines starting with # are ignored. Save the file as UTF-8.\r\n" \
    "# The started program is raised above the stock shell. If it starts another program (a launcher,\r\n" \
    "# a MortScript), name the window to raise as the third field: \"Name | path | class!title\".\r\n" \
    "# The fourth field is the program's command line: \"Name | path | | parameters\" (third field may be empty).\r\n" \
    "# Shortcuts changed on the \"Apps\" screen rewrite this file; comments other than these are not kept.\r\n"

static const char kDefaultApps[] =
    APPS_HEADER
    "\xD0\xAF\xD0\xBD\xD0\xB4\xD0\xB5\xD0\xBA\xD1\x81 \xD0\xBA\xD0\xB0\xD1\x80\xD1\x82\xD1\x8B | \\SDMMC\\YaMapsCE\\YaMapsCE.exe\r\n"
    "OSM \xD0\xBA\xD0\xB0\xD1\x80\xD1\x82\xD1\x8B | \\SDMMC\\OSMapsCE\\OSMapsCE.exe\r\n"
    "CityGuide | \\SDMMC\\Navi\\CityGuide\\CityGuideCE.exe\r\n";

// Reads apps.txt (UTF-8, optional BOM); writes an example list on the first start.
static void LoadApps()
{
    for (int i = 0; i < s_appCount; i++)
        if (s_apps[i].icon)
            DestroyIcon(s_apps[i].icon);
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
        // optional fourth: the program's command line
        a.wclass[0] = a.wtitle[0] = a.params[0] = 0;
        wchar_t* bar2 = wcschr(bar + 1, L'|');
        if (bar2) {
            *bar2 = 0;
            wchar_t* w = bar2 + 1;
            wchar_t* bar3 = wcschr(w, L'|');
            if (bar3) {
                *bar3 = 0;
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
        a.icon = NULL;
        if (a.exists)
            ExtractIconEx(a.path, 0, &a.icon, NULL, 1);
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
        wchar_t win[170], line[700];
        if (a.wtitle[0])
            _snwprintf(win, 170, L"!%s!%s", a.wclass, a.wtitle);
        else
            wcscpy(win, a.wclass);
        if (a.params[0])
            _snwprintf(line, 700, L"%s | %s | %s | %s\r\n", a.name, a.path, win, a.params);
        else if (win[0])
            _snwprintf(line, 700, L"%s | %s | %s\r\n", a.name, a.path, win);
        else
            _snwprintf(line, 700, L"%s | %s\r\n", a.name, a.path);
        line[699] = 0;
        char out[2100];
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

static void RunApp(int i)
{
    const App& a = s_apps[i];
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
        CloseHandle(pi.hProcess);
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
struct FindApp { HWND found; };

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
        s_status[0] = 0;
        s_waitApp = -1;
        ShowWindow(s_wnd, SW_HIDE);   // the program is on top now; the shell starts us again on "Navigation"
    } else if (GetTickCount() - s_waitTick > kWaitMs) {
        KillTimer(s_wnd, TIMER_WAIT);
        _snwprintf(s_status, 96, L"окно не найдено: %s", s_apps[s_waitApp].name);
        s_waitApp = -1;
    }
}

// ---------------------------------------------------------------- shortcut editor

static void OpenEditor(int i)
{
    s_editIdx = i;
    if (i >= 0)
        s_edit = s_apps[i];
    else
        memset(&s_edit, 0, sizeof(s_edit));
    s_edit.icon = NULL;   // the icon stays owned by s_apps
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
        HICON icon = s_apps[s_editIdx].icon;
        s_apps[s_editIdx] = s_edit;
        s_apps[s_editIdx].icon = icon;
    }
    FinishEdit();
}

static void DeleteEdit()
{
    if (s_apps[s_editIdx].icon)
        DestroyIcon(s_apps[s_editIdx].icon);
    memmove(&s_apps[s_editIdx], &s_apps[s_editIdx + 1], (s_appCount - s_editIdx - 1) * sizeof(App));
    s_appCount--;
    s_waitApp = -1;   // indices moved: stop waiting for a window of a started program
    s_delArmed = false;
    FinishEdit();
}

static void OpenKeys(int field)
{
    s_keysField = field;
    wcscpy(s_keysText, field == F_NAME ? s_edit.name : s_edit.params);
    s_keysShift = false;
    s_screen = S_KEYS;
}

static void KeyType(wchar_t ch)
{
    int n = (int)wcslen(s_keysText);
    if (n >= (s_keysField == F_NAME ? 47 : 199))
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

static void KeysDone()
{
    wchar_t* dst = s_keysField == F_NAME ? s_edit.name : s_edit.params;
    wcscpy(dst, s_keysText);
    Trim(dst);
    s_screen = S_EDIT;
}

static int CmpFiles(const void* pa, const void* pb)
{
    const FileEnt* a = (const FileEnt*)pa;
    const FileEnt* b = (const FileEnt*)pb;
    if (a->dir != b->dir)
        return a->dir ? -1 : 1;
    return _wcsicmp(a->name, b->name);
}

// Folders and .exe files of s_fdir, folders first.
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
        if (!dir && (n < 5 || _wcsicmp(fd.cFileName + n - 4, L".exe")))
            continue;
        s_files[s_fileCount].dir = dir;
        wcscpy(s_files[s_fileCount].name, fd.cFileName);
        s_fileCount++;
    } while (s_fileCount < kMaxFiles && FindNextFile(h, &fd));
    FindClose(h);
    qsort(s_files, s_fileCount, sizeof(FileEnt), CmpFiles);
}

// The browser opens in the folder of the current program, otherwise at the root.
static void OpenFiles()
{
    wcscpy(s_fdir, L"\\");
    wchar_t* slash = wcsrchr(s_edit.path, L'\\');
    if (slash && slash > s_edit.path) {
        wchar_t dir[MAX_PATH];
        int n = (int)(slash - s_edit.path);
        wcsncpy(dir, s_edit.path, n);
        dir[n] = 0;
        DWORD attr = GetFileAttributes(dir);
        if (attr != 0xFFFFFFFF && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
            wcscpy(s_fdir, dir);
            wcscat(s_fdir, L"\\");
        }
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

// ---------------------------------------------------------------- buttons

static void AddButtons(const Btn* b, int n)
{
    for (int i = 0; i < n; i++)
        s_btns[kBtnCount++] = b[i];
}

// Keyboard key spanning columns c0..c1-1 of a 12-column grid.
static void AddKey(int c0, int c1, int row, Icon icon, const wchar_t* label, Action action, int arg)
{
    Btn& b = s_btns[kBtnCount++];
    b.rc.left = 12 + c0 * 65;
    b.rc.right = 12 + c1 * 65 - 5;
    b.rc.top = 148 + row * 64;
    b.rc.bottom = b.rc.top + 58;
    b.icon = icon;
    b.label = label;
    b.action = action;
    b.tile = false;
    b.arg = arg;
}

// Buttons of the current screen: the fixed home set, app tiles 4 x 2 of the current page plus the dock,
// the editor, the keyboard, or the file list (2 columns x 5) plus its dock.
static void BuildButtons()
{
    kBtnCount = 0;
    switch (s_screen) {
    case S_HOME:
        AddButtons(kHome, sizeof(kHome) / sizeof(kHome[0]));
        return;
    case S_RADIO: case S_AUDIO: case S_PHONE: case S_VIDEO:
    case S_PHOTO: case S_SETTINGS: case S_AUX: case S_INFO:
        AddButtons(kScreenDock, sizeof(kScreenDock) / sizeof(kScreenDock[0]));
        return;
    case S_EDIT:
        AddButtons(kEdit, s_editIdx < 0 ? 5 : 6);   // no "Delete" for a new shortcut
        return;
    case S_KEYS: {
        AddButtons(kKeysTop, 2);
        const wchar_t** rows = s_keysEn ? kKeysEn : kKeysRu;
        for (int r = 0; r < 4; r++)
            for (int c = 0; rows[r][c]; c++)
                AddKey(c, c + 1, r, IC_NONE, NULL, A_KEY, rows[r][c]);
        AddKey(0, 2, 4, IC_SHIFT, NULL, A_SHIFT, 0);
        AddKey(2, 4, 4, IC_NONE, NULL, A_LANG, 0);
        AddKey(4, 10, 4, IC_NONE, L"ПРОБЕЛ", A_KEY, L' ');
        AddKey(10, 12, 4, IC_BKSP, NULL, A_BKSP, 0);
        return;
    }
    case S_FILES: {
        int first = s_filePage * kFilesPerPage;
        for (int k = 0; k < kFilesPerPage && first + k < s_fileCount; k++) {
            Btn& b = s_btns[kBtnCount++];
            b.rc.left = 16 + (k / 5) * 390;
            b.rc.right = b.rc.left + 378;
            b.rc.top = 112 + (k % 5) * 56;
            b.rc.bottom = b.rc.top + 50;
            b.icon = IC_NONE;
            b.label = NULL;
            b.action = A_FILE;
            b.tile = false;
            b.arg = first + k;
        }
        AddButtons(kFilesDock, sizeof(kFilesDock) / sizeof(kFilesDock[0]));
        return;
    }
    default:
        break;
    }
    int first = s_page * kAppsPerPage;
    int tiles = s_appCount + (s_appCount < kMaxApps ? 1 : 0);   // the last one is "Add"
    for (int k = 0; k < kAppsPerPage && first + k < tiles; k++) {
        Btn b;
        int col = k % 4, row = k / 4;
        b.rc.left = 16 + col * 194;
        b.rc.right = b.rc.left + 182;
        b.rc.top = 86 + row * 154;
        b.rc.bottom = b.rc.top + 144;
        b.icon = IC_NONE;
        b.label = NULL;
        b.action = A_RUN;
        b.tile = true;
        b.arg = first + k;
        if (first + k == s_appCount) {
            b.icon = IC_ADD;
            b.label = L"ДОБАВИТЬ";
            b.action = A_ADD;
        }
        s_btns[kBtnCount++] = b;
    }
    AddButtons(kAppsDock, sizeof(kAppsDock) / sizeof(kAppsDock[0]));
}

// The stock shell keeps running underneath: bring its window back and step aside.
// Its window is the biggest visible top-level window that is not ours.
struct FindShell { HWND best; int area; };

static BOOL CALLBACK EnumShell(HWND h, LPARAM lp)
{
    FindShell* f = (FindShell*)lp;
    if (h == s_wnd || !IsWindowVisible(h))
        return TRUE;
    for (int i = 0; i < s_raisedCount; i++)
        if (h == s_raised[i])
            return TRUE;
#ifdef UNDER_CE
    if (h == s_taskbar)
        return TRUE;
#endif
    RECT rc;
    GetWindowRect(h, &rc);
    int area = (rc.right - rc.left) * (rc.bottom - rc.top);
    if (area > f->area) {
        f->area = area;
        f->best = h;
    }
    return TRUE;
}

static void ShowShell()
{
    // programs we raised would stay above the shell: put them back into the normal z-order
    for (int i = 0; i < s_raisedCount; i++)
        if (IsWindow(s_raised[i]))
            SetWindowPos(s_raised[i], HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    FindShell f = { NULL, 0 };
    EnumWindows(EnumShell, (LPARAM)&f);
    s_raisedCount = 0;
    if (f.best && f.area >= 640 * 400) {
        ShowWindow(s_wnd, SW_HIDE);
        SetForegroundWindow(f.best);
        s_status[0] = 0;
    } else {
        wcscpy(s_status, L"оболочка не найдена");
    }
}

// "Приложения" tile: launch the separate VestaLaunch launcher (the ARM build from
// \SDMMC on the head unit, the desktop build relative to this exe on a PC).
static void LaunchVestaLaunch()
{
    wchar_t path[MAX_PATH];
#ifdef UNDER_CE
    wcscpy(path, L"\\SDMMC\\VestaLaunch\\VestaLaunch.exe");
#else
    _snwprintf(path, MAX_PATH, L"%s..\\..\\VestaLaunch\\VestaLaunch\\VestaLaunchPC.exe", s_dir);
    path[MAX_PATH - 1] = 0;
#endif
    PROCESS_INFORMATION pi;
    memset(&pi, 0, sizeof(pi));
#ifdef UNDER_CE
    LPSTARTUPINFO si = NULL;
    wchar_t* args = NULL;
#else
    STARTUPINFO sib;
    memset(&sib, 0, sizeof(sib));
    sib.cb = sizeof(sib);
    LPSTARTUPINFO si = &sib;
    wchar_t cmd[MAX_PATH + 4];
    _snwprintf(cmd, MAX_PATH + 4, L"\"%s\"", path);
    wchar_t* args = cmd;
#endif
    if (CreateProcess(path, args, NULL, NULL, FALSE, 0, NULL, NULL, si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        wcscpy(s_status, L"запущен VestaLaunch");
    } else {
        wcscpy(s_status, L"VestaLaunch не найден");
    }
}

static void DoAction(const Btn& b)
{
    if (b.action != A_DELETE)
        s_delArmed = false;
    switch (b.action) {
    case A_SHELL:    ShowShell(); break;
    case A_RUN:      if (s_editMode) OpenEditor(b.arg); else RunApp(b.arg); break;
    case A_ADD:      OpenEditor(-1); break;
    case A_HOME:     s_screen = S_HOME; s_editMode = false; s_status[0] = 0; break;
    case A_APPS:     LoadApps(); s_screen = S_APPS; s_page = 0; s_editMode = false; s_status[0] = 0; break;
    case A_PREV:
        if (s_screen == S_FILES) {
            if (s_filePage > 0)
                s_filePage--;
        } else if (s_page > 0) {
            s_page--;
        }
        break;
    case A_NEXT:
        if (s_screen == S_FILES) {
            if (s_filePage + 1 < FilePages())
                s_filePage++;
        } else if (s_page + 1 < AppsPages()) {
            s_page++;
        }
        break;
    case A_NIGHT:    s_night = !s_night; BuildBackground(); break;
    case A_SCREEN:   s_screen = (Screen)b.arg; break;
    case A_LAUNCH:   LaunchVestaLaunch(); break;
    case A_EXIT:     DestroyWindow(s_wnd); return;
    case A_EDITMODE: s_editMode = !s_editMode; break;
    case A_FIELD:    if (b.arg == F_PATH) OpenFiles(); else OpenKeys(b.arg); break;
    case A_SAVE:     SaveEdit(); break;
    case A_DELETE:   if (s_delArmed) DeleteEdit(); else s_delArmed = true; break;
    case A_CANCEL:   // editor -> apps, keyboard/browser -> editor
        if (s_screen == S_EDIT) {
            s_screen = S_APPS;
            s_status[0] = 0;
        } else {
            s_screen = S_EDIT;
        }
        break;
    case A_OK:       KeysDone(); break;
    case A_KEY:      KeyType((wchar_t)b.arg); break;
    case A_SHIFT:    s_keysShift = !s_keysShift; break;
    case A_LANG:     s_keysEn = !s_keysEn; break;
    case A_BKSP:     KeyBack(); break;
    case A_FILE:     PickFile(b.arg); break;
    case A_UP:       UpDir(); break;
    default:         break;
    }
    BuildButtons();
}

static int HitTest(int x, int y)
{
    for (int i = 0; i < kBtnCount; i++) {
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

// Writes the built-in graphics to Skin\: background.png, every icon as <name>.png and <name>_down.png
// (64 x 64, drawn 4x larger white on black and averaged into the alpha channel: antialiased lines),
// and skin.ini with the built-in colours. Doesn't read the existing skin.ini.
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

    static unsigned char out[800 * 480 * 3];
    PaintBackground(dc);
    GdiFlush();
    for (int i = 0; i < 800 * 480; i++) {
        out[i * 3] = (unsigned char)(px[i] >> 16);
        out[i * 3 + 1] = (unsigned char)(px[i] >> 8);
        out[i * 3 + 2] = (unsigned char)px[i];
    }
    _snwprintf(path, MAX_PATH, L"%sbackground.png", dir);
    WritePng(path, out, 800, 480, 3);

    for (int ic = 0; ic < IC_COUNT; ic++) {
        if (!kIconKey[ic])
            continue;
        Fill(dc, 0, 0, 256, 256, RGB(0, 0, 0));
        PaintIcon(dc, (Icon)ic, 0, 0, 256, RGB(255, 255, 255));
        GdiFlush();
        for (int down = 0; down < 2; down++) {
            COLORREF c = down ? kInk : kText;
            for (int y = 0; y < 64; y++)
                for (int x = 0; x < 64; x++) {
                    int sum = 0;
                    for (int j = 0; j < 4; j++)
                        for (int i = 0; i < 4; i++)
                            sum += (px[(y * 4 + j) * 800 + x * 4 + i] >> 16) & 255;
                    unsigned char* o = out + (y * 64 + x) * 4;
                    o[0] = GetRValue(c);
                    o[1] = GetGValue(c);
                    o[2] = GetBValue(c);
                    o[3] = (unsigned char)(sum / 16);
                }
            _snwprintf(path, MAX_PATH, L"%s%s%s.png", dir, kIconKey[ic], down ? L"_down" : L"");
            WritePng(path, out, 64, 64, 4);
        }
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
               "; Icons are square and drawn scaled: 60 px on tiles, 40 px on buttons, 32 px in the file list.\n"
               "; <name>_down is the pressed variant (optional). background is 800 x 480.\n"
               "; A missing line or file falls back to the built-in drawing.\n\n[colors]\n");
    for (int k = 0; k < (int)(sizeof(kSkinColors) / sizeof(kSkinColors[0])); k++) {
        COLORREF c = *kSkinColors[k].c;
        fprintf(f, "%-8S= %02X%02X%02X%s%s\n", kSkinColors[k].key, GetRValue(c), GetGValue(c), GetBValue(c),
                kSkinColors[k].note[0] ? "   ; " : "", kSkinColors[k].note);
    }
    fprintf(f, "\n[images]\nbackground = background.png\n");
    for (int ic = 0; ic < IC_COUNT; ic++)
        if (kIconKey[ic])
            fprintf(f, "%S = %S.png\n%S_down = %S_down.png\n", kIconKey[ic], kIconKey[ic], kIconKey[ic], kIconKey[ic]);
    fclose(f);
}
#endif

// ---------------------------------------------------------------- window

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE: {
        s_wnd = hwnd;
        s_back = MakeDib(&s_backBits);
        LoadSkin();
        BuildBackground();
        s_fClock = MakeFont(72, FW_BOLD);
        s_fBig = MakeFont(38, FW_SEMIBOLD);
        s_fMid = MakeFont(26, FW_SEMIBOLD);
        s_fSmall = MakeFont(17, FW_SEMIBOLD);
        s_fTiny = MakeFont(14, FW_BOLD);
#ifdef UNDER_CE
        s_taskbar = FindWindow(L"HHTaskBar", NULL);
        if (s_taskbar)
            ShowWindow(s_taskbar, SW_HIDE);
#endif
        BuildButtons();
        SetTimer(hwnd, 1, 1000, NULL);
        return 0;
    }
    case WM_TIMER:
        if (wp == TIMER_WAIT)
            OnWaitTimer();
        if (IsWindowVisible(hwnd))
            Redraw();
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        Redraw();
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_LBUTTONDOWN:
        s_pressed = HitTest((short)LOWORD(lp), (short)HIWORD(lp));
        SetCapture(hwnd);
        Redraw();
        return 0;
    case WM_LBUTTONUP: {
        ReleaseCapture();
        int i = HitTest((short)LOWORD(lp), (short)HIWORD(lp));
        int was = s_pressed;
        s_pressed = -1;
        Redraw();
        if (i >= 0 && i == was) {
            DoAction(s_btns[i]);
            if (IsWindow(hwnd) && IsWindowVisible(hwnd))
                Redraw();
        }
        return 0;
    }
#ifndef UNDER_CE
    case WM_CHAR:      // PC test: typing on the keyboard screen; Enter = done, Esc = cancel
        if (s_screen != S_KEYS)
            return 0;
        if (wp == VK_ESCAPE)
            s_screen = S_EDIT;
        else if (wp == VK_RETURN)
            KeysDone();
        else if (wp == VK_BACK)
            KeyBack();
        else if (wp >= 32)
            KeyType((wchar_t)wp);
        BuildButtons();
        Redraw();
        return 0;
    case WM_KEYDOWN:   // PC test: N = night, Esc = exit
        if (s_screen == S_KEYS)
            return 0;
        if (wp == 'N') {
            Btn night = kHome[9];   // dock slot 9 = day/night (IC_MOON)
            DoAction(night);
            Redraw();
        }
        else if (wp == VK_ESCAPE)
            DestroyWindow(hwnd);
        return 0;
#endif
    case WM_DESTROY:
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
#ifndef UNDER_CE
    if (wcsstr(cmd, L"/export")) {
        ExportSkin();
        return 0;
    }
#endif

    // The shell starts us again on every "Navigation" press: show the running copy instead.
    HWND prev = FindWindow(APP_CLASS, NULL);
    if (prev) {
        ShowWindow(prev, SW_SHOW);
        SetWindowPos(prev, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        SetForegroundWindow(prev);
        return 0;
    }

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
