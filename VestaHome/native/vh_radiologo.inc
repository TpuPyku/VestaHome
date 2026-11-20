// vh_radiologo.inc - part of home.cpp (included there, in order): the FM station's name by the city and the
// frequency, and its logo (the user 2026-11-19: as it is, not dimmed - at the left of the frequency, above the
// scale; on the widgets too). The set is its own archive (the logos are the stations') in VestaHome's content
// (VHContent, the user 2026-11-19): VHContent\RadioLogos\stations.txt (make_radio.py) and
// RadioLogos\logos\<the station's name>.png / .jpg on the partition or the SD card (VhContentDirs) - anyone may
// add their own station's by its name. The place: the last GPS fix, else the home point. The name shown and the
// logo's: the table's, else RDS's, else the name learned.
enum { kRlFiles = 600, kRlFileName = 48, kRlSize = 192, kRlSizes = 4, kRlTableMax = 1024 * 1024 };
static int VhContentDirs(const wchar_t* sub, wchar_t (*dirs)[MAX_PATH]);   // (vh_userwidgets.inc)

static Rdb     s_rdb;                        // the table's stations near the place
static bool    s_rdbRead, s_rdbNone;         // read once; no table there (not looked for again)
static char    s_rlKey[kRlFiles][RDB_KEY];   // the logos' files: their names' keys
static wchar_t s_rlFile[kRlFiles][kRlFileName];
static char    s_rlDir[kRlFiles];             // which content folder it is in
static int     s_rlCount = -1;               // -1: not listed yet
static char    s_rlShown[RDB_KEY] = "\x01";  // the key of the logo in s_rlPic ("\x01": none asked yet)
static Pic     s_rlPic, s_rlSized[kRlSizes];  // that logo (32-bit, kRlSize at most), its smaller copies
static Pic     s_rlBack;                     // its dimmed copy behind the tall widget's card (Widget, PicBack: as
                                             // the music's cover there - the user 2026-11-19)

static bool RadioPlace(double* lat, double* lon)   // (a fix of this run however old, else the last place, home)
{
    return HerePlace(0xFFFFFFFF, lat, lon) != PLACE_NONE;
}

// The table read for the place (again when it moved RDB_KM / 4 away).
static void RadioTableFor(double lat, double lon)
{
    if (s_rdbNone || (s_rdbRead && !RdbStale(&s_rdb, lat, lon)))
        return;
    s_rdbRead = true;
    wchar_t dirs[2][MAX_PATH], path[MAX_PATH];
    int nd = VhContentDirs(L"RadioLogos", dirs);
    HANDLE f = INVALID_HANDLE_VALUE;
    for (int i = 0; i < nd && f == INVALID_HANDLE_VALUE; i++) {   // (the first that has it)
        _snwprintf(path, MAX_PATH, L"%sstations.txt", dirs[i]);
        f = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    }
    if (f == INVALID_HANDLE_VALUE) {
        s_rdbNone = true;
        return;
    }
    DWORD size = GetFileSize(f, NULL), got = 0;
    char* text = size < kRlTableMax ? (char*)malloc(size + 1) : NULL;
    if (text && ReadFile(f, text, size, &got, NULL)) {
        text[got] = 0;
        RdbLoad(&s_rdb, text, lat, lon);
        VLog(L"радио: таблица станций, рядом %d", s_rdb.n);
    } else {
        s_rdbNone = true;
    }
    free(text);
    CloseHandle(f);
}

// The table's name of the station on f10 here; NULL: none (no table, no place, not in it).
static const wchar_t* RadioTableName(int f10)
{
    double lat, lon;
    if (f10 <= 0 || !RadioPlace(&lat, &lon))
        return NULL;
    RadioTableFor(lat, lon);
    return s_rdb.n > 0 ? RdbName(&s_rdb, f10, lat, lon) : NULL;
}

static void RadioLogosIn(const wchar_t* dir, int d)   // one content folder's logos/ into the list
{
    wchar_t pat[MAX_PATH];
    _snwprintf(pat, MAX_PATH, L"%slogos\\*.*", dir);
    WIN32_FIND_DATA fd;
    HANDLE h = FindFirstFile(pat, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return;
    do {
        wchar_t* dot = wcsrchr(fd.cFileName, L'.');
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || !dot || !PicIsName(fd.cFileName) ||
            wcslen(fd.cFileName) >= kRlFileName)
            continue;
        wchar_t name[kRlFileName];
        wcsncpy(name, fd.cFileName, dot - fd.cFileName);
        name[dot - fd.cFileName] = 0;
        RdbKey(name, s_rlKey[s_rlCount]);
        if (s_rlKey[s_rlCount][0]) {
            wcscpy(s_rlFile[s_rlCount], fd.cFileName);
            s_rlDir[s_rlCount] = (char)d;
            s_rlCount++;
        }
    } while (s_rlCount < kRlFiles && FindNextFile(h, &fd));
    FindClose(h);
}

static void RadioLogosList()   // the partition's first (a name in both: its own), then the SD card's
{
    s_rlCount = 0;
    wchar_t dirs[2][MAX_PATH];
    int nd = VhContentDirs(L"RadioLogos", dirs);
    for (int i = 0; i < nd; i++)
        RadioLogosIn(dirs[i], i);
    VLog(L"радио: логотипов %d", s_rlCount);
}

static int RadioLogoFile(const wchar_t* name)   // the logo file for a station's name, -1 none
{
    if (!name || !name[0])
        return -1;
    char key[RDB_KEY];
    RdbKey(name, key);
    for (int i = 0; key[0] && i < s_rlCount; i++)
        if (!strcmp(s_rlKey[i], key))
            return i;
    return -1;
}

// The logo for the first of the names that has one (the table's, RDS's, the learned), dimmed, in s_rlPic; false none.
static bool RadioLogoFor(const wchar_t* const* names, int n)
{
    if (s_rlCount < 0)
        RadioLogosList();
    int file = -1;
    for (int i = 0; i < n && file < 0; i++)
        file = RadioLogoFile(names[i]);
    const char* key = file >= 0 ? s_rlKey[file] : "";
    if (strcmp(key, s_rlShown)) {
        strcpy(s_rlShown, key);
        wchar_t dirs[2][MAX_PATH], path[MAX_PATH];
        VhContentDirs(L"RadioLogos", dirs);
        _snwprintf(path, MAX_PATH, L"%slogos\\%s", dirs[file >= 0 ? s_rlDir[file] : 0], file >= 0 ? s_rlFile[file] : L"");
        PicFree(&s_rlPic);
        for (int i = 0; i < kRlSizes; i++)
            PicFree(&s_rlSized[i]);
        PicFree(&s_rlBack);
        if (file >= 0)
            PicLoad(path, kRlSize, kRlSize, false, 0, false, &s_rlPic, NULL, NULL, true);
    }
    return s_rlPic.px32 != NULL;
}

// ---- the logos not in the set, from the net (the user 2026-11-19): the name shown without a logo file - searched in
// radio-browser.info (the internet radio's catalogue, its hosts kNcHosts) in the country of the place, the first
// station of the same name's key with a picture of 96 px and more (a PNG or a JPEG) - saved into RadioLogos\logos\
// as <the name>.png / .jpg (the SD card's content, else the partition's) and used from then on as any other. A name
// not found: into net_miss.txt there, not asked again for RDB_MISS_DAYS; no net: asked again in 5 minutes.
#define WM_APP_RADIOLOGO (WM_APP + 33)
enum { kRlNetRetryMs = 5 * 60 * 1000, kRlTried = 64, kRlMinPx = 96 };

static CRITICAL_SECTION s_rnCs;
static HANDLE  s_rnEv, s_rnThread;
static wchar_t s_rnWant[RDB_NAME];   // the name asked for (under s_rnCs)
static char    s_rnCountry[3];
static char    s_rnTried[kRlTried][RDB_KEY];   // the UI thread's: asked this session (the worker answers by a miss)
static int     s_rnTriedN;
static DWORD   s_rnNoNetTick;        // the last time the net did not answer (the worker's; read by the UI)

static int RadioNetDay()   // days since 2000 (the misses' dates)
{
    SYSTEMTIME st;
    GetSystemTime(&st);
    FILETIME ft;
    SystemTimeToFileTime(&st, &ft);
    ULARGE_INTEGER u;
    u.LowPart = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    return (int)(u.QuadPart / 864000000000ULL) - 145731;   // (1601 .. 2000: 145731 days)
}

static void RadioNetDir(wchar_t* dir)   // RadioLogos\ of the SD card's content when there is a card, else the partition's
{
    wchar_t dirs[2][MAX_PATH];
    VhContentDirs(L"RadioLogos", dirs);
#ifdef UNDER_CE
    bool card = GetFileAttributes(L"\\SDMemory") != 0xFFFFFFFF;
#else
    bool card = false;
#endif
    wcscpy(dir, dirs[card ? 1 : 0]);
}

static bool RadioNetMissed(const wchar_t* dir, const char* key)
{
    wchar_t p[MAX_PATH];
    _snwprintf(p, MAX_PATH, L"%snet_miss.txt", dir);
    HANDLE f = CreateFile(p, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return false;
    char text[8192];
    DWORD got = 0;
    ReadFile(f, text, sizeof(text) - 1, &got, NULL);
    CloseHandle(f);
    text[got] = 0;
    return RdbMissed(text, key, RadioNetDay());
}

static void RadioNetMiss(const wchar_t* dir, const char* key)
{
    wchar_t p[MAX_PATH];
    _snwprintf(p, MAX_PATH, L"%snet_miss.txt", dir);
    HANDLE f = CreateFile(p, GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, 0, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return;
    char line[64];
    int n = _snprintf(line, sizeof(line), "%s;%d\r\n", key, RadioNetDay());
    SetFilePointer(f, 0, NULL, FILE_END);
    DWORD put;
    WriteFile(f, line, n > 0 ? n : 0, &put, NULL);
    CloseHandle(f);
}

// The catalogue's answer for the path: HTTPS, then plain HTTP, on either host (as vh_netcat.inc); NULL: no net.
static unsigned char* RadioNetGet(const char* path, int* len)
{
    for (int k = 0; k < 4; k++) {
        unsigned char* body = NULL;
        int status = 0;
        if (HttpGet(kNcHosts[k % 2], k < 2, path, &body, len, &status) == NET_OK && status == 200)
            return body;
        free(body);
    }
    return NULL;
}

// The name's picture: found, downloaded, checked (a PNG or a JPEG, kRlMinPx and more), saved; 1 saved, 0 none
// (a miss), -1 no net.
static int RadioNetFetch(const wchar_t* name, const char* country, const wchar_t* dir)
{
    char path[300], host[64], pic[300];
    RdbNetPath(name, country, path, sizeof(path));
    if (!path[0])
        return 0;
    int len = 0;
    unsigned char* json = RadioNetGet(path, &len);
    if (!json)
        return -1;
    bool tls = false, found = RdbNetLogo((const char*)json, name, host, sizeof(host), pic, sizeof(pic), &tls);
    free(json);
    if (!found) {
        VLog(L"радио: логотип из сети «%s» - в каталоге (%S) нет станции с картинкой", name, country);
        return 0;
    }
    unsigned char* body = NULL;
    int status = 0, w = 0, h = 0, comp = 0;
    int err = HttpGet(host, tls, pic, &body, &len, &status);
    if (tls && err != NET_OK) {   // (a site's root not among vhttp's: the public picture by plain HTTP, as the catalogue)
        free(body);
        body = NULL;
        err = HttpGet(host, false, pic, &body, &len, &status);
    }
    if (err != NET_OK || status != 200 || len < 8) {
        VLog(L"радио: логотип из сети «%s» - %S%S не скачался (ошибка %d, ответ %d)", name, host, pic, err, status);
        free(body);
        return 0;
    }
    bool png = !memcmp(body, "\x89PNG", 4), jpg = body[0] == 0xFF && body[1] == 0xD8;
    bool ok = (png || jpg) && stbi_info_from_memory(body, len, &w, &h, &comp) && w >= kRlMinPx && h >= kRlMinPx;
    if (ok) {
        wchar_t file[MAX_PATH], safe[RDB_NAME];
        wcscpy(safe, name);
        for (wchar_t* c = safe; *c; c++)
            if (wcschr(L"\\/:*?\"<>|", *c))
                *c = L' ';
        _snwprintf(file, MAX_PATH, L"%slogos", dir);
        CreateDirectory(file, NULL);
        _snwprintf(file, MAX_PATH, L"%slogos\\%s.%s", dir, safe, png ? L"png" : L"jpg");
        HANDLE f = CreateFile(file, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        DWORD put = 0;
        ok = f != INVALID_HANDLE_VALUE && WriteFile(f, body, len, &put, NULL) && (int)put == len;
        if (f != INVALID_HANDLE_VALUE)
            CloseHandle(f);
        if (!ok)
            DeleteFile(file);
    }
    VLog(L"радио: логотип из сети «%s» - %S%S, %dx%d%s", name, host, pic, w, h, ok ? L"" : L" - не подошёл");
    free(body);
    return ok ? 1 : 0;
}

static DWORD WINAPI RadioNetThread(LPVOID)
{
    for (;;) {
        WaitForSingleObject(s_rnEv, INFINITE);
        wchar_t name[RDB_NAME];
        char country[3], key[RDB_KEY];
        EnterCriticalSection(&s_rnCs);
        wcscpy(name, s_rnWant);
        strcpy(country, s_rnCountry);
        LeaveCriticalSection(&s_rnCs);
        RdbKey(name, key);
        wchar_t dir[MAX_PATH];
        RadioNetDir(dir);
        CreateDirectory(dir, NULL);
        if (!key[0] || RadioNetMissed(dir, key))
            continue;
        int got = RadioNetFetch(name, country, dir);
        if (got < 0)
            s_rnNoNetTick = GetTickCount() | 1;
        else if (got == 0)
            RadioNetMiss(dir, key);
        PostMessage(s_wnd, WM_APP_RADIOLOGO, got, 0);
        Sleep(3000);   // (one search in three seconds at most)
    }
    return 0;
}

// A name shown without a logo: asked for once this session (no net: again after kRlNetRetryMs), in the place's country.
static void RadioNetWant(const wchar_t* name)
{
    char key[RDB_KEY], cc[3];
    double lat, lon;
    RdbKey(name, key);
    if (!key[0])
        return;
    if (s_rnNoNetTick && GetTickCount() - s_rnNoNetTick >= kRlNetRetryMs) {
        s_rnNoNetTick = 0;   // (the net may be there now: the ones asked while it was not, again)
        s_rnTriedN = 0;
    }
    for (int i = 0; i < s_rnTriedN && i < kRlTried; i++)
        if (!strcmp(s_rnTried[i], key))
            return;
    strcpy(s_rnTried[s_rnTriedN++ % kRlTried], key);
    bool place = RadioPlace(&lat, &lon);
    if (!place || !CountryAt(lat, lon, cc)) {
        VLog(L"радио: логотипа «%s» нет, в сети не ищу - %s", name, place ? L"страна по месту не определилась"
             : L"нет места (GPS, домашняя точка)");
        return;
    }
    VLog(L"радио: логотипа «%s» нет - ищу в сети (%S)", name, cc);
    if (!s_rnThread) {
        InitializeCriticalSection(&s_rnCs);
        s_rnEv = CreateEvent(NULL, FALSE, FALSE, NULL);
        s_rnThread = CreateThread(NULL, 0, RadioNetThread, NULL, 0, NULL);
    }
    EnterCriticalSection(&s_rnCs);
    wcsncpy(s_rnWant, name, RDB_NAME - 1);
    s_rnWant[RDB_NAME - 1] = 0;
    memcpy(s_rnCountry, cc, 3);
    LeaveCriticalSection(&s_rnCs);
    SetEvent(s_rnEv);
}

// WM_APP_RADIOLOGO: a logo saved - the list read again (the picture shown at the next frame).
static bool RadioNetTake(int saved)
{
    if (saved != 1)
        return false;
    s_rlCount = -1;
    strcpy(s_rlShown, "\x01");
    return true;
}

// The shown name of the station playing (the table's, else RDS's, else the learned) and its logo got ready;
// "" none.
static const wchar_t* RadioNow(const Tuner& t, bool* logo)
{
    const wchar_t* table = RadioTableName(t.f10);
    const wchar_t* learned = RadioName(t, t.f10);
    const wchar_t* names[3] = { table, t.ps, learned };
    bool has = t.f10 > 0 && RadioLogoFor(names, 3);
    if (logo)
        *logo = has;
    const wchar_t* shown = table ? table : t.ps[0] ? t.ps : learned;
    if (!has && t.f10 > 0 && shown[0])
        RadioNetWant(shown);   // (not in the set: from the net)
    // the log, once per station (the tester 2026-11-20: no logos, the names they were looked for by unknown)
    static int s_loggedF10;
    static bool s_loggedPs, s_loggedHas;
    if (t.f10 > 0 && (t.f10 != s_loggedF10 || (t.ps[0] != 0) != s_loggedPs || has != s_loggedHas)) {
        s_loggedF10 = t.f10;
        s_loggedPs = t.ps[0] != 0;
        s_loggedHas = has;
        double lat, lon;
        VLog(L"радио: %d.%02d МГц — по таблице «%s»%s, RDS «%s», логотип %s", t.f10 / 100, t.f10 % 100,
             table ? table : L"", RadioPlace(&lat, &lon) ? L"" : L" (нет места: GPS или домашняя точка)", t.ps,
             has ? L"есть" : L"нет");
    }
    return shown;
}

// The logo got ready s x s at x, y, its corners rounded by rad; false: none (the caller draws its own).
static bool RadioLogoArt(int x, int y, int s, int rad)
{
    return PicSized(s_rlPic, s_rlSized, kRlSizes, x, y, s, rad);
}
