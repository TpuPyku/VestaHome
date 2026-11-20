// vh_widgetset.inc - part of home.cpp (included there, in order): widget settings, the main screen's widgets.

// ---- widgets' settings (opened from a widget's caption): sub screens, rows of 56 px over the whole width

static wchar_t s_coordText[48];   // the home point's latitude or longitude while it is typed
static int     s_coordWhich;      // K_HOMELAT / K_HOMELON

// Row: label and a second line; the control is drawn by the caller to the right of `right`. With an action the
// left part is a button of its own.
static void WRow(HDC dc, int y, const wchar_t* label, const wchar_t* sub, Action a = A_NONE, int arg = 0,
                 int right = 520, bool last = false)
{
    bool down = RowCore(dc, 24, y, 56, right - 24, 752, right - 52, label, sub, a, arg, last);   // (the settings')
    if (a != A_NONE)
        Ico(dc, IC_NEXT, right - 36, y + 14, 28, down ? kInk : kDim);
}

static void ScrWSetWeather(HDC dc)
{
    static const wchar_t* prov[W_COUNT] = { L"Open-Meteo", L"OpenWeather", L"Яндекс" };
    Frame(dc, L"Погода · Настройки");
    Panel(dc, 16, 64, 768, 344);   // (above y 408: "back" at the bottom left)
    wchar_t s[120];
    int p = s_cfg.weatherProv;
    WRow(dc, 68, L"Источник", p == W_OMETEO ? L"открытый, без ключа" : p == W_OWM ? L"бесплатный ключ на openweathermap.org"
         : L"свой ключ «Погоды для умного дома»", A_NONE, 0, 300);
    Seg(dc, 300, 74, 468, 44, prov, W_COUNT, G_WPROV, 14);
    const wchar_t* key = s_cfg.weatherKey[p];
    WRow(dc, 124, L"Ключ API", p == W_OMETEO ? L"не нужен" : key[0] ? key : L"не задан — нажмите, чтобы ввести",
         p == W_OMETEO ? A_NONE : A_KEYS, K_WKEY, 776);
    WRow(dc, 180, L"Место по GPS", L"основное; без фикса — домашняя точка", A_NONE, 0, 640);
    Toggle(dc, 688, 190, T_W_GPS);
    WRow(dc, 236, L"Домашняя точка", s_cfg.homeName[0] ? s_cfg.homeName : L"название — нажмите, чтобы ввести", A_KEYS,
         K_HOMENAME, 776);
    bool set = s_cfg.homeLat || s_cfg.homeLon;   // two fields of digits (the user 2026-11-14)
    _snwprintf(s, 120, set ? L"%.5f" : L"не задана", s_cfg.homeLat);
    RowCore(dc, 24, 292, 56, 268, 268, 240, L"Широта дома", s, A_KEYS, K_HOMELAT, false);
    _snwprintf(s, 120, set ? L"%.5f" : L"не задана", s_cfg.homeLon);
    RowCore(dc, 300, 292, 56, 268, 268, 240, L"Долгота дома", s, A_KEYS, K_HOMELON, false);
    Button(dc, 576, 298, 192, 44, L"Из GPS", B_N, IC_NAV, 15, 22, A_HOMEGPS);
    Weather w;
    WxGet(&w);
    if (s_wxBusy)
        wcscpy(s, L"загрузка…");
    else if (w.ok < 0)
        _snwprintf(s, 120, L"ошибка: %s", w.err);
    else if (w.have)
        _snwprintf(s, 120, L"обновлено %02d:%02d · %s · %+d°", w.at.wHour, w.at.wMinute, w.place, w.temp);
    else
        wcscpy(s, L"ещё не загружалась");
    WRow(dc, 348, L"Состояние", s, A_NONE, 0, 568, true);
    Button(dc, 576, 354, 192, 44, L"Обновить", B_PRI, IC_REFRESH, 15, 22, A_WXNOW);
    // Open-Meteo's data is CC BY 4.0: its credit wherever it is shown in detail (here; also "About the system")
    bool om = p == W_OMETEO || (w.have && w.prov == W_OMETEO);
    T(dc, 96, 450, om ? L"Обновляется раз в 30 мин. Weather data by Open-Meteo.com (CC BY 4.0)"
                      : L"Обновляется раз в 30 мин.", 13, kDim, 400, AL_START, 680);
}

// The trip widget's settings: the service (ТО, the user 2026-11-14, service.h) - the odometer, the last service's km
// and date (typed, A_KEYS K_ODO / K_SVCKM / K_SVCDATE), the 7000 km interval, "ТО пройдено" (now: twice, "Точно?").
static wchar_t s_svcText[24];   // what the keyboard edits for them
static int     s_svcKey;        // which of them (K_*)
static bool    s_svcAsk;        // "ТО пройдено" pressed once

static void ScrWSetTrip(HDC dc)
{
    static DWORD s_seen;
    if (s_seen != s_screenTick) {
        s_seen = s_screenTick;
        s_svcAsk = false;
    }
    Frame(dc, L"Бортовой компьютер · ТО");
    Panel(dc, 16, 64, 768, 344);   // (above y 408: "back" at the bottom left)
    wchar_t s[96];
    if (s_cfg.odoKm >= 0)
        _snwprintf(s, 96, L"%.0f км · дальше считается по скорости (CAN или GPS)", s_cfg.odoKm);
    else
        wcscpy(s, L"не задан — нажмите, чтобы ввести (как на приборной панели)");
    WRow(dc, 68, L"Пробег сейчас", s, A_KEYS, K_ODO, 776);
    _snwprintf(s, 96, L"%.0f км", s_cfg.svcKm);
    WRow(dc, 124, L"Прошлое ТО: пробег", s, A_KEYS, K_SVCKM, 776);
    if (s_cfg.svcYmd)
        _snwprintf(s, 96, L"%02d.%02d.%d", s_cfg.svcYmd % 100, s_cfg.svcYmd / 100 % 100, s_cfg.svcYmd / 10000);
    else
        wcscpy(s, L"не задана — ДД.ММ.ГГГГ");
    WRow(dc, 180, L"Прошлое ТО: дата", s, A_KEYS, K_SVCDATE, 776);
    WRow(dc, 236, L"Межсервисное ТО", s_on[T_SVC7000] ? L"включено: 7000 км или 1 год"
         : L"выключено: по регламенту Весты — 15 000 км или 1 год", A_NONE, 0, 640);
    Toggle(dc, 688, 246, T_SVC7000);
    SvcText(s, 96);
    WRow(dc, 292, L"Следующее ТО", s, A_NONE, 0, 568, true);
    Button(dc, 576, 298, 192, 44, s_svcAsk ? L"Точно?" : L"ТО пройдено", s_cfg.odoKm < 0 ? B_DIS : s_svcAsk ? B_PRI : B_N,
           IC_NONE, 15, 0, A_SVCDONE);
    T(dc, 96, 450, L"ТО — по пробегу или через год после прошлого, что раньше. «ТО пройдено» — сейчас, на этом пробеге.",
      13, kDim, 400, AL_START, 680);
}

static void SvcDone()   // "ТО пройдено": twice (the first asks)
{
    if (s_cfg.odoKm < 0)
        return;
    if (!s_svcAsk) {
        s_svcAsk = true;
        return;
    }
    s_svcAsk = false;
    s_cfg.svcKm = s_cfg.odoKm;
    s_cfg.svcYmd = TodayYmd();   // (0 while the clock is not set: by km alone)
    SaveCfg();
}

static void SvcKeys(int which)   // the keyboard for one of them, with its value now
{
    s_svcKey = which;
    if (which == K_ODO)
        _snwprintf(s_svcText, 24, s_cfg.odoKm >= 0 ? L"%.0f" : L"", s_cfg.odoKm);
    else if (which == K_SVCKM)
        _snwprintf(s_svcText, 24, L"%.0f", s_cfg.svcKm);
    else if (s_cfg.svcYmd)
        _snwprintf(s_svcText, 24, L"%02d.%02d.%d", s_cfg.svcYmd % 100, s_cfg.svcYmd / 100 % 100, s_cfg.svcYmd / 10000);
    else
        s_svcText[0] = 0;
    s_svcText[23] = 0;
    OpenNumFor(s_svcText, 23, which == K_ODO ? L"Пробег сейчас, км" : which == K_SVCKM ? L"Пробег на прошлом ТО, км"
               : L"Дата прошлого ТО, ДД.ММ.ГГГГ", S_WSET_TRIP, which == K_SVCDATE ? NUM_DATE : NUM_INT);
}

static void SvcKeysDone()   // (CfgKeysDone) typed: kept when it parses
{
    double km;
    if (s_svcKey == K_SVCDATE) {
        int ymd = ParseDmy(s_svcText);
        if (ymd)
            s_cfg.svcYmd = ymd;
    } else if (ParseKm(s_svcText, &km)) {
        if (s_svcKey == K_ODO) {
            s_cfg.odoKm = km;
            s_odoSaved = -1;   // (counted on from the typed one)
        } else {
            s_cfg.svcKm = km;
        }
    }
    SaveCfg();
}

static void RegionsText(wchar_t* s, int n);

static void ScrWSetSpeed(HDC dc)
{
    static const wchar_t* dist[4] = { L"По скорости", L"200 м", L"400 м", L"600 м" };
    Frame(dc, L"Скорость и камеры");
    Panel(dc, 16, 64, 768, 344);   // (the GPS port and time: in "Car" and "General" only, 2026-10-24)
    wchar_t s[160];
    WRow(dc, 68, L"Предупреждать о камерах", L"знак на виджете и в верхнем баре", A_NONE, 0, 640);
    Toggle(dc, 688, 78, T_CAM_ALERT);
    static const wchar_t* snd[3] = { L"Выкл", L"Всегда", L"Превышение" };
    WRow(dc, 124, L"Звуковой сигнал", L"всегда или если скорость выше лимита камеры", A_NONE, 0, 340);
    Seg(dc, 352, 130, 416, 44, snd, 3, G_CAMSND, 14);
    _snwprintf(s, 160, L"сигнал и знак, если скорость выше лимита камеры на +%d км/ч", s_cfg.camOverBy);
    WRow(dc, 180, L"Порог превышения", s, A_NONE, 0, 552);   // (the user 2026-10-26: by - and +, 1 km/h a step)
    _snwprintf(s, 16, L"+%d км/ч", s_cfg.camOverBy);
    Stepper(dc, 560, 186, 200, s, 44, 18, A_CAMOVERSTEP);
    WRow(dc, 236, L"Дистанция", L"по скорости: за 10 с, 100–500 м", A_NONE, 0, 340);
    Seg(dc, 352, 242, 416, 44, dist, 4, G_CAMDIST, 14);
    wchar_t path[MAX_PATH];
    CamPath(path);
    if (GetFileAttributes(path) == 0xFFFFFFFF)
        _snwprintf(s, 160, L"%s — нет файла", s_cfg.camFile);
    else if (!s_camLoaded)
        _snwprintf(s, 160, L"%s · загрузится по GPS", s_cfg.camFile);
    else
        _snwprintf(s, 160, L"%s · рядом %d из %d", s_cfg.camFile, s_camCount, s_camTotal);
    WRow(dc, 292, L"Файл камер (iGO / Navitel)", s, A_CAMFILE, 0, 776);
    wchar_t regs[160];
    RegionsText(regs, 160);
    if (s_cfg.camUpdated)
        _snwprintf(s, 160, L"%s · обновлено %02d.%02d", regs, s_cfg.camUpdated % 100, s_cfg.camUpdated / 100 % 100);
    else
        wcscpy(s, regs);
    WRow(dc, 348, L"Скачать базу камер (speedcamonline.ru)", s, A_SCREEN, S_CAMDL, 776, true);
}


// One line on the CAN module's stream: the car's speed and the steering angle when they come.
static void CanStatusText(wchar_t* s, int n)
{
    Gps g;
    GpsGet(&g);
    DWORD now = GetTickCount();
    bool spd = g.carTick && now - g.carTick < 5000, ang = g.angTick && now - g.angTick < 5000;
    if (g.status != GPS_OPEN)
        _snwprintf(s, n, L"порт %s не открыт", g.port[0] ? g.port : GpsPortNow());
    else if (spd && ang)
        _snwprintf(s, n, L"v%S · %d км/ч · руль %c%d,%d°", g.canVer, g.carSpeed, g.angle10 < 0 ? L'-' : L'+',
                   abs(g.angle10) / 10, abs(g.angle10) % 10);
    else if (spd)
        _snwprintf(s, n, L"v%S · %d км/ч", g.canVer, g.carSpeed);
    else if (g.carTick)
        wcsncpy(s, L"данные были, сейчас нет", n);
    else
        wcsncpy(s, L"нет строк «@» в потоке", n);
    s[n - 1] = 0;
}

// Settings -> "Car": the data port (the shell's NMEA port is the CAN module's stream), the climate widget, what
// the CAN stream gives, its monitor. CAN settings gather here as they are decoded.
static void ScrSetCar(HDC dc)
{
    enum { H = kSetH };   // six rows: they fit
    wchar_t port[16], baud[16], gps[160], can[64];
    GpsPortText(port, 16);
    GpsBaudText(baud, 16);
    GpsStatusText(gps, 160, true);
    CanStatusText(can, 64);
    Settings(dc, S_SET_CAR);
    SetRow(dc, 64, L"Порт GPS · CAN", gps);
    Stepper(dc, 512, 70, 248, port, 44, 19, A_GPSPORT);
    SetRow(dc, 64 + H, L"Скорость порта", L"по умолчанию 115200");
    Stepper(dc, 512, 70 + H, 248, baud, 44, 19, A_GPSBAUD);
    SetRow(dc, 64 + 2 * H, L"Данные CAN", can, false, A_SCREEN, S_TEST_CAN);
    Chevron(dc, 728, 76 + 2 * H);
    wchar_t mcu[80];
    LONG attach, wraps;
    int st = McuState(&attach, &wraps);
    if (st < 0)
        wcscpy(mcu, L"нет общей памяти");
    else if (!st)
        wcscpy(mcu, L"загрузчик VestaHome не запущен или старый");
    else
        _snwprintf(mcu, 80, L"идут · сообщений %u", (unsigned)s_mcuTotal);
    SetRow(dc, 64 + 3 * H, L"Данные MCU", mcu, false, A_SCREEN, S_TEST_MCU);
    Chevron(dc, 728, 76 + 3 * H);
    wchar_t tank[16];   // (the camera: always ours - the user 2026-10-29; shown by hand from "Diagnostics")
    _snwprintf(tank, 16, L"%d Л", s_cfg.tank);
    SetRow(dc, 64 + 4 * H, L"Объём бака", L"для запаса хода");
    Stepper(dc, 560, 70 + 4 * H, 200, tank, 44, 19, A_TANK);
    wchar_t nm[64], com[8];   // the navigators' port (vh_gps.inc), its number by the arrows
    if (s_nmeaBusy)
        wcscpy(nm, L"загружается…");
    else if (s_nmeaState == -2)
        _snwprintf(nm, 64, L"COM%d занят системой — другой номер", s_cfg.nmeaCom);
    else if (s_nmeaState < 0)
        _snwprintf(nm, 64, L"не загрузился (ошибка %lu)", s_nmeaErr);
    else
        _snwprintf(nm, 64, L"только NMEA%s", s_nmeaState > 0 ? L" · работает" : L"");
    SetRow(dc, 64 + 5 * H, L"Порт для навигаторов", nm, true);
    _snwprintf(com, 8, L"COM%d", s_cfg.nmeaCom);
    Stepper(dc, 508, 70 + 5 * H, 164, com, 44, 19, A_NMEACOM);
    Toggle(dc, 688, 74 + 5 * H, T_NMEA_PORT);
}

// Settings -> Wi-Fi -> saved: every network kept in VestaHome.ini (and given to WZC at the start), in sight or not;
// "Забыть" asks once more ("Точно?"), then WifiKnownSet(ssid, NULL) - out of the ini and of WZC.
static Scroll s_wsList = { 0, 0, 4 * kRowStep };   // a list the finger scrolls (the user 2026-11-08: no pages in the settings)
static int s_wsAsk = -1;   // s_wsAsk: the row whose removal waits for the second press

static int WifiSavedCount()
{
    return s_cfg.wifiCount;
}

static void ScrWifiSaved(HDC dc)
{
    Settings(dc, S_WIFI_SAVED);
    static DWORD s_seen;
    if (s_seen != s_screenTick) {
        s_seen = s_screenTick;
        s_wsAsk = -1;
    }
    WifiInfo w;
    WifiGet(&w);
    int n = s_cfg.wifiCount;
    if (!n)
        EmptyList(dc, 264, 100, 500, L"Сохранённых сетей нет", L"сеть запоминается при подключении с паролем");
    s_wsList.content = n * kRowStep;
    ScrollBegin(dc, s_wsList, 256, 72, 712, 392);
    for (int i = 0; i < n; i++) {   // the rows as every list's: plates of kRowH (the paired phones')
        int y = 72 + i * kRowStep - s_wsList.pos;
        if (y + kRowH <= 72 || y >= 392)
            continue;
        bool cur = (w.state == WIFI_CONNECTED || w.state == WIFI_CONNECTING) && !wcscmp(s_cfg.wifiSsid[i], w.ssid);
        int rssi = 0;
        bool seen = false;
        for (int j = 0; j < w.count && !seen; j++)
            if (!wcscmp(w.nets[j].ssid, s_cfg.wifiSsid[i])) {
                seen = true;
                rssi = w.nets[j].rssi;
            }
        wchar_t sub[64];
        if (s_cfg.wifiLocked[i])
            wcscpy(sub, L"пароль от другого ГУ — не используется");
        else if (cur)
            wcscpy(sub, w.state == WIFI_CONNECTED ? L"подключено" : L"подключение…");
        else if (seen)
            _snwprintf(sub, 64, L"рядом · %d дБм", rssi);
        else
            wcscpy(sub, L"не видна");
        RSel(dc, 256, y, 456, kRowH, 10, cur, false, kPanel2);
        T(dc, 272, y + 23, s_cfg.wifiSsid[i], 18, cur ? kOrange : kText, 700, AL_START, 296);
        T(dc, 272, y + 44, sub, 14, cur ? kOrange : s_cfg.wifiLocked[i] ? kStop : kMuted, 400, AL_START, 296);
        Button(dc, 584, y + 9, 112, 34, s_wsAsk == i ? L"Точно?" : L"Забыть", s_wsAsk == i ? B_SEL : B_N, IC_NONE, 13,
               0, A_WIFIDEL, i);
    }
    ScrollEnd(dc, s_wsList, 720, 72, 48, 320, 52);
}

// MCU monitor ("Car -> MCU data", "Diagnostics -> keys"): the proxy's state, every message id with its count,
// age and payload (the latest first), the climate decoded; recording into mcu.log with marks, the same МЕТКА
// goes into can.log too when that one records.
static void ScrTestMcu(HDC dc)
{
    McuStat st[kMcuStatMax];
    int count;
    DWORD total, logBytes;
    bool rec;
    EnterCriticalSection(&s_mcuCs);
    count = s_mcuStatCount;
    memcpy(st, s_mcuStat, count * sizeof(McuStat));
    total = s_mcuTotal;
    rec = s_mcuLog != INVALID_HANDLE_VALUE;
    logBytes = s_mcuLogBytes;
    LeaveCriticalSection(&s_mcuCs);
    // the latest first: what changed when something was done in the car is on top
    for (int i = 1; i < count; i++)
        for (int j = i; j > 0 && (int)(st[j].tick - st[j - 1].tick) > 0; j--) {
            McuStat t = st[j];
            st[j] = st[j - 1];
            st[j - 1] = t;
        }

    wchar_t s[160];
    Frame(dc, L"Данные MCU");
    Panel(dc, 16, 68, 768, 336);   // (its buttons in the bottom strip)
    LONG attach, wraps;
    int state = McuState(&attach, &wraps);
    if (state < 0)
        wcscpy(s, L"Общей памяти нет — VestaHome не смог её создать");
    else if (!state)
        wcscpy(s, L"Proxy не пишет: не установлен или старая версия — установить новую (VestaHomeInstall)");
    else
        _snwprintf(s, 160, L"Proxy: загрузок %d, обработчиков %d · сообщений %u, разных %d", (int)attach, (int)wraps,
                   (unsigned)total, count);
    T(dc, 32, 96, s, 16, state > 0 ? kText : kOrange, 700, AL_START, 736);
    Hvac h;
    if (HvacGet(&h) && HvacChip(s, 160)) {
        if (h.outer > -100) {
            wchar_t o[24];
            _snwprintf(o, 24, L" · за бортом %+d°", h.outer);
            wcscat(s, o);
        }
    } else {
        s[0] = 0;   // no climate: no line (the user 2026-10-24)
    }
    T(dc, 32, 122, s, 15, kMuted, 600, AL_START, 736);
    Sep(dc, 32, 134, 736);
    DWORD now = GetTickCount();
    for (int i = 0; i < count && i < 7; i++) {
        int y = 140 + i * 34;
        wchar_t name[16], meta[32], hex[128];
        _snwprintf(name, 16, L"%04X", (unsigned)st[i].id);
        _snwprintf(meta, 32, L"%u · %u с", (unsigned)st[i].count, (unsigned)((now - st[i].tick) / 1000));
        int n = 0, shown = st[i].len < 14 ? st[i].len : 14;
        for (int k = 0; k < shown && n < 120; k++)
            n += _snwprintf(hex + n, 128 - n, L"%02X ", st[i].data[k]);
        hex[n] = 0;
        if (n + 4 < 128 && shown) {   // and as text when it looks like text (versions, names)
            int printable = 0;
            for (int k = 0; k < shown; k++)
                printable += st[i].data[k] >= 0x20 && st[i].data[k] <= 0x7E;
            if (printable >= 4 && printable * 2 >= shown) {
                wcscat(hex, L" «");
                n = (int)wcslen(hex);
                for (int k = 0; k < shown && n < 124; k++)
                    hex[n++] = st[i].data[k] >= 0x20 && st[i].data[k] <= 0x7E ? (wchar_t)st[i].data[k] : L'.';
                hex[n++] = L'»';
                hex[n] = 0;
            }
        }
        DWORD age = (now - st[i].tick) / 1000;
        T(dc, 32, y + 24, name, 15, age < 3 ? kOrange : kMuted, 700, AL_START, 60);
        T(dc, 96, y + 24, meta, 14, kMuted, 400, AL_START, 110);
        T(dc, 212, y + 24, hex, 14, kText, 400, AL_START, 556);
    }
    if (!count)
        T(dc, 32, 170, L"Сообщений ещё не было", 16, kMuted, 400);
    if (rec)
        _snwprintf(s, 160, L"запись: mcu.log · %u КБ · меток %d", (unsigned)(logBytes / 1024), s_marks);
    else
        wcscpy(s, L"запись выключена");
    TWrap(dc, 96, 436, s, 13, rec ? kOrange : kDim, 400, 216, 2, 16);   // the bottom strip, right of "back"
    Button(dc, 320, 414, 120, 52, L"Запрос", state > 0 ? B_N : B_DIS, IC_NONE, 14, 0, A_MCUREQ);
    Button(dc, 448, 414, 152, 52, rec ? L"Стоп записи" : L"Запись", rec ? B_SEL : B_N, IC_NONE, 15, 0, A_MCULOG);
    Button(dc, 608, 414, 176, 52, L"Метка", rec ? B_N : B_DIS, IC_NONE, 15, 0, A_CANMARK);
}

// "About the system -> Details": what the stock "Info" and its engineering "Software" / "System status" show
// (photos MAIN/Foto/info: SW / MCU / CAN / OS versions, ID, BT, HW, temperatures, steering-wheel keys voltages,
// tuner). Here: what VestaHome knows itself, and the MCU's answers that look like text (versions, names) - asked
// with "Request" (product info, BT address). Which message carries which field is decoded from mcu.log later.
static void McuRequest();

// The stock shell's SW number as its "Info" shows it (27.0.0.<n>): its own string "Itelma_TR8005_14_02_41.25"
// (the user 2026-11-20: "Details" all zero - it went by G3NavHMI.exe's size: our stub's under VestaStub, and
// 4125's shell has 4120's size); the vendor's copy beside our stub first. Read once, in pieces (4 MB). 0 none.
static int ShellSw()
{
    static int s_sw = -1;
    if (s_sw >= 0)
        return s_sw;
    s_sw = 0;
    const wchar_t* path[2] = { L"\\ResidentFlash\\navigation\\app\\G3NavHMI_orig.exe",
                               L"\\ResidentFlash\\navigation\\app\\G3NavHMI.exe" };
    for (int i = 0; i < 2 && !s_sw; i++) {
        HANDLE f = CreateFile(path[i], GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        if (f == INVALID_HANDLE_VALUE)
            continue;
        static unsigned char buf[64 * 1024 + 32];
        DWORD got = 0, keep = 0;
        while (!s_sw && ReadFile(f, buf + keep, 64 * 1024, &got, NULL) && got) {
            DWORD total = keep + got;
            s_sw = FwVersion(buf, (int)total, 2);
            keep = total < 32 ? total : 32;   // (its last 32 bytes on: a string across two pieces)
            memmove(buf, buf + total - keep, keep);
        }
        CloseHandle(f);
    }
    return s_sw;
}

static void ScrInfoMore(HDC dc)
{
    wchar_t s[160], can[64];
    Frame(dc, L"О системе · Подробнее");
    Panel(dc, 16, 68, 444, 396);
    Panel(dc, 472, 68, 312, 396);
    T(dc, 32, 96, L"Версии", 13, kMuted, 700);
    int shellSw = ShellSw();
    if (shellSw)
        _snwprintf(s, 160, L"27.0.0.%d", shellSw);
    else
        wcscpy(s, L"неизвестна");
    OSVERSIONINFO vi;
    memset(&vi, 0, sizeof(vi));
    vi.dwOSVersionInfoSize = sizeof(vi);
    wchar_t os[40] = L"—";
    if (GetVersionEx(&vi))
        _snwprintf(os, 40, L"%u.%u.%u", (unsigned)vi.dwMajorVersion, (unsigned)vi.dwMinorVersion,
                   (unsigned)vi.dwBuildNumber);
    Gps g;
    GpsGet(&g);
    wchar_t canv[16] = L"—";
    if (g.canVer[0])
        _snwprintf(canv, 16, L"%S", g.canVer);
    LONG attach, wraps;
    int st = McuState(&attach, &wraps);
    static DWORD s_asked;   // the MCU's versions and BT address, asked for once on opening (as the stock "Info")
    if (st > 0 && (!s_asked || GetTickCount() - s_asked > 30000)) {
        s_asked = GetTickCount();
        McuRequest();
    }
    EnterCriticalSection(&s_mcuCs);
    McuInfo inf = s_info;
    LeaveCriticalSection(&s_mcuCs);
    wchar_t mcu[48], hw[24], bt[24], id[40];
    if (inf.sw[0] && inf.cs >= 0)
        _snwprintf(mcu, 48, L"%s · CS %04X", inf.sw, inf.cs);
    else
        wcscpy(mcu, inf.sw[0] ? inf.sw : L"—");
    wcscpy(hw, inf.hw[0] ? inf.hw : L"—");
    if (inf.btHave)
        _snwprintf(bt, 24, L"%02X:%02X:%02X:%02X:%02X:%02X", inf.bt[0], inf.bt[1], inf.bt[2], inf.bt[3], inf.bt[4],
                   inf.bt[5]);
    else
        wcscpy(bt, L"—");
    if (inf.id[0])
        _snwprintf(id, 40, L"%s %s %s", inf.id, inf.idDate, inf.idNum);
    else
        wcscpy(id, L"—");
    const wchar_t* rows[9][2] = { { L"Прошивка ГУ (SW)", s }, { L"Windows CE", os }, { L"MCU (радио)", mcu },
                                  { L"Железо (HW)", hw }, { L"CAN-модуль", canv }, { L"Bluetooth-адрес", bt },
                                  { L"ID устройства", id }, { L"Proxy MCU", st > 0 ? L"работает" : L"нет" },
                                  { L"VestaHome", VhVersion() } };
    for (int i = 0; i < 9; i++) {
        int y = 100 + i * 33;
        T(dc, 32, y + 22, rows[i][0], 15, kMuted, 400);
        T(dc, 444, y + 22, rows[i][1], 15, kText, 700, AL_END, 260);
    }
    Button(dc, 96, 412, 168, 44, L"Запросить", st > 0 ? B_N : B_DIS, IC_REFRESH, 14, 22, A_MCUREQ);   // (right of "back")
    Button(dc, 276, 412, 168, 44, L"Данные MCU", B_N, IC_NONE, 14, 0, A_SCREEN, S_TEST_MCU);

    T(dc, 488, 96, L"Состояние", 13, kMuted, 700);
    MEMORYSTATUS ms;
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatus(&ms);
    wchar_t ram[40], up[24], temp[32], sw[32], field[16], gps[80];
    _snwprintf(ram, 40, L"%u из %u МБ", (unsigned)(ms.dwAvailPhys >> 20), (unsigned)(ms.dwTotalPhys >> 20));
    DWORD m = GetTickCount() / 60000;
    _snwprintf(up, 24, L"%u ч %02u мин", (unsigned)(m / 60), (unsigned)(m % 60));
    // System status (0x7905; the MCU sends it every 2 s for a while after start, not always)
    if (inf.sys[0x0A] >= 0 || inf.sys[0x06] >= 0)
        _snwprintf(temp, 32, L"плата %d °C · усилитель %d °C", inf.sys[0x0A], inf.sys[0x06]);
    else
        wcscpy(temp, L"—");
    if (inf.sys[0x03] >= 0)
        _snwprintf(sw, 32, L"%d · %d", inf.sys[0x03], inf.sys[0x0E]);
    else
        wcscpy(sw, L"—");
    if (inf.sys[0x07] >= 0)
        _snwprintf(field, 16, L"%d дБ", inf.sys[0x07]);
    else
        wcscpy(field, L"—");
    _snwprintf(gps, 80, L"%s · %d спутн.", GpsFresh(g, 5000) ? L"фикс" : L"время по GPS недоступно", g.sats);
    CanStatusText(can, 64);
    const wchar_t* right[7][2] = { { L"Память свободно", ram }, { L"Работает", up }, { L"GPS", gps }, { L"CAN", can },
                                   { L"Температура", temp }, { L"Кнопки руля (покой)", sw },
                                   { L"Поле тюнера", field } };
    for (int i = 0; i < 7; i++) {
        int y = 100 + i * 41;
        T(dc, 488, y + 18, right[i][0], 13, kMuted, 400);
        T(dc, 488, y + 37, right[i][1], 15, kText, 700, AL_START, 284);
    }
    Button(dc, 488, 412, 136, 44, L"Лицензии", B_N, IC_INFO, 13, 22, A_SCREEN, S_LICENSES);
    Button(dc, 632, 412, 136, 44, L"Поддержали", B_N, IC_STAR, 13, 22, A_SCREEN, S_THANKS);
}

// "Details -> Licenses and services": what VestaHome brings that the unit's firmware doesn't have - the libraries
// and their licenses - and the internet services it uses (Open-Meteo's CC BY 4.0 credit among them). A window
// over the screen; the text scrolls with the finger or the arrows.
struct LicItem { const wchar_t* title; const wchar_t* text; };
static const LicItem kLicenses[] = {
    { L"Библиотеки (в составе VestaHome)", NULL },
    { L"BearSSL 0.6", L"Thomas Pornin · лицензия MIT · bearssl.org. HTTPS: погода Яндекса, база камер." },
    { L"Корневые сертификаты Mozilla", L"Mozilla CA Certificate Store · MPL 2.0. Проверка серверов HTTPS "
                                      L"(часть сертификатов, встроена в программу)." },
    { L"stb_image 2.30", L"Sean Barrett · public domain (или MIT) · github.com/nothings/stb. Значки скина PNG, "
                         L"фото PNG, BMP и прогрессивные JPEG." },
    { L"TJpgDec R0.03", L"ChaN · свободная лицензия (с сохранением уведомления об авторстве) · elm-chan.org. "
                        L"Фото JPEG: уменьшение при чтении." },
    { L"BASS 2.4.17 (Windows CE)", L"Un4seen Developments · бесплатно для некоммерческого использования · "
                                   L"un4seen.com. Музыка и интернет-радио." },
    { L"BASSFLAC 2.4.4, BASSALAC 2.4, BASSOPUS 2.4.2, BASSWV 2.4.7", L"Un4seen Developments · дополнения к BASS. "
                                                                    L"FLAC, Apple Lossless, Opus, WavPack." },
    { L"BASS_AAC 2.4.5, BASS_MPC 2.4.1.3", L"MaresWEB · дополнения к BASS. AAC и M4A, Musepack." },
    { L"tags.dll (18)", L"Дополнение к BASS. Названия, исполнители и альбомы из тегов треков." },
    { L"Шрифт Golos Text", L"The Golos Text Project Authors · SIL Open Font License 1.1 · github.com/googlefonts/golos-text. "
                           L"Весь текст VestaHome (папка Fonts, там же OFL.txt)." },
    { L"Значки Lucide 1.61", L"Lucide Contributors · лицензия ISC (части из Feather — MIT) · lucide.dev. Значки "
                             L"VestaHome (папка Skin)." },
    { L"Сервисы", NULL },
    { L"Open-Meteo.com", L"Weather data by Open-Meteo.com, CC BY 4.0. Погода по умолчанию, без ключа." },
    { L"OpenWeatherMap", L"openweathermap.org. Погода, если выбрана и задан свой ключ." },
    { L"Яндекс Погода", L"yandex.ru/pogoda. Погода, если выбрана и задан свой ключ (тариф «погода для умного "
                        L"дома», до 30 запросов в день)." },
    { L"speedcamonline.ru", L"База камер скорости: скачивается по кнопке в настройках камер." },
    { L"Интернет-радио", L"Потоки станций из radio.txt принадлежат самим станциям." },
    { L"Свои виджеты", L"Загружают адреса из своих .ini (url=) через HTTPS VestaHome; данные принадлежат их источникам. "
                       L"Пример probki.ini - баллы Яндекс Пробок (неофициальный адрес export.yandex.ru)." },
    { L"Из прошивки ГУ (не входит в VestaHome)", NULL },
    { NULL, L"Видео — GStreamer штатного плеера; Bluetooth — стек FLC оболочки; радио, звук и кнопки — MCU через "
            L"SPIPowerMgr; шрифт Tahoma; Windows CE 6." },
};
// "Details -> Supported" (the user 2026-11-16): who supports the development on Boosty, special thanks, where to support
static const LicItem kThanks[] = {
    { L"Поддержали разработку на Boosty", NULL },
    { L"Дмитрий", L"" },
    { L"Insaf S.", L"" },
    { L"Александр М.", L"" },
    { L"Алексей", L"" },
    { L"Hobta Evgen", L"" },
    { L"Roman R.", L"" },
    { L"Дмитрий П.", L"" },
    { L"outerdusk", L"" },
    { L"Особое спасибо", NULL },
    { L"Сергей Ludo13", L"За тесты." },
    { L"Поддержать проект", NULL },
    { NULL, L"boosty.to/vesta_home" },
};
enum { kLicTop = 104, kLicBottom = 420 };
static Scroll s_licList = { 0, 0, 160 }, s_thanksList = { 0, 0, 160 };   // the text: its height known after a frame

// A window of headings, titles and texts over the screen (a title with an empty text: the title alone); it scrolls
static void TextWindow(HDC dc, const wchar_t* title, const LicItem* items, int n, Scroll& list)
{
    Under(dc, 153);
    Zone(0, 0, 800, 480, A_BACK);   // a tap beside the window closes it
    Zone(40, 40, 720, 400, A_NONE);
    Panel(dc, 40, 40, 720, 400, 16);
    T(dc, 64, 82, title, 17, kText, 700);
    Button(dc, 696, 52, 52, 44, NULL, B_N, IC_CLOSE, 15, 24, A_BACK);
    Sep(dc, 56, kLicTop - 6, 688);
    ScrollBegin(dc, list, 56, kLicTop, 704, kLicBottom);
    int y = kLicTop + 8 - list.pos;
    for (int i = 0; i < n; i++) {
        const LicItem& it = items[i];
        if (!it.text) {   // a section's heading
            y += i ? 14 : 0;
            T(dc, 64, y + 14, it.title, 13, kOrange, 700);
            y += 24;
            continue;
        }
        if (it.title) {
            T(dc, 64, y + 18, it.title, 17, kText, 700, AL_START, 620);
            y += 24;
        }
        if (it.text[0])
            y += TWrap(dc, 64, y + 16, it.text, 14, kMuted, 400, 620, 6, 19) * 19;
        y += 8;
    }
    list.content = y + list.pos - kLicTop;
    ScrollEnd(dc, list, 712, kLicTop, 40, kLicBottom - kLicTop, 60);
}

static void ScrLicenses(HDC dc)
{
    TextWindow(dc, L"Лицензии и сервисы", kLicenses, sizeof(kLicenses) / sizeof(kLicenses[0]), s_licList);
}

static void ScrThanks(HDC dc)
{
    TextWindow(dc, L"Поддержали разработку", kThanks, sizeof(kThanks) / sizeof(kThanks[0]), s_thanksList);
}

// "Request": product info and the BT address, as the stock "Info" asks for them (FUN_00679190); the answers come
// back as MCU messages and show up in the monitor and on "Details".
static void McuRequest()
{
    if (McuSend0("SPApi_SendRadioRequestProductInfo")) {
        McuSend0("SPApi_SendRadioRequestBTAddress");
        wcscpy(s_status, L"запрос сведений → MCU");
    }
}
