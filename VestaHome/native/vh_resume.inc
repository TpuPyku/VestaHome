// vh_resume.inc - part of home.cpp (included there, in order): resume after a restart, navigation.

// ---- the screen and the source come back after a restart (the unit off and on): the screen as it was (its
// section's main one: a settings page -> Settings), and what played - the radio (the MCU's source), our music
// (the last track), Bluetooth (the phone: when it connects, its music becomes the source and plays). Remembered
// in the ini at most once a minute (CfgSoon), so going round the screens doesn't wear the flash.
static const struct { Screen s; const wchar_t* key; } kUiScreens[] = {
    { S_HOME, L"home" }, { S_RADIO, L"radio" }, { S_AUDIO, L"music" }, { S_NETRADIO, L"netradio" },
    { S_AUDIO_BT, L"bt" }, { S_AUX, L"aux" }, { S_VIDEO, L"video" }, { S_PHOTO, L"photo" }, { S_PHONE, L"phone" },
    { S_APPS, L"apps" }, { S_SETTINGS, L"settings" } };
static DWORD s_bootTick;

static const wchar_t* UiScreenKey()
{
    Screen root = s_histLen ? s_hist[0] : s_screen;
    if (Section(root) == 5)
        root = S_SETTINGS;
    for (int i = 0; i < (int)(sizeof(kUiScreens) / sizeof(kUiScreens[0])); i++)
        if (kUiScreens[i].s == root)
            return kUiScreens[i].key;
    return NULL;   // not a main screen (a player, a call): the last main one stays
}

// What plays now: "music" (ours), "net" (the internet radio), "radio", "bt", "" (another source: the shell's);
// NULL - not known yet.
static const wchar_t* UiSourceKey()
{
    if (s_netOn)
        return L"net";
    if (PlayerPlaying())
        return L"music";
    int vol, mute, src;
    if (!VolGet(&vol, &mute, &src) || src < 0)
        return NULL;
    return src == 0 ? L"radio" : src == 9 ? L"bt" : src == s_auxSrc ? L"aux" : L"";
}

// The start's screen (the user, 2026-10-21): the settings and the programs -> the main screen; the music, video
// and photo browsers -> the player / viewer of the file last played or viewed if it is still there, otherwise the
// main screen; the rest as they were. The file is opened before the first frame (ResumeMedia, after the photos'
// part): the video "parked" (it opens when its screen is in front, VidSync), the photo in the viewer.
static int s_resumeMedia;   // 0 none, 1 the video, 2 the photo
static void ResumeMedia();

static bool FileThere(const wchar_t* dir, const wchar_t* file)
{
    if (!dir[0] || !file[0])
        return false;
    wchar_t p[MAX_PATH];
    _snwprintf(p, MAX_PATH, L"%s%s", dir, file);
    p[MAX_PATH - 1] = 0;
    return GetFileAttributes(p) != 0xFFFFFFFF;
}

static void ResumeScreen()   // at the start
{
    s_mediaLast = !wcscmp(s_cfg.uiSource, L"net") ? 2 : 1;
    Screen s = S_NONE;
    for (int i = 0; i < (int)(sizeof(kUiScreens) / sizeof(kUiScreens[0])); i++)   // the media's tab (ui.media)
        if (!wcscmp(kUiScreens[i].key, s_cfg.uiMedia) && Section(kUiScreens[i].s) == 2)
            s_media = kUiScreens[i].s;
    for (int i = 0; i < (int)(sizeof(kUiScreens) / sizeof(kUiScreens[0])); i++)
        if (!wcscmp(kUiScreens[i].key, s_cfg.uiScreen))
            s = kUiScreens[i].s;
    if (s == S_NONE)
        return;
    Screen over = S_NONE;   // a player opened over its browser
    if (s == S_SETTINGS || s == S_APPS)
        s = S_HOME;
    else if (s == S_AUDIO && !FileThere(s_cfg.musicDir, s_cfg.musicFile))
        s = S_HOME;   // (S_AUDIO is the music's player)
    else if (s == S_VIDEO) {
        if (FileThere(s_cfg.videoDir, s_cfg.videoFile)) {
            over = S_VIDEO_PLAY;
            s_resumeMedia = 1;
        } else {
            s = S_HOME;
        }
    } else if (s == S_PHOTO) {
        if (FileThere(s_cfg.photoDir, s_cfg.photoFile)) {
            over = S_PHOTO_VIEW;
            s_resumeMedia = 2;
        } else {
            s = S_HOME;
        }
    }
    s_screen = s;
    s_screenTick = GetTickCount();
    if (Section(s_screen) == 2)
        s_media = s_screen;
    if (over != S_NONE) {
        Go(over, true);
        ResumeMedia();
    }
    VLog(L"запуск: экран «%s» -> %d", s_cfg.uiScreen, (int)s_screen);
}

// The volume at the start: no louder than 10 (the user, 2026-10-19: the shell's scale 0..30, comfortable 5..15).
// The shell's volume window sets its "default" volume with SPApi_SendRadioAudioVolData(0, v) (CAudioVolumeDlg::
// OnTimerDefaultSlot, Ghidra 2026-10-19; beside it UpdateStartedVol - the volume at power-on); whether it moves
// the volume playing now or only the next start's is to be seen on the unit: the volume the MCU reports after it
// goes to the log. Once, 10 s after the start (the MCU's link up); then again at 15 s only to log the result.

static void StartVolumeTick()
{
    static int s_step;   // 0 waiting, 1 sent, 2 done
    DWORD el = GetTickCount() - s_bootTick;
    int vol, mute, src;
    if (s_step == 0 && el >= 10000) {
        s_step = 2;
        if (s_cfg.startMax > 0 && VolGet(&vol, &mute, &src) && vol > s_cfg.startMax) {   // (Sound -> Volume)
            McuSend3("SPApi_SendRadioAudioVolData", 0, s_cfg.startMax, 0);
            VLog(L"запуск: громкость %d > %d - SendRadioAudioVolData(0, %d)", vol, s_cfg.startMax, s_cfg.startMax);
            s_step = 1;
        }
    } else if (s_step == 1 && el >= 15000) {
        s_step = 2;
        bool have = VolGet(&vol, &mute, &src);
        VLog(L"запуск: громкость после команды - %d (MCU %s)", have ? vol : -1,
             have && vol <= s_cfg.startMax ? L"убавил сейчас" : L"сейчас не убавил: возможно, только при следующем включении");
    }
}

// ---- Bluetooth for one device (btowner.h): the phone or the OBD adapter, remembered (bt.owner); the adapter's
// owner: the phone let go and the stack's own connects off (the proxy's BT_CMD_PHONE), the adapter connected.
static int   s_btoLastAct = BTA_NONE, s_btoTries;
static DWORD s_btoActTick;

static bool BtPhoneOn(const BtState& bt)   // handsfree, music or internet
{
    return bt.hf >= 1 || bt.av >= 1 || bt.pan == BT_PAN_ONLINE || bt.pan == BT_PAN_CONNECTING;
}

static void BtOwnerSet(int owner)   // the user's choice: a phone or the adapter connected by hand
{
    s_btoTries = 0;
    if (owner == s_cfg.btOwner)
        return;
    s_cfg.btOwner = owner;
    s_btoLastAct = BTA_NONE;
    SaveCfg();
    VLog(L"bt: Bluetooth отдан %s", owner == BTO_OBD ? L"OBD-адаптеру (телефон отключаю)" : L"телефону");
    if (owner == BTO_PHONE) {
        ObdStop();
        BtCmdText(BT_CMD_PHONE, L"1");   // the stack's connects as they were
    }
}

static void BtPhoneBack(const wchar_t* why)   // Bluetooth to the phone, the last one connected again
{
    VLog(L"bt: %s - Bluetooth телефону", why);
    BtOwnerSet(BTO_PHONE);
    if (s_cfg.btLast[0]) {
        VLog(L"телефон: подключить %s", s_cfg.btLast);
        BtCmdText(BT_CMD_CONNECT, s_cfg.btLast);
    }
}

static void BtObdConnectNow()
{
    char a[18];
    WideCharToMultiByte(CP_ACP, 0, s_obdAddr, -1, a, 18, NULL, NULL);
    a[17] = 0;
    s_btoTries++;
    s_btoLastAct = BTA_OBD_CONNECT;
    s_btoActTick = GetTickCount();
    VLog(L"obd: подключаю %S (попытка %d)", a, s_btoTries);
    ObdConnect(a);
}

// Bluetooth off / on (Settings -> Bluetooth; the proxy's BT_CMD_POWER, its own thread - off takes up to ~10 s)
static int BtPowerState();

static void BtPowerSet(bool on)   // the user's switch: kept (bt.power), then the stack
{
    int pw = BtPowerState();
    if (pw == BT_POW_GOING_OFF || pw == BT_POW_GOING_ON)
        return;
    s_cfg.btPower = on;
    SaveCfg();
    VLog(L"bt: Bluetooth %s", on ? L"включаю" : L"выключаю");
    BtCmdText(BT_CMD_POWER, on ? L"1" : L"0");
}

static void BtPowerTick(const BtState& bt)   // the start: as it was left (VestaStub starts the stack always)
{
    static bool s_done;
    if (s_done)
        return;
    int pw = BtPowerState();
    if (pw == BT_POW_OFF || (pw == BT_POW_ON && bt.ready)) {
        s_done = true;
        if (s_cfg.btPower != (pw == BT_POW_ON)) {
            VLog(L"bt: в настройках Bluetooth %s - %s", s_cfg.btPower ? L"включён" : L"выключен",
                 s_cfg.btPower ? L"включаю" : L"выключаю");
            BtCmdText(BT_CMD_POWER, s_cfg.btPower ? L"1" : L"0");
        }
    }
}

static void BtOwnerTick()   // once a second, 10 s on from the start
{
    BtState bt;
    if (GetTickCount() - s_bootTick < 10000 || !BtGet(&bt, NULL))
        return;
    BtPowerTick(bt);
    if (!kObdOn) {
        if (s_cfg.btOwner == BTO_OBD)
            BtPhoneBack(L"OBD отключён в этой версии");
        return;
    }
    ObdInfo o;
    ObdGet(&o);
    if (o.state == OBD_ON)
        s_btoTries = 0;
    BtOwnerIn in;
    in.owner = s_cfg.btOwner;
    in.phone = BtPhoneOn(bt);
    in.obdBusy = o.state == OBD_CONNECTING || o.state == OBD_ON;
    in.obdAddr = s_obdAddr[0] != 0;
    in.hold = (s_obdEvSeen >= 0 && GetTickCount() - s_obdSearchTick < 15000) ||
              (s_obdPairTick && GetTickCount() - s_obdPairTick < 30000);
    in.lastAct = s_btoLastAct;
    in.sinceAct = GetTickCount() - s_btoActTick;
    in.tries = s_btoTries;
    switch (BtOwnerStep(in)) {
    case BTA_PHONE_OFF:
        VLog(L"bt: Bluetooth у OBD-адаптера - телефон отключаю%s", in.phone ? L"" : L" (его ещё нет)");
        BtCmdText(BT_CMD_PHONE, L"0");
        s_btoLastAct = BTA_PHONE_OFF;
        s_btoActTick = GetTickCount();
        break;
    case BTA_OBD_CONNECT:
        BtObdConnectNow();
        break;
    case BTA_PHONE_BACK:
        BtPhoneBack(in.obdAddr ? L"OBD не подключился за 3 попытки" : L"OBD-адаптер не выбран");
        break;
    case BTA_OBD_STOP:
        VLog(L"obd: Bluetooth у телефона - адаптер отключаю");
        ObdStop();
        break;
    }
}

// Once a second from the start: 10 s on (the MCU's link up) the source comes back. Bluetooth: the stack itself
// connects the last phone at power-on when the shell allows it; not connected after 15 s (and again at 45 s) we
// connect the one remembered (bt.last) as the shell does (Hf_Connect + Av_Connect, the proxy's BT_CMD_CONNECT),
// the phone only (no music link) - its music link (Av_Connect); then its music becomes the source (BtMusicOn) and
// plays (Av_Mute(0) + Av_Play, as the shell's play). Given up after 2 min.
// The phone's music chosen and its link gone (HU sheet 30: BT music no more until a reboot): asked again, as at the
// start - its music link when the phone is there, else the phone remembered (bt.last)
static bool PbAutoOn()
{
    return s_cfg.pbAuto != 0;
}

static void BtMusicLink()
{
    BtState bt;
    if (!BtGet(&bt, NULL) || bt.av != 0 || s_cfg.btOwner == BTO_OBD)
        return;
    if (bt.hf >= 1) {
        VLog(L"Bluetooth: музыка телефона не подключена - подключаю (A2DP)");
        BtCmd(BT_CMD_AVCONNECT);
    } else if (s_cfg.btLast[0]) {
        VLog(L"Bluetooth: телефон не подключён - подключаю %s", s_cfg.btLast);
        BtCmdText(BT_CMD_CONNECT, s_cfg.btLast);
    }
}

// 60 s on (the log is on the card by then): the MCU's source and the programs running - who plays (HU sheet 30: after a
// reboot SD music sounded with the phone's, no player of ours had started)
static void StartReport(DWORD el)
{
    static bool s_done;
    if (s_done || el < 60000)
        return;
    s_done = true;
    static TaskInfo t;
    TaskSample(&t, s_wnd);
    wchar_t s[400] = L"";
    for (int i = 0; i < t.procCount && wcslen(s) < 340; i++)
        if (!TaskIsSystem(t.proc[i].name)) {
            wcscat(s, L" ");
            wcsncat(s, t.proc[i].name, 39);
        }
    int vol, mute, src;
    VolGet(&vol, &mute, &src);
    VLog(L"запуск: через 60 с источник MCU %d, наш плеер %s; программы:%s", src, PlayerPlaying() ? L"играет" : L"нет",
         s[0] ? s : L" —");
}

static void ResumeSourceTick()
{
    DWORD el = GetTickCount() - s_bootTick;
    StartReport(el);
    if (s_resumed || el < 10000)
        return;
    int vol, mute, src;
    VolGet(&vol, &mute, &src);
    s_resuming = true;   // (ResumeByUser: not the user's)
    if (!wcscmp(s_cfg.uiSource, L"radio")) {
        VLog(L"запуск: радио (источник MCU %d)", src);
        RadioOn();
    } else if (!wcscmp(s_cfg.uiSource, L"net")) {   // the station last played (radio.station)
        NetLoadList();
        VLog(L"запуск: интернет-радио «%s»", s_cfg.radioStation);
        if (NetCur())
            NetPlayNow();
    } else if (!wcscmp(s_cfg.uiSource, L"aux")) {   // (its number learned: AuxOn)
        VLog(L"запуск: AUX (источник MCU %d)", src);
        AuxOn();
    } else if (!wcscmp(s_cfg.uiSource, L"music")) {
        VLog(L"запуск: музыка «%s»", s_cfg.musicFile);
        if (!s_chan && s_cfg.musicDir[0])
            PlayerLoadDir(s_cfg.musicDir, s_cfg.musicFile, s_cfg.musicTree);
    } else if (!wcscmp(s_cfg.uiSource, L"bt")) {
        static DWORD s_srcAsked;
        static int s_tries;
        BtState bt;
        bool proxy = BtGet(&bt, NULL);
        bool phone = proxy && bt.av >= 1;   // A2DP connected
        if (s_cfg.btOwner == BTO_OBD) {
            VLog(L"запуск: Bluetooth у OBD-адаптера - телефон не подключаю");
        } else if (!phone) {
            if (proxy && ((s_tries == 0 && el >= 15000) || (s_tries == 1 && el >= 45000))) {
                s_tries++;
                if (bt.hf >= 1) {   // the phone is there, its music not
                    VLog(L"запуск: Bluetooth - телефон есть, подключаю музыку (A2DP)");
                    BtCmd(BT_CMD_AVCONNECT);
                } else if (s_cfg.btLast[0]) {
                    VLog(L"запуск: Bluetooth - подключаю телефон %s", s_cfg.btLast);
                    BtCmdText(BT_CMD_CONNECT, s_cfg.btLast);
                } else {
                    VLog(L"запуск: Bluetooth - последний телефон не известен, жду подключения");
                }
            }
            if (el < 120000) {
                s_resuming = false;
                return;
            }
            VLog(L"запуск: Bluetooth - телефон не подключился за 2 мин");
        } else if (src != 9 && !s_srcAsked) {
            VLog(L"запуск: Bluetooth - телефон подключён, источник BT");
            BtMusicOn();
            s_srcAsked = GetTickCount();
            s_resuming = false;
            return;
        } else if (src != 9 && GetTickCount() - s_srcAsked < 4000) {
            s_resuming = false;
            return;   // the source still switching
        } else {
            VLog(L"запуск: Bluetooth - играть (источник MCU %d)", src);
            BtCmd(BT_CMD_PLAYON);
        }
    }
    s_resuming = false;
    s_resumed = true;
}

// Once a second: the screen and the source to remember (after the start's resume, not before it), and the phone
// connected (bt.last).
static void UiRememberTick()
{
    BtState bt;
    if (BtGet(&bt, NULL) && bt.hf >= 1 && bt.devAddr[0]) {
        wchar_t a[18];
        BtText(bt.devAddr, a, 18);
        if (wcscmp(a, s_cfg.btLast)) {
            wcscpy(s_cfg.btLast, a);
            VLog(L"запомнить: телефон %s", a);
            CfgSoon();
        }
    }
    if (!s_resumed)
        return;
    const wchar_t* sc = UiScreenKey();
    const wchar_t* so = UiSourceKey();
    const wchar_t* sm = NULL;
    for (int i = 0; i < (int)(sizeof(kUiScreens) / sizeof(kUiScreens[0])); i++)
        if (kUiScreens[i].s == s_media)
            sm = kUiScreens[i].key;
    if (sm && wcscmp(sm, s_cfg.uiMedia)) {
        wcscpy(s_cfg.uiMedia, sm);
        CfgSoon();
    }
    if ((sc && wcscmp(sc, s_cfg.uiScreen)) || (so && wcscmp(so, s_cfg.uiSource))) {
        if (sc)
            wcscpy(s_cfg.uiScreen, sc);
        if (so)
            wcscpy(s_cfg.uiSource, so);
        VLog(L"запомнить: экран «%s», источник «%s»", s_cfg.uiScreen, s_cfg.uiSource);
        CfgSoon();
    }
}

static void Nav(Screen s)
{
    if (s == S_RADIO)
        RadioOn();
    s_histLen = 0;
    s_status[0] = 0;
    s_editMode = false;
    s_screen = s;
    s_screenTick = GetTickCount();
    if (Section(s) == 2)
        s_media = s;
}

// A swipe turns the page of a paged screen (kPagers: some go round, the others stop - false at the first / last
// page) or the photo viewer's picture.
static bool Swipe(int dir)
{
    if (s_screen == S_PHOTO_VIEW) {
        PhotoStep(dir);
        return true;
    }
    const Pager* p = PagerOf(s_screen);
    int to;
    if (!p || !PageStep(*p, dir, &to))
        return false;
    if (s_screen == S_RADIO)
        RadioPageTo(to);   // (its band too)
    else
        *p->page = to;
    return true;
}

// Day / night, two things set apart: the theme - VestaHome's colours a third darker, OSMapsCE's night map (VestaCore
// vnight) - and the unit's backlight (the shell's key: the MCU switches it and reports 0x4105, its own brightness
// for each).
static void SetTheme(bool night)
{
    if (night != s_night)   // (sheet 41: Android Auto fell after the theme was changed by hand - when, in the log)
        VLog(L"тема: %s", night ? L"ночь" : L"день");
    s_night = night;
    NightShare(night);
}

static void SetLight(bool night)
{
    if (s_mcuNight != (night ? 1 : 0))
        McuKey(night ? 0x65 : 0x64, false);
    s_mcuNight = night ? 1 : 0;   // at once in the settings; the MCU's 0x4105 confirms
}

static DWORD s_lastTouch;   // the last tap: the screen saver counts from it

// Sunrise or sunset for the day of the year, minutes after local midnight; -1: the sun stays down, -2: up.
// The almanac's algorithm (zenith 90.833 deg), good to a couple of minutes.
static int SunEvent(int doy, double lat, double lon, bool rise)
{
    const double k = 3.14159265358979 / 180;
    double lh = lon / 15, t = doy + ((rise ? 6 : 18) - lh) / 24;
    double m = 0.9856 * t - 3.289;
    double l = fmod(m + 1.916 * sin(m * k) + 0.020 * sin(2 * m * k) + 282.634 + 720, 360);
    double ra = fmod(atan(0.91764 * tan(l * k)) / k + 720, 360);
    ra = (ra + floor(l / 90) * 90 - floor(ra / 90) * 90) / 15;
    double sd = 0.39782 * sin(l * k), cd = cos(asin(sd));
    double ch = (cos(90.833 * k) - sd * sin(lat * k)) / (cd * cos(lat * k));
    if (ch > 1)
        return -1;
    if (ch < -1)
        return -2;
    double h = (rise ? 360 - acos(ch) / k : acos(ch) / k) / 15;
    double ut = h + ra - 0.06571 * t - 6.622 - lh;
    return ((int)floor(ut * 60 + 0.5) + s_cfg.tzMin + 2880) % 1440;
}

// The automatic mode's answer for now: night between sunset and sunrise (the place: the GPS's last fix, else home,
// else 7:00-20:00). rise / set for the settings' line.
static bool SunNight(int* rise, int* set)
{
    SYSTEMTIME t;
    GetLocalTime(&t);
    static const int kDays0[12] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
    int doy = kDays0[(t.wMonth + 11) % 12] + t.wDay + (t.wMonth > 2 && t.wYear % 4 == 0 ? 1 : 0);
    int now = t.wHour * 60 + t.wMinute;
    double lat, lon;
    int a = 7 * 60, b = 20 * 60;
    if (HerePlace(0xFFFFFFFF, &lat, &lon) != PLACE_NONE) {   // (the fix, else the last place, else home)
        a = SunEvent(doy, lat, lon, true);
        b = SunEvent(doy, lat, lon, false);
    }
    *rise = a;
    *set = b;
    if (a == -1 || b == -1)
        return true;    // polar night
    if (a == -2 || b == -2)
        return false;   // polar day
    return a < b ? now < a || now >= b : now >= b && now < a;
}

// Applies the theme's and the backlight's modes. "By the sun" switches when the sun's answer changes (a switch by
// hand - the bar, the shell, OSMapsCE - holds until the next sunrise / sunset); force: apply all now (the start,
// a choice in the settings).
static void NightAuto(bool force)
{
    static int s_last = -1;
    int a, b;
    int n = SunNight(&a, &b) ? 1 : 0;
    bool edge = n != s_last || force;
    if (n != s_last)
        VLog(L"экран: по солнцу %s", n ? L"ночь" : L"день");
    s_last = n;
    if (s_cfg.themeMode == 0 ? edge : force)
        SetTheme(s_cfg.themeMode == 0 ? n != 0 : s_cfg.themeMode == 2);
    if (s_cfg.lightMode == 0 ? edge : force)
        SetLight(s_cfg.lightMode == 0 ? n != 0 : s_cfg.lightMode == 2);
}

// Settings -> Screen: the theme's and the backlight's day / night, the screen saver, the touch calibration. (No
// brightness: the display takes only the day and the night levels - HU 2026-10-20, the user.)
static void ScrSetScreen(HDC dc)
{
    static const wchar_t* saver[5] = { L"Выкл", L"1", L"3", L"5", L"10" };
    Settings(dc, S_SET_SCREEN);
    wchar_t sub[48];
    int a, b;
    SunNight(&a, &b);
    if (a >= 0 && b >= 0)   // "auto": the sun's times
        _snwprintf(sub, 48, L"восход %d:%02d · закат %d:%02d", a / 60, a % 60, b / 60, b % 60);
    else
        wcscpy(sub, a == -1 ? L"полярная ночь" : L"полярный день");
    int light = s_mcuNight;
    for (int row = 0; row < 2; row++) {   // the theme, the backlight: АВТО / ДЕНЬ / НОЧЬ
        int y = 64 + row * 56, mode = row ? s_cfg.lightMode : s_cfg.themeMode;
        Action act = row ? A_LIGHT : A_THEME;
        SetRow(dc, y, row ? L"Подсветка" : L"День и ночь",
               row ? (light < 0 ? L"нет данных от MCU" : light ? L"сейчас ночная" : L"сейчас дневная") : sub);
        Button(dc, 456, y + 6, 96, 44, L"Авто", mode == 0 ? B_SEL : B_N, IC_NONE, 15, 0, act, 0);
        Button(dc, 560, y + 6, 96, 44, L"День", mode == 1 ? B_SEL : B_N, IC_SUN, 15, 22, act, 1);
        Button(dc, 664, y + 6, 104, 44, L"Ночь", mode == 2 ? B_SEL : B_N, IC_MOON, 15, 22, act, 2);
    }
    SetRow(dc, 176, L"Заставка, мин", L"часы и дата без касаний");
    for (int i = 0; i < 5; i++)
        if (kSaverMins[i] == s_cfg.saverMin)
            s_seg[G_SAVER] = i;
    Seg(dc, 456, 182, 312, 44, saver, 5, G_SAVER, 14);
    SetRow(dc, 232, L"Калибровка экрана", L"системные перекрестья", false, A_CALIBRATE);
    Chevron(dc, 736, 244);
    wchar_t acc[48];   // the accent and the saver's background (moved here from "General", the user 2026-10-26)
    _snwprintf(acc, 48, L"акцент: %s · фон заставки", AccentName());
    SetRow(dc, 288, L"Тема", acc, true, A_SCREEN, S_SET_THEME);
    Disc(dc, 700, 316, 9, kOrange);
    Chevron(dc, 736, 300);
}
