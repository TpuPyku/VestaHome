// vh_player.inc - part of home.cpp (included there, in order): the music player (BASS).

// ---- player: local music through BASS 2.4.12 (un4seen; bass.dll and its plugins for CE/ARM, from the
// SinglePlayer package - MAIN/SinglePlayer), loaded at run time from bass\ next to the exe, so the exe still
// imports COREDLL (+ WS2) only. The shell's GStreamer (GstProbe) had no AAC and no ICY; BASS plays MP3/MP2/OGG/WAV
// itself, AAC/M4A, FLAC, ALAC and MPC through plugins. The sound goes out through waveOut like any program's:
// switching the MCU's source radio <-> media is not done yet. Playlist = the audio files of the track's folder.
#define WM_APP_PLAYER (WM_APP + 21)
typedef unsigned __int64 QWORD;
typedef BOOL  (WINAPI *BassInitFn)(int, DWORD, DWORD, HWND, const void*);
typedef DWORD (WINAPI *BassPluginLoadFn)(const void*, DWORD);
typedef DWORD (WINAPI *BassStreamFileFn)(BOOL, const void*, QWORD, QWORD, DWORD);
typedef BOOL  (WINAPI *BassChanFn)(DWORD);
typedef BOOL  (WINAPI *BassPlayFn)(DWORD, BOOL);
typedef DWORD (WINAPI *BassActiveFn)(DWORD);
typedef QWORD (WINAPI *BassLenFn)(DWORD, DWORD);
typedef BOOL  (WINAPI *BassSetPosFn)(DWORD, QWORD, DWORD);
typedef double (WINAPI *BassB2SFn)(DWORD, QWORD);
typedef void  (CALLBACK *BassSyncProc)(DWORD, DWORD, DWORD, void*);
typedef DWORD (WINAPI *BassSyncFn)(DWORD, DWORD, QWORD, BassSyncProc, void*);
typedef int   (WINAPI *BassErrFn)(void);
typedef const char* (WINAPI *TagsReadFn)(DWORD, const char*);
// a "file" read by callbacks (the internet radio: BASS_StreamCreateFileUser, BASS_FILEPROCS)
typedef void  (CALLBACK *BassFileCloseProc)(void*);
typedef QWORD (CALLBACK *BassFileLenProc)(void*);
typedef DWORD (CALLBACK *BassFileReadProc)(void*, DWORD, void*);
typedef BOOL  (CALLBACK *BassFileSeekProc)(QWORD, void*);
struct BassFileProcs { BassFileCloseProc close; BassFileLenProc length; BassFileReadProc read; BassFileSeekProc seek; };
typedef DWORD (WINAPI *BassStreamUserFn)(DWORD, DWORD, const BassFileProcs*, void*);
enum { BASS_UNICODE_F = 0x80000000, BASS_SYNC_END_F = 2, BASS_SYNC_STALL_F = 6, BASS_ACTIVE_PLAYING_F = 1,
       BASS_ACTIVE_PAUSED_F = 3, BASS_STREAMFILE_BUFFER_F = 1, BASS_STREAM_BLOCK_F = 0x100000,
       kPlMax = 400, kMusRows = 4, kMusCols = 2, kMusMax = 400 };

static struct {
    int              state;   // 0 not loaded yet, 1 ready, -1 no bass.dll, -2 BASS_Init failed
    BassInitFn       init;
    BassPluginLoadFn pluginLoad;
    BassStreamFileFn streamFile;
    BassChanFn       streamFree, pause, stop;
    BassPlayFn       play;
    BassActiveFn     active;
    BassLenFn        getLength, getPos;
    BassSetPosFn     setPos;
    BassB2SFn        b2s;
    BassSyncFn       setSync;
    BassErrFn        error;
    TagsReadFn       tags;
    BassStreamUserFn streamUser;
} s_bass;

// Only one of the music, the internet radio and the video plays: each one starting stops / pauses the others.
static void NetStop();
static void VidPauseForAudio();
static void Redraw();
static void RedrawArea(int l, int t, int r, int b);

static DWORD    s_chan;                    // the stream playing (0 = none)
static wchar_t  s_plDir[MAX_PATH];         // the playlist's folder
static wchar_t* s_pl[kPlMax];              // its audio files (names), sorted
static int      s_plCount, s_plCur = -1;
static wchar_t  s_tTitle[96], s_tArtist[96], s_tAlbum[96];
// the folder browser of "Media -> list": "" = the volumes
static wchar_t  s_musDir[MAX_PATH];
static wchar_t* s_mus[kMusMax];            // entries: folders first ("\" at the end), then audio files
static int      s_musCount, s_musPage;

#ifdef UNDER_CE
#define BASS_PROC(name) GetProcAddress(lib, L##name)
#else
#define BASS_PROC(name) GetProcAddress(lib, name)
#endif

static bool BassLoad()
{
    if (s_bass.state)
        return s_bass.state > 0;
    wchar_t dir[MAX_PATH], path[MAX_PATH];
    _snwprintf(dir, MAX_PATH, L"%sbass\\", s_dir);
    _snwprintf(path, MAX_PATH, L"%sbass.dll", dir);
    HMODULE lib = LoadLibrary(path);
    if (!lib) {   // also right next to the exe
        wcscpy(dir, s_dir);
        _snwprintf(path, MAX_PATH, L"%sbass.dll", dir);
        lib = LoadLibrary(path);
    }
    if (!lib) {
        s_bass.state = -1;
        VLog(L"плеер: нет bass.dll (%sbass\\)", s_dir);
        return false;
    }
    s_bass.init = (BassInitFn)BASS_PROC("BASS_Init");
    s_bass.pluginLoad = (BassPluginLoadFn)BASS_PROC("BASS_PluginLoad");
    s_bass.streamFile = (BassStreamFileFn)BASS_PROC("BASS_StreamCreateFile");
    s_bass.streamFree = (BassChanFn)BASS_PROC("BASS_StreamFree");
    s_bass.pause = (BassChanFn)BASS_PROC("BASS_ChannelPause");
    s_bass.stop = (BassChanFn)BASS_PROC("BASS_ChannelStop");
    s_bass.play = (BassPlayFn)BASS_PROC("BASS_ChannelPlay");
    s_bass.active = (BassActiveFn)BASS_PROC("BASS_ChannelIsActive");
    s_bass.getLength = (BassLenFn)BASS_PROC("BASS_ChannelGetLength");
    s_bass.getPos = (BassLenFn)BASS_PROC("BASS_ChannelGetPosition");
    s_bass.setPos = (BassSetPosFn)BASS_PROC("BASS_ChannelSetPosition");
    s_bass.b2s = (BassB2SFn)BASS_PROC("BASS_ChannelBytes2Seconds");
    s_bass.setSync = (BassSyncFn)BASS_PROC("BASS_ChannelSetSync");
    s_bass.error = (BassErrFn)BASS_PROC("BASS_ErrorGetCode");
    s_bass.streamUser = (BassStreamUserFn)BASS_PROC("BASS_StreamCreateFileUser");
    if (!s_bass.init || !s_bass.streamFile || !s_bass.play || !s_bass.active || !s_bass.getLength ||
        !s_bass.getPos || !s_bass.setPos || !s_bass.b2s || !s_bass.streamFree || !s_bass.pause || !s_bass.error) {
        s_bass.state = -1;
        VLog(L"плеер: bass.dll без нужных функций");
        return false;
    }
    if (!s_bass.init(-1, 44100, 0, s_wnd, NULL)) {
        s_bass.state = -2;
        VLog(L"плеер: BASS_Init ошибка %d", s_bass.error());
        return false;
    }
    static const wchar_t* plugins[6] = { L"bass_aac.dll", L"bassflac.dll", L"bassalac.dll", L"bass_mpc.dll",
                                         L"bassopus.dll", L"basswv.dll" };
    int loaded = 0;
    for (int i = 0; i < 6 && s_bass.pluginLoad; i++) {
        _snwprintf(path, MAX_PATH, L"%s%s", dir, plugins[i]);
        if (GetFileAttributes(path) != 0xFFFFFFFF && s_bass.pluginLoad(path, BASS_UNICODE_F))
            loaded++;
    }
    _snwprintf(path, MAX_PATH, L"%stags.dll", dir);
    HMODULE tl = LoadLibrary(path);
    if (!tl && GetFileAttributes(path) != 0xFFFFFFFF)
        VLog(L"плеер: tags.dll не загрузилась, ошибка %u", (unsigned)GetLastError());
    if (tl) {
#ifdef UNDER_CE
        s_bass.tags = (TagsReadFn)GetProcAddress(tl, L"TAGS_Read");
#else
        s_bass.tags = (TagsReadFn)GetProcAddress(tl, "TAGS_Read");
#endif
    }
    s_bass.state = 1;
    VLog(L"плеер: BASS готов, плагинов %d, теги %s", loaded, s_bass.tags ? L"есть" : L"нет");
    return true;
}

// (.mp4 is the video's: in the music list a film played as sound only - 2026-10-21; audio-only MP4 is .m4a)
static bool IsAudioName(const wchar_t* n)
{
    static const wchar_t* ext[13] = { L".mp3", L".mp2", L".mp1", L".ogg", L".wav", L".aac", L".m4a",
                                      L".flac", L".mpc", L".aif", L".aiff", L".opus", L".wv" };
    const wchar_t* dot = wcsrchr(n, L'.');
    if (!dot)
        return false;
    for (int i = 0; i < 13; i++)
        if (!_wcsicmp(dot, ext[i]))
            return true;
    return false;
}

// What the shell's GStreamer can open (its demuxers; the pictures by omxdecoder, see video.cpp).
static bool IsVideoName(const wchar_t* n)
{
    static const wchar_t* ext[14] = { L".mp4", L".m4v", L".mov", L".3gp", L".avi", L".mkv", L".flv", L".wmv",
                                      L".asf", L".mpg", L".mpeg", L".ts", L".vob", L".rmvb" };
    const wchar_t* dot = wcsrchr(n, L'.');
    if (!dot)
        return false;
    for (int i = 0; i < 14; i++)
        if (!_wcsicmp(dot, ext[i]))
            return true;
    return false;
}

static int CmpName(const void* a, const void* b)
{
    const wchar_t* x = *(const wchar_t* const*)a;
    const wchar_t* y = *(const wchar_t* const*)b;
    bool dx = x[wcslen(x) - 1] == L'\\', dy = y[wcslen(y) - 1] == L'\\';
    if (dx != dy)
        return dx ? -1 : 1;   // folders first
    return _wcsicmp(x, y);
}

static void FreeNames(wchar_t** v, int* n)
{
    for (int i = 0; i < *n; i++)
        free(v[i]);
    *n = 0;
}

// Folder `dir` ("X\" form): subfolders (with "\" appended, when folders) and audio (or video) files, sorted.
static int ReadMusicDir(const wchar_t* dir, wchar_t** v, int cap, bool folders, int kind = 0)   // 0 audio, 1 video,
                                                                                               // 2 photos
{
    wchar_t pat[MAX_PATH];
    _snwprintf(pat, MAX_PATH, L"%s*", dir);
    WIN32_FIND_DATA fd;
    HANDLE h = FindFirstFile(pat, &fd);
    int n = 0;
    if (h == INVALID_HANDLE_VALUE)
        return 0;
    do {
        bool isDir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if (fd.cFileName[0] == L'.' || (isDir && !folders) ||
            (!isDir && !(kind == 2 ? PicIsName(fd.cFileName) : kind ? IsVideoName(fd.cFileName)
                                                                      : IsAudioName(fd.cFileName))) ||
            (fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN))
            continue;
        size_t len = wcslen(fd.cFileName);
        wchar_t* s = (wchar_t*)malloc((len + 2) * sizeof(wchar_t));
        if (!s)
            break;
        wcscpy(s, fd.cFileName);
        if (isDir)
            wcscat(s, L"\\");
        v[n++] = s;
    } while (n < cap && FindNextFile(h, &fd));
    FindClose(h);
    qsort(v, n, sizeof(wchar_t*), CmpName);
    return n;
}

// Folder `dir` with all its subfolders (up to 6 deep): the audio files as paths inside it ("Artist\Album\01.mp3"),
// sorted by path - disc, artist, album, track. For "ИГРАТЬ ВСЁ".
static int ReadMusicTree(const wchar_t* dir, const wchar_t* sub, wchar_t** v, int n, int cap, int depth)
{
    wchar_t pat[MAX_PATH];
    _snwprintf(pat, MAX_PATH, L"%s%s*", dir, sub);
    pat[MAX_PATH - 1] = 0;
    WIN32_FIND_DATA fd;
    HANDLE h = FindFirstFile(pat, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return n;
    int first = n;
    do {
        bool isDir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if (fd.cFileName[0] == L'.' || (fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN))
            continue;
        if (isDir) {
            if (depth < 6) {
                wchar_t s2[MAX_PATH];
                _snwprintf(s2, MAX_PATH, L"%s%s\\", sub, fd.cFileName);
                s2[MAX_PATH - 1] = 0;
                n = ReadMusicTree(dir, s2, v, n, cap, depth + 1);
            }
        } else if (IsAudioName(fd.cFileName)) {
            size_t len = wcslen(sub) + wcslen(fd.cFileName);
            wchar_t* p = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
            if (!p)
                break;
            wcscpy(p, sub);
            wcscat(p, fd.cFileName);
            v[n++] = p;
        }
    } while (n < cap && FindNextFile(h, &fd));
    FindClose(h);
    if (!depth)
        qsort(v + first, n - first, sizeof(wchar_t*), CmpName);
    return n;
}

// The volumes that exist, as entries of a browser ("\SDMemory\" ...).
static int ReadVolumes(wchar_t** v)
{
#ifdef UNDER_CE
    static const wchar_t* vols[6] = { L"\\SDMemory\\", L"\\USBDisk\\", L"\\USBDevice\\", L"\\SDMMC\\",
                                      L"\\StorageFlash\\", L"\\ResidentFlash\\" };
#else
    static const wchar_t* vols[6] = { L"C:\\", L"D:\\", L"E:\\", L"F:\\", L"G:\\", L"H:\\" };
#endif
    int n = 0;
    for (int i = 0; i < 6; i++) {
        DWORD a = GetFileAttributes(vols[i]);
        if (a != 0xFFFFFFFF && (a & FILE_ATTRIBUTE_DIRECTORY)) {
            v[n] = (wchar_t*)malloc((wcslen(vols[i]) + 1) * sizeof(wchar_t));
            if (v[n])
                wcscpy(v[n++], vols[i]);
        }
    }
    return n;
}

static void MusicOpenDir(const wchar_t* dir)
{
    FreeNames(s_mus, &s_musCount);
    wcsncpy(s_musDir, dir, MAX_PATH - 1);
    s_musDir[MAX_PATH - 1] = 0;
    s_musPage = 0;
    s_musCount = s_musDir[0] ? ReadMusicDir(s_musDir, s_mus, kMusMax, true) : ReadVolumes(s_mus);
}

static void PlayerReadTags()
{
    s_tTitle[0] = s_tArtist[0] = s_tAlbum[0] = 0;
    if (s_bass.tags && s_chan) {
        const char* t = s_bass.tags(s_chan, "%UTF8(%TITL)");
        if (t && *t)
            MultiByteToWideChar(CP_UTF8, 0, t, -1, s_tTitle, 96);
        t = s_bass.tags(s_chan, "%UTF8(%ARTI)");
        if (t && *t)
            MultiByteToWideChar(CP_UTF8, 0, t, -1, s_tArtist, 96);
        t = s_bass.tags(s_chan, "%UTF8(%IFV2(%ALBM,%ALBM,)%IFV2(%YEAR, · %YEAR,))");
        if (t && *t)
            MultiByteToWideChar(CP_UTF8, 0, t, -1, s_tAlbum, 96);
    }
    if (!s_tTitle[0] && s_plCur >= 0) {   // the file name without its extension (and its folders, "ИГРАТЬ ВСЁ")
        const wchar_t* fn = wcsrchr(s_pl[s_plCur], L'\\');
        wcsncpy(s_tTitle, fn ? fn + 1 : s_pl[s_plCur], 95);
        s_tTitle[95] = 0;
        wchar_t* dot = wcsrchr(s_tTitle, L'.');
        if (dot)
            *dot = 0;
    }
}

static void CALLBACK PlayerEnd(DWORD, DWORD, DWORD, void*)
{
    PostMessage(s_wnd, WM_APP_PLAYER, 0, 0);   // from BASS's thread: the window takes the next track
}

static void PlayerStopStream()
{
    if (s_chan) {
        s_bass.streamFree(s_chan);
        s_chan = 0;
    }
}

static bool PlayerPlay(int i)
{
    ResumeByUser();
    s_mediaLast = 1;
    if (!BassLoad() || i < 0 || i >= s_plCount)
        return false;
    NetStop();
    VidPauseForAudio();
    PlayerStopStream();
    wchar_t path[MAX_PATH];
    _snwprintf(path, MAX_PATH, L"%s%s", s_plDir, s_pl[i]);
    s_plCur = i;
    s_chan = s_bass.streamFile(FALSE, path, 0, 0, BASS_UNICODE_F);
    if (!s_chan) {
        int e = s_bass.error();
        _snwprintf(s_status, 96, L"не играет: %s (BASS %d)", s_pl[i], e);
        VLog(L"плеер: %s не открылся, BASS %d", path, e);
        return false;
    }
    if (s_bass.setSync)
        s_bass.setSync(s_chan, BASS_SYNC_END_F, 0, PlayerEnd, NULL);
    s_bass.play(s_chan, FALSE);
    MediaSource();
    PlayerReadTags();
    wcscpy(s_cfg.musicDir, s_plDir);
    wcsncpy(s_cfg.musicFile, s_pl[i], MAX_PATH - 1);
    s_cfg.musicFile[MAX_PATH - 1] = 0;
    CfgSoon();   // (not at once: every track would write the flash)
    return true;
}

// The playlist = the audio files of `dir` (tree: with all its subfolders), starting with `file` (or the first).
static bool PlayerLoadDir(const wchar_t* dir, const wchar_t* file, bool tree = false)
{
    FreeNames(s_pl, &s_plCount);
    wcsncpy(s_plDir, dir, MAX_PATH - 1);
    s_plDir[MAX_PATH - 1] = 0;
    s_cfg.musicTree = tree;
    s_plCount = tree ? ReadMusicTree(s_plDir, L"", s_pl, 0, kPlMax, 0) : ReadMusicDir(s_plDir, s_pl, kPlMax, false);
    s_plCur = -1;
    if (!s_plCount)
        return false;
    int start = 0;
    for (int i = 0; file && i < s_plCount; i++)
        if (!_wcsicmp(s_pl[i], file))
            start = i;
    return PlayerPlay(start);
}

static bool PlayerPlaying()
{
    return s_chan && s_bass.active(s_chan) == BASS_ACTIVE_PLAYING_F;
}

// Next / previous: shuffle picks any other track; past the end repeat goes round, otherwise it stops.
static void PlayerStep(int dir, bool automatic)
{
    if (!s_plCount)
        return;
    int i;
    if (s_cfg.shuffle && s_plCount > 1) {
        do {
            i = rand() % s_plCount;
        } while (i == s_plCur);
    } else {
        i = s_plCur + dir;
        if (i >= s_plCount || i < 0) {
            if (!s_cfg.repeat && automatic) {
                PlayerStopStream();
                return;
            }
            i = (i + s_plCount) % s_plCount;
        }
    }
    PlayerPlay(i);
}

static void PlayerPrev()
{
    // more than 3 s into the track: back to its start, like every player
    if (s_chan && s_bass.b2s(s_chan, s_bass.getPos(s_chan, 0)) > 3.0) {
        s_bass.setPos(s_chan, 0, 0);
        return;
    }
    PlayerStep(-1, false);
}

static void PlayerToggle()
{
    if (!BassLoad()) {
        wcscpy(s_status, s_bass.state == -1 ? L"нет bass.dll: папка bass рядом с VestaHome (см. «Медиа»)" : L"звук не запускается (BASS_Init)");
        return;
    }
    if (!s_chan) {   // nothing loaded: the last track, otherwise the list
        if (!s_cfg.musicDir[0] || !PlayerLoadDir(s_cfg.musicDir, s_cfg.musicFile, s_cfg.musicTree))
            Go(S_AUDIO_LIST, true);
        return;
    }
    DWORD a = s_bass.active(s_chan);
    if (a == BASS_ACTIVE_PLAYING_F) {
        s_bass.pause(s_chan);
    } else {
        ResumeByUser();
        NetStop();
        VidPauseForAudio();
        s_bass.play(s_chan, a == 0);   // stopped at the end: from the start
        MediaSource();
    }
}

static void PlayerSeek(int k)   // k of 20 along the bar
{
    if (!s_chan)
        return;
    QWORD len = s_bass.getLength(s_chan, 0);
    if (len && len != (QWORD)-1)
        s_bass.setPos(s_chan, len / 20 * k, 0);
}

// "1:42" of seconds.
static void Mmss(wchar_t* s, int n, double sec)
{
    int t = sec > 0 ? (int)sec : 0;
    _snwprintf(s, n, L"%d:%02d", t / 60, t % 60);
}

// The track for the screens: title, second line, position and length (s), false = nothing loaded.
static bool PlayerNow(wchar_t* title, wchar_t* sub, int n, double* pos, double* len)
{
    if (!s_chan)
        return false;
    wcsncpy(title, s_tTitle, n);
    title[n - 1] = 0;
    if (s_tArtist[0] && s_tAlbum[0])
        _snwprintf(sub, n, L"%s · %s", s_tArtist, s_tAlbum);
    else
        wcsncpy(sub, s_tArtist[0] ? s_tArtist : s_tAlbum, n);
    sub[n - 1] = 0;
    QWORD l = s_bass.getLength(s_chan, 0), p = s_bass.getPos(s_chan, 0);
    *len = l != (QWORD)-1 ? s_bass.b2s(s_chan, l) : 0;
    *pos = p != (QWORD)-1 ? s_bass.b2s(s_chan, p) : 0;
    return true;
}

// The orange play / pause circle with its action.
static void PlayBtn(HDC dc, int cx, int cy, int r, int isz)
{
    bool down = Zone(cx - r, cy - r, 2 * r, 2 * r, A_PLAY);
    Disc(dc, cx, cy, r, down ? kFacet : kOrange);
    Ico(dc, PlayerPlaying() ? IC_PAUSE : IC_PLAY, cx - isz / 2, cy - isz / 2, isz, kInk);
}

// Media -> music: the track, the bar (a tap seeks), shuffle, previous, play/pause, next, repeat.
static void ScrAudio(HDC dc)
{
    Frame(dc, L"Медиа · Музыка");
    MediaTabs(dc, S_AUDIO, IC_LIST, A_SCREEN, S_AUDIO_LIST, S_SET_SOUND);
    Cover(dc, IC_MUSIC, 16, 124, 232);
    wchar_t title[96], sub[200], s[160];
    double pos = 0, len = 0;
    bool have = PlayerNow(title, sub, 96, &pos, &len);
    if (have) {
        const wchar_t* folder = s_plDir;   // the folder's own name
        int l = (int)wcslen(s_plDir);
        for (int i = l - 2; i >= 0; i--)
            if (s_plDir[i] == L'\\') {
                folder = s_plDir + i + 1;
                break;
            }
        _snwprintf(s, 160, L"ТРЕК %d ИЗ %d · %s", s_plCur + 1, s_plCount, folder);
        if (s[wcslen(s) - 1] == L'\\')
            s[wcslen(s) - 1] = 0;
        T(dc, 272, 150, s, 14, kMuted, 600, AL_START, 512);
        T(dc, 272, 192, title, 34, kText, 700, AL_START, 512);
        T(dc, 272, 222, sub, 20, kMuted, 400, AL_START, 512);
    } else if (s_bass.state < 0) {
        T(dc, 272, 170, L"НЕТ БИБЛИОТЕКИ ЗВУКА", 22, kText, 700, AL_START, 512);
        if (s_bass.state == -1) {   // where it was looked for: the folder bass\ next to the exe
            wchar_t where[MAX_PATH + 16];
            _snwprintf(where, MAX_PATH + 16, L"нет %sbass\\bass.dll", s_dir);
            where[MAX_PATH + 15] = 0;
            T(dc, 272, 200, where, 15, kMuted, 400, AL_START, 512);
#ifdef UNDER_CE
            T(dc, 272, 224, L"Скопируйте папку bass из Tests\\SDMemory\\VestaHome", 15, kMuted, 400, AL_START, 512);
#else
            T(dc, 272, 224, L"ПК: BASS для Windows x86 (un4seen.com) в эту папку", 15, kMuted, 400, AL_START, 512);
#endif
        } else {
            T(dc, 272, 200, L"BASS_Init не удался — звук занят?", 16, kMuted, 400, AL_START, 512);
        }
    } else {
        wchar_t last[MAX_PATH] = L"—", folder[MAX_PATH] = L"";   // the last track (▶ goes on with it), "—" none
        if (s_cfg.musicFile[0]) {
            const wchar_t* fn = wcsrchr(s_cfg.musicFile, L'\\');
            wcscpy(last, fn ? fn + 1 : s_cfg.musicFile);
            wchar_t* dot = wcsrchr(last, L'.');
            if (dot && dot != last)
                *dot = 0;
            wcscpy(folder, s_cfg.musicDir);
            int fl = (int)wcslen(folder);
            if (fl && folder[fl - 1] == L'\\')
                folder[--fl] = 0;
            const wchar_t* dn = wcsrchr(folder, L'\\');
            if (dn)
                memmove(folder, dn + 1, (wcslen(dn + 1) + 1) * sizeof(wchar_t));
        }
        T(dc, 272, 192, last, 34, kText, 700, AL_START, 512);
        if (folder[0])
            T(dc, 272, 222, folder, 20, kMuted, 400, AL_START, 512);
    }
    int k = len > 0 ? (int)(pos * 512 / len) : 0;
    R(dc, 272, 248, 512, 8, 4, kLine);
    if (have) {
        R(dc, 272, 248, k > 8 ? k : 8, 8, 4, kOrange);
        Disc(dc, 272 + k, 252, 11, kText);
        for (int z = 0; z < 20; z++)   // tap anywhere along the bar
            Zone(272 + z * 512 / 20, 232, 512 / 20 + 1, 40, A_SEEK, z);
        Mmss(s, 16, pos);
        T(dc, 272, 282, s, 17, kMuted, 600);
        Mmss(s, 16, len);
        T(dc, 784, 282, s, 17, kMuted, 600, AL_END);
    }
    IconBtn(dc, IC_SHUFFLE, 290, 316, 40, s_cfg.shuffle ? kOrange : kDim, A_SHUFFLE);
    IconBtn(dc, IC_PREV, 392, 312, 48, kText, A_PREVTRK);
    PlayBtn(dc, 528, 336, 42, 48);
    IconBtn(dc, IC_FWD, 616, 312, 48, kText, A_NEXTTRK);
    IconBtn(dc, IC_REPEAT, 726, 316, 40, s_cfg.repeat ? kOrange : kDim, A_REPEAT);
}

// The file lists' pages (music, video).
static int MusPages()
{
    int per = kMusRows * kMusCols;
    return s_musCount ? (s_musCount + per - 1) / per : 1;
}

// Media -> music -> list (design 0.6): the sources' pills stay on top (the round button goes back to the player),
// the path line ("‹" up, a tap on the path - the volumes; "Играть всё" at its right inside a folder), then the
// volumes, folders and audio files; a file plays its folder from it. Pages: a swipe and the dots.
static void ScrAudioList(HDC dc)
{
    if (!s_musCount && !s_musDir[0])
        MusicOpenDir(s_cfg.musicDir[0] && GetFileAttributes(s_cfg.musicDir) != 0xFFFFFFFF ? s_cfg.musicDir : L"");
    Frame(dc, L"Медиа · Музыка");
    MediaTabs(dc, S_AUDIO, IC_MUSIC, A_BACK, 0, S_SET_SOUND);
    if (s_musPage >= MusPages())
        s_musPage = MusPages() - 1;
    int right = 784;
    if (s_musDir[0]) {   // this folder and everything under it, in path order
        Button(dc, 604, 110, 180, 40, L"Играть всё", B_SEL, IC_PLAY, 15, 18, A_MALL);
        right = 588;
    }
    DirPath(dc, s_musDir, A_MUP, right);
    DirEntries(dc, s_musDir, s_mus, s_musCount, s_musPage, kMusRows, kMusCols, A_MFILE, IC_MUSIC,
               s_chan && s_plCur >= 0 && !_wcsicmp(s_plDir, s_musDir) ? s_pl[s_plCur] : NULL,
               L"Здесь нет папок и музыки");
    PageDots(dc, s_musPage, MusPages(), 400, 396);
}

// A browser's folder ("X\" form) -> its parent; the volume root ("\SDMemory\" or "C:\") -> "" (the volumes).
static void ParentDir(wchar_t* d)
{
    int l = (int)wcslen(d);
    if (l && d[l - 1] == L'\\')
        d[--l] = 0;
    wchar_t* slash = wcsrchr(d, L'\\');
    // the volume root ("\SDMemory\" or "C:\") goes back to the volume list
    if (!slash || slash == d || (slash == d + 2 && d[1] == L':' && !d[3]))
        d[0] = 0;
    else
        slash[1] = 0;
}

static void MusicUp(bool toVolumes)
{
    if (toVolumes || !s_musDir[0]) {
        MusicOpenDir(L"");
        return;
    }
    wchar_t d[MAX_PATH];
    wcscpy(d, s_musDir);
    ParentDir(d);
    MusicOpenDir(d);
}

static void MusicPick(int i)
{
    if (i < 0 || i >= s_musCount)
        return;
    const wchar_t* n = s_mus[i];
    wchar_t d[MAX_PATH];
    if (n[wcslen(n) - 1] == L'\\') {   // a folder (or a volume)
        _snwprintf(d, MAX_PATH, L"%s%s", s_musDir, n);
        MusicOpenDir(d);
        return;
    }
    wchar_t file[MAX_PATH];
    wcsncpy(file, n, MAX_PATH - 1);
    file[MAX_PATH - 1] = 0;
    wcscpy(d, s_musDir);
    if (!BassLoad()) {
        wcscpy(s_status, s_bass.state == -1 ? L"нет bass.dll: папка bass рядом с VestaHome (см. «Медиа»)" : L"звук не запускается (BASS_Init)");
        return;
    }
    if (PlayerLoadDir(d, file))
        Back();
}
