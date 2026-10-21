// Interceptors for high-value SPIPowerMgr exports (P2: outbound logging).
// Each logs its arguments (only when g_logging is on) and forwards to the
// original function via g_real[].
#include <windows.h>
#include "g_proxy.h"
#include "mcushm.h"

extern McuShm* g_shm;   // spipower_proxy.cpp: the shared ring, NULL if the mapping failed

// The flag \SDMMC\SPIPower.on is read in DllMain, but the shell loads us at boot, possibly before the cards are
// mounted (2026-10-17: no line of the shell's at all): it is looked at again every 10 s, both ways - with
// \SDMMC\VestaLogs.off, VestaHome's switch of all our logs ("Diagnostics -> Logs", 2026-10-21).
static int LogOn()
{
    static DWORD s_checked;
    if (GetTickCount() - s_checked > 10000) {
        s_checked = GetTickCount();
        g_logging = GetFileAttributesW(L"\\SDMMC\\SPIPower.on") != 0xFFFFFFFF &&
                    GetFileAttributesW(L"\\SDMMC\\VestaLogs.off") == 0xFFFFFFFF;
    }
    return g_logging;
}

static void logLine(const wchar_t* s)
{
    if (!LogOn())
        return;
    CreateDirectoryW(L"\\SDMemory\\VestaLogs", NULL);   // logs only on the SD card: no card, no log
    HANDLE f = CreateFileW(L"\\SDMemory\\VestaLogs\\SPIPower.log", GENERIC_WRITE, FILE_SHARE_READ,
                           NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return;
    SetFilePointer(f, 0, NULL, FILE_END);
    DWORD n = 0, t = GetTickCount();
    wchar_t ts[16];   // seconds since boot, as VestaHome's mcu.log and its marks: the records line up
    wsprintfW(ts, L"%u.%03u ", t / 1000, t % 1000);
    WriteFile(f, ts, wcslen(ts) * sizeof(wchar_t), &n, NULL);
    WriteFile(f, s, wcslen(s) * sizeof(wchar_t), &n, NULL);
    static const wchar_t nl[] = L"\r\n";
    WriteFile(f, nl, sizeof(nl) - sizeof(wchar_t), &n, NULL);
    CloseHandle(f);
}

void ProxyLog(const wchar_t* s)   // for spipower_proxy.cpp (the shell's calls for VestaHome, the call journal)
{
    logLine(s);
}

extern "C" int ProxyLogOn()
{
    return LogOn();
}

// ---- inbound: rmtObjectSkeleton vtable wrap (P3) ---------------------------
// interpretCommand calls skel->vtable[1](&buf[1]) with the message buffer
// [id_hi][id_lo][len][payload...]. We swap each skeleton's vtable for a wrapper
// whose slot 1 logs the message and forwards to the original handler.
typedef void (*SkelHandler)(void* self, unsigned char* data);

#define MAX_WRAPS 20
static struct { void* self; SkelHandler orig; int id; } s_wraps[MAX_WRAPS];
static int s_nwraps = 0;
static void* s_wrapper_vtbl[MAX_WRAPS][2];

static void hookSkelHandler(void* self, unsigned char* data)
{
    int i;
    for (i = 0; i < s_nwraps; i++)
        if (s_wraps[i].self == self)
            break;
    if (i == s_nwraps)
        return;
    unsigned short msg = ((unsigned short)data[0] << 8) | data[1];
    unsigned char len = data[2];
    if (g_shm) {   // into the shared ring for VestaHome (always, the flag only gates the file log)
        LONG n = InterlockedIncrement((LONG*)&g_shm->seq);
        McuMsg* m = &g_shm->msg[(n - 1) & (MCU_SHM_SLOTS - 1)];
        m->seq = 0;
        m->tick = GetTickCount();
        m->id = msg;
        m->len = len;
        m->skel = (unsigned char)s_wraps[i].id;
        for (int k = 0; k < MCU_SHM_DATA; k++)
            m->data[k] = k < (int)len ? data[3 + k] : 0;
        m->seq = n;
    }
    if (LogOn()) {
        unsigned char p[4] = { 0, 0, 0, 0 };
        for (int k = 0; k < 4 && k < (int)len; k++)
            p[k] = data[3 + k];
        wchar_t buf[160];
        wsprintfW(buf, L"SKEL id=%d msg=0x%04X len=%u p=%02X %02X %02X %02X",
                  s_wraps[i].id, (unsigned)msg, (unsigned)len,
                  (unsigned)p[0], (unsigned)p[1], (unsigned)p[2], (unsigned)p[3]);
        logLine(buf);
    }
    s_wraps[i].orig(self, data);
}

extern "C" void proxy_EngSendTFTBrightnessData(unsigned char v)
{
    wchar_t b[64];
    wsprintfW(b, L"EngSendTFTBrightnessData %u", (unsigned)v);
    logLine(b);
    ((void (*)(unsigned char))g_real[IDX_EngSendTFTBrightnessData])(v);
}

extern "C" void proxy_EngSendCarSpeed(unsigned short v)
{
    wchar_t b[64];
    wsprintfW(b, L"EngSendCarSpeed %u", (unsigned)v);
    logLine(b);
    ((void (*)(unsigned short))g_real[IDX_EngSendCarSpeed])(v);
}

extern "C" void proxy_SPApi_SendKeyEvent(unsigned char a, unsigned char b, unsigned char c)
{
    wchar_t buf[64];
    wsprintfW(buf, L"SPApi_SendKeyEvent %u %u %u", (unsigned)a, (unsigned)b, (unsigned)c);
    logLine(buf);
    ((void (*)(unsigned char, unsigned char, unsigned char))g_real[IDX_SPApi_SendKeyEvent])(a, b, c);
}

extern "C" void proxy_SPApi_SendExtendKeyEvent(unsigned char a, unsigned char b, unsigned char c)
{
    wchar_t buf[64];
    wsprintfW(buf, L"SPApi_SendExtendKeyEvent %u %u %u", (unsigned)a, (unsigned)b, (unsigned)c);
    logLine(buf);
    ((void (*)(unsigned char, unsigned char, unsigned char))g_real[IDX_SPApi_SendExtendKeyEvent])(a, b, c);
}

extern "C" void proxy_SPApi_SendRotaryEvent(unsigned char a, unsigned char b, unsigned char c)
{
    wchar_t buf[64];
    wsprintfW(buf, L"SPApi_SendRotaryEvent %u %u %u", (unsigned)a, (unsigned)b, (unsigned)c);
    logLine(buf);
    ((void (*)(unsigned char, unsigned char, unsigned char))g_real[IDX_SPApi_SendRotaryEvent])(a, b, c);
}

extern "C" void proxy_SPApi_SetSkelonton(int id, void* skeleton)
{
    // wrapped always (2026-10-15): the shared ring feeds VestaHome's MCU monitor and live data; the
    // wrap itself was proven on the unit with the flag on (install p2, no crashes)
    if (skeleton && s_nwraps < MAX_WRAPS)
    {
        if (g_shm)
            InterlockedIncrement((LONG*)&g_shm->wraps);
        void** vtbl = *(void***)skeleton;            // original vtable
        s_wraps[s_nwraps].self = skeleton;
        s_wraps[s_nwraps].orig = (SkelHandler)vtbl[1];
        s_wraps[s_nwraps].id = id;
        s_wrapper_vtbl[s_nwraps][0] = vtbl[0];       // keep original dtor
        s_wrapper_vtbl[s_nwraps][1] = (void*)&hookSkelHandler;
        *(void***)skeleton = s_wrapper_vtbl[s_nwraps];
        s_nwraps++;

        wchar_t buf[64];
        wsprintfW(buf, L"SPApi_SetSkelonton id=%d skel=0x%08X", id, (unsigned)skeleton);
        logLine(buf);
    }
    ((void (*)(int, void*))g_real[IDX_SPApi_SetSkelonton])(id, skeleton);
}

extern "C" void proxy_SPApi_LoadByMain(void)
{
    logLine(L"SPApi_LoadByMain");
    ((void (*)(void))g_real[IDX_SPApi_LoadByMain])();
}

// ---- sound and source (2026-10-17): one line per call, with the caller's process id (the shell or VestaHome)
typedef void (*Fn0)(void);
typedef void (*Fn1)(unsigned char);
typedef void (*Fn2)(unsigned char, unsigned char);

static void logCall(const wchar_t* name, int n, unsigned a, unsigned b)
{
    wchar_t buf[96];
    if (n == 2)
        wsprintfW(buf, L"%s %u %u pid=%08X", name, a, b, GetCurrentProcessId());
    else if (n == 1)
        wsprintfW(buf, L"%s %u pid=%08X", name, a, GetCurrentProcessId());
    else
        wsprintfW(buf, L"%s pid=%08X", name, GetCurrentProcessId());
    logLine(buf);
}

#define PROXY2(name) extern "C" void proxy_##name(unsigned char a, unsigned char b)     { logCall(L ## #name, 2, a, b); ((Fn2)g_real[IDX_##name])(a, b); }
#define PROXY1(name) extern "C" void proxy_##name(unsigned char a)     { logCall(L ## #name, 1, a, 0); ((Fn1)g_real[IDX_##name])(a); }
#define PROXY0(name) extern "C" void proxy_##name(void)     { logCall(L ## #name, 0, 0, 0); ((Fn0)g_real[IDX_##name])(); }

PROXY2(SPApi_SendRadioAudioHMLData)
PROXY2(SPApi_SendRadioAudioBalanceFaderData)
PROXY2(SPApi_SendRadioAudioVolData)
PROXY2(SPApi_SendMediaSourceSelect)
PROXY1(SPApi_SendRadioNaviVoiceGuidance)
PROXY1(SPApi_SendRadioIsNaviSourceCouldIn)
PROXY1(SPApi_SendRadioNaviMediaMuteStatus)
PROXY0(SPApi_SendRadioChangeSource)

// The radio's lists: (1, preset, ?) tunes a preset, (0, row, 0) a station of the list (Ghidra); a preset saved by a
// long press is not known yet. Three arguments as VestaHome passes them; r0..r3 are passed on as they came.
extern "C" void proxy_SPApi_SendListItemControl(unsigned a, unsigned b, unsigned c, unsigned d)
{
    wchar_t buf[96];
    wsprintfW(buf, L"SPApi_SendListItemControl %u %u %u pid=%08X", a, b, c, GetCurrentProcessId());
    logLine(buf);
    ((void (*)(unsigned, unsigned, unsigned, unsigned))g_real[IDX_SPApi_SendListItemControl])(a, b, c, d);
}
