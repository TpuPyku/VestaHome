// vh_netradio.inc - part of home.cpp (included there, in order): internet radio.

// ---- internet radio (Media -> "Интернет"): the stations of radio.txt next to the exe ("name|address" lines),
// played by BASS from our own HTTP / HTTPS reader (https.cpp: BearSSL, redirects; the BASS for CE has no HTTPS):
// BASS_StreamCreateFileUser with a buffered "file" in blocks (nothing kept after it is played) whose read
// callback takes the ICY metadata (the title of the song) out of the audio. MP3 and OGG by BASS, AAC / AAC+ by
// bass_aac. The connection is made in a thread of its own (it takes seconds); a broken stream is opened again
// after 3, 6 ... 60 s while the radio is on.
#define WM_APP_NET (WM_APP + 22)
#define WM_APP_WIFI (WM_APP + 24)
enum { NET_READY = 1, NET_FAILED, NET_ENDED, NET_META, NET_STALL, kNetMax = 100, kNetRows = 5 };
enum { NR_OFF, NR_CONNECT, NR_PLAY, NR_STALL, NR_WAIT, NR_ERROR };

struct NetStation { wchar_t name[64]; char url[400]; };
struct NetFile {
    HttpStream* http;     // NULL once closed (under s_netCs)
    int         metaint;  // the ICY block size (0: no metadata)
    int         left;     // audio bytes before the next metadata block
    LONG        gen;
};
struct NetReady { LONG gen; DWORD chan; NetFile* file; };

static NetStation* s_stations;          // radio.txt, read when the screen is first opened
static int         s_netCount = -1;
static int         s_netCur = -1;  // the station chosen
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

static void NetInit()
{
    InitializeCriticalSection(&s_netCs);
}

static void NetLoadList()
{
    if (s_netCount >= 0)
        return;
    s_netCount = 0;
    s_stations = (NetStation*)calloc(kNetMax, sizeof(NetStation));
    wchar_t path[MAX_PATH];
    _snwprintf(path, MAX_PATH, L"%sradio.txt", s_dir);
    DWORD size;
    char* buf = s_stations ? ReadAll(path, &size) : NULL;
    if (!buf)
        return;
    char* p = buf;
    if ((unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF)
        p += 3;
    while (*p && s_netCount < kNetMax) {
        char* end = strchr(p, '\n');
        if (end)
            *end = 0;
        int n = (int)strlen(p);
        while (n > 0 && (p[n - 1] == '\r' || p[n - 1] == ' ' || p[n - 1] == '\t'))
            p[--n] = 0;
        char* bar = strchr(p, '|');
        if (p[0] != '#' && bar && (!_strnicmp(bar + 1, "http://", 7) || !_strnicmp(bar + 1, "https://", 8))) {
            *bar = 0;
            NetStation& s = s_stations[s_netCount];
            MultiByteToWideChar(CP_UTF8, 0, p, -1, s.name, 64);
            s.name[63] = 0;
            Trim(s.name);
            strncpy(s.url, bar + 1, sizeof(s.url) - 1);
            if (s.name[0])
                s_netCount++;
        }
        p = end ? end + 1 : p + strlen(p);
    }
    free(buf);
    for (int i = 0; i < s_netCount; i++)
        if (!wcscmp(s_stations[i].name, s_cfg.radioStation))
            s_netCur = i;
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

static bool NetReadFull(NetFile* f, char* buf, int len)
{
    while (len > 0) {
        int k = HttpStreamRead(f->http, buf, len);
        if (k <= 0)
            return false;
        buf += k;
        len -= k;
    }
    return true;
}

// BASS's download thread: the audio bytes; the metadata blocks are taken out. 0 = the end of the "file".
static DWORD CALLBACK NetReadProc(void* buf, DWORD len, void* user)
{
    NetFile* f = (NetFile*)user;
    for (;;) {
        if (f->metaint && f->left == 0) {
            unsigned char l;
            if (!NetReadFull(f, (char*)&l, 1))
                return 0;
            if (l) {
                char m[16 * 255 + 1];
                if (!NetReadFull(f, m, l * 16))
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
        int k = HttpStreamRead(f->http, (char*)buf, want);
        if (k <= 0)
            return 0;   // broken or closed: BASS plays what it has, then the END sync
        if (f->metaint)
            f->left -= k;
        return (DWORD)k;
    }
}

static void CALLBACK NetCloseProc(void* user)
{
    NetFile* f = (NetFile*)user;
    EnterCriticalSection(&s_netCs);
    if (f->http)
        HttpStreamClose(f->http);
    f->http = NULL;
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

static DWORD WINAPI NetThread(LPVOID p)
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
            if (!chan && f->http)
                HttpStreamClose(f->http);
            if (!chan)
                f->http = NULL;
            LeaveCriticalSection(&s_netCs);
            if (!chan) {
                free(f);
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
            free(f);
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
    free(s_netFile);
    s_netFile = NULL;
}

static void NetConnect()
{
    if (s_netCur < 0 || s_netCur >= s_netCount)
        return;
    NetStart* st = (NetStart*)malloc(sizeof(NetStart));
    if (!st)
        return;
    st->gen = InterlockedIncrement(&s_netGen);
    strcpy(st->url, s_stations[s_netCur].url);
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

static void NetPlay(int i)
{
    ResumeByUser();
    s_mediaLast = 2;
    NetLoadList();
    if (i < 0 || i >= s_netCount)
        return;
    if (!BassLoad() || !s_bass.streamUser) {
        wcscpy(s_status, s_bass.state == -1 ? L"нет bass.dll: папка bass рядом с VestaHome" : L"звук не запускается (BASS)");
        return;
    }
    NetStop();
    PlayerStopStream();
    VidPauseForAudio();
    s_netCur = i;
    s_netOn = true;
    s_netRetries = 0;
    s_netFatal = false;
    wcscpy(s_cfg.radioStation, s_stations[i].name);
    SaveCfg();
    VLog(L"радио: %s", s_stations[i].name);
    MediaSource();
    NetConnect();
}

static void NetToggle()
{
    NetLoadList();
    if (s_netOn)
        NetStop();
    else
        NetPlay(s_netCur >= 0 ? s_netCur : 0);
}

static void NetStep(int dir)
{
    NetLoadList();
    if (s_netCount > 0)
        NetPlay(s_netCur < 0 ? 0 : (s_netCur + dir + s_netCount) % s_netCount);
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
            free(r->file);
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

// Media -> internet radio: the station, the song, the state; previous / on-off / next station.
static void ScrNetRadio(HDC dc)
{
    NetLoadList();
    Frame(dc, L"МЕДИА · ИНТЕРНЕТ-РАДИО");
    MediaTabs(dc, S_NETRADIO, IC_LIST, A_SCREEN, S_NETRADIO_LIST, S_SET_SOUND);
    Cover(dc, IC_NETRADIO, 16, 124, 232);
    wchar_t s[160], title[160];
    if (s_netCount <= 0) {
        T(dc, 272, 170, L"НЕТ СТАНЦИЙ", 22, kText, 700, AL_START, 512);
        _snwprintf(s, 160, L"нет %sradio.txt", s_dir);
        s[159] = 0;
        T(dc, 272, 200, s, 15, kMuted, 400, AL_START, 512);
        T(dc, 272, 224, L"Строка: название|адрес потока (http:// или https://)", 15, kMuted, 400, AL_START, 512);
        return;
    }
    if (s_netCur < 0) {
        T(dc, 272, 170, L"ВЫБЕРИТЕ СТАНЦИЮ", 22, kText, 700, AL_START, 512);
        T(dc, 272, 200, L"Кнопка списка вверху или ▶ — первая станция", 16, kMuted, 400, AL_START, 512);
    } else {
        _snwprintf(s, 160, L"СТАНЦИЯ %d ИЗ %d", s_netCur + 1, s_netCount);
        T(dc, 272, 150, s, 14, kMuted, 600, AL_START, 512);
        T(dc, 272, 192, s_stations[s_netCur].name, 34, kText, 700, AL_START, 512);
        EnterCriticalSection(&s_netCs);
        wcscpy(title, s_netTitle);
        LeaveCriticalSection(&s_netCs);
        if (s_netOn && title[0])
            T(dc, 272, 222, title, 20, kMuted, 400, AL_START, 512);
        NetStateText(s, 160);
        T(dc, 272, 282, s, 17, s_netState == NR_PLAY ? kOrange : kMuted, 600, AL_START, 512);
    }
    IconBtn(dc, IC_PREV, 392, 312, 48, kText, A_NRSTEP, -1);
    NetPlayBtn(dc, 528, 336, 42, 48);
    IconBtn(dc, IC_FWD, 616, 312, 48, kText, A_NRSTEP, 1);
}

// The stations of radio.txt, a list that scrolls (the finger, or the arrows by kNetRows rows); the current one
// orange.
enum { kNetTop = 72, kNetBottom = 386, kNetPitch = 62 };
static Scroll s_netList = { 0, 0, kNetRows * kNetPitch };

static void ScrNetRadioList(HDC dc)
{
    NetLoadList();
    s_netList.content = s_netCount > 0 ? s_netCount * kNetPitch - 6 + 8 : 0;   // the rows, 4 px above and below
    static DWORD s_seen;
    if (s_seen != s_screenTick) {   // opened: the station playing in view
        s_seen = s_screenTick;
        if (s_netCur >= 0 && (s_netCur * kNetPitch < s_netList.pos ||
                              s_netCur * kNetPitch + kNetPitch > s_netList.pos + kNetBottom - kNetTop))
            s_netList.pos = (s_netCur - 2) * kNetPitch;
    }
    Frame(dc, L"ИНТЕРНЕТ-РАДИО · СТАНЦИИ");
    Panel(dc, 16, 68, 704, 322);
    ScrollBegin(dc, s_netList, 20, kNetTop, 716, kNetBottom);
    for (int i = 0; i < s_netCount; i++) {
        int y = kNetTop + 4 + i * kNetPitch - s_netList.pos;
        if (y + 56 <= kNetTop || y >= kNetBottom)
            continue;
        ListRow(dc, 24, y, 680, 56, s_stations[i].name, NULL, i == s_netCur ? L_CUR : L_N, IC_RADIO, NULL, NULL,
                A_NRPICK, i);
    }
    ScrollEnd(dc, s_netList, 728, 68, 56, 322, 96);
    if (s_netCount <= 0)
        T(dc, 40, 114, L"Нет станций: radio.txt рядом с VestaHome", 18, kMuted, 400);
    Dock(dc);
    IconBtn(dc, IC_RADIO, 600, 417, 44, kText, A_BACK);
    IconBtn(dc, IC_GEAR, 712, 417, 44, kText, A_SCREEN, S_SET_SOUND);
}
