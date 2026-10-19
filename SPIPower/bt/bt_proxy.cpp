// FLC_BLUETOOTH_SDK.dll proxy (Windows CE / ARM, VS2005 -- no C++11): the stock shell's Bluetooth SDK, passed
// through, so that VestaHome sees the shell's Bluetooth and can steer its music (btshm.h).
//
// Replaces \ResidentFlash\navigation\app\FLC_BLUETOOTH_SDK.dll; the original sits beside it as
// FLC_BLUETOOTH_SDK_orig.dll. Every export is an ARM thunk into g_real[] (thunks.s, gen_thunks_bt.py), only
// Flc_Bluetooth_Init is ours: it hands the SDK our callback, which publishes each event and calls the shell's.
// Safety first: if the original does not load, every export answers "failed" (1) instead of jumping to nothing -
// the shell then runs without Bluetooth rather than not at all.
#include <windows.h>
#include <string.h>
#include "g_btproxy.h"
#include "btshm.h"

extern "C" void* g_real[PROXY_COUNT] = {0};

typedef void (*FlcCb)(int* ev);
typedef int (*FlcFn0)(void);

static HMODULE s_real;
static BtShm*  s_shm;
static HANDLE  s_map, s_cmdEv, s_thread;
static FlcCb   s_appCb;
static int     s_logging;

static int Failed()   // stands in for every export when the original is missing
{
    return 1;
}

// \SDMemory\VestaLogs\BTProxy.log, only while \SDMMC\SPIPower.on exists (the MCU proxy's flag; the shell loads us
// at boot, maybe before the cards are mounted: looked at again every 10 s)
static int LogOn()
{
    static DWORD s_checked;
    if (!s_logging && GetTickCount() - s_checked > 10000) {
        s_checked = GetTickCount();
        s_logging = GetFileAttributesW(L"\\SDMMC\\SPIPower.on") != 0xFFFFFFFF;
    }
    return s_logging;
}

static void LogLine(const wchar_t* s)
{
    if (!LogOn())
        return;
    CreateDirectoryW(L"\\SDMemory\\VestaLogs", NULL);
    HANDLE f = CreateFileW(L"\\SDMemory\\VestaLogs\\BTProxy.log", GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return;
    SetFilePointer(f, 0, NULL, FILE_END);
    DWORD n = 0, t = GetTickCount();
    wchar_t ts[16];   // seconds since boot, as the other logs
    wsprintfW(ts, L"%u.%03u ", t / 1000, t % 1000);
    WriteFile(f, ts, wcslen(ts) * sizeof(wchar_t), &n, NULL);
    WriteFile(f, s, wcslen(s) * sizeof(wchar_t), &n, NULL);
    static const wchar_t nl[] = L"\r\n";
    WriteFile(f, nl, sizeof(nl) - sizeof(wchar_t), &n, NULL);
    CloseHandle(f);
}

static void CopyText(char* dst, const char* src, int cap)
{
    int i = 0;
    for (; i < cap - 1 && src[i]; i++)
        dst[i] = src[i];
    dst[i] = 0;
}

// One event into the ring and the state. The event is the SDK's buffer: read with a guard, only the fields the
// shell itself reads for that code.
static void Publish(int* ev)
{
    if (!s_shm)
        return;
    __try {
        int code = ev[0];
        LONG n = InterlockedIncrement((LONG*)&s_shm->evSeq);
        BtEvent* e = &s_shm->ev[(n - 1) & (BT_EV_SLOTS - 1)];
        e->seq = 0;
        e->tick = GetTickCount();
        e->code = code;
        memcpy(e->data, ev + 1, BT_EV_DATA);
        e->seq = n;

        BtState& st = s_shm->st;
        InterlockedIncrement((LONG*)&s_shm->seq);   // odd: being written
        switch (code) {
        case 0x01: st.ready = 1; break;
        case 0x0A:
            CopyText(st.devAddr, (char*)ev + 4, 18);
            CopyText(st.devName, (char*)ev + 0x16, BT_TEXT);
            st.devTick = e->tick;
            break;
        case 0x08: {   // a paired device: kept once per address
            const char* a = (const char*)ev + 4;
            int i = 0;
            while (i < st.paired && strncmp(st.pairedAddr[i], a, 17))
                i++;
            if (i == st.paired && st.paired < BT_PAIRED)
                st.paired++;
            if (i < st.paired) {
                CopyText(st.pairedAddr[i], a, 18);
                CopyText(st.pairedName[i], (const char*)ev + 0x16, 48);
            }
            break;
        }
        case 0x11: st.hf = ev[1]; break;
        case 0x0E: if (ev[1] == 1) CopyText(st.callNumber, (char*)ev + 9, 32); break;
        case 0x10:
            if (ev[2] == 1 || ev[2] == 2) {
                st.call[ev[2] - 1] = ev[1];
                st.callTick = e->tick;
            }
            break;
        case 0x14: CopyText(st.operatorName, (char*)ev + 4, 32); break;
        case 0x12: st.hfAudio = ev[1]; break;
        case 0x15: st.battery = (char)ev[1]; break;
        case 0x16: st.signal = (char)ev[1]; break;
        case 0x2B: st.av = ev[1]; st.avTick = e->tick; break;
        case 0x2C: st.lengthMs = ev[1]; st.posMs = ev[2]; st.playing = ev[3]; st.playTick = e->tick; break;
        case 0x30: st.avrcp = ev[1]; break;
        case 0x2E: {
            int items = ev[1] < 0 ? 0 : ev[1] > 7 ? 7 : ev[1];
            st.title[0] = st.artist[0] = st.album[0] = 0;
            st.trackNo = st.trackCount = st.durationMs = 0;
            for (int i = 0; i < items; i++) {
                int attr = ev[2 + i * 0x42];
                const char* t = (const char*)(ev + 4 + i * 0x42);
                if (attr == 1)
                    CopyText(st.title, t, BT_TEXT);
                else if (attr == 2)
                    CopyText(st.artist, t, BT_TEXT);
                else if (attr == 3)
                    CopyText(st.album, t, BT_TEXT);
                else if (attr == 4)
                    st.trackNo = atoi(t);
                else if (attr == 5)
                    st.trackCount = atoi(t);
                else if (attr == 7)
                    st.durationMs = atoi(t);
            }
            st.mediaTick = e->tick;
            break;
        }
        }
        InterlockedIncrement((LONG*)&s_shm->seq);   // even: done
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        if (s_shm->seq & 1)
            InterlockedIncrement((LONG*)&s_shm->seq);
    }
}

static void LogEvent(int* ev)
{
    if (!LogOn())
        return;
    wchar_t b[200];
    __try {
        int k = wsprintfW(b, L"EV 0x%02X:", (unsigned)ev[0]);
        const unsigned char* p = (const unsigned char*)(ev + 1);
        for (int i = 0; i < 24 && k < 180; i++)
            k += wsprintfW(b + k, L" %02X", (unsigned)p[i]);
        const char* t = ev[0] == 0x2E && ev[1] > 0 ? (const char*)(ev + 4)   // the first text of a track's info
                      : ev[0] == 0x0A ? (const char*)ev + 0x16 : NULL;            // the device's name
        if (t) {
            char a[41];
            int i = 0;
            for (; i < 40 && t[i]; i++)
                a[i] = t[i];
            a[i] = 0;
            wcscpy(b + k, L" | ");
            k += 3;
            if (!MultiByteToWideChar(CP_UTF8, 0, a, -1, b + k, 200 - k))
                b[k] = 0;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        wcscpy(b, L"EV ?");
    }
    LogLine(b);
}

static void ProxyCb(int* ev)
{
    Publish(ev);
    LogEvent(ev);
    if (s_appCb)
        s_appCb(ev);
}

// VestaHome's commands, carried out in the shell's process by the SDK itself
static DWORD WINAPI CmdThread(LPVOID)
{
    LONG done = s_shm ? s_shm->cmdSeq : 0;
    while (s_shm) {
        WaitForSingleObject(s_cmdEv, 1000);
        LONG n = s_shm->cmdSeq;
        if (n == done)
            continue;
        done = n;
        int cmd = s_shm->cmd, idx = -1;
        switch (cmd) {
        case BT_CMD_PLAYPAUSE:  idx = IDX_Flc_Bluetooth_Av_PlayPause; break;
        case BT_CMD_PLAY:       idx = IDX_Flc_Bluetooth_Av_Play; break;
        case BT_CMD_PAUSE:      idx = IDX_Flc_Bluetooth_Av_Pause; break;
        case BT_CMD_NEXT:       idx = IDX_Flc_Bluetooth_Av_Forward; break;
        case BT_CMD_PREV:       idx = IDX_Flc_Bluetooth_Av_Backward; break;
        case BT_CMD_MEDIAINFO:  idx = IDX_Flc_Bluetooth_Av_GetMediaInfo; break;
        case BT_CMD_PLAYSTATUS: idx = IDX_Flc_Bluetooth_Av_GetPlayStatus; break;
        case BT_CMD_AVSTATE:    idx = IDX_Flc_Bluetooth_Av_GetConnectState; break;
        case BT_CMD_FF_PRESS:   idx = IDX_Flc_Bluetooth_Av_FF_Press; break;
        case BT_CMD_FF_RELEASE: idx = IDX_Flc_Bluetooth_Av_FF_Release; break;
        case BT_CMD_REW_PRESS:  idx = IDX_Flc_Bluetooth_Av_Rewind_Press; break;
        case BT_CMD_REW_RELEASE: idx = IDX_Flc_Bluetooth_Av_Rewind_Release; break;
        }
        int r = -1;
        if (idx >= 0)
            r = ((FlcFn0)g_real[idx])();
        else if (cmd == BT_CMD_ANSWER)   // the first phone (line 0), as the shell's
            r = ((int (*)(int))g_real[IDX_Flc_Bluetooth_Hf_Answer])(0);
        else if (cmd == BT_CMD_END)
            r = ((int (*)(int))g_real[IDX_Flc_Bluetooth_Hf_End])(0);
        else if (cmd == BT_CMD_DIAL) {   // Hf_DialNumber(number, its length, line)
            char num[40];
            CopyText(num, s_shm->cmdText, 40);
            r = ((int (*)(const char*, unsigned short, int))g_real[IDX_Flc_Bluetooth_Hf_DialNumber])(
                num, (unsigned short)strlen(num), 0);
        }
        s_shm->cmdResult = r;
        s_shm->cmdDone = n;
        wchar_t b[64];
        wsprintfW(b, L"CMD %d -> %d", cmd, r);
        LogLine(b);
    }
    return 0;
}

extern "C" int proxy_Flc_Bluetooth_Init(FlcCb cb)
{
    s_appCb = cb;
    if (s_shm)
        InterlockedIncrement((LONG*)&s_shm->inits);
    int r = ((int (*)(FlcCb))g_real[IDX_Flc_Bluetooth_Init])(cb ? ProxyCb : NULL);
    wchar_t b[64];
    wsprintfW(b, L"Flc_Bluetooth_Init -> %d", r);
    LogLine(b);
    if (!s_thread && s_shm && s_cmdEv)   // not in DllMain (the loader lock): here, once the shell uses us
        s_thread = CreateThread(NULL, 0, CmdThread, NULL, 0, NULL);
    return r;
}

BOOL WINAPI DllMain(HANDLE hInstDLL, DWORD fdwReason, LPVOID)
{
    if (fdwReason == DLL_PROCESS_ATTACH) {
        wchar_t path[MAX_PATH];
        GetModuleFileNameW((HMODULE)hInstDLL, path, MAX_PATH);
        wchar_t* slash = wcsrchr(path, L'\\');
        if (slash)
            slash[1] = 0;
        else
            path[0] = 0;
        wcscat(path, L"FLC_BLUETOOTH_SDK_orig.dll");
        s_real = LoadLibraryW(path);
        int resolved = 0;
        for (int i = 0; i < PROXY_COUNT; i++) {
            g_real[i] = s_real ? (void*)GetProcAddressW(s_real, kProxyNames[i]) : NULL;
            if (g_real[i])
                resolved++;
            else
                g_real[i] = (void*)Failed;
        }
        s_map = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(BtShm), BT_SHM_NAME);
        if (s_map) {
            s_shm = (BtShm*)MapViewOfFile(s_map, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(BtShm));
            if (s_shm && s_shm->magic != BT_SHM_MAGIC) {
                memset(&s_shm->st, 0, sizeof(s_shm->st));
                s_shm->st.av = s_shm->st.avrcp = s_shm->st.hf = s_shm->st.hfAudio = -1;
                s_shm->st.battery = s_shm->st.signal = -1;
                s_shm->version = 3;
                s_shm->magic = BT_SHM_MAGIC;
            }
            if (s_shm) {
                InterlockedIncrement((LONG*)&s_shm->attach);
                s_shm->origOk = s_real != NULL;
            }
        }
        s_cmdEv = CreateEventW(NULL, FALSE, FALSE, BT_CMD_EVENT);
        wchar_t b[96];
        wsprintfW(b, L"BTPROXY %s orig=%d exports=%d/%d shm=%d", s_real ? L"OK" : L"NO ORIGINAL", s_real ? 1 : 0,
                  resolved, PROXY_COUNT, s_shm ? 1 : 0);
        LogLine(b);
    }
    // DLL_PROCESS_DETACH: nothing is undone - the SDK may still call our callback while the process goes down
    return TRUE;
}
