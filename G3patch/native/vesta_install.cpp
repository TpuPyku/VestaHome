// VestaInstall - native installer/uninstaller for the Vesta skin of the stock shell G3NavHMI.exe
// on the Lada Vesta MMC (Windows CE 6.0, ARM, 800x480). Also builds as a desktop app for testing.
//
// It rewrites the skin resource paths inside G3NavHMI.exe   res/img/Itelma/MMC27/... ->
// res/img/Itelma/VESTA/...   (length-preserving, 5 chars -> 5 chars, so no offsets shift) and
// clones the skin folder   res\img\Itelma\MMC27  ->  res\img\Itelma\VESTA.  Fully reversible:
// uninstall rewrites the paths back and deletes the VESTA folder.
//
// The registry does not survive a head-unit reboot, so nothing is stored there: state is read back
// from the files themselves (path counts inside the exe + presence of the VESTA folder) and the
// installer keeps its own backup   G3NavHMI.exe.vesta.bak   next to the exe.
//
// Before patching it checks whether G3NavHMI.exe was already modified by AppLauncher (the
// 127.0.0.1 -> 127.0.0.2 loopback redirect) and asks the user whether to continue.
#include <windows.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <tlhelp32.h>

#define APP_CLASS  L"VestaInstall_Wnd"

static const char* kOld    = "Itelma/MMC27";   // factory skin path fragment
static const char* kNew    = "Itelma/VESTA";   // our skin path fragment (same length)
static const char* kAppl   = "127.0.0.2";      // AppLauncher loopback patch marker
static const wchar_t* kExeName = L"G3NavHMI.exe";
static const wchar_t* kCfgName   = L"Config\\ExternNavi.cfg";
static const wchar_t* kLaunchDir = L"\\SDMMC\\VestaLaunch";
static const wchar_t* kLaunchExe = L"\\SDMMC\\VestaLaunch\\VestaLaunch.exe";
static const char*   kLoopOld    = "127.0.0.1";
static const char*   kLoopNew    = "127.0.0.2";
#define IDR_PAYLOAD 101

static wchar_t g_appDir[MAX_PATH];     // ...\navigation\app
static wchar_t g_exe[MAX_PATH];        // ...\app\G3NavHMI.exe
static wchar_t g_itelma[MAX_PATH];     // ...\app\res\img\Itelma
static wchar_t g_srcSkin[MAX_PATH];    // ...\Itelma\MMC27
static wchar_t g_dstSkin[MAX_PATH];    // ...\Itelma\VESTA
static int   g_mmc, g_vesta, g_appl;   // detection counts
static int   g_hasVesta;               // VESTA folder exists
static int   g_hasLaunch;              // \SDMMC\VestaLaunch\VestaLaunch.exe exists
static int   g_exeExists;
static wchar_t g_cfg[MAX_PATH];        // ...\app\Config\ExternNavi.cfg
static wchar_t g_exeBak[MAX_PATH];     // ...\app\G3NavHMI.exe.vesta.bak
static wchar_t g_status[300];          // last action result
static HWND  g_hwnd;                    // our window (for MessageBox prompts)

// ---------------------------------------------------------------- paths
static void Join(wchar_t* out, const wchar_t* a, const wchar_t* b)
{
    int la = (int)wcslen(a);
    _snwprintf(out, MAX_PATH, L"%s%s%s", a, (la && a[la - 1] == L'\\') ? L"" : L"\\", b);
    out[MAX_PATH - 1] = 0;
}

static void ExeDir(wchar_t* out)
{
    GetModuleFileName(NULL, out, MAX_PATH);
    wchar_t* p = wcsrchr(out, L'\\');
    if (p) *p = 0;
}

static int DirExists(const wchar_t* p)
{
    DWORD a = GetFileAttributes(p);
    return (a != 0xFFFFFFFF) && (a & FILE_ATTRIBUTE_DIRECTORY);
}

// Append a line to VestaInstall.log next to the exe (handy for tests / on-device diagnostics).
static void LogMsg(const wchar_t* s)
{
    wchar_t dir[MAX_PATH], path[MAX_PATH];
    ExeDir(dir); Join(path, dir, L"VestaInstall.log");
    HANDLE h = CreateFile(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    SetFilePointer(h, 0, NULL, FILE_END);
    char buf[1200];
    int n = WideCharToMultiByte(CP_ACP, 0, s, -1, buf, sizeof(buf) - 2, NULL, NULL);
    if (n > 0) {
        DWORD w;
        WriteFile(h, buf, (DWORD)(n - 1), &w, NULL);
        WriteFile(h, "\r\n", 2, &w, NULL);
    }
    CloseHandle(h);
}

// ---------------------------------------------------------------- file helpers
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

static void CloneTree(const wchar_t* src, const wchar_t* dst)
{
    EnsureDir(dst);
    wchar_t pat[MAX_PATH]; Join(pat, src, L"*");
    WIN32_FIND_DATA fd; HANDLE f = FindFirstFile(pat, &fd);
    if (f == INVALID_HANDLE_VALUE) return;
    do {
        if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
        wchar_t s[MAX_PATH], d[MAX_PATH];
        Join(s, src, fd.cFileName); Join(d, dst, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) CloneTree(s, d);
        else MyCopyFile(s, d);
    } while (FindNextFile(f, &fd));
    FindClose(f);
}

static void RemoveTree(const wchar_t* path)
{
    wchar_t pat[MAX_PATH]; Join(pat, path, L"*");
    WIN32_FIND_DATA fd; HANDLE f = FindFirstFile(pat, &fd);
    if (f != INVALID_HANDLE_VALUE) {
        do {
            if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
            wchar_t c[MAX_PATH]; Join(c, path, fd.cFileName);
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) RemoveTree(c);
            else { SetFileAttributes(c, FILE_ATTRIBUTE_NORMAL); DeleteFile(c); }
        } while (FindNextFile(f, &fd));
        FindClose(f);
    }
    SetFileAttributes(path, FILE_ATTRIBUTE_NORMAL);
    RemoveDirectory(path);
}

// Count occurrences of kOld / kNew / kAppl in one streaming pass (no whole-file buffer).
static void ScanFile(const wchar_t* path)
{
    g_mmc = g_vesta = g_appl = 0;
    HANDLE h = CreateFile(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    const int BUF = 1 << 16, KEEP = 32;
    char* b = (char*)malloc(BUF + KEEP + 4);
    if (!b) { CloseHandle(h); return; }
    const char* pats[3] = { kOld, kNew, kAppl };
    int* cnt[3] = { &g_mmc, &g_vesta, &g_appl };
    int carry = 0;
    for (;;) {
        DWORD got = 0; ReadFile(h, b + carry, BUF, &got, NULL);
        int total = carry + got;
        int lim = got ? total - KEEP : total;
        if (lim < 0) lim = 0;
        for (int p = 0; p < 3; p++) {
            int n = (int)strlen(pats[p]);
            for (int i = 0; i + n <= total && i < lim; i++)
                if (memcmp(b + i, pats[p], n) == 0) (*cnt[p])++;
        }
        int rem = total - lim; if (rem < 0) rem = 0;
        memmove(b, b + lim, rem); carry = rem;
        if (!got) break;
    }
    free(b);
    CloseHandle(h);
}

// Replace every oldp with newp in place (equal length). Returns count, or -1 if the file is locked.
static int ReplaceInFile(const wchar_t* path, const char* oldp, const char* newp)
{
    int oldn = (int)strlen(oldp);
    HANDLE h = CreateFile(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return -1;
    DWORD size = GetFileSize(h, NULL);
    const int BUF = 1 << 16;
    char* b = (char*)malloc(BUF + oldn + 4);
    if (!b) { CloseHandle(h); return -1; }
    DWORD pos = 0; int count = 0;
    while (pos < size) {
        DWORD want = BUF; if (pos + want > size) want = size - pos;
        if (SetFilePointer(h, pos, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER) break;
        DWORD got = 0; ReadFile(h, b, want, &got, NULL);
        int total = (int)got;
        int lim = (pos + got >= size) ? total : total - (oldn - 1);
        if (lim < 0) lim = 0;
        int changed = 0;
        for (int i = 0; i < lim; ) {
            if (i + oldn <= total && memcmp(b + i, oldp, oldn) == 0) { memcpy(b + i, newp, oldn); count++; i += oldn; changed = 1; }
            else i++;
        }
        if (changed) {
            if (SetFilePointer(h, pos, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER) break;
            DWORD w; WriteFile(h, b, total, &w, NULL);
        }
        if (lim <= 0) break;
        pos += lim;
    }
    FlushFileBuffers(h);
    free(b);
    CloseHandle(h);
    return count;
}

// ---------------------------------------------------------------- processes
// toolhelp.dll is loaded dynamically (like YaMaps): without it the program still starts.
typedef HANDLE (WINAPI *SnapshotFn)(DWORD, DWORD);
typedef BOOL   (WINAPI *ProcessFn)(HANDLE, PROCESSENTRY32*);
typedef BOOL   (WINAPI *CloseFn)(HANDLE);

static int KillProcess(const wchar_t* exeName)
{
#ifdef UNDER_CE
    HMODULE lib = LoadLibrary(L"toolhelp.dll");
#else
    HMODULE lib = LoadLibrary(L"kernel32.dll");
#endif
    if (!lib) return 0;
#ifdef UNDER_CE
    SnapshotFn snapshot = (SnapshotFn)GetProcAddress(lib, L"CreateToolhelp32Snapshot");
    ProcessFn  first    = (ProcessFn)GetProcAddress(lib, L"Process32First");
    ProcessFn  next     = (ProcessFn)GetProcAddress(lib, L"Process32Next");
    CloseFn    close    = (CloseFn)GetProcAddress(lib, L"CloseToolhelp32Snapshot");
#else
    SnapshotFn snapshot = (SnapshotFn)GetProcAddress(lib, "CreateToolhelp32Snapshot");
    ProcessFn  first    = (ProcessFn)GetProcAddress(lib, "Process32FirstW");
    ProcessFn  next     = (ProcessFn)GetProcAddress(lib, "Process32NextW");
    CloseFn    close    = (CloseFn)GetProcAddress(lib, "CloseToolhelp32Snapshot");
#endif
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

// ---------------------------------------------------------------- detection / actions
static int CanWrite(const wchar_t* p)
{
    HANDLE h = CreateFile(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    CloseHandle(h);
    return 1;
}

static void SetPaths()
{
    Join(g_exe,     g_appDir, kExeName);
    Join(g_itelma,  g_appDir, L"res\\img\\Itelma");
    Join(g_srcSkin, g_itelma, L"MMC27");
    Join(g_dstSkin, g_itelma, L"VESTA");
    Join(g_cfg,     g_appDir, kCfgName);
    wcscpy(g_exeBak, g_exe);
    wcscat(g_exeBak, L".vesta.bak");
}

static void Detect()
{
    SetPaths();
    g_exeExists = (GetFileAttributes(g_exe) != 0xFFFFFFFF);
    g_hasVesta  = DirExists(g_dstSkin);
    g_hasLaunch = (GetFileAttributes(kLaunchExe) != 0xFFFFFFFF);
    if (g_exeExists) ScanFile(g_exe);
    else g_mmc = g_vesta = g_appl = 0;
}

// The shell keeps the exe open for writing; closing it makes the HU reboot in ~2 minutes.
static int EnsureUnlocked()
{
    if (CanWrite(g_exe)) return 1;
    int r = MessageBox(g_hwnd,
        L"Оболочка G3NavHMI сейчас запущена, файл занят.\n"
        L"Чтобы пропатчить, её надо закрыть. После закрытия оболочки\n"
        L"ГУ перезагрузится примерно через 2 минуты.\n\n"
        L"Закрыть оболочку и продолжить?", L"Vesta Skin", MB_YESNO | MB_ICONWARNING);
    if (r != IDYES) return 0;
    KillProcess(kExeName);
    Sleep(2500);
    return CanWrite(g_exe);
}

// Write bytes from memory to a file, creating parent directories first.
static void EnsureDirTree(const wchar_t* dir)
{
    wchar_t tmp[MAX_PATH];
    wcscpy(tmp, dir);
    for (wchar_t* p = tmp + 1; *p; p++) {   // skip the leading backslash
        if (*p == L'\\') {
            *p = 0;
            CreateDirectory(tmp, NULL);
            *p = L'\\';
        }
    }
    CreateDirectory(tmp, NULL);
}

static int WriteFileBytes(const wchar_t* path, const void* data, DWORD len)
{
    wchar_t dir[MAX_PATH];
    wcscpy(dir, path);
    wchar_t* sl = wcsrchr(dir, L'\\');
    if (sl) { *sl = 0; EnsureDirTree(dir); }
    HANDLE h = CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    DWORD w;
    int ok = WriteFile(h, data, len, &w, NULL) && w == len;
    FlushFileBuffers(h);
    CloseHandle(h);
    return ok;
}

static unsigned ReadU16(const unsigned char* p) { return p[0] | (p[1] << 8); }
static unsigned ReadU32(const unsigned char* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((unsigned)p[3] << 24); }

// Make kLaunchExe the first line of ExternNavi.cfg (backup the original once).
static void RewriteCfg()
{
    static const char target[] = "\\SDMMC\\VestaLaunch\\VestaLaunch.exe";
    if (GetFileAttributes(g_cfg) == 0xFFFFFFFF) {
        char buf[64];
        strcpy(buf, target); strcat(buf, "\r\n");
        WriteFileBytes(g_cfg, buf, (DWORD)strlen(buf));
        return;
    }
    HANDLE h = CreateFile(g_cfg, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    DWORD n = 0, got = 0;
    char* buf = NULL;
    if (h != INVALID_HANDLE_VALUE) {
        n = GetFileSize(h, NULL); if (n == 0xFFFFFFFF) n = 0;
        buf = (char*)malloc(n + 1);
        if (buf) { ReadFile(h, buf, n, &got, NULL); buf[got] = 0; }
        CloseHandle(h);
    }
    if (!buf) { buf = (char*)malloc(1); buf[0] = 0; got = 0; }
    if (got >= (DWORD)strlen(target) && memcmp(buf, target, strlen(target)) == 0) {
        free(buf); return;   // already first
    }
    wchar_t bak[MAX_PATH]; wcscpy(bak, g_cfg); wcscat(bak, L".vesta.bak");
    if (GetFileAttributes(bak) == 0xFFFFFFFF) MyCopyFile(g_cfg, bak);

    char* out = (char*)malloc(strlen(target) + 2 + got + 1);
    strcpy(out, target); strcat(out, "\r\n");
    memcpy(out + strlen(out), buf, got);
    out[strlen(target) + 2 + got] = 0;
    WriteFileBytes(g_cfg, out, (DWORD)strlen(out));
    free(out);
    free(buf);
}

// Write the embedded launcher + skin art out of the RCDATA payload. "VestaLaunch.exe" and "VestaLaunch/..."
// (its Skin folder) go to kLaunchDir; other names are skin files laid out over the VESTA folder (Temp\main\... etc.).
static int ExtractPayload()
{
    HRSRC r = FindResource(NULL, MAKEINTRESOURCE(IDR_PAYLOAD), RT_RCDATA);
    if (!r) return 0;
    DWORD size = SizeofResource(NULL, r);
    HGLOBAL g = LoadResource(NULL, r);
    const unsigned char* base = (const unsigned char*)LockResource(g);
    if (!base || size < 4) return 0;
    const unsigned char* p = base;
    DWORD n = ReadU32(p); p += 4;
    int ok = 0;
    for (DWORD i = 0; i < n; i++) {
        if ((DWORD)(p - base) + 2 > size) break;
        unsigned short nl = ReadU16(p); p += 2;
        char name[256];
        if (nl >= sizeof(name)) break;
        if ((DWORD)(p - base) + nl + 4 > size) break;
        memcpy(name, p, nl); name[nl] = 0; p += nl;
        DWORD dl = ReadU32(p); p += 4;
        if ((DWORD)(p - base) + dl > size) break;
        const unsigned char* data = p; p += dl;
        if (!strcmp(name, "VestaLaunch.exe")) {
            WriteFileBytes(kLaunchExe, data, dl);
        } else if (!strncmp(name, "VestaLaunch/", 12)) {
            wchar_t wname[256];
            MultiByteToWideChar(CP_UTF8, 0, name + 12, -1, wname, 256);
            for (wchar_t* s = wname; *s; s++)
                if (*s == L'/')
                    *s = L'\\';
            wchar_t dst[MAX_PATH];
            Join(dst, kLaunchDir, wname);
            WriteFileBytes(dst, data, dl);
        } else {
            wchar_t wname[256];
            MultiByteToWideChar(CP_UTF8, 0, name, -1, wname, 256);
            wchar_t dst[MAX_PATH];
            Join(dst, g_dstSkin, wname);
            WriteFileBytes(dst, data, dl);
        }
        ok++;
    }
    return ok;
}

static void DoCheck()
{
    Detect();
    if (!g_exeExists)
        _snwprintf(g_status, 300, L"G3NavHMI.exe не найден в %s", g_appDir);
    else
        _snwprintf(g_status, 300, L"Состояние: MMC27=%d VESTA=%d, loopback=%s, лаунчер=%s.",
                   g_mmc, g_vesta, g_appl ? L"ДА" : L"нет", g_hasLaunch ? L"есть" : L"нет");
}

static void DoInstall()
{
    Detect();
    if (!g_exeExists)      { wcscpy(g_status, L"Ошибка: G3NavHMI.exe не найден."); return; }
    if (!EnsureUnlocked()) { wcscpy(g_status, L"Отменено: файл занят оболочкой."); return; }

    // one backup restores both patches (loopback + skin paths) on uninstall
    if (GetFileAttributes(g_exeBak) == 0xFFFFFFFF) MyCopyFile(g_exe, g_exeBak);
    int lp = ReplaceInFile(g_exe, kLoopOld, kLoopNew);
    int sp = 0;
    if (g_mmc && !g_vesta)
        sp = ReplaceInFile(g_exe, kOld, kNew);

    // clone the factory skin to VESTA (once), overlay our art, drop the launcher on SD
    if (!g_hasVesta) CloneTree(g_srcSkin, g_dstSkin);
    int art = ExtractPayload();

    RewriteCfg();

    Detect();
    _snwprintf(g_status, 300, L"Готово: loopback %d, скин %d путей, файлов %d.", lp, sp, art);
}

static void DoUninstall()
{
    Detect();
    if (!g_exeExists)      { wcscpy(g_status, L"Ошибка: G3NavHMI.exe не найден."); return; }
    if (!EnsureUnlocked()) { wcscpy(g_status, L"Отменено: файл занят оболочкой."); return; }

    if (GetFileAttributes(g_exeBak) != 0xFFFFFFFF) {
        MyCopyFile(g_exeBak, g_exe);
    } else {
        ReplaceInFile(g_exe, kNew, kOld);
        ReplaceInFile(g_exe, kLoopNew, kLoopOld);
    }

    if (g_hasVesta) RemoveTree(g_dstSkin);

    wchar_t bak[MAX_PATH]; wcscpy(bak, g_cfg); wcscat(bak, L".vesta.bak");
    if (GetFileAttributes(bak) != 0xFFFFFFFF) {
        MyCopyFile(bak, g_cfg);
        DeleteFile(bak);
    }

    if (g_hasLaunch) RemoveTree(kLaunchDir);

    Detect();
    _snwprintf(g_status, 300, L"Удалено: exe восстановлен, папки VESTA и VestaLaunch удалены.");
}

// ---------------------------------------------------------------- UI
#define C_INK    RGB(0x0b,0x0c,0x0e)
#define C_PANEL  RGB(0x17,0x19,0x1c)
#define C_LINE   RGB(0x34,0x38,0x3f)
#define C_TEXT   RGB(0xf3,0xf1,0xec)
#define C_MUTED  RGB(0xa3,0xa8,0xb0)
#define C_ORANGE RGB(0xff,0x6b,0x1a)
#define C_FACET  RGB(0xc2,0x4a,0x0b)

struct Btn { RECT rc; const wchar_t* label; int id; };
enum { B_INSTALL, B_UNINSTALL, B_EXIT };
static const Btn g_btns[3] = {
    { {  30, 398, 260, 452 }, L"УСТАНОВИТЬ", B_INSTALL },
    { { 280, 398, 510, 452 }, L"УДАЛИТЬ",    B_UNINSTALL },
    { { 530, 398, 770, 452 }, L"ВЫХОД",      B_EXIT },
};
static HFONT g_font, g_fontB;
static int   g_press = -1;

static HFONT MakeFont(int height, int weight)
{
    LOGFONT lf;
    memset(&lf, 0, sizeof(lf));
    lf.lfHeight = -height;
    lf.lfWeight = weight;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = ANTIALIASED_QUALITY;
    wcscpy(lf.lfFaceName, L"Tahoma");
    return CreateFontIndirect(&lf);
}

static void TextL(HDC dc, int x, int y, const wchar_t* s, COLORREF c, HFONT f)
{
    RECT rc = { x, y, 1600, y + 44 };
    HGDIOBJ of = SelectObject(dc, f);
    SetTextColor(dc, c);
    SetBkMode(dc, TRANSPARENT);
    DrawText(dc, s, -1, &rc, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(dc, of);
}

static void TextC(HDC dc, const RECT* rc, const wchar_t* s, COLORREF c, HFONT f)
{
    RECT r = *rc;
    HGDIOBJ of = SelectObject(dc, f);
    SetTextColor(dc, c);
    SetBkMode(dc, TRANSPARENT);
    DrawText(dc, s, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(dc, of);
}

static void FillC(HDC dc, RECT* r, COLORREF c)
{
    HBRUSH b = CreateSolidBrush(c);
    FillRect(dc, r, b);
    DeleteObject(b);
}

static void FrameC(HDC dc, RECT* r, COLORREF c)
{
    RECT t;
    t = *r; t.bottom = t.top + 2;    FillC(dc, &t, c);
    t = *r; t.top    = t.bottom - 2; FillC(dc, &t, c);
    t = *r; t.right  = t.left + 2;   FillC(dc, &t, c);
    t = *r; t.left   = t.right - 2;  FillC(dc, &t, c);
}

static void Paint(HDC dc)
{
    RECT full = { 0, 0, 800, 480 };
    FillC(dc, &full, C_INK);

    TextL(dc, 30, 18, L"VESTA — VestaLaunch + скин оболочки", C_TEXT, g_fontB);
    TextL(dc, 30, 62, L"Ставит запускалку VestaLaunch и скин VESTA в один проход. Обратимо.", C_MUTED, g_font);

    wchar_t line[512];
    int y = 110; const int lh = 32;
    _snwprintf(line, 512, L"Папка: %s", g_appDir);
    TextL(dc, 30, y, line, C_TEXT, g_font); y += lh;
    _snwprintf(line, 512, L"G3NavHMI.exe: %s", g_exeExists ? L"найден" : L"НЕ найден");
    TextL(dc, 30, y, line, g_exeExists ? C_TEXT : C_ORANGE, g_font); y += lh;
    _snwprintf(line, 512, L"Пути в exe: MMC27 = %d, VESTA = %d", g_mmc, g_vesta);
    TextL(dc, 30, y, line, C_TEXT, g_font); y += lh;
    _snwprintf(line, 512, L"Папка VESTA: %s", g_hasVesta ? L"есть" : L"нет");
    TextL(dc, 30, y, line, C_TEXT, g_font); y += lh;
    _snwprintf(line, 512, L"Патч loopback (127.0.0.2): %s", g_appl ? L"ДА" : L"нет");
    TextL(dc, 30, y, line, g_appl ? C_ORANGE : C_TEXT, g_font); y += lh;
    _snwprintf(line, 512, L"VestaLaunch: %s", g_hasLaunch ? L"есть" : L"нет");
    TextL(dc, 30, y, line, g_hasLaunch ? C_TEXT : C_ORANGE, g_font); y += lh;
    y += 10;
    TextL(dc, 30, y, g_status, C_ORANGE, g_font);

    for (int i = 0; i < 3; i++) {
        RECT r = g_btns[i].rc;
        int pressed = (g_press == i);
        FillC(dc, &r, pressed ? C_ORANGE : C_PANEL);
        FrameC(dc, &r, pressed ? C_FACET : C_LINE);
        TextC(dc, &r, g_btns[i].label, C_TEXT, g_font);
    }
}

static int HitTest(int x, int y)
{
    for (int i = 0; i < 3; i++) {
        RECT r = g_btns[i].rc;
        if (x >= r.left && x < r.right && y >= r.top && y < r.bottom) return i;
    }
    return -1;
}

static void RunAction(int id)
{
    if (id == B_INSTALL)   DoInstall();
    if (id == B_UNINSTALL) DoUninstall();
    InvalidateRect(g_hwnd, NULL, TRUE);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE:
        g_hwnd = hwnd;
        g_font  = MakeFont(20, FW_NORMAL);
        g_fontB = MakeFont(30, FW_BOLD);
        DoCheck();
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(hwnd, &ps);
        Paint(dc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        int i = HitTest(LOWORD(lp), HIWORD(lp));
        if (i >= 0) { g_press = i; InvalidateRect(hwnd, NULL, TRUE); }
        return 0;
    }
    case WM_LBUTTONUP: {
        int i = HitTest(LOWORD(lp), HIWORD(lp));
        int was = g_press; g_press = -1;
        if (i >= 0 && i == was) {
            if (g_btns[i].id == B_EXIT) { DestroyWindow(hwnd); return 0; }
            RunAction(g_btns[i].id);
        } else {
            InvalidateRect(hwnd, NULL, TRUE);
        }
        return 0;
    }
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) { DestroyWindow(hwnd); return 0; }
        return 0;
    case WM_DESTROY:
        if (g_font)  { DeleteObject(g_font);  g_font = 0; }
        if (g_fontB) { DeleteObject(g_fontB); g_fontB = 0; }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

// ---------------------------------------------------------------- command line
// Args: [<appdir>] [status|install|uninstall]
static void ParseArgs(LPWSTR cmd, wchar_t* appOut, int* verb)
{
    *verb = 0; appOut[0] = 0;
    wchar_t* p = cmd;
    while (*p) {
        while (*p == L' ' || *p == L'\t') p++;
        if (!*p) break;
        wchar_t tok[MAX_PATH]; int n = 0;
        int quoted = (*p == L'"'); if (quoted) p++;
        while (*p && ((quoted && *p != L'"') || (!quoted && *p != L' ' && *p != L'\t'))) {
            if (n < MAX_PATH - 1) tok[n++] = *p;
            p++;
        }
        tok[n] = 0;
        if (quoted && *p == L'"') p++;
        if (!_wcsicmp(tok, L"install"))        *verb = 1;
        else if (!_wcsicmp(tok, L"uninstall")) *verb = 2;
        else if (!_wcsicmp(tok, L"status"))    *verb = 3;
        else if (tok[0])                       wcscpy(appOut, tok);
    }
}

#ifdef UNDER_CE
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPWSTR lpCmdLine, int nShow)
#else
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, LPWSTR lpCmdLine, int nShow)
#endif
{
    wchar_t app[MAX_PATH]; int verb;
    ParseArgs(lpCmdLine, app, &verb);
    if (app[0]) {
        wcscpy(g_appDir, app);
    } else {
#ifdef UNDER_CE
        wcscpy(g_appDir, L"\\ResidentFlash\\navigation\\app");
#else
        wcscpy(g_appDir, L"..\\out\\scratch_app");
#endif
    }

    if (verb) {
        Detect();
        if (verb == 1)      DoInstall();
        else if (verb == 2) DoUninstall();
        else                DoCheck();
        LogMsg(g_status);
        MessageBox(NULL, g_status, L"Vesta Skin", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    WNDCLASS wc; memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = APP_CLASS;
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    if (!RegisterClass(&wc)) return 1;

    HWND hwnd = CreateWindow(APP_CLASS, L"Vesta Skin", WS_POPUP,
        0, 0, 800, 480, NULL, NULL, hInst, NULL);
    if (!hwnd) return 1;
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
