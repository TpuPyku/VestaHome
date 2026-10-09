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

// Pointer table filled from the original DLL at load time (thunks tail-jump
// into it). C linkage so the symbol name matches armasm's IMPORT. The explicit
// initializer makes this a definition (a bare "extern \"C\" type name[N];" is
// only a declaration and the linker then reports g_real unresolved).
extern "C" void* g_real[PROXY_COUNT] = {0};

// Interceptor logging is enabled only when the flag file \SDMMC\SPIPower.on
// exists. Off by default -> pure pass-through.
int g_logging = 0;

static HMODULE s_real = 0;

// Append one line to \SDMMC\SPIPower.log. Always writes (even without the flag)
// so a fatal attach failure is visible; normal "PROXY OK" heartbeat is emitted
// only when g_logging is on (the on-device watchdog reads it then).
static void logLine(const wchar_t* s)
{
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

BOOL WINAPI DllMain(HANDLE hInstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        g_logging = (GetFileAttributesW(L"\\SDMMC\\SPIPower.on") != 0xFFFFFFFF);

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

        if (g_logging)
        {
            wchar_t buf[96];
            wsprintfW(buf, L"PROXY OK orig=1 exports=%d/%d logging=%d",
                      resolved, PROXY_COUNT, g_logging);
            logLine(buf);
        }
    }
    else if (fdwReason == DLL_PROCESS_DETACH)
    {
        if (s_real)
            FreeLibrary(s_real);
    }
    return TRUE;
}
