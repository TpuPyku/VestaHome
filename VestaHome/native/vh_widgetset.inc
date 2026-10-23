// vh_widgetset.inc - part of home.cpp (included there, in order): widget settings, the main screen's widgets.

// ---- widgets' settings (opened from a widget's caption): sub screens, rows of 56 px over the whole width

static wchar_t s_coordText[48];   // the home point's coordinates while they are typed

// Row: label and a second line; the control is drawn by the caller to the right of `right`. With an action the
// left part is a button of its own.
static void WRow(HDC dc, int y, const wchar_t* label, const wchar_t* sub, Action a = A_NONE, int arg = 0,
                 int right = 520, bool last = false)
{
    bool down = a != A_NONE && Zone(24, y + 2, right - 24, 52, a, arg);
    if (down)
        R(dc, 24, y + 2, right - 24, 52, 10, kOrange);
    T(dc, 36, y + (sub ? 26 : 35), label, 19, down ? kInk : kText, 700, AL_START, right - 52);
    if (sub)
        T(dc, 36, y + 47, sub, 15, down ? kInk : kMuted, 400, AL_START, right - 52);
    if (a != A_NONE)
        Ico(dc, IC_NEXT, right - 36, y + 14, 28, down ? kInk : kDim);
    if (!last)
        Sep(dc, 24, y + 55, 752);
}

static void ScrWSetWeather(HDC dc)
{
    static const wchar_t* prov[W_COUNT] = { L"Open-Meteo", L"OpenWeather", L"Яндекс" };
    Frame(dc, L"Погода · Настройки");
    Panel(dc, 16, 64, 768, 400);
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
    if (s_cfg.homeLat || s_cfg.homeLon)
        _snwprintf(s, 120, L"%.5f, %.5f", s_cfg.homeLat, s_cfg.homeLon);
    else
        wcscpy(s, L"не заданы — ввести или взять из GPS");
    WRow(dc, 292, L"Координаты дома", s, A_KEYS, K_HOMECOORD, 568);
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
    T(dc, 36, 440, om ? L"Обновляется раз в 30 мин. Weather data by Open-Meteo.com (CC BY 4.0)"
                      : L"Обновляется раз в 30 мин.", 13, kDim, 400, AL_START, 728);
}

static void RegionsText(wchar_t* s, int n);

static void ScrWSetSpeed(HDC dc)
{
    static const wchar_t* dist[4] = { L"По скорости", L"200 м", L"400 м", L"600 м" };
    Frame(dc, L"Скорость и камеры");
    Panel(dc, 16, 64, 768, 400);
    wchar_t s[160], label[48];
    WRow(dc, 68, L"Предупреждать о камерах", L"знак на виджете и в верхнем баре", A_NONE, 0, 640);
    Toggle(dc, 688, 78, T_CAM_ALERT);
    static const wchar_t* snd[5] = { L"Выкл", L"Всегда", L"> лимит", L"+10", L"+20" };
    WRow(dc, 124, L"Звуковой сигнал", L"всегда или если скорость выше лимита камеры", A_NONE, 0, 340);
    Seg(dc, 352, 130, 416, 44, snd, 5, G_CAMSND, 14);
    WRow(dc, 180, L"Дистанция", L"по скорости: за 10 с, 100–500 м", A_NONE, 0, 340);
    Seg(dc, 352, 186, 416, 44, dist, 4, G_CAMDIST, 14);
    wchar_t path[MAX_PATH];
    CamPath(path);
    if (GetFileAttributes(path) == 0xFFFFFFFF)
        _snwprintf(s, 160, L"%s — нет файла", s_cfg.camFile);
    else if (!s_camLoaded)
        _snwprintf(s, 160, L"%s · загрузится по GPS", s_cfg.camFile);
    else
        _snwprintf(s, 160, L"%s · рядом %d из %d", s_cfg.camFile, s_camCount, s_camTotal);
    WRow(dc, 236, L"Файл камер (iGO / Navitel)", s, A_CAMFILE, 0, 776);
    wchar_t port[16], baud[16];
    GpsPortText(port, 16);
    GpsBaudText(baud, 16);
    _snwprintf(label, 48, L"Порт GPS · %s · %s", port, baud);
    GpsStatusText(s, 160);
    WRow(dc, 292, label, s, A_SCREEN, S_SET_CAR, 776);   // set on "Car" for the whole shell
    WRow(dc, 348, L"Время по GPS", L"часы по спутникам, если расходятся больше чем на 30 с", A_NONE, 0, 640);
    Toggle(dc, 688, 358, T_GPS_TIME);
    wchar_t regs[160];
    RegionsText(regs, 160);
    if (s_cfg.camUpdated)
        _snwprintf(s, 160, L"%s · обновлено %02d.%02d", regs, s_cfg.camUpdated % 100, s_cfg.camUpdated / 100 % 100);
    else
        wcscpy(s, regs);
    WRow(dc, 404, L"Скачать базу камер (speedcamonline.ru)", s, A_SCREEN, S_CAMDL, 776, true);
}


// One line on the CAN module's stream: the car's speed and the steering angle when they come.
static void CanStatusText(wchar_t* s, int n)
{
    Gps g;
    GpsGet(&g);
    DWORD now = GetTickCount();
    bool spd = g.carTick && now - g.carTick < 5000, ang = g.angTick && now - g.angTick < 5000;
    if (g.status != GPS_OPEN)
        _snwprintf(s, n, L"порт %s не открыт", g.port[0] ? g.port : s_cfg.gpsPort);
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
    enum { H = 48 };
    wchar_t port[16], baud[16], gps[160], can[64];
    GpsPortText(port, 16);
    GpsBaudText(baud, 16);
    GpsStatusText(gps, 160, true);
    CanStatusText(can, 64);
    Settings(dc, S_SET_CAR);
    SetRow(dc, 64, L"Порт GPS · CAN", gps, false, A_NONE, 0, H);
    Stepper(dc, 512, 68, 248, port, 40, 19, A_GPSPORT);
    SetRow(dc, 112, L"Скорость порта", L"по умолчанию 115200", false, A_NONE, 0, H);
    Stepper(dc, 512, 116, 248, baud, 40, 19, A_GPSBAUD);
    SetRow(dc, 160, L"Данные CAN", can, false, A_SCREEN, S_TEST_CAN, H);
    Chevron(dc, 728, 168);
    wchar_t mcu[80];
    LONG attach, wraps;
    int st = McuState(&attach, &wraps);
    if (st < 0)
        wcscpy(mcu, L"нет общей памяти");
    else if (!st)
        wcscpy(mcu, L"proxy SPIPowerMgr не установлен или старый");
    else
        _snwprintf(mcu, 80, L"proxy работает · сообщений %u", (unsigned)s_mcuTotal);
    SetRow(dc, 208, L"Данные MCU", mcu, false, A_SCREEN, S_TEST_MCU, H);
    Chevron(dc, 728, 216);
    SetRow(dc, 256, L"Камера заднего хода — своя", L"видео и разметка VestaHome", false, A_NONE, 0, H);
    Button(dc, 552, 262, 124, 40, L"Показать", B_N, IC_NONE, 14, 0, A_RVCVIEW);   // by hand; a tap closes it
    Toggle(dc, 688, 262, T_RVC_OWN);
    wchar_t tank[16];
    _snwprintf(tank, 16, L"%d Л", s_cfg.tank);
    SetRow(dc, 304, L"Объём бака", L"для запаса хода", false, A_NONE, 0, H);
    Stepper(dc, 560, 308, 200, tank, 40, 19, A_TANK);
    wchar_t nm[64];   // the navigators' COM6 (vh_gps.inc)
    if (s_nmeaState < 0)
        _snwprintf(nm, 64, L"COM%d не загрузился (ошибка %lu)", kNmeaCom, s_nmeaErr);
    else
        _snwprintf(nm, 64, L"COM%d · только NMEA%s", kNmeaCom, s_nmeaState > 0 ? L" · работает" : L"");
    SetRow(dc, 352, L"Порт для навигаторов", nm, true, A_NONE, 0, H);
    Toggle(dc, 688, 358, T_NMEA_PORT);
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
    Panel(dc, 16, 68, 768, 396);
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
        wcscpy(s, L"Климат: ещё не приходил (0x61xx)");
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
    T(dc, 32, 432, s, 14, rec ? kOrange : kDim, 400, AL_START, 280);
    Button(dc, 320, 400, 120, 52, L"Запрос", state > 0 ? B_N : B_DIS, IC_NONE, 14, 0, A_MCUREQ);
    Button(dc, 448, 400, 152, 52, rec ? L"Стоп записи" : L"Запись", rec ? B_SEL : B_N, IC_NONE, 15, 0, A_MCULOG);
    Button(dc, 608, 400, 160, 52, L"Метка", rec ? B_N : B_DIS, IC_NONE, 15, 0, A_CANMARK);
}

// "About the system -> Details": what the stock "Info" and its engineering "Software" / "System status" show
// (photos MAIN/Foto/info: SW / MCU / CAN / OS versions, ID, BT, HW, temperatures, steering-wheel keys voltages,
// tuner). Here: what VestaHome knows itself, and the MCU's answers that look like text (versions, names) - asked
// with "Request" (product info, BT address). Which message carries which field is decoded from mcu.log later.
static void McuRequest();

static void ScrInfoMore(HDC dc)
{
    wchar_t s[160], can[64];
    Frame(dc, L"О системе · Подробнее");
    Panel(dc, 16, 68, 444, 396);
    Panel(dc, 472, 68, 312, 396);
    T(dc, 32, 96, L"Версии", 13, kMuted, 700);
    WIN32_FIND_DATA fd;
    HANDLE f = FindFirstFile(L"\\ResidentFlash\\navigation\\app\\G3NavHMI.exe", &fd);
    if (f != INVALID_HANDLE_VALUE) {
        FindClose(f);
        _snwprintf(s, 160, L"%s", fd.nFileSizeLow == 4077056 ? L"27.0.0.4120" : L"неизвестна");
    } else {
        wcscpy(s, L"—");
    }
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
    Button(dc, 32, 412, 200, 44, L"Запросить", st > 0 ? B_N : B_DIS, IC_REFRESH, 14, 22, A_MCUREQ);
    Button(dc, 244, 412, 200, 44, L"Данные MCU", B_N, IC_NONE, 14, 0, A_SCREEN, S_TEST_MCU);

    T(dc, 488, 96, L"Состояние", 13, kMuted, 700);
    MEMORYSTATUS ms;
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatus(&ms);
    wchar_t ram[40], up[24], temp[32], sw[32], field[16], gps[40];
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
    _snwprintf(gps, 40, L"%s · %d спутн.", GpsFresh(g, 5000) ? L"фикс" : L"нет фикса", g.sats);
    CanStatusText(can, 64);
    const wchar_t* right[7][2] = { { L"Память свободно", ram }, { L"Работает", up }, { L"GPS", gps }, { L"CAN", can },
                                   { L"Температура", temp }, { L"Кнопки руля (покой)", sw },
                                   { L"Поле тюнера", field } };
    for (int i = 0; i < 7; i++) {
        int y = 100 + i * 41;
        T(dc, 488, y + 18, right[i][0], 13, kMuted, 400);
        T(dc, 488, y + 37, right[i][1], 15, kText, 700, AL_START, 284);
    }
    Button(dc, 488, 412, 280, 44, L"Лицензии и сервисы", B_N, IC_INFO, 13, 22, A_SCREEN, S_LICENSES);
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
enum { kLicTop = 104, kLicBottom = 420 };
static Scroll s_licList = { 0, 0, 160 };   // the text: its height known after a frame

static void ScrLicenses(HDC dc)
{
    Under(dc, 153);
    Zone(0, 0, 800, 480, A_BACK);   // a tap beside the window closes it
    Zone(40, 40, 720, 400, A_NONE);
    Panel(dc, 40, 40, 720, 400, 16);
    T(dc, 64, 82, L"Лицензии и сервисы", 17, kText, 700);
    Button(dc, 696, 52, 52, 44, NULL, B_N, IC_CLOSE, 15, 24, A_BACK);
    Sep(dc, 56, kLicTop - 6, 688);
    ScrollBegin(dc, s_licList, 56, kLicTop, 704, kLicBottom);
    int y = kLicTop + 8 - s_licList.pos;
    for (int i = 0; i < (int)(sizeof(kLicenses) / sizeof(kLicenses[0])); i++) {
        const LicItem& it = kLicenses[i];
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
        y += TWrap(dc, 64, y + 16, it.text, 14, kMuted, 400, 620, 6, 19) * 19 + 8;
    }
    s_licList.content = y + s_licList.pos - kLicTop;
    ScrollEnd(dc, s_licList, 712, kLicTop, 40, kLicBottom - kLicTop, 60);
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
