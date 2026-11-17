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
static bool      s_cvAddon;   // the cover wanted: the add-on's (its widget is being drawn), not our music's
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
    if (s_cvAddon)
        wcscpy(p, s_ap.art);
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

static bool CoverHave()
{
    CoverWant();
    return s_cv.px32 != NULL;
}

static void BlitPic(const Pic& p, int x, int y, int rad);   // (vh_photo.inc)

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
static bool CoverBack(int x, int y, int w, int h, int rad)
{
    if (!CoverHave())
        return false;
    if (!s_cvBack.px32 || s_cvBack.w != w || s_cvBack.h != h) {
        PicFree(&s_cvBack);
        if (PicFit(&s_cv, w, h, true, &s_cvBack, true) != PIC_OK)
            return false;
        for (int i = 0; i < w * h; i++) {
            DWORD c = s_cvBack.px32[i];
            s_cvBack.px32[i] = ((c >> 16 & 255) * kCoverDim >> 8) << 16 | ((c >> 8 & 255) * kCoverDim >> 8) << 8 |
                               ((c & 255) * kCoverDim >> 8);
        }
    }
    BlitPic(s_cvBack, x, y, rad);
    return true;
}
