// vh_settings.inc - part of home.cpp (included there, in order): the settings: VestaHome.ini.

// ---------------------------------------------------------------- services: settings, GPS, weather, cameras

// ---- settings: VestaHome.ini next to the exe (UTF-8, key=value). The unit resets its registry on every boot;
// only the card keeps them.
enum { W_OMETEO, W_OWM, W_YANDEX, W_COUNT };
static const char* kProvKey[W_COUNT] = { "openmeteo", "owm", "yandex" };   // anything else (old wttr, gismeteo): Open-Meteo

// The main screen's widgets (DrawHome's table follows them; the ini names them).
enum { WG_MEDIA, WG_SPEED, WG_RADIO, WG_WEATHER, WG_APPS, WG_TRIP, WG_CLIMATE, WG_DATE, WG_PHONE, WG_CAR, WG_NAVI,
       WG_COUNT };
static const wchar_t* kWgKey[WG_COUNT] = { L"media", L"speed", L"radio", L"weather", L"apps", L"trip", L"climate",
                                           L"date", L"phone", L"car", L"navi" };

// The user's widgets (VestaWidgets\*.ini on the SD card, UserWidgetsScan): ids after the built-in ones, keys
// "u.<file name>". A script (MortScript) writes the data file, VestaHome only reads and draws it.
enum { kWgUser = 8, kWgMax = WG_COUNT + kWgUser };
struct UserWg {
    wchar_t  key[40], title[40], dir[MAX_PATH], data[MAX_PATH], tap[MAX_PATH], start[MAX_PATH];
    wchar_t  file[MAX_PATH];        // its .ini's name (the text editor opens it)
    Icon     ic;
    bool     wide, have;
    int      refresh, state;        // refresh: seconds between reads; state: 0 none, 1 ok, 2 warn, 3 alert
    DWORD    readTick;
    FILETIME dataTime;              // the data file's write time last read
    wchar_t  value[24], unit[24], text[2][96];
    // HTTPS through VestaHome (UserWgFetch): url, every (s), the raw answer saved to resp, what to take from it
    // (fields 0 value, 1 unit, 2 text, 3 text2, 4 state: "start|end" markers) and the fixed parts ("%s" = taken)
    char     url[512];
    int      every;
    DWORD    fetchTick;
    wchar_t  resp[MAX_PATH], from[5][64], fixed[5][96];
};
static UserWg s_uw[kWgUser];
static int    s_uwCount;

static int WgCount() { return WG_COUNT + s_uwCount; }
static const wchar_t* WgKey(int id) { return id < WG_COUNT ? kWgKey[id] : s_uw[id - WG_COUNT].key; }

struct Cfg {
    int     weatherProv;
    wchar_t weatherKey[W_COUNT][64];   // API key per provider (Open-Meteo needs none)
    bool    weatherGps;                // the place where the car is; without a fix the home point
    wchar_t homeName[48];
    double  homeLat, homeLon;          // both 0 = not set
    wchar_t gpsPort[16];               // the shell's NMEA port, only read
    DWORD   gpsBaud;                   // 0 = leave the speed the shell set
    bool    gpsTime;                   // set the clock from GPS
    int     tzMin;                     // local time = UTC + this (minutes): the unit's own zone resets with the registry
    bool    camAlert, camSound;
    int     camOver;          // the beep: -1 always, else only above the camera's limit + camOver km/h
    int     beepDev;          // the beep's waveOut device, -1 = WAVE_MAPPER (2026-10-17: which one the MCU mixes?)
    int     camDist;                   // m, 0 = by speed
    wchar_t camFile[MAX_PATH];         // the camera file (iGO / Navitel), relative = from the exe's folder
    wchar_t camRegions[200];           // speedcamonline.ru regions to download: "63,73", "-1" = all of Russia
    wchar_t camKinds[100];             // its camera kinds (the site's filter numbers)
    bool    camWeekly;                 // download them again once a week
    int     camUpdated;                // yyyymmdd of the last download, 0 = never
    int     yaDay, yaCount;            // Yandex weather requests made on that day (yyyymmdd): its limit, see YaTake
    int     eq[3];                     // equalizer low, mid, high: -6..6 (what was last sent to the MCU)
    int     balance, fader;            // -8..8, 0 = centre (CAudioFieldDlg)
    wchar_t musicDir[MAX_PATH];        // the player's folder and track, to come back to
    wchar_t musicFile[MAX_PATH];
    bool    shuffle, repeat;
    bool    musicTree;                 // the playlist takes the subfolders too ("ИГРАТЬ ВСЁ")
    wchar_t videoDir[MAX_PATH];        // the video player's folder and file
    wchar_t videoFile[MAX_PATH];
    bool    videoStretch;              // fill the window instead of keeping the proportions
    bool    videoMoving;               // the picture also while the car moves
    bool    rvcOwn;                    // the rear view camera shown by VestaHome (rvc.cpp), not by the shell
    bool    nmeaPort;                  // COM6 for the navigators: the GPS stream's NMEA only (VestaNMEA.dll)
    bool    wifiOn;                    // the Wi-Fi radio (wifi.cpp WifiEnable)
    int     saverMin;                  // the screen saver after this many minutes untouched, 0 = never
    int     speedVol;                  // the volume by speed: 0 off, 1..3 = +2 / +3 / +4 per 100 km/h
    int     carSens[3];                // the car widget's lines: sensors (kCarSensor), -1 none
    int     tank;                      // the fuel tank, L (the range from OBD's level): Vesta 55
    bool    time12;                    // the clock 12 h (else 24 h)
    int     startMax;                  // the volume at the start: no louder than this (VestaHome's own), 0 = no cap
    int     keyVol;                    // the navigation's / the keys' volume (SendRadioKeyVolData), -1 not set yet
    int     themeMode, lightMode;      // our colours and maps / the unit's backlight: 0 by the sun, 1 day, 2 night
    int     homeOrder[kWgMax];         // the main screen's widgets (WG_*, then the user's) in their order
    wchar_t homeRaw[400];              // home.widgets as read: keeps the user widgets not found now (no card)
    int     homeHidden;                // bit 1 << WG_*: hidden
    wchar_t uiScreen[16], uiSource[8];   // the screen and the source last seen (Resume*): "home", "radio"...
    wchar_t uiMedia[16];                 // the media's tab last seen: "music", "netradio", "bt"... (the nav bar)
    wchar_t btLast[18];                // the phone connected last, "A4:E2:87:98:D6:9F"
    wchar_t favs[640];                 // the phone's favourites: "name|number;" each (Phone -> Favourites)
    wchar_t photoDir[MAX_PATH];        // the photos' folder last opened
    wchar_t photoFile[MAX_PATH];       // the picture last viewed there (the start goes back to it)
    int     photoInterval;             // the slide show's seconds
    bool    photoFill, photoSlides, photoAutoRot;   // fill the screen / the slide show on opening / EXIF turn
    bool    wifiApAuto;                // the access point for a phone, automatically (wifi.cpp WifiApSetup)
    wchar_t wifiApSsid[33], wifiApPass[65];
    bool    rvcInvert;                 // the trajectory bends the other way (the CAN angle's sign)
    wchar_t radioStation[64];          // the internet radio's station (its name in radio.txt)
    int     wifiCount;                 // the known Wi-Fi networks: WZC forgets them on every boot (the registry)
    wchar_t wifiSsid[kWifiKnown][33];
    wchar_t wifiPass[kWifiKnown][64];  // open in memory; in the file sealed with the unit's key (secret.cpp)
    bool    wifiLocked[kWifiKnown];    // sealed by another unit (or damaged): kept as it was, not used
    wchar_t wifiSealed[kWifiKnown][300];
};
static Cfg s_cfg;
static const int kSaverMins[5] = { 0, 1, 3, 5, 10 };
static const int kCamDists[4] = { 0, 200, 400, 600 };   // 0 = by speed
// Camera bases live in their own folder next to the exe (kept out of git): downloads and the default file.
static const wchar_t* kCamFolder  = L"speedcam\\";
static const wchar_t* kCamDefault = L"speedcam\\speedcam.txt";

static void CfgDefaults()
{
    memset(&s_cfg, 0, sizeof(s_cfg));
    s_cfg.weatherProv = W_OMETEO;
    s_cfg.weatherGps = true;
    wcscpy(s_cfg.gpsPort, L"COM5:");
    s_cfg.gpsTime = true;
    s_cfg.tzMin = 180;                            // Moscow
    s_cfg.camAlert = s_cfg.camSound = true;
    s_cfg.camOver = -1;
    s_cfg.beepDev = 1;   // "Audio Output" 1: heard over the radio (HU 2026-10-18-2)
    s_cfg.wifiOn = true;
    s_cfg.saverMin = 5;
    s_cfg.speedVol = 2;
    s_cfg.tank = 55;
    s_cfg.startMax = 10;
    s_cfg.keyVol = -1;
    s_cfg.carSens[0] = 0;   // the speed (CAN), the trip, the unit's board
    s_cfg.carSens[1] = 2;
    s_cfg.carSens[2] = 10;
    s_cfg.photoInterval = 5;
    s_cfg.photoAutoRot = true;
    for (int i = 0; i < WG_COUNT; i++)
        s_cfg.homeOrder[i] = i;
    s_cfg.homeHidden = 0;
    wcscpy(s_cfg.wifiApSsid, L"VestaHome");
    s_cfg.camDist = 0;
    wcscpy(s_cfg.camFile, kCamDefault);
    wcscpy(s_cfg.camKinds, L"0,14,11,110,2,3");   // the speed cameras
    s_cfg.camWeekly = true;
}

static void CfgPath(wchar_t* path)
{
    _snwprintf(path, MAX_PATH, L"%sVestaHome.ini", s_dir);
}

static bool CfgBool(const wchar_t* v) { return v[0] == L'1' || !_wcsicmp(v, L"true") || !_wcsicmp(v, L"yes"); }

static void SaveCfg();
static void RadioCacheLoad(const wchar_t* k, const wchar_t* v);   // the tuner's last state, the stations' names
static int  RadioCacheText(wchar_t* out, int cap);

// "media,speed,-radio,u.fuel,...": the order, "-" hides; names not known now are skipped (and kept in homeRaw for
// when they come back), widgets not named go to the end. Again after the user widgets are scanned.
static void HomeWidgetsLoad(const wchar_t* v)
{
    if (v != s_cfg.homeRaw) {
        wcsncpy(s_cfg.homeRaw, v, 399);
        s_cfg.homeRaw[399] = 0;
    }
    int n = 0;
    bool seen[kWgMax] = { false };
    s_cfg.homeHidden = 0;
    while (*v) {
        while (*v == L' ' || *v == L',')
            v++;
        bool hide = *v == L'-';
        if (hide)
            v++;
        const wchar_t* e = v;
        while (*e && *e != L',' && *e != L' ')
            e++;
        for (int i = 0; i < WgCount(); i++)
            if (!seen[i] && (int)wcslen(WgKey(i)) == e - v && !wcsncmp(WgKey(i), v, e - v)) {
                seen[i] = true;
                s_cfg.homeOrder[n++] = i;
                if (hide)
                    s_cfg.homeHidden |= 1 << i;
            }
        v = e;
    }
    for (int i = 0; i < WgCount(); i++)
        if (!seen[i])
            s_cfg.homeOrder[n++] = i;
}

// The order to save: the widgets known now, then the user widgets of homeRaw not found now (the card out).
static void HomeWidgetsText(wchar_t* out, int cap)
{
    out[0] = 0;
    for (int i = 0; i < WgCount(); i++) {
        int id = s_cfg.homeOrder[i];
        wchar_t t[48];
        _snwprintf(t, 48, L"%s%s%s", i ? L"," : L"", s_cfg.homeHidden >> id & 1 ? L"-" : L"", WgKey(id));
        if ((int)(wcslen(out) + wcslen(t)) < cap)
            wcscat(out, t);
    }
    const wchar_t* v = s_cfg.homeRaw;
    while (*v) {
        while (*v == L' ' || *v == L',')
            v++;
        const wchar_t* b = v;
        const wchar_t* k = *v == L'-' ? v + 1 : v;
        const wchar_t* e = k;
        while (*e && *e != L',' && *e != L' ')
            e++;
        bool known = false;
        for (int i = 0; i < WgCount() && !known; i++)
            known = (int)wcslen(WgKey(i)) == e - k && !wcsncmp(WgKey(i), k, e - k);
        if (!known && e - k > 2 && !wcsncmp(k, L"u.", 2) && (int)(wcslen(out) + (e - b) + 2) < cap) {
            wcscat(out, L",");
            wcsncat(out, b, e - b);
        }
        v = e;
    }
    wcsncpy(s_cfg.homeRaw, out, 399);
    s_cfg.homeRaw[399] = 0;
}

static void LoadCfg()
{
    CfgDefaults();
    bool resave = false;   // a password written open by hand: sealed now
    wchar_t path[MAX_PATH];
    CfgPath(path);
    DWORD size;
    char* buf = ReadAll(path, &size);
    if (!buf)
        return;
    char* p = Utf8Text(buf);
    wchar_t line[400];
    while (NextUtf8Line(&p, line, 400)) {
        wchar_t* eq = line[0] != L'#' ? wcschr(line, L'=') : NULL;
        if (!eq)
            continue;
        *eq = 0;
        wchar_t* k = line;
        wchar_t* v = eq + 1;
        Trim(k);
        Trim(v);
        if (!wcscmp(k, L"weather.provider")) {
            for (int i = 0; i < W_COUNT; i++) {
                wchar_t w[16];
                _snwprintf(w, 16, L"%S", kProvKey[i]);
                if (!_wcsicmp(v, w))
                    s_cfg.weatherProv = i;
            }
        } else if (!wcsncmp(k, L"weather.key.", 12)) {
            for (int i = 0; i < W_COUNT; i++) {
                wchar_t w[16];
                _snwprintf(w, 16, L"%S", kProvKey[i]);
                if (!_wcsicmp(k + 12, w)) {
                    wcsncpy(s_cfg.weatherKey[i], v, 63);
                    s_cfg.weatherKey[i][63] = 0;
                }
            }
        } else if (!wcscmp(k, L"weather.gps")) {
            s_cfg.weatherGps = CfgBool(v);
        } else if (!wcscmp(k, L"home.name")) {
            wcsncpy(s_cfg.homeName, v, 47);
            s_cfg.homeName[47] = 0;
        } else if (!wcscmp(k, L"home.lat")) {
            s_cfg.homeLat = wcstod(v, NULL);
        } else if (!wcscmp(k, L"home.lon")) {
            s_cfg.homeLon = wcstod(v, NULL);
        } else if (!wcscmp(k, L"gps.port")) {
            wcsncpy(s_cfg.gpsPort, v, 15);
            s_cfg.gpsPort[15] = 0;
        } else if (!wcscmp(k, L"gps.baud")) {
            s_cfg.gpsBaud = wcstoul(v, NULL, 10);
        } else if (!wcscmp(k, L"gps.time")) {
            s_cfg.gpsTime = CfgBool(v);
        } else if (!wcscmp(k, L"time.tz")) {
            s_cfg.tzMin = _wtoi(v);
        } else if (!wcscmp(k, L"cam.alert")) {
            s_cfg.camAlert = CfgBool(v);
        } else if (!wcscmp(k, L"cam.sound")) {
            s_cfg.camSound = CfgBool(v);
        } else if (!wcscmp(k, L"cam.over")) {
            s_cfg.camOver = _wtoi(v);
        } else if (!wcscmp(k, L"sound.beepdev")) {
            s_cfg.beepDev = _wtoi(v) < 0 ? 1 : _wtoi(v);   // AUTO was not heard over the radio
        } else if (!wcscmp(k, L"cam.distance")) {
            s_cfg.camDist = _wtoi(v);
        } else if (!wcscmp(k, L"cam.regions")) {
            wcsncpy(s_cfg.camRegions, v, 199);
            s_cfg.camRegions[199] = 0;
        } else if (!wcscmp(k, L"cam.kinds")) {
            wcsncpy(s_cfg.camKinds, v, 99);
            s_cfg.camKinds[99] = 0;
        } else if (!wcscmp(k, L"cam.weekly")) {
            s_cfg.camWeekly = CfgBool(v);
        } else if (!wcscmp(k, L"cam.updated")) {
            s_cfg.camUpdated = _wtoi(v);
        } else if (!wcscmp(k, L"weather.yandex.day")) {
            s_cfg.yaDay = _wtoi(v);
        } else if (!wcscmp(k, L"weather.yandex.count")) {
            s_cfg.yaCount = _wtoi(v);
        } else if (!wcscmp(k, L"cam.file")) {
            wcsncpy(s_cfg.camFile, v, MAX_PATH - 1);
            s_cfg.camFile[MAX_PATH - 1] = 0;
        } else if (!wcscmp(k, L"audio.eq")) {
            swscanf(v, L"%d,%d,%d", &s_cfg.eq[0], &s_cfg.eq[1], &s_cfg.eq[2]);
            for (int i = 0; i < 3; i++)
                s_cfg.eq[i] = s_cfg.eq[i] < -6 ? -6 : s_cfg.eq[i] > 6 ? 6 : s_cfg.eq[i];
        } else if (!wcscmp(k, L"audio.balance")) {
            s_cfg.balance = _wtoi(v);
        } else if (!wcscmp(k, L"audio.fader")) {
            s_cfg.fader = _wtoi(v);
        } else if (!wcscmp(k, L"music.dir")) {
            wcsncpy(s_cfg.musicDir, v, MAX_PATH - 1);
            s_cfg.musicDir[MAX_PATH - 1] = 0;
        } else if (!wcscmp(k, L"music.file")) {
            wcsncpy(s_cfg.musicFile, v, MAX_PATH - 1);
            s_cfg.musicFile[MAX_PATH - 1] = 0;
        } else if (!wcscmp(k, L"music.tree")) {
            s_cfg.musicTree = CfgBool(v);
        } else if (!wcscmp(k, L"music.shuffle")) {
            s_cfg.shuffle = CfgBool(v);
        } else if (!wcscmp(k, L"music.repeat")) {
            s_cfg.repeat = CfgBool(v);
        } else if (!wcscmp(k, L"video.dir")) {
            wcsncpy(s_cfg.videoDir, v, MAX_PATH - 1);
            s_cfg.videoDir[MAX_PATH - 1] = 0;
        } else if (!wcscmp(k, L"video.file")) {
            wcsncpy(s_cfg.videoFile, v, MAX_PATH - 1);
            s_cfg.videoFile[MAX_PATH - 1] = 0;
        } else if (!wcscmp(k, L"video.stretch")) {
            s_cfg.videoStretch = CfgBool(v);
        } else if (!wcscmp(k, L"video.moving")) {
            s_cfg.videoMoving = CfgBool(v);
        } else if (!wcscmp(k, L"rvc.own")) {
            s_cfg.rvcOwn = CfgBool(v);
        } else if (!wcscmp(k, L"gps.nmeaport")) {
            s_cfg.nmeaPort = CfgBool(v);
        } else if (!wcscmp(k, L"wifi.on")) {
            s_cfg.wifiOn = CfgBool(v);
        } else if (!wcscmp(k, L"ui.media")) {
            wcsncpy(s_cfg.uiMedia, v, 15);
            s_cfg.uiMedia[15] = 0;
        } else if (!wcscmp(k, L"ui.screen")) {
            wcsncpy(s_cfg.uiScreen, v, 15);
            s_cfg.uiScreen[15] = 0;
        } else if (!wcscmp(k, L"photo.dir")) {
            wcsncpy(s_cfg.photoDir, v, MAX_PATH - 1);
            s_cfg.photoDir[MAX_PATH - 1] = 0;
        } else if (!wcscmp(k, L"photo.file")) {
            wcsncpy(s_cfg.photoFile, v, MAX_PATH - 1);
            s_cfg.photoFile[MAX_PATH - 1] = 0;
        } else if (!wcscmp(k, L"photo.interval")) {
            s_cfg.photoInterval = _wtoi(v) < 2 ? 2 : _wtoi(v) > 60 ? 60 : _wtoi(v);
        } else if (!wcscmp(k, L"photo.fill")) {
            s_cfg.photoFill = CfgBool(v);
        } else if (!wcscmp(k, L"photo.slides")) {
            s_cfg.photoSlides = CfgBool(v);
        } else if (!wcscmp(k, L"photo.autorotate")) {
            s_cfg.photoAutoRot = CfgBool(v);
        } else if (!wcscmp(k, L"bt.last")) {
            wcsncpy(s_cfg.btLast, v, 17);
            s_cfg.btLast[17] = 0;
        } else if (!wcscmp(k, L"phone.fav")) {
            wcsncpy(s_cfg.favs, v, 639);
            s_cfg.favs[639] = 0;
        } else if (!wcscmp(k, L"obd.addr")) {
            wcsncpy(s_obdAddr, v, 17);
            s_obdAddr[17] = 0;
        } else if (!wcscmp(k, L"ui.source")) {
            wcsncpy(s_cfg.uiSource, v, 7);
            s_cfg.uiSource[7] = 0;
        } else if (!wcscmp(k, L"home.widgets")) {
            HomeWidgetsLoad(v);
        } else if (!wcscmp(k, L"screen.theme")) {
            s_cfg.themeMode = _wtoi(v) % 3;
        } else if (!wcscmp(k, L"screen.light")) {
            s_cfg.lightMode = _wtoi(v) % 3;
        } else if (!wcscmp(k, L"car.tank")) {
            s_cfg.tank = _wtoi(v) < 20 ? 20 : _wtoi(v) > 120 ? 120 : _wtoi(v);
        } else if (!wcscmp(k, L"car.sensors")) {   // "0,2,10"
            const wchar_t* q = v;
            for (int i = 0; i < 3; i++) {
                s_cfg.carSens[i] = *q ? _wtoi(q) : -1;
                if (s_cfg.carSens[i] < -1 || s_cfg.carSens[i] >= 64)
                    s_cfg.carSens[i] = -1;
                q = wcschr(q, L',');
                q = q ? q + 1 : L"";
            }
        } else if (!wcscmp(k, L"time.12h")) {
            s_cfg.time12 = CfgBool(v);
        } else if (!wcscmp(k, L"sound.startmax")) {
            s_cfg.startMax = _wtoi(v) < 0 ? 0 : _wtoi(v) > 30 ? 30 : _wtoi(v);
        } else if (!wcscmp(k, L"sound.keyvol")) {
            s_cfg.keyVol = _wtoi(v) < -1 ? -1 : _wtoi(v) > 30 ? 30 : _wtoi(v);
        } else if (!wcscmp(k, L"sound.speedvol")) {
            s_cfg.speedVol = _wtoi(v) < 0 ? 0 : _wtoi(v) > 3 ? 3 : _wtoi(v);
        } else if (!wcscmp(k, L"screen.saver")) {
            s_cfg.saverMin = _wtoi(v) < 0 ? 0 : _wtoi(v) > 60 ? 60 : _wtoi(v);
        } else if (!wcscmp(k, L"wifi.ap.auto")) {
            s_cfg.wifiApAuto = CfgBool(v);
        } else if (!wcscmp(k, L"wifi.ap.ssid") && v[0]) {
            wcsncpy(s_cfg.wifiApSsid, v, 32);
        } else if (!wcscmp(k, L"wifi.ap.pass") && wcslen(v) >= 8) {
            wcsncpy(s_cfg.wifiApPass, v, 63);
        } else if (!wcscmp(k, L"rvc.invert")) {
            s_cfg.rvcInvert = CfgBool(v);
        } else if (!wcsncmp(k, L"radio.c", 7) || !wcscmp(k, L"radio.names")) {
            RadioCacheLoad(k, v);
        } else if (!wcscmp(k, L"radio.station")) {
            wcsncpy(s_cfg.radioStation, v, 63);
            s_cfg.radioStation[63] = 0;
        } else if (!wcscmp(k, L"wifi.net") && s_cfg.wifiCount < kWifiKnown) {   // name|sealed password
            wchar_t* bar = wcschr(v, L'|');
            if (bar && bar > v) {
                *bar = 0;
                int i = s_cfg.wifiCount++;
                wcsncpy(s_cfg.wifiSsid[i], v, 32);
                s_cfg.wifiSsid[i][32] = 0;
                const wchar_t* pass = bar + 1;
                if (!SecretIsSealed(pass)) {   // written open by hand: taken, sealed at the next save
                    wcsncpy(s_cfg.wifiPass[i], pass, 63);
                    s_cfg.wifiPass[i][63] = 0;
                    if (pass[0])
                        resave = true;
                } else if (!SecretOpen(pass, s_cfg.wifiPass[i], 64)) {
                    s_cfg.wifiLocked[i] = true;
                    wcsncpy(s_cfg.wifiSealed[i], pass, 299);
                    s_cfg.wifiSealed[i][299] = 0;
                    VLog(L"wifi: пароль сети %s не расшифрован (другое ГУ?)", s_cfg.wifiSsid[i]);
                }
            }
        }
    }
    free(buf);
    if (resave)
        SaveCfg();
    // the old default next to the exe: the file moves into the speedcam folder, the setting with it
    if (!_wcsicmp(s_cfg.camFile, L"speedcam.txt")) {
        wchar_t from[MAX_PATH], dir[MAX_PATH], to[MAX_PATH];
        _snwprintf(from, MAX_PATH, L"%sspeedcam.txt", s_dir);
        _snwprintf(dir, MAX_PATH, L"%s%s", s_dir, kCamFolder);
        _snwprintf(to, MAX_PATH, L"%s%s", s_dir, kCamDefault);
        CreateDirectory(dir, NULL);
        if (GetFileAttributes(to) == 0xFFFFFFFF)
            MoveFile(from, to);
        wcscpy(s_cfg.camFile, kCamDefault);
        SaveCfg();
    }
}

// Written whole through a temporary file, like apps.txt.
static void SaveCfg()
{
    wchar_t homeWg[200];
    HomeWidgetsText(homeWg, 200);
    static wchar_t text[12000];
    int n = _snwprintf(text, 12000,
        L"# VestaHome settings (changed from its screens; the file is rewritten whole).\r\n"
        L"weather.provider=%S\r\nweather.key.owm=%s\r\nweather.key.yandex=%s\r\n"
        L"# Yandex requests made that day (its free tariff: 30 a day)\r\nweather.yandex.day=%d\r\nweather.yandex.count=%d\r\n"
        L"weather.gps=%d\r\nhome.name=%s\r\nhome.lat=%.5f\r\nhome.lon=%.5f\r\n"
        L"# gps.baud: 0 = leave the port's speed as the shell set it\r\n"
        L"gps.port=%s\r\ngps.baud=%u\r\ngps.time=%d\r\n"
        L"# time.tz: local time = UTC + minutes (180 = Moscow)\r\ntime.tz=%d\r\n"
        L"# cam.file: the camera file (iGO or Navitel), a relative path is from the VestaHome folder;\r\n"
        L"# downloads go to speedcam\\speedcam.txt\r\n"
        L"# cam.over: the beep only above the camera's limit + this many km/h; -1 = always\r\n"
        L"cam.alert=%d\r\ncam.sound=%d\r\ncam.over=%d\r\nsound.beepdev=%d\r\ncam.distance=%d\r\ncam.file=%s\r\n"
        L"# cam.regions, cam.kinds: what to download from speedcamonline.ru (region codes, -1 = Russia; kinds)\r\n"
        L"cam.regions=%s\r\ncam.kinds=%s\r\ncam.weekly=%d\r\ncam.updated=%d\r\n"
        L"# audio: equalizer low,mid,high -6..6; balance/fader -8..8 (sent to the MCU when changed)\r\n"
        L"audio.eq=%d,%d,%d\r\naudio.balance=%d\r\naudio.fader=%d\r\n"
        L"# music.tree: the playlist is music.dir with all its subfolders (music.file a path inside it)\r\n"
        L"music.dir=%s\r\nmusic.file=%s\r\nmusic.shuffle=%d\r\nmusic.repeat=%d\r\nmusic.tree=%d\r\n"
        L"# video.moving: the video also while the car moves (0: paused)\r\n"
        L"video.dir=%s\r\nvideo.file=%s\r\nvideo.stretch=%d\r\nvideo.moving=%d\r\nradio.station=%s\r\n"
        L"# rvc.own: the rear view camera by VestaHome (without the stock shell); rvc.invert: the trajectory mirrored\r\n"
        L"rvc.own=%d\r\nrvc.invert=%d\r\nwifi.on=%d\r\n"
        L"# gps.nmeaport: COM6 for the navigators - only the NMEA of the GPS stream (Settings -> Car)\r\ngps.nmeaport=%d\r\n"
        L"# screen.saver: the clock after this many minutes untouched (0 = never)\r\n"
        L"screen.saver=%d\r\n"
        L"# sound.speedvol: the volume by speed, 0 off, 1..3 = +2 / +3 / +4 per 100 km/h\r\nsound.speedvol=%d\r\n"
        L"# sound.startmax: the volume at the start no louder than this (0 = no cap); sound.keyvol: navigation / keys\r\n"
        L"sound.startmax=%d\r\nsound.keyvol=%d\r\ntime.12h=%d\r\n"
        L"# car.sensors: the car widget's three lines (Settings of the widget), -1 none\r\ncar.sensors=%d,%d,%d\r\n"
        L"# car.tank: the fuel tank, L (the range: OBD's fuel level)\r\ncar.tank=%d\r\n"
        L"# screen.theme / screen.light: our colours and maps / the unit's backlight: 0 by sunrise and sunset (GPS, else\r\n"
        L"# home, else 7:00-20:00), 1 day, 2 night\r\n"
        L"screen.theme=%d\r\nscreen.light=%d\r\n"
        L"# home.widgets: the main screen's widgets in their order, \"-\" before a hidden one\r\n"
        L"home.widgets=%s\r\n"
        L"# ui.screen / ui.source: the screen and what played when last seen - both come back at the start\r\n"
        L"ui.screen=%s\r\nui.source=%s\r\nui.media=%s\r\n"
        L"# bt.last: the phone connected last (its address): connected again at the start when the source was BT\r\n"
        L"bt.last=%s\r\n"
        L"# phone.fav: the phone's favourites, name|number; each\r\nphone.fav=%s\r\n"
        L"# obd.addr: the OBD-II Bluetooth adapter (ELM327; Settings -> Bluetooth -> OBD)\r\n"
        L"obd.addr=%s\r\n"
        L"# photo.*: the photos' folder, the slide show's seconds, fill the screen, the slide show on opening, EXIF turn\r\n"
        L"photo.dir=%s\r\nphoto.file=%s\r\nphoto.interval=%d\r\nphoto.fill=%d\r\nphoto.slides=%d\r\nphoto.autorotate=%d\r\n"
        L"# wifi.ap.*: the unit's own access point for a phone with its hotspot on 5 GHz (auto: when no known network)\r\n"
        L"wifi.ap.ssid=%s\r\nwifi.ap.pass=%s\r\nwifi.ap.auto=%d\r\n",
        kProvKey[s_cfg.weatherProv], s_cfg.weatherKey[W_OWM], s_cfg.weatherKey[W_YANDEX], s_cfg.yaDay, s_cfg.yaCount,
        s_cfg.weatherGps, s_cfg.homeName, s_cfg.homeLat, s_cfg.homeLon,
        s_cfg.gpsPort, (unsigned)s_cfg.gpsBaud, s_cfg.gpsTime, s_cfg.tzMin, s_cfg.camAlert, s_cfg.camSound,
        s_cfg.camOver, s_cfg.beepDev, s_cfg.camDist, s_cfg.camFile, s_cfg.camRegions, s_cfg.camKinds, s_cfg.camWeekly, s_cfg.camUpdated,
        s_cfg.eq[0], s_cfg.eq[1], s_cfg.eq[2], s_cfg.balance, s_cfg.fader, s_cfg.musicDir, s_cfg.musicFile,
        s_cfg.shuffle, s_cfg.repeat, s_cfg.musicTree, s_cfg.videoDir, s_cfg.videoFile, s_cfg.videoStretch, s_cfg.videoMoving,
        s_cfg.radioStation, s_cfg.rvcOwn, s_cfg.rvcInvert, s_cfg.wifiOn, s_cfg.nmeaPort, s_cfg.saverMin, s_cfg.speedVol, s_cfg.startMax, s_cfg.keyVol, s_cfg.time12, s_cfg.carSens[0], s_cfg.carSens[1], s_cfg.carSens[2], s_cfg.tank, s_cfg.themeMode, s_cfg.lightMode, homeWg, s_cfg.uiScreen, s_cfg.uiSource, s_cfg.uiMedia, s_cfg.btLast, s_cfg.favs, s_obdAddr, s_cfg.photoDir, s_cfg.photoFile, s_cfg.photoInterval, s_cfg.photoFill,
        s_cfg.photoSlides, s_cfg.photoAutoRot,
        s_cfg.wifiApSsid, s_cfg.wifiApPass, s_cfg.wifiApAuto);
    if (n < 0)
        return;
    n += RadioCacheText(text + n, 12000 - n);
    if (s_cfg.wifiCount) {
        int k = _snwprintf(text + n, 12000 - n,
                           L"# wifi.net: a known Wi-Fi network, name|password sealed with this unit's key\r\n");
        n += k > 0 ? k : 0;
    }
    for (int i = 0; i < s_cfg.wifiCount && n < 11500; i++) {
        wchar_t sealed[300];
        if (s_cfg.wifiLocked[i])
            wcscpy(sealed, s_cfg.wifiSealed[i]);
        else if (!SecretSeal(s_cfg.wifiPass[i], sealed, 300))
            continue;   // never written open
        int k = _snwprintf(text + n, 12000 - n, L"wifi.net=%s|%s\r\n", s_cfg.wifiSsid[i], sealed);
        n += k > 0 ? k : 0;
    }
    text[n < 12000 ? n : 11999] = 0;
    static char out[36000];
    int len = WideCharToMultiByte(CP_UTF8, 0, text, -1, out, sizeof(out), NULL, NULL);
    wchar_t path[MAX_PATH], tmp[MAX_PATH];
    CfgPath(path);
    _snwprintf(tmp, MAX_PATH, L"%sVestaHome.tmp", s_dir);
    HANDLE h = CreateFile(tmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return;
    DWORD w;
    bool ok = len > 1 && WriteFile(h, out, len - 1, &w, NULL);
    CloseHandle(h);
    if (ok) {
        DeleteFile(path);
        MoveFile(tmp, path);
    } else {
        DeleteFile(tmp);
    }
}

static const wchar_t* BtLastAddr()
{
    return s_cfg.btLast;
}

// Favourites (the user 2026-10-22, in place of the demo tab): a star on a contact's card; kept in the ini
// (phone.fav = "name|number;" each), so they dial before the book is pulled.
enum { kFavMax = 16 };

static int FavCount(wchar_t names[][48], wchar_t nums[][24])   // the entries, up to kFavMax
{
    int n = 0;
    const wchar_t* p = s_cfg.favs;
    while (*p && n < kFavMax) {
        const wchar_t* bar = wcschr(p, L'|');
        const wchar_t* end = wcschr(p, L';');
        if (!end)
            end = p + wcslen(p);
        if (bar && bar < end) {
            int ln = bar - p < 47 ? bar - p : 47, lu = end - bar - 1 < 23 ? end - bar - 1 : 23;
            if (names) {
                wcsncpy(names[n], p, ln);
                names[n][ln] = 0;
                wcsncpy(nums[n], bar + 1, lu);
                nums[n][lu] = 0;
            }
            n++;
        }
        p = *end ? end + 1 : end;
    }
    return n;
}

static int FavFind(const wchar_t* num)
{
    wchar_t names[kFavMax][48], nums[kFavMax][24];
    int n = FavCount(names, nums);
    for (int i = 0; i < n; i++)
        if (!wcscmp(nums[i], num))
            return i;
    return -1;
}

static void FavToggle(const wchar_t* name, const wchar_t* num)
{
    wchar_t names[kFavMax][48], nums[kFavMax][24];
    int n = FavCount(names, nums), at = -1;
    for (int i = 0; i < n; i++)
        if (!wcscmp(nums[i], num))
            at = i;
    s_cfg.favs[0] = 0;
    for (int i = 0; i < n; i++)
        if (i != at) {
            wchar_t e[80];
            _snwprintf(e, 80, L"%s|%s;", names[i], nums[i]);
            e[79] = 0;
            wcscat(s_cfg.favs, e);
        }
    if (at < 0 && n < kFavMax) {
        wchar_t nm[48], e[80];
        wcsncpy(nm, name, 47);
        nm[47] = 0;
        for (wchar_t* c = nm; *c; c++)   // (the separators)
            if (*c == L'|' || *c == L';')
                *c = L' ';
        _snwprintf(e, 80, L"%s|%s;", nm, num);
        e[79] = 0;
        if (wcslen(s_cfg.favs) + wcslen(e) < 639)
            wcscat(s_cfg.favs, e);
    }
    SaveCfg();
}

static void ScrFavs(HDC dc)
{
    Frame(dc, L"ТЕЛЕФОН · ИЗБРАННОЕ");
    PhoneNav(dc, 2);
    Panel(dc, 192, 68, 592, 322);
    wchar_t names[kFavMax][48], nums[kFavMax][24];
    int n = FavCount(names, nums);
    if (!n) {
        T(dc, 216, 140, L"Избранных нет", 20, kText, 700, AL_START, 540);
        T(dc, 216, 172, L"Звёздочка в карточке контакта добавляет его сюда.", 16, kMuted, 400, AL_START, 540);
        return;
    }
    for (int i = 0; i < n && i < 10; i++)   // two columns of five
        ListRow(dc, 200 + i / 5 * 292, 76 + i % 5 * 62, 284, 56, names[i][0] ? names[i] : nums[i],
                NULL, L_N, IC_STAR_F, names[i][0] ? nums[i] : NULL, NULL, A_FAVDIAL, i, kOrange);
}
