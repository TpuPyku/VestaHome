// vh_netradio.inc - part of home.cpp (included there, in order): internet radio.

// ---- internet radio (Media -> "Интернет"): the stations of radio.txt next to the exe ("name|address" lines),
// played by BASS from our own HTTP / HTTPS reader (https.cpp: BearSSL, redirects; the BASS for CE has no HTTPS):
// BASS_StreamCreateFileUser with a buffered "file" in blocks (nothing kept after it is played) whose read
// callback takes the ICY metadata (the title of the song) out of the audio. MP3 and OGG by BASS, AAC / AAC+ by
// bass_aac. The connection is made in a thread of its own (it takes seconds); a broken stream is opened again
// after 3, 6 ... 60 s while the radio is on.
#define WM_APP_NET (WM_APP + 22)
#define WM_APP_WIFI (WM_APP + 24)
enum { NET_READY = 1, NET_FAILED, NET_ENDED, NET_META, NET_STALL, kNetMax = 100 };
enum { NR_OFF, NR_CONNECT, NR_PLAY, NR_STALL, NR_WAIT, NR_ERROR };

// The stations come in lists the player steps through (the user 2026-11-04: switched like playlists): ours
// (radio.txt), the favourites (radio_fav.txt beside it, a star on any station), a page of the catalogue
// (radio-browser.info, netcat.h), the recent ones (radio_recent.txt, the latest played first). The station playing is
// a copy: it plays on when its list changes. (The numbers are kept in VestaHome.ini's radio.list.)
enum { NL_MINE, NL_FAV, NL_CAT, NL_RECENT, kNetLists, kNetRecent = 20 };
struct NetList { NcStation* st; int n; };
// A stream's "file" - owned by its references (the user 2026-11-03: VestaHome fell, C0000005, 17-40 s after the station
// was changed, in none of our guarded threads: BASS's reading thread read the HTTP stream while the close freed it,
// then the file itself was freed under it): the owner's one and each read's own; the HTTP stream closed and the file
// freed with the last one (NetFileRelease). A close only marks it and breaks a blocked read.
struct NetFile {
    HttpStream* http;     // (under s_netCs)
    int         metaint;  // the ICY block size (0: no metadata)
    int         left;     // audio bytes before the next metadata block
    LONG        gen;
    int         refs;     // (under s_netCs)
    bool        closed;   // BASS let it go: reads end
};
struct NetReady { LONG gen; DWORD chan; NetFile* file; };

static NetList     s_nl[kNetLists];    // read when the screen is first opened (the catalogue's: when asked)
static bool        s_netLoaded;
static NcStation   s_netNow;       // the station chosen (url "" none)
static int         s_netWhich;     // its list: what the player steps through
static int         s_netCur = -1;  // its place there (-1: no longer in it)
static int         s_netView;      // the list the list screen shows
static bool        s_netOn;        // the radio should play: reconnects while on
static int         s_netState;     // NR_*
static DWORD       s_netChan;
static NetFile*    s_netFile;      // the playing stream's
static NetFile*    s_netOpening;   // the stream BASS_StreamCreateFileUser is reading now (under s_netCs)
static LONG       s_netGen;       // the attempt: an older thread's result is thrown away
static DWORD       s_netRetryAt;
static int         s_netRetries;
static bool        s_netFatal;     // the last failure was the format, not the network: no retry
static wchar_t     s_netMsg[96];   // the state or the error (under s_netCs while the thread writes it)
static wchar_t     s_netTitle[160]; // the song (ICY StreamTitle), under s_netCs
static wchar_t     s_netInfo[64];   // MP3 / AAC / OGG
static CRITICAL_SECTION s_netCs;

static int CrashNote(EXCEPTION_POINTERS* e, const wchar_t* where);   // (home.cpp)

static void NetInit()
{
    InitializeCriticalSection(&s_netCs);
}

static const wchar_t* kNetFile[kNetLists] = { L"radio.txt", L"radio_fav.txt", NULL, L"radio_recent.txt" };

static void NetReadList(int which)   // radio.txt, radio_fav.txt, radio_recent.txt beside the exe, a station a line
{
    NetList& l = s_nl[which];
    l.n = 0;
    wchar_t path[MAX_PATH];
    _snwprintf(path, MAX_PATH, L"%s%s", s_dir, kNetFile[which]);
    DWORD size;
    char* buf = l.st ? ReadAll(path, &size) : NULL;
    if (!buf)
        return;
    char* p = buf;
    if ((unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF)
        p += 3;
    while (*p && l.n < kNetMax) {
        char* end = strchr(p, '\n');
        if (end)
            *end = 0;
        if (NcParseLine(p, &l.st[l.n]))
            l.n++;
        p = end ? end + 1 : p + strlen(p);
    }
    free(buf);
}

static int NetFind(int which, const char* url)
{
    for (int i = 0; i < s_nl[which].n; i++)
        if (!strcmp(s_nl[which].st[i].url, url))
            return i;
    return -1;
}

static void NetLoadList()
{
    if (s_netLoaded)
        return;
    s_netLoaded = true;
    for (int i = 0; i < kNetLists; i++)
        s_nl[i].st = (NcStation*)calloc(kNetMax, sizeof(NcStation));
    NetReadList(NL_MINE);
    NetReadList(NL_FAV);
    NetReadList(NL_RECENT);
    // the station last chosen, in its list (radio.list, radio.url; before them: by its name in radio.txt)
    char url[400];
    WideCharToMultiByte(CP_UTF8, 0, s_cfg.radioUrl, -1, url, sizeof(url), NULL, NULL);
    int which = s_cfg.radioList >= 0 && s_cfg.radioList < kNetLists ? s_cfg.radioList : NL_MINE;
    int at = which != NL_CAT && url[0] ? NetFind(which, url) : -1;
    for (int i = 0; at < 0 && which == NL_MINE && i < s_nl[NL_MINE].n; i++)
        if (!wcscmp(s_nl[NL_MINE].st[i].name, s_cfg.radioStation))
            at = i;
    if (at >= 0) {
        s_netNow = s_nl[which].st[at];
    } else if (url[0] && s_cfg.radioStation[0]) {   // a catalogue's station (its page is not kept): itself
        memset(&s_netNow, 0, sizeof(s_netNow));
        wcscpy(s_netNow.name, s_cfg.radioStation);
        strcpy(s_netNow.url, url);
    }
    s_netWhich = s_netView = which;
    s_netCur = at;
}

static const NcStation* NetCur()   // the station chosen, NULL none
{
    return s_netNow.url[0] ? &s_netNow : NULL;
}

static bool NetIsFav(const NcStation& s)
{
    return NetFind(NL_FAV, s.url) >= 0;
}

// (rare: a star tapped, a station chosen - the flash's wear does not matter: VestaHome.ini is written then too)
static void NetSaveList(int which)
{
    wchar_t path[MAX_PATH];
    _snwprintf(path, MAX_PATH, L"%s%s", s_dir, kNetFile[which]);
    HANDLE f = CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) {
        VLog(L"радио: не записать %s", path);
        return;
    }
    DWORD w;
    static const char head[] = "# VestaHome: the internet radio's stations, name|address|catalogue id|second line\r\n";
    WriteFile(f, head, sizeof(head) - 1, &w, NULL);
    for (int i = 0; i < s_nl[which].n; i++) {
        char line[700];
        int n = NcFormatLine(s_nl[which].st[i], line, sizeof(line) - 2);
        line[n++] = '\r';
        line[n++] = '\n';
        WriteFile(f, line, n, &w, NULL);
    }
    CloseHandle(f);
}

// The star: in the favourites or out of them. The station playing keeps playing; its place is found again.
static void NetFavToggle(const NcStation& s)
{
    NetList& f = s_nl[NL_FAV];
    int at = NetFind(NL_FAV, s.url);
    if (at >= 0) {
        memmove(&f.st[at], &f.st[at + 1], (f.n - at - 1) * sizeof(NcStation));
        f.n--;
    } else if (f.n < kNetMax && f.st) {
        f.st[f.n++] = s;
    }
    VLog(at >= 0 ? L"радио: «%s» убрана из избранного" : L"радио: «%s» добавлена в избранное", s.name);
    NetSaveList(NL_FAV);
    if (s_netNow.url[0])
        s_netCur = NetFind(s_netWhich, s_netNow.url);
}

static bool ValidUtf8(const unsigned char* s)
{
    while (*s) {
        int n = *s < 0x80 ? 0 : (*s & 0xE0) == 0xC0 ? 1 : (*s & 0xF0) == 0xE0 ? 2 : (*s & 0xF8) == 0xF0 ? 3 : -1;
        if (n < 0)
            return false;
        s++;
        for (; n > 0; n--, s++)
            if ((*s & 0xC0) != 0x80)
                return false;
    }
    return true;
}

// Russian stations send their titles in UTF-8 or in Windows-1251.
static void DecodeTitle(const char* s, wchar_t* w, int cap)
{
    if (ValidUtf8((const unsigned char*)s) && MultiByteToWideChar(CP_UTF8, 0, s, -1, w, cap))
        return;
    int i = 0;
    for (; s[i] && i < cap - 1; i++) {   // 1251 by hand: the unit may lack the code page
        unsigned char c = (unsigned char)s[i];
        w[i] = c >= 0xC0 ? (wchar_t)(0x410 + c - 0xC0) : c == 0xA8 ? 0x401 : c == 0xB8 ? 0x451 : c < 0x80 ? c : L'?';
    }
    w[i] = 0;
}

// A metadata block: "StreamTitle='Artist - Song';StreamUrl='';"
static void NetMeta(const char* m, LONG gen)
{
    const char* p = strstr(m, "StreamTitle='");
    if (!p)
        return;
    p += 13;
    const char* e = strstr(p, "';");
    if (!e)
        e = p + strlen(p);
    char t[320];
    int n = (int)(e - p) < 319 ? (int)(e - p) : 319;
    memcpy(t, p, n);
    t[n] = 0;
    wchar_t w[160];
    DecodeTitle(t, w, 160);
    Trim(w);
    EnterCriticalSection(&s_netCs);
    bool changed = wcscmp(w, s_netTitle) != 0;
    if (changed)
        wcscpy(s_netTitle, w);
    LeaveCriticalSection(&s_netCs);
    if (changed)
        PostMessage(s_wnd, WM_APP_NET, NET_META, gen);
}

static void NetFileRelease(NetFile* f)   // a reference let go; the last one closes the stream, frees the file
{
    if (!f)
        return;
    EnterCriticalSection(&s_netCs);
    bool last = --f->refs <= 0;
    HttpStream* h = last ? f->http : NULL;
    if (last)
        f->http = NULL;
    LeaveCriticalSection(&s_netCs);
    if (h)
        HttpStreamClose(h);
    if (last)
        free(f);
}

static bool NetReadFull(NetFile* f, HttpStream* h, char* buf, int len)
{
    while (len > 0) {
        int k = HttpStreamRead(h, buf, len);
        if (k <= 0)
            return false;
        buf += k;
        len -= k;
    }
    return true;
}

// BASS's download thread: the audio bytes; the metadata blocks are taken out. 0 = the end of the "file".
static DWORD NetReadBody(NetFile* f, HttpStream* h, void* buf, DWORD len)
{
    for (;;) {
        if (f->metaint && f->left == 0) {
            unsigned char l;
            if (!NetReadFull(f, h, (char*)&l, 1))
                return 0;
            if (l) {
                char m[16 * 255 + 1];
                if (!NetReadFull(f, h, m, l * 16))
                    return 0;
                m[l * 16] = 0;
                NetMeta(m, f->gen);
            }
            f->left = f->metaint;
            continue;
        }
        int want = (int)len;
        if (f->metaint && want > f->left)
            want = f->left;
        int k = HttpStreamRead(h, (char*)buf, want);
        if (k <= 0)
            return 0;   // broken or closed: BASS plays what it has, then the END sync
        if (f->metaint)
            f->left -= k;
        return (DWORD)k;
    }
}

static DWORD CALLBACK NetReadProc(void* buf, DWORD len, void* user)   // (BASS's thread: a crash logged, not ours)
{
    NetFile* f = (NetFile*)user;
    EnterCriticalSection(&s_netCs);
    HttpStream* h = f->closed ? NULL : f->http;
    if (h)
        f->refs++;   // (the file and its stream stay while this read is on)
    LeaveCriticalSection(&s_netCs);
    if (!h)
        return 0;
    DWORD r = 0;
    __try {
        r = NetReadBody(f, h, buf, len);
    } __except (CrashNote(GetExceptionInformation(), L"чтении радио-сети")) {
        r = 0;
    }
    NetFileRelease(f);
    return r;
}

static void CALLBACK NetCloseProc(void* user)   // BASS lets the file go: reads end (the owner's release frees it)
{
    NetFile* f = (NetFile*)user;
    EnterCriticalSection(&s_netCs);
    f->closed = true;
    if (f->http)
        HttpStreamAbort(f->http);
    LeaveCriticalSection(&s_netCs);
}

static QWORD CALLBACK NetLenProc(void*) { return 0; }      // unknown: endless
static BOOL CALLBACK NetSeekProc(QWORD, void*) { return FALSE; }

static const BassFileProcs kNetProcs = { NetCloseProc, NetLenProc, NetReadProc, NetSeekProc };

static void NetAbortFile(NetFile* f)   // the blocked read returns: BASS can free the stream at once
{
    EnterCriticalSection(&s_netCs);
    if (f && f->http)
        HttpStreamAbort(f->http);
    LeaveCriticalSection(&s_netCs);
}

static void CALLBACK NetEndSync(DWORD, DWORD, DWORD, void* user)
{
    PostMessage(s_wnd, WM_APP_NET, NET_ENDED, (LPARAM)user);
}

static void CALLBACK NetStallSync(DWORD, DWORD, DWORD data, void* user)
{
    PostMessage(s_wnd, WM_APP_NET, NET_STALL | (data << 8), (LPARAM)user);   // data 0 stalled, 1 going on
}

struct NetStart { LONG gen; char url[400]; };

static DWORD NetThreadRun(LPVOID p);

static DWORD WINAPI NetThread(LPVOID p)   // (a crash's place into the log first)
{
    __try {
        return NetThreadRun(p);
    } __except (CrashNote(GetExceptionInformation(), L"потоке радио-сети")) {
    }
    return 0;
}

static DWORD NetThreadRun(LPVOID p)
{
    VLog(L"поток радио-сети: id %08X", (unsigned)GetCurrentThreadId());
    NetStart* st = (NetStart*)p;
    int err = 0, metaint = 0;
    char name[96], type[64];
    HttpStream* h = HttpStreamOpen(st->url, &err, &metaint, name, sizeof(name), type, sizeof(type));
    NetFile* f = NULL;
    DWORD chan = 0;
    wchar_t msg[96] = L"";
    if (!h) {
        _snwprintf(msg, 96, L"%s", NetErrorText(err));
    } else {
        f = (NetFile*)calloc(1, sizeof(NetFile));
        if (f) {
            f->refs = 1;   // (the owner's: the thread's, then the window's)
            f->http = h;
            f->metaint = metaint;
            f->left = metaint;
            f->gen = st->gen;
            EnterCriticalSection(&s_netCs);
            s_netOpening = f;
            LeaveCriticalSection(&s_netCs);
            if (st->gen == s_netGen)   // reads the start of the stream to know its format (blocks)
                chan = s_bass.streamUser(BASS_STREAMFILE_BUFFER_F, BASS_STREAM_BLOCK_F, &kNetProcs, f);
            int berr = chan ? 0 : s_bass.error();
            EnterCriticalSection(&s_netCs);
            s_netOpening = NULL;
            LeaveCriticalSection(&s_netCs);
            if (!chan) {
                NetFileRelease(f);   // (the stream closed with it)
                f = NULL;
                _snwprintf(msg, 96, L"поток не играет (BASS %d)", berr);
            }
        } else {
            HttpStreamClose(h);
            wcscpy(msg, NetErrorText(NET_MEMORY));
        }
    }
    if (chan) {
        EnterCriticalSection(&s_netCs);
        wcscpy(s_netInfo, strstr(type, "aac") ? L"AAC" : strstr(type, "ogg") ? L"OGG" : strstr(type, "mpeg") ? L"MP3" : L"");
        LeaveCriticalSection(&s_netCs);
        NetReady* r = (NetReady*)malloc(sizeof(NetReady));
        if (r) {
            r->gen = st->gen;
            r->chan = chan;
            r->file = f;
        }
        if (!r || !PostMessage(s_wnd, WM_APP_NET, NET_READY, (LPARAM)r)) {
            NetAbortFile(f);
            s_bass.streamFree(chan);
            NetFileRelease(f);
            free(r);
        }
    } else if (st->gen == s_netGen) {   // (an older attempt, stopped or replaced, ends quietly)
        EnterCriticalSection(&s_netCs);
        wcscpy(s_netMsg, msg);
        s_netFatal = h != NULL;   // connected, but BASS doesn't take it
        LeaveCriticalSection(&s_netCs);
        VLog(L"радио: %S — %s", st->url, msg);
        PostMessage(s_wnd, WM_APP_NET, NET_FAILED, st->gen);
    }
    free(st);
    return 0;
}

static void NetFreeChan()
{
    if (s_netChan) {
        NetAbortFile(s_netFile);
        s_bass.streamFree(s_netChan);   // calls NetCloseProc
        s_netChan = 0;
    }
    NetFileRelease(s_netFile);
    s_netFile = NULL;
}

static void NetConnect()
{
    if (!NetCur())
        return;
    NetStart* st = (NetStart*)malloc(sizeof(NetStart));
    if (!st)
        return;
    st->gen = InterlockedIncrement(&s_netGen);
    strcpy(st->url, s_netNow.url);
    s_netState = NR_CONNECT;
    EnterCriticalSection(&s_netCs);
    wcscpy(s_netMsg, L"подключение…");
    s_netTitle[0] = s_netInfo[0] = 0;
    LeaveCriticalSection(&s_netCs);
    HANDLE t = CreateThread(NULL, 0, NetThread, st, 0, NULL);
    if (t)
        CloseHandle(t);
    else
        free(st);
}

static void NetStop()
{
    if (!s_netOn && !s_netChan)
        return;
    s_netOn = false;
    InterlockedIncrement(&s_netGen);   // a connection under way is thrown away when it comes
    EnterCriticalSection(&s_netCs);
    if (s_netOpening && s_netOpening->http)
        HttpStreamAbort(s_netOpening->http);
    LeaveCriticalSection(&s_netCs);
    NetFreeChan();
    s_netState = NR_OFF;
    s_netMsg[0] = 0;
}

static void NetCatClick(const char* uuid);   // (vh_netcat.inc: the catalogue counts its station played)

// The station chosen (s_netNow) on.
static void NetPlayNow()
{
    ResumeByUser();
    s_mediaLast = 2;
    NetLoadList();
    if (!NetCur())
        return;
    if (!BassLoad() || !s_bass.streamUser) {
        wcscpy(s_status, s_bass.state == -1 ? L"нет bass.dll: папка bass рядом с VestaHome" : L"звук не запускается (BASS)");
        return;
    }
    NetStop();
    PlayerStopStream();
    VidPauseForAudio();
    s_netOn = true;
    s_netRetries = 0;
    s_netFatal = false;
    wcscpy(s_cfg.radioStation, s_netNow.name);
    MultiByteToWideChar(CP_UTF8, 0, s_netNow.url, -1, s_cfg.radioUrl, 400);
    s_cfg.radioList = s_netWhich;
    SaveCfg();
    if (s_netWhich != NL_RECENT && s_nl[NL_RECENT].st) {   // (played from the recent ones: they keep their order,
        NetList& r = s_nl[NL_RECENT];                       // or the next would be the one just played)
        r.n = NcRecentPut(r.st, r.n, kNetRecent, s_netNow);
        NetSaveList(NL_RECENT);
    }
    VLog(L"радио: %s", s_netNow.name);
    if (s_netNow.uuid[0])
        NetCatClick(s_netNow.uuid);
    MediaSource();
    NetConnect();
}

static void NetPlay(int which, int i)   // a list's station on: the player steps through that list now
{
    NetLoadList();
    if (which < 0 || which >= kNetLists || i < 0 || i >= s_nl[which].n)
        return;
    s_netWhich = which;
    s_netCur = i;
    s_netNow = s_nl[which].st[i];
    NetPlayNow();
}

static void NetToggle()
{
    NetLoadList();
    if (s_netOn)
        NetStop();
    else if (NetCur())
        NetPlayNow();
    else
        NetPlay(s_netWhich, 0);
}

static void NetStep(int dir)
{
    NetLoadList();
    int n = s_nl[s_netWhich].n;
    if (n > 0)
        NetPlay(s_netWhich, s_netCur < 0 ? 0 : (s_netCur + dir + n) % n);
}

// The thread's results and BASS's syncs (wp = what, lp = the attempt).
static void OnNetMessage(WPARAM wp, LPARAM lp)
{
    int what = (int)(wp & 0xFF);
    if (what == NET_READY) {
        NetReady* r = (NetReady*)lp;
        if (r->gen != s_netGen || !s_netOn) {   // stopped or another station meanwhile
            NetAbortFile(r->file);
            s_bass.streamFree(r->chan);
            NetFileRelease(r->file);
        } else {
            s_netChan = r->chan;
            s_netFile = r->file;
            if (s_bass.setSync) {
                s_bass.setSync(s_netChan, BASS_SYNC_END_F, 0, NetEndSync, (void*)r->gen);
                s_bass.setSync(s_netChan, BASS_SYNC_STALL_F, 0, NetStallSync, (void*)r->gen);
            }
            s_bass.play(s_netChan, FALSE);
            s_netState = NR_PLAY;
            s_netRetries = 0;
            s_netMsg[0] = 0;
        }
        free(r);
        return;
    }
    if ((LONG)lp != s_netGen || !s_netOn)
        return;
    if (what == NET_STALL) {
        if (s_netState == NR_PLAY || s_netState == NR_STALL)
            s_netState = (wp >> 8) ? NR_PLAY : NR_STALL;
    } else if (what == NET_ENDED || what == NET_FAILED) {
        NetFreeChan();
        if (what == NET_FAILED && s_netFatal) {
            s_netState = NR_ERROR;   // the message is the thread's
            return;
        }
        if (what == NET_ENDED)
            wcscpy(s_netMsg, L"связь прервалась");
        int delay = s_netRetries < 5 ? 3 << s_netRetries : 60;
        delay = delay > 60 ? 60 : delay;
        s_netRetries++;
        s_netRetryAt = GetTickCount() + delay * 1000;
        s_netState = NR_WAIT;
    }
}

// Once a second (the window's clock timer): the next attempt.
static void NetTick()
{
    if (s_netOn && s_netState == NR_WAIT && (int)(GetTickCount() - s_netRetryAt) >= 0)
        NetConnect();
}

static void NetStateText(wchar_t* s, int n)
{
    EnterCriticalSection(&s_netCs);
    switch (s_netState) {
    case NR_CONNECT: _snwprintf(s, n, L"%s", s_netMsg); break;
    case NR_PLAY:    _snwprintf(s, n, L"%s%sв эфире", s_netInfo, s_netInfo[0] ? L" · " : L""); break;
    case NR_STALL:   _snwprintf(s, n, L"буферизация…"); break;
    case NR_WAIT: {
        int left = (int)(s_netRetryAt - GetTickCount()) / 1000 + 1;
        _snwprintf(s, n, L"%s — снова через %d с", s_netMsg[0] ? s_netMsg : L"нет связи", left > 0 ? left : 1);
        break;
    }
    case NR_ERROR:   _snwprintf(s, n, L"%s", s_netMsg); break;
    default:         _snwprintf(s, n, L"выключено"); break;
    }
    s[n - 1] = 0;
    LeaveCriticalSection(&s_netCs);
}

static void NetPlayBtn(HDC dc, int cx, int cy, int r, int isz)
{
    bool down = Zone(cx - r, cy - r, 2 * r, 2 * r, A_NRPLAY);
    Disc(dc, cx, cy, r, down ? kFacet : kOrange);
    Ico(dc, s_netOn ? IC_PAUSE : IC_PLAY, cx - isz / 2, cy - isz / 2, isz, kInk);
}

// (the player's and the lists' screens: vh_netcat.inc)
