// SPIPowerTest - Level 0 pre-swap check of the proxy DLL.
//
// Loads the proxy and verifies that DllMain attached (i.e. the proxy found
// SPIPowerMgr_orig.dll beside itself) and that all PROXY_COUNT exports resolve.
// Runs WITHOUT G3NavHMI, so it also works on the CE6 emulator, where G3NavHMI
// cannot start (dlldiag.txt: SPIPowerMgr/desay_sdk_api/NCL_BaseCom all load OK).
//
// The proxy is searched in several directories so the same exe works on the HU
// (\SDMMC\SPIPower_test\) and in the emulator (\StaticStore\SPIPower_test\ or the
// exe's own folder; the emulator is launched with /sharedfolder StaticStore, so
// \SDMMC does not exist there at all). A candidate counts only when
// SPIPowerMgr_orig.dll sits beside the proxy - without it the proxy's DllMain
// returns FALSE on purpose (HU brick guard) and the failure would be misread.
// The log goes to the exe's own folder (\SDMMC\SPIPower_test on the HU or the shared
// StaticStore folder in the emulator), else \SDMMC\SPIPower_test.log, else
// \StaticStore\SPIPower_test.log. Exit code 0 = OK, 1 = FAIL.
#include <windows.h>
#include "../native/g_proxy.h"

static const wchar_t* kDllName  = L"SPIPowerMgr.dll";
static const wchar_t* kOrigName = L"SPIPowerMgr_orig.dll";

static wchar_t g_buf[8192];
static int     g_len = 0;
static wchar_t g_log[MAX_PATH];

static void w(const wchar_t* s)
{
    int n = (int)wcslen(s);
    if (g_len + n >= (int)(sizeof(g_buf) / sizeof(wchar_t)) - 4) return;
    wcscpy(g_buf + g_len, s);
    g_len += n;
}

static void line(const wchar_t* s) { w(s); w(L"\r\n"); }

static void flush()
{
    HANDLE f = CreateFileW(g_log, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return;
    DWORD done = 0;
    WriteFile(f, g_buf, (DWORD)(g_len * sizeof(wchar_t)), &done, NULL);
    CloseHandle(f);
}

// Flush the log, tell the user, and leave with the given exit code.
static void finish(const wchar_t* msg, int code)
{
    flush();
    MessageBoxW(NULL, msg, L"SPIPowerTest",
                MB_OK | (code ? MB_ICONERROR : MB_ICONINFORMATION));
    ExitProcess((UINT)code);
}

static int exists(const wchar_t* p) { return GetFileAttributesW(p) != 0xFFFFFFFF; }

// "<root>\a\b\c.exe" -> "<root>\a\b\" (a bare name -> "")
static void dirOf(const wchar_t* full, wchar_t* out)
{
    wcscpy(out, full);
    wchar_t* p = wcsrchr(out, L'\\');
    if (p) p[1] = 0; else out[0] = 0;
}

static void join(wchar_t* out, const wchar_t* dir, const wchar_t* name)
{
    wsprintfW(out, L"%s%s", dir, name);
}

static int writable(const wchar_t* p)
{
    HANDLE h = CreateFileW(p, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    CloseHandle(h);
    return 1;
}

// Log location: the exe's own folder first - on the HU that is \SDMMC\SPIPower_test
// and in the emulator it is the shared StaticStore folder, so the log can be read
// from the host immediately (an emulator \SDMMC is RAM-only and vanishes with the
// session). Then \SDMMC, then \StaticStore.
static void pickLog(const wchar_t* ownDir)
{
    static const wchar_t* roots[] = { L"\\SDMMC\\", L"\\StaticStore\\" };
    join(g_log, ownDir, L"SPIPower_test.log");
    if (writable(g_log)) return;
    for (int i = 0; i < 2; i++)
    {
        join(g_log, roots[i], L"SPIPower_test.log");
        if (writable(g_log)) return;
    }
}

// Append "<what>: err=<n>" as one line.
static void dumpErr(const wchar_t* what, DWORD err)
{
    wchar_t b[24];
    wsprintfW(b, L"%u", (unsigned)err);
    w(what); w(L": err="); w(b); line(L"");
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPWSTR lpCmd, int nShow)
{
    wchar_t exe[MAX_PATH], ownDir[MAX_PATH], b[512];
    if (!GetModuleFileNameW(NULL, exe, MAX_PATH)) exe[0] = 0;
    dirOf(exe, ownDir);
    pickLog(ownDir);

    line(L"SPIPowerTest v2 (2026-10-09): proxy load check");
    w(L"log:     "); line(g_log);
    w(L"exe dir: "); line(ownDir);
    wsprintfW(b, L"exports in proxy: %d", (int)PROXY_COUNT);
    line(b);

    // Candidate directories, most specific first; the app folder is last, so the
    // harness also works after the proxy has been swapped in on the HU.
    // A candidate is accepted only when proxy AND factory DLL are both there: the
    // emulator has no \SDMMC (shared folder = StaticStore) and a half-populated
    // folder would make the proxy fail with "LoadLibrary returned NULL" although
    // the proxy itself is fine.
    wchar_t cands[4][MAX_PATH];
    wcscpy(cands[0], L"\\SDMMC\\SPIPower_test\\");
    wcscpy(cands[1], L"\\StaticStore\\SPIPower_test\\");
    wcscpy(cands[2], ownDir);
    wcscpy(cands[3], L"\\ResidentFlash\\navigation\\app\\");

    wchar_t proxy[MAX_PATH] = { 0 }, orig[MAX_PATH] = { 0 }, used[MAX_PATH] = { 0 };
    int found = 0, i;
    for (i = 0; i < 4 && !found; i++)
    {
        wchar_t p[MAX_PATH], o[MAX_PATH];
        int hp, ho;
        if (!cands[i][0]) continue;
        join(p, cands[i], kDllName);
        join(o, cands[i], kOrigName);
        hp = exists(p);
        ho = exists(o);
        w(L"  check "); w(cands[i]);
        w(L"proxy="); w(hp ? L"yes" : L"no");
        w(L" orig="); w(ho ? L"yes" : L"no"); line(L"");
        if (!hp) { line(L"    -> no proxy here"); continue; }
        if (!ho) { line(L"    -> incomplete: no _orig beside it, skipped"); continue; }
        wcscpy(proxy, p);
        wcscpy(orig, o);
        wcscpy(used, cands[i]);
        found = 1;
    }

    if (!found)
    {
        line(L"FAIL: proxy not found in any known directory");
        finish(L"SPIPowerTest FAIL: proxy SPIPowerMgr.dll not found (see the log)", 1);
    }

    w(L"proxy:   "); line(proxy);
    w(L"orig:    ");
    line(exists(orig) ? L"present" : L"MISSING (DllMain will fail)");

    // Pin the factory DLL's dependencies by full path (copied here by
    // tools/make_test_dir.py). The CE loader may not search this folder on its
    // own; keeping them resident makes the proxy's LoadLibrary(orig) succeed
    // regardless. Harmless if the loader would have resolved them anyway.
    {
        static const wchar_t* deps[2] = { L"NCL_BaseCom.dll", L"desay_sdk_api.dll" };
        for (i = 0; i < 2; i++)
        {
            wchar_t dp[MAX_PATH];
            join(dp, used, deps[i]);
            w(L"  preload "); w(deps[i]);
            if (!exists(dp)) { line(L": absent from this folder"); continue; }
            if (LoadLibraryW(dp)) line(L": ok");
            else { w(L" "); dumpErr(L"failed", GetLastError()); }
        }
    }

    HMODULE h = LoadLibraryW(proxy);
    if (!h)
    {
        DWORD e = GetLastError();   // 126 = file/dependency not found, 1114 = DllMain FALSE
        dumpErr(L"LoadLibrary(proxy)", e);
        HMODULE ho = LoadLibraryW(orig);       // is the factory DLL loadable here?
        if (ho) { line(L"  note: the factory DLL itself loads fine here"); FreeLibrary(ho); }
        else    { dumpErr(L"LoadLibrary(factory)", GetLastError());
                  line(L"  -> a dependency of the factory DLL is missing here"); }
        wchar_t m[512];
        wsprintfW(m, L"SPIPowerTest FAIL: proxy did not load (err=%u)\r\nlog: %s",
                  (unsigned)e, g_log);
        line(L"RESULT: FAIL");
        finish(m, 1);
    }

    int missing = 0, shown = 0;
    for (i = 0; i < PROXY_COUNT; i++)
    {
        if (!GetProcAddressW(h, kProxyNames[i]))
        {
            missing++;
            if (shown < 10) { w(L"  MISSING: "); line(kProxyNames[i]); shown++; }
        }
    }

    wsprintfW(b, L"exports resolved %d/%d, missing %d",
              PROXY_COUNT - missing, PROXY_COUNT, missing);
    line(b);
    line(missing == 0 ? L"RESULT: OK" : L"RESULT: FAIL");
    FreeLibrary(h);

    wchar_t m[512];
    if (missing == 0)
        wsprintfW(m, L"SPIPowerTest OK: %d/%d exports resolved\r\nlog: %s",
                  PROXY_COUNT, PROXY_COUNT, g_log);
    else
        wsprintfW(m, L"SPIPowerTest FAIL: %d exports missing\r\nsee: %s", missing, g_log);
    finish(m, missing == 0 ? 0 : 1);
    return 0;
}
