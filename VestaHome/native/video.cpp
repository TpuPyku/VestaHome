// Video through the stock shell's GStreamer 0.10, as its own player does it (CSRMedia.dll): playbin2 with
// video-sink = ddrawsink (SiRF, DirectDraw overlay) given our window by gst_x_overlay_set_xwindow_id, and
// audio-sink = waveoutsink. Decoders come with it: omxdecoder (the MSVDX hardware: H.264, MPEG-4 / XviD / DivX,
// MPEG-2, WMV3, RealVideo, H.263) and the SiRF audecoder (AAC, MP3); demuxers for MP4/MOV, AVI, MKV, FLV, ASF,
// MPEG-PS/TS. The libraries are loaded by their full paths in dependency order (the loader of CE doesn't look
// for a DLL's dependencies in its folder), as GstProbe found it on the unit (2026-10-12).
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "video.h"

#ifdef UNDER_CE
#define PROC(m, n) GetProcAddress(m, L##n)
#else
#define PROC(m, n) GetProcAddress(m, n)
#endif

static const wchar_t* kGstDir = L"\\ResidentFlash\\navigation\\app\\";
static const char* kGstLibs[] = { "libglib.dll", "libgmodule.dll", "libgthread.dll", "libgobject.dll",
    "libgstreamer.dll", "libgstinterfaces.dll", "libgstbase.dll", "libgstaudio.dll", "libgstnetbuffer.dll",
    "libgstvideo.dll", "libgstpbutils.dll", "libgsttag.dll", "libgstriff.dll", "libgstrtp.dll", "libgstrtsp.dll",
    "libgstsdp.dll", "libgstvframer.dll" };
enum { kLibs = sizeof(kGstLibs) / sizeof(kGstLibs[0]), kGlib = 0, kGobject = 3, kGst = 4, kInterfaces = 5 };

enum { GST_STATE_NULL = 1, GST_STATE_PAUSED = 3, GST_STATE_PLAYING = 4, GST_FORMAT_TIME = 3,
       GST_SEEK_FLAG_FLUSH = 1, GST_SEEK_FLAG_KEY_UNIT = 4, MSG_EOS = 1, MSG_ERROR = 2, MSG_WARNING = 4,
       MSG_STATE_CHANGED = 0x40, MSG_QOS = 1 << 24 };
// GstMessage (0.10, 32 bits; CSRMedia reads the same offsets): its type at 0x18, its source object at 0x28.
enum { kMsgType = 0x18, kMsgSrc = 0x28 };
// playbin2 "flags". The stock player (CSRMedia.dll, Ghidra 2026-10-18): video 0x01 + native video 0x40 (the hardware
// decoder's frames straight to the overlay, no colour conversion) + audio 0x02, and 0x40000000 when its OMX init
// failed. 2026-10-17 we had video + audio + soft volume (0x13): sound, a black picture; 2026-10-18 as the shell: the
// picture (the hardware overlay over everything, see VidSync in home.cpp).
enum { kFlagsShell = 0x01 | 0x40 | 0x02, kFlagNoOmx = 0x40000000 };
static int  s_omx = -1;              // plugin_init_omx: 1 ok, 0 failed, -1 not there

struct GError_ { unsigned domain; int code; char* message; };

static struct {
    void (*init)(int*, char***);
    void* (*make)(const char*, const char*);
    void* (*factoryFind)(const char*);
    void (*unref)(void*);
    int (*setState)(void*, int);
    void* (*getBus)(void*);
    void* (*popFiltered)(void*, int);
    void (*msgUnref)(void*);
    void (*parseError)(void*, GError_**, char**);
    int (*queryPosition)(void*, int*, __int64*);
    int (*queryDuration)(void*, int*, __int64*);
    int (*seekSimple)(void*, int, int, __int64);
    void (*setXid)(void*, unsigned long);
    void (*expose)(void*);
    void (*objectSet)(void*, const char*, ...);
    void* (*findProperty)(void*, const char*);
    void (*gfree)(void*);
    void (*errorFree)(GError_*);
    void (*sliceConfig)(int, int, int);   // (key, gint64 value in two registers), as CSRMedia calls it
    char* (*fileToUri)(const char*, const char*, void**);
    void (*parseStateChanged)(void*, int*, int*, int*);
    void (*parseQosStats)(void*, int*, unsigned __int64*, unsigned __int64*);
    void (*parseQosValues)(void*, __int64*, double*, int*);
    char* (*objectName)(void*);
} G;

static int        s_loaded;          // 0 not tried, 1 ready, -1 can't be
static wchar_t    s_loadWhy[120];
static VideoLogFn s_log;
static void*      s_pipe;
static void*      s_bus;
static void*      s_sink;            // owned by the pipeline
static int        s_state;
static char       s_built[300];      // the elements that came to PLAYING (the log: which decoder)
static bool       s_builtDone;
static struct { unsigned count, processed, dropped; int lateMax; DWORD tick; } s_qos;

static void Say(wchar_t* why, int cap, const wchar_t* text)
{
    if (why && cap > 0) {
        wcsncpy(why, text, cap - 1);
        why[cap - 1] = 0;
    }
}

static void LogU8(const wchar_t* what, const char* s)
{
    if (!s_log)
        return;
    wchar_t w[300];
    if (!s || !MultiByteToWideChar(CP_UTF8, 0, s, -1, w, 300))
        wcscpy(w, L"(null)");
    w[299] = 0;
    s_log(L"видео: %s: %s", what, w);
}

#define GETP(mod, field, name) \
    (*(FARPROC*)&G.field = mods[mod] ? PROC(mods[mod], name) : NULL, G.field ? true : (missing = name, false))

bool VideoLoad(VideoLogFn log, wchar_t* why, int whyCap)
{
    if (log)
        s_log = log;
    if (s_loaded) {
        if (s_loaded < 0)
            Say(why, whyCap, s_loadWhy);
        return s_loaded > 0;
    }
#ifndef UNDER_CE
    s_loaded = -1;
    wcscpy(s_loadWhy, L"видео — только на ГУ (GStreamer штатной оболочки)");
    Say(why, whyCap, s_loadWhy);
    return false;
#else
    HMODULE mods[kLibs];
    for (int i = 0; i < kLibs; i++) {
        wchar_t path[MAX_PATH];
        _snwprintf(path, MAX_PATH, L"%s%S", kGstDir, kGstLibs[i]);
        mods[i] = LoadLibrary(path);
        if (!mods[i]) {
            if (s_log)
                s_log(L"видео: LoadLibrary %s: ошибка %u", path, (unsigned)GetLastError());
            if (i <= kInterfaces) {   // glib .. libgstinterfaces are needed
                s_loaded = -1;
                _snwprintf(s_loadWhy, 120, L"нет GStreamer оболочки (%S)", kGstLibs[i]);
                Say(why, whyCap, s_loadWhy);
                return false;
            }
        }
    }
    const char* missing = NULL;
    bool ok = GETP(kGst, init, "gst_init") & GETP(kGst, make, "gst_element_factory_make") &
              GETP(kGst, factoryFind, "gst_element_factory_find") & GETP(kGst, unref, "gst_object_unref") &
              GETP(kGst, setState, "gst_element_set_state") & GETP(kGst, getBus, "gst_element_get_bus") &
              GETP(kGst, popFiltered, "gst_bus_pop_filtered") & GETP(kGst, msgUnref, "gst_mini_object_unref") &
              GETP(kGst, parseError, "gst_message_parse_error") &
              GETP(kGst, queryPosition, "gst_element_query_position") &
              GETP(kGst, queryDuration, "gst_element_query_duration") &
              GETP(kGst, seekSimple, "gst_element_seek_simple") &
              GETP(kInterfaces, setXid, "gst_x_overlay_set_xwindow_id") &
              GETP(kInterfaces, expose, "gst_x_overlay_expose") & GETP(kGobject, objectSet, "g_object_set") &
              GETP(kGobject, findProperty, "g_object_class_find_property") & GETP(kGlib, gfree, "g_free") &
              GETP(kGlib, errorFree, "g_error_free");
    // for the log only (what was built, the frames the sink dropped): may be missing
    *(FARPROC*)&G.parseStateChanged = PROC(mods[kGst], "gst_message_parse_state_changed");
    *(FARPROC*)&G.parseQosStats = PROC(mods[kGst], "gst_message_parse_qos_stats");
    *(FARPROC*)&G.parseQosValues = PROC(mods[kGst], "gst_message_parse_qos_values");
    *(FARPROC*)&G.objectName = PROC(mods[kGst], "gst_object_get_name");
    *(FARPROC*)&G.fileToUri = PROC(mods[kGlib], "g_filename_to_uri_utf8");   // the CE build's name, as CSRMedia
    if (!G.fileToUri)
        *(FARPROC*)&G.fileToUri = PROC(mods[kGlib], "g_filename_to_uri");
    if (!ok || !G.fileToUri) {
        s_loaded = -1;
        _snwprintf(s_loadWhy, 120, L"GStreamer без функции %S", missing ? missing : "g_filename_to_uri");
        Say(why, whyCap, s_loadWhy);
        return false;
    }
    DWORD t0 = GetTickCount();
    // as CSRMedia does it: g_slice always through malloc (before anything uses g_slice), gst_init, then the OMX
    // core of the hardware decoder by the plugin's own plugin_init_omx (OMX_Init) - without it, no picture
    *(FARPROC*)&G.sliceConfig = PROC(mods[kGlib], "g_slice_set_config");
    if (G.sliceConfig)
        G.sliceConfig(1, 1, 0);   // G_SLICE_CONFIG_ALWAYS_MALLOC = TRUE
    G.init(NULL, NULL);
    {
        wchar_t path[MAX_PATH];
        // the plugin opens the OMX core by its bare name (g_module_open("omx_core.dll")): CE finds it only beside
        // the exe or in \Windows - loaded first by its path, the name finds it (VestaAuto, HU 2026-11-13)
        _snwprintf(path, MAX_PATH, L"%somx_core.dll", kGstDir);
        if (!LoadLibrary(path) && s_log)
            s_log(L"видео: LoadLibrary %s: ошибка %u", path, (unsigned)GetLastError());
        _snwprintf(path, MAX_PATH, L"%slibgstomxdecoder.dll", kGstDir);
        HMODULE omx = LoadLibrary(path);
        typedef int (*OmxInit)(void);
        OmxInit oi = omx ? (OmxInit)PROC(omx, "plugin_init_omx") : NULL;
        s_omx = oi ? (oi() ? 1 : 0) : -1;
        if (s_log)
            s_log(L"видео: OMX (аппаратный декодер) %s", s_omx > 0 ? L"готов" : s_omx == 0 ? L"не поднялся" :
                  L"нет plugin_init_omx");
    }
    static const char* kNeed[3] = { "playbin2", "ddrawsink", "waveoutsink" };
    for (int i = 0; i < 3; i++) {
        void* f = G.factoryFind(kNeed[i]);
        if (!f) {
            s_loaded = -1;
            _snwprintf(s_loadWhy, 120, L"в GStreamer нет %S", kNeed[i]);
            Say(why, whyCap, s_loadWhy);
            return false;
        }
        G.unref(f);
    }
    if (s_log)
        s_log(L"видео: GStreamer готов за %u мс", (unsigned)(GetTickCount() - t0));
    s_loaded = 1;
    return true;
#endif
}

void VideoStop()
{
    if (s_pipe) {
        G.setState(s_pipe, GST_STATE_NULL);
        G.unref(s_pipe);
    }
    if (s_bus)
        G.unref(s_bus);
    s_pipe = s_bus = s_sink = NULL;
    s_state = VID_NONE;
}

// G_OBJECT_GET_CLASS: a GTypeInstance starts with its class.
static bool HasProperty(void* obj, const char* name)
{
    return obj && G.findProperty(*(void**)obj, name) != NULL;
}

bool VideoOpen(const wchar_t* path, HWND wnd, bool stretch, wchar_t* why, int whyCap)
{
    if (!VideoLoad(NULL, why, whyCap))
        return false;
    VideoStop();
    char u8[MAX_PATH * 3];
    WideCharToMultiByte(CP_UTF8, 0, path, -1, u8, sizeof(u8), NULL, NULL);
    char* uri = G.fileToUri(u8, NULL, NULL);
    if (!uri) {
        Say(why, whyCap, L"не получить адрес файла");
        return false;
    }
    s_pipe = G.make("playbin2", "vh_video");
    s_sink = G.make("ddrawsink", "vh_vsink");
    void* audio = G.make("waveoutsink", "vh_asink");
    if (!s_pipe || !s_sink) {
        G.gfree(uri);
        if (s_sink && !s_pipe)
            G.unref(s_sink);
        if (audio)
            G.unref(audio);
        s_sink = NULL;
        VideoStop();
        Say(why, whyCap, L"не создать playbin2 / ddrawsink");
        return false;
    }
    if (HasProperty(s_sink, "force-aspect-ratio"))
        G.objectSet(s_sink, "force-aspect-ratio", stretch ? 0 : 1, NULL);
    G.setXid(s_sink, (unsigned long)wnd);   // before PLAYING: the sink must not make a window of its own
    G.objectSet(s_pipe, "uri", uri, NULL);
    G.objectSet(s_pipe, "flags", kFlagsShell | (s_omx > 0 ? 0 : kFlagNoOmx), NULL);
    G.objectSet(s_pipe, "video-sink", s_sink, NULL);   // the pipeline takes them
    if (audio)
        G.objectSet(s_pipe, "audio-sink", audio, NULL);
    LogU8(L"открываю", uri);
    G.gfree(uri);
    s_bus = G.getBus(s_pipe);
    s_built[0] = 0;
    s_builtDone = false;
    memset(&s_qos, 0, sizeof(s_qos));
    s_qos.tick = GetTickCount();
    if (G.setState(s_pipe, GST_STATE_PLAYING) == 0) {   // GST_STATE_CHANGE_FAILURE
        VideoStop();
        Say(why, whyCap, L"файл не открылся");
        return false;
    }
    s_state = VID_PLAYING;
    return true;
}

void VideoPause(bool pause)
{
    if (!s_pipe || s_state == VID_ERROR)
        return;
    if (s_state == VID_ENDED && !pause) {   // played to the end: from the start
        G.seekSimple(s_pipe, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT, 0);
    }
    G.setState(s_pipe, pause ? GST_STATE_PAUSED : GST_STATE_PLAYING);
    s_state = pause ? VID_PAUSED : VID_PLAYING;
}

void VideoSetStretch(bool stretch)
{
    if (HasProperty(s_sink, "force-aspect-ratio")) {
        G.objectSet(s_sink, "force-aspect-ratio", stretch ? 0 : 1, NULL);
        G.expose(s_sink);
    }
}

void VideoSeek(double sec)
{
    if (!s_pipe || s_state == VID_ERROR)
        return;
    if (sec < 0)
        sec = 0;
    G.seekSimple(s_pipe, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT, (__int64)(sec * 1e9));
    if (s_state == VID_ENDED)
        s_state = VID_PAUSED;
}

bool VideoTimes(double* pos, double* len)
{
    *pos = *len = 0;
    if (!s_pipe)
        return false;
    int fmt = GST_FORMAT_TIME;
    __int64 v = 0;
    if (G.queryPosition(s_pipe, &fmt, &v) && v > 0)
        *pos = v / 1e9;
    fmt = GST_FORMAT_TIME;
    v = 0;
    if (G.queryDuration(s_pipe, &fmt, &v) && v > 0)
        *len = v / 1e9;
    return true;
}

void VideoExpose()
{
    if (s_sink)
        G.expose(s_sink);
}

// An element of the pipeline came to PLAYING: its name (the factory's + a number: "omxdecoder0" or a software
// "ffdec_h2640") into the list; the pipeline itself playing - the list into the log, once per file.
static void OnStateChanged(void* m)
{
    int was = 0, now = 0;
    void* src = *(void**)((char*)m + kMsgSrc);
    if (!G.parseStateChanged || !G.objectName || s_builtDone)
        return;
    G.parseStateChanged(m, &was, &now, NULL);
    if (now != GST_STATE_PLAYING)
        return;
    if (src == s_pipe) {
        s_builtDone = true;
        LogU8(L"собрано", s_built);
        return;
    }
    char* name = G.objectName(src);
    if (name && strlen(s_built) + strlen(name) + 2 < sizeof(s_built)) {
        if (s_built[0])
            strcat(s_built, " ");
        strcat(s_built, name);
    }
    G.gfree(name);
}

// The video sink's QoS (a frame late or dropped): the counts it reports, the worst lateness - a minute's sum
// into the log.
static void OnQos(void* m)
{
    if (*(void**)((char*)m + kMsgSrc) != s_sink || !G.parseQosStats || !G.parseQosValues)
        return;
    int fmt = 0, quality = 0;
    unsigned __int64 processed = 0, dropped = 0;
    __int64 jitter = 0;
    double prop = 0;
    G.parseQosStats(m, &fmt, &processed, &dropped);
    G.parseQosValues(m, &jitter, &prop, &quality);
    s_qos.processed = (unsigned)processed;
    s_qos.dropped = (unsigned)dropped;
    s_qos.count++;
    if (jitter / 1000000 > s_qos.lateMax)
        s_qos.lateMax = (int)(jitter / 1000000);
}

static void QosMinute()
{
    if (GetTickCount() - s_qos.tick < 60000)
        return;
    if (s_log && s_qos.count)
        s_log(L"видео: QoS за минуту %u, кадров %u, выброшено %u (всего с начала), опоздание до %d мс",
              s_qos.count, s_qos.processed, s_qos.dropped, s_qos.lateMax);
    s_qos.count = 0;
    s_qos.lateMax = 0;
    s_qos.tick = GetTickCount();
}

static void OnError(void* m, int type, wchar_t* why, int whyCap)
{
    GError_* e = NULL;
    char* dbg = NULL;
    G.parseError(m, &e, &dbg);   // errors and warnings have the same layout
    LogU8(type == MSG_ERROR ? L"ОШИБКА" : L"предупреждение", e ? e->message : NULL);
    if (dbg)
        LogU8(L"  подробно", dbg);
    if (type == MSG_ERROR) {
        s_state = VID_ERROR;
        wchar_t w[200] = L"ошибка GStreamer";
        if (e && e->message)
            MultiByteToWideChar(CP_UTF8, 0, e->message, -1, w, 200);
        w[199] = 0;
        Say(why, whyCap, w);
    }
    if (e)
        G.errorFree(e);
    G.gfree(dbg);
}

// Every message off the bus, as the stock player's sync handler (it drops each one). Only taking the three we
// needed left all the others there (state changes, tags, QoS of every late frame): the queue grew all the file
// long, each pop walked it under the bus's lock - the streaming threads waited to post, frames came late, more
// QoS (1080p, HU 2026-11-25: 5-15 frames a second of 25, the stock shell all of them).
int VideoPoll(wchar_t* why, int whyCap)
{
    if (!s_bus)
        return s_state;
    void* m;
    while ((m = G.popFiltered(s_bus, -1)) != NULL) {   // GST_MESSAGE_ANY
        int type = *(int*)((char*)m + kMsgType);
        if (type == MSG_EOS) {
            s_state = VID_ENDED;
            if (s_log)
                s_log(L"видео: конец файла");
        } else if (type == MSG_ERROR || type == MSG_WARNING) {
            OnError(m, type, why, whyCap);
        } else if (type == MSG_STATE_CHANGED) {
            OnStateChanged(m);
        } else if (type == MSG_QOS) {
            OnQos(m);
        }
        G.msgUnref(m);
    }
    QosMinute();
    return s_state;
}
