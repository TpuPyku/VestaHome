// vh_sound.inc - part of home.cpp (included there, in order): sound settings.

// ---- sound: the stock shell's real controls (PROJECT.md): a 3-band equalizer -6..6 (CAudioEffectDlg) and
// balance/fader -8..8 (CAudioFieldDlg), sent to the MCU as G3NavHMI sends them. The MCU keeps them itself; what
// VestaHome shows is what it last sent (VestaHome.ini).
struct EqPreset { const wchar_t* name; int v[3]; };
static const EqPreset kEqPresets[5] = { { L"Ровно", { 0, 0, 0 } }, { L"Поп", { -1, 2, 1 } },
                                        { L"Рок", { 4, -1, 3 } }, { L"Джаз", { 2, 1, 3 } },
                                        { L"Голос", { -2, 3, 1 } } };
static const int kEqBand[3] = { 2, 1, 0 };   // low, mid, high -> the MCU's band (0 = high, OnTimerHighSlot)

static DWORD s_sndSentTick;   // our last EQ / balance: the MCU's echoes of older values are not taken back meanwhile

// The MCU takes the values only on its own sound page (Ghidra + the shell's logs, 2026-10-29; sheet 17 item 10:
// sent from the radio / BT, all ignored): the shell goes "back" to the main menu (0x30, MainMenuKey), "settings"
// 0x13 (-> 0x3903), then the page - 0xE6 equalizer (-> 0x4102 00 10), 0xE4 balance (-> 0x4102 00 0C; from the
// other sound page directly). The values wait until the page is confirmed, then the last ones go (a slider's steps
// thinned as the shell's 0.5 s timer); a balance slider pressed (0x97) before its position and let go after.
// No page in 1.5 s: the way again, 3 times at most.
enum { TIMER_SOUND = 25 };
static int   s_sndWant;     // the page wanted (0: idle)
static int   s_sndPhase;    // 1 to the settings (MainMenuKey 0x13), 2 the page's key sent, 3 on the page
static int   s_sndTries;
static DWORD s_sndT;        // the phase's start
static int   s_sndDirty;    // bits 0..2 the bands, 4 the balance

static void SoundFlush()   // on the page: what changed
{
    for (int i = 0; i < 3; i++)
        if ((s_sndDirty & (1 << i)) && s_sndWant == 0xE6) {
            s_sndDirty &= ~(1 << i);
            if (McuSend2("SPApi_SendRadioAudioHMLData", kEqBand[i], s_cfg.eq[i] + 6))
                wcscpy(s_status, L"эквалайзер → MCU");
        }
    if ((s_sndDirty & 16) && s_sndWant == 0xE4) {
        s_sndDirty &= ~16;
        McuKey(0x97, false);   // the slider pressed
        // the MCU's fader runs the other way from our screen's (HU 2026-10-20: front and rear were swapped)
        if (McuSend2("SPApi_SendRadioAudioBalanceFaderData", s_cfg.balance + 8, 8 - s_cfg.fader))
            wcscpy(s_status, L"баланс → MCU");
        McuKey(0x97, false);   // ... and let go
    }
}

static void SoundPageKey()
{
    McuKey(s_sndWant, false);
    s_sndPhase = 2;
    s_sndT = GetTickCount();
}

static void SoundStep()   // TIMER_SOUND, every 100 ms
{
    DWORD now = GetTickCount();
    if (!s_sndWant) {
        int next = s_sndDirty & 7 ? 0xE6 : s_sndDirty & 16 ? 0xE4 : 0;   // the other page's values left
        if (!next) {
            KillTimer(s_wnd, TIMER_SOUND);
            return;
        }
        s_sndWant = next;
        s_sndPhase = s_sndTries = 0;
    }
    if (s_sndPageNow == s_sndWant) {   // there: the values, thinned
        if (s_sndPhase != 3 || now - s_sndT >= 500) {
            s_sndPhase = 3;
            s_sndT = now;
            SoundFlush();
            if (!(s_sndDirty & (s_sndWant == 0xE6 ? 7 : 16)))
                s_sndWant = 0;
        }
        return;
    }
    if (s_sndPhase == 1 && (s_mkKey || (LONG)(s_settingsTick - s_sndT) < 0) && now - s_sndT < 4000)
        return;   // "back"s and "settings" going, its 0x3903 awaited
    if (s_sndPhase == 1 && (LONG)(s_settingsTick - s_sndT) >= 0) {
        SoundPageKey();
        return;
    }
    if (s_sndPhase == 2 && now - s_sndT < 1500)
        return;   // the page's answer awaited
    if (s_sndPhase && ++s_sndTries >= 3) {
        VLog(L"звук: MCU не открыл страницу 0x%02X - значения не применены", s_sndWant);
        wcscpy(s_status, L"MCU не открыл страницу звука");
        s_sndDirty &= s_sndWant == 0xE6 ? ~7 : ~16;
        s_sndWant = 0;
        return;
    }
    if (s_sndPageNow) {   // on the other sound page: straight there
        SoundPageKey();
        return;
    }
    s_sndPhase = 1;
    s_sndT = now;
    MainMenuKey(0x13);
}

static void SoundGo(int page, int dirty)
{
    s_sndDirty |= dirty;
    s_sndSentTick = GetTickCount();
    if (!s_sndWant) {
        s_sndWant = page;
        s_sndPhase = s_sndTries = 0;
    }
    SetTimer(s_wnd, TIMER_SOUND, 100, NULL);
}

static void EqSend(int i)
{
    SoundGo(0xE6, 1 << i);
}

static void EqSet(int arg)   // band * 16 + value + 6
{
    int band = arg / 16, v = arg % 16 - 6;
    if (band < 0 || band > 2 || s_cfg.eq[band] == v)
        return;
    s_cfg.eq[band] = v;
    SaveCfg();
    EqSend(band);
}

static void EqPresetSet(int p)
{
    for (int i = 0; i < 3; i++)
        if (s_cfg.eq[i] != kEqPresets[p].v[i]) {
            s_cfg.eq[i] = kEqPresets[p].v[i];
            EqSend(i);
        }
    SaveCfg();
}

static void BalanceSend()
{
    SoundGo(0xE4, 16);
}

static void BalanceStep(bool fader, int dir)   // dir 0 = both to the centre
{
    int* v = fader ? &s_cfg.fader : &s_cfg.balance;
    if (!dir) {
        s_cfg.balance = s_cfg.fader = 0;
    } else {
        int n = *v + dir;
        if (n < -8 || n > 8)
            return;
        *v = n;
    }
    SaveCfg();
    BalanceSend();
}

// The car from above on the balance screen: a tap or a drag puts the point there (as in the stock shell's
// CAudioFieldDlg). The point's place: x = kBalX + balance * kBalDX, y = kBalY + fader * kBalDY.
enum { kBalX = 376, kBalY = 230, kBalDX = 10, kBalDY = 18 };

static int BalRound(int d, int step)
{
    int v = (d + (d < 0 ? -step / 2 : step / 2)) / step;
    return v < -8 ? -8 : v > 8 ? 8 : v;
}

// A point of the screen -> balance and fader; sent when they change. save: the finger is up.
static void BalanceAt(int x, int y, bool save)
{
    int b = BalRound(x - kBalX, kBalDX), f = BalRound(y - kBalY, kBalDY);
    if (b != s_cfg.balance || f != s_cfg.fader) {
        s_cfg.balance = b;
        s_cfg.fader = f;
        BalanceSend();
        Redraw();
    }
    if (save)
        SaveCfg();
}

static McuSound McuSoundGet()   // what the MCU reported last (its decoding thread writes it)
{
    EnterCriticalSection(&s_mcuCs);
    McuSound m = s_mcuSnd;
    LeaveCriticalSection(&s_mcuCs);
    return m;
}

// Equalizer: presets, three sliders (a tap on a slider sets the value there), balance and volumes as buttons.
static void ScrSetSound(HDC dc)
{
    static const wchar_t* bands[3] = { L"Низкие", L"Средние", L"Высокие" };
    static DWORD s_seen;   // opened: the shell's key of its equalizer screen - the MCU takes the changes only after it
    if (s_seen != s_screenTick) {   // (HU 2026-10-18-2: ours alone had no answer, the shell's 0xE6 first had)
        s_seen = s_screenTick;
        McuKey(0xE6, false);
    }
    Settings(dc, S_SET_SOUND);
    T(dc, 264, 92, L"Эквалайзер", 19, kText, 700);
    for (int p = 0; p < 5; p++) {
        bool cur = s_cfg.eq[0] == kEqPresets[p].v[0] && s_cfg.eq[1] == kEqPresets[p].v[1] &&
                   s_cfg.eq[2] == kEqPresets[p].v[2];
        Button(dc, 256 + p * 84, 104, 78, 40, kEqPresets[p].name, cur ? B_SEL : B_N, IC_NONE, 13, 0, A_EQPRESET, p);
    }
    enum { Y = 186, H = 132 };
    for (int i = 0; i < 3; i++) {
        int x = 304 + i * 104;
        wchar_t v[8];
        _snwprintf(v, 8, L"%+d", s_cfg.eq[i]);
        VSlider(dc, x, Y, H, (s_cfg.eq[i] + 6) * 100 / 12, bands[i], s_cfg.eq[i] ? v : L"0");
        for (int k = 0; k <= 12; k++)   // 13 tap zones down the slider, top = +6
            Zone(x - 30, Y - 6 + (12 - k) * H / 12, 60, H / 12 + 1, A_EQ, i * 16 + k);
    }
    Button(dc, 600, 160, 168, 64, L"Баланс", B_N, IC_EQ, 15, 28, A_SCREEN, S_SET_BALANCE);
    Button(dc, 600, 236, 168, 64, L"Громкость", B_N, IC_VOL, 15, 28, A_SCREEN, S_SET_VOLUME);
    // (loudness and the "volume algorithm" switches removed: on the unit they changed nothing, HU sheet 30 p. 4)
}

static void ScrSetBalance(HDC dc)
{
    static DWORD s_seen;   // opened: the shell's key of its balance screen (as 0xE6 for the equalizer)
    if (s_seen != s_screenTick) {
        s_seen = s_screenTick;
        McuKey(0xE4, false);
    }
    Settings(dc, S_SET_BALANCE);
    R(dc, 276, 76, 200, 316, 40, kPanel2);   // the car from above, four seats
    R(dc, 296, 126, 70, 76, 10, kInk);
    R(dc, 386, 126, 70, 76, 10, kInk);
    R(dc, 296, 256, 70, 76, 10, kInk);
    R(dc, 386, 256, 70, 76, 10, kInk);
    Dash(dc, 376, 84, 376, 384, 4, 6, kDim);
    Dash(dc, 284, 230, 468, 230, 4, 6, kDim);
    Zone(276, 76, 200, 316, A_BALXY);   // the whole car: tap or drag
    int cx = kBalX + s_cfg.balance * kBalDX, cy = kBalY + s_cfg.fader * kBalDY;   // + fader = to the rear (to verify)
    Disc(dc, cx, cy, 18, kOrange);
    Disc(dc, cx, cy, 6, kInk);
    wchar_t b[24], f[24];
    if (s_cfg.balance)
        _snwprintf(b, 24, L"%s %d", s_cfg.balance < 0 ? L"Лево" : L"Право", abs(s_cfg.balance));
    else
        wcscpy(b, L"Центр");
    if (s_cfg.fader)
        _snwprintf(f, 24, L"%s %d", s_cfg.fader < 0 ? L"Перед" : L"Зад", abs(s_cfg.fader));
    else
        wcscpy(f, L"Центр");
    T(dc, 504, 96, L"Баланс", 19, kText, 700);
    Stepper(dc, 504, 108, 256, b, 48, 20, A_BAL);
    T(dc, 504, 184, L"Фейдер", 19, kText, 700);
    Stepper(dc, 504, 196, 256, f, 48, 20, A_FADE);
    Button(dc, 504, 280, 256, 48, L"В центр", B_N, IC_NONE, 16, 0, A_BALCENTER);
}

// Volumes (the user 2026-10-22, out of the demo) as the stock shell's CAudioVolumeDlg (Ghidra 2026-10-22): the
// navigation's / the keys' volume - its value sent (SendRadioKeyVolData + ext key 0x9E); VestaHome's cap of the
// volume at the start (StartVolumeTick). (The volume by speed: removed 2026-11-12 - the MCU takes the current volume
// from no port of CE, HU sheet 30 p. 12.) (The MCU's start / phone / TA volumes: not here - on the
// unit they showed and changed nothing, HU 2026-11-11, the user: not needed.)
static void ScrSetVolume(HDC dc)
{
    static const wchar_t* rows[2][2] = { { L"Навигация и кнопки", L"подсказки, звук нажатий" },
                                         { L"Макс. при включении", L"была громче — убавит до неё" } };
    Settings(dc, S_SET_SOUND);
    for (int i = 0; i < 2; i++) {
        wchar_t v[8];
        if (i == 0)
            _snwprintf(v, 8, s_cfg.keyVol < 0 ? L"—" : L"%d", s_cfg.keyVol);
        else
            _snwprintf(v, 8, s_cfg.startMax ? L"%d" : L"выкл", s_cfg.startMax);
        SetRow(dc, 64 + i * kSetH, rows[i][0], rows[i][1]);
        Stepper(dc, 560, 70 + i * kSetH, 200, v, 44, 20, A_SNDVOL, i * 4 + 2);   // (row * 4 + 1 down, + 3 up)
    }
}

// The stream monitor ("Diagnostics -> CAN / UART", "Car -> Data stream"): the port, bytes, the car's values and
// every sentence type with its count, age and last line; recording into can.log with marks for decoding.
static void ScrTestCan(HDC dc)
{
    Gps g;
    GpsGet(&g);
    Sentence sent[kSentMax];
    int count;
    DWORD bytes, bin, bad, logBytes;
    bool rec;
    EnterCriticalSection(&s_gpsCs);
    count = s_sentCount;
    memcpy(sent, s_sent, count * sizeof(Sentence));
    bytes = g.bytes;          // of the program that reads the port (VestaCore)
    bin = s_streamBin;        // seen only while this program reads it
    bad = (DWORD)g.badLines;
    rec = s_canLog != INVALID_HANDLE_VALUE;
    logBytes = s_canLogBytes;
    LeaveCriticalSection(&s_gpsCs);

    wchar_t s[160], gps[160], can[64];
    _snwprintf(s, 160, L"Поток данных · %s", GpsPortNow());
    Frame(dc, s);
    Panel(dc, 16, 68, 768, 336);   // (its buttons in the bottom strip)
    GpsStatusText(gps, 160);
    CanStatusText(can, 64);
    _snwprintf(s, 160, L"GPS: %s", gps);
    T(dc, 32, 96, s, 16, kText, 700, AL_START, 736);
    _snwprintf(s, 160, L"CAN: %s · принято %u КБ, не текст %u Б, битых строк %u", can, (unsigned)(bytes / 1024),
               (unsigned)bin, (unsigned)bad);
    T(dc, 32, 122, s, 16, kText, 700, AL_START, 736);
    Sep(dc, 32, 134, 736);
    // the busiest types first would jump around; keep the order they first came in, 7 rows
    DWORD now = GetTickCount();
    if (!count)
        T(dc, 32, 170, L"Строк «$» и «@» ещё не было", 16, kMuted, 400);
    for (int i = 0; i < count && i < 7; i++) {
        int y = 140 + i * 34;
        wchar_t name[16], last[104], meta[32];
        MultiByteToWideChar(CP_ACP, 0, sent[i].name[0] ? sent[i].name : "?", -1, name, 16);
        MultiByteToWideChar(CP_ACP, 0, sent[i].last, -1, last, 104);
        DWORD age = (now - sent[i].tick) / 1000;
        _snwprintf(meta, 32, L"%u · %u с", (unsigned)sent[i].count, (unsigned)age);
        T(dc, 32, y + 24, name, 15, age < 5 ? kOrange : kMuted, 700, AL_START, 90);
        T(dc, 128, y + 24, meta, 14, kMuted, 400, AL_START, 100);
        T(dc, 236, y + 24, last, 14, kText, 400, AL_START, 532);
    }
    if (count > 7) {
        _snwprintf(s, 160, L"ещё типов: %d", count - 7);
        T(dc, 32, 392, s, 14, kDim, 400);
    }
    if (rec)
        _snwprintf(s, 160, L"запись: can.log · %u КБ из %u · меток %d", (unsigned)(logBytes / 1024),
                   (unsigned)(kCanLogMax / 1024), s_canMarks);
    else
        wcscpy(s, L"запись выключена: включите и делайте действия в машине, отмечая их МЕТКОЙ");
    TWrap(dc, 96, 436, s, 13, rec ? kOrange : kDim, 400, 330, 2, 16);   // the bottom strip, right of "back"
    Button(dc, 440, 414, 160, 52, rec ? L"Стоп записи" : L"Запись", rec ? B_SEL : B_N, IC_NONE, 15, 0, A_CANLOG);
    Button(dc, 608, 414, 176, 52, L"Метка", rec ? B_N : B_DIS, IC_NONE, 15, 0, A_CANMARK);
}

// The keyboard opened from these screens: which setting it edits.
static void OpenCfgKeys(int which)
{
    switch (which) {
    case K_WKEY:
        OpenKeysFor(s_cfg.weatherKey[s_cfg.weatherProv], 63, L"Ключ API", S_WSET_WEATHER);
        break;
    case K_HOMENAME:
        OpenKeysFor(s_cfg.homeName, 47, L"Домашняя точка", S_WSET_WEATHER);
        break;
    case K_BTNAME:   // the unit's name in Bluetooth: VestaStub writes it before the stack starts (next boot)
        OpenKeysFor(s_cfg.btName, 31, L"Имя ГУ в Bluetooth", S_SET_BT);
        break;
    case K_NETAPN:
        OpenKeysFor(s_cfg.netApn, 63, L"APN", S_SET_NET3G);
        break;
    case K_NETUSER:
        OpenKeysFor(s_cfg.netUser, 31, L"Логин", S_SET_NET3G);
        break;
    case K_NETPASS:
        OpenKeysFor(s_cfg.netPass, 31, L"Пароль", S_SET_NET3G);
        break;
    case K_ODO: case K_SVCKM: case K_SVCDATE: case K_SVCEVERY:   // the trip widget's service (vh_widgetset.inc)
        SvcKeys(which);
        break;
    case K_HOMELAT: case K_HOMELON:   // each its own field of digits; the other one kept
        s_coordWhich = which;
        if (s_cfg.homeLat || s_cfg.homeLon)
            _snwprintf(s_coordText, 48, L"%.5f", which == K_HOMELAT ? s_cfg.homeLat : s_cfg.homeLon);
        else
            s_coordText[0] = 0;
        OpenNumFor(s_coordText, 12, which == K_HOMELAT ? L"Широта дома, градусы (−90…90)"
                   : L"Долгота дома, градусы (−180…180)", S_WSET_WEATHER, NUM_DEC);
        break;
    }
}

// A known network (name + password) into the settings and to the Wi-Fi thread; pass NULL = forget it.
static void WifiKnownSet(const wchar_t* ssid, const wchar_t* pass)
{
    int k = 0;
    while (k < s_cfg.wifiCount && wcscmp(s_cfg.wifiSsid[k], ssid))
        k++;
    if (!pass) {
        if (k < s_cfg.wifiCount) {
            int last = --s_cfg.wifiCount;
            wcscpy(s_cfg.wifiSsid[k], s_cfg.wifiSsid[last]);
            wcscpy(s_cfg.wifiPass[k], s_cfg.wifiPass[last]);
            wcscpy(s_cfg.wifiSealed[k], s_cfg.wifiSealed[last]);
            s_cfg.wifiLocked[k] = s_cfg.wifiLocked[last];
        }
        WifiForget(ssid);
    } else {
        if (k == kWifiKnown)
            return;
        wcsncpy(s_cfg.wifiSsid[k], ssid, 32);
        s_cfg.wifiSsid[k][32] = 0;
        wcsncpy(s_cfg.wifiPass[k], pass, 63);
        s_cfg.wifiPass[k][63] = 0;
        s_cfg.wifiLocked[k] = false;
        if (k == s_cfg.wifiCount)
            s_cfg.wifiCount++;
        WifiRemember(ssid, pass);
    }
    SaveCfg();
}

// A tap on a network: a known or open one connects at once, a new secured one asks its password first.
static void WifiTap(int i)
{
    WifiInfo w;
    WifiGet(&w);
    if (i < 0 || i >= w.count)
        return;
    const WifiNet& n = w.nets[i];
    if (w.state == WIFI_CONNECTED && !wcscmp(w.ssid, n.ssid)) {
        wcscpy(s_status, L"уже подключено");
        return;
    }
    for (int k = 0; k < s_cfg.wifiCount; k++)
        if (!s_cfg.wifiLocked[k] && !wcscmp(s_cfg.wifiSsid[k], n.ssid)) {
            WifiConnect(n.ssid, s_cfg.wifiPass[k]);
            return;
        }
    if (n.sec == WSEC_OPEN) {
        WifiKnownSet(n.ssid, L"");
        WifiConnect(n.ssid, L"");
        return;
    }
    wcscpy(s_wifiTarget, n.ssid);
    s_wifiPass[0] = 0;
    _snwprintf(s_wifiTitle, 64, L"ПАРОЛЬ · %s", n.ssid);
    s_wifiTitle[63] = 0;
    OpenKeysFor(s_wifiPass, 63, s_wifiTitle, S_SET_WIFI);
}

static void WifiForgetTap(int i)
{
    WifiInfo w;
    WifiGet(&w);
    if (i >= 0 && i < w.count)
        WifiKnownSet(w.nets[i].ssid, NULL);
}

// Typed into a setting: coordinates are parsed, the file written, what depends on it restarted.
static void CfgKeysDone()
{
    if (s_keysTarget == s_teBuf) {   // the text editor's line
        TeKeysDone();
        return;
    }
    if (s_keysTarget == s_svcText) {   // the service's km or date
        SvcKeysDone();
        return;
    }
    if (s_keysTarget == s_ncQuery) {   // the radio catalogue's search
        NcSearchDone();
        return;
    }
    if (s_keysTarget == s_wifiPass) {   // the password of a new network: kept and connected with
        if (s_wifiPass[0]) {
            WifiKnownSet(s_wifiTarget, s_wifiPass);
            WifiConnect(s_wifiTarget, s_wifiPass);
        }
        return;
    }
    if (s_keysTarget == s_cfg.netApn || s_keysTarget == s_cfg.netUser || s_keysTarget == s_cfg.netPass)
        s_cfg.netOp = NetOwn();   // typed: the operator's own settings now
    if (s_keysTarget == s_coordText && s_coordText[0]) {   // the latitude or the longitude, in its range
        double v = wcstod(s_coordText, NULL), lim = s_coordWhich == K_HOMELAT ? 90 : 180;
        if (v >= -lim && v <= lim)
            (s_coordWhich == K_HOMELAT ? s_cfg.homeLat : s_cfg.homeLon) = v;
    }
    SaveCfg();
    WeatherRefresh();
}

// The settings shown as switches and choices: s_cfg <-> s_on / s_seg.
static void CfgToControls()
{
    s_on[T_GPS_TIME] = s_cfg.gpsTime;
    s_on[T_W_GPS] = s_cfg.weatherGps;
    s_on[T_CAM_ALERT] = s_cfg.camAlert;
    s_on[T_PB_AUTO] = s_cfg.pbAuto != 0;
    s_seg[G_TIME_FMT] = s_cfg.time12 ? 0 : 1;
    s_seg[G_CAMSND] = !s_cfg.camSound ? 0 : s_cfg.camOver < 0 ? 1 : 2;   // off / always / over (cam.overby)
    s_on[T_CAM_WEEKLY] = s_cfg.camWeekly;
    s_on[T_VIDEO_MOVING] = s_cfg.videoMoving;
    s_on[T_NET_AUTO] = s_cfg.netAuto;
    s_on[T_PHONE_USB] = s_cfg.phoneUsb;
    s_on[T_PHONE_ANY] = s_cfg.phoneAny;
    s_on[T_NMEA_PORT] = s_cfg.nmeaPort;
    s_on[T_WIFI] = s_cfg.wifiOn;
    s_on[T_WIFI_AP_AUTO] = s_cfg.wifiApAuto;
    s_on[T_SLIDES_OPEN] = s_cfg.photoSlides;
    s_on[T_AUTOROTATE] = s_cfg.photoAutoRot;
    s_seg[G_PHOTO_SCALE] = s_cfg.photoFill ? 1 : 0;
    s_seg[G_VIDEO_SCALE] = s_cfg.videoStretch ? 1 : 0;
    s_seg[G_WPROV] = s_cfg.weatherProv;
    s_seg[G_CAMDIST] = 0;
    for (int i = 0; i < 4; i++)
        if (kCamDists[i] == s_cfg.camDist)
            s_seg[G_CAMDIST] = i;
}

static bool IsCfgToggle(int id)
{
    return id == T_GPS_TIME || id == T_W_GPS || id == T_CAM_ALERT ||
           id == T_CAM_WEEKLY || id == T_VIDEO_MOVING || id == T_WIFI ||
           id == T_WIFI_AP_AUTO || id == T_SLIDES_OPEN || id == T_AUTOROTATE || id == T_NMEA_PORT;
}

static void ControlsToCfg()
{
    bool weather = s_cfg.weatherGps != s_on[T_W_GPS] || s_cfg.weatherProv != s_seg[G_WPROV];
    s_cfg.gpsTime = s_on[T_GPS_TIME];
    s_cfg.weatherGps = s_on[T_W_GPS];
    s_cfg.camAlert = s_on[T_CAM_ALERT];
    {   // the beep: off / always / above the camera's limit by 0, 10, 20 km/h
        s_cfg.camSound = s_seg[G_CAMSND] > 0;
        s_cfg.camOver = s_seg[G_CAMSND] == 2 ? s_cfg.camOverBy : -1;
    }
    s_cfg.camWeekly = s_on[T_CAM_WEEKLY];
    s_cfg.videoMoving = s_on[T_VIDEO_MOVING];
    s_cfg.netAuto = s_on[T_NET_AUTO];
    if (s_cfg.phoneUsb != s_on[T_PHONE_USB] || s_cfg.phoneAny != s_on[T_PHONE_ANY]) {
        s_cfg.phoneUsb = s_on[T_PHONE_USB];
        s_cfg.phoneAny = s_on[T_PHONE_ANY];
        PhoneUsbSet(s_cfg.phoneUsb);
    }
    s_cfg.photoSlides = s_on[T_SLIDES_OPEN];
    s_cfg.photoAutoRot = s_on[T_AUTOROTATE];
    if (s_cfg.nmeaPort != s_on[T_NMEA_PORT]) {
        s_cfg.nmeaPort = s_on[T_NMEA_PORT];
        NmeaPortStart(s_cfg.nmeaPort, false);
    }
    if (s_cfg.wifiOn != s_on[T_WIFI]) {
        s_cfg.wifiOn = s_on[T_WIFI];
        WifiEnable(s_cfg.wifiOn);
    }
    if (s_cfg.wifiApAuto != s_on[T_WIFI_AP_AUTO]) {
        s_cfg.wifiApAuto = s_on[T_WIFI_AP_AUTO];
        WifiApSetup(s_cfg.wifiApSsid, s_cfg.wifiApPass, s_cfg.wifiApAuto);
    }
    if (s_cfg.videoStretch != (s_seg[G_VIDEO_SCALE] == 1)) {
        s_cfg.videoStretch = s_seg[G_VIDEO_SCALE] == 1;
        VideoSetStretch(s_cfg.videoStretch);
    }
    s_cfg.weatherProv = s_seg[G_WPROV];
    s_cfg.camDist = kCamDists[s_seg[G_CAMDIST] & 3];
    if (!s_cfg.camAlert)
        s_camNear = -1;
    SaveCfg();
    if (weather)
        WeatherRefresh();
}

// "From GPS": the current fix becomes the home point.
static void HomeFromGps()
{
    Gps g;
    GpsGet(&g);
    if (!GpsFresh(g, 10000)) {
        wcscpy(s_status, L"место по GPS недоступно");
        return;
    }
    s_cfg.homeLat = g.lat;
    s_cfg.homeLon = g.lon;
    SaveCfg();
    WeatherRefresh();
}
