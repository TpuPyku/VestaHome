// vh_speedcam.inc - part of home.cpp (included there, in order): speed cameras.

// ---- speed cameras: speedcam.txt in the iGO (X,Y,TYPE,SPEED,DIRTYPE,DIRECTION) or Navitel
// (IDX,X,Y,TYPE,SPEED,DIRTYPE,DIRECTION) format, columns found by the header. Only the cameras within 150 km of
// the car (or the home point) are kept, reloaded after 75 km: a country's list doesn't fit the unit's memory.
// A camera ahead (see CamCheck) shows on the speed widget and in the top bar, with two beeps when it comes up.
struct Cam { float lat, lon; unsigned char speed, dirType; short dir; };
enum { kMaxCams = 6000 };
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

static double Bearing(double lat1, double lon1, double lat2, double lon2)   // degrees from north
{
    double k = 3.14159265 / 180;
    double b = atan2((lon2 - lon1) * cos(lat1 * k), lat2 - lat1) / k;
    return b < 0 ? b + 360 : b;
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
    cm.speed = (unsigned char)(n > s_camCol[3] ? atoi(f[s_camCol[3]]) : 0);
    cm.dirType = (unsigned char)(n > s_camCol[4] ? atoi(f[s_camCol[4]]) : 0);
    cm.dir = (short)(n > s_camCol[5] ? atoi(f[s_camCol[5]]) : 0);
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
    for (int k = 0; k < 6; k++)
        s_camCol[k] = k;   // the iGO order when there is no header
    s_camFirst = true;
    wchar_t path[MAX_PATH];
    CamPath(path);
    HANDLE h = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        VLog(L"камеры: нет файла %s", path);
        return;
    }
    DWORD t0 = GetTickCount();
    VLog(L"камеры: читаю %s (%u Б) у %.4f, %.4f", path, (unsigned)GetFileSize(h, NULL), lat, lon);
    static char chunk[16384];
    char line[256];
    int ll = 0;
    for (;;) {
        DWORD got = 0;
        bool eof = !ReadFile(h, chunk, sizeof(chunk), &got, NULL) || !got;
        for (DWORD i = 0; i < got; i++) {
            char c = chunk[i];
            if (c == '\n' || c == '\r') {
                line[ll] = 0;
                CamLine(line);
                ll = 0;
            } else if (ll < (int)sizeof(line) - 1) {
                line[ll++] = c;
            }
        }
        if (eof) {
            line[ll] = 0;
            CamLine(line);
            break;
        }
    }
    CloseHandle(h);
    VLog(L"камеры: %d в 150 км из %d за %u мс", s_camCount, s_camTotal, (unsigned)(GetTickCount() - t0));
}

// A sound over the radio, as an external navigator says its prompts (Ghidra, PROJECT.md): WM_NAVIGATION_VOICE
// (0x101D, wParam 1 = a prompt starts, 0 = ended) to the stock shell's main window "RadioCommWindow"; the shell
// tells the MCU (SPApi_SendRadioNaviVoiceGuidance), which turns the radio down and lets our waveOut through.
// The sound starts kVoiceLeadMs after "1" (the MCU turns the radio down meanwhile), "0" goes kVoiceTailMs after
// it ends. No shell window: played at once. A device that never says "done" (Bluetooth HF without a phone, HU
// 2026-10-18: the "1" stayed on, the knob then set the navigation's volume) is cut after kVoiceMaxMs.
enum { TIMER_VOICE = 9, WM_NAVIGATION_VOICE = 0x101D, kVoiceLeadMs = 1000, kVoiceTailMs = 800, kVoiceMaxMs = 2000 };
static int s_voice;   // 0 idle, 1 waiting to play, 2 playing, 3 waiting to end

static bool NaviVoice(bool on)
{
    HWND h = FindWindow(NULL, L"RadioCommWindow");
    if (h)
        PostMessage(h, WM_NAVIGATION_VOICE, on ? 1 : 0, 0);
    static int s_logged = -1;
    if (s_logged != (h != NULL)) {
        s_logged = h != NULL;
        VLog(L"звук поверх радио: %s", h ? L"через оболочку (RadioCommWindow, 0x101D)" : L"окна оболочки нет — сразу");
    }
    return h != NULL;
}

static HWAVEOUT s_beepOut;
static WAVEHDR  s_beepHdr;
static short    s_beep[11025 / 2];   // 2 x 120 ms at 1.2 kHz with a gap, 11 kHz mono
static short*   s_glass;             // breaking glass (a new OBD trouble code), 1.2 s, made on its first play
static int      s_glassN;
static bool     s_beepGlass;         // the next BeepPlay: the glass, not the beep

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
    if (NaviVoice(true)) {
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
    UINT dev = s_cfg.beepDev < 0 ? WAVE_MAPPER : (UINT)s_cfg.beepDev;
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

// Diagnostics -> "Sound and video": the beep over the radio and short test files (VestaHome/tools/
// make_testmedia.py -> Tests\SDMemory\TestMedia) played by our own players. The folder: TestMedia on the SD card,
// a USB disk or next to the exe.
struct AvTest { const wchar_t* label; const wchar_t* file; bool video; };
static const AvTest kAvTests[10] = {
    { L"ПИСК ПОВЕРХ РАДИО", NULL, false },     { L"ЛЕВО → ПРАВО (MP3)", L"lr.mp3", false },
    { L"СВИП 40 Гц – 12 кГц", L"sweep.mp3", false }, { L"MP3 + теги", L"test.mp3", false },
    { L"M4A (AAC)", L"test.m4a", false },      { L"FLAC", L"test.flac", false },
    { L"WAV", L"test.wav", false },            { L"ВИДЕО MP4 (H.264)", L"test.mp4", true },
    { L"ВИДЕО AVI (XviD)", L"test.avi", true }, { L"ВИДЕО MKV (H.264)", L"test.mkv", true } };

// The TestMedia folder with a trailing '\', or "" when there is none.
static const wchar_t* AvDir()
{
    static wchar_t d[MAX_PATH];
    const wchar_t* roots[4] = { L"\\SDMemory\\", L"\\USBDisk\\", L"\\USBDevice\\", s_dir };
    for (int i = 0; i < 4; i++) {
        _snwprintf(d, MAX_PATH, L"%sTestMedia\\", roots[i]);
        d[MAX_PATH - 1] = 0;
        if (GetFileAttributes(d) != 0xFFFFFFFF)
            return d;
    }
    d[0] = 0;
    return d;
}

static bool AvHas(const wchar_t* dir, const wchar_t* file)
{
    wchar_t p[MAX_PATH];
    _snwprintf(p, MAX_PATH, L"%s%s", dir, file);
    p[MAX_PATH - 1] = 0;
    return dir[0] && GetFileAttributes(p) != 0xFFFFFFFF;
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
}

static void ScrTestAv(HDC dc)
{
    Frame(dc, L"Звук и видео");
    Panel(dc, 16, 68, 768, 396);
    const wchar_t* dir = AvDir();
    for (int i = 0; i < 10; i++) {
        bool ok = !kAvTests[i].file || AvHas(dir, kAvTests[i].file);
        Button(dc, 40 + (i % 2) * 368, 84 + (i / 2) * 54, 352, 46, kAvTests[i].label, ok ? B_N : B_DIS, IC_NONE,
               16, 0, ok ? A_AVTEST : A_NONE, i);
    }
    wchar_t s[160];
    WaveDevName(s_cfg.beepDev, s, 64);
    wchar_t b[96];
    _snwprintf(b, 96, L"УСТРОЙСТВО ПИСКА: %s", s);
    Button(dc, 40, 356, 470, 42, b, B_N, IC_NONE, 15, 0, A_AVDEV);   // the next device and a beep on it
    Button(dc, 520, 356, 240, 42, L"СТЕКЛО (ОШИБКА OBD)", B_N, IC_NONE, 15, 0, A_AVTEST, 100);
    if (dir[0])
        _snwprintf(s, 160, L"Файлы: %s. Писк — через канал голоса навигации (радио тише).", dir);
    else
        wcscpy(s, L"Нет папки TestMedia (на SD-карте, USB или рядом с VestaHome) — работает только писк.");
    T(dc, 40, 420, s, 14, kDim, 400, AL_START, 720);
    T(dc, 40, 444, L"Лево → право: 3 с слева, 3 с справа, 3 с оба. Свип — для проверки эквалайзера.", 14, kDim,
      400, AL_START, 720);
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

static void AvTestRun(int i)
{
    if (i == 100) {   // the OBD's new trouble code sound
        GlassPlay();
        return;
    }
    if (i < 0 || i >= 10)
        return;
    if (!kAvTests[i].file) {
        Beep2();
        return;
    }
    const wchar_t* dir = AvDir();
    if (!AvHas(dir, kAvTests[i].file))
        return;
    wchar_t d[MAX_PATH];
    wcscpy(d, dir);
    if (kAvTests[i].video) {
        VidLoadDir(d, kAvTests[i].file);
        Go(S_VIDEO_PLAY, true);
        return;
    }
    if (!BassLoad()) {
        wcscpy(s_status, s_bass.state == -1 ? L"нет bass.dll: папка bass рядом с VestaHome" : L"звук не запускается (BASS_Init)");
        return;
    }
    if (PlayerLoadDir(d, kAvTests[i].file))
        Go(S_AUDIO, true);
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
    _snwprintf(s, n, L"КЛИМАТ%s%s%s%s", t, b, h.ac == 1 ? L" · A/C" : L"", h.autoMode == 2 ? L" · АВТО" : L"");
    s[n - 1] = 0;
    return true;
}

static bool CamChip(wchar_t* s, int n)
{
    if (s_camNear < 0)
        return false;
    if (s_cams[s_camNear].speed)
        _snwprintf(s, n, L"КАМЕРА %d · %d м", s_cams[s_camNear].speed, s_camNearDist);
    else
        _snwprintf(s, n, L"КАМЕРА · %d м", s_camNearDist);
    return true;
}
