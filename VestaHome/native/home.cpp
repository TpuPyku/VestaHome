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
#ifdef UNDER_CE
#include <ras.h>        // the USB modem's dial-up (vh_gps.inc ModemNet)
#include <raserror.h>   // (its results: ERROR_PORT_DISCONNECTED, 619)
#ifdef UNDER_CE
#include <tapi.h>       // its dialling location (ModemCarLocation)
#endif
#endif
#include "dialloc.h"    // its number as a local call; the modem ready to dial
#include "vhttp.h"     // VestaCore's HTTP(S) client (HttpInit at the start)
#include "vhap.h"      // VestaCore: the add-ons' protocol
#include "drawwhy.h"   // who asks for the full frames (the load line)
#define Redraw() RedrawFrom(__FILE__, __LINE__)
#include "vnav.h"      // VestaCore: the route's next manoeuvre from OSMapsCE, its sign
#include "vfont.h"     // VestaCore: the fonts
#include "netcat.h"    // the internet radio's lines and catalogue
#include "netcache.h"  // its lists kept in files
#include "country.h"   // the country by GPS (the catalogue's "Рядом")
#include "vjson.h"     // VestaCore: JSON (the catalogue's answers, in netcat.cpp)
#include "vlicense.h"  // VestaCore: the support license (its ID, its file)
#include "video.h"
#include "wifi.h"
#include "inikey.h"     // Navitel's GPSEpoch (vh_apps.inc)
#include "obd.h"
#include "btrf.h"     // its direct channel: the link brought up by an SDP search
#include "btowner.h"   // Bluetooth for the phone or the OBD adapter, one at a time
#include "trackend.h"  // a track ended: the next one only while the sound is ours
#include "srcwatch.h"   // a source change: ours or the user's (MODE)
#include "numpad.h"    // the digits' keyboard (km, coordinates, a date)
#include "service.h"   // the car service (ТО) due by km or a year
#include "presetpos.h"  // the preset playing: the MCU names its place on a page only
#include "zonepart.h"  // a partial frame's buttons against the full frame's
#include "vsecret.h"
#include "soundlost.h"
#include "pbcache.h"   // the phone book kept beside VestaHome, sealed
#include "netmirror.h" // the radio catalogue's mirror (VestaCore's release "radio")
#include "vrvc.h"   // VestaCore: the rear view camera
#include "vtouch.h"   // VestaCore: the finger on the resistive panel, filtered
#include "touchdrag.h"   // what its drag moves, whether the page turns
#include "vlogdir.h"   // VestaCore: logs only on the SD card / USB disk
#include "ramlog.h"    // the log in RAM while no card is there
#include "vnight.h"    // VestaCore: the night mode shared with OSMapsCE
#include "vkeys.h"     // VestaCore: the keyboard's text and cursor
#include "vfavs.h"     // VestaCore: OSMapsCE's favourite places (its map widget before it runs)
#include "mdm.h"       // VestaModem: the operators' presets (Settings -> Network)
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
#include "musdir.h"
#include "tagtext.h"   // tags in Windows-1251 read as Latin-1
#include "cardpic.h"   // the skin's card picture: the mask, the pixel, the blend
#include "camlimit.h"  // a speed camera's limit: fillers (5, 160) are none
#include "slide.h"     // a page sliding over the still background
#include "cover.h"     // the track's cover: in its tag or its folder
#include "radiodb.h"   // the FM station by the city and the frequency, its logo by its name
#include "clockfix.h"  // the date from the net's Date header (the MCU's clock: VestaStub)
#include "placepick.h" // where the unit is: the fix, the last place, the home point
#include "skinline.h"  // a line of skin.ini / user_skin.ini
#include "../../VHCheck/native/pefile.h"   // FwFileVersion: the shell's SW number from its own string
#include "wglayout.h"  // the main screen's widgets in their places
#include "webcover.h"  // a song's cover from the net (Bluetooth, the internet radio)
#include "callwin.h"   // an incoming call over any program

#define APP_CLASS L"VestaHome_Wnd"

// ---------------------------------------------------------------- palette (design 0.6 tokens; skin.ini may override)

static COLORREF kInk      = RGB(0x0e, 0x10, 0x13);   // background
static COLORREF kPanel    = RGB(0x1a, 0x1d, 0x22);   // cards, tiles, widgets (no borders)
static COLORREF kPanel2   = RGB(0x23, 0x27, 0x2e);   // pills, buttons, nested
static COLORREF kLine     = RGB(0x2a, 0x2f, 0x37);   // separators, tracks
static COLORREF kText     = RGB(0xf5, 0xf3, 0xee);
static COLORREF kMuted    = RGB(0xaa, 0xb0, 0xb8);
static COLORREF kDim      = RGB(0x6c, 0x72, 0x7c);
static COLORREF kOrange   = RGB(0xff, 0x6b, 0x1a);   // the one accent: action / active
static COLORREF kFacet    = RGB(0xc2, 0x4a, 0x0b);
static COLORREF kTri[4]   = { RGB(0x17, 0x1a, 0x20), RGB(0x12, 0x14, 0x19), RGB(0x1b, 0x1e, 0x24), RGB(0x0f, 0x11, 0x14) };   // the mosaic's cell: left, right, top, bottom (design 0.6)
static COLORREF kTriOr[4] = { RGB(0x8f, 0x36, 0x08), RGB(0xc2, 0x4a, 0x0b), RGB(0xa8, 0x3f, 0x09), RGB(0xe6, 0x5a, 0x12) };   // the accent's facets (ApplyAccent)
static COLORREF kOff    = RGB(0x3d, 0x42, 0x4a);   // unavailable
static COLORREF kOk     = RGB(0x2f, 0xbf, 0x71);   // call
static COLORREF kStop   = RGB(0xe8, 0x48, 0x3b);   // hang up, missed call
static COLORREF kMargin = RGB(0x08, 0x09, 0x0a);   // around a photo
// user_skin.ini (VHContent\Skin, the user 2026-11-19): its accent / facet given - they win over "Тема"; its font
static bool    s_userAccent, s_userFacet;
static wchar_t s_userFace[48], s_userFont[2][MAX_PATH];   // [fonts] face, regular, bold

// ---------------------------------------------------------------- layout

enum Icon { IC_NONE, IC_MAP, IC_NAV, IC_RADIO, IC_MUSIC, IC_PHONE, IC_GEAR, IC_HOME, IC_MOON, IC_BACK, IC_SUN,
            IC_APPS, IC_VIDEO, IC_NEXT, IC_CLOSE, IC_FOLDER, IC_SHIFT, IC_BKSP,
            IC_AUX, IC_PHOTO, IC_INFO, IC_PREV, IC_FWD, IC_PAUSE,
            // built-in only, not in the skin: icons of the other screens (the mockups' set, 48 grid)
            IC_BT, IC_USB, IC_LIST, IC_VOL, IC_SEARCH, IC_SHUFFLE, IC_REPEAT, IC_CALL, IC_HANGUP, IC_WIFI,
            IC_STAR, IC_STAR_F, IC_USER, IC_USERS, IC_CLOCK, IC_KEYPAD, IC_MIC_OFF, IC_HANDOVER, IC_PLAY,
            IC_ROT_L, IC_ROT_R, IC_WARN, IC_CALL_IN, IC_CALL_OUT, IC_CALL_MISS, IC_LOCK, IC_REFRESH, IC_EQ,
            IC_DISC, IC_EYE, IC_POWER, IC_EDIT, IC_ADD, IC_FILE, IC_UP, IC_CAR, IC_CLOUD, IC_TASKS, IC_SD, IC_DOWN, IC_GALLERY, IC_FULL, IC_UNFULL, IC_NETRADIO, IC_BTMUSIC, IC_RETURN, IC_EYE_OFF, IC_SIGNAL, IC_AAUTO, IC_SERVICE, IC_COUNT };

// Picture names in skin.ini (and Skin\<name>.png), by icon; NULL = no picture, always the built-in drawing. Since
// design 0.6 (2026-10-23) every icon has one: Lucide's, drawn by skin/make_icons.py (the built-in drawing - when the
// skin gives none).
static const wchar_t* kIconKey[IC_COUNT] = { NULL, L"map", L"nav", L"radio", L"music", L"phone", L"settings",
    L"home", L"night", L"prev", L"day", L"apps", L"video", L"next", L"exit", L"folder", L"shift", L"backspace",
    L"aux_ico", L"photo", L"info", L"rew", L"fwd", L"pause", L"bt", L"usb", L"list", L"vol", L"search", L"shuffle",
    L"repeat", L"call", L"hangup", L"wifi", L"star", L"star_f", L"user", L"users", L"clock", L"keypad", L"mic_off",
    L"handover", L"play", L"rot_l", L"rot_r", L"warn", L"call_in", L"call_out", L"call_miss", L"lock", L"refresh",
    L"eq", L"disc", L"eye", L"power", L"edit", L"add", L"file", L"up", L"car", L"cloud", L"tasks", L"sd", L"down",
    L"gallery", L"full", L"unfull", L"netradio", L"btmusic", L"return", L"eye_off", L"signal",
    NULL, NULL };   // (IC_AAUTO, IC_SERVICE: the built-in drawing alone)
typedef char kIconKeyCount[sizeof(kIconKey) / sizeof(kIconKey[0]) == IC_COUNT ? 1 : -1];

// A_NAV opens a main screen from the bottom bar (no history), A_SCREEN opens a screen over the current one,
// A_TAB opens it instead of the current one (sections, sources), A_BACK returns. A_TOGGLE, A_SEG, A_KEY and A_DEL
// only change the demo values below. A_PAGE and the rest belong to the programs ("Apps", the shortcut editor).
enum Action { A_NONE, A_NIGHT, A_EXIT, A_NAV, A_SCREEN, A_TAB, A_BACK, A_TOGGLE, A_SEG, A_KEY, A_DEL, A_PAGE,
              A_RUN, A_ADD, A_EDITMODE, A_FIELD, A_SAVE, A_DELETE, A_CANCEL, A_OK, A_CHAR, A_SHIFT, A_LANG,
              A_BKSP, A_FILE, A_UP, A_NOICON, A_MINIMIZE, A_CALIBRATE, A_TEST,
              A_KEYS, A_HOMEGPS, A_WXNOW, A_CAMFILE, A_KIND, A_KINDS, A_REGION, A_CAMDL, A_TZ,
              A_GPSPORT, A_GPSBAUD, A_HOUR, A_MINUTE, A_CANLOG, A_CANMARK, A_MCULOG, A_MCUREQ, A_EQ, A_EQPRESET,
              A_BAL, A_FADE, A_BALCENTER, A_BALXY, A_AVTEST, A_AVDEV, A_PRESET, A_RKEY, A_REBOOTNO, A_BTMUSIC, A_BTCMD, A_RPAGE, A_RLIST, A_RSCAN, A_SNDVOL, A_PLAY, A_PREVTRK, A_NEXTTRK, A_SEEK, A_SHUFFLE, A_REPEAT,
              A_MFILE, A_MUP, A_MALL, A_PHONECALL, A_PHONEANS, A_PHONEEND, A_PBCONTACT, A_PBDIAL, A_PBRELOAD, A_PBFAV, A_FAVDIAL, A_PBFIND, A_PBSIM, A_PBDEL, A_BTCONNECT, A_SHELL, A_PTYSEL, A_PTYGO, A_CARSENS, A_THEME, A_LIGHT, A_WGVIS, A_WGMOVE, A_PHFILE, A_PHSTEP, A_PHROT, A_PHSHOW, A_PHINT, A_SCROLL, A_USERWG, A_TASKTAB, A_TASKSORT, A_TASKKILL, A_TASKSWITCH, A_NAVCHIP,
              A_TASKCLOSE, A_KEYFIELD, A_CLIPCOPY, A_CLIPPASTE, A_CLIPCLEAR,
              A_VFILE, A_VUP, A_VPLAY, A_VPREV, A_VNEXT, A_VSEEK, A_VFULL,
              A_NRPLAY, A_NRSTEP, A_NRPICK, A_WIFINET, A_WIFIFORGET, A_WIFISCAN, A_PAGESTEP, A_WIFIAP,
              A_OBDSCAN, A_OBDPICK, A_OBDPIN, A_OBDPAIR, A_OBDCONN, A_BTUNPAIR, A_RVCVIEW, A_TANK, A_DTCREAD, A_DTCCLEAR, A_DTCINFO, A_TELINE, A_TEADD, A_TEDEL, A_TESAVE, A_TEBACK, A_TEXTFILES, A_UWEDIT, A_PNPICK, A_PNPAIR, A_PNCONN, A_BTVISIBLE, A_PAIRANSWER, A_ACCENT, A_SAVERBG, A_PHFULL, A_NMEACOM, A_RFILL, A_MODEMTEST, A_NETOP, A_NETPAN, A_OSMFAV, A_WIFIDEL, A_PLUGEV, A_PLUGOPEN, A_PLUGON, A_VOICEDIAL,
              A_PLUGPAGE, A_CAMOVERSTEP, A_PLUGNAV, A_SVCDONE,
              A_NRLIST, A_NRFAV, A_NCTILE, A_NCPICK, A_NCMORE, A_NCSORT, A_NCSEARCH, A_ADDONKEY, A_TOUCHCLEAR };

// S_SETTINGS is the general section; S_VIDEO and S_PHOTO are the file lists.
// S_INCOMING, S_VOLUME, S_EXIT (closing VestaHome) and S_REBOOT (the knob held) are windows over the previous screen.
// S_APPS: the programs; S_EDIT, S_KEYS, S_FILES: their shortcut editor, keyboard and file browser; S_TASKS: the
// task manager (processes, windows, CPU and memory).
// S_SET_CAR: the car section of the settings (the data port, CAN). S_DIAG: diagnostics, opened from "About the
// system" (as the stock shell's engineering menu from its "Info"), S_TEST_*: its tests. S_WSET_*: widgets' settings.
enum Screen { S_HOME, S_RADIO, S_RADIO_LIST, S_RADIO_PTY, S_AUDIO, S_AUDIO_LIST, S_AUDIO_BT, S_AUX,
              S_VIDEO, S_VIDEO_PLAY, S_PHOTO, S_PHOTO_VIEW,
              S_PHONE, S_CONTACTS, S_CONTACT, S_CALLS, S_FAVS, S_CALL, S_INCOMING,
              S_SETTINGS, S_SET_SOUND, S_SET_BALANCE, S_SET_SCREEN, S_SET_RADIO, S_SET_BT, S_SET_WIFI, S_WIFI_KEYS,
              S_SET_VIDEO, S_SET_PHOTO, S_INFO, S_WARNING, S_VOLUME, S_SAVER, S_EXIT, S_REBOOT,
              S_CAR, S_APPS, S_EDIT, S_KEYS, S_FILES, S_DIAG, S_TEST_SCREEN, S_TEST_TOUCH, S_TEST_MEM,
              S_WSET_WEATHER, S_WSET_SPEED, S_WSET_CAR, S_SET_HOME, S_CAMDL, S_CAMREG, S_SET_CAR, S_TEST_CAN, S_TEST_MCU,
              S_INFO_MORE, S_LICENSES, S_SET_VOLUME, S_TASKS, S_NETRADIO, S_NETRADIO_LIST, S_TEST_AV, S_SET_OBD, S_BT_PAIRED, S_OBD_DTC, S_TEXTEDIT, S_BT_PAIRNEW, S_BT_PAIRASK, S_SET_THEME, S_SAVER_PICK, S_WIFI_SAVED, S_PLUGIN, S_SET_PLUGINS, S_SET_NET, S_NETCAT, S_NETCATS, S_WSET_TRIP, S_SET_NET3G, S_THANKS, S_NONE };

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
enum { T_GPS_TIME, T_NOISE, T_MW, T_RDS, T_AF, T_CT, T_TA, T_BT, T_AUTOCONNECT, T_AUTOANSWER,
       T_WIFI, T_VIDEO_MOVING, T_SLIDES_OPEN, T_AUTOROTATE,
       T_W_GPS, T_CAM_ALERT, T_CAM_WEEKLY, T_WIFI_AP_AUTO, T_MS, T_LOGS, T_NMEA_PORT, T_NET_AUTO, T_PB_AUTO, T_SVC7000, T_PHONE_USB, T_PHONE_ANY, T_COUNT };
static bool s_on[T_COUNT] = { true, true, true, false, true, false, true, true, true, true, false,
                              true, false, false, true };
enum { G_TIME_FMT, G_VIDEO_SCALE, G_PHOTO_SCALE, G_WPROV, G_CAMDIST, G_CAMSND, G_SAVER, G_COUNT };
static int s_seg[G_COUNT] = { 1, 2, 1, 0 };
enum { K_DIAL, K_PASS };   // the number being dialled, the Wi-Fi password
enum { K_WKEY, K_HOMENAME, K_HOMELAT, K_HOMELON, K_BTNAME, K_NETAPN, K_NETUSER, K_NETPASS, K_ODO, K_SVCKM, K_SVCDATE };   // typed settings (A_KEYS)
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
static wchar_t* s_iconPath[IC_COUNT][2];   // the skin's picture not loaded yet (Ico loads it at its first use)
static bool LoadImg(const wchar_t* path, Img* im);
static wchar_t s_skinBg[MAX_PATH];     // main menu background picture, optional
static wchar_t s_skinSaver[MAX_PATH];  // the screen saver's picture, optional
static wchar_t s_skinTile[MAX_PATH];   // the background's tile (all screens), optional: the mosaic otherwise
static wchar_t s_skinCard[2][MAX_PATH];   // the cards' picture (card), the small tiles' (card_small): optional
static Img     s_card[2];                 // (cardpic.h: CardPic draws them instead of the panel colour)

static HWND    s_wnd;
static HBITMAP s_back;          // whole frame, drawn off-screen
static HBITMAP s_bg;            // the skin's main menu background picture, only if it gives one (750 KB)
typedef DWORD Pix;   // a pixel of the back buffer and the backgrounds: 0x00RRGGBB (32-bit DIBs; the unit's screen
                     // is more than 16-bit - the user 2026-10-23, photos of the stock shell's gradients)
static Pix*     s_backBits;   // their pixels: bottom-up, 800 per row
static Pix*     s_bgBits;
static HBITMAP s_saverBg;       // the screen saver's picture: loaded while it shows (750 KB)
static Pix*     s_saverBits;
static Pix*     s_tile[2];    // one period of the background (s_tileW x s_tileH, top-down), day / night
static int     s_tileW, s_tileH;      // the skin's tile, or the triangle mosaic's period
static int     s_pressed = -1;
static int     s_pressAct = -1, s_pressArg;   // what was pressed: the list may be rebuilt before the finger lifts
static bool    s_night;
static int     s_mcuNight = -1, s_bright = -1;   // the backlight: 0x4105 day / night, 0x4107 level
// A partial frame (RedrawArea): everything is drawn clipped to s_clip, the buttons are not registered again
// (the layout is the same), only the area goes to the screen and only when its pixels changed.
static RECT    s_clip = { 0, 0, 800, 480 };
static bool    s_partial;
static int     s_zoneIdx;   // the buttons counted during a partial frame (for the pressed look)
static bool    s_zoneMoved; // a partial frame's button not where the full frame had it: the whole frame
static wchar_t s_dir[MAX_PATH];

// Events log VestaHome.log - on the unit only on the SD card / USB disk (VestaCore LogDir: VestaLogs folder), never
// the built-in flash; without a card the lines wait in RAM for one (ramlog.h). What happened just before a hang of
// the unit. Opened and closed for every line, so a reset keeps it; past kLogMax it becomes VestaHome.old.log. Uptime
// first - the clock may jump.
static CRITICAL_SECTION s_logLock;
static const DWORD kLogMax = 1024 * 1024;

// All our logs on / off (Diagnostics -> Logs; the user 2026-11-16: off by default, was on): the flag
// \SDMMC\VestaLogs.on, written once when switched on (not a log) - the proxies, VestaStub and the add-ons read it
// too. PC: in the exe's folder.
static bool s_logsOn;

static void LogsOnPath(wchar_t* p)
{
#ifdef UNDER_CE
    wcscpy(p, L"\\SDMMC\\VestaLogs.on");
#else
    _snwprintf(p, MAX_PATH - 1, L"%sVestaLogs.on", s_dir);
    p[MAX_PATH - 1] = 0;
#endif
}

static void VLog(const wchar_t* fmt, ...);

static void LogsSet(bool on)
{
    wchar_t p[MAX_PATH];
    LogsOnPath(p);
    if (on) {
        HANDLE h = CreateFile(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h != INVALID_HANDLE_VALUE)
            CloseHandle(h);
        s_logsOn = true;
        VLog(L"журналы включены");
    } else {
        VLog(L"журналы выключены");
        s_logsOn = false;
        DeleteFile(p);
    }
}

// The start brings the last source back 10 s after it (ResumeSourceTick); a source the user starts before that
// cancels it - the radio, the internet radio, music, Bluetooth or a video started by hand stays (the user
// 2026-10-22: the disk's music came over the radio chosen meanwhile). s_resuming: the resume's own start.
static bool s_resumed, s_resuming;
static int  s_mediaLast;   // our media last played: 1 music, 2 the internet radio (the home widget shows it idle)

static void ResumeByUser()
{
    if (s_resuming || s_resumed)
        return;
    s_resumed = true;
    VLog(L"запуск: источник выбран вручную — прошлый не возвращаю");
}

static void VLog(const wchar_t* fmt, ...)
{
    if (!s_logsOn)
        return;
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
    HANDLE h = INVALID_HANDLE_VALUE;
    if (LogDir(path, s_dir)) {
        wcscat(path, L"VestaHome.log");
        h = CreateFile(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, 0, NULL);
    }
#ifdef UNDER_CE
    // no card (or it just went): the line waits in RAM (ramlog.h; the crash note of a card pulled included)
    static bool s_ramWaits = true;   // (a VestaHome before this one may have left lines there)
    if (h == INVALID_HANDLE_VALUE) {
        RamLogPut(L"\\Temp\\VestaHome.log", L"\\Temp\\VestaHome.old.log", buf, n);
        s_ramWaits = true;
        LeaveCriticalSection(&s_logLock);
        return;
    }
#endif
    if (h != INVALID_HANDLE_VALUE && GetFileSize(h, NULL) > kLogMax) {
        // into VestaHome.old.log, a new one started (HU 2026-11-03: emptied at 256 KB, the OBD session lost with it)
        CloseHandle(h);
        wchar_t old[MAX_PATH];
        wcscpy(old, path);
        wcscpy(old + wcslen(old) - 4, L".old.log");
        DeleteFile(old);
        MoveFile(path, old);
        h = CreateFile(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, 0, NULL);
    }
    if (h != INVALID_HANDLE_VALUE) {
        SetFilePointer(h, 0, NULL, FILE_END);
#ifdef UNDER_CE
        if (s_ramWaits)   // the lines kept in RAM meanwhile, before this one
            RamLogFlush(L"\\Temp\\VestaHome.log", L"\\Temp\\VestaHome.old.log", h);
        s_ramWaits = false;
#endif
        DWORD w;
        WriteFile(h, buf, n, &w, NULL);
        CloseHandle(h);
    }
    LeaveCriticalSection(&s_logLock);
}

static wchar_t s_status[96];    // last action result, shown in the top bar
// VestaHome is closing (WM_DESTROY on): nothing new started - no widgets rescanned, no fetch (the tester 2026-11-20: an
// add-on's "bye" at the exit rescanned the widgets, their fetch hung in DNS with no network - the process could not
// end, TerminateProcess "busy", the setup refused the first time).
static volatile bool s_quitting;
static int     s_downX, s_downY;   // where the press started (the keyboard's field: a hold moved off is no hold)
// The finger, filtered (VestaCore vtouch.h; the user 2026-11-23: "smooth, as in the maps"): a tap at its settled press
// point, a drag past kVTouchDragPx (the panel's wander is no move, HU 2026-10-24-2: a preset's tap turned the page)
static VTouch  s_vt;
static bool    s_drag;          // a page follows the finger (DragStart .. DragEnd): no other redraws meanwhile
static int     s_dragBaseX, s_dragBaseY;   // the finger's way (s_vt.dx, dy) when the page began to follow it
static bool    s_listDrag;      // a list follows the finger up / down (Scroll)
static int     s_listDrag0;     // its scroll when the finger started
static int     s_listBaseY;     // the finger's way (s_vt.dy) then
#ifdef UNDER_CE
static HWND    s_taskbar;
#endif
static int     s_exitCode;   // 3: "Выход" pressed - VestaStub then keeps the stock shell away (HU 2026-11-15-2)

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
// The rest of VestaHome, in this order (one translation unit: the parts are not compiled on their own).
// A crash's place in the log (HU 2026-10-31: VestaHome fell with C0000005 playing the internet radio over the phone,
// nowhere known): the code, the address and the program's base; then on as before (the system's handler next)
static int CrashNote(EXCEPTION_POINTERS* e, const wchar_t* where)
{
    VLog(L"СБОЙ в %s: код %08X, адрес %08X (VestaHome с %08X)", where, (unsigned)e->ExceptionRecord->ExceptionCode,
         (unsigned)(size_t)e->ExceptionRecord->ExceptionAddress, (unsigned)(size_t)GetModuleHandle(NULL));
    return EXCEPTION_CONTINUE_SEARCH;
}

// A vhttp error for the screen and the log.
static const wchar_t* NetErrorText(int err)
{
    switch (err) {
    case NET_DNS:     return L"нет сети (DNS)";
    case NET_CONNECT: return L"сервер не отвечает";
    case NET_TIMEOUT: return L"обрыв связи";
    case NET_HTTP:    return L"ошибка сервера";
    case NET_TLS:     return L"ошибка HTTPS";
    case NET_MEMORY:  return L"мало памяти";
    }
    return L"";
}

// ---- The support (2026-11-15, the user: Android Auto's splash gone with a key for this unit): the unit's ID shown in
// "О системе" and told to the add-ons ("welcome": support, unit); the key - VestaLicense.key in VestaHome's folder,
// made by the author for the ID (VestaCore vlicense.h). Nothing in VestaHome is locked without it. Its level
// (2026-11-18): a subscriber (Android Auto's splash gone) or a co-author (also the smart home add-on).
static wchar_t s_unitId[LICENSE_ID_LEN];
static int     s_support;   // LICENSE_NONE / SUPPORTER / COAUTHOR

static void SupportInit()
{
    LicenseUnitId(s_unitId);
    wchar_t path[MAX_PATH];
    _snwprintf(path, MAX_PATH, L"%sVestaLicense.key", s_dir);
    bool found;
    s_support = LicenseFileValid(path, s_unitId, &found);
    VLog(L"поддержка: ID %s, ключ %s", s_unitId, s_support == LICENSE_COAUTHOR ? L"Соавтора" :
         s_support ? L"подписчика" : !found ? L"нет" : L"не подходит (другой ГУ или повреждён)");
}

#include "vh_draw.inc"
#include "vh_screens.inc"
#include "vh_diag.inc"
#include "vh_settings.inc"
#include "vh_gps.inc"
#include "vh_mcu.inc"
#include "vh_demo.inc"   // (the PC build only: the demo head unit)
#include "vh_player.inc"
#include "vh_cover.inc"
#include "vh_netradio.inc"
#include "vh_netcat.inc"
#include "vh_radiologo.inc"   // (after vh_netcat.inc: its hosts)
#include "vh_video.inc"
#include "vh_sources.inc"
#include "vh_camera.inc"
#include "vh_weather.inc"
#include "vh_speedcam.inc"
#include "vh_apps.inc"
#include "vh_tasks.inc"
#include "vh_userwidgets.inc"
#include "vh_widgetset.inc"
#include "vh_textedit.inc"
#include "vh_sound.inc"
#include "vh_camdl.inc"
#include "vh_plugins.inc"
#include "vh_frame.inc"
#include "vh_resume.inc"
#include "vh_photo.inc"
#include "vh_skin.inc"

// ---------------------------------------------------------------- window

// VestaLaunch follows our day/night: it asks with this message, 1 = night.
#define WM_VESTA_NIGHT (WM_APP + 2)

// The list the press started on, which a finger moved up / down scrolls (Scroll: the one drawn last), or NULL.
static Scroll* ListUnderFinger()
{
    POINT p = { s_vt.px, s_vt.py };
    return s_scroll && PtInRect(&s_scroll->view, p) ? s_scroll : NULL;
}

// The finger's drag (vtouch: past kVTouchDragPx) taken by a page (clearly sideways on a paged screen) or by the list
// it began on; neither yet - asked again at its next point. Each follows its way from where it took it: no jump.
static void FingerDragBegin()
{
    Scroll* ls = ListUnderFinger();
    int what = DragWhat(s_vt.x - s_vt.px, s_vt.y - s_vt.py, CanSwipe() && !s_btHold, ls != NULL);
    if (what == DRAG_PAGE) {
        DragStart();   // (no memory for the page's copy: s_drag stays off, the release turns it)
        s_dragBaseX = s_vt.dx;
        s_dragBaseY = s_vt.dy;
    } else if (what == DRAG_LIST) {
        s_listDrag = true;
        s_listDrag0 = ls->pos;
        s_listBaseY = s_vt.dy;
        KillTimer(s_wnd, TIMER_LONG);
        s_pressed = -1;
    }
}

// The finger moved (its points filtered by vtouch): the balance point, a page or a list follows it
static void FingerMove(int x, int y)
{
    int st = VTouchMove(&s_vt, GetTickCount(), x, y);
    if (s_screen == S_TEST_TOUCH && s_pressed < 0) {
        TouchTestAdd(x, y);
        Redraw();
        return;
    }
    if (s_pressed >= 0 && s_btns[s_pressed].action == A_BALXY) {
        BalanceAt(s_vt.x, s_vt.y, false);
        return;
    }
    if (st != VT_DRAG)
        return;
    if (!s_drag && !s_listDrag)
        FingerDragBegin();
    Scroll* ls = s_listDrag ? ListUnderFinger() : NULL;
    if (s_drag) {
        DragMove(s_vt.dx - s_dragBaseX);
    } else if (ls) {
        ScrollTo(*ls, s_listDrag0 - (s_vt.dy - s_listBaseY));
        RedrawArea(ls->area.left, ls->area.top, ls->area.right, ls->area.bottom);
    }
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE: {
        s_wnd = hwnd;
        s_back = MakeDib(800, 480, &s_backBits);
        LoadSkin();   // (before the fonts: user_skin.ini may name its own)
        {
            int nf = FontsLoad();
            VLog(L"шрифт: %s (своих загружено %d из 2)", s_face, nf);
        }
        BuildBackground();
        for (int i = 0; i < 2; i++)
            if (s_skinCard[i][0] && !LoadImg(s_skinCard[i], &s_card[i]))
                VLog(L"оформление: не прочитана картинка карточки %s", s_skinCard[i]);
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
            SupportInit();
        }
        LoadCfg();
        ApplyAccent();
        CfgToControls();
        s_bootTick = GetTickCount();
        UserWidgetsScan();   // (after the ini: their places in the order)
        PluginsStart();      // the add-ons (VHContent\VHPlugins): they say hello when ready
        ResumeScreen();
        WaveDevsLog();
        {
            WSADATA wsa;
            WSAStartup(MAKEWORD(2, 2), &wsa);
        }
        InitializeCriticalSection(&s_wxCs);
        WxLoad();
        NetInit();
        static const HttpSetup http = { "VestaHome/0.5", "*/*", 3 * 1024 * 1024, NULL };   // (no log: as before)
        HttpInit(&http);
#ifndef UNDER_CE
        DemoStart();   // the PC: the head unit's MCU, Bluetooth, GPS made up (vh_demo.inc)
#endif
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
        WifiApSetup(s_cfg.wifiApSsid, s_cfg.wifiApPass, false);   // (no automatic access point: the user 2026-11-08)
        WifiStart(hwnd, WM_APP_WIFI, VLog);
        ObdInit(hwnd, WM_APP_OBD, VLog);
        if (s_cfg.nmeaPort)   // the navigators' COM port (the registry is new at every boot)
            NmeaPortStart(true, false);
        if (s_cfg.phoneUsb)   // the phone by cable: Android Auto, the modem (the registry is new at every boot)
            PhoneUsbSet(true);
        ObdSetSppAddrFn(ObdSppAddr);
        BtRfSetWake(ObdSdpWake);
        for (int i = 0; i < s_cfg.wifiCount; i++)
            if (!s_cfg.wifiLocked[i])
                WifiRemember(s_cfg.wifiSsid[i], s_cfg.wifiPass[i]);
        s_mediaMask = MediaMask();
        RvcInit(VLog);
        SetTimer(hwnd, TIMER_RVC, kRvcMs, NULL);   // the reverse gear (rvc.cpp), cheap: one GPIO read
        if (WeatherWanted())
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
        if (wp == TIMER_NAVANIM) {   // the bottom bar's change of section: its next frame, the last one stops it
            if (s_navFrom < 0 || (s_navStart && GetTickCount() - s_navStart >= (DWORD)NavAnimMs())) {
                KillTimer(hwnd, TIMER_NAVANIM);
                s_navFrom = -1;
            }
            if (IsWindowVisible(hwnd))
                RedrawArea(0, 408, 800, 480);
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
        if (wp == TIMER_RFILL) {   // the list's stations into the presets: the next step
            RadioFillTick();
            return 0;
        }
        if (wp == TIMER_MENUKEY) {   // a main menu key: "back" to it first
            MenuKeyStep();
            return 0;
        }
        if (wp == TIMER_SOUND) {   // the equalizer / balance: the MCU's sound page first
            SoundStep();
            return 0;
        }
        if (wp == TIMER_BTSRC) {   // BtMusicOn's second key
            KillTimer(hwnd, TIMER_BTSRC);
            McuKey(0x54, false);
            return 0;
        }
        if (wp == 1)
            NetAutoStep();
        if (wp == 1)
            WifiWatch();   // the Wi-Fi thread stuck in a driver's call: into the log
        if (wp == 1)
            PlugScreenWatch();   // an add-on's screen came or went: screen.open / screen.close
        if (wp == 1) {
            static int s_appTicks;
            if (++s_appTicks % 2 == 0 && AppsRecheck() && s_screen == S_APPS && IsWindowVisible(hwnd))
                Redraw();
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
        if (wp == 1) {   // calls: an incoming one opens its window over any program (callwin.cpp), a taken one the
                         // call's, the end closes them and gives the front back
            BtState bt;
            static int s_lastCall;
            static bool s_virtualDown;   // Android's virtual call (callwin.h CallVirtual) put down already
            bool got = BtGet(&bt, NULL), virt = got && bt.call[0] != BT_CALL_NONE && CallVirtual(bt.callNumber);
            if (virt && !s_virtualDown) {
                VLog(L"Bluetooth: «звонок» 10000000 - телефон взял микрофон машины для приложения (помощник), "
                     L"не звонок: кладу, телефон вернётся к музыке");
                BtCmd(BT_CMD_END);
            }
            s_virtualDown = virt;
            if (got && (virt ? BT_CALL_NONE : bt.call[0]) != s_lastCall) {   // (no window for it; one opened
                int c = virt ? BT_CALL_NONE : bt.call[0];                    // before its number came - closed)
                s_lastCall = c;
                if (c == BT_CALL_ACTIVE)
                    s_callTick = GetTickCount();
                CallAct act = CallStep(c, s_screen == S_INCOMING, s_screen == S_CALL, AaInFront());
                if (act == CA_INCOMING) {
                    CallFront();
                    Go(S_INCOMING, true);
                } else if (act == CA_TAKEN) {
                    Back();
                    Go(S_CALL, true);
                } else if (act == CA_CLOSE) {
                    Back();
                    CallGiveBack();
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
            static bool s_wasFront;   // (the user 2026-11-14: OSMapsCE closed - the saver at once: the minutes in
            if (!s_lastTouch || (front && !s_wasFront))   // another program are use, not idle - counted from the return)
                s_lastTouch = now;
            s_wasFront = front;
            if (s_screen != S_SAVER)
                SaverFree();
            if (s_screen == S_SAVER) {
                static int s_min = -1;
                SYSTEMTIME t;
                GetLocalTime(&t);
                static wchar_t s_song[160];
                wchar_t song[160];
                SaverNowPlaying(song, 160);
                if ((t.wMinute != s_min || wcscmp(song, s_song)) && front) {   // the clock, what plays
                    s_min = t.wMinute;
                    wcscpy(s_song, song);
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
            NetCoverTick();
            PbTick();
            PairTick();
            BtOwnerTick();
            ClockWatchTick();
            NetTimeTick();
            TripTick();
            {   // the trip widget: again when its numbers changed (a tenth of a km, a minute)
                static DWORD s_tripSig;
                RECT wr;
                DWORD sig = (DWORD)(s_trip.km * 10) * 1000 + s_trip.movingS / 60 + (DWORD)s_trip.maxKmh * 7919
                            + (DWORD)(s_trip.fuelL * 10) * 104729;
                if (sig != s_tripSig && s_screen == S_HOME && IsWindowVisible(hwnd) && HomeRect(WG_TRIP, &wr)) {
                    s_tripSig = sig;
                    RedrawArea(wr.left, wr.top, wr.right, wr.bottom);
                }
            }
            UiRememberTick();
            PlaceRemember();
            for (int u = 0; u < s_uwCount; u++) {   // the user widgets' data: re-read as often as each asks
                UserWg& w = s_uw[u];
                if (!WgShown(WG_COUNT + u) || GetTickCount() - w.readTick < (DWORD)w.refresh * 1000)
                    continue;   // (a hidden one reads nothing)
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
                if (src != 0 || McuHostOn()) {   // the keys did not do it (or silent, under our host): a preset does
                    Tuner t;                       // (the one playing, else the first)
                    TunerGet(&t);
                    int n = t.cur >= 0 && t.cur < 24 ? t.cur : 0;
                    VLog(L"радио: источник %d после клавиш - пресет %d", src, n + 1);
                    RadioPreset(n);
                    SrcAsked(SrcBit(0));
                }
            }
            if (s_mediaAsk && now - s_mediaAsk > 2000) {
                s_mediaAsk = 0;
                MediaAskSignal(false);
                if (src != 6 && src != 7 && src != 9) {
                    VLog(L"источник: медиа не включилось (%d) - путь Bluetooth", src);
                    MainMenuKey(0x84);
                    SrcAsked(SrcBit(9));
                    s_btPathTick = now;
                }
            }
            PhonePlayTakes(src, now);
            BtSoundWatch(src, now);
            if (s_btPathTick && now - s_btPathTick > 3000) {   // the BT key may start the phone: it is paused
                s_btPathTick = 0;
                BtQuiet();
            }
            if (s_srcSeen == -2) {   // the first tick with the MCU: AUX into MODE's round (AuxPlugIn)
                AuxPlugIn();
                VLog(L"AUX: MCU сказано «штекер вставлен» (для круга MODE)");
            }
            if (src != s_srcSeen) {
                int aux = LearnSource(s_auxSrc, s_auxAsk ? now - s_auxAsk : 0xFFFFFFFF, s_srcSeen, src);
                if (aux != s_auxSrc) {
                    VLog(L"источник: AUX у MCU - номер %d", aux);
                    s_auxSrc = aux;
                }
                bool ours = SrcOurs(now, s_srcByUs, s_srcWant, src);
                if (s_srcSeen != -2 && (src == 0 || src == 9) && !ours) {
                    VLog(L"источник: %s - наши плееры на паузу", src ? L"Bluetooth" : L"радио");
                    OursQuiet();
                }
                // MODE to the media (not our asking): our music on - as the shell opens its player there (Ghidra
                // 2026-10-29: the MCU asks for a player, 0x9101; VestaStub names the card) - unless ours plays
                if (s_srcSeen != -2 && (src == 6 || src == 7) && !ours && !PlayerPlaying() &&
                    !s_netOn && s_vidState != VID_PLAYING && McuHostOn()) {
                    VLog(L"источник: медиа (MODE) - наша музыка");
                    PlayerToggle();
                }
                if (s_srcSeen != -2 && !ours)
                    FollowSource(src);
                if (src == 0 && s_srcSeen > 0 && !s_radioSrcAsk && McuHostOn())
                    s_radioSrcAsk = now - 1000;  // back to the radio under our host: silent until a preset (next tick)
                s_srcSeen = src;
            }
        }
        if (wp == 1 && (s_screen == S_SET_OBD || s_screen == S_BT_PAIRNEW) && IsWindowVisible(hwnd) &&
            ObdScanEvents())
            Redraw();   // the search's devices
        if (wp == 1 && IsWindowVisible(hwnd)) {   // the top bar's Bluetooth mark: a phone connected or gone
            BtState bt;
            static int s_btMark = -1;
            int on = BtGet(&bt, NULL) && (bt.hf == 2 || bt.av >= 1) ? 1 : 0;
            if (on != s_btMark) {
                if (s_btMark >= 0)
                    RedrawArea(0, 0, 800, 56);
                s_btMark = on;
            }
        }
        if (wp == 1 && s_screen == S_HOME && IsWindowVisible(hwnd)) {   // the phone widget follows the proxy
            BtState bt;
            static unsigned s_sig;
            RECT wr;
            if (BtGet(&bt, NULL) && HomeRect(WG_PHONE, &wr)) {
                unsigned sig = bt.hf * 7 + bt.battery * 131 + bt.signal * 1031 + bt.call[0] * 9973 + bt.call[1] * 7919;
                for (const char* q = bt.operatorName; *q; q++)
                    sig = sig * 31 + (unsigned char)*q;
                for (const char* q = bt.callNumber; *q; q++)
                    sig = sig * 31 + (unsigned char)*q;
                for (const char* q = bt.devName; *q; q++)
                    sig = sig * 31 + (unsigned char)*q;
                if (sig != s_sig) {
                    s_sig = sig;
                    RedrawArea(wr.left, wr.top, wr.right, wr.bottom);
                }
            }
        }
        if (wp == 1)
            BtMusicTick(hwnd);
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
            if (!s_wxBusy && WeatherWanted() &&
                ((w.have && w.ok > 0 && now - w.tick > 1800000) || (w.ok <= 0 && now - s_wxTry > 300000)))
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
                int vol, mute, src;
                int lost = s_mediaMask & ~m;
                if (lost && VolGet(&vol, &mute, &src) && CardOutRadio(src, s_netOn, lost, PlayingVolumes())) {
                    VLog(L"%s вынут при источнике %d - радио", lost & 1 ? L"SD-карта:" : L"USB-диск:", src);
                    RadioOn();
                }
                s_mediaMask = m;
                UserWidgetsScan();   // the card came or went: its widgets
                PluginsStart();      // (and its add-ons: the ones running are left alone)
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
                s_screen == S_TEST_MCU)   // (Wi-Fi: not each second - WM_APP_WIFI comes with every change, sheet 26)
                Redraw();
            else if (s_screen == S_AUDIO && PlayerPlaying())
                RedrawArea(256, 130, 800, 300);   // the track's position (the knob, r 11, from x 261)
            else if (s_screen == S_HOME && HomeMediaRect(&wr) && PlayerPlaying())
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
    case WM_APP_PLAYER:   // the track ended (BASS's sync): the next one - unless paused meanwhile or the source not ours
        if (!s_holdNext) {
            int vol, mute, src;
            VolGet(&vol, &mute, &src);
            if (NextTrackPlays(src, s_mediaAsk || s_btPathTick))
                PlayerStep(1, true);
            else   // (a play again - MODE to the media, the play key - starts the track ended)
                VLog(L"плеер: трек кончился, источник %d не наш - следующий не начинаю", src);
        }
        if (IsWindowVisible(hwnd) && GetForegroundWindow() == hwnd)
            Redraw();
        return 0;
    case WM_APP_UWG:   // a widget's url fetched: its data read at the next second
        for (int u = 0; u < s_uwCount; u++)
            s_uw[u].readTick = 0;
        return 0;
    case WM_APP_NETTIME:   // the date from the net (vh_gps.inc)
        NetTimeTake();
        return 0;
    case WM_APP_RADIOLOGO:   // a radio station's logo from the net (vh_radiologo.inc)
        if (RadioNetTake((int)wp) && IsWindowVisible(hwnd) && !s_drag)
            Redraw();
        return 0;
    case WM_APP_WEBCOVER:   // a song's cover downloaded (vh_cover.inc)
        if (WebCoverTake((unsigned long)wp) && IsWindowVisible(hwnd) && !s_drag)
            Redraw();
        return 0;
    case WM_APP_COVER:   // the music's cover decoded (vh_cover.inc)
        if (CoverTake((CoverJob*)lp) && IsWindowVisible(hwnd) && !s_drag &&
            (s_screen == S_AUDIO || s_screen == S_HOME || s_screen == S_NETRADIO || s_screen == S_AUDIO_BT))
            Redraw();   // (the internet radio's and Bluetooth's screens too - HU 17.11: seen only after a redraw)
        return 0;
    case WM_APP_PHOTO:   // a picture or a thumbnail decoded
        if ((s_screen == S_PHOTO || s_screen == S_PHOTO_VIEW || s_screen == S_SAVER_PICK) && IsWindowVisible(hwnd) &&
            !s_drag)   // (the saver's pick: the same tiles, HU 2026-11-05 - the thumbnails only after a tap)
            Redraw();
        return 0;
    case WM_APP_NETCAT:   // the radio catalogue's answer
        OnNetCat((NcJob*)lp);
        if (IsWindowVisible(hwnd) && GetForegroundWindow() == hwnd &&
            (s_screen == S_NETCAT || s_screen == S_NETCATS || s_screen == S_NETRADIO_LIST || s_screen == S_NETRADIO))
            Redraw();
        return 0;
    case WM_APP_NET:   // the internet radio: connected, failed, broken, a new song title
        OnNetMessage(wp, lp);
        if (IsWindowVisible(hwnd) && GetForegroundWindow() == hwnd) {
            RECT wr;
            if (s_screen == S_NETRADIO)
                RedrawArea(272, 130, 800, 390);   // the station's lines and its buttons
            else if (s_screen == S_HOME && HomeMediaRect(&wr))
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
        if (!IsWindowVisible(hwnd) || GetForegroundWindow() != hwnd)
            return 0;
        if (s_screen == S_SET_WIFI || s_screen == S_WIFI_SAVED)
            Redraw();
        else if (mark)   // elsewhere it is the top bar's mark only (sheet 26: whole frames as the link dropped)
            RedrawArea(0, 0, 800, 56);
        return 0;
    }
    case WM_APP_OBD:   // the OBD thread's step or values
        {   // a new trouble code (more of them, or the lamp lit): breaking glass, once a change
            static int s_dtcsWas = -1, s_milWas = -1;
            ObdInfo o;
            ObdGet(&o);
            if (o.state == OBD_ON && o.dtcs >= 0) {
                if ((o.dtcs > 0 && o.dtcs > s_dtcsWas) || (o.mil == 1 && s_milWas == 0)) {
                    VLog(L"OBD: ошибок %d (было %d), лампа %d - звук", o.dtcs, s_dtcsWas, o.mil);
                    GlassPlay();
                }
                s_dtcsWas = o.dtcs;
                s_milWas = o.mil;
            }
        }
        if ((s_screen == S_SET_OBD || s_screen == S_OBD_DTC || s_screen == S_CAR) && IsWindowVisible(hwnd))
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
            RECT wr;
            if (!IsWindowVisible(hwnd))
                return 0;
            if (s_screen == S_RADIO || s_screen == S_SET_RADIO || s_screen == S_RADIO_LIST) {
                Redraw();
            } else if (s_screen == S_HOME) {   // only its widgets that show the tuner (sheet 26: the whole frame
                if (HomeMediaRect(&wr))         // each tuner message - a third of the full frames)
                    RedrawArea(wr.left, wr.top, wr.right, wr.bottom);
                if (HomeRect(WG_RADIO, &wr))
                    RedrawArea(wr.left, wr.top, wr.right, wr.bottom);
            }
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
            if (changed)
                SaveCfg();
            if (IsWindowVisible(hwnd) && (s_screen == S_SET_SOUND || s_screen == S_SET_BALANCE))
                Redraw();   // (their switches show the MCU's values: McuSoundGet)
            return 0;
        }
        if (wp == WP_MCU_HWKEY) {
            HwKey(lp & 0xFF, lp >> 8 & 0xFF, lp >> 16 & 0xFF);
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
        if (wp == WP_MCU_VOLUME) {   // the volume or the mute changed: the top bar (the window kept 2.5 s if open)
            int v, mu, src;
            VolGet(&v, &mu, &src);
            {   // MUTE pressed twice (on, off within 1.5 s) while another program is in front: VestaHome back in
                // front (the user 2026-10-22: the panel's MUTE is the MCU's own - only 0x3109 says it, no key
                // message, so a long press cannot be seen or kept from muting; twice leaves the sound on)
                static int   s_muteWas = -1;
                static DWORD s_muteOn;
                if (mu != s_muteWas) {
                    VLog(L"MUTE: %d", mu);   // (HU sheet 30: only 0x3109 00 seen - does "on" come at all?)
                    if (mu)
                        s_muteOn = GetTickCount();
                    else if (s_muteWas == 1 && GetTickCount() - s_muteOn < 1500 &&
                             (!IsWindowVisible(hwnd) || GetForegroundWindow() != hwnd)) {
                        VLog(L"MUTE дважды: VestaHome вперёд");
                        HideOthers(GetForegroundWindow());
#ifdef UNDER_CE
                        ShowWindow(hwnd, SW_SHOW);
                        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
#else
                        ShowWindow(hwnd, IsIconic(hwnd) ? SW_RESTORE : SW_SHOW);
#endif
                        SetForegroundWindow(hwnd);
                    }
                    s_muteWas = mu;
                }
            }
            if (!IsWindowVisible(hwnd))
                return 0;
            // no window by itself (the user 2026-11-04: it held the whole screen while up; the level and the mute
            // are in the top bar) - only the speaker there opens it
            if (s_screen == S_VOLUME)
                s_screenTick = GetTickCount();   // kept open while it keeps changing
            BuildButtons();
            static int s_srcDrawn = -2;   // the source changes what plays (widgets, screens): the whole frame then;
            bool srcMoved = src != s_srcDrawn;   // the level alone is the top bar's (sheet 26: the knob's steps
            s_srcDrawn = src;                    // were whole frames)
            if (srcMoved || s_screen == S_VOLUME || s_screen == S_CALL)
                Redraw();   // the window, the call's level
            else
                RedrawArea(0, 0, 800, 56);
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
        if (s_cfg.weatherGps && !s_wxByGps && GpsFresh(g, 5000) && WeatherWanted()) {   // the first fix: the weather here
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
#ifdef UNDER_CE
    case WM_RASDIALEVENT:   // the modem's dial: a step (vh_gps.inc OnRasEvent)
        OnRasEvent((DWORD)wp, (DWORD)lp);
        return 0;
#endif
    case WM_APP_MODEM:   // the modem's dial is done (vh_gps.inc ModemDial)
        if (IsWindowVisible(hwnd) && s_screen == S_DIAG)
            Redraw();
        return 0;
    case WM_APP_NMEA:   // the navigators' port is done; wp: it failed - the setting off again
        if (wp && s_cfg.nmeaPort) {
            s_cfg.nmeaPort = false;
            s_on[T_NMEA_PORT] = false;
            SaveCfg();
        }
        if (IsWindowVisible(hwnd) && s_screen == S_SET_CAR)
            Redraw();
        return 0;
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
        s_lastTouch = GetTickCount();   // (also the finger's press: vtouch)
        s_downX = (short)LOWORD(lp);
        s_downY = (short)HIWORD(lp);
        s_pressed = HitTest((short)LOWORD(lp), (short)HIWORD(lp));
        s_pressAct = s_pressed >= 0 ? s_btns[s_pressed].action : -1;
        s_pressArg = s_pressed >= 0 ? s_btns[s_pressed].arg : 0;
        VTouchDown(&s_vt, s_lastTouch, s_downX, s_downY);
        if (s_screen == S_TEST_TOUCH && s_pressed < 0)
            TouchTestDown(s_downX, s_downY);
        if (s_pressed >= 0 && s_btns[s_pressed].action == A_KEYFIELD)   // the keyboard's cursor to the finger
            VKeysCursorAt(&s_keys, (short)LOWORD(lp) - kKeyFieldX, kKeyFieldW);
        if (s_pressed >= 0 && (s_btns[s_pressed].action == A_KEYFIELD || s_btns[s_pressed].action == A_PRESET ||
                               (s_btns[s_pressed].action == A_KEY && s_btns[s_pressed].arg == K_DIAL * 65536 + L'0') ||
                               (s_btns[s_pressed].action == A_BTCMD &&
                                (s_btns[s_pressed].arg == BT_CMD_NEXT || s_btns[s_pressed].arg == BT_CMD_PREV))))
            SetTimer(hwnd, TIMER_LONG, s_btns[s_pressed].action == A_PRESET ? 1200 : kLongMs, NULL);
        SetCapture(hwnd);
        if (s_pressed >= 0 && s_screen != S_TEST_TOUCH)
            RedrawBtn(s_pressed);   // (only its pressed look)
        else
            Redraw();
        return 0;
    case WM_MOUSEMOVE:   // the touch test and the balance point follow the finger, a swiped page, a scrolled list
        if ((wp & MK_LBUTTON) && s_pressed >= 0 && s_btns[s_pressed].action == A_KEYFIELD) {   // the cursor follows
            int dx = (short)LOWORD(lp) - s_downX, dy = (short)HIWORD(lp) - s_downY;
            if (dx > 6 || dx < -6 || dy > 6 || dy < -6)
                KillTimer(hwnd, TIMER_LONG);   // moved: not a hold for copy / paste
            VKeysCursorAt(&s_keys, (short)LOWORD(lp) - kKeyFieldX, kKeyFieldW);
            RedrawArea(0, 60, 800, 140);
            return 0;
        }
        if (wp & MK_LBUTTON)
            FingerMove((short)LOWORD(lp), (short)HIWORD(lp));
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
        double vx, vy;   // (the pen-up point not used: a stray on the panel - the settled press point, the last shown)
        bool dragged = VTouchUp(&s_vt, GetTickCount(), &vx, &vy);
        if (s_btHold) {   // rewind / fast forward ends with the finger
            BtCmd(s_btHold);
            s_btHold = 0;
            s_pressed = -1;
            ReleaseCapture();
            Redraw();
            return 0;
        }
        ReleaseCapture();
        if (s_screen == S_TEST_TOUCH && s_pressed < 0) {   // the touch test: no tap, the press counted
            TouchTestUp();
            Redraw();
            return 0;
        }
        if (s_listDrag) {   // the list scrolled: no tap; the buttons of the new positions
            s_listDrag = false;
            s_pressed = -1;
            Redraw();
            return 0;
        }
        if (s_drag) {
            s_pressed = -1;
            DragEnd(PageTurn(s_vt.dx - s_dragBaseX, s_vt.dy - s_dragBaseY, vx, vy, s_dragNb));
            return 0;
        }
        // a tap: at the settled press point; a drag nothing followed: where the finger was last
        int i = dragged ? HitTest(s_vt.x, s_vt.y) : HitTest(s_vt.px, s_vt.py);
        int was = s_pressed;
        s_pressed = -1;
        if (was >= 0 && s_btns[was].action == A_BALXY) {   // the finger may have left the car: still the point
            BalanceAt(s_vt.x, s_vt.y, true);
            Redraw();
            return 0;
        }
        // a drag no page followed (the photo viewer; no memory for the page's copy): its way or a flick turns the page
        int dir = dragged && (CanSwipe() || s_screen == S_PHOTO_VIEW) ?
                  PageDir(s_vt.x - s_vt.px, s_vt.y - s_vt.py, vx, vy) : 0;
        if (dir) {
            Redraw();   // the old page without the pressed look: the frame the new one pushes out
            if (Swipe(dir)) {
                BuildButtons();
                RedrawSlide(dir);
            }
            return 0;
        }
        // the same control as pressed - by what it does, not by its place on the list: a redraw between the press and
        // the lift (an add-on's update, a status line) may have rebuilt the list (the first tap was lost, 2026-10-26)
        bool tap = i >= 0 && was >= 0 && s_btns[i].action == s_pressAct && s_btns[i].arg == s_pressArg;
        if (!tap)
            RedrawBtn(was);   // only its pressed look off (a tap: the action's own frame below)
        if (tap) {
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
            KeyChar((wchar_t)wp);
        BuildButtons();
        Redraw();
        return 0;
    case WM_KEYDOWN:   // PC test: N = night, R = reverse, C = a demo call (again: its end), Esc = exit (not while typing)
        if (s_screen == S_KEYS)
            return 0;
        if (wp == 'N') {
            SetTheme(!s_night);
            Redraw();
        } else if (wp == 'R') {   // a "reverse" to try the camera window
            s_rvcFake = !s_rvcFake;
        } else if (wp == 'C') {
            DemoRing();
        } else if (wp == VK_ESCAPE) {
            DestroyWindow(hwnd);
        }
        return 0;
#endif
    case WM_COPYDATA: {   // an add-on's command (vh_plugins.inc, docs/plugin-api.md)
        const COPYDATASTRUCT* c = (const COPYDATASTRUCT*)lp;
        if (!c || c->dwData != VHAP_MAGIC || !c->lpData)
            return FALSE;
        static wchar_t cmd[1024];
        int n = (int)(c->cbData / sizeof(wchar_t));
        n = n > 1023 ? 1023 : n;
        memcpy(cmd, c->lpData, n * sizeof(wchar_t));
        cmd[n] = 0;
        PlugReceive((HWND)wp, cmd);
        return TRUE;
    }
    case MM_WOM_DONE:   // the camera beep has been played
        VoiceDone();
        return 0;
    case WM_CLOSE: {   // HU 2026-11-15-2: closed twice ~10 s after the start, code 0, by whom unknown - said here
        HWND f = GetForegroundWindow();
        wchar_t cls[64] = L"";
        DWORD pid = 0;
        if (f) {
            GetClassName(f, cls, 64);
            GetWindowThreadProcessId(f, &pid);
        }
        VLog(L"выход: WM_CLOSE извне (впереди окно «%s», процесс %08X)", cls, pid);
        break;
    }
    case WM_DESTROY:
        s_quitting = true;
        VLog(L"выход: окно VestaHome закрывается");
        PluginsQuit();
        UiRememberTick();   // the screen and the source as they are now, and what waits for the minute's save
        if (s_cfgSoon) {
            s_cfgSoon = false;
            SaveCfg();
        }
        if (s_voice || s_plugVoice)
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
    {
        wchar_t p[MAX_PATH];
        LogsOnPath(p);
        s_logsOn = GetFileAttributes(p) != 0xFFFFFFFF;
        s_on[T_LOGS] = s_logsOn;
    }
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
    VLog(L"=== VestaHome %s запущен", VhVersion());
    StockBack();   // started by hand while the stock shell is on: back to VestaHome through a restart (asked first)
    {   // the screen's real colour depth (VestaHome draws into 32-bit buffers, GDI converts when it is less)
        HDC sdc = GetDC(NULL);
        VLog(L"экран: %d x %d, %d бит на точку", GetDeviceCaps(sdc, HORZRES), GetDeviceCaps(sdc, VERTRES),
             GetDeviceCaps(sdc, BITSPIXEL) * GetDeviceCaps(sdc, PLANES));
        ReleaseDC(NULL, sdc);
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
        __try {
            DispatchMessage(&msg);
        } __except (CrashNote(GetExceptionInformation(), L"окне")) {
        }
    }
    return s_exitCode;
}
