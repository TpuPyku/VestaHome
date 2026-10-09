// Interceptors for high-value SPIPowerMgr exports (P2: outbound logging).
// Each logs its arguments (only when g_logging is on) and forwards to the
// original function via g_real[].
#include <windows.h>
#include "g_proxy.h"

static void logLine(const wchar_t* s)
{
    if (!g_logging)
        return;
    HANDLE f = CreateFileW(L"\\SDMMC\\SPIPower.log", GENERIC_WRITE, FILE_SHARE_READ,
                           NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return;
    SetFilePointer(f, 0, NULL, FILE_END);
    DWORD n = 0;
    WriteFile(f, s, wcslen(s) * sizeof(wchar_t), &n, NULL);
    static const wchar_t nl[] = L"\r\n";
    WriteFile(f, nl, sizeof(nl) - sizeof(wchar_t), &n, NULL);
    CloseHandle(f);
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
    unsigned char p[4] = { 0, 0, 0, 0 };
    for (int k = 0; k < 4 && k < (int)len; k++)
        p[k] = data[3 + k];
    wchar_t buf[160];
    wsprintfW(buf, L"SKEL id=%d msg=0x%04X len=%u p=%02X %02X %02X %02X",
              s_wraps[i].id, (unsigned)msg, (unsigned)len,
              (unsigned)p[0], (unsigned)p[1], (unsigned)p[2], (unsigned)p[3]);
    logLine(buf);
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
    if (g_logging && skeleton && s_nwraps < MAX_WRAPS)
    {
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
