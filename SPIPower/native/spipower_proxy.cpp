// SPIPowerMgr proxy (Windows CE / ARM, VS2005 -- no C++11).
//
// Replaces \ResidentFlash\navigation\app\SPIPowerMgr.dll. Exports the same 139
// names (generated as ARM thunks in thunks.s) and forwards every call to the
// original library, renamed SPIPowerMgr_orig.dll, via the g_real[] table.
//
// P1: transparent pass-through only -- no interception, no logging. The
// original DLL must sit beside us as SPIPowerMgr_orig.dll.
#include <windows.h>
#include "g_proxy.h"
#include "mcushm.h"
#include "mcucmd.h"

// The shared ring of MCU messages (mcushm.h), mapped in every process that loads us.
McuShm* g_shm = 0;
static HANDLE s_shmMap = 0;

// ---- the call journal (2026-10-21, the shell's replacement, step 0): every export that is not intercepted calls
// ProxyNote first (thunks.s). Into SPIPower.log (with the flag): a call whose arguments differ from that
// function's last ones ("CALL name(r0, r1, r2, r3) pid"; the same again only counted), and once a minute how many
// times each function was called - a periodic one may be what the MCU waits for from the shell.
static CRITICAL_SECTION s_noteCs;
static bool  s_noteReady;
static DWORD s_noteLast[PROXY_COUNT][4], s_noteCount[PROXY_COUNT], s_noteSame[PROXY_COUNT], s_noteMinute;
static bool  s_noteSeen[PROXY_COUNT];
extern "C" int ProxyLogOn();   // intercept.cpp: the flag \SDMMC\SPIPower.on (looked at again every 10 s)
void ProxyLog(const wchar_t* s);

extern "C" void ProxyNote(int idx, const DWORD* a)
{
    if (!s_noteReady || idx < 0 || idx >= PROXY_COUNT || !ProxyLogOn())
        return;
    EnterCriticalSection(&s_noteCs);
    wchar_t b[200];
    s_noteCount[idx]++;
    if (s_noteSeen[idx] && !memcmp(s_noteLast[idx], a, sizeof(s_noteLast[idx]))) {
        s_noteSame[idx]++;
    } else {
        if (s_noteSame[idx]) {
            wsprintfW(b, L"CALL %s: the same %u times more", kProxyNames[idx], s_noteSame[idx]);
            ProxyLog(b);
        }
        wsprintfW(b, L"CALL %s(%08X, %08X, %08X, %08X) pid=%08X", kProxyNames[idx], a[0], a[1], a[2], a[3],
                  GetCurrentProcessId());
        ProxyLog(b);
        memcpy(s_noteLast[idx], a, sizeof(s_noteLast[idx]));
        s_noteSeen[idx] = true;
        s_noteSame[idx] = 0;
    }
    DWORD now = GetTickCount();
    if (!s_noteMinute)
        s_noteMinute = now;
    if (now - s_noteMinute >= 60000) {   // the minute's counts
        s_noteMinute = now;
        int k = wsprintfW(b, L"CALLS 60s pid=%08X:", GetCurrentProcessId());
        for (int i = 0; i < PROXY_COUNT; i++) {
            if (!s_noteCount[i])
                continue;
            if (k > 140) {
                ProxyLog(b);
                k = wsprintfW(b, L"CALLS 60s pid=%08X:", GetCurrentProcessId());
            }
            k += wsprintfW(b + k, L" %s=%u", kProxyNames[i], s_noteCount[i]);
            s_noteCount[i] = 0;
        }
        ProxyLog(b);
    }
    LeaveCriticalSection(&s_noteCs);
}

// VestaHome's calls made in the shell (mcucmd.h): only in G3NavHMI.exe.
static McuCmd* s_cmd = 0;
static HANDLE  s_cmdEv = 0;
void ProxyLog(const wchar_t* s);   // intercept.cpp: a line of SPIPower.log (with the flag)

typedef void (*CallFn6)(unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int);

static DWORD WINAPI CmdThread(LPVOID)
{
    s_cmd->done = s_cmd->seq;   // what an earlier shell left is not made again
    s_cmd->ready = (LONG)GetCurrentProcessId();
    for (;;) {
        WaitForSingleObject(s_cmdEv, 1000);
        while (s_cmd->done != s_cmd->seq) {
            LONG n = s_cmd->done + 1;
            if (s_cmd->seq - n >= MCU_CMD_SLOTS)   // fallen behind by a whole queue: from the oldest still there
                n = s_cmd->seq - MCU_CMD_SLOTS + 1;
            McuCall c = s_cmd->call[n % MCU_CMD_SLOTS];
            c.name[sizeof(c.name) - 1] = 0;
            wchar_t w[48];
            MultiByteToWideChar(CP_ACP, 0, c.name, -1, w, 48);
            int idx = -1;
            for (int i = 0; i < PROXY_COUNT && idx < 0; i++)
                if (!wcscmp(kProxyNames[i], w))
                    idx = i;
            if (idx >= 0 && g_real[idx])
                ((CallFn6)g_real[idx])(c.arg[0], c.arg[1], c.arg[2], c.arg[3], c.arg[4], c.arg[5]);
            wchar_t b[128];
            wsprintfW(b, L"VH CALL %s(%u, %u, %u)%s", w, c.arg[0], c.arg[1], c.arg[2], idx < 0 ? L" - no such" : L"");
            ProxyLog(b);
            s_cmd->done = n;
        }
    }
    return 0;
}

static void CmdStart()
{
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(NULL, exe, MAX_PATH);
    if (!wcsstr(exe, L"G3NavHMI"))
        return;
    HANDLE m = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(McuCmd), MCU_CMD_NAME);
    if (!m)
        return;
    s_cmd = (McuCmd*)MapViewOfFile(m, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(McuCmd));
    s_cmdEv = CreateEventW(NULL, FALSE, FALSE, MCU_CMD_EVENT);
    if (!s_cmd || !s_cmdEv)
        return;
    s_cmd->magic = MCU_CMD_MAGIC;
    CloseHandle(CreateThread(NULL, 0, CmdThread, NULL, 0, NULL));   // runs once DllMain has returned
}

// Pointer table filled from the original DLL at load time (thunks tail-jump
// into it). C linkage so the symbol name matches armasm's IMPORT. The explicit
// initializer makes this a definition (a bare "extern \"C\" type name[N];" is
// only a declaration and the linker then reports g_real unresolved).
extern "C" void* g_real[PROXY_COUNT] = {0};

// Interceptor logging is enabled only when the flag file \SDMMC\SPIPower.on
// exists. Off by default -> pure pass-through.
int g_logging = 0;

static HMODULE s_real = 0;

// Append one line to \SDMemory\VestaLogs\SPIPower.log (logs only on the SD card:
// no card, no log). Always writes (even without the flag) so a fatal attach
// failure is visible; normal "PROXY OK" heartbeat is emitted only when
// g_logging is on (the on-device watchdog reads it then).
static void logLine(const wchar_t* s)
{
    CreateDirectoryW(L"\\SDMemory\\VestaLogs", NULL);
    HANDLE f = CreateFileW(L"\\SDMemory\\VestaLogs\\SPIPower.log", GENERIC_WRITE, FILE_SHARE_READ,
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

BOOL WINAPI DllMain(HANDLE hInstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        g_logging = (GetFileAttributesW(L"\\SDMMC\\SPIPower.on") != 0xFFFFFFFF);
        InitializeCriticalSection(&s_noteCs);   // before any export can be called
        s_noteReady = true;

        // Load the original DLL from our own folder (by full path) so the loader's
        // search path can never pick a wrong/absent copy and brick the shell.
        wchar_t origPath[MAX_PATH];
        GetModuleFileNameW((HMODULE)hInstDLL, origPath, MAX_PATH);
        wchar_t* slash = wcsrchr(origPath, L'\\');
        if (slash) slash[1] = 0; else origPath[0] = 0;
        wcscat(origPath, L"SPIPowerMgr_orig.dll");

        s_real = LoadLibraryW(origPath);
        if (!s_real)
        {
            logLine(L"PROXY FAIL: SPIPowerMgr_orig.dll not found");
            return FALSE;   // G3NavHMI's LoadLibrary then fails -> crash (caught by watchdog)
        }

        int resolved = 0;
        for (int i = 0; i < PROXY_COUNT; i++)
        {
            g_real[i] = (void*)GetProcAddressW(s_real, kProxyNames[i]);
            if (g_real[i])
                resolved++;
        }

        // the shared ring: created by the first process, opened by the others (same name); a new mapping
        // comes zeroed, so the header is set up once
        s_shmMap = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(McuShm), MCU_SHM_NAME);
        if (s_shmMap)
        {
            g_shm = (McuShm*)MapViewOfFile(s_shmMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(McuShm));
            if (g_shm)
            {
                if (g_shm->magic != MCU_SHM_MAGIC)
                {
                    g_shm->version = 2;
                    g_shm->magic = MCU_SHM_MAGIC;
                }
                InterlockedIncrement((LONG*)&g_shm->attach);
            }
        }
        CmdStart();

        if (g_logging)
        {
            wchar_t buf[96];
            wsprintfW(buf, L"PROXY OK orig=1 exports=%d/%d logging=%d shm=%d",
                      resolved, PROXY_COUNT, g_logging, g_shm ? 1 : 0);
            logLine(buf);
        }
    }
    else if (fdwReason == DLL_PROCESS_DETACH)
    {
        if (g_shm)
            UnmapViewOfFile(g_shm);
        if (s_shmMap)
            CloseHandle(s_shmMap);
        if (s_real)
            FreeLibrary(s_real);
    }
    return TRUE;
}
