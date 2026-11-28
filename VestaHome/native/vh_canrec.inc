// vh_canrec.inc - part of home.cpp (included there, in order; branch canrec): the CAN panel's COM3 recorded raw
// (canrec.h). Diagnostics -> "COM3 панели": the panel ended, COM3 read kCanRecMs into VestaLogs\com3_*.bin, the
// panel started again from \ResidentFlash\CAN; then can.log (its NMEA on COM4) for kCanRecLogMs to compare.

#define WM_APP_CANREC (WM_APP + 40)   // wParam 1 recorded, 0 failed; lParam the bytes
enum { TIMER_CANREC = 40, TIMER_CANRECUI = 41, kCanRecLogMs = 120000 };   // (kCanRecMs: canrec.h)

static volatile LONG s_canRecOn;      // the thread records
static volatile LONG s_canThread;     // the COM3 thread runs: the recording, or the bridge alone (canrec.h)
static volatile LONG s_canRecStop;    // "stop" pressed
static volatile LONG s_canRecBytes;   // read so far
static DWORD         s_canRecStart;
static DWORD         s_canRecPid;     // the panel's process, 0: it did not run
static bool          s_canRecBack;    // the panel started after: it ran, or this boot left it off (the canrec job)
static wchar_t       s_canRecPath[MAX_PATH];
static wchar_t       s_canRecLast[64];   // the last run's result, for the button's line
// the decoded values (VestaCore vcan3) while it records: the thread writes, the screen reads a copy (the user
// 2026-11-27: seen live); the marks pressed meanwhile into <recording>_marks.txt
static CRITICAL_SECTION s_canLiveCs;
static VCan3            s_canLive;
static wchar_t          s_canMarkPath[MAX_PATH];
static wchar_t          s_canMarkLast[48];   // the last mark, for the screen
static int              s_canRecMarkN;
static int              s_canRecStep;          // the step on the screen (canrec.h CanRecSteps)

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

// The bridge's ports (canrec.h CanBridgeWanted): the panel's own - VSP4 (COM4: RMC, @ANG, "@,"), VSP5 / COM7 (the
// navigators: RMC, GGA); a port that does not open is left out
static const wchar_t* const kCanOuts[3] = { L"VSP4:", L"VSP5:", L"COM7:" };

static void CanBridgeOpen(HANDLE* h)
{
    wchar_t got[48] = L"";
    for (int i = 0; i < 3; i++) {
#ifdef UNDER_CE
        h[i] = CreateFile(kCanOuts[i], GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
#else
        h[i] = INVALID_HANDLE_VALUE;   // (the PC: the unit's ports only)
#endif
        if (h[i] != INVALID_HANDLE_VALUE)
            wcscat(got, kCanOuts[i]);
    }
    VLog(L"CAN без панели: GPS и руль из COM3 в %s", got[0] ? got : L"(ни один порт не открылся)");
}

static void CanPut(HANDLE h, const char* s)
{
    DWORD put;
    if (h != INVALID_HANDLE_VALUE)
        WriteFile(h, s, (DWORD)strlen(s), &put, NULL);
}

static void CanBridgeWrite(const HANDLE* h, const VCan3* c, CanOut* o)
{
    char s[128];
    int due = CanBridgeDue(c, o, GetTickCount());
    if (due & CBO_POS) {
        if (VCan3Rmc(c, s, 128))
            for (int i = 0; i < 3; i++)
                CanPut(h[i], s);
        if (VCan3Gga(c, s, 128)) {
            CanPut(h[1], s);
            CanPut(h[2], s);
        }
        if (VCan3Car(c, s, 128))
            CanPut(h[0], s);
    }
    if ((due & CBO_ANG) && VCan3Ang(c, s, 128))
        CanPut(h[0], s);
}

// arg non-NULL: the recording (the panel ended first, the file, the panel back after), NULL: the bridge alone
static DWORD WINAPI CanRecThread(LPVOID arg)
{
    bool rec = arg != NULL;
    if (rec && s_canRecPid && !CanRecEndPanel()) {
        PostMessage(s_wnd, WM_APP_CANREC, 2, 0);   // the panel runs on, COM3 is its: the boot job instead
        InterlockedExchange((LONG*)&s_canRecOn, 0);
        InterlockedExchange((LONG*)&s_canThread, 0);
        return 0;
    }
    HANDLE com = CanRecOpenCom3();
#ifndef UNDER_CE   // the PC: a recording from the unit played back at its pace (com3_replay.bin beside the exe)
    wchar_t rp[MAX_PATH];
    _snwprintf(rp, MAX_PATH, L"%scom3_replay.bin", s_dir);
    rp[MAX_PATH - 1] = 0;
    HANDLE rf = CreateFile(rp, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    bool replay = rf != INVALID_HANDLE_VALUE;   // (it wins over a COM3 the PC may have)
    if (replay) {
        if (com != INVALID_HANDLE_VALUE)
            CloseHandle(com);
        com = rf;
    }
#endif
    HANDLE f = com == INVALID_HANDLE_VALUE || !rec ? INVALID_HANDLE_VALUE
               : CreateFile(s_canRecPath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    bool ok = rec ? f != INVALID_HANDLE_VALUE : com != INVALID_HANDLE_VALUE;
    if (com == INVALID_HANDLE_VALUE)
        VLog(L"COM3 панели: COM3 не открылся, ошибка %lu", GetLastError());
    else if (!ok)
        VLog(L"COM3 панели: файл не создаётся, ошибка %lu", GetLastError());
    static unsigned char buf[512], out[6 + 512];
    static VCan3 dec;
    VCan3Init(&dec);
    HANDLE outs[3];
    CanOut sent;
    memset(&sent, 0, sizeof(sent));
    if (ok)
        CanBridgeOpen(outs);   // (while it records too: the navigators keep their GPS)
    else
        outs[0] = outs[1] = outs[2] = INVALID_HANDLE_VALUE;
    while (ok && !s_canRecStop && (!rec || GetTickCount() - s_canRecStart < kCanRecMs)) {
        DWORD got = 0, put;
#ifndef UNDER_CE
        if (replay) {   // one chunk [ms u32][len u16][bytes], given when its time comes
            unsigned char h[6];
            DWORD r = 0;
            if (!ReadFile(com, h, 6, &r, NULL) || r != 6)
                break;
            DWORD at = h[0] | h[1] << 8 | h[2] << 16 | (DWORD)h[3] << 24, len = h[4] | h[5] << 8;
            while (GetTickCount() - s_canRecStart < at && !s_canRecStop)
                Sleep(10);
            ReadFile(com, buf, len < sizeof(buf) ? len : sizeof(buf), &got, NULL);
        } else
#endif
        if (!ReadFile(com, buf, sizeof(buf), &got, NULL))
            Sleep(50);
        int n = rec ? CanRecPack(out, GetTickCount() - s_canRecStart, buf, (int)got) : 0;
        if (n && WriteFile(f, out, n, &put, NULL))
            InterlockedExchangeAdd((LONG*)&s_canRecBytes, (LONG)got);
        if (VCan3Feed(&dec, buf, (int)got)) {   // (one writer: this thread; the screen takes a copy)
            EnterCriticalSection(&s_canLiveCs);
            s_canLive = dec;
            LeaveCriticalSection(&s_canLiveCs);
            CanBridgeWrite(outs, &dec, &sent);
        }
    }
    for (int i = 0; i < 3; i++)
        if (outs[i] != INVALID_HANDLE_VALUE)
            CloseHandle(outs[i]);
    if (f != INVALID_HANDLE_VALUE)
        CloseHandle(f);
    if (com != INVALID_HANDLE_VALUE)
        CloseHandle(com);
    if (rec) {
        CanRecPanelBack();
        PostMessage(s_wnd, WM_APP_CANREC, ok ? 1 : 0, s_canRecBytes);
        InterlockedExchange((LONG*)&s_canRecOn, 0);
    } else {
        VLog(L"CAN без панели: остановлен, кадров %lu", dec.frames);
    }
    InterlockedExchange((LONG*)&s_canThread, 0);
    return 0;
}

// COM3 let go: the bridge (or the recording) told to stop and waited for (3 s at most)
static void CanThreadStop()
{
    if (!s_canThread)
        return;
    InterlockedExchange((LONG*)&s_canRecStop, 1);
    for (int i = 0; i < 30 && s_canThread; i++)
        Sleep(100);
}

static DWORD CanPanelPid()   // the panel's process, 0: not running
{
    static TaskInfo s_crTask;
    static wchar_t s_names[TASK_PROC_MAX][40];
    TaskSample(&s_crTask, s_wnd);
    int n = s_crTask.procOk ? s_crTask.procCount : 0;
    for (int i = 0; i < n; i++)
        wcscpy(s_names[i], s_crTask.proc[i].name);
    int at = CanRecPanelAt(s_names, n);
    return at >= 0 ? s_crTask.proc[at].pid : 0;
}

static void CanCsMake()
{
    static bool s_csMade;
    if (!s_csMade) {
        InitializeCriticalSection(&s_canLiveCs);
        s_csMade = true;
    }
}

// Each second (the main tick), every 10 s in fact: the bridge started when the panel is off, stopped when it is back
// (COM3 is its then - it may have missed it already: a reboot gives it back for sure)
static void CanBridgeTick(DWORD now)
{
    static DWORD s_last, s_up;
    static int s_exe = -1;
    if (!s_up)
        s_up = now;
    if (now - s_last < 10000 || s_canRecOn)
        return;
    s_last = now;
    if (s_exe < 0)
        s_exe = GetFileAttributes(kCanPanelExe) != 0xFFFFFFFF;
    if (!s_exe)
        return;
    bool runs = CanPanelPid() != 0;
    if (runs && s_canThread) {
        VLog(L"CAN без панели: панель запущена - COM3 отдаю ей");
        CanThreadStop();
        return;
    }
    if (!CanBridgeWanted(true, runs, now - s_up, s_canThread != 0))
        return;
    CanCsMake();
    s_canRecStop = 0;
    s_canThread = 1;
    HANDLE h = CreateThread(NULL, 0, CanRecThread, NULL, 0, NULL);
    if (h)
        CloseHandle(h);
    else
        s_canThread = 0;
}

// boot: this boot is the canrec job's (the panel off, nothing to end, started after)
static void CanRecBegin(bool boot)
{
    wchar_t dir[MAX_PATH], name[40];
    if (!LogDir(dir, s_dir)) {   // records go only to the SD card / USB disk
        wcscpy(s_canRecLast, L"нет SD-карты или USB-диска");
        return;
    }
    CanThreadStop();   // the bridge, if it reads COM3 (the recording writes its lines too)
    s_canRecPid = CanPanelPid();
    s_canRecBack = s_canRecPid || boot;
    SYSTEMTIME t;
    GetLocalTime(&t);
    CanRecName(name, 40, &t);
    _snwprintf(s_canRecPath, MAX_PATH, L"%s%s", dir, name);
    s_canRecPath[MAX_PATH - 1] = 0;
    s_canRecStop = 0;
    s_canRecBytes = 0;
    CanRecMarkPath(s_canMarkPath, MAX_PATH, s_canRecPath);
    s_canMarkLast[0] = 0;
    s_canRecMarkN = 0;
    s_canRecStep = 0;
    CanCsMake();
    EnterCriticalSection(&s_canLiveCs);
    VCan3Init(&s_canLive);
    LeaveCriticalSection(&s_canLiveCs);
    s_canRecStart = GetTickCount();
    s_canRecOn = 1;
    s_canThread = 1;
    VLog(L"COM3 панели: запись в %s, панель %s", s_canRecPath, s_canRecPid ? L"останавливается"
         : boot ? L"не запущена (задание загрузчика)" : L"не запущена");
    HANDLE h = CreateThread(NULL, 0, CanRecThread, (LPVOID)1, 0, NULL);
    if (h)
        CloseHandle(h);
    else
        s_canRecOn = s_canThread = 0;
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
    // (its folder from Tests, or the card's root - the user 2026-11-27: it lay in the root, "нет установщика")
    static const wchar_t* const dirs[4] = { L"\\SDMemory\\VestaHomeInstall\\", L"\\SDMemory\\",
                                            L"\\USBDevice\\VestaHomeInstall\\", L"\\USBDevice\\" };
    bool unsigned_ = false;
    for (int i = 0; i < 4; i++) {
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
            unsigned_ = true;
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
    wcscpy(s_canRecLast, unsigned_ ? L"VestaHomeInstall.exe: подпись (.sig) не прошла"   // (64 at most: s_canRecLast)
                                   : L"нет VestaHomeInstall.exe на SD или USB");
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
    CanThreadStop();   // (the bridge too: COM3 let go)
    for (int i = 0; i < 10 && s_canThread; i++)
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
        _snwprintf(s, n, s_canRecLast[0] ? L"COM3 панели: %s" : L"COM3 панели: записать (до 30 мин)", s_canRecLast);
    }
    s[n - 1] = 0;
}

// A step's button while recording: "Готово" / "Пропустить" append its mark (opened, flushed and closed each time - a
// reset keeps it) and go on, "Назад" goes back
static void CanRecMark(int button)
{
    if (!s_canRecOn || s_canRecStep >= CRS_COUNT && button != CRB_BACK)
        return;
    if (CanRecStepMarks(button)) {
        DWORD ms = GetTickCount() - s_canRecStart;
        char line[48];
        CanRecMarkLine(line, 48, ms, s_canRecStep, button == CRB_SKIP);
        HANDLE f = CreateFile(s_canMarkPath, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        DWORD put = 0;
        bool ok = f != INVALID_HANDLE_VALUE && SetFilePointer(f, 0, NULL, FILE_END) != 0xFFFFFFFF &&
                  WriteFile(f, line, (DWORD)strlen(line), &put, NULL) && FlushFileBuffers(f);
        if (f != INVALID_HANDLE_VALUE)
            CloseHandle(f);
        s_canRecMarkN += ok ? 1 : 0;
        _snwprintf(s_canMarkLast, 48, ok ? L"шаг %d %s на %u:%02u" : L"шаг %d: метка не записана", s_canRecStep + 1,
                   button == CRB_SKIP ? L"пропущен" : L"отмечен", (unsigned)(ms / 60000), (unsigned)(ms / 1000 % 60));
        s_canMarkLast[47] = 0;
    }
    s_canRecStep = CanRecStepAfter(s_canRecStep, button);
}

// The check picture's colours (canrec.h CanPart): open / on - its colour, closed / off - grey, not come yet - dim
static COLORREF CanPartColor(int part, COLORREF on)
{
    return part == CNP_ON ? on : part == CNP_OFF ? kMuted : kOff;
}

// A door: closed - a strip along the body's side, open - swung out (side -1 the left, +1 the right)
static void CanDoor(HDC dc, int x, int y, int h, int side, int part)
{
    if (part != CNP_ON) {
        Fill(dc, side < 0 ? x - 6 : x, y, side < 0 ? x : x + 6, y + h, CanPartColor(part, kStop));
        return;
    }
    POINT p[4] = { { x, y }, { x + side * 40, y + 18 }, { x + side * 40, y + 18 + h - 8 }, { x, y + h } };
    Poly(dc, p, 4, kStop);
}

// A turn's arrow at a corner, pointing out (side -1 / +1)
static void CanTurn(HDC dc, int x, int y, int side, int part)
{
    POINT p[3] = { { x + side * 14, y }, { x, y - 9 }, { x, y + 9 } };
    Poly(dc, p, 3, CanPartColor(part, kOrange));
}

// The car from above, its front up, cx its middle, y its top (the beams above it): 100 x 240, the doors 40 out
static void CanCarPic(HDC dc, int cx, int y, const VCan3* c)
{
    int l = cx - 50, r = cx + 50, top = y + 34;
    bool b = c->hasBody, li = c->hasLight;
    int light = c->light;
    if (li && light & (VCL_LOW | VCL_HIGH)) {   // the beams: the low short, the high long
        int len = light & VCL_HIGH ? 34 : 18;
        COLORREF beam = light & VCL_HIGH ? RGB(0xa8, 0xd0, 0xff) : RGB(0xff, 0xd6, 0x5a);
        for (int s = -1; s <= 1; s += 2) {
            int x = cx + s * 32;
            POINT q[4] = { { x - 10, top - 2 }, { x + 10, top - 2 }, { x + 16, top - len }, { x - 16, top - len } };
            Poly(dc, q, 4, beam);
        }
    }
    R(dc, l, top, 100, 240, 26, kPanel, kLine);
    R(dc, l + 8, top + 6, 84, 52, 14, CanPartColor(CanPart(b, (c->open & VCO_HOOD) != 0), kStop));   // the hood
    T(dc, cx, top + 38, L"капот", 13, kInk, 700, AL_MID);
    R(dc, l + 12, top + 66, 76, 28, 8, kLine);    // the windscreen
    R(dc, l + 8, top + 192, 84, 42, 14, CanPartColor(CanPart(b, (c->open & VCO_TRUNK) != 0), kStop));   // the trunk
    T(dc, cx, top + 219, L"багажник", 13, kInk, 700, AL_MID);
    int fl = CanPart(b, (c->open & VCO_FL) != 0), fr = CanPart(b, (c->open & VCO_FR) != 0);
    int rear = CanPart(b, (c->open & VCO_REAR) != 0);   // (one flag for both rear doors)
    CanDoor(dc, l, top + 74, 56, -1, fl);
    CanDoor(dc, r, top + 74, 56, 1, fr);
    CanDoor(dc, l, top + 134, 52, -1, rear);
    CanDoor(dc, r, top + 134, 52, 1, rear);
    int lo = CanPart(li, (light & VCL_LOW) != 0), hi = CanPart(li, (light & VCL_HIGH) != 0);
    int side = CanPart(li, (light & VCL_SIDE) != 0);
    COLORREF head = hi == CNP_ON ? RGB(0xa8, 0xd0, 0xff) : lo == CNP_ON ? RGB(0xff, 0xd6, 0x5a)
                    : side == CNP_ON ? RGB(0xc0, 0x8a, 0x30) : CanPartColor(lo, 0);
    R(dc, l + 6, top - 4, 30, 8, 4, head);    // the headlights
    R(dc, r - 36, top - 4, 30, 8, 4, head);
    COLORREF tail = side == CNP_ON ? RGB(0x90, 0x20, 0x18) : CanPartColor(side, 0);
    R(dc, l + 6, top + 236, 24, 7, 3, tail);   // the side lights at the back
    R(dc, r - 30, top + 236, 24, 7, 3, tail);
    int ff = CanPart(li, (light & VCL_FOGF) != 0), fo = CanPart(li, (light & VCL_FOGR) != 0);
    Disc(dc, l - 8, top + 14, 6, CanPartColor(ff, RGB(0xff, 0xf0, 0xb0)));   // the front fogs
    Disc(dc, r + 8, top + 14, 6, CanPartColor(ff, RGB(0xff, 0xf0, 0xb0)));
    R(dc, cx - 12, top + 238, 24, 8, 3, CanPartColor(fo, RGB(0xff, 0x20, 0x20)));   // the rear fog
    int tl = CanPart(li, (light & VCL_LEFT) != 0), tr = CanPart(li, (light & VCL_RIGHT) != 0);
    CanTurn(dc, l - 6, top + 40, -1, tl);
    CanTurn(dc, r + 6, top + 40, 1, tr);
    CanTurn(dc, l - 6, top + 210, -1, tl);
    CanTurn(dc, r + 6, top + 210, 1, tr);
}

// The wheel turned by the angle CAN gives (as can_panel's @ANG: plus - clockwise on the picture; which way is the
// car's right is what the step "руль" checks)
static void CanWheelPic(HDC dc, int cx, int cy, const VCan3* c)
{
    Disc(dc, cx, cy, 36, CLR_INVALID, c->hasWheel ? kText : kOff);
    double an = c->hasWheel && c->wheel ? (c->wheel - 32767) / 15.0 * 3.14159265 / 180 : 0;
    double s = sin(an), k = cos(an);
    int ux = (int)(34 * s), uy = (int)(-34 * k);
    Line(dc, cx - (int)(34 * k), cy - (int)(34 * s), cx + (int)(34 * k), cy + (int)(34 * s), 4,
         c->hasWheel ? kMuted : kOff);   // the spokes across
    Disc(dc, cx + ux, cy + uy, 6, c->hasWheel ? kOrange : kOff);   // its top mark
    wchar_t w[24];
    CanWheelText(w, 24, c);
    T(dc, cx, cy + 58, w, 15, kText, 700, AL_MID);
}

// A pill: the handbrake, the ignition
static void CanPill(HDC dc, int x, int y, int w, const wchar_t* label, int part, COLORREF on)
{
    R(dc, x, y, w, 28, 14, part == CNP_ON ? on : kPanel, CanPartColor(part, on));
    T(dc, x + w / 2, y + 20, label, 14, part == CNP_ON ? kInk : CanPartColor(part, on), 700, AL_MID);
}

// The diagnostics while it records (instead of its tests and its "Журналы" row; the stop in the bottom bar): the
// car's picture on the left (what CAN says, to check against the car itself), the step and its buttons on the
// right, the wheel and the numbers under them
static bool CanRecScreen(HDC dc)   // false: not recording
{
    if (!s_canRecOn)
        return false;
    static const wchar_t* const labels[3] = { L"Готово", L"Пропустить", L"Назад" };
    VCan3 c;
    EnterCriticalSection(&s_canLiveCs);
    c = s_canLive;
    LeaveCriticalSection(&s_canLiveCs);
    CanCarPic(dc, 150, 72, &c);
    CanPill(dc, 40, 362, 104, L"Ручник", CanPart(c.hasHand, c.hand), kStop);
    CanPill(dc, 156, 362, 104, L"Зажигание", CanPart(c.hasBody, c.ign == 5), kOk);
    T(dc, 312, 96, L"Запись COM3 панели", 19, kText, 700);
    wchar_t step[160];
    CanRecStepTitle(step, 160, s_canRecStep);
    T(dc, 312, 126, step, 16, kOrange, 700, AL_START, 460);
    bool done = s_canRecStep >= CRS_COUNT;
    for (int i = 0; i < 3; i++)
        Button(dc, 312 + i * 156, 142, 148, 52, labels[i], done && i < 2 ? B_DIS : i ? B_N : B_SEL, IC_NONE, 16, 0,
               done && i < 2 ? A_NONE : A_CANRECMARK, i);
    wchar_t m[64];
    _snwprintf(m, 64, s_canRecMarkN ? L"Меток: %d · %s" : L"Меток пока нет", s_canRecMarkN, s_canMarkLast);
    m[63] = 0;
    T(dc, 312, 220, m, 14, kMuted, 400, AL_START, 460);
    CanWheelPic(dc, 356, 290, &c);
    wchar_t rows[CRR_COUNT][96];
    CanRecRows(rows, &c);
    for (int i = 0; i < CRR_COUNT; i++)
        T(dc, 420, 254 + i * 24, rows[i], 15, i == CRR_COUNT - 1 ? kMuted : kText, 400, AL_START, 356);
    wchar_t cr[64];   // its stop in the bottom bar, right of "back" (the user 2026-11-28)
    bool busy;
    CanRecLabel(cr, 64, &busy);
    Button(dc, 456, 418, 320, 52, cr, B_SEL, IC_NONE, 16, 0, A_CANREC);
    return true;
}
