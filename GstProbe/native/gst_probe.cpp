// GstProbe - a test for the head unit: can a program of ours play media with the stock shell's GStreamer 0.10
// (\ResidentFlash\navigation\app, the CSR SDK the shell's own player is built on: playbin2, ffmpeg,
// neonhttpsrc, waveoutsink), a file and HTTP radio, and does the sound reach the speakers.
// While the unit's radio plays, programs are not heard: the MCU keeps the radio as the source until the
// shell's player starts. "MCU: МЕДИА" sends what the shell sends for its "Audio" tile,
// SPApi_SendMediaSourceSelect(5, 3) (SPIPowerMgr.dll: a write to PIC1:, no reader thread of ours).
// "ТОН 1 кГц" plays a tone through waveOut alone, without GStreamer.
// Everything goes to the screen and to GstProbe.log next to the exe; the stream addresses are in GstProbe.txt.
// The import table stays COREDLL only: GStreamer and SPIPowerMgr are loaded at run time.
// Also builds for the desktop (GstProbePC.exe): only the UI, the tone and the logging work there.
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#ifdef UNDER_CE
#define PROC(m, n) GetProcAddress(m, L##n)
#else
#define PROC(m, n) GetProcAddress(m, n)
#endif

static HWND    s_wnd;
static wchar_t s_dir[MAX_PATH];          // the exe's folder, ends with a backslash

// ---------------------------------------------------------------- log

enum { kLines = 22 };
static wchar_t s_lines[kLines][120];     // the screen's log, oldest first
static int     s_lineCount;

static void Log(const wchar_t* fmt, ...)
{
    wchar_t line[400];
    va_list a;
    va_start(a, fmt);
    _vsnwprintf(line, 400, fmt, a);
    va_end(a);
    line[399] = 0;

    if (s_lineCount == kLines) {
        memmove(s_lines, s_lines + 1, (kLines - 1) * sizeof(s_lines[0]));
        s_lineCount--;
    }
    wcsncpy(s_lines[s_lineCount], line, 119);
    s_lines[s_lineCount][119] = 0;
    s_lineCount++;

    wchar_t path[MAX_PATH];
    _snwprintf(path, MAX_PATH, L"%sGstProbe.log", s_dir);
    HANDLE h = CreateFile(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, 0, NULL);
    if (h != INVALID_HANDLE_VALUE) {
        SetFilePointer(h, 0, NULL, FILE_END);
        SYSTEMTIME t;
        GetLocalTime(&t);
        char buf[1300];
        int n = _snprintf(buf, 16, "%02d:%02d:%02d ", t.wHour, t.wMinute, t.wSecond);
        n += WideCharToMultiByte(CP_UTF8, 0, line, -1, buf + n, sizeof(buf) - n - 3, NULL, NULL) - 1;
        buf[n++] = '\r';
        buf[n++] = '\n';
        DWORD w;
        WriteFile(h, buf, n, &w, NULL);
        CloseHandle(h);
    }
    if (s_wnd)
        InvalidateRect(s_wnd, NULL, FALSE);
}

// UTF-8 from GStreamer to the log.
static void LogU8(const wchar_t* what, const char* s)
{
    wchar_t w[300];
    if (!s || !MultiByteToWideChar(CP_UTF8, 0, s, -1, w, 300))
        wcscpy(w, L"(null)");
    w[299] = 0;
    Log(L"%s: %s", what, w);
}

// ---------------------------------------------------------------- settings (GstProbe.txt, UTF-8 key=value)

static char s_gstDir[MAX_PATH] = "\\ResidentFlash\\navigation\\app\\";
static char s_file[MAX_PATH];
static char s_mp3[400] = "http://icecast.vgtrk.cdnvideo.ru/mayakfm_mp3_192kbps";
static char s_aac[400] = "http://radiorecord.hostingradio.ru/rr_main96.aacp";

static void LoadSettings()
{
    wchar_t path[MAX_PATH];
    _snwprintf(path, MAX_PATH, L"%sGstProbe.txt", s_dir);
    HANDLE h = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) {   // first start: write the defaults to edit
        h = CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        if (h != INVALID_HANDLE_VALUE) {
            char buf[1200];
            int n = _snprintf(buf, sizeof(buf),
                "# GstProbe: key=value. gst = folder with the stock shell's GStreamer DLLs;\r\n"
                "# file = a file to play (empty: the first .mp3 next to the exe or in \\SDMMC\\Music, \\SDMMC);\r\n"
                "# mp3, aac = HTTP radio streams (no HTTPS: the stock HTTP source can't do it).\r\n"
                "gst=%s\r\nfile=%s\r\nmp3=%s\r\naac=%s\r\n", s_gstDir, s_file, s_mp3, s_aac);
            DWORD w;
            WriteFile(h, buf, n, &w, NULL);
            CloseHandle(h);
        }
        return;
    }
    char buf[2048];
    DWORD got = 0;
    ReadFile(h, buf, sizeof(buf) - 1, &got, NULL);
    CloseHandle(h);
    buf[got] = 0;
    for (char* p = buf; *p;) {
        char* end = strchr(p, '\n');
        if (end)
            *end = 0;
        int n = (int)strlen(p);
        while (n > 0 && (p[n - 1] == '\r' || p[n - 1] == ' '))
            p[--n] = 0;
        char* eq = strchr(p, '=');
        if (p[0] != '#' && eq) {
            *eq = 0;
            char* v = eq + 1;
            if (!strcmp(p, "gst"))
                strncpy(s_gstDir, v, MAX_PATH - 2);
            else if (!strcmp(p, "file"))
                strncpy(s_file, v, MAX_PATH - 1);
            else if (!strcmp(p, "mp3"))
                strncpy(s_mp3, v, 399);
            else if (!strcmp(p, "aac"))
                strncpy(s_aac, v, 399);
        }
        p = end ? end + 1 : p + strlen(p);
    }
    int n = (int)strlen(s_gstDir);
    if (n && s_gstDir[n - 1] != '\\')
        strcat(s_gstDir, "\\");
}

// ---------------------------------------------------------------- GStreamer, loaded at run time

// The shared libraries in dependency order (found from their import tables); the loader of CE doesn't look for
// a DLL's dependencies in that DLL's folder, so each is loaded by its full path before the one that needs it.
// libgstvframer is a plugin other plugins link to.
static const char* kGstLibs[] = { "libglib.dll", "libgmodule.dll", "libgthread.dll", "libgobject.dll",
    "libgstreamer.dll", "libgstinterfaces.dll", "libgstbase.dll", "libgstaudio.dll", "libgstnetbuffer.dll",
    "libgstvideo.dll", "libgstpbutils.dll", "libgsttag.dll", "libgstriff.dll", "libgstrtp.dll", "libgstrtsp.dll",
    "libgstsdp.dll", "libgstvframer.dll" };

enum { GST_STATE_NULL = 1, GST_STATE_PLAYING = 4, GST_FORMAT_TIME = 3 };
enum { MSG_EOS = 1, MSG_ERROR = 2, MSG_WARNING = 4, MSG_TAG = 16, MSG_BUFFERING = 32 };

struct GError_ { unsigned domain; int code; char* message; };
struct GList_ { void* data; GList_* next; GList_* prev; };

static struct {
    void (*init)(int*, char***);
    void (*version)(unsigned*, unsigned*, unsigned*, unsigned*);
    void* (*registry)(void);
    GList_* (*plugins)(void*);
    void (*pluginsFree)(GList_*);
    const char* (*pluginName)(void*);
    int (*scanPath)(void*, const char*);
    void* (*factoryFind)(const char*);
    void* (*make)(const char*, const char*);
    void (*unref)(void*);
    int (*setState)(void*, int);
    int (*getState)(void*, int*, int*, unsigned __int64);
    void* (*getBus)(void*);
    void* (*popFiltered)(void*, int);
    void (*msgUnref)(void*);
    void (*parseError)(void*, GError_**, char**);
    void (*parseTag)(void*, void**);
    void (*parseBuffering)(void*, int*);
    int (*tagString)(void*, const char*, char**);
    void (*tagFree)(void*);
    int (*queryPosition)(void*, int*, __int64*);
    void (*objectSet)(void*, const char*, ...);
    void (*objectGet)(void*, const char*, ...);
    unsigned long (*signalConnect)(void*, const char*, void*, void*, void*, int);
    void* (*findProperty)(void*, const char*);
    void (*gfree)(void*);
    void (*errorFree)(GError_*);
    char* (*fileToUri)(const char*, const char*, void**);
} G;
static bool  s_gstOk;
static void* s_pipe;
static void* s_bus;
static int   s_lastState;

#define GETP(mod, field, name) \
    (*(FARPROC*)&G.field = PROC(mod, name), G.field ? true : (Log(L"нет функции %S", name), false))

static bool LoadGst()
{
    if (s_gstOk)
        return true;
    HMODULE mods[sizeof(kGstLibs) / sizeof(kGstLibs[0])];
    int n = sizeof(kGstLibs) / sizeof(kGstLibs[0]);
    for (int i = 0; i < n; i++) {
        wchar_t path[MAX_PATH];
        _snwprintf(path, MAX_PATH, L"%S%S", s_gstDir, kGstLibs[i]);
        mods[i] = LoadLibrary(path);
        if (!mods[i]) {
            Log(L"LoadLibrary %s: ошибка %u", path, (unsigned)GetLastError());
            if (i < 5)
                return false;   // glib .. libgstreamer are needed for anything
        }
    }
    Log(L"библиотеки загружены из %S", s_gstDir);
    HMODULE glib = mods[0], gobj = mods[3], gst = mods[4];
    bool ok = GETP(gst, init, "gst_init") & GETP(gst, version, "gst_version") &
              GETP(gst, registry, "gst_registry_get_default") & GETP(gst, plugins, "gst_registry_get_plugin_list") &
              GETP(gst, pluginsFree, "gst_plugin_list_free") & GETP(gst, pluginName, "gst_plugin_get_name") &
              GETP(gst, scanPath, "gst_registry_scan_path") & GETP(gst, factoryFind, "gst_element_factory_find") &
              GETP(gst, make, "gst_element_factory_make") & GETP(gst, unref, "gst_object_unref") &
              GETP(gst, setState, "gst_element_set_state") & GETP(gst, getState, "gst_element_get_state") &
              GETP(gst, getBus, "gst_element_get_bus") & GETP(gst, popFiltered, "gst_bus_pop_filtered") &
              GETP(gst, msgUnref, "gst_mini_object_unref") & GETP(gst, parseError, "gst_message_parse_error") &
              GETP(gst, parseTag, "gst_message_parse_tag") & GETP(gst, parseBuffering, "gst_message_parse_buffering") &
              GETP(gst, tagString, "gst_tag_list_get_string") & GETP(gst, tagFree, "gst_tag_list_free") &
              GETP(gst, queryPosition, "gst_element_query_position") & GETP(gobj, objectSet, "g_object_set") &
              GETP(gobj, objectGet, "g_object_get") & GETP(gobj, signalConnect, "g_signal_connect_data") &
              GETP(gobj, findProperty, "g_object_class_find_property") &
              GETP(glib, gfree, "g_free") & GETP(glib, errorFree, "g_error_free");
    if (!ok)
        return false;
    *(FARPROC*)&G.fileToUri = PROC(glib, "g_filename_to_uri_utf8");   // the CE build's name, as CSRMedia uses
    if (!G.fileToUri)
        *(FARPROC*)&G.fileToUri = PROC(glib, "g_filename_to_uri");

    DWORD t0 = GetTickCount();
    G.init(NULL, NULL);
    unsigned a, b, c, d;
    G.version(&a, &b, &c, &d);
    Log(L"gst_init за %u мс, GStreamer %u.%u.%u.%u", (unsigned)(GetTickCount() - t0), a, b, c, d);

    static const char* kFactories[] = { "playbin2", "decodebin2", "neonhttpsrc", "souphttpsrc", "filesrc",
        "waveoutsink", "ddrawsink", "ffdec_mp3", "ffdec_aac", "ffdec_wmav2", "mad", "faad", "vorbisdec" };
    for (int pass = 0; pass < 2; pass++) {
        int count = 0;
        GList_* list = G.plugins(G.registry());
        char names[1000] = "";
        for (GList_* l = list; l; l = l->next, count++)
            if (strlen(names) + 40 < sizeof(names)) {
                strcat(names, G.pluginName(l->data));
                strcat(names, " ");
            }
        G.pluginsFree(list);
        Log(L"плагинов: %d", count);
        LogU8(L"плагины", names);
        char have[300] = "", miss[300] = "";
        for (int i = 0; i < (int)(sizeof(kFactories) / sizeof(kFactories[0])); i++) {
            void* f = G.factoryFind(kFactories[i]);
            strcat(f ? have : miss, kFactories[i]);
            strcat(f ? have : miss, " ");
            if (f)
                G.unref(f);
        }
        LogU8(L"есть", have);
        LogU8(L"нет", miss);
        if (strstr(have, "playbin2") || pass)
            break;
        Log(L"playbin2 не найден: сканирую папку плагинов");
        G.scanPath(G.registry(), s_gstDir);
    }
    s_gstOk = true;
    return true;
}

static void Stop()
{
    KillTimer(s_wnd, 2);
    if (s_pipe) {
        G.setState(s_pipe, GST_STATE_NULL);
        G.unref(s_pipe);
        Log(L"остановлено");
    }
    if (s_bus)
        G.unref(s_bus);
    s_pipe = s_bus = NULL;
}

// Icecast servers answer "Content-Type: application/x-icy" with metadata blocks inside the audio when the
// source asks for them (icy-metadata: 1), and this GStreamer has no icydemux to strip them. neonhttpsrc asks
// in its iradio-mode, so it is switched off on the source playbin2 creates; the plain mp3 then goes to ffdec_mp3.
static void OnSource(void* playbin, void* pspec, void* data)
{
    void* src = NULL;
    G.objectGet(playbin, "source", &src, NULL);
    if (!src)
        return;
    void* cls = *(void**)src;   // G_OBJECT_GET_CLASS: GTypeInstance starts with its class
    if (G.findProperty(cls, "iradio-mode")) {
        int was = -1;
        G.objectGet(src, "iradio-mode", &was, NULL);
        G.objectSet(src, "iradio-mode", 0, NULL);
        Log(L"источник: iradio-mode было %d -> 0", was);
    } else {
        Log(L"источник без iradio-mode");
    }
    G.unref(src);
}

static void Play(const char* uri)
{
    if (!LoadGst())
        return;
    Stop();
    s_pipe = G.make("playbin2", "probe");
    if (!s_pipe) {
        Log(L"не создать playbin2");
        return;
    }
    LogU8(L"играю", uri);
    G.objectSet(s_pipe, "uri", uri, NULL);
    G.signalConnect(s_pipe, "notify::source", (void*)OnSource, NULL, NULL, 0);
    int r = G.setState(s_pipe, GST_STATE_PLAYING);
    Log(L"set_state(PLAYING) = %d (0 ошибка, 1 успех, 2 асинхронно, 3 живой поток)", r);
    s_bus = G.getBus(s_pipe);
    s_lastState = 0;
    SetTimer(s_wnd, 2, 250, NULL);
}

// Every 250 ms while playing: the bus messages that tell how it goes, the state and the position.
static void OnBus()
{
    if (!s_bus)
        return;
    static const int kTypes[5] = { MSG_ERROR, MSG_WARNING, MSG_EOS, MSG_TAG, MSG_BUFFERING };
    for (int k = 0; k < 5; k++) {
        void* m;
        while ((m = G.popFiltered(s_bus, kTypes[k])) != NULL) {
            if (kTypes[k] == MSG_ERROR || kTypes[k] == MSG_WARNING) {
                GError_* e = NULL;
                char* dbg = NULL;
                G.parseError(m, &e, &dbg);   // the warning has the same layout
                LogU8(kTypes[k] == MSG_ERROR ? L"ОШИБКА" : L"предупреждение", e ? e->message : NULL);
                if (dbg)
                    LogU8(L"  подробно", dbg);
                if (e)
                    G.errorFree(e);
                G.gfree(dbg);
            } else if (kTypes[k] == MSG_EOS) {
                Log(L"конец файла");
            } else if (kTypes[k] == MSG_TAG) {
                void* tags = NULL;
                G.parseTag(m, &tags);
                static const char* kTags[4] = { "title", "artist", "organization", "audio-codec" };
                for (int t = 0; t < 4 && tags; t++) {
                    char* s = NULL;
                    if (G.tagString(tags, kTags[t], &s) && s) {
                        wchar_t what[24];
                        _snwprintf(what, 24, L"тег %S", kTags[t]);
                        LogU8(what, s);
                        G.gfree(s);
                    }
                }
                if (tags)
                    G.tagFree(tags);
            } else {
                static int s_lastPct = -1;
                int pct = 0;
                G.parseBuffering(m, &pct);
                if (pct / 25 != s_lastPct / 25 || pct == 100)
                    Log(L"буфер %d%%", pct);
                s_lastPct = pct;
            }
            G.msgUnref(m);
        }
    }
    if (!s_pipe)
        return;
    int st = 0, pend = 0;
    G.getState(s_pipe, &st, &pend, 0);
    static DWORD s_lastPos;
    if (st != s_lastState) {
        static const wchar_t* kNames[5] = { L"?", L"NULL", L"READY", L"PAUSED", L"PLAYING" };
        Log(L"состояние %s", kNames[st >= 0 && st < 5 ? st : 0]);
        s_lastState = st;
    }
    if (st == GST_STATE_PLAYING && GetTickCount() - s_lastPos > 5000) {
        int fmt = GST_FORMAT_TIME;
        __int64 pos = 0;
        if (G.queryPosition(s_pipe, &fmt, &pos))
            Log(L"позиция %u с", (unsigned)(pos / 1000000000));
        s_lastPos = GetTickCount();
    }
}

// The file: from the settings, otherwise the first .mp3 next to the exe, in \SDMMC\Music, in \SDMMC.
static void PlayFile()
{
    if (!LoadGst())
        return;
    char path[MAX_PATH];
    strcpy(path, s_file);
    if (!path[0]) {
        wchar_t dirs[3][MAX_PATH];
        wcscpy(dirs[0], s_dir);
        wcscpy(dirs[1], L"\\SDMMC\\Music\\");
        wcscpy(dirs[2], L"\\SDMMC\\");
        for (int i = 0; i < 3 && !path[0]; i++) {
            wchar_t mask[MAX_PATH];
            _snwprintf(mask, MAX_PATH, L"%s*.mp3", dirs[i]);
            WIN32_FIND_DATA fd;
            HANDLE h = FindFirstFile(mask, &fd);
            if (h != INVALID_HANDLE_VALUE) {
                wchar_t full[MAX_PATH];
                _snwprintf(full, MAX_PATH, L"%s%s", dirs[i], fd.cFileName);
                WideCharToMultiByte(CP_UTF8, 0, full, -1, path, MAX_PATH, NULL, NULL);
                FindClose(h);
            }
        }
    }
    if (!path[0]) {
        Log(L"нет .mp3: положите файл рядом с exe или укажите file= в GstProbe.txt");
        return;
    }
    char* uri = G.fileToUri ? G.fileToUri(path, NULL, NULL) : NULL;
    if (!uri) {
        LogU8(L"не получить URI для", path);
        return;
    }
    Play(uri);
    G.gfree(uri);
}

// ---------------------------------------------------------------- MCU source, tone

static void McuMedia()
{
    typedef void (*SourceFn)(unsigned char, unsigned char);
    static HMODULE s_spi;
    if (!s_spi)
        s_spi = LoadLibrary(L"\\ResidentFlash\\navigation\\app\\SPIPowerMgr.dll");
    SourceFn f = s_spi ? (SourceFn)PROC(s_spi, "SPApi_SendMediaSourceSelect") : NULL;
    if (!f) {
        Log(L"SPIPowerMgr.dll: %s (%u)", s_spi ? L"нет SPApi_SendMediaSourceSelect" : L"не загрузилась",
            (unsigned)GetLastError());
        return;
    }
    f(5, 3);
    Log(L"SPApi_SendMediaSourceSelect(5, 3) отправлено — так оболочка включает «Аудио»");
}

static HWAVEOUT s_wave;
static WAVEHDR  s_hdr;
static short    s_tone[22050 * 2];   // 2 s, 22 kHz mono

static void Tone()
{
    if (s_wave)
        return;
    WAVEFORMATEX wf;
    memset(&wf, 0, sizeof(wf));
    wf.wFormatTag = WAVE_FORMAT_PCM;
    wf.nChannels = 1;
    wf.nSamplesPerSec = 22050;
    wf.wBitsPerSample = 16;
    wf.nBlockAlign = 2;
    wf.nAvgBytesPerSec = 44100;
    MMRESULT r = waveOutOpen(&s_wave, WAVE_MAPPER, &wf, 0, 0, CALLBACK_NULL);
    if (r != MMSYSERR_NOERROR) {
        Log(L"waveOutOpen: ошибка %u", (unsigned)r);
        s_wave = NULL;
        return;
    }
    for (int i = 0; i < 22050 * 2; i++)
        s_tone[i] = (short)(8000 * sin(i * 2 * 3.14159265 * 1000 / 22050));
    memset(&s_hdr, 0, sizeof(s_hdr));
    s_hdr.lpData = (LPSTR)s_tone;
    s_hdr.dwBufferLength = sizeof(s_tone);
    waveOutPrepareHeader(s_wave, &s_hdr, sizeof(s_hdr));
    r = waveOutWrite(s_wave, &s_hdr, sizeof(s_hdr));
    Log(L"тон 1 кГц 2 с: waveOutWrite = %u", (unsigned)r);
    SetTimer(s_wnd, 3, 300, NULL);
}

static void OnToneTimer()
{
    if (s_wave && !(s_hdr.dwFlags & WHDR_DONE))
        return;
    KillTimer(s_wnd, 3);
    if (s_wave) {
        waveOutUnprepareHeader(s_wave, &s_hdr, sizeof(s_hdr));
        waveOutClose(s_wave);
        s_wave = NULL;
        Log(L"тон доигран");
    }
}

// ---------------------------------------------------------------- window

enum { B_LOAD, B_FILE, B_MP3, B_AAC, B_STOP, B_MCU, B_TONE, B_EXIT, B_COUNT };
static const wchar_t* kBtn[B_COUNT] = { L"ЗАГРУЗИТЬ GST", L"ФАЙЛ MP3", L"РАДИО MP3", L"РАДИО AAC", L"СТОП",
                                        L"MCU: МЕДИА", L"ТОН 1 кГц", L"ВЫХОД" };
static int s_down = -1;

static RECT BtnRect(int i)
{
    RECT r = { 8, 8 + i * 58, 228, 8 + i * 58 + 52 };
    return r;
}

static int HitBtn(int x, int y)
{
    for (int i = 0; i < B_COUNT; i++) {
        RECT r = BtnRect(i);
        if (x >= r.left && x < r.right && y >= r.top && y < r.bottom)
            return i;
    }
    return -1;
}

static HFONT MakeFont(int h, int weight)
{
    LOGFONT lf;
    memset(&lf, 0, sizeof(lf));
    lf.lfHeight = -h;
    lf.lfWeight = weight;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = NONANTIALIASED_QUALITY;   // ClearType on the unit fringes letters in colour
    wcscpy(lf.lfFaceName, L"Tahoma");
    return CreateFontIndirect(&lf);
}

static void Paint(HDC hdc)
{
    static HFONT s_big, s_small;
    if (!s_big) {
        s_big = MakeFont(18, FW_BOLD);
        s_small = MakeFont(14, FW_NORMAL);
    }
    HDC dc = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, 800, 480);
    HGDIOBJ ob = SelectObject(dc, bmp);
    RECT all = { 0, 0, 800, 480 };
    HBRUSH bg = CreateSolidBrush(RGB(0x0b, 0x0c, 0x0e));
    FillRect(dc, &all, bg);
    DeleteObject(bg);
    SetBkMode(dc, TRANSPARENT);
    SelectObject(dc, s_big);
    for (int i = 0; i < B_COUNT; i++) {
        RECT r = BtnRect(i);
        HBRUSH b = CreateSolidBrush(i == s_down ? RGB(0xff, 0x6b, 0x1a) : RGB(0x22, 0x25, 0x2a));
        FillRect(dc, &r, b);
        DeleteObject(b);
        SetTextColor(dc, i == s_down ? RGB(0x0b, 0x0c, 0x0e) : RGB(0xf3, 0xf1, 0xec));
        DrawText(dc, kBtn[i], -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    SelectObject(dc, s_small);
    SetTextColor(dc, RGB(0xa3, 0xa8, 0xb0));
    for (int i = 0; i < s_lineCount; i++) {
        RECT r = { 240, 6 + i * 21, 796, 6 + i * 21 + 21 };
        DrawText(dc, s_lines[i], -1, &r, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);
    }
    BitBlt(hdc, 0, 0, 800, 480, dc, 0, 0, SRCCOPY);
    SelectObject(dc, ob);
    DeleteObject(bmp);
    DeleteDC(dc);
}

static void OnButton(int i)
{
    switch (i) {
    case B_LOAD: LoadGst(); break;
    case B_FILE: PlayFile(); break;
    case B_MP3:  Play(s_mp3); break;
    case B_AAC:  Play(s_aac); break;
    case B_STOP: if (s_gstOk) Stop(); break;
    case B_MCU:  McuMedia(); break;
    case B_TONE: Tone(); break;
    case B_EXIT: DestroyWindow(s_wnd); break;
    }
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        Paint(hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_LBUTTONDOWN:
        s_down = HitBtn((short)LOWORD(lp), (short)HIWORD(lp));
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    case WM_LBUTTONUP: {
        int i = HitBtn((short)LOWORD(lp), (short)HIWORD(lp));
        int was = s_down;
        s_down = -1;
        InvalidateRect(hwnd, NULL, FALSE);
        if (i >= 0 && i == was)
            OnButton(i);
        return 0;
    }
    case WM_TIMER:
        if (wp == 2)
            OnBus();
        else if (wp == 3)
            OnToneTimer();
        return 0;
    case WM_DESTROY:
        if (s_gstOk)
            Stop();
        Log(L"выход");
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

#ifdef UNDER_CE
int WINAPI WinMain(HINSTANCE inst, HINSTANCE, LPWSTR, int)
#else
int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, LPWSTR, int)
#endif
{
    GetModuleFileName(NULL, s_dir, MAX_PATH);
    wchar_t* slash = wcsrchr(s_dir, L'\\');
    if (slash)
        slash[1] = 0;
    LoadSettings();

    WNDCLASS wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.lpszClassName = L"GstProbe_Wnd";
#ifndef UNDER_CE
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
#endif
    RegisterClass(&wc);
#ifdef UNDER_CE
    DWORD style = WS_POPUP | WS_VISIBLE, ex = WS_EX_TOPMOST;   // above the stock shell, which is topmost
    int x = 0, y = 0, w = 800, h = 480;
#else
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, ex = 0;
    RECT rc = { 0, 0, 800, 480 };
    AdjustWindowRect(&rc, style, FALSE);
    int x = CW_USEDEFAULT, y = CW_USEDEFAULT, w = rc.right - rc.left, h = rc.bottom - rc.top;
#endif
    s_wnd = CreateWindowEx(ex, wc.lpszClassName, L"GstProbe", style, x, y, w, h, NULL, NULL, inst, NULL);
    if (!s_wnd)
        return 1;
    Log(L"--- GstProbe: GStreamer из %S", s_gstDir);
    ShowWindow(s_wnd, SW_SHOW);
    UpdateWindow(s_wnd);
    MSG m;
    while (GetMessage(&m, NULL, 0, 0)) {
        TranslateMessage(&m);
        DispatchMessage(&m);
    }
    return 0;
}
