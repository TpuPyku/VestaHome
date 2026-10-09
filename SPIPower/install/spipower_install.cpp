// SPIPowerInstall - safe swap/watchdog/rollback for the SPIPowerMgr proxy on the
// Lada Vesta MMC (Windows CE 6.0, ARM). Follows the reversible-install pattern of
// VestaInstall (toolhelp KillProcess, .bak-style backups, append-only log).
//
// Usage: SPIPowerInstall.exe status | install [p2|p3] | rollback
//   status        -> report current state to \SDMMC\SPIPower_install.log + a message box
//   install       -> backup the factory DLL, swap in the proxy, start the shell and
//                    watch it (pure pass-through, no flag)
//   install p2    -> same, but create \SDMMC\SPIPower.on (outbound logging)
//   install p3    -> same, flag on (inbound vtable wrap is also gated by the flag)
//   rollback      -> kill the shell, restore the factory DLL, delete the flag, restart
//
// The proxy DLL must sit next to this exe as \SDMMC\SPIPower_install\SPIPowerMgr.dll.
// The factory DLL is preserved in two places: SPIPowerMgr_orig.dll (app dir, survives
// reboot) and \SDMMC\SPIPower_backup\SPIPowerMgr.dll (SD, for the rescue path).
#include <windows.h>
#include <string.h>
#include <stdio.h>
#include <tlhelp32.h>

static const wchar_t* kAppDir    = L"\\ResidentFlash\\navigation\\app";
static const wchar_t* kDll       = L"SPIPowerMgr.dll";
static const wchar_t* kOrig      = L"SPIPowerMgr_orig.dll";
static const wchar_t* kProxySrc  = L"\\SDMMC\\SPIPower_install\\SPIPowerMgr.dll";
static const wchar_t* kBackup    = L"\\SDMMC\\SPIPower_backup\\SPIPowerMgr.dll";
static const wchar_t* kBackupDir = L"\\SDMMC\\SPIPower_backup";
static const wchar_t* kFlag      = L"\\SDMMC\\SPIPower.on";
static const wchar_t* kLog       = L"\\SDMMC\\SPIPower.log";
static const wchar_t* kShell     = L"G3NavHMI.exe";
static const wchar_t* kShellPath = L"\\ResidentFlash\\navigation\\app\\G3NavHMI.exe";

static wchar_t g_dll[MAX_PATH];
static wchar_t g_orig[MAX_PATH];
static wchar_t g_status[512];

static void Join(wchar_t* out, const wchar_t* a, const wchar_t* b)
{
    int la = (int)wcslen(a);
    _snwprintf(out, MAX_PATH, L"%s%s%s", a, (la && a[la - 1] == L'\\') ? L"" : L"\\", b);
    out[MAX_PATH - 1] = 0;
}

static void SetPaths()
{
    Join(g_dll,  kAppDir, kDll);
    Join(g_orig, kAppDir, kOrig);
}

// ---------------------------------------------------------------- logging
static void LogMsg(const wchar_t* s)
{
    HANDLE h = CreateFile(L"\\SDMMC\\SPIPower_install.log", GENERIC_WRITE, FILE_SHARE_READ,
                          NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    SetFilePointer(h, 0, NULL, FILE_END);
    DWORD w = 0;
    WriteFile(h, s, wcslen(s) * sizeof(wchar_t), &w, NULL);
    static const wchar_t nl[] = L"\r\n";
    WriteFile(h, nl, sizeof(nl) - sizeof(wchar_t), &w, NULL);
    CloseHandle(h);
}

// ---------------------------------------------------------------- files
static int FileExists(const wchar_t* p)
{
    return GetFileAttributes(p) != 0xFFFFFFFF;
}

static int MyCopyFile(const wchar_t* src, const wchar_t* dst)
{
    HANDLE hi = CreateFile(src, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hi == INVALID_HANDLE_VALUE) return 0;
    HANDLE ho = CreateFile(dst, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (ho == INVALID_HANDLE_VALUE) { CloseHandle(hi); return 0; }
    char buf[8192]; DWORD got; int ok = 1;
    while (ReadFile(hi, buf, sizeof(buf), &got, NULL) && got) {
        DWORD w;
        if (!WriteFile(ho, buf, got, &w, NULL) || w != got) { ok = 0; break; }
    }
    FlushFileBuffers(ho);
    CloseHandle(ho); CloseHandle(hi);
    return ok;
}

static void EnsureDir(const wchar_t* p) { CreateDirectory(p, NULL); }

static int CanWrite(const wchar_t* p)
{
    HANDLE h = CreateFile(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    CloseHandle(h);
    return 1;
}

// ---------------------------------------------------------------- processes
typedef HANDLE (WINAPI *SnapshotFn)(DWORD, DWORD);
typedef BOOL   (WINAPI *ProcessFn)(HANDLE, PROCESSENTRY32*);
typedef BOOL   (WINAPI *CloseFn)(HANDLE);

static int ProcExists(const wchar_t* exeName)
{
    HMODULE lib = LoadLibrary(L"toolhelp.dll");
    if (!lib) return -1;   // unknown
    SnapshotFn snapshot = (SnapshotFn)GetProcAddress(lib, L"CreateToolhelp32Snapshot");
    ProcessFn  first    = (ProcessFn)GetProcAddress(lib, L"Process32First");
    ProcessFn  next     = (ProcessFn)GetProcAddress(lib, L"Process32Next");
    CloseFn    close    = (CloseFn)GetProcAddress(lib, L"CloseToolhelp32Snapshot");
    int found = 0;
    if (snapshot && first && next && close) {
        HANDLE snap = snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32 pe; memset(&pe, 0, sizeof(pe)); pe.dwSize = sizeof(pe);
            for (BOOL ok = first(snap, &pe); ok; ok = next(snap, &pe)) {
                if (_wcsicmp(pe.szExeFile, exeName) == 0) { found = 1; break; }
            }
            close(snap);
        }
    }
    FreeLibrary(lib);
    return found;
}

static int KillProcess(const wchar_t* exeName)
{
    HMODULE lib = LoadLibrary(L"toolhelp.dll");
    if (!lib) return 0;
    SnapshotFn snapshot = (SnapshotFn)GetProcAddress(lib, L"CreateToolhelp32Snapshot");
    ProcessFn  first    = (ProcessFn)GetProcAddress(lib, L"Process32First");
    ProcessFn  next     = (ProcessFn)GetProcAddress(lib, L"Process32Next");
    CloseFn    close    = (CloseFn)GetProcAddress(lib, L"CloseToolhelp32Snapshot");
    int killed = 0;
    if (snapshot && first && next && close) {
        HANDLE snap = snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32 pe; memset(&pe, 0, sizeof(pe)); pe.dwSize = sizeof(pe);
            for (BOOL ok = first(snap, &pe); ok; ok = next(snap, &pe)) {
                if (_wcsicmp(pe.szExeFile, exeName) != 0) continue;
                HANDLE proc = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                if (proc) { killed = TerminateProcess(proc, 0) != 0; CloseHandle(proc); }
            }
            close(snap);
        }
    }
    FreeLibrary(lib);
    return killed;
}

static void StartShell()
{
    PROCESS_INFORMATION pi;
    memset(&pi, 0, sizeof(pi));
    wchar_t cmd[MAX_PATH];
    wcscpy(cmd, kShellPath);
    CreateProcess(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi);
    if (pi.hThread) CloseHandle(pi.hThread);
    if (pi.hProcess) CloseHandle(pi.hProcess);
}

// ---------------------------------------------------------------- watchdog
// Reads \SDMMC\SPIPower.log and reports whether the "PROXY OK" heartbeat is present.
static int LogHasOk()
{
    HANDLE h = CreateFile(kLog, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    DWORD size = GetFileSize(h, NULL);
    if (size == 0xFFFFFFFF || size == 0) { CloseHandle(h); return 0; }
    if (size > (1u << 20)) size = (1u << 20);
    wchar_t* buf = (wchar_t*)malloc(size + 2);
    int found = 0;
    if (buf) {
        DWORD got = 0;
        if (ReadFile(h, buf, size, &got, NULL) && got >= (DWORD)(2 * sizeof(wchar_t))) {
            buf[got / sizeof(wchar_t)] = 0;
            found = wcsstr(buf, L"PROXY OK") != NULL;
        }
        free(buf);
    }
    CloseHandle(h);
    return found;
}

// Returns 0 on success, 1 on failure.
static int Watchdog(int timeoutMs, int requireHeartbeat)
{
    int waited = 0;
    for (;;) {
        if (ProcExists(kShell) == 0)
            return 1;   // shell died
        if (requireHeartbeat && LogHasOk())
            return 0;   // heartbeat seen while shell alive
        if (waited >= timeoutMs)
            break;
        Sleep(500);
        waited += 500;
    }
    if (requireHeartbeat)
        return LogHasOk() ? 0 : 1;
    return 0;
}

// ---------------------------------------------------------------- actions
static void SetFlag(int on)
{
    if (on) {
        HANDLE h = CreateFile(kFlag, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    } else {
        DeleteFile(kFlag);
    }
}

static int Backup()
{
    EnsureDir(kBackupDir);
    if (!FileExists(g_orig)) {
        if (!MyCopyFile(g_dll, g_orig)) {
            wcscpy(g_status, L"Ошибка: не удалось сохранить SPIPowerMgr_orig.dll");
            return 0;
        }
    }
    if (!FileExists(kBackup)) {
        if (!MyCopyFile(g_orig, kBackup)) {
            wcscpy(g_status, L"Ошибка: не удалось сохранить копию на SD");
            return 0;
        }
    }
    return 1;
}

static void Rollback()
{
    LogMsg(L"--- rollback ---");
    KillProcess(kShell);
    Sleep(2000);
    if (FileExists(g_orig)) {
        if (MyCopyFile(g_orig, g_dll))
            LogMsg(L"rollback: restored factory SPIPowerMgr.dll");
        else
            LogMsg(L"rollback FAILED: could not write SPIPowerMgr.dll");
    } else {
        LogMsg(L"rollback FAILED: SPIPowerMgr_orig.dll not found");
    }
    DeleteFile(kFlag);
    StartShell();
}

static void DoInstall(int logging)
{
    SetPaths();
    LogMsg(logging ? L"--- install (logging on) ---" : L"--- install (pass-through) ---");

    if (!FileExists(g_dll)) { wcscpy(g_status, L"Ошибка: SPIPowerMgr.dll не найден в app"); return; }
    if (!FileExists(kProxySrc)) { wcscpy(g_status, L"Ошибка: proxy не найден (\\SDMMC\\SPIPower_install\\SPIPowerMgr.dll)"); return; }

    KillProcess(kShell);
    Sleep(2500);
    if (!CanWrite(g_dll)) { wcscpy(g_status, L"Ошибка: SPIPowerMgr.dll занят оболочкой"); return; }

    if (!Backup()) return;

    if (!MyCopyFile(kProxySrc, g_dll)) { wcscpy(g_status, L"Ошибка: не удалось скопировать proxy на место SPIPowerMgr.dll"); return; }

    SetFlag(logging ? 1 : 0);

    StartShell();
    int fail = Watchdog(30000, logging ? 1 : 0);
    if (fail) {
        LogMsg(L"install: watchdog failed -> rollback");
        Rollback();
        wcscpy(g_status, L"ПРОВАЛ: оболочка упала или proxy не ответил; выполнен откат.");
        return;
    }

    wcscpy(g_status, logging
        ? L"OK: proxy установлен, лог включён, оболочка жива."
        : L"OK: proxy установлен (pass-through), оболочка жива.");
}

static void DoStatus()
{
    SetPaths();
    wchar_t line[512];
    _snwprintf(line, 512, L"dll:%s orig:%s sd-bak:%s flag:%s shell:%s",
               FileExists(g_dll) ? L"y" : L"n",
               FileExists(g_orig) ? L"y" : L"n",
               FileExists(kBackup) ? L"y" : L"n",
               FileExists(kFlag) ? L"y" : L"n",
               ProcExists(kShell) ? L"up" : L"down");
    wcscpy(g_status, line);
}

// ---------------------------------------------------------------- entry
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPWSTR lpCmd, int nShow)
{
    wchar_t a[64];
    int n = 0;
    if (lpCmd) {
        for (; lpCmd[n] && n < 63; n++)
            a[n] = (lpCmd[n] >= L'A' && lpCmd[n] <= L'Z') ? lpCmd[n] + 32 : lpCmd[n];
    }
    a[n] = 0;

    int logging = (wcsstr(a, L"p2") || wcsstr(a, L"p3"));

    if (wcsstr(a, L"rollback")) {
        Rollback();
        wcscpy(g_status, L"Откат выполнен: оригинал восстановлен, оболочка перезапущена.");
    } else if (wcsstr(a, L"status")) {
        DoStatus();
    } else if (wcsstr(a, L"install")) {
        DoInstall(logging);
    } else {
        wcscpy(g_status, L"Способ: SPIPowerInstall.exe status | install [p2|p3] | rollback");
    }

    LogMsg(g_status);
    MessageBox(NULL, g_status, L"SPIPower Install", MB_OK | MB_ICONINFORMATION);
    return 0;
}
