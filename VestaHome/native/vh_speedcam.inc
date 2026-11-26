// vh_speedcam.inc - part of home.cpp (included there, in order): speed cameras.

// ---- speed cameras: speedcam.txt in the iGO (X,Y,TYPE,SPEED,DIRTYPE,DIRECTION) or Navitel
// (IDX,X,Y,TYPE,SPEED,DIRTYPE,DIRECTION) format, columns found by the header. Only the cameras within 150 km of
// the car (or the home point) are kept, reloaded after 75 km: a country's list doesn't fit the unit's memory.
// A camera ahead (see CamCheck) shows on the speed widget and in the top bar, with two beeps when it comes up.
struct Cam { float lat, lon; unsigned char speed, dirType; short dir; };
enum { kMaxCams = 16000 };   // 192 KB: Moscow and its region within 150 km are ~7200 (2026-10-24); openspeedcam's
                             // Moscow region alone 11 212 (2026-11-22)
static Cam    s_cams[kMaxCams];
static int    s_camCount, s_camTotal;   // kept / seen in the file
static double s_camLat, s_camLon;       // centre of the kept ones
static bool   s_camLoaded;
static int    s_camNear = -1;           // camera ahead, -1 none
static int    s_camNearDist;

static double Dist(double lat1, double lon1, double lat2, double lon2)   // m, fine for some km
{
    double k = 3.14159265 / 180, x = (lon2 - lon1) * k * cos((lat1 + lat2) * k / 2), y = (lat2 - lat1) * k;
    return sqrt(x * x + y * y) * 6371000;
}

static double AngleDiff(double a, double b)
{
    double d = fmod(fabs(a - b), 360.0);
    return d > 180 ? 360 - d : d;
}

static void CamPath(wchar_t* path)
{
    if (s_cfg.camFile[0] == L'\\' || wcschr(s_cfg.camFile, L':'))
        wcscpy(path, s_cfg.camFile);
    else
        _snwprintf(path, MAX_PATH, L"%s%s", s_dir, s_cfg.camFile);
}

static int    s_camCol[6];   // columns of X, Y, TYPE, SPEED, DIRTYPE, DIRECTION
static bool   s_camFirst;
static double s_camDLat, s_camDLon;

// One line of the file: the header (column names) or a camera.
static void CamLine(char* line)
{
    if (strlen(line) < 3)
        return;
    char* f[12];
    int n = 0;
    f[n++] = line;
    for (char* p = line; *p && n < 12; p++)
        if (*p == ',' || *p == ';') {
            *p = 0;
            f[n++] = p + 1;
        }
    bool header = s_camFirst && ((line[0] >= 'A' && line[0] <= 'Z') || (line[0] >= 'a' && line[0] <= 'z'));
    s_camFirst = false;
    if (header) {
        static const char* names[6] = { "X", "Y", "TYPE", "SPEED", "DIRTYPE", "DIRECTION" };
        for (int k = 0; k < 6; k++)
            for (int j = 0; j < n; j++) {
                const char* nm = f[j];
                while (*nm == ' ')
                    nm++;
                int l = (int)strlen(names[k]);
                if (!_strnicmp(nm, names[k], l) && (nm[l] == 0 || nm[l] == ' '))
                    s_camCol[k] = j;
            }
        return;
    }
    if (n <= s_camCol[1])
        return;
    double x = atof(f[s_camCol[0]]), y = atof(f[s_camCol[1]]);
    if (x == 0 && y == 0)
        return;
    s_camTotal++;
    if (fabs(y - s_camLat) > s_camDLat || fabs(x - s_camLon) > s_camDLon || s_camCount >= kMaxCams)
        return;
    Cam& cm = s_cams[s_camCount++];
    cm.lat = (float)y;
    cm.lon = (float)x;
    cm.speed = (unsigned char)(n > s_camCol[3] ? CamLimit(atoi(f[s_camCol[3]])) : 0);   // (fillers: 0)
    cm.dirType = (unsigned char)(n > s_camCol[4] ? atoi(f[s_camCol[4]]) : 0);
    cm.dir = (short)(n > s_camCol[5] ? atoi(f[s_camCol[5]]) : 0);
}

// The lines of a piece of the file (line: the one not finished yet, 256 chars at most; ll its length).
static void CamChunk(const char* chunk, DWORD got, char* line, int* ll)
{
    for (DWORD i = 0; i < got; i++) {
        char c = chunk[i];
        if (c == '\n' || c == '\r') {
            line[*ll] = 0;
            CamLine(line);
            *ll = 0;
        } else if (*ll < 255) {
            line[(*ll)++] = c;
        }
    }
}

// One camera file into s_cams (its own header); false when there is no such file.
static bool CamReadFile(const wchar_t* path)
{
    for (int k = 0; k < 6; k++)
        s_camCol[k] = k;   // the iGO order when there is no header
    s_camFirst = true;
    HANDLE h = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return false;
    VLog(L"камеры: читаю %s (%u Б)", path, (unsigned)GetFileSize(h, NULL));
    static char chunk[16384];
    char line[256];
    int ll = 0;
    DWORD got = 0;
    while (ReadFile(h, chunk, sizeof(chunk), &got, NULL) && got)
        CamChunk(chunk, got, line, &ll);
    line[ll] = 0;
    CamLine(line);
    CloseHandle(h);
    return true;
}

// ---- the cameras of OSMapsCE's maps (the user 2026-11-22): the OSMapsCE-Maps loader writes per region
// OSMapsData\<region>\speedcam.txt (Navitel, openspeedcam.net) at the root of a volume, as OSMapsCE finds its data
// (offdata.cpp). When there is any, the cameras come from all of them, else from VestaHome's own file. The regions'
// boxes overlap: a camera in two files (the same lat/lon) is kept once - sorted and made unique after each file.
static int s_camMapFiles;   // map files read by the last LoadCams; 0: the own file

static int CmpCam(const void* a, const void* b)
{
    const Cam* x = (const Cam*)a;
    const Cam* y = (const Cam*)b;
    if (x->lat != y->lat)
        return x->lat < y->lat ? -1 : 1;
    return x->lon < y->lon ? -1 : x->lon > y->lon ? 1 : 0;
}

static void CamUnique()
{
    qsort(s_cams, s_camCount, sizeof(Cam), CmpCam);
    int n = 0;
    for (int i = 0; i < s_camCount; i++)
        if (!n || CmpCam(&s_cams[n - 1], &s_cams[i]))
            s_cams[n++] = s_cams[i];
    s_camCount = n;
}

static bool CamMapFile(const wchar_t* data, const wchar_t* region)
{
    wchar_t path[MAX_PATH];
    _snwprintf(path, MAX_PATH, L"%s\\%s\\speedcam.txt", data, region);
    path[MAX_PATH - 1] = 0;
    int total = s_camTotal, had = s_camCount;
    if (!CamReadFile(path))
        return false;
    int got = s_camCount;
    if (got > had)
        CamUnique();
    VLog(L"камеры: регион %s — строк %d, в 150 км +%d (повторов %d)", region, s_camTotal - total, s_camCount - had,
         got - s_camCount);
    return true;
}

// The regions' files in one OSMapsData folder; their count.
static int CamMapDir(const wchar_t* data)
{
    wchar_t mask[MAX_PATH];
    _snwprintf(mask, MAX_PATH, L"%s\\*", data);
    mask[MAX_PATH - 1] = 0;
    WIN32_FIND_DATA fd;
    HANDLE ff = FindFirstFile(mask, &fd);
    if (ff == INVALID_HANDLE_VALUE)
        return 0;
    int files = 0;
    do {
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && fd.cFileName[0] != L'.')
            files += CamMapFile(data, fd.cFileName) ? 1 : 0;
    } while (FindNextFile(ff, &fd));
    FindClose(ff);
    return files;
}

// The volumes' roots: CE - every folder of "\" (\SDMemory, \USBDevice...); PC - the drives C:..Z: and, for the
// tests, the OSMapsCE build folder next to this repo.
#ifdef UNDER_CE
static int CamVolumes(wchar_t roots[][MAX_PATH], int cap)
{
    int n = 0;
    WIN32_FIND_DATA fd;
    HANDLE ff = FindFirstFile(L"\\*", &fd);
    if (ff == INVALID_HANDLE_VALUE)
        return 0;
    do {
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && n < cap)
            _snwprintf(roots[n++], MAX_PATH, L"\\%s", fd.cFileName);
    } while (FindNextFile(ff, &fd));
    FindClose(ff);
    return n;
}
#else
static int CamVolumes(wchar_t roots[][MAX_PATH], int cap)
{
    int n = 0;
    UINT was = SetErrorMode(SEM_FAILCRITICALERRORS);   // (a drive without its disk: no window)
    DWORD drives = GetLogicalDrives();
    for (int d = 2; d < 26 && n < cap - 1; d++)
        if (drives & (1 << d))
            _snwprintf(roots[n++], MAX_PATH, L"%c:", L'A' + d);
    SetErrorMode(was);
    _snwprintf(roots[n++], MAX_PATH, L"%s..\\..\\..\\OSMapsCE\\OSMapsCE", s_dir);
    return n;
}
#endif

static int CamFromMaps()
{
    static wchar_t roots[32][MAX_PATH];
    int nv = CamVolumes(roots, 32), files = 0;
    for (int i = 0; i < nv; i++) {
        wchar_t data[MAX_PATH];
        _snwprintf(data, MAX_PATH, L"%s\\OSMapsData", roots[i]);
        data[MAX_PATH - 1] = 0;
        if (GetFileAttributes(data) != 0xFFFFFFFF)
            files += CamMapDir(data);
    }
    return files;
}

static void LoadCams(double lat, double lon)
{
    s_camCount = s_camTotal = 0;
    s_camNear = -1;
    s_camLoaded = true;
    s_camLat = lat;
    s_camLon = lon;
    s_camDLat = 150000.0 / 111000;
    s_camDLon = s_camDLat / (cos(lat * 3.14159265 / 180) + 0.01);
    DWORD t0 = GetTickCount();
    VLog(L"камеры: загрузка у %.4f, %.4f", lat, lon);
    s_camMapFiles = CamFromMaps();
    wchar_t path[MAX_PATH];
    CamPath(path);
    if (s_camMapFiles)
        VLog(L"камеры: из карт OSMapsCE (openspeedcam.net), файлов %d; свой файл не читаю", s_camMapFiles);
    else if (!CamReadFile(path)) {
        VLog(L"камеры: нет файла %s", path);
        return;
    }
    VLog(L"камеры: %d в 150 км из %d за %u мс", s_camCount, s_camTotal, (unsigned)(GetTickCount() - t0));
}

// The settings' line on the cameras' source: the maps' regions or the own file.
static void CamSourceText(wchar_t* s, int n)
{
    wchar_t path[MAX_PATH];
    CamPath(path);
    if (s_camLoaded && s_camMapFiles)
        _snwprintf(s, n, L"из карт OSMapsCE (openspeedcam.net), регионов %d · рядом %d из %d", s_camMapFiles,
                   s_camCount, s_camTotal);
    else if (GetFileAttributes(path) == 0xFFFFFFFF)
        _snwprintf(s, n, L"%s — нет файла", s_cfg.camFile);
    else if (!s_camLoaded)
        _snwprintf(s, n, L"%s · загрузится по GPS", s_cfg.camFile);
    else
        _snwprintf(s, n, L"%s · рядом %d из %d", s_cfg.camFile, s_camCount, s_camTotal);
    s[n - 1] = 0;
}

// A sound over the radio, as an external navigator says its prompts (Ghidra, PROJECT.md): WM_NAVIGATION_VOICE
// (0x101D, wParam 1 = a prompt starts, 0 = ended) to the stock shell's main window "RadioCommWindow"; the shell
// tells the MCU (SPApi_SendRadioNaviVoiceGuidance), which turns the radio down and lets our waveOut through.
// The sound starts kVoiceLeadMs after "1" (the MCU turns the radio down meanwhile), "0" goes kVoiceTailMs after
// it ends. No shell window: played at once. A device that never says "done" (Bluetooth HF without a phone, HU
// 2026-10-18: the "1" stayed on, the knob then set the navigation's volume) is cut after kVoiceMaxMs.
enum { TIMER_VOICE = 9, WM_NAVIGATION_VOICE = 0x101D, kVoiceLeadMs = 1000, kVoiceTailMs = 800, kVoiceMaxMs = 2000 };
static int s_voice;   // 0 idle, 1 waiting to play, 2 playing, 3 waiting to end
static bool s_plugVoice;   // an add-on's prompt on (its "voice", vh_plugins.inc): the beep's end leaves the radio down

// No shell window (VestaStub in its place, HU 2026-11-03: "окна оболочки нет — сразу", nothing told the MCU): the
// MCU told straight, as the shell would (SPApi_SendRadioNaviVoiceGuidance). Only per prompt: the channel held open
// for a whole navigator (the "sound with the radio" of a shortcut, removed 2026-11-08) let our player's sound
// through over any source - the card's music played together with Bluetooth's.
static bool NaviVoice(bool on)
{
    HWND h = FindWindow(NULL, L"RadioCommWindow");
    bool sent = h != NULL;
    if (h)
        PostMessage(h, WM_NAVIGATION_VOICE, on ? 1 : 0, 0);
    else
        sent = McuSend3("SPApi_SendRadioNaviVoiceGuidance", on ? 1 : 0, 0, 0);
    static int s_logged = -1;
    if (s_logged != (h != NULL)) {
        s_logged = h != NULL;
        VLog(L"звук поверх радио: %s", h ? L"через оболочку (RadioCommWindow, 0x101D)" : L"окна оболочки нет — MCU напрямую");
    }
    return sent;
}

// Navitel's voice over the radio (navimix.h, the user's switch in Sound): IsNaviSourceCouldIn held while it runs
static bool s_naviMix;

static void NaviMixSet(bool on, const wchar_t* exe)
{
    if (on == s_naviMix)
        return;
    s_naviMix = on;
    McuSend3("SPApi_SendRadioIsNaviSourceCouldIn", on ? 1 : 0, 0, 0);
    VLog(L"голос Навитела поверх радио: %s%s", on ? L"вкл - " : L"выкл", on ? exe : L"");
}

// The source back to the radio while held (HU 2026-11-05: told once at Navitel's start, its prompts not heard after
// the switch to FM): told again
static void NaviMixAgain()
{
    if (!s_naviMix)
        return;
    McuSend3("SPApi_SendRadioIsNaviSourceCouldIn", 1, 0, 0);
    VLog(L"голос Навитела поверх радио: снова - источник радио");
}

static void NaviMixTick()   // every 2 s
{
    static TaskInfo s_mixTask;   // (its own: the load log's minute keeps its sample)
    static wchar_t s_names[TASK_PROC_MAX][40];
    bool shell = FindWindow(NULL, L"RadioCommWindow") != NULL;
    int at = -1;
    if (s_cfg.naviMix && !shell) {
        TaskSample(&s_mixTask, s_wnd);
        int n = s_mixTask.procOk ? s_mixTask.procCount : 0;
        for (int i = 0; i < n; i++)
            wcscpy(s_names[i], s_mixTask.proc[i].name);
        at = NaviMixRunning(s_names, n);
    }
    NaviMixSet(NaviMixHold(s_cfg.naviMix, shell, at >= 0), at >= 0 ? s_names[at] : NULL);
}

static HWAVEOUT s_beepOut;
static WAVEHDR  s_beepHdr;
static short    s_beep[11025 / 2];   // 2 x 120 ms at 1.2 kHz with a gap, 11 kHz mono
static short*   s_glass;             // breaking glass (a new OBD trouble code), 1.2 s, made on its first play
static int      s_glassN;
static bool     s_beepGlass;         // the next BeepPlay: the glass, not the beep
static int      s_beepDevNow = -1;   // the next BeepPlay's device (BeepRoute; -1: WAVE_MAPPER)

static void BeepPlay();

// Breaking glass, made here (no file): a crack - a burst of rough noise falling in 60 ms - then splinters: short
// high pings (2.2..5 kHz, 20..70 ms each) scattered over a second, quieter and sparser towards its end. 11 kHz mono.
static bool GlassMake()
{
    if (s_glass)
        return true;
    s_glassN = 11025 * 12 / 10;
    int* acc = (int*)calloc(s_glassN, sizeof(int));
    s_glass = (short*)malloc(s_glassN * sizeof(short));
    if (!acc || !s_glass) {
        free(acc);
        free(s_glass);
        s_glass = NULL;
        return false;
    }
    unsigned rnd = 12345;
#define GRND() (rnd = rnd * 1103515245 + 12345, (rnd >> 16) & 0x7FFF)
    int prev = 0;
    for (int i = 0; i < 11025 * 9 / 100; i++) {   // the crack: noise, its highs (a difference), fast decay
        int nz = (int)GRND() - 16384, hi = nz - prev;
        prev = nz;
        acc[i] += (int)(hi * 0.9 * exp(-i / (11025 * 0.025)));
    }
    for (int k = 0; k < 34; k++) {   // the splinters
        double at = k < 8 ? GRND() % 1000 / 1000.0 * 0.12 : 0.05 + pow(GRND() % 1000 / 1000.0, 1.6) * 1.0;
        double f = 2200 + GRND() % 2800, len = 0.02 + GRND() % 50 / 1000.0;
        double amp = (5000 + GRND() % 6000) * (1.15 - at);
        int s0 = (int)(at * 11025), n = (int)(len * 11025);
        for (int i = 0; i < n && s0 + i < s_glassN; i++)
            acc[s0 + i] += (int)(amp * exp(-i / (n * 0.25)) * sin(i * 2 * 3.14159265 * f / 11025));
    }
#undef GRND
    for (int i = 0; i < s_glassN; i++)
        s_glass[i] = (short)(acc[i] > 30000 ? 30000 : acc[i] < -30000 ? -30000 : acc[i]);
    free(acc);
    return true;
}

static void Beep2();

static void GlassPlay()   // over the radio, as the beep
{
    if (!GlassMake())
        return;
    s_beepGlass = true;
    Beep2();
}

static void Beep2()
{
    if (s_voice)
        return;   // one is on its way
    BeepWay w = BeepRoute(s_src, s_cfg.beepDev);   // (soundlost.h: on the media - our own output)
    s_beepDevNow = w.dev;
    VLog(L"писк: источник MCU %d, %s, устройство %d", s_src, w.navi ? L"голос навигации" : L"медиа: свой выход", w.dev);
    if (w.navi && NaviVoice(true)) {
        s_voice = 1;
        SetTimer(s_wnd, TIMER_VOICE, kVoiceLeadMs, NULL);
    } else {
        BeepPlay();
    }
}

// TIMER_VOICE: the lead is over (play) or the tail (the prompt has ended).
static void VoiceTimer()
{
    KillTimer(s_wnd, TIMER_VOICE);
    if (s_voice == 1) {
        s_voice = 2;
        SetTimer(s_wnd, TIMER_VOICE, kVoiceMaxMs, NULL);   // the guard (BeepPlay may end it at once: VoiceDone)
        BeepPlay();
    } else if (s_voice == 2) {   // no "done" from the device
        if (s_beepOut)
            waveOutReset(s_beepOut);
        s_voice = 3;
        SetTimer(s_wnd, TIMER_VOICE, kVoiceTailMs, NULL);
    } else if (s_voice == 3) {
        s_voice = 0;
        if (!s_plugVoice)
            NaviVoice(false);
    }
}

// MM_WOM_DONE: the sound has been played.
static void VoiceDone()
{
    if (s_voice == 2) {
        s_voice = 3;
        SetTimer(s_wnd, TIMER_VOICE, kVoiceTailMs, NULL);
    }
}

static void BeepPlay()
{
    if (s_beepOut) {
        if (!(s_beepHdr.dwFlags & WHDR_DONE))
            waveOutReset(s_beepOut);   // the last one hung: dropped
        waveOutUnprepareHeader(s_beepOut, &s_beepHdr, sizeof(s_beepHdr));
        waveOutClose(s_beepOut);
        s_beepOut = NULL;
    }
    WAVEFORMATEX wf;
    memset(&wf, 0, sizeof(wf));
    wf.wFormatTag = WAVE_FORMAT_PCM;
    wf.nChannels = 1;
    wf.nSamplesPerSec = 11025;
    wf.wBitsPerSample = 16;
    wf.nBlockAlign = 2;
    wf.nAvgBytesPerSec = 22050;
    UINT dev = s_beepDevNow < 0 ? WAVE_MAPPER : (UINT)s_beepDevNow;
    if (waveOutOpen(&s_beepOut, dev, &wf, (DWORD)s_wnd, 0, CALLBACK_WINDOW) != MMSYSERR_NOERROR) {
        s_beepOut = NULL;
        VoiceDone();
        return;
    }
    int n = sizeof(s_beep) / sizeof(s_beep[0]);
    for (int i = 0; i < n; i++) {
        int t = i * 1000 / 11025;   // ms
        bool on = t < 120 || (t >= 200 && t < 320);
        s_beep[i] = on ? (short)(9000 * sin(i * 2 * 3.14159265 * 1200 / 11025)) : 0;
    }
    bool glass = s_beepGlass && s_glass;
    s_beepGlass = false;
    memset(&s_beepHdr, 0, sizeof(s_beepHdr));
    s_beepHdr.lpData = glass ? (LPSTR)s_glass : (LPSTR)s_beep;
    s_beepHdr.dwBufferLength = glass ? s_glassN * sizeof(short) : sizeof(s_beep);
    waveOutPrepareHeader(s_beepOut, &s_beepHdr, sizeof(s_beepHdr));
    waveOutWrite(s_beepOut, &s_beepHdr, sizeof(s_beepHdr));
}

// The keypad's tones (DTMF, 120 ms) on "Audio Output" 1 - the user's choice (2026-10-20), as the beep's device.
static HWAVEOUT s_dtmfOut;
static WAVEHDR  s_dtmfHdr;
static short    s_dtmf[11025 * 12 / 100];

static void DtmfPlay(wchar_t k)
{
    static const wchar_t* keys = L"123456789*0#";
    static const int row[4] = { 697, 770, 852, 941 }, col[3] = { 1209, 1336, 1477 };
    const wchar_t* at = wcschr(keys, k == L'+' ? L'0' : k);
    if (!at || !k)
        return;
    int i = (int)(at - keys), f1 = row[i / 3], f2 = col[i % 3];
    if (s_dtmfOut) {
        waveOutReset(s_dtmfOut);
        waveOutUnprepareHeader(s_dtmfOut, &s_dtmfHdr, sizeof(s_dtmfHdr));
        waveOutClose(s_dtmfOut);
        s_dtmfOut = NULL;
    }
    WAVEFORMATEX wf;
    memset(&wf, 0, sizeof(wf));
    wf.wFormatTag = WAVE_FORMAT_PCM;
    wf.nChannels = 1;
    wf.nSamplesPerSec = 11025;
    wf.wBitsPerSample = 16;
    wf.nBlockAlign = 2;
    wf.nAvgBytesPerSec = 22050;
    if (waveOutOpen(&s_dtmfOut, 1, &wf, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        s_dtmfOut = NULL;
        return;
    }
    int n = sizeof(s_dtmf) / sizeof(s_dtmf[0]);
    for (int j = 0; j < n; j++) {
        double a = j < 110 ? j / 110.0 : j > n - 110 ? (n - j) / 110.0 : 1.0;   // 10 ms in and out: no clicks
        s_dtmf[j] = (short)(a * 5000 * (sin(j * 2 * 3.14159265 * f1 / 11025) + sin(j * 2 * 3.14159265 * f2 / 11025)));
    }
    memset(&s_dtmfHdr, 0, sizeof(s_dtmfHdr));
    s_dtmfHdr.lpData = (LPSTR)s_dtmf;
    s_dtmfHdr.dwBufferLength = sizeof(s_dtmf);
    waveOutPrepareHeader(s_dtmfOut, &s_dtmfHdr, sizeof(s_dtmfHdr));
    waveOutWrite(s_dtmfOut, &s_dtmfHdr, sizeof(s_dtmfHdr));
}

// waveOut devices: "АВТО" (WAVE_MAPPER) or "<n> · <its name>". The unit has more than one (wav_proxy.dll: DSP and
// ONCHIP, Bluetooth, iPod): the navigation prompt may need another one than the default.
static void WaveDevName(int dev, wchar_t* s, int n)
{
    WAVEOUTCAPS c;
    if (dev < 0)
        _snwprintf(s, n, L"АВТО (из %u)", (unsigned)waveOutGetNumDevs());
    else if (waveOutGetDevCaps((UINT)dev, &c, sizeof(c)) == MMSYSERR_NOERROR)
        _snwprintf(s, n, L"%d · %s", dev, c.szPname);
    else
        _snwprintf(s, n, L"%d", dev);
    s[n - 1] = 0;
}

static void WaveDevsLog()
{
    UINT n = waveOutGetNumDevs();
    for (UINT i = 0; i < n; i++) {
        wchar_t s[64];
        WaveDevName((int)i, s, 64);
        VLog(L"звук: устройство %s", s);
    }
    if (!n)
        VLog(L"звук: устройств waveOut нет");
    UINT m = waveInGetNumDevs();   // the microphones (the user 2026-11-02: voice commands - is there one for us?)
    for (UINT i = 0; i < m; i++) {
        WAVEINCAPS c;
        memset(&c, 0, sizeof(c));
        waveInGetDevCaps(i, &c, sizeof(c));
        VLog(L"звук: запись %u · %s (форматы %08X, каналов %u)", i, c.szPname, (unsigned)c.dwFormats, c.wChannels);
    }
    if (!m)
        VLog(L"звук: устройств записи (waveIn) нет");
}

// Diagnostics -> "Sound" (the user 2026-11-03: the tests no longer needed out - the formats' files, the voice's and
// the microphone's trials): the beep over the radio and the device it plays on.
static void ScrTestAv(HDC dc)
{
    Frame(dc, L"Звук");
    Panel(dc, 16, 68, 768, 336);   // (above y 404: "back" at the bottom left)
    Button(dc, 40, 84, 352, 46, L"Писк поверх радио", B_N, IC_NONE, 16, 0, A_AVTEST, 0);
    wchar_t s[64], b[96];
    WaveDevName(s_cfg.beepDev, s, 64);
    _snwprintf(b, 96, L"Устройство писка: %s", s);
    Button(dc, 40, 138, 720, 46, b, B_N, IC_NONE, 16, 0, A_AVDEV);   // the next device and a beep on it
    TWrap(dc, 96, 436, L"Писк — через канал голоса навигации (радио тише). Не слышно — выберите другое устройство.", 13,
          kDim, 400, 688, 2, 16);   // the bottom strip, right of "back"
}

static void AvDevNext()
{
    int n = (int)waveOutGetNumDevs();
    s_cfg.beepDev = s_cfg.beepDev + 1 >= n ? -1 : s_cfg.beepDev + 1;
    SaveCfg();
    wchar_t s[64];
    WaveDevName(s_cfg.beepDev, s, 64);
    VLog(L"звук: писк на устройство %s", s);
    Beep2();
}

// Without the road graph a camera is matched to the car's line of travel: in front of the car along the course
// (`along`) and close to that line sideways (`cross` < kCamCross: a parallel street is further), and for a camera
// with a direction the course must agree with it (one-way: within kCamDir; both ways: either way). The warning
// distance is the setting, or by speed (kCamLeadS seconds ahead, 100..500 m) when it is "by speed". The course is
// taken only above 10 km/h and smoothed; a camera once shown stays until it is passed (or left aside), so the
// sign doesn't blink. Left wrong: a frontage road right next to the controlled one, a sharp bend before it.
enum { kCamCross = 30, kCamCrossKeep = 45, kCamDir = 35, kCamLeadS = 10 };
static double s_course = -1;   // smoothed course, -1 = not known yet

static int CamAlertDist(double kmh)
{
    if (s_cfg.camDist > 0)
        return s_cfg.camDist;
    int d = (int)(kmh / 3.6 * kCamLeadS);
    return d < 100 ? 100 : d > 500 ? 500 : d;
}

// Where the camera is from the car: metres ahead along the course and sideways off it.
static void CamOffset(const Gps& g, const Cam& cm, double* along, double* cross)
{
    double k = 3.14159265 / 180;
    double e = (cm.lon - g.lon) * k * cos(g.lat * k) * 6371000, n = (cm.lat - g.lat) * k * 6371000;
    double hx = sin(s_course * k), hy = cos(s_course * k);
    *along = e * hx + n * hy;
    *cross = fabs(e * hy - n * hx);
}

static bool CamFacing(const Cam& cm)
{
    if (cm.dirType == 1)
        return AngleDiff(cm.dir, s_course) <= kCamDir;
    if (cm.dirType == 2) {
        double d = AngleDiff(cm.dir, s_course);
        return d <= kCamDir || d >= 180 - kCamDir;
    }
    return true;   // 0: any direction
}

// The beep for the camera ahead, once per camera: at once, or (cam.over) only while the car goes faster than
// the camera's limit + cam.over km/h - then it comes when the speed gets there, while the camera is still ahead.
// A camera without a known limit beeps only in the "always" mode.
static bool s_camBeeped;

static void CamSound()
{
    if (s_camNear < 0 || s_camBeeped || !s_cfg.camSound)
        return;
    int lim = s_cams[s_camNear].speed;
    if (s_cfg.camOver >= 0 && (!lim || CarKmh() <= lim + s_cfg.camOver))
        return;   // not over the limit (yet)
    s_camBeeped = true;
    Beep2();
}

// On every GPS update: the camera to warn about; true when what is shown changed.
static bool CamCheck(const Gps& g)
{
    int was = s_camNear, wasDist = s_camNearDist / 50;
    if (!s_cfg.camAlert || !GpsFresh(g, 5000)) {
        s_camNear = -1;
        return s_camNear != was;
    }
    if (!s_camLoaded || Dist(g.lat, g.lon, s_camLat, s_camLon) > 75000)
        LoadCams(g.lat, g.lon);
    if (g.speedKmh > 10)   // a standing car's course is noise
        s_course = s_course < 0 || AngleDiff(g.course, s_course) > 60 ? g.course   // a turn: take it as is
                   : s_course + 0.5 * (fmod(g.course - s_course + 540.0, 360.0) - 180);
    if (s_course < 0)
        return false;
    s_course = fmod(s_course + 360.0, 360.0);
    int limit = CamAlertDist(g.speedKmh);
    // keep the shown camera until it is passed or the car turned away from it
    if (s_camNear >= 0) {
        double along, cross;
        CamOffset(g, s_cams[s_camNear], &along, &cross);
        if (along > -10 && along < limit + 100 && cross < kCamCrossKeep) {
            s_camNearDist = (int)(along > 0 ? along : 0);
            CamSound();   // the speed may have gone over the limit since
            return s_camNearDist / 50 != wasDist;
        }
        s_camNear = -1;
    }
    double best = limit;
    for (int i = 0; i < s_camCount; i++) {
        const Cam& cm = s_cams[i];
        if (fabs(cm.lat - g.lat) > 0.01 || fabs(cm.lon - g.lon) > 0.02)
            continue;   // far for sure: skip the trigonometry
        double along, cross;
        CamOffset(g, cm, &along, &cross);
        if (along < 0 || along > best || cross > kCamCross || !CamFacing(cm))
            continue;
        best = along;
        s_camNear = i;
    }
    s_camNearDist = s_camNear >= 0 ? (int)best : 0;
    if (s_camNear >= 0)
        VLog(L"камера: %d м впереди, лимит %d, курс %.0f%s", s_camNearDist, s_cams[s_camNear].speed, s_course,
             s_cfg.camSound ? L", сигнал" : L"");
    if (s_camNear != was)
        s_camBeeped = false;   // a new camera: its own beep
    static bool s_overShown;
    if (CamOver() != s_overShown) {   // the red warning comes / goes: everything showing it redrawn
        s_overShown = !s_overShown;
        return true;
    }
    CamSound();
    return s_camNear != was || s_camNearDist / 50 != wasDist;
}

static bool OutsideTemp(int* t)
{
    Hvac h;
    if (HvacGet(&h) && h.outer > -100) {   // the car's own sensor
        *t = h.outer;
        return true;
    }
    Weather w;
    WxGet(&w);
    *t = w.temp;
    return w.have;
}

// "КЛИМАТ 7/16 · ВЕНТ 3 · A/C" for the plate.
static bool HvacChip(wchar_t* s, int n)
{
    Hvac h;
    if (!HvacGet(&h))
        return false;
    wchar_t t[16] = L"", b[16] = L"";
    if (h.temp >= 0)
        _snwprintf(t, 16, L" %d/16", h.temp);
    if (h.blower >= 0)
        _snwprintf(b, 16, L" · ВЕНТ %d", h.blower);
    _snwprintf(s, n, L"Климат%s%s%s%s", t, b, h.ac == 1 ? L" · A/C" : L"", h.autoMode == 2 ? L" · Авто" : L"");
    s[n - 1] = 0;
    return true;
}

// Speeding at the camera ahead: its limit known and the speed above limit + the threshold (the beep's own condition
// in the "over" mode; in the others the threshold set there too) - shown in red on every screen and on the saver.
static bool CamOver()
{
    if (s_camNear < 0 || !s_cams[s_camNear].speed)
        return false;
    return CarKmh() > s_cams[s_camNear].speed + (s_cfg.camOver >= 0 ? s_cfg.camOver : s_cfg.camOverBy);
}

// A speed limit sign as the real one, whatever the theme: a red ring, white inside, black digits ("КАМ" without a
// known limit).
static void SpeedSign(HDC dc, int cx, int cy, int r, int limit)
{
    Disc(dc, cx, cy, r, RGB(214, 30, 30));
    Disc(dc, cx, cy, r * 3 / 4, RGB(255, 255, 255));
    wchar_t v[8];
    if (limit) {
        _snwprintf(v, 8, L"%d", limit);
        T(dc, cx, cy + r * 28 / 100, v, r * 75 / 100, RGB(0, 0, 0), 700, AL_MID);
    } else {
        T(dc, cx, cy + r * 18 / 100, L"КАМ", r * 42 / 100, RGB(0, 0, 0), 700, AL_MID);
    }
}

static bool CamChip(wchar_t* s, int n)
{
    if (s_camNear < 0)
        return false;
    if (CamOver())   // (the limit is on the sign)
        _snwprintf(s, n, L"%d км/ч · %d м", CarKmh(), s_camNearDist);
    else
        _snwprintf(s, n, L"КАМЕРА · %d м", s_camNearDist);
    return true;
}

static int CamLimit()
{
    return s_camNear >= 0 ? s_cams[s_camNear].speed : 0;
}
