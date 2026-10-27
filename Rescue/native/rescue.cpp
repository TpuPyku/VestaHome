// VestaRescue - boot-time safe mode for the head unit, started by the factory Main.exe from the SD card.
//
// \ResidentFlash\Main.exe, right after it has started the shell, checks for
// \SDMemory\bACKuPdESAYsV201200070012.Mainflg and, when the file is there, runs \SDMemory\Cancle60s.exe
// and then \windows\Explorer.exe (Vesta/SPIPower/MAIN.md, FUN_00016e28). This exe is meant to sit there
// as Cancle60s.exe. It touches no factory file. Fast steps go first, because the MCU may reset the unit
// within a minute or two when the shell is dead:
//   1. log to \SDMemory\VestaRescue\rescue.log, one open/write/close per line, so a reset keeps what was written;
//   2. process list, then (only in the boot-time run, the first 3 min of uptime) stop and disable our programs (VestaHome, VestaLaunch, Navit, GstProbe): the exe
//      is renamed to <name>.vrescue, so the shell's "Navigation" can't start it again; renaming it back undoes it;
//   3. boot flags of Main.exe, ExternNavi.cfg, md5 of the shell's DLL/exe, memory and free space;
//   4. the newest files of \ResidentFlash, \StorageFlash and \SDMMC (what was written just before the hang)
//      and copies of the newest small ones into \SDMemory\VestaRescue\files\.
#include <windows.h>
#include <string.h>
#include <stdio.h>
#include <tlhelp32.h>
#include "../../SPIPower/install/md5.h"

#ifdef UNDER_CE
#define PROC(m, n) GetProcAddress(m, L##n)
#else
#define PROC(m, n) GetProcAddress(m, n)
#endif

static wchar_t g_dir[MAX_PATH];    // \SDMemory\VestaRescue (or the exe's folder without an SD)
static wchar_t g_log[MAX_PATH];
static wchar_t g_sum[2048];        // summary for the message box

static void LogRaw(const wchar_t* s)
{
    HANDLE h = CreateFile(g_log, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, 0, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return;
    SetFilePointer(h, 0, NULL, FILE_END);
    char buf[1200];
    int n = WideCharToMultiByte(CP_UTF8, 0, s, -1, buf, sizeof(buf) - 3, NULL, NULL);
    if (n > 0) {
        n--;
        buf[n++] = '\r';
        buf[n++] = '\n';
        DWORD w;
        WriteFile(h, buf, n, &w, NULL);
    }
    CloseHandle(h);
}

static void Log(const wchar_t* fmt, ...)
{
    wchar_t s[1000];
    va_list a;
    va_start(a, fmt);
    _vsnwprintf(s, 999, fmt, a);
    va_end(a);
    s[999] = 0;
    LogRaw(s);
}

static void Sum(const wchar_t* s)
{
    if (wcslen(g_sum) + wcslen(s) + 2 < sizeof(g_sum) / sizeof(g_sum[0])) {
        wcscat(g_sum, s);
        wcscat(g_sum, L"\n");
    }
}

static bool Exists(const wchar_t* p)
{
    return GetFileAttributes(p) != 0xFFFFFFFF;
}

static void FileTimeText(const FILETIME& ft, wchar_t* s, int n)
{
    SYSTEMTIME st;
    FileTimeToSystemTime(&ft, &st);
    _snwprintf(s, n, L"%04u-%02u-%02u %02u:%02u:%02u", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
}

// ---------------------------------------------------------------- processes (toolhelp.dll on CE)
typedef HANDLE (WINAPI *SnapshotFn)(DWORD, DWORD);
typedef BOOL   (WINAPI *ProcessFn)(HANDLE, PROCESSENTRY32*);
typedef BOOL   (WINAPI *CloseFn)(HANDLE);

static const wchar_t* kOurs[] = { L"VestaHome.exe", L"VestaLaunch.exe", L"navit.exe", L"GstProbe.exe" };
static const int kOursN = 4;

static bool IsOurs(const wchar_t* exe)
{
    for (int i = 0; i < kOursN; i++)
        if (_wcsicmp(exe, kOurs[i]) == 0)
            return true;
    return false;
}

// kill = false: list everything; kill = true: terminate our programs.
static void Processes(bool kill)
{
    HMODULE lib = LoadLibrary(L"toolhelp.dll");
    if (!lib) {
        Log(L"процессы: нет toolhelp.dll");
        return;
    }
    SnapshotFn snapshot = (SnapshotFn)PROC(lib, "CreateToolhelp32Snapshot");
    ProcessFn  first    = (ProcessFn)PROC(lib, "Process32First");
    ProcessFn  next     = (ProcessFn)PROC(lib, "Process32Next");
    CloseFn    close    = (CloseFn)PROC(lib, "CloseToolhelp32Snapshot");
    if (snapshot && first && next && close) {
        HANDLE snap = snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32 pe;
            memset(&pe, 0, sizeof(pe));
            pe.dwSize = sizeof(pe);
            for (BOOL ok = first(snap, &pe); ok; ok = next(snap, &pe)) {
                if (!kill) {
                    Log(L"  %-24s pid=%08X потоков=%u", pe.szExeFile, (unsigned)pe.th32ProcessID, (unsigned)pe.cntThreads);
                    if (_wcsicmp(pe.szExeFile, L"G3NavHMI.exe") == 0)
                        Sum(L"оболочка G3NavHMI запущена");
                    continue;
                }
                if (!IsOurs(pe.szExeFile))
                    continue;
                HANDLE p = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                BOOL t = p ? TerminateProcess(p, 0) : FALSE;
                DWORD e = t ? 0 : GetLastError();
                if (p)
                    CloseHandle(p);
                Log(L"остановлен %s: %s err=%u", pe.szExeFile, t ? L"ok" : L"НЕТ", (unsigned)e);
                wchar_t m[96];
                _snwprintf(m, 96, L"остановлен %s", pe.szExeFile);
                Sum(m);
            }
            close(snap);
        }
    }
    FreeLibrary(lib);
}

// ---------------------------------------------------------------- disable our programs
static void Disable(const wchar_t* exe)
{
    if (!Exists(exe))
        return;
    wchar_t to[MAX_PATH + 16];
    _snwprintf(to, MAX_PATH + 16, L"%s.vrescue", exe);
    DeleteFile(to);
    BOOL ok = MoveFile(exe, to);
    Log(L"отключён %s -> .vrescue: %s err=%u", exe, ok ? L"ok" : L"НЕТ", ok ? 0u : (unsigned)GetLastError());
    wchar_t m[MAX_PATH + 32];
    _snwprintf(m, MAX_PATH + 32, L"%s %s", ok ? L"отключён" : L"НЕ отключён", exe);
    Sum(m);
}

static const wchar_t* kCfg = L"\\ResidentFlash\\navigation\\app\\Config\\ExternNavi.cfg";
static bool s_disable;   // the boot-time run: ExternNavi.cfg entries of ours are disabled too

// ExternNavi.cfg into the log, and every entry that is one of our programs disabled.
static void ExternNavi()
{
    HANDLE h = CreateFile(kCfg, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        Log(L"ExternNavi.cfg: не открыть err=%u", (unsigned)GetLastError());
        return;
    }
    char buf[4096];
    DWORD got = 0;
    ReadFile(h, buf, sizeof(buf) - 1, &got, NULL);
    CloseHandle(h);
    buf[got] = 0;
    Log(L"ExternNavi.cfg (%u Б):", (unsigned)got);
    char* p = buf;
    while (*p) {
        char* e = p + strcspn(p, "\r\n");
        char c = *e;
        *e = 0;
        if (*p) {
            wchar_t w[MAX_PATH];
            MultiByteToWideChar(CP_ACP, 0, p, -1, w, MAX_PATH);
            Log(L"  %s", w);
            const wchar_t* base = wcsrchr(w, L'\\');
            if (s_disable && IsOurs(base ? base + 1 : w))
                Disable(w);
        }
        if (!c)
            break;
        p = e + 1;
    }
}

// ---------------------------------------------------------------- files
static void Bytes(const wchar_t* path)
{
    HANDLE h = CreateFile(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        Log(L"%s: нет", path);
        return;
    }
    unsigned char b[16];
    DWORD got = 0;
    ReadFile(h, b, sizeof(b), &got, NULL);
    CloseHandle(h);
    wchar_t hex[64] = L"";
    for (DWORD i = 0; i < got; i++) {
        wchar_t t[4];
        _snwprintf(t, 4, L"%02X ", b[i]);
        wcscat(hex, t);
    }
    Log(L"%s: ЕСТЬ, %u Б: %s", path, (unsigned)got, hex);
    wchar_t m[MAX_PATH + 64];
    _snwprintf(m, MAX_PATH + 64, L"есть %s (%s)", path, hex);
    Sum(m);
}

static void Md5(const wchar_t* path)
{
    HANDLE h = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        Log(L"%s: не открыть err=%u", path, (unsigned)GetLastError());
        return;
    }
    md5_ctx c;
    md5_init(&c);
    static unsigned char buf[16384];
    DWORD got, total = 0;
    while (ReadFile(h, buf, sizeof(buf), &got, NULL) && got) {
        md5_update(&c, buf, got);
        total += got;
    }
    FILETIME ft;
    GetFileTime(h, NULL, NULL, &ft);
    CloseHandle(h);
    unsigned char d[16];
    char hex[33];
    md5_final(&c, d);
    md5_hex(d, hex);
    wchar_t t[32];
    FileTimeText(ft, t, 32);
    Log(L"%s: %u Б md5=%S изменён %s", path, (unsigned)total, hex, t);
}

struct Newest { FILETIME t; DWORD size; wchar_t path[MAX_PATH]; };
enum { kTop = 40 };
static Newest s_top[kTop];
static int    s_topN;
static int    s_scanned;

static void Keep(const FILETIME& t, DWORD size, const wchar_t* path)
{
    int i;
    if (s_topN < kTop)
        i = s_topN++;
    else if (CompareFileTime(&t, &s_top[kTop - 1].t) <= 0)
        return;
    else
        i = kTop - 1;
    while (i > 0 && CompareFileTime(&t, &s_top[i - 1].t) > 0) {
        s_top[i] = s_top[i - 1];
        i--;
    }
    s_top[i].t = t;
    s_top[i].size = size;
    wcsncpy(s_top[i].path, path, MAX_PATH - 1);
    s_top[i].path[MAX_PATH - 1] = 0;
}

static void Scan(const wchar_t* dir, int depth)
{
    if (depth > 12)
        return;
    wchar_t mask[MAX_PATH];
    _snwprintf(mask, MAX_PATH, L"%s\\*", dir);
    WIN32_FIND_DATA fd;
    HANDLE f = FindFirstFile(mask, &fd);
    if (f == INVALID_HANDLE_VALUE)
        return;
    do {
        if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L".."))
            continue;
        wchar_t p[MAX_PATH];
        _snwprintf(p, MAX_PATH, L"%s\\%s", dir, fd.cFileName);
        p[MAX_PATH - 1] = 0;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (_wcsicmp(p, g_dir) != 0)
                Scan(p, depth + 1);
        } else {
            s_scanned++;
            Keep(fd.ftLastWriteTime, fd.nFileSizeLow, p);
        }
    } while (FindNextFile(f, &fd));
    FindClose(f);
}

static bool CopyWorthy(const Newest& n)
{
    const wchar_t* dot = wcsrchr(n.path, L'.');
    if (dot && (!_wcsicmp(dot, L".exe") || !_wcsicmp(dot, L".dll") || !_wcsicmp(dot, L".bin") ||
                !_wcsicmp(dot, L".png") || !_wcsicmp(dot, L".img")))
        return false;
    return n.size > 0 && n.size <= 512 * 1024;
}

// Newest files of one volume into the log; with copy, the newest small ones to files\.
static void Volume(const wchar_t* root, int copy)
{
    s_topN = 0;
    s_scanned = 0;
    DWORD t0 = GetTickCount();
    Scan(root, 0);
    Log(L"--- новые файлы %s (просмотрено %d за %u мс):", root, s_scanned, (unsigned)(GetTickCount() - t0));
    int copied = 0;
    for (int i = 0; i < s_topN; i++) {
        wchar_t t[32];
        FileTimeText(s_top[i].t, t, 32);
        Log(L"  %s %9u %s", t, (unsigned)s_top[i].size, s_top[i].path);
        if (copied < copy && CopyWorthy(s_top[i])) {
            wchar_t to[MAX_PATH];
            _snwprintf(to, MAX_PATH, L"%s\\files\\", g_dir);
            int n = (int)wcslen(to);
            for (const wchar_t* s = s_top[i].path + 1; *s && n < MAX_PATH - 1; s++)
                to[n++] = *s == L'\\' ? L'_' : *s;
            to[n] = 0;
            if (CopyFile(s_top[i].path, to, FALSE))
                copied++;
        }
    }
}

// ---------------------------------------------------------------- main
#ifdef UNDER_CE
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
#else
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
#endif
{
    if (Exists(L"\\SDMemory"))
        wcscpy(g_dir, L"\\SDMemory\\VestaRescue");
    else {
        GetModuleFileName(NULL, g_dir, MAX_PATH);
        wchar_t* s = wcsrchr(g_dir, L'\\');
        if (s)
            wcscpy(s, L"\\VestaRescue");
    }
    CreateDirectory(g_dir, NULL);
    wchar_t files[MAX_PATH];
    _snwprintf(files, MAX_PATH, L"%s\\files", g_dir);
    CreateDirectory(files, NULL);
    _snwprintf(g_log, MAX_PATH, L"%s\\rescue.log", g_dir);

    SYSTEMTIME lt, ut;
    GetLocalTime(&lt);
    GetSystemTime(&ut);
    Log(L"=== VestaRescue: местное %02u.%02u.%04u %02u:%02u:%02u, UTC %02u:%02u, после загрузки %u с",
        lt.wDay, lt.wMonth, lt.wYear, lt.wHour, lt.wMinute, lt.wSecond, ut.wHour, ut.wMinute,
        (unsigned)(GetTickCount() / 1000));

    Log(L"процессы:");
    Processes(false);
    // Our programs are stopped and disabled only in the boot-time run (Main.exe starts this a few seconds
    // after power-on); started by hand later it only collects the report.
    bool boot = GetTickCount() < 180000;
    Log(boot ? L"запуск при загрузке: отключаю наши программы" : L"запуск вручную: только отчёт");
    if (boot) {
        Processes(true);
        Disable(L"\\ResidentFlash\\VestaHome\\VestaHome.exe");
        Disable(L"\\SDMMC\\VestaHome\\VestaHome.exe");
        Disable(L"\\SDMMC\\VestaLaunch\\VestaLaunch.exe");
        Disable(L"\\SDMMC\\navit\\navit.exe");
        Disable(L"\\SDMemory\\VestaHome\\VestaHome.exe");   // the test copies live on the SD card
        Disable(L"\\SDMemory\\navit\\navit.exe");
    }
    s_disable = boot;
    ExternNavi();

    Bytes(L"\\ResidentFlash\\SwUpdate.flag");
    Bytes(L"\\ResidentFlash\\CreateProcessFail.flag");
    Bytes(L"\\ResidentFlash\\CreateProcessResetflg.flag");
    Md5(L"\\ResidentFlash\\navigation\\app\\SPIPowerMgr.dll");
    Md5(L"\\ResidentFlash\\navigation\\app\\SPIPowerMgr_orig.dll");
    Md5(L"\\ResidentFlash\\navigation\\app\\G3NavHMI.exe");
    Md5(L"\\ResidentFlash\\Main.exe");

    MEMORYSTATUS ms;
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatus(&ms);
    Log(L"память: занято %u%%, свободно %u КБ из %u КБ", (unsigned)ms.dwMemoryLoad,
        (unsigned)(ms.dwAvailPhys / 1024), (unsigned)(ms.dwTotalPhys / 1024));
    static const wchar_t* kVols[] = { L"\\ResidentFlash", L"\\StorageFlash", L"\\SDMMC", L"\\SDMemory" };
    for (int i = 0; i < 4; i++) {
        ULARGE_INTEGER fr, tot, tfr;
        if (GetDiskFreeSpaceEx(kVols[i], &fr, &tot, &tfr))
            Log(L"том %s: свободно %u МБ из %u МБ", kVols[i], (unsigned)(fr.QuadPart >> 20), (unsigned)(tot.QuadPart >> 20));
        else
            Log(L"том %s: нет", kVols[i]);
    }

    Volume(L"\\ResidentFlash", 15);
    Volume(L"\\StorageFlash", 15);
    Volume(L"\\SDMMC", 10);
    Log(L"=== готово");

    if (!g_sum[0])
        Sum(L"наших программ не найдено, флагов загрузки нет");
    Sum(L"");
    Sum(L"Журнал: \\SDMemory\\VestaRescue\\rescue.log");
    Sum(L"Вернуть программу: переименовать <имя>.exe.vrescue обратно в .exe");
    MessageBox(NULL, g_sum, L"VestaRescue", MB_OK | MB_TOPMOST);
    return 0;
}
