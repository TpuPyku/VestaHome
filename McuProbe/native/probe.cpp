// McuProbe - the MCU without our proxy (the user 2026-11-19: "как в канпанели"): a second client of the stock
// SPIPowerMgr.dll beside whatever already runs (the stock shell, or VestaStub's MCU host), the way the CAN panel's
// can_panel.exe does it - LoadRadioEngine, InitializeRadioEngine, SetSkelonton for skeletons 1..0x12 - and for 5 min
// every message counted by skeleton and code. The question: does a second reader get all the messages (pic.dll gives
// each frame to every opener of PIC1:, by its code), and does the shell keep everything (keys, volume, radio, the
// camera, calls) meanwhile. Run from the card by hand; the log always (no VestaLogs.on: LogPlace), to
// \SDMemory\VestaLogs\McuProbe.log (else the USB disk's). Ends by ExitProcess, no Uninit (pic.dll's close drops our
// node only).
//
// The factory DLL is taken when it is there beside the proxy (SPIPowerMgr_orig.dll) - the probe is the no-proxy case;
// otherwise SPIPowerMgr.dll (the stock one on a unit without VestaHome). Sends nothing but the product info request
// (as can_panel does; its answer 0x7902 shows our own sending reaches us back too).
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>

static const wchar_t* kApp = L"\\ResidentFlash\\navigation\\app\\";
enum { kRunMs = 5 * 60 * 1000, kRing = 512, kCodes = 160, kFirstShown = 3 };

static wchar_t s_log[MAX_PATH];

// The log always (the user 2026-11-19: run on the CAN panel's unit, no VestaHome there to switch logs on - a probe
// without its log is no use), but only on removable media: the SD card, else a USB disk; neither - not run.
static bool LogPlace()
{
    static const wchar_t* kVol[2] = { L"\\SDMemory", L"\\USBDevice" };
    for (int i = 0; i < 2; i++) {
        DWORD a = GetFileAttributes(kVol[i]);
        if (a == 0xFFFFFFFF || !(a & FILE_ATTRIBUTE_DIRECTORY))
            continue;
        wchar_t dir[MAX_PATH];
        _snwprintf(dir, MAX_PATH, L"%s\\VestaLogs", kVol[i]);
        CreateDirectory(dir, NULL);
        _snwprintf(s_log, MAX_PATH, L"%s\\McuProbe.log", dir);
        return true;
    }
    return false;
}

static void Log(const wchar_t* fmt, ...)
{
    if (!s_log[0])
        return;
    wchar_t line[400];
    SYSTEMTIME t;
    GetLocalTime(&t);
    int n = _snwprintf(line, 64, L"%02d:%02d:%02d ", t.wHour, t.wMinute, t.wSecond);
    va_list a;
    va_start(a, fmt);
    _vsnwprintf(line + n, 400 - n - 3, fmt, a);
    va_end(a);
    line[397] = 0;
    wcscat(line, L"\r\n");
    char u8[1200];
    int len = WideCharToMultiByte(CP_UTF8, 0, line, -1, u8, sizeof(u8), NULL, NULL) - 1;
    HANDLE f = CreateFile(s_log, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, 0, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return;
    SetFilePointer(f, 0, NULL, FILE_END);
    DWORD w;
    WriteFile(f, u8, len > 0 ? len : 0, &w, NULL);
    CloseHandle(f);
}

// ---- what came: a ring filled on SPIPowerMgr's receive thread (no file there), emptied by the main loop
struct Msg { DWORD tick; unsigned short code; unsigned char skel, len, p[8]; };
static Msg  s_ring[kRing];
static LONG s_in, s_out;
static CRITICAL_SECTION s_cs;

struct Count { unsigned short code; unsigned char skel; unsigned n, minute; };
static Count s_cnt[kCodes];
static int   s_cntN;
static unsigned s_total;

// A skeleton: an object whose vtable's slot 1 SPIPowerMgr calls with [code hi][code lo][len][data..] (slot 0 its
// destructor, never called) - as VestaStub's MCU host (stub.cpp).
struct Skel { void** vtbl; int id; };
static void SkelDtor(void*)
{
}
static void SkelHandle(void* self, unsigned char* p)
{
    EnterCriticalSection(&s_cs);
    Msg& m = s_ring[s_in % kRing];
    m.tick = GetTickCount();
    m.code = (unsigned short)(p[0] << 8 | p[1]);
    m.skel = (unsigned char)((Skel*)self)->id;
    m.len = p[2];
    for (int k = 0; k < 8; k++)
        m.p[k] = k < p[2] ? p[3 + k] : 0;
    s_in++;
    LeaveCriticalSection(&s_cs);
}
static void* s_vtbl[2] = { (void*)SkelDtor, (void*)SkelHandle };

static Count* CountOf(unsigned short code, unsigned char skel)
{
    for (int i = 0; i < s_cntN; i++)
        if (s_cnt[i].code == code && s_cnt[i].skel == skel)
            return &s_cnt[i];
    if (s_cntN == kCodes)
        return NULL;
    Count* c = &s_cnt[s_cntN++];
    c->code = code;
    c->skel = skel;
    c->n = c->minute = 0;
    return c;
}

static void Flush()
{
    static Msg buf[kRing];
    EnterCriticalSection(&s_cs);
    LONG from = s_out, to = s_in, lost = 0;
    if (to - from > kRing) {
        lost = to - from - kRing;
        from = to - kRing;
    }
    for (LONG i = from; i < to; i++)
        buf[i - from] = s_ring[i % kRing];
    s_out = to;
    LeaveCriticalSection(&s_cs);
    if (lost)
        Log(L"кольцо переполнено: пропущено %d", (int)lost);
    for (LONG k = 0; k < to - from; k++) {
        const Msg& m = buf[k];
        s_total++;
        Count* c = CountOf(m.code, m.skel);
        if (!c)
            continue;
        c->n++;
        c->minute++;
        if (c->n <= kFirstShown)   // the first few of each code in full
            Log(L"вх. skel=%d 0x%04X len=%u %02X %02X %02X %02X %02X %02X %02X %02X", m.skel, m.code, m.len, m.p[0],
                m.p[1], m.p[2], m.p[3], m.p[4], m.p[5], m.p[6], m.p[7]);
    }
}

static void Minute(int no)
{
    Log(L"--- минута %d: всего %u, разных кодов %d", no, s_total, s_cntN);
    wchar_t line[360];
    int n = 0;
    for (int i = 0; i < s_cntN; i++) {
        if (!s_cnt[i].minute)
            continue;
        n += _snwprintf(line + n, 360 - n, L" %d:%04X x%u", s_cnt[i].skel, s_cnt[i].code, s_cnt[i].minute);
        s_cnt[i].minute = 0;
        if (n > 300) {
            Log(L"%s", line);
            n = 0;
        }
    }
    if (n)
        Log(L"%s", line);
}

typedef int (*Fn0)(void);
typedef int (*Fn2)(int, void*);

static FARPROC Proc(HMODULE m, const char* name)
{
    wchar_t w[80];
    MultiByteToWideChar(CP_ACP, 0, name, -1, w, 80);
    return GetProcAddress(m, w);
}

static bool Running(const wchar_t* cls)
{
    return FindWindow(cls, NULL) != NULL;
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
{
    if (!LogPlace()) {
        MessageBox(NULL, L"Проба MCU пишет журнал на SD-карту или USB-диск - вставьте их.", L"McuProbe",
                   MB_OK | MB_TOPMOST);
        return 1;
    }
    if (MessageBox(NULL, L"Проба MCU: 5 минут второй клиент штатной библиотеки MCU рядом с оболочкой. Всё это время "
                   L"пользуйтесь ГУ как обычно (кнопки руля, громкость, радио, камера, звонок) и запишите, если что-то "
                   L"не сработало.\r\n\r\nНачать?", L"McuProbe", MB_YESNO | MB_TOPMOST) != IDYES)
        return 0;
    InitializeCriticalSection(&s_cs);
    wchar_t path[MAX_PATH];
    _snwprintf(path, MAX_PATH, L"%sSPIPowerMgr_orig.dll", kApp);
    bool orig = GetFileAttributes(path) != 0xFFFFFFFF;
    if (!orig)
        _snwprintf(path, MAX_PATH, L"%sSPIPowerMgr.dll", kApp);
    Log(L"=== McuProbe 1.0: %s; оболочка %s, хост VestaStub %s, VestaHome %s", path,
        Running(L"RadioCommWindow") ? L"работает" : L"нет", OpenEvent(EVENT_ALL_ACCESS, FALSE, L"VestaMcuHost") ? L"есть" : L"нет",
        Running(L"VestaHome_Wnd") ? L"работает" : L"нет");
    HMODULE dep = LoadLibrary(L"\\ResidentFlash\\navigation\\app\\NCL_BaseCom.dll");   // (the DLL's dependency, as the
    HMODULE sp = LoadLibrary(path);                                                 // installer pins it)
    if (!sp) {
        Log(L"не загрузилась (%u)", GetLastError());
        MessageBox(NULL, L"Библиотека MCU не загрузилась - см. McuProbe.log.", L"McuProbe", MB_OK | MB_TOPMOST);
        return 1;
    }
    Fn0 load = (Fn0)Proc(sp, "SPApi_LoadRadioEngine");
    Fn0 inited = (Fn0)Proc(sp, "SPApi_IsInitialized");
    Fn0 init = (Fn0)Proc(sp, "SPApi_InitializeRadioEngine");
    Fn2 skel = (Fn2)Proc(sp, "SPApi_SetSkelonton");
    Fn0 product = (Fn0)Proc(sp, "SPApi_SendRadioRequestProductInfo");
    if (!load || !inited || !init || !skel) {
        Log(L"в библиотеке нет нужных функций");
        return 1;
    }
    load();   // as can_panel: load, init when not yet, the skeletons, the product info asked
    int was = inited();
    if (!was)
        init();
    static Skel skels[0x12];
    for (int i = 0; i < 0x12; i++) {
        skels[i].vtbl = s_vtbl;
        skels[i].id = i + 1;
        skel(i + 1, &skels[i]);
    }
    Log(L"клиент готов: инициализация %s, скелеты 1..18", was ? L"уже была" : L"сделана");
    if (product) {
        product();
        Log(L"запрос сведений о продукте отправлен (ждём 0x7902)");
    }
    DWORD t0 = GetTickCount();
    int minute = 0;
    while (GetTickCount() - t0 < kRunMs) {
        Sleep(1000);
        Flush();
        if ((int)((GetTickCount() - t0) / 60000) > minute)
            Minute(++minute);
    }
    Flush();
    Minute(minute + 1);
    Log(L"=== конец: всего %u сообщений, разных (скелет, код) %d", s_total, s_cntN);
    wchar_t end[400];
    _snwprintf(end, 400, L"Проба закончена: %u сообщений MCU, разных %d.\r\n\r\nПришлите %s и что не сработало за эти "
               L"5 минут.", s_total, s_cntN, s_log);
    MessageBox(NULL, end, L"McuProbe", MB_OK | MB_TOPMOST);
    (void)dep;
    ExitProcess(0);   // (no Uninit: the shell's link stays as it was; our PIC1: node goes with the process)
    return 0;
}
