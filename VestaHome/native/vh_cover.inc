// vh_cover.inc - part of home.cpp (included there, in order): the music's cover (the user 2026-11-17, as Android
// Auto shows it): the picture in the track's tag or its folder's (cover.cpp finds it, photo.cpp decodes it). Where
// there is room for it (the music screen, the big and the compact widgets) it stands in its place; the tall widget
// has it behind its text, whole and dimmed, not blurred (the user 2026-11-17: blur loses the reading). A worker
// decodes it; only the UI thread keeps it (a result comes by WM_APP_COVER): one cover kCoverPx square, 32-bit,
// its smaller copies made when drawn.
#define WM_APP_COVER (WM_APP + 31)
enum { kCoverPx = 232, kCoverSizes = 4, kCoverTagMax = 4 * 1024 * 1024, kCoverDirMax = 300 };

struct CoverJob {
    wchar_t track[MAX_PATH];
    Pic     pic;
    bool    same;   // the folder's picture as before: the cover kept
};

// An add-on's music (Android Auto: VestaAuto's "playing", plugin-api.md 7.3; the user 2026-11-17): what plays, its
// cover a picture file of the add-on's (in \Temp: RAM) - shown while that add-on's own sound plays.
struct AddonPlay {
    bool    have;
    int     plug, state, pos, len;   // state: 1 stopped, 2 playing, 3 paused; pos / len: seconds
    DWORD   tick;                    // pos as of then
    wchar_t title[96], artist[96], album[96], app[32], art[MAX_PATH];
};
static AddonPlay s_ap;
// s_cvPick (defined in vh_screens.inc, its first user): the cover wanted while set - this picture (an add-on's, a
// song's from the net), not our music's.
static int AddonSoundPlug();  // (vh_plugins.inc) the add-on whose own sound plays, -1 none

static bool AddonPlayShown()
{
    return s_ap.have && s_ap.plug >= 0 && AddonSoundPlug() == s_ap.plug;
}

static int AddonPos()   // seconds in, counted on while it plays
{
    int p = s_ap.pos + (s_ap.state == 2 ? (int)((GetTickCount() - s_ap.tick) / 1000) : 0);
    return s_ap.len > 0 && p > s_ap.len ? s_ap.len : p;
}

static CRITICAL_SECTION s_cvCs;
static HANDLE   s_cvEv, s_cvThread;
static wchar_t  s_cvWant[MAX_PATH];    // the track whose cover is wanted (the worker reads it under s_cvCs)
static wchar_t  s_cvAsked[MAX_PATH];   // the UI thread's last ask
static Pic      s_cv, s_cvSized[kCoverSizes], s_cvBack;   // UI thread only (s_cvBack: the card's size, dimmed)

static void CoverFree(Pic* p, int n)
{
    for (int i = 0; i < n; i++)
        PicFree(&p[i]);
}

// The picture inside the track's ID3v2 tag, decoded; false: no tag, no picture, not readable.
static bool CoverFromTag(const wchar_t* track, Pic* out)
{
    HANDLE f = CreateFile(track, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return false;
    unsigned char head[10];
    DWORD got = 0;
    int size = ReadFile(f, head, 10, &got, NULL) ? Id3TagSize(head, (int)got) : 0;
    bool ok = false;
    unsigned char* tag = size > 10 && size <= kCoverTagMax ? (unsigned char*)malloc(size) : NULL;
    if (tag) {
        memcpy(tag, head, 10);
        int off, len;
        ok = ReadFile(f, tag + 10, size - 10, &got, NULL) && (int)got == size - 10 && Id3Picture(tag, size, &off, &len) &&
             PicLoadMem(tag + off, len, kCoverPx, kCoverPx, true, out, true) == PIC_OK;
        free(tag);
    }
    CloseHandle(f);
    return ok;
}

// The folder's picture for its album (CoverPick): its full path into pic, false none.
static bool CoverFolderPicture(const wchar_t* track, wchar_t* pic)
{
    wchar_t dir[MAX_PATH], mask[MAX_PATH];
    wcscpy(dir, track);
    wchar_t* sl = wcsrchr(dir, L'\\');
    if (!sl)
        return false;
    sl[1] = 0;
    _snwprintf(mask, MAX_PATH, L"%s*", dir);
    mask[MAX_PATH - 1] = 0;
    static wchar_t names[kCoverDirMax][MAX_PATH];   // (the worker's only)
    const wchar_t* list[kCoverDirMax];
    int n = 0;
    WIN32_FIND_DATA fd;
    HANDLE h = FindFirstFile(mask, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                wcsncpy(names[n], fd.cFileName, MAX_PATH - 1);
                names[n][MAX_PATH - 1] = 0;
                list[n] = names[n];
                n++;
            }
        while (n < kCoverDirMax && FindNextFile(h, &fd));
        FindClose(h);
    }
    int k = CoverPick(list, n);
    if (k < 0)
        return false;
    _snwprintf(pic, MAX_PATH, L"%s%s", dir, names[k]);
    pic[MAX_PATH - 1] = 0;
    return true;
}

static DWORD WINAPI CoverThread(LPVOID)
{
    static wchar_t s_lastPic[MAX_PATH];   // the folder picture decoded last (a track of the same album: kept)
    for (;;) {
        WaitForSingleObject(s_cvEv, INFINITE);
        CoverJob* j = (CoverJob*)calloc(1, sizeof(CoverJob));
        if (!j)
            continue;
        EnterCriticalSection(&s_cvCs);
        wcscpy(j->track, s_cvWant);
        LeaveCriticalSection(&s_cvCs);
        wchar_t pic[MAX_PATH] = L"";
        bool got = false;
        if (j->track[0] && PicIsName(j->track)) {   // an add-on's cover: the picture itself
            got = PicLoad(j->track, kCoverPx, kCoverPx, true, 0, false, &j->pic, NULL, NULL, true) == PIC_OK;
            s_lastPic[0] = 0;
        } else if (j->track[0]) {
            got = CoverFromTag(j->track, &j->pic);
            if (got) {
                s_lastPic[0] = 0;
            } else if (CoverFolderPicture(j->track, pic)) {
                j->same = !wcscmp(pic, s_lastPic);
                got = j->same || PicLoad(pic, kCoverPx, kCoverPx, true, 0, true, &j->pic, NULL, NULL, true) == PIC_OK;
                wcscpy(s_lastPic, got ? pic : L"");
            } else {
                s_lastPic[0] = 0;
            }
        }
        if (!PostMessage(s_wnd, WM_APP_COVER, 0, (LPARAM)j)) {
            PicFree(&j->pic);
            free(j);
        }
    }
    return 0;
}

// The cover the screen wants now: the track loaded, otherwise the last one played (▶ goes on with it).
static void CoverWant()
{
    wchar_t p[MAX_PATH] = L"";
    if (s_cvPick)
        wcsncpy(p, s_cvPick, MAX_PATH - 1);
    else if (s_chan && s_plCur >= 0 && s_plCur < s_plCount)
        _snwprintf(p, MAX_PATH, L"%s%s", s_plDir, s_pl[s_plCur]);
    else if (s_cfg.musicFile[0])
        _snwprintf(p, MAX_PATH, L"%s%s", s_cfg.musicDir, s_cfg.musicFile);
    p[MAX_PATH - 1] = 0;
    if (!wcscmp(p, s_cvAsked))
        return;
    wcscpy(s_cvAsked, p);
    if (!s_cvThread) {
        InitializeCriticalSection(&s_cvCs);
        s_cvEv = CreateEvent(NULL, FALSE, FALSE, NULL);
        s_cvThread = CreateThread(NULL, 0, CoverThread, NULL, 0, NULL);
        VLog(L"поток обложек: %s", s_cvThread ? L"запущен" : L"не запустился");
    }
    EnterCriticalSection(&s_cvCs);
    wcscpy(s_cvWant, p);
    LeaveCriticalSection(&s_cvCs);
    if (s_cvEv)
        SetEvent(s_cvEv);
}

// WM_APP_COVER: a result in; true when it is the one wanted now (the screen redraws).
static bool CoverTake(CoverJob* j)
{
    bool wanted = !wcscmp(j->track, s_cvAsked);
    if (wanted && !j->same) {
        CoverFree(&s_cv, 1);
        CoverFree(&s_cvBack, 1);
        CoverFree(s_cvSized, kCoverSizes);
        s_cv = j->pic;
    } else {
        PicFree(&j->pic);
    }
    free(j);
    return wanted;
}

// ---- a song's cover from the net (PLAN.md, the user 2026-11-17): the phone's music over Bluetooth, the internet
// radio's song - iTunes Search (webcover.cpp), one worker, the pictures kept on the SD card / USB disk (Covers\),
// without one in RAM (\Temp); a song not found is not asked again. The UI thread asks (WebCover) and is told
// (WM_APP_WEBCOVER).
#define WM_APP_WEBCOVER (WM_APP + 32)
static CRITICAL_SECTION s_wcCs;
static HANDLE   s_wcEv, s_wcThread;
static char     s_wcArtist[128], s_wcTitle[128];   // the song wanted (UTF-8), under s_wcCs
static unsigned long s_wcKey;                      // its key (the UI thread's)
static wchar_t  s_wcFile[MAX_PATH];                // its picture once there, "" not (yet)

static void WebCoverDir(wchar_t* dir)
{
#ifdef UNDER_CE
    const wchar_t* vols[2] = { L"\\SDMemory", L"\\USBDevice" };
    for (int i = 0; i < 2; i++) {
        DWORD a = GetFileAttributes(vols[i]);
        if (a != 0xFFFFFFFF && (a & FILE_ATTRIBUTE_DIRECTORY)) {
            _snwprintf(dir, MAX_PATH, L"%s\\VestaHome\\", vols[i]);
            CreateDirectory(dir, NULL);
            wcscat(dir, L"Covers\\");
            CreateDirectory(dir, NULL);
            return;
        }
    }
    wcscpy(dir, L"\\Temp\\VestaCovers\\");
    CreateDirectory(L"\\Temp", NULL);
    CreateDirectory(dir, NULL);
#else
    _snwprintf(dir, MAX_PATH, L"%sCovers\\", s_dir);
    CreateDirectory(dir, NULL);
#endif
}

static void WebCoverFile(unsigned long key, wchar_t* path)
{
    wchar_t dir[MAX_PATH];
    WebCoverDir(dir);
    _snwprintf(path, MAX_PATH, L"%s%08lX.jpg", dir, key);
    path[MAX_PATH - 1] = 0;
}

// The song's picture searched: Yandex Music first (its servers in Russia), iTunes when it has nothing (a larger
// catalogue - the user 2026-11-17); its host and path into host / art.
static bool WebCoverFind(const char* artist, const char* title, char* host, char* art)
{
    char path[512];
    unsigned char* body = NULL;
    int len = 0, status = 0;
    YandexSearch(artist, title, path, sizeof(path));
    bool found = HttpGet("api.music.yandex.net", true, path, &body, &len, &status) == NET_OK && status == 200 &&
                 YandexArt((const char*)body, host, 64, art, 256);
    free(body);
    if (found)
        return true;
    body = NULL;
    WebCoverSearch(artist, title, path, sizeof(path));
    found = HttpGet("itunes.apple.com", true, path, &body, &len, &status) == NET_OK && status == 200 &&
            WebCoverArt((const char*)body, host, 64, art, 256);
    free(body);
    return found;
}

// The cache in RAM (no card): over kWebRamMax pictures - the others than the one just saved go (the unit's memory).
enum { kWebRamMax = 60 };
static void WebCoverPrune(const wchar_t* keep)
{
    if (wcsncmp(keep, L"\\Temp\\", 6))
        return;
    wchar_t mask[MAX_PATH], dir[MAX_PATH];
    wcscpy(dir, keep);
    *(wcsrchr(dir, L'\\') + 1) = 0;
    _snwprintf(mask, MAX_PATH, L"%s*.jpg", dir);
    int n = 0;
    WIN32_FIND_DATA fd;
    HANDLE h = FindFirstFile(mask, &fd);
    for (BOOL ok = h != INVALID_HANDLE_VALUE; ok; ok = FindNextFile(h, &fd))
        n++;
    if (h != INVALID_HANDLE_VALUE)
        FindClose(h);
    if (n <= kWebRamMax)
        return;
    h = FindFirstFile(mask, &fd);
    for (BOOL ok = h != INVALID_HANDLE_VALUE; ok; ok = FindNextFile(h, &fd)) {
        wchar_t p[MAX_PATH];
        _snwprintf(p, MAX_PATH, L"%s%s", dir, fd.cFileName);
        if (_wcsicmp(p, keep))
            DeleteFile(p);
    }
    if (h != INVALID_HANDLE_VALUE)
        FindClose(h);
}

static bool WebCoverFetch(const char* artist, const char* title, const wchar_t* file)
{
    char host[64], art[256];
    if (!WebCoverFind(artist, title, host, art))
        return false;
    unsigned char* body = NULL;
    int len = 0, status = 0;
    bool ok = HttpGet(host, true, art, &body, &len, &status) == NET_OK && status == 200 && len > 100;
    if (ok) {
        HANDLE f = CreateFile(file, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        DWORD put = 0;
        ok = f != INVALID_HANDLE_VALUE && WriteFile(f, body, len, &put, NULL) && (int)put == len;
        if (f != INVALID_HANDLE_VALUE)
            CloseHandle(f);
        if (!ok)
            DeleteFile(file);
        else
            WebCoverPrune(file);
    }
    free(body);
    return ok;
}

static DWORD WINAPI WebCoverThread(LPVOID)
{
    static unsigned long s_miss[64];   // songs not found (the worker's): not asked again
    static int s_missN;
    for (;;) {
        WaitForSingleObject(s_wcEv, INFINITE);
        char a[128], t[128];
        EnterCriticalSection(&s_wcCs);
        strcpy(a, s_wcArtist);
        strcpy(t, s_wcTitle);
        LeaveCriticalSection(&s_wcCs);
        unsigned long key = WebCoverKey(a, t);
        bool missed = false;
        for (int i = 0; i < s_missN && !missed; i++)
            missed = s_miss[i] == key;
        wchar_t file[MAX_PATH];
        WebCoverFile(key, file);
        if (missed || GetFileAttributes(file) != 0xFFFFFFFF)
            continue;
        bool ok = WebCoverFetch(a, t, file);
        VLog(L"обложка из сети: %S - %S: %s", a, t, ok ? L"есть" : L"не найдена");
        if (ok)
            PostMessage(s_wnd, WM_APP_WEBCOVER, (WPARAM)key, 0);
        else
            s_miss[s_missN++ % 64] = key;
        Sleep(2000);   // (one search in two seconds at most)
    }
    return 0;
}

// The song's picture when it is there (in the cache), otherwise asked for; NULL - none (yet).
static const wchar_t* WebCover(const char* artist, const char* title)
{
    if (!title[0])
        return NULL;
    unsigned long key = WebCoverKey(artist, title);
    if (key == s_wcKey)
        return s_wcFile[0] ? s_wcFile : NULL;
    s_wcKey = key;
    wchar_t file[MAX_PATH];
    WebCoverFile(key, file);
    if (GetFileAttributes(file) != 0xFFFFFFFF) {
        wcscpy(s_wcFile, file);
        return s_wcFile;
    }
    s_wcFile[0] = 0;
    if (!s_wcThread) {
        InitializeCriticalSection(&s_wcCs);
        s_wcEv = CreateEvent(NULL, FALSE, FALSE, NULL);
        s_wcThread = CreateThread(NULL, 0, WebCoverThread, NULL, 0, NULL);
    }
    EnterCriticalSection(&s_wcCs);
    strncpy(s_wcArtist, artist, 127);
    strncpy(s_wcTitle, title, 127);
    s_wcArtist[127] = s_wcTitle[127] = 0;
    LeaveCriticalSection(&s_wcCs);
    SetEvent(s_wcEv);
    return NULL;
}

// The song's picture if it is already there (asked before, or in the cache) - no search; NULL none.
static const wchar_t* WebCoverHad(const char* artist, const char* title)
{
    unsigned long key = WebCoverKey(artist, title);
    if (key == s_wcKey)
        return s_wcFile[0] ? s_wcFile : NULL;
    wchar_t file[MAX_PATH];
    WebCoverFile(key, file);
    if (GetFileAttributes(file) == 0xFFFFFFFF)
        return NULL;
    s_wcKey = key;
    wcscpy(s_wcFile, file);
    return s_wcFile;
}

static bool WebCoverTake(unsigned long key)   // WM_APP_WEBCOVER: true - the song wanted now (to redraw)
{
    if (key != s_wcKey)
        return false;
    WebCoverFile(key, s_wcFile);
    return true;
}

// The phone's song (AVRCP, Bluetooth): its cover file, NULL none (yet).
static const wchar_t* BtCoverFile()
{
    BtState bt;
    if (!BtGet(&bt, NULL) || !bt.title[0])
        return NULL;
    return WebCover(bt.artist, bt.title);
}

static bool CoverHave()
{
    CoverWant();
    return s_cv.px32 != NULL;
}

static void BlitPic(const Pic& p, int x, int y, int rad = 0, bool lighter = false);   // (vh_photo.inc)

// The cover s x s at x, y, its corners rounded by rad; false: none (the caller draws its placeholder).
static bool CoverArt(int x, int y, int s, int rad)
{
    if (!CoverHave())
        return false;
    const Pic* p = &s_cv;
    if (s != kCoverPx) {
        p = NULL;
        for (int i = 0; i < kCoverSizes && !p; i++)
            if (s_cvSized[i].px32 && s_cvSized[i].w == s)
                p = &s_cvSized[i];
        for (int i = 0; i < kCoverSizes && !p; i++)
            if (!s_cvSized[i].px32 && PicFit(&s_cv, s, s, true, &s_cvSized[i], true) == PIC_OK)
                p = &s_cvSized[i];
        if (!p)
            return false;
    }
    BlitPic(*p, x, y, rad);
    return true;
}

// The cover behind a card x, y, w x h (corners rad): covering the card, sharp, dimmed to kCoverDim of 256 so the
// text stays readable on it. false: none (the card's own colour).
enum { kCoverDim = 72 };
static void PicDim(Pic* p)   // its 32-bit pixels to kCoverDim of 256
{
    for (int i = 0; i < p->w * p->h; i++) {
        DWORD c = p->px32[i];
        p->px32[i] = ((c >> 16 & 255) * kCoverDim >> 8) << 16 | ((c >> 8 & 255) * kCoverDim >> 8) << 8 |
                     ((c & 255) * kCoverDim >> 8);
    }
}

static bool CoverBack(int x, int y, int w, int h, int rad)
{
    if (!CoverHave())
        return false;
    if (!s_cvBack.px32 || s_cvBack.w != w || s_cvBack.h != h) {
        PicFree(&s_cvBack);
        if (PicFit(&s_cv, w, h, true, &s_cvBack, true) != PIC_OK)
            return false;
        PicDim(&s_cvBack);
    }
    BlitPic(s_cvBack, x, y, rad);
    return true;
}
