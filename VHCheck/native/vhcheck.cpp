// VHCheck - before VestaHome moves to the stub alone, no proxy DLLs (the user 2026-11-20: one build for the stock
// firmware and the CAN panel's; the CAN panel brings RadioCommWindow up its own way): what the unit is and has, read
// and tried without changing it - the firmware's files (size, MD5, build time), whether its DLLs export every vendor
// function our code calls (needs.inc, made from our sources by tools/make_vhcheck_needs.py; read from the files, not
// loaded), the start chain (HKLM\init, the active drivers), which process holds the MCU's and Bluetooth's DLLs, every
// top window and RadioCommWindow (by class and by title, its process; at the start and later), the GPS / CAN ports
// read-only (COM3: whether the CAN panel's can_panel.exe holds it; VSP4/VSP5: what it hands out), a write into the
// shell's folder, the MCU as a client for 20 s (as McuProbe), and - asked first, put back after - whether the CAN
// panel's always-on-top window can be hidden or stayed under a top window of ours (it would lie over VestaHome).
// The log always (one run by hand, no VestaHome to switch logs on): \SDMemory\VestaLogs\VHCheck.log, else the USB
// disk's. Any firmware of this unit (the user 2026-11-20): its parts named by their build (FwKnown) - the CAN panel
// carries 4123's OS with 4125's \ResidentFlash. On the PC (VHCheckPC.exe <folders>): the same for a firmware
// unpacked there - the files, their functions, the firmware - each unit path looked for by its file name in the
// folders (MAIN's 4125_unpacked, the \ResidentFlash modules of its SwDnl.upg by tools/upg_extract.py).
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdarg.h>
#include "pecheck.h"
#include "privacy.h"
#include "needs.inc"
#include "../../SPIPower/install/md5.h"

#ifndef TH32CS_SNAPNOHEAPS
#define TH32CS_SNAPNOHEAPS 0x40000000
#endif

static const wchar_t* kApp = L"\\ResidentFlash\\navigation\\app\\";
static wchar_t s_log[MAX_PATH];

#ifndef UNDER_CE
static wchar_t** s_roots;   // the PC: the folders of an unpacked firmware
static int s_nroots;

static bool FindIn(const wchar_t* dir, const wchar_t* name, wchar_t* out)   // name anywhere under dir
{
    wchar_t pat[MAX_PATH];
    _snwprintf(pat, MAX_PATH, L"%s\\*", dir);
    WIN32_FIND_DATA fd;
    HANDLE h = FindFirstFile(pat, &fd);
    bool found = false;
    for (BOOL ok = h != INVALID_HANDLE_VALUE; ok && !found; ok = FindNextFile(h, &fd)) {
        _snwprintf(out, MAX_PATH, L"%s\\%s", dir, fd.cFileName);
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            found = !_wcsicmp(fd.cFileName, name);
        else if (fd.cFileName[0] != L'.') {
            wchar_t sub[MAX_PATH];
            wcscpy(sub, out);
            found = FindIn(sub, name, out);
        }
    }
    if (h != INVALID_HANDLE_VALUE)
        FindClose(h);
    return found;
}
#endif

// A unit path: as it is on the unit; on the PC the file of that name in the folders given (or the path itself -
// then not there).
static const wchar_t* Firm(const wchar_t* path)
{
#ifdef UNDER_CE
    return path;
#else
    static wchar_t found[4][MAX_PATH];
    static int next;
    wchar_t* out = found[next++ % 4];
    const wchar_t* name = wcsrchr(path, L'\\') ? wcsrchr(path, L'\\') + 1 : path;
    for (int i = 0; i < s_nroots; i++)
        if (FindIn(s_roots[i], name, out))
            return out;
    return path;
#endif
}

// ---------------------------------------------------------------- the log

static bool LogPlace()   // the SD card, else a USB disk; neither - not run (the PC: VHCheckPC.log here)
{
#ifndef UNDER_CE
    wcscpy(s_log, L"VHCheckPC.log");
    DeleteFile(s_log);
    return true;
#endif
    static const wchar_t* kVol[2] = { L"\\SDMemory", L"\\USBDevice" };
    for (int i = 0; i < 2; i++) {
        DWORD a = GetFileAttributes(kVol[i]);
        if (a == 0xFFFFFFFF || !(a & FILE_ATTRIBUTE_DIRECTORY))
            continue;
        wchar_t dir[MAX_PATH];
        _snwprintf(dir, MAX_PATH, L"%s\\VestaLogs", kVol[i]);
        CreateDirectory(dir, NULL);
        _snwprintf(s_log, MAX_PATH, L"%s\\VHCheck.log", dir);
        return true;
    }
    return false;
}

static void Log(const wchar_t* fmt, ...)
{
    wchar_t line[600];
    SYSTEMTIME t;
    GetLocalTime(&t);
    int n = _snwprintf(line, 64, L"%02d:%02d:%02d ", t.wHour, t.wMinute, t.wSecond);
    va_list a;
    va_start(a, fmt);
    _vsnwprintf(line + n, 600 - n - 3, fmt, a);
    va_end(a);
    line[597] = 0;
    wcscat(line, L"\r\n");
    char u8[1800];
    int len = WideCharToMultiByte(CP_UTF8, 0, line, -1, u8, sizeof(u8), NULL, NULL) - 1;
#ifndef UNDER_CE
    printf("%s", u8);   // (the PC: the console too)
#endif
    HANDLE f = CreateFile(s_log, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, 0, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return;
    SetFilePointer(f, 0, NULL, FILE_END);
    DWORD w;
    WriteFile(f, u8, len > 0 ? len : 0, &w, NULL);
    CloseHandle(f);
}

// The summary's lines (the log's end and the closing message).
static wchar_t s_sum[1500];
static void Sum(const wchar_t* fmt, ...)
{
    wchar_t line[200];
    va_list a;
    va_start(a, fmt);
    _vsnwprintf(line, 199, fmt, a);
    va_end(a);
    line[199] = 0;
    Log(L"ИТОГ: %s", line);
    if (wcslen(s_sum) + wcslen(line) + 3 < 1500) {
        wcscat(s_sum, line);
        wcscat(s_sum, L"\r\n");
    }
}

// ---------------------------------------------------------------- files

static unsigned char* ReadAll(const wchar_t* path, int* len, int max)
{
    HANDLE f = CreateFile(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return NULL;
    DWORD size = GetFileSize(f, NULL), got = 0;
    unsigned char* b = size <= (DWORD)max ? (unsigned char*)malloc(size + 1) : NULL;
    if (b && (!ReadFile(f, b, size, &got, NULL) || got != size)) {
        free(b);
        b = NULL;
    }
    CloseHandle(f);
    *len = (int)got;
    return b;
}

static void StampText(unsigned stamp, wchar_t* out, int cap)   // a PE's build time as "dd.mm.yyyy"
{
    ULARGE_INTEGER u;
    u.QuadPart = ((unsigned __int64)stamp + 11644473600ULL) * 10000000ULL;
    FILETIME ft;
    ft.dwLowDateTime = u.LowPart;
    ft.dwHighDateTime = u.HighPart;
    SYSTEMTIME st;
    FileTimeToSystemTime(&ft, &st);
    _snwprintf(out, cap, L"%02d.%02d.%04d", st.wDay, st.wMonth, st.wYear);
}

// One file into the log: size, MD5, a PE's build time and exports; its bytes kept (DLLs) when keep.
static unsigned char* FileFacts(const wchar_t* path, int* outLen, bool keep)
{
    int len = 0;
    unsigned char* b = ReadAll(path, &len, 8 * 1024 * 1024);
    if (!b) {   // (HU 2026-11-20: \Windows's modules of the ROM are there, loaded, but no file to read)
        DWORD a = GetFileAttributes(path);
        if (a == 0xFFFFFFFF)
            Log(L"  %s: нет", path);
        else
            Log(L"  %s: есть, как файл не читается (модуль ПЗУ; атрибуты 0x%X)", path, a);
        return NULL;
    }
    md5_ctx c;
    unsigned char d[16];
    md5_init(&c);
    md5_update(&c, b, (unsigned)len);
    md5_final(&c, d);
    wchar_t md[40], when[16] = L"";
    for (int i = 0; i < 16; i++)
        _snwprintf(md + 2 * i, 3, L"%02x", d[i]);
    PeFacts f;
    if (PeRead(b, len, &f))
        StampText(f.stamp, when, 16);
    Log(L"  %s: %d Б, md5 %s%s%s, экспортов %d", path, len, md, when[0] ? L", сборка " : L"", when, f.pe ? f.exports : 0);
    if (!keep) {
        free(b);
        return NULL;
    }
    *outLen = len;
    return b;
}

static void Files()
{
    static const wchar_t* const kFiles[] = {
        L"\\ResidentFlash\\Main.exe", L"\\Windows\\main.exe", L"\\Windows\\Init.exe", L"\\Windows\\mcu.exe",
        L"\\ResidentFlash\\navigation\\app\\G3NavHMI.exe", L"\\ResidentFlash\\navigation\\app\\G3NavHMI_orig.exe",
        L"\\ResidentFlash\\navigation\\app\\NCL_BT.DLL", L"\\ResidentFlash\\navigation\\app\\NCL_BaseCom.dll",
        L"\\Windows\\SPIPowerMgr_OS.dll", L"\\Windows\\pic.dll", L"\\Windows\\vsp.dll", L"\\Windows\\virtualserial.dll",
        L"\\ResidentFlash\\CAN\\can_panel.exe", L"\\ResidentFlash\\dll\\xBtProt.dll",
        L"\\ResidentFlash\\navigation\\app\\Config\\ExternNavi.cfg", L"\\ResidentFlash\\SwUpdate.flag",
        L"\\ResidentFlash\\CreateProcessFail.flag",
    };
    Log(L"== файлы");
    for (int i = 0; i < (int)(sizeof(kFiles) / sizeof(kFiles[0])); i++)
        FileFacts(Firm(kFiles[i]), NULL, false);
}

// ---------------------------------------------------------------- the firmware

static unsigned StampOf(const wchar_t* path)   // a PE's build stamp, 0 none
{
    int len = 0;
    unsigned char* b = ReadAll(Firm(path), &len, 8 * 1024 * 1024);
    PeFacts f;
    bool pe = b && PeRead(b, len, &f);
    free(b);
    return pe ? f.stamp : 0;
}

// One part of the firmware by its file's build: the vendor's own copy beside ours first (G3NavHMI_orig.exe under our
// stub, SPIPowerMgr_orig.dll under our proxy), else the file itself (orig NULL: only it).
static void Part(const wchar_t* label, int part, const wchar_t* orig, const wchar_t* path, wchar_t* line, int cap)
{
    unsigned st = orig ? StampOf(orig) : 0;
    if (!st)
        st = StampOf(path);
    WIN32_FIND_DATA fd;
    HANDLE h = st ? INVALID_HANDLE_VALUE : FindFirstFile(Firm(path), &fd);
    if (h != INVALID_HANDLE_VALUE) {   // (an XIP module of the OS, not readable as a file: its time in the ROM - the
        FindClose(h);                  // same day as its build, nkdiff "list")
        ULARGE_INTEGER u;
        u.LowPart = fd.ftLastWriteTime.dwLowDateTime;
        u.HighPart = fd.ftLastWriteTime.dwHighDateTime;
        st = u.QuadPart > 116444736000000000ULL ? (unsigned)((u.QuadPart - 116444736000000000ULL) / 10000000) : 0;
    }
    wchar_t when[16] = L"", one[96];
    if (st)
        StampText(st, when, 16);
    int fw = st ? FwKnown(part, st) : 0;
    if (!st)
        _snwprintf(one, 96, L"%s: не прочитать", label);
    else if (fw)
        _snwprintf(one, 96, L"%s %d (%s)", label, fw, when);
    else
        _snwprintf(one, 96, L"%s: неизвестная сборка от %s", label, when);
    if ((int)(wcslen(line) + wcslen(one) + 3) < cap) {
        wcscat(line, line[0] ? L"; " : L"");
        wcscat(line, one);
    }
}

static void Firmware()
{
    wchar_t line[300] = L"";
    Part(L"ОС из пакета", FW_OS, NULL, L"\\Windows\\desay_sdk_api.dll", line, 300);
    Part(L"оболочка", FW_SHELL, L"\\ResidentFlash\\navigation\\app\\G3NavHMI_orig.exe",
         L"\\ResidentFlash\\navigation\\app\\G3NavHMI.exe", line, 300);
    Part(L"SPIPowerMgr", FW_SPM, L"\\ResidentFlash\\navigation\\app\\SPIPowerMgr_orig.dll",
         L"\\ResidentFlash\\navigation\\app\\SPIPowerMgr.dll", line, 300);
    Sum(L"прошивка: %s", line);
    int len = 0, sw = 0;
    unsigned char* b = ReadAll(Firm(L"\\ResidentFlash\\navigation\\app\\G3NavHMI_orig.exe"), &len, 8 * 1024 * 1024);
    if (!b)
        b = ReadAll(Firm(L"\\ResidentFlash\\navigation\\app\\G3NavHMI.exe"), &len, 8 * 1024 * 1024);
    if (b)
        sw = FwVersion(b, len, 2);
    free(b);
    if (sw)
        Sum(L"SW 27.0.0.%d (строка оболочки, как в «Инфо»)", sw);
    else
        Sum(L"SW: строки версии в оболочке нет");
}

// ---------------------------------------------------------------- the vendor functions we call

// A module of the ROM (no file to read): its functions asked of the loaded module; -1 - it does not load.
static int MissingLoaded(const wchar_t* path, const char* const* want, int n)
{
    HMODULE h = LoadLibrary(path);
    if (!h)
        return -1;
    int m = 0;
    for (int i = 0; i < n; i++) {
#ifdef UNDER_CE
        wchar_t w[160];
        MultiByteToWideChar(CP_ACP, 0, want[i], -1, w, 160);
        FARPROC p = GetProcAddress(h, w);
#else
        FARPROC p = GetProcAddress(h, want[i]);
#endif
        if (!p) {
            Log(L"  нет функции %S", want[i]);
            m++;
        }
    }
    FreeLibrary(h);
    return m;
}

// The vendor's DLL (its own copy beside our proxy first: SPIPowerMgr_orig.dll, FLC_BLUETOOTH_SDK_orig.dll) checked
// for want[]; the count missing.
static int Needs(const wchar_t* label, const wchar_t* const* paths, int np, const char* const* want, int n)
{
    int len = 0;
    unsigned char* b = NULL;
    const wchar_t* used = NULL;
    for (int i = 0; i < np && !b; i++)
        if (GetFileAttributes(Firm(paths[i])) != 0xFFFFFFFF) {
            used = paths[i];
            b = FileFacts(Firm(paths[i]), &len, true);
        }
    PeFacts pf;
    if (b && !PeRead(b, len, &pf)) {   // read, but no PE in it (a ROM module's file): asked of the loaded module
        free(b);
        b = NULL;
    }
    // (the tester's 4120, 2026-11-20: \Windows\desay_sdk_api.dll loaded by main.exe and the shell, no file of it)
    bool asFile = used != NULL;
    if (!b && !used)
        used = paths[np - 1];
    if (!b) {
        int m = MissingLoaded(Firm(used), want, n);
        if (m < 0 && !asFile)
            Sum(L"%s: файла нет", label);
        else if (m < 0)
            Sum(L"%s: модуль ПЗУ не загрузился", label);
        else if (m)
            Sum(L"%s: нет %d из %d функций (%s, модуль ПЗУ)", label, m, n, used);
        else
            Sum(L"%s: все %d функций есть (модуль ПЗУ)", label, n);
        return m < 0 ? n : m;
    }
    int miss[200];
    int m = PeMissing(b, len, want, n, miss, 200);
    for (int i = 0; i < m && i < 200; i++)
        Log(L"  нет функции %S", want[miss[i]]);
    free(b);
    if (m)
        Sum(L"%s: нет %d из %d функций (%s)", label, m, n, used);
    else
        Sum(L"%s: все %d функций есть", label, n);
    return m;
}

#define COUNT(a) (int)(sizeof(a) / sizeof(a[0]))

static void Functions()
{
    Log(L"== функции, которые вызывает наш код (needs.inc)");
    const wchar_t* spm[2] = { L"\\ResidentFlash\\navigation\\app\\SPIPowerMgr_orig.dll",
                              L"\\ResidentFlash\\navigation\\app\\SPIPowerMgr.dll" };
    const wchar_t* flc[2] = { L"\\ResidentFlash\\navigation\\app\\FLC_BLUETOOTH_SDK_orig.dll",
                              L"\\ResidentFlash\\navigation\\app\\FLC_BLUETOOTH_SDK.dll" };
    const wchar_t* bt[1] = { L"\\ResidentFlash\\dll\\btapi.dll" };
    const wchar_t* sv[1] = { L"\\Windows\\desay_sdk_api.dll" };
    Needs(L"SPIPowerMgr", spm, 2, kNeedSpm, COUNT(kNeedSpm));
    Needs(L"FLC_BLUETOOTH_SDK", flc, 2, kNeedFlc, COUNT(kNeedFlc));
    Needs(L"btapi", bt, 1, kNeedBtapi, COUNT(kNeedBtapi));
    Needs(L"desay_sdk_api", sv, 1, kNeedSv, COUNT(kNeedSv));
}

#ifdef UNDER_CE   // (the unit only from here to the run: the registry, processes, windows, ports, MCU)
// ---------------------------------------------------------------- the registry

static void RegValues(HKEY root, const wchar_t* path)
{
    HKEY k;
    if (RegOpenKeyEx(root, path, 0, 0, &k) != ERROR_SUCCESS) {
        Log(L"  [%s] нет", path);
        return;
    }
    Log(L"  [%s]", path);
    for (DWORD i = 0;; i++) {
        wchar_t name[80];
        BYTE data[400];
        DWORD nl = 80, dl = sizeof(data) - 2, type;
        if (RegEnumValue(k, i, name, &nl, NULL, &type, data, &dl) != ERROR_SUCCESS)
            break;
        data[dl] = data[dl + 1] = 0;
        if (type == REG_SZ || type == REG_EXPAND_SZ)
            Log(L"    %s = \"%s\"", name, (wchar_t*)data);
        else if (type == REG_DWORD)
            Log(L"    %s = 0x%08X", name, *(DWORD*)data);
        else if (type == REG_MULTI_SZ)
            Log(L"    %s = \"%s\"...", name, (wchar_t*)data);
        else
            Log(L"    %s (тип %u, %u Б)", name, type, dl);
    }
    RegCloseKey(k);
}

static void Registry()
{
    Log(L"== реестр");
    RegValues(HKEY_LOCAL_MACHINE, L"init");
    HKEY k;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"Drivers\\Active", 0, 0, &k) != ERROR_SUCCESS)
        return;
    Log(L"  [Drivers\\Active]");
    for (DWORD i = 0;; i++) {
        wchar_t sub[32], path[64], name[32] = L"", key[160] = L"";
        DWORD sl = 32;
        if (RegEnumKeyEx(k, i, sub, &sl, NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
            break;
        _snwprintf(path, 64, L"Drivers\\Active\\%s", sub);
        HKEY s;
        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, path, 0, 0, &s) == ERROR_SUCCESS) {
            DWORD l = sizeof(name), t;
            RegQueryValueEx(s, L"Name", NULL, &t, (BYTE*)name, &l);
            l = sizeof(key);
            RegQueryValueEx(s, L"Key", NULL, &t, (BYTE*)key, &l);
            RegCloseKey(s);
        }
        Log(L"    %s %s <- %s", sub, name, key);
    }
    RegCloseKey(k);
}

// ---------------------------------------------------------------- processes and their modules

typedef HANDLE (WINAPI *SnapFn)(DWORD, DWORD);
typedef BOOL (WINAPI *ProcFn)(HANDLE, PROCESSENTRY32*);
typedef BOOL (WINAPI *ModFn)(HANDLE, MODULEENTRY32*);
typedef BOOL (WINAPI *CloseFn)(HANDLE);
static SnapFn s_snap;
static ProcFn s_p1, s_pn;
static ModFn s_m1, s_mn;
static CloseFn s_close;

static bool Toolhelp()
{
    HMODULE th = LoadLibrary(L"toolhelp.dll");
    if (!th)
        return false;
    s_snap = (SnapFn)GetProcAddress(th, L"CreateToolhelp32Snapshot");
    s_p1 = (ProcFn)GetProcAddress(th, L"Process32First");
    s_pn = (ProcFn)GetProcAddress(th, L"Process32Next");
    s_m1 = (ModFn)GetProcAddress(th, L"Module32First");
    s_mn = (ModFn)GetProcAddress(th, L"Module32Next");
    s_close = (CloseFn)GetProcAddress(th, L"CloseToolhelp32Snapshot");
    return s_snap && s_p1 && s_pn;
}

static void Release(HANDLE h)
{
    if (s_close)
        s_close(h);
    else
        CloseHandle(h);
}

static bool ExeOf(DWORD pid, wchar_t* out, int cap)   // a process's exe name
{
    out[0] = 0;
    HANDLE h = s_snap ? s_snap(TH32CS_SNAPPROCESS | TH32CS_SNAPNOHEAPS, 0) : INVALID_HANDLE_VALUE;
    if (h == INVALID_HANDLE_VALUE)
        return false;
    PROCESSENTRY32 e;
    e.dwSize = sizeof(e);
    for (BOOL ok = s_p1(h, &e); ok; ok = s_pn(h, &e))
        if (e.th32ProcessID == pid) {
            wcsncpy(out, e.szExeFile, cap - 1);
            out[cap - 1] = 0;
            break;
        }
    Release(h);
    return out[0] != 0;
}

// Its modules of note (the MCU's, Bluetooth's, the CAN panel's ports) into one line.
static void Modules(DWORD pid, wchar_t* line, int cap)
{
    static const wchar_t* const kNote[] = { L"spipowermgr", L"flc_bluetooth_sdk", L"btapi", L"ncl_bt", L"xbtprot",
                                            L"desay_sdk_api", L"vsp", L"virtualserial", L"ncl_basecom" };
    line[0] = 0;
    HANDLE h = s_m1 ? s_snap(TH32CS_SNAPMODULE | TH32CS_SNAPNOHEAPS, pid) : INVALID_HANDLE_VALUE;
    if (h == INVALID_HANDLE_VALUE)
        return;
    MODULEENTRY32 m;
    m.dwSize = sizeof(m);
    for (BOOL ok = s_m1(h, &m); ok; ok = s_mn(h, &m)) {
        enum { kName = sizeof(m.szModule) / sizeof(m.szModule[0]) };
        wchar_t low[kName + 1];
        wcsncpy(low, m.szModule, kName);
        low[kName] = 0;
        _wcslwr(low);
        for (int i = 0; i < COUNT(kNote); i++)
            if (!wcsncmp(low, kNote[i], wcslen(kNote[i])) && (int)(wcslen(line) + wcslen(m.szExePath) + 4) < cap) {
                wcscat(line, L" ");
                wcscat(line, m.szExePath[0] ? m.szExePath : m.szModule);
            }
    }
    Release(h);
}

static void Processes()
{
    Log(L"== процессы (модули MCU, Bluetooth, портов)");
    HANDLE h = s_snap ? s_snap(TH32CS_SNAPPROCESS | TH32CS_SNAPNOHEAPS, 0) : INVALID_HANDLE_VALUE;
    if (h == INVALID_HANDLE_VALUE) {
        Log(L"  toolhelp нет");
        return;
    }
    wchar_t mcu[200] = L"", bt[200] = L"";
    PROCESSENTRY32 e;
    e.dwSize = sizeof(e);
    for (BOOL ok = s_p1(h, &e); ok; ok = s_pn(h, &e)) {
        wchar_t mods[500];
        Modules(e.th32ProcessID, mods, 500);
        Log(L"  %s pid %08X, потоков %u:%s", e.szExeFile, e.th32ProcessID, e.cntThreads, mods);
        _wcslwr(mods);
        if (wcsstr(mods, L"spipowermgr.dll") && wcslen(mcu) + wcslen(e.szExeFile) + 3 < 200) {
            wcscat(mcu, L" ");
            wcscat(mcu, e.szExeFile);
        }
        if (wcsstr(mods, L"flc_bluetooth_sdk") && wcslen(bt) + wcslen(e.szExeFile) + 3 < 200) {
            wcscat(bt, L" ");
            wcscat(bt, e.szExeFile);
        }
    }
    Release(h);
    Sum(L"SPIPowerMgr.dll в процессах:%s", mcu[0] ? mcu : L" нет");
    Sum(L"FLC_BLUETOOTH_SDK.dll в процессах:%s", bt[0] ? bt : L" нет");
}

// ---------------------------------------------------------------- windows

struct WinScan { int n; const wchar_t* when; };

static BOOL CALLBACK OneWindow(HWND w, LPARAM lp)
{
    WinScan* s = (WinScan*)lp;
    wchar_t cls[64] = L"", title[64] = L"", exe[64] = L"";
    GetClassName(w, cls, 64);
    GetWindowText(w, title, 64);
    DWORD pid = 0;
    GetWindowThreadProcessId(w, &pid);
    ExeOf(pid, exe, 64);
    RECT r;
    GetWindowRect(w, &r);
    LONG ex = GetWindowLong(w, GWL_EXSTYLE);
    Log(L"  %08X %s «%s» класс %s: %s%s %d,%d %dx%d", (unsigned)w, exe, title, cls, IsWindowVisible(w) ? L"видно" : L"скрыто",
        ex & WS_EX_TOPMOST ? L", поверх всех" : L"", r.left, r.top, r.right - r.left, r.bottom - r.top);
    s->n++;
    return TRUE;
}

static void Windows(const wchar_t* when)
{
    Log(L"== окна (%s)", when);
    WinScan s = { 0, when };
    EnumWindows(OneWindow, (LPARAM)&s);
    HWND byClass = FindWindow(L"RadioCommWindow", NULL), byTitle = FindWindow(NULL, L"RadioCommWindow");
    wchar_t a[64] = L"-", b[64] = L"-";
    DWORD pid;
    if (byClass && GetWindowThreadProcessId(byClass, &pid))
        ExeOf(pid, a, 64);
    if (byTitle && GetWindowThreadProcessId(byTitle, &pid))
        ExeOf(pid, b, 64);
    Sum(L"RadioCommWindow (%s): по классу %s, по заголовку %s; окон %d", when, byClass ? a : L"нет",
        byTitle ? b : L"нет", s.n);
}

// ---------------------------------------------------------------- the ports, read-only

static void Port(const wchar_t* name)
{
    HANDLE h = CreateFile(name, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        Sum(L"%s: не открывается (%u) - занят или нет", name, GetLastError());
        return;
    }
    COMMTIMEOUTS t = { MAXDWORD, 0, 300, 0, 0 };
    SetCommTimeouts(h, &t);
    char buf[512];
    int total = 0, gps = 0, can = 0, other = 0;
    bool start = true;
    char first[90] = "";
    DWORD t0 = GetTickCount();
    while (GetTickCount() - t0 < 3000) {
        DWORD got = 0;
        if (!ReadFile(h, buf, sizeof(buf), &got, NULL))
            break;
        for (DWORD i = 0; i < got; i++) {
            char c = buf[i];
            if (start && c != '\r' && c != '\n') {
                if (c == '$')
                    gps++;
                else if (c == '@')
                    can++;
                else
                    other++;
                if (!first[0] && got - i > 1) {   // the first line, as it is
                    int k = 0;
                    while (i + k < got && k < 80 && buf[i + k] != '\r' && buf[i + k] != '\n')
                        k++;
                    memcpy(first, buf + i, k);
                    first[k] = 0;
                }
            }
            start = c == '\n';
        }
        total += (int)got;
    }
    CloseHandle(h);
    Sum(L"%s: открылся, за 3 с %d Б, строк $ %d, @ %d, прочих %d", name, total, gps, can, other);
    if (first[0]) {
        char kind[16];
        LineKind(first, kind, sizeof(kind));   // the type only: no position or time in the log
        Log(L"  первая строка: %S…", kind);
    }
}

static void Ports()
{
    Log(L"== порты GPS / CAN, только чтение, по 3 с (COM1 - Bluetooth - не трогаю)");
    static const wchar_t* const kPorts[] = { L"COM3:", L"COM4:", L"VSP4:", L"VSP5:" };
    for (int i = 0; i < COUNT(kPorts); i++)
        Port(kPorts[i]);
}

// ---------------------------------------------------------------- a write into the shell's folder

static void WriteTry()
{
    wchar_t p[MAX_PATH];
    _snwprintf(p, MAX_PATH, L"%sVHCheck.tmp", kApp);
    HANDLE f = CreateFile(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    DWORD w = 0;
    bool ok = f != INVALID_HANDLE_VALUE && WriteFile(f, "VHCheck", 7, &w, NULL) && w == 7;
    if (f != INVALID_HANDLE_VALUE)
        CloseHandle(f);
    bool gone = DeleteFile(p) != 0;
    Sum(L"запись в папку оболочки: %s", ok && gone ? L"можно" : ok ? L"записано, не удалено" : L"нельзя");
}

// ---------------------------------------------------------------- the MCU, a client for 20 s (as McuProbe)

static volatile LONG s_mcuMsgs, s_mcuKinds;
static unsigned short s_codes[64];
static struct { unsigned short code; int len; unsigned char d[48]; } s_info[12];   // "product info"'s answers
static int s_infoN;
static CRITICAL_SECTION s_cs;
struct Skel { void** vtbl; int id; };
static void SkelDtor(void*)
{
}
static void SkelHandle(void*, unsigned char* p)
{
    unsigned short code = (unsigned short)(p[0] << 8 | p[1]);
    EnterCriticalSection(&s_cs);
    s_mcuMsgs++;
    int i = 0;
    while (i < s_mcuKinds && s_codes[i] != code)
        i++;
    if (i == s_mcuKinds && i < 64) {
        s_codes[i] = code;
        s_mcuKinds++;
    }
    if ((code == 0x7902 || code == 0x4112 || code == 0x4118 || code == 0x4111 || code == 0x7922) && s_infoN < 12) {
        s_info[s_infoN].code = code;   // (the answers to "product info": versions, the ID, the BT address)
        s_info[s_infoN].len = p[2] < sizeof(s_info[0].d) ? p[2] : sizeof(s_info[0].d);
        memcpy(s_info[s_infoN].d, p + 3, s_info[s_infoN].len);
        s_infoN++;
    }
    LeaveCriticalSection(&s_cs);
}
static void* s_vtbl[2] = { (void*)SkelDtor, (void*)SkelHandle };

// The answers kept: each as hex and its printable text; the MCU's version from a 0x7902 (TR8005-41xx).
static void InfoLog()
{
    int mcu = 0;
    for (int i = 0; i < s_infoN; i++) {
        wchar_t hex[3 * 48 + 1] = L"", txt[49] = L"";
        for (int k = 0; k < s_info[i].len; k++) {
            if (InfoHidden(s_info[i].code, s_info[i].d, s_info[i].len, k)) {   // the serial, the BT address
                wcscpy(hex + 3 * k, L"** ");
                txt[k] = L'*';
                continue;
            }
            _snwprintf(hex + 3 * k, 4, L"%02X ", s_info[i].d[k]);
            txt[k] = s_info[i].d[k] >= 0x20 && s_info[i].d[k] < 0x7F ? s_info[i].d[k] : L'.';
        }
        txt[s_info[i].len] = 0;
        Log(L"  ответ 0x%04X: %s «%s»", s_info[i].code, hex, txt);
        if (s_info[i].code == 0x7902 && !mcu)
            mcu = FwVersion(s_info[i].d, s_info[i].len, 1);
    }
    if (mcu)
        Sum(L"MCU TR8005-%d (из ответа 0x7902)", mcu);
    else
        Sum(L"версия MCU: ответа 0x7902 нет (ответов на запрос сведений %d)", s_infoN);
}

typedef int (*Fn0)(void);
typedef int (*Fn2)(int, void*);

static void Mcu()
{
    Log(L"== MCU: клиент заводской SPIPowerMgr.dll на 20 с");
    wchar_t path[MAX_PATH];
    _snwprintf(path, MAX_PATH, L"%sSPIPowerMgr_orig.dll", kApp);
    if (GetFileAttributes(path) == 0xFFFFFFFF)
        _snwprintf(path, MAX_PATH, L"%sSPIPowerMgr.dll", kApp);
    LoadLibrary(L"\\ResidentFlash\\navigation\\app\\NCL_BaseCom.dll");
    HMODULE sp = LoadLibrary(path);
    Fn0 load = sp ? (Fn0)GetProcAddress(sp, L"SPApi_LoadRadioEngine") : NULL;
    Fn0 inited = sp ? (Fn0)GetProcAddress(sp, L"SPApi_IsInitialized") : NULL;
    Fn0 init = sp ? (Fn0)GetProcAddress(sp, L"SPApi_InitializeRadioEngine") : NULL;
    Fn2 skel = sp ? (Fn2)GetProcAddress(sp, L"SPApi_SetSkelonton") : NULL;
    if (!load || !inited || !init || !skel) {
        Sum(L"MCU: %s не загрузилась или без нужных функций", path);
        return;
    }
    InitializeCriticalSection(&s_cs);
    load();
    if (!inited())
        init();
    static Skel skels[0x12];
    for (int i = 0; i < 0x12; i++) {
        skels[i].vtbl = s_vtbl;
        skels[i].id = i + 1;
        skel(i + 1, &skels[i]);
    }
    Fn0 info = (Fn0)GetProcAddress(sp, L"SPApi_SendRadioRequestProductInfo");   // (as the stock "Info" asks)
    Fn0 bt = (Fn0)GetProcAddress(sp, L"SPApi_SendRadioRequestBTAddress");
    if (info)
        info();
    if (bt)
        bt();
    // the versions (0x7902) come after key 0x9B - as the stock shell at its start (VHCheck 1.1 on 4120 / 4125: none to
    // the product info alone); pressed, released
    typedef int (*Fn3)(int, int, int);
    Fn3 key = (Fn3)GetProcAddress(sp, L"SPApi_SendKeyEvent");
    if (key) {
        key(0x9B, 0, 0);
        key(0x9B, 1, 1);
    }
    Sleep(20000);
    EnterCriticalSection(&s_cs);
    wchar_t codes[400] = L"";
    for (int i = 0; i < s_mcuKinds && wcslen(codes) < 390; i++)
        _snwprintf(codes + wcslen(codes), 400 - wcslen(codes), L" %04X", s_codes[i]);
    LeaveCriticalSection(&s_cs);
    Log(L"  коды:%s", codes);
    InfoLog();
    Sum(L"MCU: за 20 с %d сообщений, видов %d (%s)", (int)s_mcuMsgs, (int)s_mcuKinds, path);
}

// ---------------------------------------------------------------- the CAN panel's window over all (asked first)

struct CanWin { HWND w[16]; LONG ex[16]; BOOL vis[16]; int n; DWORD pid; };

static BOOL CALLBACK CollectCan(HWND w, LPARAM lp)
{
    CanWin* c = (CanWin*)lp;
    DWORD pid = 0;
    GetWindowThreadProcessId(w, &pid);
    if (pid == c->pid && c->n < 16) {
        c->w[c->n] = w;
        c->ex[c->n] = GetWindowLong(w, GWL_EXSTYLE);
        c->vis[c->n] = IsWindowVisible(w);
        c->n++;
    }
    return TRUE;
}

static DWORD PidOf(const wchar_t* exe)
{
    HANDLE h = s_snap ? s_snap(TH32CS_SNAPPROCESS | TH32CS_SNAPNOHEAPS, 0) : INVALID_HANDLE_VALUE;
    if (h == INVALID_HANDLE_VALUE)
        return 0;
    DWORD pid = 0;
    PROCESSENTRY32 e;
    e.dwSize = sizeof(e);
    for (BOOL ok = s_p1(h, &e); ok && !pid; ok = s_pn(h, &e))
        if (!_wcsicmp(e.szExeFile, exe))
            pid = e.th32ProcessID;
    Release(h);
    return pid;
}

static void PutBack(const CanWin& c)
{
    for (int i = 0; i < c.n; i++) {
        if (c.vis[i])
            ShowWindow(c.w[i], SW_SHOWNA);
        SetWindowPos(c.w[i], c.ex[i] & WS_EX_TOPMOST ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
}

// Hidden (and not on top) for 5 s: when, if ever, it shows itself again / puts itself on top again.
static void HideTry(const CanWin& c)
{
    for (int i = 0; i < c.n; i++) {
        SetWindowPos(c.w[i], HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        ShowWindow(c.w[i], SW_HIDE);
    }
    int shownAt = -1, topAt = -1;
    for (int ms = 0; ms < 5000; ms += 200) {
        Sleep(200);
        for (int i = 0; i < c.n; i++) {
            if (shownAt < 0 && c.vis[i] && IsWindowVisible(c.w[i]))
                shownAt = ms + 200;
            if (topAt < 0 && (c.ex[i] & WS_EX_TOPMOST) && (GetWindowLong(c.w[i], GWL_EXSTYLE) & WS_EX_TOPMOST))
                topAt = ms + 200;
        }
    }
    PutBack(c);
    Sum(L"CAN-панель спрятана на 5 с: %s; «поверх всех» %s", shownAt < 0 ? L"сама не показалась" : L"показалась сама",
        topAt < 0 ? L"не вернула" : L"вернула сама");
    if (shownAt >= 0 || topAt >= 0)
        Log(L"  показалась через %d мс, поверх всех через %d мс (-1: нет)", shownAt, topAt);
}

static LRESULT CALLBACK CoverProc(HWND w, UINT m, WPARAM wp, LPARAM lp)
{
    if (m == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(w, &ps);
        RECT r;
        GetClientRect(w, &r);
        FillRect(dc, &r, (HBRUSH)GetStockObject(BLACK_BRUSH));
        SetTextColor(dc, RGB(255, 255, 255));
        SetBkMode(dc, TRANSPARENT);
        DrawText(dc, L"VHCheck: наше окно поверх всех (5 с)", -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        EndPaint(w, &ps);
        return 0;
    }
    return DefWindowProc(w, m, wp, lp);
}

// Ours on top of all for 5 s: does a window of the CAN panel come over it.
static void CoverTry(const CanWin& c)
{
    WNDCLASS wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = CoverProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"VHCheck_Cover";
    RegisterClass(&wc);
    HWND me = CreateWindowEx(WS_EX_TOPMOST, L"VHCheck_Cover", L"VHCheck", WS_POPUP | WS_VISIBLE, 0, 0,
                             GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), NULL, NULL, wc.hInstance, NULL);
    if (!me) {
        Sum(L"наше окно поверх всех: не создалось");
        return;
    }
    SetForegroundWindow(me);
    int overAt = -1;
    for (int ms = 0; ms < 5000; ms += 200) {
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
            DispatchMessage(&msg);
        Sleep(200);
        for (HWND w = GetWindow(me, GW_HWNDPREV); w && overAt < 0; w = GetWindow(w, GW_HWNDPREV))
            for (int i = 0; i < c.n; i++)
                if (w == c.w[i] && IsWindowVisible(w))
                    overAt = ms + 200;
    }
    DestroyWindow(me);
    PutBack(c);
    Sum(L"наше окно поверх всех 5 с: CAN-панель %s", overAt < 0 ? L"поверх не вылезла" : L"вылезла поверх");
    if (overAt >= 0)
        Log(L"  вылезла через %d мс", overAt);
}

static void CanPanel()
{
    CanWin c;
    memset(&c, 0, sizeof(c));
    c.pid = PidOf(L"can_panel.exe");
    if (!c.pid) {
        Sum(L"can_panel.exe: не запущен");
        return;
    }
    EnumWindows(CollectCan, (LPARAM)&c);
    int top = 0;
    for (int i = 0; i < c.n; i++)
        top += c.vis[i] && (c.ex[i] & WS_EX_TOPMOST) ? 1 : 0;
    Sum(L"can_panel.exe: окон %d, видимых поверх всех %d", c.n, top);
    if (!c.n || MessageBox(NULL, L"Проверить окно CAN-панели? На 5 с оно будет спрятано, затем на 5 с экран закроет "
                           L"чёрное окно VHCheck. Потом всё вернётся как было.", L"VHCheck",
                           MB_YESNO | MB_TOPMOST) != IDYES) {
        Log(L"  проверка окна CAN-панели пропущена");
        return;
    }
    HideTry(c);
    CoverTry(c);
}

#endif   // (the unit only)

// ---------------------------------------------------------------- the run

#ifdef UNDER_CE
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
{
    if (!LogPlace()) {
        MessageBox(NULL, L"VHCheck пишет журнал на SD-карту или USB-диск - вставьте их.", L"VHCheck", MB_OK | MB_TOPMOST);
        return 1;
    }
    if (MessageBox(NULL, L"VHCheck: сведения о ГУ перед переходом VestaHome на заглушку - около минуты. Ничего не "
                   L"меняется (кроме проверки окна CAN-панели - о ней спрошу отдельно). Начать?", L"VHCheck",
                   MB_YESNO | MB_TOPMOST) != IDYES)
        return 0;
    OSVERSIONINFO v;
    v.dwOSVersionInfoSize = sizeof(v);
    GetVersionEx(&v);
    MEMORYSTATUS ms;
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatus(&ms);
    Log(L"=== VHCheck 1.4: ОС %u.%u.%u, память свободно %u из %u КБ", v.dwMajorVersion, v.dwMinorVersion, v.dwBuildNumber,
        ms.dwAvailPhys / 1024, ms.dwTotalPhys / 1024);
    bool th = Toolhelp();
    Firmware();
    Files();
    Functions();
    Registry();
    if (th)
        Processes();
    else
        Sum(L"toolhelp.dll нет: процессы не видны");
    Windows(L"в начале");
    Ports();
    WriteTry();
    Mcu();
    Windows(L"через ~40 с");
    if (th)
        CanPanel();
    Log(L"=== конец");
    wchar_t end[1700];
    _snwprintf(end, 1700, L"%s\r\nЖурнал: %s - пришлите его.", s_sum, s_log);
    end[1699] = 0;
    MessageBox(NULL, end, L"VHCheck", MB_OK | MB_TOPMOST);
    ExitProcess(0);   // (the MCU client: no Uninit - our PIC1: node goes with the process)
    return 0;
}
#else
int wmain(int argc, wchar_t** argv)   // the PC: VHCheckPC.exe <folders of an unpacked firmware>
{
    if (argc < 2) {
        printf("VHCheckPC.exe <folder> [folder...]: a firmware unpacked there (its files by name)\n");
        return 1;
    }
    s_roots = argv + 1;
    s_nroots = argc - 1;
    LogPlace();
    Log(L"=== VHCheckPC 1.0: %d папок", s_nroots);
    Firmware();
    Files();
    Functions();
    Log(L"=== конец");
    return 0;
}
#endif
