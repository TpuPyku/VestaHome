// vh_diag.inc - part of home.cpp (included there, in order): diagnostics and its tests.

// ---- diagnostics: the stock shell hides its engineering menu (CEngMainMenuDlg: CAN/UART, speakers, keys, fan,
// GPS, Wi-Fi MAC, touch, TFT calibration...) behind the four corners of its "Info" window; here it opens from
// "About the system". The tests that need the MCU protocol are grey until it is known.

static int s_testStep;            // the screen test's current fill
enum { kTestSteps = 7, kTouchMax = 256 };
static POINT s_touch[kTouchMax];  // the touch test's trail, oldest first
static int   s_touchCount;

struct Test { const wchar_t* label; Action a; int arg; };
static const Test kTests[10] = { { L"Экран", A_SCREEN, S_TEST_SCREEN }, { L"Касания", A_SCREEN, S_TEST_TOUCH },
                                { L"Калибровка", A_CALIBRATE, 0 }, { L"Память и диски", A_SCREEN, S_TEST_MEM },
                                { L"Кнопки · MCU", A_SCREEN, S_TEST_MCU }, { L"Звук", A_SCREEN, S_TEST_AV },
                                { L"GPS · CAN · Поток", A_SCREEN, S_TEST_CAN },
                                { L"Камера заднего хода", A_RVCVIEW, 0 },   // (the user 2026-10-29: here, not in "Car")
                                { L"Текстовые файлы", A_TEXTFILES, 0 } };
                                // ("Перезапуск Bluetooth" gone: the user 2026-11-16)

static int   s_modemState;   // the USB modem's driver (vh_gps.inc ModemUsbSet): 0 not yet, 1 registered, -1 failed
static DWORD s_modemErr;
struct ModemInfo {           // (vh_gps.inc ModemGet)
    wchar_t port[16];        // "" - no modem (or no COM port)
    int     rssi, mode, sim;
    bool    online;
};
static volatile LONG s_rasBusy;   // dialing (vh_gps.inc ModemNet)
static DWORD         s_rasErr;    // the last dial's error
static bool          s_netWait;   // "Подключить" before the modem found its network: dialled when it does (vh_gps.inc)
static void ModemGet(ModemInfo* m);

static const wchar_t* ModemNetName(int mode)   // Huawei's sys_mode
{
    return mode == 3 ? L"2G" : mode == 5 || mode == 7 ? L"3G" : L"нет сети";
}

// The modem's button: what it does next, with the signal (0..31) and the network
static void ModemLabel(wchar_t* s, int n)
{
    ModemInfo m;
    ModemGet(&m);
    if (s_modemState < 0)
        _snwprintf(s, n, L"USB-модем: ошибка %lu", s_modemErr);
    else if (!s_modemState)
        _snwprintf(s, n, L"USB-модем: включить");
    else if (s_rasBusy)
        _snwprintf(s, n, L"Модем: подключение…");
    else if (m.online)
        _snwprintf(s, n, L"Модем в сети · отключить");
    else if (!m.port[0])
        _snwprintf(s, n, L"USB-модем: вставьте модем");
    else if (m.sim == 255)
        _snwprintf(s, n, L"Модем: нет SIM-карты");
    else if (!DialReady(m.rssi, m.mode, m.sim))
        _snwprintf(s, n, s_netWait ? L"Модем ищет сеть… подключится сам" : L"Модем ищет сеть… · подключить");
    else if (s_rasErr)
        _snwprintf(s, n, L"Модем: ошибка %lu · ещё раз", s_rasErr);
    else
        _snwprintf(s, n, L"Модем %s · %d/31 · подключить", ModemNetName(m.mode), m.rssi < 0 || m.rssi > 31 ? 0 : m.rssi);
    s[n - 1] = 0;
}

// Diagnostics (from "About the system"): the tests 2 x 4, then closing and minimizing VestaHome.
static void ScrDiag(HDC dc)
{
    Frame(dc, L"Диагностика");
    Panel(dc, 16, 68, 768, 336);   // (above y 404: "back" at the bottom left)
    for (int i = 0; i < 10 && kTests[i].label; i++)
        Button(dc, 40 + (i % 2) * 368, 80 + (i / 2) * 52, 352, 44, kTests[i].label,
               B_N, IC_NONE, 16, 0, kTests[i].a, kTests[i].arg);
    Sep(dc, 40, 340, 720);
    T(dc, 40, 368, L"Журналы", 19, kText, 700);   // VestaHome, the proxies, VestaStub (s_logsOn)
    T(dc, 40, 390, L"VestaHome и загрузчик — в VestaLogs на SD-карте или USB", 14, kMuted, 400, AL_START, 600);
    Toggle(dc, 688, 354, T_LOGS);
}

// Screen test: full-screen white, red, green, blue, black (dead pixels, uniformity), a grey ramp (banding) and a
// 40 px grid (geometry); the colours are pure, night doesn't dim them. A tap shows the next one, after the last
// one diagnostics come back.
static void ScrTestScreen(HDC dc)
{
    static const COLORREF fills[5] = { RGB(255, 255, 255), RGB(255, 0, 0), RGB(0, 255, 0), RGB(0, 0, 255),
                                       RGB(0, 0, 0) };
    Zone(0, 0, 800, 480, A_TEST);
#ifndef UNDER_CE
    GdiFlush();
#endif
    for (int y = 0; y < 480; y++) {
        Pix* row = s_backBits + (479 - y) * 800;
        for (int x = 0; x < 800; x++) {
            int v;
            if (s_testStep < 5) {
                COLORREF c = fills[s_testStep];
                row[x] = PIX(GetRValue(c), GetGValue(c), GetBValue(c));
                continue;
            }
            if (s_testStep == 5)
                v = x * 255 / 799;
            else
                v = x % 40 == 0 || y % 40 == 0 || x == 799 || y == 479 ? 255 : 0;
            row[x] = PIX(v, v, v);
        }
    }
    if (s_testStep == 0) {   // on the first fill only: the hint would hide pixels of the others
        wchar_t s[48];
        _snwprintf(s, 48, L"Нажмите — следующий (всего %d)", kTestSteps);
        T(dc, 400, 300, s, 18, kInk, 600, AL_MID);
    }
}

// Touch test: every point the finger passes is marked, the last one with a crosshair and its coordinates.
static void ScrTestTouch(HDC dc)
{
    Fill(dc, 0, 0, 800, 480, kInk);
    for (int x = 0; x < 800; x += 80)
        Fill(dc, x, 0, x + 1, 480, kPanel2);
    for (int y = 0; y < 480; y += 80)
        Fill(dc, 0, y, 800, y + 1, kPanel2);
    for (int i = 0; i < s_touchCount; i++)
        Fill(dc, s_touch[i].x - 2, s_touch[i].y - 2, s_touch[i].x + 3, s_touch[i].y + 3, kOrange);
    if (s_touchCount) {
        const POINT& p = s_touch[s_touchCount - 1];
        Fill(dc, 0, p.y, 800, p.y + 1, kText);
        Fill(dc, p.x, 0, p.x + 1, 480, kText);
        wchar_t s[48];
        _snwprintf(s, 48, L"x %d · y %d · точек %d", p.x, p.y, s_touchCount);
        T(dc, 16, 36, s, 18, kText, 700);
    } else {
        T(dc, 16, 36, L"Проведите пальцем по экрану, углы и края тоже", 18, kMuted, 400);
    }
    Button(dc, 680, 8, 112, 48, L"Выход", B_FILL, IC_NONE, 16, 0, A_BACK);
}

static void AddTouch(int x, int y)
{
    if (s_touchCount == kTouchMax) {
        memmove(s_touch, s_touch + 1, (kTouchMax - 1) * sizeof(POINT));
        s_touchCount--;
    }
    s_touch[s_touchCount].x = x;
    s_touch[s_touchCount].y = y;
    s_touchCount++;
}

// "123 МБ" or "12,3 ГБ".
static void Size(wchar_t* s, int n, unsigned __int64 bytes)
{
    unsigned mb = (unsigned)(bytes / (1024 * 1024));
    if (mb < 2048)
        _snwprintf(s, n, L"%u МБ", mb);
    else
        _snwprintf(s, n, L"%u,%u ГБ", mb / 1024, mb % 1024 * 10 / 1024);
}

// Memory and volumes: free RAM, uptime, free space of the unit's volumes (absent ones say so).
static void ScrTestMem(HDC dc)
{
#ifdef UNDER_CE
    static const wchar_t* vols[5] = { L"\\SDMMC", L"\\ResidentFlash", L"\\StorageFlash", L"\\SDMemory",
                                      L"\\USBDisk" };
#else
    static const wchar_t* vols[5] = { L"C:\\", L"D:\\", L"E:\\", L"F:\\", L"G:\\" };
#endif
    Frame(dc, L"Диагностика · Память и диски");
    Panel(dc, 16, 68, 768, 336);   // (above y 404: "back" at the bottom left)
    for (int i = 0; i < 7; i++) {
        int y = 74 + i * 46;
        wchar_t label[40], free_[24], total[24], v[64];
        int pct = -1;
        if (i == 0) {
            MEMORYSTATUS ms;
            ms.dwLength = sizeof(ms);
            GlobalMemoryStatus(&ms);
            wcscpy(label, L"Оперативная память");
            Size(free_, 24, ms.dwAvailPhys);
            Size(total, 24, ms.dwTotalPhys);
            _snwprintf(v, 64, L"свободно %s из %s", free_, total);
            pct = ms.dwTotalPhys ? (int)((ms.dwTotalPhys - ms.dwAvailPhys) / (ms.dwTotalPhys / 100 + 1)) : 0;
        } else if (i == 1) {
            DWORD m = GetTickCount() / 60000;
            wcscpy(label, L"Время работы");
            _snwprintf(v, 64, L"%u ч %02u мин", (unsigned)(m / 60), (unsigned)(m % 60));
        } else {
            const wchar_t* vol = vols[i - 2];
            ULARGE_INTEGER avail, size, freeAll;
            wcsncpy(label, vol, 39);
            label[39] = 0;
            if (GetDiskFreeSpaceEx(vol, &avail, &size, &freeAll) && size.QuadPart) {
                Size(free_, 24, freeAll.QuadPart);
                Size(total, 24, size.QuadPart);
                _snwprintf(v, 64, L"свободно %s из %s", free_, total);
                pct = (int)((size.QuadPart - freeAll.QuadPart) * 100 / size.QuadPart);
            } else {
                wcscpy(v, L"нет");
            }
        }
        v[63] = 0;
        T(dc, 40, y + 29, label, 17, kMuted, 400, AL_START, 220);
        if (pct >= 0) {
            R(dc, 270, y + 18, 180, 10, 5, kLine);
            R(dc, 270, y + 18, pct * 180 / 100 > 10 ? pct * 180 / 100 : 10, 10, 5, pct > 90 ? kStop : kMuted);
        }
        T(dc, 760, y + 29, v, 17, kText, 700, AL_END, 290);
        if (i < 6)
            Sep(dc, 32, y + 44, 736);
    }
}

// Touch calibration: the system's crosshair screen (coredll's TouchCalibrate, as G3NavHMI's engineering menu
// does). The registry is reset on every boot, so the result holds until the next start.
static void Calibrate()
{
#ifdef UNDER_CE
    typedef BOOL (*CalibrateFn)(void);
    CalibrateFn f = (CalibrateFn)GetProcAddress(GetModuleHandle(L"coredll.dll"), L"TouchCalibrate");
    if (!f) {
        wcscpy(s_status, L"в системе нет TouchCalibrate");
        return;
    }
    SetWindowPos(s_wnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);   // its screen must not stay under us
    BOOL ok = f();
    SetWindowPos(s_wnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    SetForegroundWindow(s_wnd);
    wcscpy(s_status, ok ? L"калибровка действует до перезагрузки" : L"калибровка отменена");
#else
    wcscpy(s_status, L"калибровка — только на ГУ");
#endif
}

// Explorer's desktop and taskbar for leaving VestaHome (minimize or exit): started when it does not run.
static void StartExplorer()
{
#ifdef UNDER_CE
    s_taskbar = FindWindow(L"HHTaskBar", NULL);
    if (!s_taskbar) {
        PROCESS_INFORMATION pi;
        if (!CreateProcess(L"\\Windows\\explorer.exe", NULL, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi)) {
            wcscpy(s_status, L"explorer.exe не запускается");
            return;
        }
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        for (int i = 0; i < 30 && !s_taskbar; i++) {   // its taskbar comes up within a few seconds
            Sleep(100);
            s_taskbar = FindWindow(L"HHTaskBar", NULL);
        }
    }
    if (s_taskbar)
        ShowWindow(s_taskbar, SW_SHOW);
#endif
}

// "Minimize": VestaHome stays open but goes behind everything (not hidden), Explorer comes up. Starting
// VestaHome.exe again, or any activation, brings it back on top (WM_ACTIVATE).
static bool s_minimized;

static void MinimizeHome()
{
#ifdef UNDER_CE
    s_minimized = true;
    SetWindowPos(s_wnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    SetWindowPos(s_wnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    StartExplorer();
    VLog(L"свернут (на задний план)");
#else
    ShowWindow(s_wnd, SW_MINIMIZE);   // PC: Explorer runs anyway
#endif
}

static void ScrWarning(HDC dc)
{
    Frame(dc, L"", A_NONE);
    Panel(dc, 100, 84, 600, 320, 16);
    Ico(dc, IC_WARN, 132, 112, 56, kText);
    T(dc, 204, 150, L"Внимание", 26, kText, 700);
    T(dc, 132, 212, L"Не отвлекайтесь от дороги. Управляйте системой", 19, kMuted, 400, AL_START, 540);
    T(dc, 132, 242, L"только когда это безопасно. Правила дорожного", 19, kMuted, 400, AL_START, 540);
    T(dc, 132, 272, L"движения важнее подсказок навигации.", 19, kMuted, 400, AL_START, 540);
    Button(dc, 452, 324, 216, 60, L"Принять", B_PRI, IC_NONE, 19, 0, A_BACK);
    T(dc, 132, 360, L"Окно закроется само через 10 с", 15, kDim, 400, AL_START, 310);
}

static void DrawScreen(HDC dc, Screen s);

// Windows over the previous screen: it is drawn (with its bottom bar), dimmed below the top bar and loses its
// buttons.
static void Under(HDC dc, int alpha)
{
    Screen prev = s_histLen ? s_hist[s_histLen - 1] : S_HOME;
    int pressed = s_pressed;
    s_pressed = -1;   // the pressed button is the window's, not one of the screen under it
    s_histLen--;      // as it was drawn: the window is not in its history
    if (prev == S_HOME)
        MainBackground(dc);
    s_frameBack = A_NONE;
    DrawScreen(dc, prev);
    if (NavSection(prev) >= 0)
        NavBar(dc, NavSection(prev));
    else
        BackButton(dc);   // (the screen's look under the window; its button goes with the others below)
    s_frameBack = A_NONE;   // the window has none of its own
    s_histLen++;
    s_pressed = pressed;
    if (s_partial) {   // the screen under the window has no buttons (nor list)
        s_zoneIdx = 0;
        s_zoneMoved = false;
    } else {
        kBtnCount = 0;
        s_scroll = NULL;
    }
    Shade(0, 56, 800, 424, alpha, alpha);
}

// A window as a sheet (design 0.7): a card over the bottom bar, the whole width, a soft shadow above and below - the
// driver's finger is near the bottom of the screen. The main action is on the right. Returns the sheet's top.
enum { kSheetX = 16, kSheetW = 768, kSheetBottom = 408, kSheetShadow = 14, kSheetDark = 120,
       kSheetInX = 40, kSheetInW = 720, kSheetBtnY = 318, kSheetBtnH = 66, kSheetGap = 16 };

static int Sheet(HDC dc, int h)
{
    int top = kSheetBottom - h;
    Shade(kSheetX, top - kSheetShadow, kSheetW, kSheetShadow, 0, kSheetDark);
    Shade(kSheetX, kSheetBottom, kSheetW, kSheetShadow, kSheetDark, 0);
    Panel(dc, kSheetX, top, kSheetW, h, 22);
    return top;
}

// The power button of "About the system": exit or minimize VestaHome; both bring up Explorer. The stock shell
// and started programs keep running.
static void ScrExit(HDC dc)
{
    Under(dc, 184);
    int top = Sheet(dc, 208);
    Ico(dc, IC_POWER, kSheetInX, top + 14, 48, kText);
    T(dc, kSheetInX + 60, top + 48, L"VestaHome", 24, kText, 700, AL_START, 600);
    T(dc, kSheetInX, top + 88, L"Свернуть — останется позади.  Штатная — перезагрузка в штатную.  Выход — закроется.",
      15, kMuted, 400, AL_START, kSheetInW);
    int w = (kSheetInW - 3 * kSheetGap) / 4;
    Button(dc, kSheetInX, kSheetBtnY, w, kSheetBtnH, L"Отмена", B_N, IC_NONE, 16, 0, A_BACK);
    Button(dc, kSheetInX + w + kSheetGap, kSheetBtnY, w, kSheetBtnH, L"Свернуть", B_N, IC_NONE, 16, 0, A_MINIMIZE);
    Button(dc, kSheetInX + 2 * (w + kSheetGap), kSheetBtnY, w, kSheetBtnH, L"Штатная", B_N, IC_NONE, 16, 0, A_SHELL);
    Button(dc, kSheetInX + 3 * (w + kSheetGap), kSheetBtnY, w, kSheetBtnH, L"Выход", B_PRI, IC_NONE, 16, 0, A_EXIT);
}

// The knob held 5 s (MCU 0x1102 18): the MCU reboots the unit 5 s later. The stock shell shows its own window
// with "Cancel" = key 0x5A (SPIPower.log 2026-10-18); this is it over VestaHome.
static DWORD s_rebootTick;

static void ScrReboot(HDC dc)
{
    Under(dc, 184);
    int top = Sheet(dc, 208);
    Ico(dc, IC_POWER, kSheetInX, top + 14, 48, kOrange);
    int left = 5 - (int)((GetTickCount() - s_rebootTick) / 1000);
    wchar_t s[48];
    _snwprintf(s, 48, L"Перезагрузка через %d с", left < 0 ? 0 : left);
    T(dc, kSheetInX + 60, top + 48, s, 24, kText, 700, AL_START, 600);
    T(dc, kSheetInX, top + 88, L"Крутилка громкости удержана. Отменить?", 15, kMuted, 400, AL_START, kSheetInW);
    Button(dc, kSheetInX + kSheetInW - 240, kSheetBtnY, 240, kSheetBtnH, L"Отмена", B_PRI, IC_NONE, 18, 0, A_REBOOTNO);
}

static void ScrIncoming(HDC dc)
{
    BtState bt;
    BtGet(&bt, NULL);
    wchar_t num[40], name[64];
    BtText(bt.callNumber, num, 40);
    CallerName(bt, name, 64);
    Under(dc, 184);
    int top = Sheet(dc, 236);
    Disc(dc, kSheetInX + 36, top + 62, 36, kPanel2);
    Ico(dc, IC_CALL, kSheetInX + 12, top + 38, 48, kOk);
    T(dc, kSheetInX + 96, top + 36, L"Входящий вызов", 14, kMuted, 700);
    T(dc, kSheetInX + 96, top + 80, name[0] ? name : num[0] ? num : L"номер скрыт", 32, kText, 700, AL_START, 600);
    if (name[0])
        T(dc, kSheetInX + 96, top + 110, num, 18, kMuted, 400, AL_START, 600);
    enum { kW = (kSheetInW - kSheetGap) / 2, kY = 296, kH = 88 };
    int xEnd = kSheetInX, xAns = kSheetInX + kW + kSheetGap;   // the main action on the right, as in every sheet
    bool d = Zone(xEnd, kY, kW, kH, A_PHONEEND);   // green and red: here the colour carries the meaning
    R(dc, xEnd, kY, kW, kH, 12, d ? Mix(kStop, kInk, 160) : kStop);
    Ico(dc, IC_HANGUP, xEnd + kW / 2 - 24, kY + 20, 48, kInk);
    d = Zone(xAns, kY, kW, kH, A_PHONEANS);
    R(dc, xAns, kY, kW, kH, 12, d ? Mix(kOk, kInk, 160) : kOk);
    Ico(dc, IC_CALL, xAns + kW / 2 - 24, kY + 20, 48, kInk);
}

enum { kVolTop = 30 };   // the shell's volume scale (user 2026-10-19)

static void ScrVolume(HDC dc)
{
    Under(dc, 153);
    Zone(0, 0, 800, 480, A_BACK);
    Panel(dc, 150, 150, 500, 180, 16);
    // the MCU's volume of the current source, 0..30 as the shell's (30 bars, one a step), as it comes (the knob,
    // the wheel)
    int vol, mute, src;
    bool have = VolGet(&vol, &mute, &src);
    const wchar_t* name = src == 0 ? L"Громкость · Радио" : src == 6 || src == 7 ? L"Громкость · Медиа"
                        : src == 9 ? L"Громкость · Bluetooth" : L"Громкость";
    Ico(dc, IC_VOL, 182, 182, 48, have && !mute ? kText : kDim);
    T(dc, 246, 216, name, 15, kMuted, 700);
    wchar_t s[16];
    if (!have)
        wcscpy(s, L"–");
    else if (mute)
        wcscpy(s, L"Без звука");
    else
        _snwprintf(s, 16, L"%d", vol);
    T(dc, 618, 222, s, mute ? 28 : 48, mute ? kMuted : kText, 700, AL_END);
    int lit = have && !mute ? (vol < kVolTop ? vol : kVolTop) : 0;
    for (int i = 0; i < kVolTop; i++) {
        int h = 14 + i * 36 / (kVolTop - 1);
        R(dc, 182 + i * 15, 300 - h, 10, h, 2, i < lit ? kOrange : kLine);
    }
    if (!have)
        T(dc, 182, 316, L"MCU ещё не присылал громкость (нужен загрузчик)", 13, kDim, 400);
}

// The screen saver: the skin's picture (Skin\saver.png, skin.ini "saver": Lada's X-design, made by
// skin/make_saver.py), dimmed by the night theme; the clock, the date, the weather with its picture in the middle,
// what plays below. A tap goes back.
static void SaverWeather(HDC dc, int cx, int y);   // (after the weather's part)
static Icon SaverNowPlaying(wchar_t* s, int n);    // (after the players' parts)

static void ScrSaver(HDC dc)
{
    SYSTEMTIME t;
    GetLocalTime(&t);
    wchar_t clock[8], date[48], s[160];
    Now(clock, 8);
    _snwprintf(date, 48, L"%s, %d %s", kDays[t.wDayOfWeek % 7], t.wDay, kMonths[(t.wMonth + 11) % 12]);
    CharUpperBuff(date, 1);
    Zone(0, 0, 800, 480, A_BACK);
    SaverBackground(dc);
    T(dc, 400, 218, clock, 132, kText, 700, AL_MID);
    T(dc, 400, 270, date, 24, kMuted, 400, AL_MID);
    SaverWeather(dc, 400, 300);
    if (CamOver()) {   // speeding at a camera: on the saver too (the user 2026-10-26) - the sign, the speed in red
        SpeedSign(dc, 316, 70, 44, CamLimit());
        _snwprintf(s, 160, L"%d км/ч", CarKmh());
        T(dc, 376, 88, s, 44, kStop, 700);
    }
    Icon ic = SaverNowPlaying(s, 160);   // what plays
    if (s[0]) {
        int w = TextWidth(dc, s, 18, 700);
        w = w > 560 ? 560 : w;
        Ico(dc, ic, 400 - (w + 40) / 2, 424, 30, kMuted);
        T(dc, 400 - (w + 40) / 2 + 40, 446, s, 18, kText, 700, AL_START, 560);
    }
}

static void Trim(wchar_t* s);
