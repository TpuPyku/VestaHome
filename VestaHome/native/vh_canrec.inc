// vh_canrec.inc - part of home.cpp (included there, in order; branch canrec): the CAN panel's COM3 recorded raw
// (canrec.h). Diagnostics -> "COM3 панели": the panel ended, COM3 read kCanRecMs into VestaLogs\com3_*.bin, the
// panel started again from \ResidentFlash\CAN; then can.log (its NMEA on COM4) for kCanRecLogMs to compare.

#define WM_APP_CANREC (WM_APP + 40)   // wParam 1 recorded, 0 failed; lParam the bytes
enum { TIMER_CANREC = 40, TIMER_CANRECUI = 41, kCanRecMs = 180000, kCanRecLogMs = 120000 };

static volatile LONG s_canRecOn;      // the thread runs
static volatile LONG s_canRecStop;    // "stop" pressed
static volatile LONG s_canRecBytes;   // read so far
static DWORD         s_canRecStart;
static DWORD         s_canRecPid;     // the panel's process, 0: it did not run
static bool          s_canRecBack;    // the panel started after: it ran, or this boot left it off (the canrec job)
static wchar_t       s_canRecPath[MAX_PATH];
static wchar_t       s_canRecLast[64];   // the last run's result, for the button's line

static const wchar_t kCanPanelExe[] = L"\\ResidentFlash\\CAN\\can_panel.exe";   // (Init.exe starts it from there)

static HANDLE CanRecOpenCom3()
{
    for (int i = 0; i < 10; i++) {   // the ended panel's handle goes a little later
        HANDLE h = CreateFile(L"COM3:", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (h != INVALID_HANDLE_VALUE) {
            DCB dcb;
            memset(&dcb, 0, sizeof(dcb));
            dcb.DCBlength = sizeof(dcb);
            GetCommState(h, &dcb);
            dcb.BaudRate = 115200;   // as the panel (PROJECT.md: 115200 8N1)
            dcb.ByteSize = 8;
            dcb.Parity = NOPARITY;
            dcb.StopBits = ONESTOPBIT;
            dcb.fBinary = TRUE;
            dcb.fOutxCtsFlow = dcb.fOutxDsrFlow = FALSE;
            dcb.fDtrControl = DTR_CONTROL_ENABLE;
            dcb.fRtsControl = RTS_CONTROL_ENABLE;
            SetCommState(h, &dcb);
            COMMTIMEOUTS to = { 20, 0, 200, 0, 0 };
            SetCommTimeouts(h, &to);
            return h;
        }
        Sleep(300);
    }
    return INVALID_HANDLE_VALUE;
}

static void CanRecPanelBack()
{
    if (!s_canRecBack)
        return;
    PROCESS_INFORMATION pi;
    if (CreateProcess(kCanPanelExe, NULL, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        VLog(L"COM3 панели: панель запущена снова");
    } else {
        VLog(L"COM3 панели: панель не запустилась, ошибка %lu - перезагрузите ГУ", GetLastError());
    }
}

// The panel ended and gone (its handle signalled within 3 s). The shell and VestaHome itself answer
// TerminateProcess 170 while their threads sit in drivers' calls (PROJECT.md) - the panel may too: then false,
// it keeps COM3 and runs on, so it is not started a second time.
static bool CanRecEndPanel()
{
    HANDLE h = OpenProcess(0, FALSE, s_canRecPid);
    if (!h) {
        VLog(L"COM3 панели: процесс панели не открыть, ошибка %lu", GetLastError());
        return false;
    }
    BOOL ok = TerminateProcess(h, 1);
    DWORD err = ok ? 0 : GetLastError();
    bool gone = WaitForSingleObject(h, 3000) == WAIT_OBJECT_0;
    CloseHandle(h);
    VLog(L"COM3 панели: TerminateProcess %s (ошибка %lu), процесс %s", ok ? L"да" : L"нет", err,
         gone ? L"завершился" : L"остался");
    return gone;
}

static DWORD WINAPI CanRecThread(LPVOID)
{
    if (s_canRecPid && !CanRecEndPanel()) {
        PostMessage(s_wnd, WM_APP_CANREC, 2, 0);   // the panel runs on, COM3 is its: the boot job instead
        InterlockedExchange((LONG*)&s_canRecOn, 0);
        return 0;
    }
    HANDLE com = CanRecOpenCom3();
    HANDLE f = com == INVALID_HANDLE_VALUE ? INVALID_HANDLE_VALUE
               : CreateFile(s_canRecPath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    bool ok = f != INVALID_HANDLE_VALUE;
    if (com == INVALID_HANDLE_VALUE)
        VLog(L"COM3 панели: COM3 не открылся, ошибка %lu", GetLastError());
    else if (!ok)
        VLog(L"COM3 панели: файл не создаётся, ошибка %lu", GetLastError());
    static unsigned char buf[512], out[6 + 512];
    while (ok && !s_canRecStop && GetTickCount() - s_canRecStart < kCanRecMs) {
        DWORD got = 0, put;
        if (!ReadFile(com, buf, sizeof(buf), &got, NULL))
            Sleep(50);
        int n = CanRecPack(out, GetTickCount() - s_canRecStart, buf, (int)got);
        if (n && WriteFile(f, out, n, &put, NULL))
            InterlockedExchangeAdd((LONG*)&s_canRecBytes, (LONG)got);
    }
    if (f != INVALID_HANDLE_VALUE)
        CloseHandle(f);
    if (com != INVALID_HANDLE_VALUE)
        CloseHandle(com);
    CanRecPanelBack();
    PostMessage(s_wnd, WM_APP_CANREC, ok ? 1 : 0, s_canRecBytes);
    InterlockedExchange((LONG*)&s_canRecOn, 0);
    return 0;
}

// boot: this boot is the canrec job's (the panel off, nothing to end, started after)
static void CanRecBegin(bool boot)
{
    wchar_t dir[MAX_PATH], name[40];
    if (!LogDir(dir, s_dir)) {   // records go only to the SD card / USB disk
        wcscpy(s_canRecLast, L"нет SD-карты или USB-диска");
        return;
    }
    static TaskInfo s_crTask;
    static wchar_t s_names[TASK_PROC_MAX][40];
    TaskSample(&s_crTask, s_wnd);
    int n = s_crTask.procOk ? s_crTask.procCount : 0;
    for (int i = 0; i < n; i++)
        wcscpy(s_names[i], s_crTask.proc[i].name);
    int at = CanRecPanelAt(s_names, n);
    s_canRecPid = at >= 0 ? s_crTask.proc[at].pid : 0;
    s_canRecBack = s_canRecPid || boot;
    SYSTEMTIME t;
    GetLocalTime(&t);
    CanRecName(name, 40, &t);
    _snwprintf(s_canRecPath, MAX_PATH, L"%s%s", dir, name);
    s_canRecPath[MAX_PATH - 1] = 0;
    s_canRecStop = 0;
    s_canRecBytes = 0;
    s_canRecStart = GetTickCount();
    s_canRecOn = 1;
    VLog(L"COM3 панели: запись в %s, панель %s", s_canRecPath, s_canRecPid ? L"останавливается"
         : boot ? L"не запущена (задание загрузчика)" : L"не запущена");
    HANDLE h = CreateThread(NULL, 0, CanRecThread, NULL, 0, NULL);
    if (h)
        CloseHandle(h);
    else
        s_canRecOn = 0;
    SetTimer(s_wnd, TIMER_CANRECUI, 1000, NULL);   // the button's seconds (COM4 is quiet meanwhile: no WM_APP_GPS)
}

static void CanRecToggle()   // the diagnostics' button
{
    if (s_canRecOn)
        InterlockedExchange((LONG*)&s_canRecStop, 1);
    else
        CanRecBegin(false);
}

// The panel did not end (TerminateProcess 170, as the shell's): the installer's boot job "canrec" - the next boot
// leaves the panel off (our SwDnl.exe instead of Init.exe) and marks it; VestaHome records then (CanRecAtStart).
// Only an installer signed by the author is run (VestaCore UpdateCheck, as the update from "О системе").
static void CanRecArm()
{
    static const wchar_t* const dirs[2] = { L"\\SDMemory\\VestaHomeInstall\\", L"\\USBDevice\\VestaHomeInstall\\" };
    for (int i = 0; i < 2; i++) {
        wchar_t exe[MAX_PATH], sig[MAX_PATH], args[] = L"boot canrec";
        char ver[32];
        unsigned char sha[32];
        _snwprintf(exe, MAX_PATH, L"%sVestaHomeInstall.exe", dirs[i]);
        _snwprintf(sig, MAX_PATH, L"%sVestaHomeInstall.sig", dirs[i]);
        if (GetFileAttributes(exe) == 0xFFFFFFFF)
            continue;
        int r = UpdateCheck(exe, sig, NULL, 0, ver, 32, sha);
        if (r != UPDATE_OK) {
            VLog(L"COM3 панели: установщик %s не проверен (%d) - задание не ставится", exe, r);
            continue;
        }
        PROCESS_INFORMATION pi;
        if (CreateProcess(exe, args, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi)) {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            wcscpy(s_canRecLast, L"панель не завершилась - перезагрузите ГУ");
            VLog(L"COM3 панели: панель не завершилась - задание загрузчика (%s boot canrec), запись после перезагрузки", exe);
            return;
        }
    }
    wcscpy(s_canRecLast, L"нет установщика VestaHomeInstall на SD");
    VLog(L"COM3 панели: панель не завершилась, установщика с подписью на SD/USB нет - задание не поставлено");
}

// At the start: this boot left the panel off for us (the installer's canrec job wrote canrec.now this boot) - the
// recording, then the panel. The mark is this boot's only: an old one would race Init.exe's panel.
static void CanRecAtStart()
{
    static const wchar_t kMark[] = L"\\SDMemory\\SPIPower_boot\\canrec.now";
    if (GetFileAttributes(kMark) == 0xFFFFFFFF)
        return;
    DeleteFile(kMark);
    CanRecBegin(true);
}

// VestaHome closing while it records: the thread told to stop and waited for - the panel must come back
static void CanRecQuit()
{
    if (!s_canRecOn)
        return;
    InterlockedExchange((LONG*)&s_canRecStop, 1);
    for (int i = 0; i < 40 && s_canRecOn; i++)
        Sleep(100);
}

// The recording done: can.log (the panel's NMEA on COM4) for kCanRecLogMs, unless it is recorded already
static void CanRecDone(int how, DWORD bytes)   // how: 1 recorded, 0 failed, 2 the panel did not end
{
    if (how == 2) {
        CanRecArm();
        return;
    }
    bool ok = how == 1;
    _snwprintf(s_canRecLast, 64, ok ? L"записано %u КБ" : L"не записано (см. журнал)", (unsigned)(bytes / 1024));
    VLog(L"COM3 панели: %s", s_canRecLast);
    if (ok && s_canRecBack && s_canLog == INVALID_HANDLE_VALUE) {
        CanLogToggle();
        SetTimer(s_wnd, TIMER_CANREC, kCanRecLogMs, NULL);
    }
}

static void CanRecLogEnd()   // TIMER_CANREC
{
    KillTimer(s_wnd, TIMER_CANREC);
    if (s_canLog != INVALID_HANDLE_VALUE)
        CanLogToggle();
}

static void CanRecLabel(wchar_t* s, int n, bool* busy)   // the diagnostics' button
{
    *busy = s_canRecOn != 0;
    if (s_canRecOn) {
        DWORD left = (kCanRecMs - (GetTickCount() - s_canRecStart)) / 1000;
        _snwprintf(s, n, L"COM3: %u КБ · %u:%02u · стоп", (unsigned)(s_canRecBytes / 1024), (unsigned)(left / 60),
                   (unsigned)(left % 60));
    } else if (s_canLog != INVALID_HANDLE_VALUE && s_canRecLast[0]) {
        _snwprintf(s, n, L"COM3 есть · пишется can.log");
    } else {
        _snwprintf(s, n, s_canRecLast[0] ? L"COM3 панели: %s" : L"COM3 панели: записать 3 мин", s_canRecLast);
    }
    s[n - 1] = 0;
}
