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
// The log only on removable media: \SDMemory\VestaLogs\VestaStub.log (\SDMMC\VestaLogs.off - none). At the boot the
// card is often not mounted yet (2026-10-24: no VestaStub.log ever): the lines go to RAM (\Temp\VestaStub.log, not the
// flash) and are copied to the card as soon as it is there.
// The PC build (testing) maps the HU's paths "\X\..." to "<exe folder>\hu\X\..." and takes the 3 minutes from
// the environment variable VESTASTUB_OKMS.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

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
    HuPath(path, L"\\SDMMC\\VestaLogs.off");   // VestaHome's switch of all our logs ("Diagnostics -> Logs")
    if (There(path))
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
    for (int i = 0; i < to - from; i++) {
        const InMsg* m = &buf[i];
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
static void SkelHandle(void* self, unsigned char* p)
{
    int code = p[0] << 8 | p[1];
    RingRecord(((Skel*)self)->id, code, p[2], p + 3);
    switch (code) {
    case 0x3901: {   // the MCU's clock: hour, minute, second, year - 2000, month, day -> the system's (CClock)
        SYSTEMTIME t;
        memset(&t, 0, sizeof(t));
        t.wHour = p[4];
        t.wMinute = p[5];
        t.wSecond = p[6];
        t.wYear = (WORD)(2000 + p[7]);
        t.wMonth = p[8];
        t.wDay = p[9];
        if (t.wHour < 24 && t.wMinute < 60 && t.wSecond < 60 && t.wMonth >= 1 && t.wMonth <= 12 && t.wDay >= 1 &&
            t.wDay <= 31 && p[7] >= 20) {
            SetLocalTime(&t);
            Log(L"mcu: часы %02d:%02d:%02d %02d.%02d.%04d", t.wHour, t.wMinute, t.wSecond, t.wDay, t.wMonth, t.wYear);
        }
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
        int has = DirThere(dev ? L"\\USBDevice" : L"\\SDMemory") ? 1 : 0;
        if (s_spMedia)
            s_spMedia(dev, has, 0);
        Log(L"mcu: медиа %s -> %d", dev ? L"USB" : L"SD", has);
        break;
    }
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

static int McuHost()
{
    wchar_t p[MAX_PATH];
    _snwprintf(p, MAX_PATH - 1, L"%sSPIPowerMgr.dll", s_dir);   // the proxy (or the factory DLL), by full path
    p[MAX_PATH - 1] = 0;
    HMODULE sp = LoadLibrary(p);
    if (!sp) {
        Log(L"mcu: нет %s (%u)", p, GetLastError());
        return 1;
    }
    InitializeCriticalSection(&s_ringCs);   // before the skeletons: SkelHandle records into the ring
    SpFn0 load = (SpFn0)Proc(sp, "SPApi_LoadRadioEngine");
    SpFn0 inited = (SpFn0)Proc(sp, "SPApi_IsInitialized");
    SpFn0 init = (SpFn0)Proc(sp, "SPApi_InitializeRadioEngine");
    SpFn2 skel = (SpFn2)Proc(sp, "SPApi_SetSkelonton");
    s_spDeMute = (SpFn1)Proc(sp, "SPApi_SendRadioDeMuteCurrentSource");
    s_spRvc = (SpFn1)Proc(sp, "SPApi_SendRadioReceivedRVCStatus");
    s_spMedia = (SpFn3)Proc(sp, "SPApi_SendMediaAvailability");
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
    for (int i = 0; i < 4 && askTimes; i++)
        askTimes(i);
    Log(L"mcu: скелеты зарегистрированы (%d), запуск как у оболочки %s, маршрут звука %s",
        (int)(sizeof(kIds) / sizeof(kIds[0])), navStarted && sendDialog ? L"да" : L"НЕТ", s_svRoute ? L"есть" : L"НЕТ");
    s_cntTick = GetTickCount();
    for (;;) {
        Sleep(1000);
        RingFlush();
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

static int RunOriginal(const wchar_t* args, const wchar_t* why)
{
    HostStop();   // the shell registers its own skeletons
    wchar_t orig[MAX_PATH];
    _snwprintf(orig, MAX_PATH - 1, L"%sG3NavHMI_orig.exe", s_dir);
    orig[MAX_PATH - 1] = 0;
    HANDLE h = Start(orig, args);
    Log(L"оригинальная оболочка (%s): %s", why, h ? L"запущена" : L"НЕ ЗАПУЩЕНА");
    if (h)
        CloseHandle(h);
    return 0;
}

static bool FindVesta(wchar_t* exe)
{
    static const wchar_t* kCand[] = { L"\\SDMMC\\VestaHome\\VestaHome.exe", L"\\SDMemory\\VestaHome\\VestaHome.exe",
                                      L"\\ResidentFlash\\VestaHome\\VestaHome.exe" };
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

    wchar_t p[MAX_PATH];
    {   // the boot: the volumes come up after us - wait a little for the SD card (its flag) and for VestaHome
        wchar_t sd[MAX_PATH], vh0[MAX_PATH];
        HuPath(sd, L"\\SDMemory");
        DWORD t0 = GetTickCount();
        while (!There(sd) && GetTickCount() - t0 < 6000)
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
    wchar_t vh[MAX_PATH], self[MAX_PATH];
    if (!FindVesta(vh))
        return RunOriginal(args, L"нет VestaHome.exe");
    GetModuleFileName(NULL, self, MAX_PATH);
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
        if (code == 0) {   // closed itself (or a second copy that found the first)
            if (!lived)
                WriteCount(0);
            return RunOriginal(args, L"VestaHome закрылся сам");
        }
        Log(L"VestaHome упал (код %08X): снова", code);
        Sleep(1000);
    }
}
