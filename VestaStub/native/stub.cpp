// VestaStub: the stand-in for the stock shell (Windows CE / ARM, VS2005 -- no C++11).
//
// Put in place of \ResidentFlash\navigation\app\G3NavHMI.exe; the original is renamed G3NavHMI_orig.exe in the
// same folder. At the boot it starts VestaHome instead of the stock shell and watches it:
//   - VestaHome not there, or the flag VestaStub.off on the SD card / USB disk -> the original shell;
//   - the counter VestaStub.cnt (beside us) counts the starts of VestaHome that did not live 3 minutes: +1 before
//     each start, 0 once it has run 3 minutes (so at most two writes to the flash per boot); at 3 the original
//     shell is started instead - the stub is off until the counter is deleted (or set to 0);
//   - VestaHome fell (an exit code that is not 0) -> started again (the counter above stops a loop);
//   - VestaHome closed itself (code 0) -> the original shell;
//   - VestaHome closed by VestaHomeInstall updating it (it holds the mutex "VestaHomeUpdate" meanwhile) -> waited
//     for the update to end, then the new VestaHome (not counted as a failed start).
// While VestaHome runs, a second copy of ours ("G3NavHMI.exe /mcu", McuHost) does for the MCU what the stock shell
// did (Ghidra 2026-10-21, docs/MCU.md "Обработчики в оболочке"): it registers the skeletons - without them the MCU's
// messages reach nobody, VestaHome's ring included - and answers the messages the shell answered. A process of its
// own: the watchdog never loads SPIPowerMgr.dll. It falls - started again (3 times); VestaStub.nomcu on the SD card /
// USB disk - not started (a test without it). It goes before the original shell is started.
// The log only on removable media: \SDMemory\VestaLogs\VestaStub.log (only with \SDMMC\VestaLogs.on). At the boot the
// card is often not mounted yet (2026-10-24: no VestaStub.log ever): the lines go to RAM (\Temp\VestaStub.log, not the
// flash) and are copied to the card as soon as it is there.
// The PC build (testing) maps the HU's paths "\X\..." to "<exe folder>\hu\X\..." and takes the 3 minutes from
// the environment variable VESTASTUB_OKMS.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#define STBI_NO_STDIO   // (the boot picture: PNG / JPEG, as VestaHome's photos - stb_image from VestaHome/native)
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_SIMD
#define STBI_NO_THREAD_LOCALS
#define STBI_ASSERT(x) ((void)0)
#define STB_IMAGE_IMPLEMENTATION
#include "callroute.h"
#include "stockwatch.h"
#include "clockfix.h"   // (VestaHome's native\: the MCU's clock without its date)
#include "../../SPIPower/native/mcushm.h"   // VestaHome's ring of MCU messages (ShmPut)
#include "../../SPIPower/native/mcucmd.h"   // VestaHome's MCU calls (CmdThread)
#include "../../SPIPower/bt/g_btproxy.h"   // the SDK's functions (g_real[], bt_proxy.cpp built in with BT_HOST)
#include "stb_image.h"

static const int kMaxFails = 3;
static wchar_t s_dir[MAX_PATH];   // our folder, with the trailing '\'

static void HuPath(wchar_t* out, const wchar_t* p)   // a path on the HU
{
#ifdef UNDER_CE
    wcsncpy(out, p, MAX_PATH - 1);
#else
    _snwprintf(out, MAX_PATH - 1, L"%shu%s", s_dir, p);
#endif
    out[MAX_PATH - 1] = 0;
}

static bool There(const wchar_t* p)
{
    return GetFileAttributes(p) != 0xFFFFFFFF;
}

// ---- the boot picture (the user 2026-10-26: not a black screen while VestaHome loads; the stock shell shows its
// NEW_LOGO.PNG right after the firmware's logo). From the stub's start until VestaHome's window is in sight (or the
// stock shell's - a fallback - or 90 s): the picture over the whole screen and three dots running under it. The
// picture: one's own Skin\bootmy.png (or .jpg) as it is; else VestaHome's Skin\boot.png with the soft spot of the
// accent chosen in VestaHome (as on its saver: 16 %, the same shape and place; screen.accent of VestaHome.ini); else
// the stock shell's own NEW_LOGO.PNG on the unit.
static volatile bool s_splashEnd;
static HBITMAP s_splashBmp;
static int     s_splashDot;

static void SplashGlow(DWORD* px);
static bool SplashLoad(const wchar_t* path, bool glow);
static void HuPath(wchar_t* out, const wchar_t* path);
static void Log(const wchar_t* fmt, ...);

// The pictures in their order: ours (one's own, then VestaHome's), then the stock shell's. The built-in card \SDMMC
// comes up seconds after the stub starts (HU 2026-10-27: 4.5 s - the stock picture was shown though ours was
// there): the stock one is shown meanwhile, and ours replaces it as soon as the card is there (the timer, 300 ms).
// (VestaHome on \ResidentFlash since 2026-10-27: there at once; \SDMMC - an older install)
static const wchar_t* const kSplashPics[7] = {
    L"\\ResidentFlash\\VestaHome\\Skin\\bootmy.png", L"\\ResidentFlash\\VestaHome\\Skin\\bootmy.jpg",
    L"\\ResidentFlash\\VestaHome\\Skin\\boot.png",
    L"\\SDMMC\\VestaHome\\Skin\\bootmy.png", L"\\SDMMC\\VestaHome\\Skin\\bootmy.jpg", L"\\SDMMC\\VestaHome\\Skin\\boot.png",
    L"\\ResidentFlash\\navigation\\app\\res\\img\\Itelma\\MMC27\\Main\\NEW_LOGO.PNG" };
static const int kSplashStock = 6;
static int s_splashPick = -1;   // the one shown

// The first picture that is there, if it comes before the one shown; true = a new one.
static bool SplashPick()
{
    int upto = s_splashPick < 0 ? kSplashStock + 1 : s_splashPick;
    for (int i = 0; i < upto; i++) {
        wchar_t path[MAX_PATH];
        HuPath(path, kSplashPics[i]);
        if (GetFileAttributes(path) == 0xFFFFFFFF)
            continue;
        HBITMAP old = s_splashBmp;
        s_splashBmp = NULL;
        if (SplashLoad(path, i == 2 || i == 5)) {   // the spot only on VestaHome's boot.png
            if (old)
                DeleteObject(old);
            s_splashPick = i;
            Log(L"загрузка: картинка %s", kSplashPics[i]);
            return true;
        }
        s_splashBmp = old;
    }
    return false;
}

static bool SplashLoad(const wchar_t* path, bool glow)   // the picture into an 800 x 480 DIB (centred, cut)
{
    HANDLE f = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return false;
    DWORD size = GetFileSize(f, NULL), got = 0;
    unsigned char* data = size > 0 && size < 8 * 1024 * 1024 ? (unsigned char*)malloc(size) : NULL;
    bool read = data && ReadFile(f, data, size, &got, NULL) && got == size;
    CloseHandle(f);
    int w = 0, h = 0, n = 0;
    unsigned char* px = read ? stbi_load_from_memory(data, (int)size, &w, &h, &n, 3) : NULL;
    free(data);
    if (!px)
        return false;
    BITMAPINFO bi;
    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = 800;
    bi.bmiHeader.biHeight = -480;   // top down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = NULL;
    s_splashBmp = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (s_splashBmp && bits) {
        DWORD* dst = (DWORD*)bits;
        memset(dst, 0, 800 * 480 * 4);
        int ox = (w - 800) / 2, oy = (h - 480) / 2;
        for (int y = 0; y < 480; y++) {
            int sy = y + oy;
            if (sy < 0 || sy >= h)
                continue;
            for (int x = 0; x < 800; x++) {
                int sx = x + ox;
                if (sx < 0 || sx >= w)
                    continue;
                const unsigned char* q = px + (sy * w + sx) * 3;
                dst[y * 800 + x] = (DWORD)q[0] << 16 | (DWORD)q[1] << 8 | q[2];
            }
        }
        if (glow)
            SplashGlow(dst);
    }
    stbi_image_free(px);
    return s_splashBmp != NULL;
}

// The accent of VestaHome's settings (a Lada Vesta paint code, VestaHome's kAccents); Марс when not found.
static COLORREF SplashAccent()
{
    static const struct { int code; COLORREF c; } kAcc[] = { { 130, RGB(0xff, 0x6b, 0x1a) }, { 195, RGB(0xdc, 0x3c, 0x3c) },
        { 247, RGB(0xc3, 0xb8, 0x9c) }, { 372, RGB(0x72, 0xc0, 0x59) }, { 476, RGB(0x4f, 0x7a, 0xca) },
        { 619, RGB(0xab, 0xba, 0xd8) }, { 691, RGB(0xee, 0xee, 0xee) } };
    wchar_t path[MAX_PATH];
    HuPath(path, L"\\ResidentFlash\\VestaHome\\VestaHome.ini");
    if (!There(path))
        HuPath(path, L"\\SDMMC\\VestaHome\\VestaHome.ini");   // (an older install)
    HANDLE f = CreateFile(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    int code = 130;
    if (f != INVALID_HANDLE_VALUE) {
        static char buf[16384];
        DWORD got = 0;
        ReadFile(f, buf, sizeof(buf) - 1, &got, NULL);
        CloseHandle(f);
        buf[got] = 0;
        const char* k = strstr(buf, "\nscreen.accent=");
        if (k)
            code = atoi(k + 15);
    }
    for (int i = 0; i < (int)(sizeof(kAcc) / sizeof(kAcc[0])); i++)
        if (kAcc[i].code == code)
            return kAcc[i].c;
    return kAcc[0].c;
}

// The saver's spot (VestaHome: SaverSpot) over the picture: exp(-2.2 r^2) around (400, 192), 16 % of the accent.
static void SplashGlow(DWORD* px)
{
    COLORREF c = SplashAccent();
    int ar = GetRValue(c), ag = GetGValue(c), ab = GetBValue(c);
    for (int y = 0; y < 480; y++)
        for (int x = 0; x < 800; x++) {
            double dx = (x - 400) / 330.0, dy = (y - 192) / 200.0;
            int a = (int)(10486 * exp(-2.2 * (dx * dx + dy * dy)));   // of 65536
            if (a <= 0)
                continue;
            DWORD p = px[y * 800 + x];
            int r = p >> 16 & 255, g = p >> 8 & 255, b = p & 255;
            r += ((ar - r) * a + 32768) >> 16;
            g += ((ag - g) * a + 32768) >> 16;
            b += ((ab - b) * a + 32768) >> 16;
            px[y * 800 + x] = (DWORD)r << 16 | (DWORD)g << 8 | b;
        }
}

static void SplashDots(HDC dc)   // three dots under the picture, the one lit runs
{
    for (int i = 0; i < 3; i++) {
        bool on = i == s_splashDot % 3;
        HBRUSH b = CreateSolidBrush(on ? RGB(255, 255, 255) : RGB(90, 90, 90));
        HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, GetStockObject(NULL_PEN));
        int cx = 376 + i * 24, r = on ? 6 : 4;
        Ellipse(dc, cx - r, 452 - r, cx + r + 1, 452 + r + 1);
        SelectObject(dc, op);
        SelectObject(dc, ob);
        DeleteObject(b);
    }
}

static LRESULT CALLBACK SplashProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        HDC mem = CreateCompatibleDC(dc);
        HBITMAP buf = CreateCompatibleBitmap(dc, 800, 480);
        HGDIOBJ ob = SelectObject(mem, buf);
        if (s_splashBmp) {
            HDC pic = CreateCompatibleDC(dc);
            HGDIOBJ op = SelectObject(pic, s_splashBmp);
            BitBlt(mem, 0, 0, 800, 480, pic, 0, 0, SRCCOPY);
            SelectObject(pic, op);
            DeleteDC(pic);
        } else {
            RECT r = { 0, 0, 800, 480 };
            FillRect(mem, &r, (HBRUSH)GetStockObject(BLACK_BRUSH));
        }
        SplashDots(mem);
        BitBlt(dc, 0, 0, 800, 480, mem, 0, 0, SRCCOPY);
        SelectObject(mem, ob);
        DeleteObject(buf);
        DeleteDC(mem);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_TIMER: {
        HWND vh = FindWindow(L"VestaHome_Wnd", NULL);
        if (s_splashEnd || (vh && IsWindowVisible(vh)) || FindWindow(NULL, L"RadioCommWindow") || wp == 2) {
            DestroyWindow(hwnd);
            return 0;
        }
        s_splashDot++;
        if ((s_splashPick < 0 || s_splashPick == kSplashStock) && SplashPick()) {   // the card came: ours
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        RECT r = { 350, 436, 450, 468 };
        InvalidateRect(hwnd, &r, FALSE);
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

static DWORD WINAPI SplashThread(LPVOID)
{
    SplashPick();   // whatever is there now: at the very start the built-in card often is not yet
    WNDCLASS wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = SplashProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"VestaStub_Splash";
    RegisterClass(&wc);
    HWND w = CreateWindowEx(WS_EX_TOPMOST, L"VestaStub_Splash", L"VestaHome", WS_POPUP | WS_VISIBLE, 0, 0, 800, 480,
                            NULL, NULL, wc.hInstance, NULL);
    if (!w)
        return 0;
    SetTimer(w, 1, 300, NULL);
    SetTimer(w, 2, 90000, NULL);   // at the most
    MSG m;
    while (GetMessage(&m, NULL, 0, 0))
        DispatchMessage(&m);
    if (s_splashBmp)
        DeleteObject(s_splashBmp);
    s_splashBmp = NULL;
    return 0;
}

static bool s_logFlushed;   // the RAM log copied to the card

static void Append(const wchar_t* path, const char* b, DWORD n)
{
    HANDLE f = CreateFile(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return;
    DWORD wr;
    SetFilePointer(f, 0, NULL, FILE_END);
    WriteFile(f, b, n, &wr, NULL);
    CloseHandle(f);
}

static void Log(const wchar_t* fmt, ...)
{
    wchar_t dir[MAX_PATH], path[MAX_PATH], ram[MAX_PATH];
    HuPath(path, L"\\SDMMC\\VestaLogs.on");   // VestaHome's switch of all our logs ("Diagnostics -> Logs")
    if (!There(path))
        return;
    wchar_t w[300];
    SYSTEMTIME t;
    GetLocalTime(&t);
    int k = _snwprintf(w, 40, L"%04d-%02d-%02d %02d:%02d:%02d %6u ", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute,
                       t.wSecond, GetTickCount() / 1000);
    va_list a;
    va_start(a, fmt);
    _vsnwprintf(w + k, 296 - k, fmt, a);
    va_end(a);
    w[296] = 0;
    wcscat(w, L"\r\n");
    char b[600];
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, b, sizeof(b), NULL, NULL);
    if (n <= 1)
        return;
    HuPath(ram, L"\\Temp\\VestaStub.log");
    HuPath(dir, L"\\SDMemory\\VestaLogs");
    HuPath(path, L"\\SDMemory\\VestaLogs\\VestaStub.log");
    if (!s_logFlushed) {
        Append(ram, b, n - 1);
        CreateDirectory(dir, NULL);   // fails without the card: the lines wait in RAM
        if (!There(dir))
            return;
        HANDLE f = CreateFile(ram, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        if (f != INVALID_HANDLE_VALUE) {   // everything so far, this line included
            static char buf[16384];
            DWORD got = 0;
            ReadFile(f, buf, sizeof(buf), &got, NULL);
            CloseHandle(f);
            Append(path, buf, got);
        }
        s_logFlushed = true;
        return;
    }
    Append(path, b, n - 1);
}

static int ReadCount()
{
    wchar_t p[MAX_PATH];
    _snwprintf(p, MAX_PATH - 1, L"%sVestaStub.cnt", s_dir);
    p[MAX_PATH - 1] = 0;
    HANDLE f = CreateFile(p, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return 0;
    char b[16] = { 0 };
    DWORD n = 0;
    ReadFile(f, b, sizeof(b) - 1, &n, NULL);
    CloseHandle(f);
    return atoi(b);
}

static void WriteCount(int c)
{
    wchar_t p[MAX_PATH];
    _snwprintf(p, MAX_PATH - 1, L"%sVestaStub.cnt", s_dir);
    p[MAX_PATH - 1] = 0;
    HANDLE f = CreateFile(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) {
        Log(L"счётчик не записан (%u)", GetLastError());
        return;
    }
    char b[16];
    DWORD n = 0;
    _itoa(c, b, 10);
    WriteFile(f, b, strlen(b), &n, NULL);
    // onto the flash now (HU logs 2026-11-08: the power cut 9 s after "3" - the next boot read 2 and tried a 4th time)
    FlushFileBuffers(f);
    CloseHandle(f);
}

static HANDLE Start(const wchar_t* exe, const wchar_t* args)
{
    PROCESS_INFORMATION pi;
    memset(&pi, 0, sizeof(pi));
#ifdef UNDER_CE
    if (!CreateProcess(exe, args && args[0] ? (LPWSTR)args : NULL, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi))
        return 0;
#else
    STARTUPINFO si;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    wchar_t cl[MAX_PATH * 2];
    _snwprintf(cl, MAX_PATH * 2 - 1, L"\"%s\" %s", exe, args ? args : L"");
    cl[MAX_PATH * 2 - 1] = 0;
    if (!CreateProcess(exe, cl, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
        return 0;
#endif
    CloseHandle(pi.hThread);
    return pi.hProcess;
}

// ---- the MCU's host (McuHost, in "G3NavHMI.exe /mcu")
// A skeleton is an object whose vtable's slot 1 SPIPowerMgr calls with the message [code hi][code lo][len][data..]
// (slot 0: its destructor, never called); a hand-made table - the same call on ARM and in the PC build.
typedef int (*SpFn0)(void);
typedef int (*SpFn1)(int);
typedef int (*SpFn2)(int, void*);
typedef int (*SpFn3)(int, int, int);
static SpFn1 s_spDeMute, s_spRvc, s_svRoute;
static SpFn3 s_spMedia;
typedef int (*SpFnII)(int, int);
static SpFn3  s_spCard;       // SPApi_SendMediaCardStatus(dev, in, state)
static SpFnII s_spCardNew;    // SPApi_SendMediaCardIsNewStatus(new, dev)
static SpFn1  s_spBtOpen;     // SPApi_SendBTOpenStatus(on)
static SpFn1  s_spBtChan;     // SPApi_SendRadioBTAudioChannelChoiceStatus(1)
static SpFnII s_spPhone;      // SPApi_SendBTPhoneStatus(type, state)
static SpFn1  s_spMatch;      // SPApi_SendBTMatchStatus(connected)
static SpFn0  s_spBtRoute;    // SPApi_SendRadioBTRouteChange()
static SpFn1  s_spPrivacy;    // SPApi_SendRadioBTPrivacyStatus(on the phone)
static bool   s_btUp;         // our Bluetooth stack runs (BtHostStart)
static SpFn1  s_spSelDev;     // SPApi_SendSelectedDevice(dev): the media player opened on the SD card (0) / USB (1)
static SpFn0  s_btPlay, s_btPause;   // the phone's music (Flc_Bluetooth_Av_Play / _Av_Pause, our BT proxy)
static bool   InCall();       // (a call going on: BtCb's, below)
static void   ShmPut(int skel, int code, int len, const unsigned char* data);   // (VestaHome's ring, below)

// ---- the inbound trace (2026-10-25): the MCU resets the unit ~40 s after the shell is gone (its watchdog misses the
// shell's periodic signal). To see what the MCU expected, every message the skeleton handler receives is recorded into
// a ring in memory here -- no file I/O on SPIPowerMgr's receive thread -- and McuHost's idle loop flushes it to the log.
// The stream then ends right at the last message before the reset.
struct InMsg {
    DWORD         tick;   // GetTickCount() when it came
    unsigned      code;   // the MCU message id
    unsigned char skel;   // the skeleton id it arrived on
    unsigned char len;    // the payload length
    unsigned char p[4];   // the payload's first bytes
};
#define RING_SLOTS 256
static InMsg s_ring[RING_SLOTS];
static LONG  s_ringPos;   // messages recorded so far
static LONG  s_flushed;   // messages handed to the log
static CRITICAL_SECTION s_ringCs;

// the minute's counts, for the summary (a periodic signal shows up as a steady high count)
struct InCnt { unsigned code; unsigned count; };
#define CNT_SLOTS 64
static InCnt s_cnt[CNT_SLOTS];
static int   s_cntN;
static DWORD s_cntTick;

static void RingRecord(int skel, int code, int len, const unsigned char* data)
{
    EnterCriticalSection(&s_ringCs);
    int n = s_ringPos;
    InMsg* m = &s_ring[n & (RING_SLOTS - 1)];
    m->tick = GetTickCount();
    m->code = (unsigned)code;
    m->skel = (unsigned char)skel;
    m->len  = (unsigned char)len;
    for (int k = 0; k < 4; k++)
        m->p[k] = k < len ? data[k] : 0;
    s_ringPos = n + 1;
    LeaveCriticalSection(&s_ringCs);
}

static void RingFlush()
{
    static InMsg buf[RING_SLOTS];
    int skipped = 0;
    EnterCriticalSection(&s_ringCs);
    int from = s_flushed, to = s_ringPos;
    if (to - from > RING_SLOTS) {
        skipped = to - from - RING_SLOTS;
        from = to - RING_SLOTS;
    }
    for (int i = from; i < to; i++)
        buf[i - from] = s_ring[i & (RING_SLOTS - 1)];
    s_flushed = to;
    LeaveCriticalSection(&s_ringCs);

    if (skipped)
        Log(L"mcu: кольцо переполнено, пропущено %d", skipped);
    for (int k = 0; k < to - from; k++) {   // not i: eVC4's cl 12.20 has no conforming for-scope (build_evc4.bat)
        const InMsg* m = &buf[k];
        Log(L"mcu: вх. skel=%d 0x%04X len=%u p=%02X %02X %02X %02X", (int)m->skel, m->code, (unsigned)m->len,
            (unsigned)m->p[0], (unsigned)m->p[1], (unsigned)m->p[2], (unsigned)m->p[3]);
        int j;
        for (j = 0; j < (int)s_cntN; j++)
            if (s_cnt[j].code == m->code) { s_cnt[j].count++; break; }
        if (j == (int)s_cntN && s_cntN < CNT_SLOTS) { s_cnt[s_cntN].code = m->code; s_cnt[s_cntN].count = 1; s_cntN++; }
    }
}

struct Skel {
    void** vtbl;
    int    id;
};

static void SkelDtor(void*)
{
}

static bool DirThere(const wchar_t* hu)
{
    wchar_t p[MAX_PATH];
    HuPath(p, hu);
    DWORD a = GetFileAttributes(p);
    return a != 0xFFFFFFFF && (a & FILE_ATTRIBUTE_DIRECTORY);
}

// What the shell itself does on a message (the rest only fed its screens). p[3] is the data's first byte (almost
// always 00), p[4] the second.
static int s_dial = CALL_IDLE;   // the call's state (BtCb)
static bool s_hfAudio;           // the call's sound is in the car (BtCb)
static int s_btRoute = 2;        // the route asked for with the channel

static void SkelHandle(void* self, unsigned char* p)
{
    int code = p[0] << 8 | p[1];
    RingRecord(((Skel*)self)->id, code, p[2], p + 3);
    ShmPut(((Skel*)self)->id, code, p[2], p + 3);
    switch (code) {
    case 0x3901: {   // the MCU's clock: hour, minute, second, year - 2000, month, day -> the system's (CClock);
                     // its date lost (2005, HU 2026-11-20): the time of day only, while the system's date is no real one
        SYSTEMTIME t;
        GetLocalTime(&t);
        int how = McuClock(p + 3, t.wYear, &t);
        if (how != MCLK_NONE)
            SetLocalTime(&t);
        if (how == MCLK_FULL)
            Log(L"mcu: часы %02d:%02d:%02d %02d.%02d.%04d", t.wHour, t.wMinute, t.wSecond, t.wDay, t.wMonth, t.wYear);
        else
            Log(L"mcu: часы MCU %s: %02X %02X %02X %02X %02X %02X %02X", how == MCLK_TIME ? L"без даты - взято время суток"
                : L"не приняты", p[3], p[4], p[5], p[6], p[7], p[8], p[9]);
        break;
    }
    case 0x310E: {   // the MCU asks for a source's sound path: the route (5 - 3, 6..10 - 2), then "unmute it"
        int src = p[4], route = src == 5 ? 3 : src >= 6 && src <= 10 ? 2 : -1;
        if (route < 0)
            break;
        if (s_svRoute)
            s_svRoute(route);
        if (s_spDeMute)
            s_spDeMute(src);
        Log(L"mcu: звук источника %d -> маршрут %d", src, route);
        break;
    }
    case 0x4101:   // the reversing camera on / off: acknowledged (the picture is VestaHome's)
        if (s_spRvc)
            s_spRvc(p[4] ? 1 : 0);
        Log(L"mcu: задний ход %d", p[4]);
        break;
    case 0x9201: {   // is there media on the SD card (0) / the USB disk (1)? The shell answers 1 when its card
                     // manager has found files, 2 in a state not known yet; here: 1 = the card is there
        int dev = p[4];
        if (dev > 1)
            break;
        int has = dev == 0 || DirThere(L"\\USBDevice") ? 1 : 0;   // (the SD: always - CardsTell)
        if (s_spMedia)
            s_spMedia(dev, has, 3);   // (3: the music - as the shell's card managers send it; 0 is no type)
        Log(L"mcu: медиа %s -> %d", dev ? L"USB" : L"SD", has);
        break;
    }
    case 0x8A01:   // is Bluetooth on? (the shell: BTApi_GetOpenStatus() == 1)
        if (s_spBtOpen)
            s_spBtOpen(s_btUp ? 1 : 0);
        Log(L"mcu: Bluetooth включён? -> %d", s_btUp ? 1 : 0);
        break;
    case 0x9101: {   // the MCU's player screens (00 05 06 - the SD's folders, after SendMediaSourceSelect(5, 3) or a
                     // "back" from the player; 00 00 02 - the SD's player, after MODE). SendSelectedDevice(0 SD / 1 USB)
                     // opens the player from the folders.
        // Only to VestaHome's own "media" (its event VestaMediaAsk, up 2 s): the shell sends SendSelectedDevice only
        // from its folders' screen (Ghidra 2026-10-29: "back to the player") - answering every 0x9101 put the MCU
        // back into the player after each "back" 0x30, so the radio's key never reached its main menu.
        HANDLE ask = OpenEvent(EVENT_ALL_ACCESS, FALSE, L"VestaMediaAsk");
        bool ours = ask && WaitForSingleObject(ask, 0) == WAIT_OBJECT_0;
        if (ask) {
            if (ours)
                ResetEvent(ask);
            CloseHandle(ask);
        }
        int dev = 0;   // the SD - always "in" for the MCU (CardsTell): a USB disk pulled out takes nothing away
        if (ours && s_spSelDev)
            s_spSelDev(dev);
        Log(L"mcu: плеер медиа (%02X %02X) -> %s %d", p[4], p[5], ours ? L"устройство" : L"не наш запрос, без ответа",
            dev);
        break;
    }
    case 0x8903:   // the source is Bluetooth's music (01) or no more (00): the shell starts / stops the phone's music
                   // (Ghidra 2026-10-29, FUN_00666ac8: BTApi_PlayMusic / BTApi_StopMusic, not in a call)
        if (p[4] && s_btPlay)
            s_btPlay();
        else if (!p[4] && s_btPause && !InCall())
            s_btPause();
        Log(L"mcu: источник Bluetooth-музыка %d", p[4]);
        break;
    case 0x8905: {   // the MCU's answer to the phone's status: the shell asks for the channel here (Ghidra 2026-11-06,
                     // CBtPhone::MsgDecode; never sent before: no 0x8906 in any log, the radio played on through a call)
        if (p[5])
            break;
        int route = p[4] == 0 ? CallRouteTarget(s_dial, s_hfAudio) : p[4] == 1 ? CallRouteIncoming(s_hfAudio) : 0;
        if (!route)
            break;
        if (p[4] == 0 && s_dial == CALL_OUT)
            Sleep(500);   // (as the shell)
        s_btRoute = route;
        if (s_spBtChan)
            s_spBtChan(0);
        Log(L"mcu: звонок: запрос канала Bluetooth, маршрут %d", route);
        break;
    }
    case 0x8906:   // "the BT channel may be taken": the shell routes the sound to Bluetooth's and says so
        if (s_svRoute)
            s_svRoute(s_btRoute);
        if (s_spBtChan)
            s_spBtChan(1);
        Log(L"mcu: канал Bluetooth -> маршрут %d", s_btRoute);
        break;
    }
}

// The cards as the shell's card managers tell the MCU (Ghidra 2026-10-27, CSDCardManager / CUSBCardManager: attach
// FUN_00476584 / FUN_004761ac, search done FUN_00473040 / FUN_00472dc8, removed FUN_00473938): a card in -
// SendMediaCardStatus(dev, 1, 0), SendMediaCardIsNewStatus(0, dev), then SendMediaAvailability(dev, 1, type) for each
// kind of file found (3 - the music; 4 video, 1 / 2 others); out - SendMediaCardStatus(dev, 0, 2). Without it the MCU
// knew no source but the radio: MODE and VestaHome's switches did nothing (HU 2026-10-27). dev 0 = \SDMemory,
// 1 = \USBDevice. Looked at each second by the host; sent at the start and on each change.
static int s_cardWas[2] = { -1, -1 };

// The SD card always "in" (the user 2026-11-08: with no card the media source never came - the internet radio had no
// sound; with only a USB disk, pulling it out took the source away mid-stream): VestaHome's sound needs the source,
// not the card's files (VestaHome plays them itself), and the SD is the device it asks for (0x9101 below).
static void CardsTell()
{
    static const wchar_t* kVol[2] = { L"\\SDMemory", L"\\USBDevice" };
    for (int dev = 0; dev < 2; dev++) {
        int has = dev == 0 || DirThere(kVol[dev]) ? 1 : 0;
        if (has == s_cardWas[dev])
            continue;
        s_cardWas[dev] = has;
        if (has) {
            if (s_spCard)
                s_spCard(dev, 1, 0);
            if (s_spCardNew)
                s_spCardNew(0, dev);
            if (s_spMedia)
                s_spMedia(dev, 1, 3);
        } else if (s_spCard) {
            s_spCard(dev, 0, 2);
        }
        Log(L"mcu: носитель %s %s", dev ? L"USB" : L"SD", has ? L"есть (музыка)" : L"вынут");
    }
}

static void* s_skelVtbl[2] = { (void*)SkelDtor, (void*)SkelHandle };

static FARPROC Proc(HMODULE m, const char* name)   // CE's GetProcAddress takes the name in UTF-16
{
#ifdef UNDER_CE
    wchar_t w[64];
    MultiByteToWideChar(CP_ACP, 0, name, -1, w, 64);
    return GetProcAddress(m, w);
#else
    return GetProcAddress(m, name);
#endif
}

// ---- VestaHome's ring of MCU messages and its MCU calls (the user 2026-11-20, variant B: no proxy of SPIPowerMgr.dll,
// its two jobs are the host's - the same layouts, SPIPower/native/mcushm.h and mcucmd.h): every message our skeletons
// get goes to "VestaMCU2" (SkelHandle); VestaHome's calls queued in "VestaMCUCmd" are made here, in the MCU's host
// (the sound calls reach the MCU only from the host's process). The vendor's DLL itself, not the proxy: its copy
// SPIPowerMgr_orig.dll first (a unit with the proxy still installed).
static McuShm* s_shm;
static HMODULE s_sp;            // the vendor's SPIPowerMgr in this process
static McuCmd* s_cmd;
static HANDLE  s_cmdEv;

static void ShmPut(int skel, int code, int len, const unsigned char* data)
{
    if (!s_shm)
        return;
    LONG n = InterlockedIncrement((LONG*)&s_shm->seq);
    McuMsg* m = &s_shm->msg[(n - 1) & (MCU_SHM_SLOTS - 1)];
    m->seq = 0;
    m->tick = GetTickCount();
    m->id = (unsigned short)code;
    m->len = (unsigned char)len;
    m->skel = (unsigned char)skel;
    for (int k = 0; k < MCU_SHM_DATA; k++)
        m->data[k] = k < len ? data[k] : 0;
    m->seq = n;
}

static void ShmOpen()
{
    HANDLE h = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(McuShm), MCU_SHM_NAME);
    s_shm = h ? (McuShm*)MapViewOfFile(h, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(McuShm)) : NULL;
    if (s_shm && s_shm->magic != MCU_SHM_MAGIC) {
        s_shm->version = 2;
        s_shm->magic = MCU_SHM_MAGIC;
    }
    if (s_shm)
        InterlockedIncrement((LONG*)&s_shm->attach);
}

typedef void (*CallFn6)(unsigned, unsigned, unsigned, unsigned, unsigned, unsigned);

static DWORD WINAPI CmdThread(LPVOID)
{
    s_cmd->done = s_cmd->seq;   // what an earlier host left is not made again
    s_cmd->ready = (LONG)GetCurrentProcessId();
    for (;;) {
        WaitForSingleObject(s_cmdEv, 1000);
        while (s_cmd->done != s_cmd->seq) {
            LONG n = s_cmd->done + 1;
            if (s_cmd->seq - n >= MCU_CMD_SLOTS)   // fallen behind by a whole queue: from the oldest still there
                n = s_cmd->seq - MCU_CMD_SLOTS + 1;
            McuCall c = s_cmd->call[n % MCU_CMD_SLOTS];
            c.name[sizeof(c.name) - 1] = 0;
            CallFn6 f = !strncmp(c.name, "SPApi_", 6) ? (CallFn6)Proc(s_sp, c.name) : NULL;
            if (f)
                f(c.arg[0], c.arg[1], c.arg[2], c.arg[3], c.arg[4], c.arg[5]);
            Log(L"mcu: VH %S(%u, %u, %u)%s", c.name, c.arg[0], c.arg[1], c.arg[2], f ? L"" : L" - нет такой");
            s_cmd->done = n;
        }
    }
    return 0;
}

static void CmdStart()
{
    HANDLE m = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(McuCmd), MCU_CMD_NAME);
    s_cmd = m ? (McuCmd*)MapViewOfFile(m, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(McuCmd)) : NULL;
    s_cmdEv = CreateEvent(NULL, FALSE, FALSE, MCU_CMD_EVENT);
    if (!s_cmd || !s_cmdEv)
        return;
    s_cmd->magic = MCU_CMD_MAGIC;
    HANDLE t = CreateThread(NULL, 0, CmdThread, NULL, 0, NULL);
    if (t)
        CloseHandle(t);
}

// The vendor's DLL beside us: its copy *_orig.dll first (left by the proxies of earlier installs), else the DLL itself.
static HMODULE LoadVendor(const wchar_t* name, wchar_t* used)
{
    _snwprintf(used, MAX_PATH - 1, L"%s%s_orig.dll", s_dir, name);
    used[MAX_PATH - 1] = 0;
    HMODULE m = GetFileAttributes(used) != 0xFFFFFFFF ? LoadLibrary(used) : NULL;
    if (m)
        return m;
    _snwprintf(used, MAX_PATH - 1, L"%s%s.dll", s_dir, name);
    used[MAX_PATH - 1] = 0;
    return LoadLibrary(used);
}

// ---- Bluetooth without the stock shell (HU 2026-10-27: "прокси BT не установлен" - nobody started the stack; the
// phone connected only once the stock shell ran). The shell's NCL_BT.DLL does it (Ghidra 2026-10-27, FUN_10004f5c /
// FUN_10005018): Flc_Bluetooth_PowerOn(), Flc_Bluetooth_Init(its handler), Flc_Bluetooth_Conn_EnablePairIndForApp(1)
// (the pairing requests to the application: VestaHome answers them through the proxy); its shutdown -
// Flc_Bluetooth_Deinit(). Done here, in the MCU's host: FLC_BLUETOOTH_SDK.dll beside us is our proxy, which publishes
// the stack's events and does VestaHome's commands (btshm.h) - as in the shell. One SDK at a time: the host is told
// to close (VestaMcuHostQuit) before the stock shell starts, and deinits. \SDMemory\VestaStub.nobt - not started.
typedef int (*BtFn0)(void);
typedef int (*BtFn1)(int);
typedef void (*BtCbFn)(int* ev);
extern "C" void BtProxyAttach(HMODULE real);   // bt_proxy.cpp (BT_HOST): the BT proxy's work in our process
extern "C" int BtProxyInit(BtCbFn cb);
static BtFn0 s_btDeinit;
static int BtDeinitNow() { return ((BtFn0)g_real[IDX_Flc_Bluetooth_Deinit])(); }
static int BtAvPlay() { return ((BtFn0)g_real[IDX_Flc_Bluetooth_Av_Play])(); }
static int BtAvPause() { return ((BtFn0)g_real[IDX_Flc_Bluetooth_Av_Pause])(); }

// The calls (HU 2026-10-27-2: without the shell the radio played on through a call, and the call was not heard): the
// shell tells the MCU the phone's state (Ghidra 2026-10-27: NCL_BT's handler FUN_10005c6c keeps (type, state) and
// raises EVT_ID_CHANGE_BT_PHONE_STATUS, on which G3NavHMI FUN_0041a4a4 sends SPApi_SendBTPhoneStatus(type, state)).
// The SDK's events (code = NCL_BT's case + 1): 0x10 the call's setup on line ev[2] (1 - the first; ev[1] 0 idle -> (0, 0),
// 1 incoming -> (0, 1), 2 dialing / 3 alerting -> (2, 1), 4 active -> (4, 1) for an outgoing call, (5, 1) for an
// incoming one); 0x11 the hands-free link (ev[1] 2 up -> SendBTMatchStatus(1) + SendBTPhoneStatus(1, 1); 0 down ->
// (0, 0) + MatchStatus(0)); 0x12 the call's sound (ev[1] 1 in the car -> SendRadioBTRouteChange(); in a call also
// SendRadioBTPrivacyStatus: 0 in the car, 1 on the phone). The MCU then mutes the radio and gives the channel (0x8906).
static int s_callType = -1;   // the last type sent in a call (4 / 5 active), -1 none

static bool InCall()
{
    return s_callType >= 0;
}
static bool s_callOut;        // the call being set up is ours (dialing / alerting)

static void BtCb(int* ev)   // the shell's handler's place: the proxy has published the event already
{
    if (!ev)
        return;
    int type = -1, state = 0;
    bool matchDown = false;
    switch (ev[0]) {
    case 0x10:
        if (ev[2] != 1)
            break;
        s_dial = CallDial(ev[1], s_callOut);
        switch (ev[1]) {
        case 0: type = 0; state = 0; s_callOut = false; break;
        case 1: type = 0; state = 1; s_callOut = false; break;
        case 2:
        case 3: type = 2; state = 1; s_callOut = true; break;
        case 4: type = s_callOut ? 4 : 5; state = 1; break;
        }
        break;
    case 0x11:
        if (ev[1] == 2 || ev[1] == 0) {
            int up = ev[1] == 2;
            if (up && s_spMatch)
                s_spMatch(1);   // (down: after the phone's status, as the shell)
            matchDown = !up;
            type = up;
            state = up;
        }
        break;
    case 0x12:
        s_hfAudio = ev[1] == 1;
        if ((ev[1] == 0 || ev[1] == 1) && (s_callType == 4 || s_callType == 5) && s_spPrivacy)
            s_spPrivacy(ev[1] ? 0 : 1);
        if (ev[1] == 1 && s_spBtRoute)
            s_spBtRoute();
        Log(L"bt: звук звонка %s", ev[1] == 1 ? L"в машине" : L"нет");
        break;
    }
    if (type < 0)
        return;
    if (s_spPhone)
        s_spPhone(type, state);
    s_callType = state && type >= 4 ? type : -1;
    if (matchDown && s_spMatch)
        s_spMatch(0);
    Log(L"bt: телефон -> MCU (%d, %d)", type, state);
}

// The unit's name seen by phones: the stack reads HKLM\Drivers\BT\XCBTStack "DeviceName" when it starts (NCL_BT's
// BTApi_ChangeName writes there, Ghidra 2026-10-27). The registry comes from the firmware at each boot ("FLC WINCE");
// with the stock shell it was "VESTA" (MAIN/Backup/WindowsCE.reg) - something of the shell's set it. Now: VestaHome's
// bt.name (its VestaHome.ini), "VESTA" without one.
// VestaHome's setting from its ini ("\nkey=": UTF-8); empty - none
static void CfgValue(const char* key, char* v, int cap)
{
    wchar_t path[MAX_PATH];
    static const wchar_t* kIni[2] = { L"\\ResidentFlash\\VestaHome\\VestaHome.ini", L"\\SDMMC\\VestaHome\\VestaHome.ini" };
    v[0] = 0;
    for (int f = 0; f < 2; f++) {
        HuPath(path, kIni[f]);
        HANDLE h = CreateFile(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        if (h == INVALID_HANDLE_VALUE)
            continue;
        static char buf[16384];
        DWORD got = 0;
        ReadFile(h, buf, sizeof(buf) - 1, &got, NULL);
        CloseHandle(h);
        buf[got] = 0;
        const char* k = strstr(buf, key);
        if (k) {
            k += strlen(key);
            int n = 0;
            while (k[n] && k[n] != '\r' && k[n] != '\n' && n < cap - 1) {
                v[n] = k[n];
                n++;
            }
            v[n] = 0;
        }
        return;
    }
}

static void BtName()
{
    wchar_t name[40] = L"VESTA";
    char v[64];
    CfgValue("\nbt.name=", v, 64);
    if (v[0])
        MultiByteToWideChar(CP_UTF8, 0, v, -1, name, 40);   // (VestaHome's ini is UTF-8)
    name[39] = 0;
#ifdef UNDER_CE
    HKEY key;
    if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, L"Drivers\\BT\\XCBTStack", 0, NULL, 0, 0, NULL, &key, NULL) == ERROR_SUCCESS) {
        RegSetValueEx(key, L"DeviceName", 0, REG_SZ, (const BYTE*)name, (DWORD)(wcslen(name) + 1) * sizeof(wchar_t));
        RegCloseKey(key);
        Log(L"bt: имя ГУ «%s»", name);
    }
#else
    Log(L"bt: имя ГУ «%s» (ПК: не записывается)", name);
#endif
}

// The stack's options live in its memory only: the shell sets them at each start (FUN_0041a4a4, LAB_0041a794), from now
// on here, as VestaHome's settings have them (bt.autoconnect, bt.autoanswer; none - never touched by the user)
static void BtOptions(HMODULE m)
{
    typedef int (*BtFn2)(int, int);
    BtFn1 connect = (BtFn1)Proc(m, "Flc_Bluetooth_Conn_EnablePowerOnConnect");
    BtFn2 answer = (BtFn2)Proc(m, "Flc_Bluetooth_Conn_EnableAutoAnswerIncomingCall");
    char v[8];
    CfgValue("\nbt.autoconnect=", v, 8);
    if (connect && (v[0] == '0' || v[0] == '1')) {
        connect(v[0] == '1');
        Log(L"bt: автосоединение %c", v[0]);
    }
    CfgValue("\nbt.autoanswer=", v, 8);
    if (answer && (v[0] == '0' || v[0] == '1')) {
        answer(v[0] == '1', 5);
        Log(L"bt: автоответ %c", v[0]);
    }
}

static void BtHostStart()
{
    wchar_t p[MAX_PATH];
    HuPath(p, L"\\SDMemory\\VestaStub.nobt");
    if (There(p)) {
        Log(L"bt: флаг VestaStub.nobt — стек не запускается");
        return;
    }
    HMODULE m = LoadVendor(L"FLC_BLUETOOTH_SDK", p);   // the vendor's SDK; what the BT proxy did - built in
    if (!m) {
        Log(L"bt: нет %s (%u)", p, GetLastError());
        return;
    }
    Log(L"bt: %s", p);
    BtProxyAttach(m);   // the SDK's functions into g_real[] (bt_proxy.cpp; "failed" ones while Bluetooth is off)
    BtName();   // the unit's name, before the stack reads it
    BtFn0 powerOn = (BtFn0)Proc(m, "Flc_Bluetooth_PowerOn");
    BtFn1 pairInd = (BtFn1)g_real[IDX_Flc_Bluetooth_Conn_EnablePairIndForApp];
    s_btDeinit = BtDeinitNow;   // (through g_real at each call: "failed" while Bluetooth is off)
    s_btPlay = BtAvPlay;
    s_btPause = BtAvPause;
    if (!powerOn || !Proc(m, "Flc_Bluetooth_Init")) {
        Log(L"bt: в FLC_BLUETOOTH_SDK.dll нет PowerOn / Init");
        return;
    }
    int on = powerOn() & 0xFF;   // (the shell keeps it as a byte: non-zero = loaded)
    int r = on ? BtProxyInit(BtCb) : -1;
    if (r == 0 && pairInd)
        pairInd(1);
    if (r == 0)
        BtOptions(m);
    s_btUp = r == 0;
    Log(L"bt: стек %s (PowerOn %d, Init %d)", s_btUp ? L"запущен" : L"НЕ запущен", on, r);
}

static void BtHostStop()
{
    if (s_btUp && s_btDeinit) {
        s_btDeinit();
        Log(L"bt: стек остановлен");
    }
    s_btUp = false;
}

static int McuHost()
{
    wchar_t p[MAX_PATH];
    HMODULE sp = LoadVendor(L"SPIPowerMgr", p);   // the vendor's, by full path
    if (!sp) {
        Log(L"mcu: нет %s (%u)", p, GetLastError());
        return 1;
    }
    s_sp = sp;
    Log(L"mcu: %s", p);
    ShmOpen();
    CmdStart();
    InitializeCriticalSection(&s_ringCs);   // before the skeletons: SkelHandle records into the ring
    SpFn0 load = (SpFn0)Proc(sp, "SPApi_LoadRadioEngine");
    SpFn0 inited = (SpFn0)Proc(sp, "SPApi_IsInitialized");
    SpFn0 init = (SpFn0)Proc(sp, "SPApi_InitializeRadioEngine");
    SpFn2 skel = (SpFn2)Proc(sp, "SPApi_SetSkelonton");
    s_spDeMute = (SpFn1)Proc(sp, "SPApi_SendRadioDeMuteCurrentSource");
    s_spRvc = (SpFn1)Proc(sp, "SPApi_SendRadioReceivedRVCStatus");
    s_spMedia = (SpFn3)Proc(sp, "SPApi_SendMediaAvailability");
    s_spCard = (SpFn3)Proc(sp, "SPApi_SendMediaCardStatus");
    s_spCardNew = (SpFnII)Proc(sp, "SPApi_SendMediaCardIsNewStatus");
    s_spBtOpen = (SpFn1)Proc(sp, "SPApi_SendBTOpenStatus");
    s_spBtChan = (SpFn1)Proc(sp, "SPApi_SendRadioBTAudioChannelChoiceStatus");
    s_spPhone = (SpFnII)Proc(sp, "SPApi_SendBTPhoneStatus");
    s_spMatch = (SpFn1)Proc(sp, "SPApi_SendBTMatchStatus");
    s_spBtRoute = (SpFn0)Proc(sp, "SPApi_SendRadioBTRouteChange");
    s_spPrivacy = (SpFn1)Proc(sp, "SPApi_SendRadioBTPrivacyStatus");
    s_spSelDev = (SpFn1)Proc(sp, "SPApi_SendSelectedDevice");
    HMODULE sv = LoadLibrary(L"desay_sdk_api.dll");
    s_svRoute = sv ? (SpFn1)Proc(sv, "SV_AudioSetRoute") : NULL;
    if (!load || !inited || !init || !skel) {
        Log(L"mcu: в SPIPowerMgr.dll нет нужных функций");
        return 1;
    }
    // while it lives: VestaHome does the panel's buttons itself (the stock shell does not run), see its HwKey
    CreateEvent(NULL, TRUE, FALSE, L"VestaMcuHost");
    load();   // as the shell starts it (FUN_00677740)
    if (!inited())
        init();
    static const int kIds[] = { 1, 6, 2, 3, 4, 5, 7, 8, 9, 11, 12, 13, 14, 17, 16, 18, 15 };   // the shell's order
    static Skel skels[sizeof(kIds) / sizeof(kIds[0])];
    for (int i = 0; i < (int)(sizeof(kIds) / sizeof(kIds[0])); i++) {
        skels[i].vtbl = s_skelVtbl;
        skels[i].id = kIds[i];
        skel(kIds[i], &skels[i]);
        if (s_shm)   // (VestaHome's "Diagnostics": the handlers in place)
            InterlockedIncrement((LONG*)&s_shm->wraps);
    }
    // then as the shell's CMainDialog::SlotInitModule (Ghidra 2026-10-22): "the navigation started" (SPIPowerMgr's
    // power state 2 - perhaps what keeps the unit from resetting without the shell), "send the current source",
    // the media play times asked
    SpFn0 navStarted = (SpFn0)Proc(sp, "SPApi_NavStarted");
    SpFn0 sendDialog = (SpFn0)Proc(sp, "SPApi_TellRadioToSendDialog");
    SpFn1 askTimes = (SpFn1)Proc(sp, "SPApi_RequestMediaPlayTimeFromRadio");
    if (navStarted)
        navStarted();
    if (sendDialog)
        sendDialog();
    for (int k = 0; k < 4 && askTimes; k++)   // not i: eVC4's cl 12.20 has no conforming for-scope
        askTimes(k);
    Log(L"mcu: скелеты зарегистрированы (%d), запуск как у оболочки %s, маршрут звука %s",
        (int)(sizeof(kIds) / sizeof(kIds[0])), navStarted && sendDialog ? L"да" : L"НЕТ", s_svRoute ? L"есть" : L"НЕТ");
    BtHostStart();
    if (s_spBtOpen)   // the MCU learns that Bluetooth is there (its sources: BT music, the phone)
        s_spBtOpen(s_btUp ? 1 : 0);
    CardsTell();
    s_cntTick = GetTickCount();
    HANDLE quit = CreateEvent(NULL, TRUE, FALSE, L"VestaMcuHostQuit");
    for (;;) {
        if (quit && WaitForSingleObject(quit, 1000) == WAIT_OBJECT_0) {   // the stock shell comes: its Bluetooth
            BtHostStop();
            RingFlush();
            Log(L"mcu: хост закрыт (запускается штатная оболочка)");
            return 0;
        }
        RingFlush();
        CardsTell();   // a card put in / taken out: the MCU is told (its sources)
        Log(L"mcu: хост жив, всего %d", (int)s_ringPos);
        if (GetTickCount() - s_cntTick >= 60000) {
            s_cntTick = GetTickCount();
            wchar_t b[280];
            int k = _snwprintf(b, 280, L"mcu: входящих за минуту:");
            for (int i = 0; i < (int)s_cntN && k < 250; i++)
                k += _snwprintf(b + k, 280 - k, L" 0x%04X=%u", s_cnt[i].code, s_cnt[i].count);
            b[279] = 0;
            Log(L"%s", b);
            s_cntN = 0;
        }
    }
}

static HANDLE s_mcuHost;   // the MCU's host process
static int    s_hostStarts;

static void HostStart(const wchar_t* self)
{
    wchar_t p[MAX_PATH];
    HuPath(p, L"\\SDMemory\\VestaStub.nomcu");
    bool off = There(p);
    HuPath(p, L"\\USBDevice\\VestaStub.nomcu");
    off = off || There(p);
    if (off) {
        Log(L"mcu: флаг VestaStub.nomcu — хост MCU не запускается");
        s_hostStarts = 3;
        return;
    }
    if (s_hostStarts >= 3) {
        Log(L"mcu: хост MCU упал 3 раза — дальше без него");
        return;
    }
    s_hostStarts++;
    s_mcuHost = Start(self, L"/mcu");
}

static void HostStop()
{
    if (!s_mcuHost)
        return;
    HANDLE quit = CreateEvent(NULL, TRUE, FALSE, L"VestaMcuHostQuit");   // asked first: it stops Bluetooth itself
    if (quit) {
        SetEvent(quit);
        bool gone = WaitForSingleObject(s_mcuHost, 3000) == WAIT_OBJECT_0;
        ResetEvent(quit);
        CloseHandle(quit);
        if (gone) {
            CloseHandle(s_mcuHost);
            s_mcuHost = 0;
            return;
        }
    }
    TerminateProcess(s_mcuHost, 0);
    WaitForSingleObject(s_mcuHost, 2000);
    CloseHandle(s_mcuHost);
    s_mcuHost = 0;
}

static bool UpdateRunning()   // VestaHomeInstall replaces VestaHome's files (its mutex is there)
{
    HANDLE m = CreateMutex(NULL, FALSE, L"VestaHomeUpdate");
    bool held = m && GetLastError() == ERROR_ALREADY_EXISTS;
    if (m)
        CloseHandle(m);
    return held;
}

// ---- the unit's watchdog (Ghidra 2026-10-26: the ~40 s reboots without the shell). The shell's CMainDialog starts
// \ResidentFlash\navigation\app\HzWD.exe with its own process id ("%ld"); HzWD activates Drivers\WatchDog (WDG.dll
// pulses to the MCU) and every 6 s counts a miss: more than 7 (~48 s) without WM_RADIO_WATCHDOGEVENT (0x464, the
// shell broadcasts it every 5 s from a QTimer) - or that process gone - and it deactivates the driver: the MCU
// resets the unit. We do the same: HzWD with this process's id, 0x464 every 5 s from a thread of ours (VestaHome
// hanging does not reset the unit). From then on this process must live: a fallback to the shell keeps it.
enum { kWdMsg = 0x464, kWdEveryMs = 5000 };
static bool s_wdOn;

static DWORD WINAPI WdFeed(LPVOID)
{
    for (;;) {
        PostMessage(HWND_BROADCAST, kWdMsg, 0, 0);
        Sleep(kWdEveryMs);
    }
}

static void WatchdogStart()
{
    wchar_t wd[MAX_PATH], pid[16];
    HuPath(wd, L"\\ResidentFlash\\navigation\\app\\HzWD.exe");
    if (!There(wd)) {
        Log(L"сторож: нет %s", wd);
        return;
    }
    _snwprintf(pid, 16, L"%lu", GetCurrentProcessId());
    HANDLE h = Start(wd, pid);
    if (!h) {
        Log(L"сторож: HzWD.exe не запустился (%u)", GetLastError());
        return;
    }
    CloseHandle(h);
    HANDLE t = CreateThread(NULL, 0, WdFeed, NULL, 0, NULL);
    if (t)
        CloseHandle(t);
    s_wdOn = t != NULL;
    Log(L"сторож: HzWD.exe запущен (за процессом %s), сигнал 0x464 каждые %d с%s", pid, kWdEveryMs / 1000,
        t ? L"" : L" - ПОТОК НЕ СОЗДАН");
}

static int RunOriginal(const wchar_t* args, const wchar_t* why)
{
    s_splashEnd = true;   // (the stock shell shows its own)
    HostStop();   // the shell registers its own skeletons
    wchar_t orig[MAX_PATH];
    _snwprintf(orig, MAX_PATH - 1, L"%sG3NavHMI_orig.exe", s_dir);
    orig[MAX_PATH - 1] = 0;
    HANDLE h = Start(orig, args);
    Log(L"оригинальная оболочка (%s): %s", why, h ? L"запущена" : L"НЕ ЗАПУЩЕНА");
    if (h)
        CloseHandle(h);
    if (s_wdOn)   // HzWD watches this process: we stay (and keep feeding it; the shell's broadcasts feed it too)
        for (;;)
            Sleep(60000);
    return 0;
}

// The stock shell after "Штатная" watched (stockwatch.h): its window "RadioCommWindow" must be there by the deadline,
// else the unit is restarted once (the flag VestaStub.retry on the built-in card marks the boot after it).
static int StockWatch()
{
    wchar_t retry[MAX_PATH];
    HuPath(retry, L"\\SDMMC\\VestaStub.retry");
    bool restarted = There(retry);
    for (;;) {
        // its window there (not WM_NULL: a busy moment would cost a needless restart - HU 2026-11-14, OSMapsCE)
        HWND w = FindWindow(NULL, L"RadioCommWindow");
        switch (StockVerdict(w != NULL, GetTickCount(), restarted)) {
        case STOCK_ALIVE:
            DeleteFile(retry);
            Log(L"штатная оболочка ожила за %u с%s", GetTickCount() / 1000, restarted ? L" (после нашей перезагрузки)" : L"");
            return 0;
        case STOCK_GIVEUP:
            DeleteFile(retry);
            Log(L"штатная оболочка не ожила за %u с и после перезагрузки - оставляю", kStockDeadlineMs / 1000);
            return 0;
        case STOCK_RESTART: {
            HANDLE f = CreateFile(retry, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (f != INVALID_HANDLE_VALUE)
                CloseHandle(f);
            Log(L"штатная оболочка не ожила за %u с (её окна нет) - перезагрузка ГУ", kStockDeadlineMs / 1000);
#ifdef UNDER_CE
            typedef BOOL (*KernelIoFn)(DWORD, LPVOID, DWORD, LPVOID, DWORD, LPDWORD);
            HMODULE core = GetModuleHandle(L"coredll.dll");
            KernelIoFn kio = core ? (KernelIoFn)GetProcAddress(core, L"KernelIoControl") : NULL;
            if (kio)
                kio(0x0101003C, NULL, 0, NULL, 0, NULL);   // IOCTL_HAL_REBOOT, as VestaHome's Reboot()
#endif
            Log(L"перезагрузка не вышла (%u)", (unsigned)GetLastError());
            return 0;
        }
        }
        Sleep(500);
    }
}

static bool FindVesta(wchar_t* exe)
{
    static const wchar_t* kCand[] = { L"\\ResidentFlash\\VestaHome\\VestaHome.exe", L"\\SDMMC\\VestaHome\\VestaHome.exe",
                                      L"\\SDMemory\\VestaHome\\VestaHome.exe" };
    for (int i = 0; i < (int)(sizeof(kCand) / sizeof(kCand[0])); i++) {
        HuPath(exe, kCand[i]);
        if (There(exe))
            return true;
    }
    return false;
}

#ifdef UNDER_CE
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPWSTR args, int)
#else
int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR args, int)
#endif
{
    GetModuleFileName(NULL, s_dir, MAX_PATH);
    wchar_t* slash = wcsrchr(s_dir, L'\\');
    if (slash)
        slash[1] = 0;
    DWORD okMs = 180000;   // VestaHome alive this long: not a failed start
#ifndef UNDER_CE
    const char* e = getenv("VESTASTUB_OKMS");
    if (e)
        okMs = (DWORD)atoi(e);
#endif
    if (args && wcsstr(args, L"/mcu"))
        return McuHost();
    Log(L"=== VestaStub");
    {   // the boot picture until VestaHome is in sight (its own thread: the waits below go on)
        HANDLE t = CreateThread(NULL, 0, SplashThread, NULL, 0, NULL);
        if (t)
            CloseHandle(t);
    }

    wchar_t p[MAX_PATH];
    bool early = false;   // VestaHome on \ResidentFlash: started at once, the SD card's flag looked at after (below)
    {   // the boot: the volumes come up after us - wait a little for the SD card (its flag) and for VestaHome
        wchar_t sd[MAX_PATH], vh0[MAX_PATH];
        HuPath(sd, L"\\SDMemory");
        HuPath(vh0, L"\\ResidentFlash\\VestaHome\\VestaHome.exe");
        early = There(vh0) && !There(sd);
        DWORD t0 = GetTickCount();
        while (!early && !There(sd) && GetTickCount() - t0 < 6000)
            Sleep(250);
        while (!FindVesta(vh0) && GetTickCount() - t0 < 20000)
            Sleep(250);
        Log(L"тома: SD %s, VestaHome %s (через %u мс)", There(sd) ? L"есть" : L"нет", FindVesta(vh0) ? vh0 : L"не найден",
            GetTickCount() - t0);
    }
    HuPath(p, L"\\SDMemory\\VestaStub.off");
    if (There(p))
        return RunOriginal(args, L"флаг VestaStub.off на SD");
    HuPath(p, L"\\USBDevice\\VestaStub.off");
    if (There(p))
        return RunOriginal(args, L"флаг VestaStub.off на USB");
    {   // "Штатная" in VestaHome (the user 2026-11-13: through a restart - started on the fly the shell hung the unit):
        // its flag on the built-in card - the shell at each boot, our Bluetooth and MCU host never up; back - VestaHome
        // started by hand removes the flag and restarts the unit
        wchar_t card[MAX_PATH];
        HuPath(card, L"\\SDMMC");
        DWORD t0 = GetTickCount();
        while (!There(card) && GetTickCount() - t0 < 5000)
            Sleep(250);
        HuPath(p, L"\\SDMMC\\VestaStub.stock");
        if (There(p)) {
            RunOriginal(args, L"флаг VestaStub.stock - штатная по кнопке; назад - запустить VestaHome вручную");
            return StockWatch();
        }
    }
    wchar_t vh[MAX_PATH], self[MAX_PATH];
    if (!FindVesta(vh))
        return RunOriginal(args, L"нет VestaHome.exe");
    GetModuleFileName(NULL, self, MAX_PATH);
    WatchdogStart();   // (only when VestaHome goes in place of the shell: the shell starts its own)
    HostStart(self);

    for (;;) {
        int fails = ReadCount();
        if (fails >= kMaxFails)
            return RunOriginal(args, L"VestaHome не прожил 3 минуты 3 раза подряд; заново - удалить VestaStub.cnt");
        WriteCount(fails + 1);   // counted as failed until it has lived okMs
        HANDLE h = Start(vh, NULL);
        if (!h) {
            Log(L"VestaHome не запустился (%u): %s", GetLastError(), vh);
            Sleep(3000);   // (at the boot it may need a moment: not three tries in a blink)
            continue;   // counted: the third time gives the original
        }
        Log(L"VestaHome запущен (%d-я попытка): %s", fails + 1, vh);
        if (early) {   // the cards come a few seconds later: their stop flag still works (VestaHome closed, the shell)
            early = false;
            DWORD tw = GetTickCount();
            while (GetTickCount() - tw < 12000 && WaitForSingleObject(h, 250) == WAIT_TIMEOUT) {
                wchar_t f1[MAX_PATH], f2[MAX_PATH];
                HuPath(f1, L"\\SDMemory\\VestaStub.off");
                HuPath(f2, L"\\USBDevice\\VestaStub.off");
                if (There(f1) || There(f2)) {
                    TerminateProcess(h, 0);
                    CloseHandle(h);
                    HostStop();
                    WriteCount(0);
                    return RunOriginal(args, L"флаг VestaStub.off (VestaHome уже был запущен с \\ResidentFlash)");
                }
            }
        }
        bool lived = false;
        DWORD t0 = GetTickCount();
        for (;;) {   // VestaHome and the MCU's host: the host falls - again
            HANDLE w[2] = { h, s_mcuHost };
            DWORD left = lived ? INFINITE : GetTickCount() - t0 >= okMs ? 0 : okMs - (GetTickCount() - t0);
            DWORD r = WaitForMultipleObjects(s_mcuHost ? 2 : 1, w, FALSE, left);
            if (r == WAIT_TIMEOUT) {
                lived = true;
                WriteCount(0);
                Log(L"VestaHome работает %u с: счётчик сброшен", okMs / 1000);
            } else if (r == WAIT_OBJECT_0 + 1) {
                DWORD hc = 0;
                GetExitCodeProcess(s_mcuHost, &hc);
                CloseHandle(s_mcuHost);
                s_mcuHost = 0;
                Log(L"mcu: хост MCU завершился (код %08X)", hc);
                HostStart(self);
            } else {
                break;
            }
        }
        DWORD code = 1;
        GetExitCodeProcess(h, &code);
        CloseHandle(h);
        if (UpdateRunning()) {   // closed for an update: the new one when it is in place
            Log(L"VestaHome закрыт установщиком (код %08X): жду конца обновления", code);
            for (int i = 0; i < 600 && UpdateRunning(); i++)   // up to 5 min
                Sleep(500);
            if (!lived)
                WriteCount(fails);   // not a failed start
            Log(L"обновление закончено: запускаю VestaHome");
            continue;
        }
        if (code == 3) {   // "Выход" pressed (HU 2026-11-15-2: the stock shell came "for no reason" - it was this): the
            if (!lived)    // explorer instead, VestaHome may be started again by hand; we stay (watchdog, MCU host)
                WriteCount(0);
            Log(L"VestaHome закрыт кнопкой «Выход»: штатную не запускаю");
            for (;;)
                Sleep(60000);
        }
        if (code == 0) {   // closed itself (or a second copy that found the first)
            if (!lived)
                WriteCount(0);
            return RunOriginal(args, L"VestaHome закрылся сам");
        }
        Log(L"VestaHome упал (код %08X): снова", code);
        Sleep(1000);
    }
}
