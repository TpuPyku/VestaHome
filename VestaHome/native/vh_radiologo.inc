// vh_radiologo.inc - part of home.cpp (included there, in order): the FM station's name by the city and the
// frequency, and its logo behind the frequency, dimmed as the tall widget's cover (the user 2026-11-19: the logo
// shaded behind the frequency, down to the presets, the scale under it too; not on the presets). The set is its own
// archive (the logos are the stations') in VestaHome's content (the user 2026-11-19: VHContent, not VestaHome's own
// folder): VHContent\RadioLogos\stations.txt (make_radio.py) and RadioLogos\logos\<the station's name>.png / .jpg
// on the partition or the SD card (VhContentDirs) - anyone may add their own station's by its name. The place:
// the last GPS fix, else the home point. The logo: by the table's name, else RDS's, else the name learned.
enum { kRlFiles = 600, kRlFileName = 48, kRlSize = 192, kRlTableMax = 1024 * 1024 };
static int VhContentDirs(const wchar_t* sub, wchar_t (*dirs)[MAX_PATH]);   // (vh_userwidgets.inc)

static Rdb     s_rdb;                        // the table's stations near the place
static bool    s_rdbRead, s_rdbNone;         // read once; no table there (not looked for again)
static char    s_rlKey[kRlFiles][RDB_KEY];   // the logos' files: their names' keys
static wchar_t s_rlFile[kRlFiles][kRlFileName];
static char    s_rlDir[kRlFiles];             // which content folder it is in
static int     s_rlCount = -1;               // -1: not listed yet
static char    s_rlShown[RDB_KEY] = "\x01";  // the key of the logo in s_rlPic ("\x01": none asked yet)
static Pic     s_rlPic;                      // that logo, dimmed

static bool RadioPlace(double* lat, double* lon)
{
    Gps g;
    GpsGet(&g);
    if (g.fixTick && (g.lat != 0 || g.lon != 0)) {
        *lat = g.lat;
        *lon = g.lon;
        return true;
    }
    *lat = s_cfg.homeLat;
    *lon = s_cfg.homeLon;
    return s_cfg.homeLat != 0 || s_cfg.homeLon != 0;
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
        PicFree(&s_rlPic);
        wchar_t dirs[2][MAX_PATH], path[MAX_PATH];
        VhContentDirs(L"RadioLogos", dirs);
        _snwprintf(path, MAX_PATH, L"%slogos\\%s", dirs[file >= 0 ? s_rlDir[file] : 0], file >= 0 ? s_rlFile[file] : L"");
        if (file >= 0 && PicLoad(path, kRlSize, kRlSize, false, 0, false, &s_rlPic, NULL, NULL, true) == PIC_OK)
            PicDim(&s_rlPic);   // (as the tall widget's cover)
    }
    return s_rlPic.px32 != NULL;
}

// The logo behind x, y (kRlSize square, its corners rounded): each channel the lighter of the screen's and the
// logo's - the text over it stays as it is, the dark background takes the logo.
static void RadioLogoBack(int x, int y)
{
    if (s_rlPic.px32)
        BlitPic(s_rlPic, x + (kRlSize - s_rlPic.w) / 2, y + (kRlSize - s_rlPic.h) / 2, 20, true);
}
