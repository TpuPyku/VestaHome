// SPIPowerInstall - safe swap/watchdog/rollback for the SPIPowerMgr proxy on the
// Lada Vesta MMC (Windows CE 6.0, ARM). Follows the reversible-install pattern of
// VestaInstall (toolhelp KillProcess, .bak-style backups, append-only log).
//
// Usage: SPIPowerInstall.exe status | install [p2|p3] | rollback | drill | drill-off
//   status        -> report current state to \SDMMC\SPIPower_install.log + a message box
//   install       -> backup the factory DLL, swap in the proxy, start the shell and
//                    watch it (pure pass-through, no flag)
//   install p2    -> same, but create \SDMMC\SPIPower.on (outbound logging)
//   install p3    -> same, flag on (inbound vtable wrap is also gated by the flag)
//   rollback      -> kill the shell, restore the factory DLL, delete the flag, restart
//   drill         -> arm the factory boot updater SwDnl.exe with the harmless marker
//                    package: stage SwDnl_drill.upg + SwDnl.cfg on the SD card and
//                    write one byte 0x02 into \ResidentFlash\SwUpdate.flag; reboot the
//                    HU, then read the verdict with "status"/"drill-off" (../SWDNL.md)
//   drill-off     -> disarm: delete \ResidentFlash\SwUpdate.flag, remove the staged
//                    drill files from the SD and report the drill verdict
//
// The proxy DLL is taken from this exe's own folder, so the whole SPIPower_install
// folder can live on the SD card as well as on \SDMMC. The factory DLL is preserved
// in two places: SPIPowerMgr_orig.dll (app dir, survives reboot) and
// <SD>\SPIPower_backup\SPIPowerMgr.dll (real SD when present, else \SDMMC).
//
// Mount points (ROM registry, MAIN/4120_unpacked/TINYNK/Registry/default.reg):
//   \SDMemory = StorageManager profile "SDMemory" ("SD Memory Card") -> the SD card
//   \SDMMC    = profiles "MMC"/"MMC2" ("EMMC Disk Drive")            -> internal eMMC
// SwDnl.exe and Main.exe hardcode \SDMemory, so the drill resolves the card at runtime.
#include <windows.h>
#include <string.h>
#include <stdio.h>
#include <tlhelp32.h>

static const wchar_t* kAppDir    = L"\\ResidentFlash\\navigation\\app";
static const wchar_t* kDll       = L"SPIPowerMgr.dll";
static const wchar_t* kOrig      = L"SPIPowerMgr_orig.dll";
static const wchar_t* kBackupSub = L"SPIPower_backup";
static const wchar_t* kFlag      = L"\\SDMMC\\SPIPower.on";     // written/read by the proxy
static const wchar_t* kLog       = L"\\SDMMC\\SPIPower.log";    // written by the proxy
static const wchar_t* kShell     = L"G3NavHMI.exe";
static const wchar_t* kShellPath = L"\\ResidentFlash\\navigation\\app\\G3NavHMI.exe";

// drill / factory boot updater (SwDnl.exe) - see ../SWDNL.md
static const wchar_t* kSwFlag    = L"\\ResidentFlash\\SwUpdate.flag";
static const wchar_t* kDrillUpg  = L"SwDnl_drill.upg";
static const wchar_t* kDrillCfg  = L"SwDnl.cfg";
static const wchar_t* kMarker    = L"\\ResidentFlash\\navigation\\app\\VestaDrill.txt";
static const char*    kDrillTag  = "VestaDrill";   // file name inside the drill package
static const wchar_t* kDanger[]  = { L"update.pkg", L"RdDnl.upg", L"CANUpdate.s19" };
static const int      kDangerN   = 3;

// the SD card is \SDMemory (profile "SDMemory" = "SD Memory Card"); \SDMMC is the
// internal eMMC (profiles "MMC"/"MMC2" = "EMMC Disk Drive")
static const wchar_t* kSdCand[]  = { L"SDMemory", L"SDMMC" };
static const int      kSdCandN   = 2;

static wchar_t g_dll[MAX_PATH];
static wchar_t g_orig[MAX_PATH];
static wchar_t g_ownDir[MAX_PATH];
static wchar_t g_proxySrc[MAX_PATH];
static wchar_t g_backupDir[MAX_PATH];
static wchar_t g_backup[MAX_PATH];
static wchar_t g_sd[MAX_PATH];      // "\SDMemory" / "\SDMMC", empty = no card
static wchar_t g_sdName[32];        // bare mount name for the cfg ("SDMemory")
static wchar_t g_status[512];

static void Join(wchar_t* out, const wchar_t* a, const wchar_t* b)
{
    int la = (int)wcslen(a);
    _snwprintf(out, MAX_PATH, L"%s%s%s", a, (la && a[la - 1] == L'\\') ? L"" : L"\\", b);
    out[MAX_PATH - 1] = 0;
}

static int FileExists(const wchar_t* p);   // defined below
static const wchar_t* SdRoot();            // defined below

static void SetPaths()
{
    Join(g_dll,  kAppDir, kDll);
    Join(g_orig, kAppDir, kOrig);

    // the proxy lives next to this exe (SD card or \SDMMC - wherever the user dropped
    // the SPIPower_install folder), not in a hardcoded place
    wchar_t exe[MAX_PATH];
    if (GetModuleFileName(NULL, exe, MAX_PATH)) {
        exe[MAX_PATH - 1] = 0;
        for (int i = (int)wcslen(exe) - 1; i >= 0; i--) {
            if (exe[i] == L'\\') { exe[i] = 0; break; }
        }
        wcscpy(g_ownDir, exe);
    } else {
        wcscpy(g_ownDir, L"\\SDMMC\\SPIPower_install");
    }
    Join(g_proxySrc, g_ownDir, kDll);

    // the second backup copy goes to the real SD card when one is mounted
    const wchar_t* sd = SdRoot();
    wchar_t dir[MAX_PATH];
    Join(dir, sd ? sd : L"\\SDMMC", kBackupSub);
    wcscpy(g_backupDir, dir);
    Join(g_backup, g_backupDir, kDll);
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

// ---------------------------------------------------------------- mount points
// SwDnl.exe reads its cfg from the hardcoded \SDMemory\SwDnl.cfg, so the drill has
// to stage its files on the real card; resolve it at runtime and only fall back to
// \SDMMC (eMMC) when no card is mounted.
static int IsWritableDir(const wchar_t* dir)
{
    wchar_t probe[MAX_PATH];
    Join(probe, dir, L"spipower_w.tmp");
    HANDLE h = CreateFile(probe, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    DWORD w = 0;
    int ok = WriteFile(h, "x", 1, &w, NULL) && w == 1;
    CloseHandle(h);
    DeleteFile(probe);
    return ok;
}

static const wchar_t* SdRoot()
{
    if (g_sd[0]) return g_sd;
    for (int i = 0; i < kSdCandN; i++) {
        wchar_t d[MAX_PATH];
        _snwprintf(d, MAX_PATH, L"\\%s", kSdCand[i]);
        d[MAX_PATH - 1] = 0;
        if (FileExists(d) && IsWritableDir(d)) {
            wcscpy(g_sd, d);
            wcsncpy(g_sdName, kSdCand[i], 31);
            g_sdName[31] = 0;
            return g_sd;
        }
    }
    return NULL;
}

// ---------------------------------------------------------------- SwUpdate.flag
// One byte, bitmask of the pending update types (1 Radio, 2 Navi, 4 CAN, 8 OS);
// Main.exe sets it and SwDnl.exe clears one bit per finished stage (../SWDNL.md).
static int FlagByte()   // -1 = no flag file
{
    HANDLE h = CreateFile(kSwFlag, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                          NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return -1;
    unsigned char b = 0;
    DWORD got = 0;
    int ok = ReadFile(h, &b, 1, &got, NULL) && got == 1;
    CloseHandle(h);
    return ok ? (int)b : -1;
}

static int WriteFlagByte(int v)
{
    HANDLE h = CreateFile(kSwFlag, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                          FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    unsigned char b = (unsigned char)v;
    DWORD w = 0;
    int ok = WriteFile(h, (const char*)&b, 1, &w, NULL) && w == 1;
    FlushFileBuffers(h);
    CloseHandle(h);
    return ok;
}

// SwDnl reads the cfg with _wfopen(path, L"r") + fwscanf(f, L"%s"), i.e. as an ANSI
// stream of whitespace-separated tokens - mirror the factory file layout exactly.
static int WriteAnsiFile(const wchar_t* path, const char* text)
{
    HANDLE h = CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    DWORD len = (DWORD)strlen(text), w = 0;
    int ok = WriteFile(h, text, len, &w, NULL) && w == len;
    FlushFileBuffers(h);
    CloseHandle(h);
    return ok;
}

// Scans the first bytes of a package for the marker name in its TOC - a cheap guard
// against arming the boot updater with something that is not the drill package.
static int FileHasDrillTag(const wchar_t* path, DWORD maxLen)
{
    HANDLE h = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    char buf[4096 + 1];
    DWORD total = 0, got = 0;
    int found = 0;
    while (ReadFile(h, buf, 4096, &got, NULL) && got) {
        buf[got] = 0;
        if (strstr(buf, kDrillTag)) { found = 1; break; }
        total += got;
        if (total >= maxLen) break;
    }
    CloseHandle(h);
    return found;
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
    EnsureDir(g_backupDir);
    if (!FileExists(g_orig)) {
        if (!MyCopyFile(g_dll, g_orig)) {
            wcscpy(g_status, L"Ошибка: не удалось сохранить SPIPowerMgr_orig.dll");
            return 0;
        }
    }
    if (!FileExists(g_backup)) {
        if (!MyCopyFile(g_orig, g_backup)) {
            wchar_t m[512];
            _snwprintf(m, 512, L"Ошибка: не удалось сохранить копию в %s", g_backupDir);
            wcscpy(g_status, m);
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

    if (FlagByte() >= 0) {
        wcscpy(g_status, L"Отказ: заряжен \\ResidentFlash\\SwUpdate.flag (загрузочное обновление) — сначала: SPIPowerInstall.exe drill-off");
        return;
    }
    if (!FileExists(g_dll)) { wcscpy(g_status, L"Ошибка: SPIPowerMgr.dll не найден в app"); return; }
    if (!FileExists(g_proxySrc)) {
        wchar_t m[512];
        _snwprintf(m, 512, L"Ошибка: рядом с exe нет proxy SPIPowerMgr.dll (%s)", g_proxySrc);
        wcscpy(g_status, m);
        return;
    }

    KillProcess(kShell);
    Sleep(2500);
    if (!CanWrite(g_dll)) { wcscpy(g_status, L"Ошибка: SPIPowerMgr.dll занят оболочкой"); return; }

    if (!Backup()) return;

    if (!MyCopyFile(g_proxySrc, g_dll)) { wcscpy(g_status, L"Ошибка: не удалось скопировать proxy на место SPIPowerMgr.dll"); return; }

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
    const wchar_t* sd = SdRoot();
    int fb = FlagByte();
    wchar_t flagTxt[16], sdTxt[MAX_PATH], drillTxt[16];
    if (fb < 0) wcscpy(flagTxt, L"none");
    else _snwprintf(flagTxt, 16, L"0x%02X", fb);
    if (sd) {
        wchar_t p[MAX_PATH];
        int staged = 0;
        Join(p, sd, kDrillCfg); if (FileExists(p)) staged |= 1;
        Join(p, sd, kDrillUpg); if (FileExists(p)) staged |= 2;
        wcscpy(drillTxt, staged == 3 ? L"staged" : (staged ? L"partial" : L"none"));
        wcscpy(sdTxt, sd);
    } else {
        wcscpy(sdTxt, L"none");
        wcscpy(drillTxt, L"-");
    }
    _snwprintf(line, 512, L"dll:%s orig:%s sd-bak:%s flag:%s shell:%s | sd:%s swflag:%s drill:%s marker:%s",
               FileExists(g_dll) ? L"y" : L"n",
               FileExists(g_orig) ? L"y" : L"n",
               FileExists(g_backup) ? L"y" : L"n",
               FileExists(kFlag) ? L"y" : L"n",
               ProcExists(kShell) ? L"up" : L"down",
               sdTxt, flagTxt, drillTxt, FileExists(kMarker) ? L"y" : L"n");
    wcscpy(g_status, line);
}

// ---------------------------------------------------------------- drill
// End-to-end check of the boot-rescue path before the real DLL is ever involved:
// stage SwDnl_drill.upg + SwDnl.cfg on the SD card (SwDnl reads the hardcoded
// \SDMemory\SwDnl.cfg and takes the package named by SWDNL_SRC_NV_FILE) and write
// one byte 0x02 (bit "Navi") into \ResidentFlash\SwUpdate.flag. On the next boot
// Main.exe runs SwDnl.exe instead of G3NavHMI, SwDnl unpacks the package into
// SWDNL_DST_DIR (\ResidentFlash) and clears the bit again.
static void DoDrill()
{
    SetPaths();
    LogMsg(L"--- drill (arm boot updater) ---");

    const wchar_t* sd = SdRoot();
    if (!sd) {
        wcscpy(g_status, L"Ошибка: SD-карта не найдена (\\SDMemory / \\SDMMC) или недоступна для записи");
        return;
    }
    for (int i = 0; i < kDangerN; i++) {
        wchar_t p[MAX_PATH];
        Join(p, sd, kDanger[i]);
        if (FileExists(p)) {
            _snwprintf(g_status, 512,
                L"Отказ: на SD лежит %s (%s) — по нему SwDnl захочет обновлять OS/Radio/CAN. Убрать файл.",
                kDanger[i], p);
            return;
        }
    }

    wchar_t src[MAX_PATH], dstUpg[MAX_PATH], dstCfg[MAX_PATH];
    Join(src,    g_ownDir, kDrillUpg);
    Join(dstUpg, sd,       kDrillUpg);
    Join(dstCfg, sd,       kDrillCfg);

    if (!FileExists(src)) {
        _snwprintf(g_status, 512, L"Ошибка: рядом с exe нет %s (%s) — собрать: python tools\\make_drill.py",
                   kDrillUpg, src);
        return;
    }
    if (!FileHasDrillTag(src, 256 * 1024)) {
        _snwprintf(g_status, 512, L"Отказ: %s не содержит маркер VestaDrill — это не drill-пакет.", kDrillUpg);
        return;
    }
    if (!MyCopyFile(src, dstUpg)) {
        _snwprintf(g_status, 512, L"Ошибка: не удалось скопировать %s на %s", kDrillUpg, sd);
        return;
    }

    // the cfg SwDnl reads: destination \ResidentFlash, Navi source = our package
    wchar_t wcfg[256];
    _snwprintf(wcfg, 256,
               L"SWDNL_DST_DIR\t\tResidentFlash\r\nSWDNL_SRC_NV_FILE\t\"%s\\%s\"\r\n",
               g_sdName, kDrillUpg);
    char cfg[256];
    int n = 0;
    for (; wcfg[n] && n < 255; n++) cfg[n] = (char)(wcfg[n] < 128 ? wcfg[n] : '?');
    cfg[n] = 0;
    if (!WriteAnsiFile(dstCfg, cfg)) {
        _snwprintf(g_status, 512, L"Ошибка: не удалось записать %s на %s", kDrillCfg, sd);
        return;
    }

    if (!WriteFlagByte(0x02)) {
        wcscpy(g_status, L"Ошибка: не удалось записать \\ResidentFlash\\SwUpdate.flag (0x02)");
        return;
    }
    LogMsg(L"drill: armed, \\ResidentFlash\\SwUpdate.flag = 0x02");
    _snwprintf(g_status, 512,
        L"Дрилл заряжен.\nSD: %s\nпакет: %s\ncfg: %s\nфлаг: \\ResidentFlash\\SwUpdate.flag = 0x02\n\n"
        L"Перезагрузите ГУ, оставив SD в слоте. После загрузки: SPIPowerInstall.exe status\n"
        L"Ожидание: маркер \\ResidentFlash\\navigation\\app\\VestaDrill.txt есть, флаг снят.",
        sd, dstUpg, dstCfg);
}

static void DoDrillOff()
{
    SetPaths();
    LogMsg(L"--- drill-off (disarm) ---");

    int flag = FlagByte();
    int marker = FileExists(kMarker);
    if (flag >= 0) DeleteFile(kSwFlag);

    const wchar_t* sd = SdRoot();
    if (sd) {
        wchar_t p[MAX_PATH];
        Join(p, sd, kDrillCfg); if (FileExists(p)) DeleteFile(p);
        Join(p, sd, kDrillUpg); if (FileExists(p)) DeleteFile(p);
    }

    wchar_t flagTxt[16], verdict[192];
    if (flag < 0) wcscpy(flagTxt, L"нет");
    else _snwprintf(flagTxt, 16, L"0x%02X", flag);
    if (flag < 0 && marker)
        wcscpy(verdict, L"ИТОГ: SwDnl отработал — флаг снят, маркер на месте (PASS).");
    else if (flag < 0)
        wcscpy(verdict, L"ИТОГ: флаг снят, но маркера нет — распаковка не дошла до app; смотреть лог SwDnl.");
    else if (flag == 0x02)
        wcscpy(verdict, L"ИТОГ: флаг 0x02 на месте — SwDnl не запускался (проверить SD и Main.exe).");
    else
        wcscpy(verdict, L"ИТОГ: флаг не 0x02 — разбирать по debug-логу SwDnl (NKDbgPrintfW).");

    _snwprintf(g_status, 512, L"drill-off: флаг был %s, маркер %s; с SD убраны %s/%s.\n%s",
               flagTxt, marker ? L"есть" : L"нет", kDrillCfg, kDrillUpg, verdict);
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

    if (wcsstr(a, L"drill-off") || wcsstr(a, L"drill off")) {
        DoDrillOff();
    } else if (wcsstr(a, L"drill")) {
        DoDrill();
    } else if (wcsstr(a, L"rollback")) {
        Rollback();
        wcscpy(g_status, L"Откат выполнен: оригинал восстановлен, оболочка перезапущена.");
    } else if (wcsstr(a, L"status")) {
        DoStatus();
    } else if (wcsstr(a, L"install")) {
        DoInstall(logging);
    } else {
        wcscpy(g_status, L"Способ: SPIPowerInstall.exe status | install [p2|p3] | rollback | drill | drill-off");
    }

    LogMsg(g_status);
    MessageBox(NULL, g_status, L"SPIPower Install", MB_OK | MB_ICONINFORMATION);
    return 0;
}
