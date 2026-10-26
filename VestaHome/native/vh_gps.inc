// vh_gps.inc - part of home.cpp (included there, in order): GPS (VestaCore vgps), the CAN lines, the clock by GPS.

// ---- GPS: VestaCore (core/vgps.h) - the unit's NMEA port, shared with OSMapsCE: the first program reads it, the
// others take its data from shared memory ("VestaGPS"). The same stream carries the CAN module's '@' lines (car
// speed, steering angle, its version), parsed there too. Here: the monitor's sentence table and can.log, fed by
// the module's taps. The clock, the weather's place, the speed widget and the cameras use the state.
// PC test: gps.port=track.nmea plays a recording from the exe folder.
#include "vgps.h"
#include "vtask.h"   // VestaCore: the task manager's processes, windows and load
#define WM_APP_GPS (WM_APP + 10)
#define WM_APP_WX  (WM_APP + 11)
typedef GpsState Gps;

// ---- The navigators' NMEA port (2026-10-22, the user: "like GpsGate, only NMEA on COM6"): VestaNMEA.dll
// (VestaNMEA/native) gives the '$' lines of the GPS stream from VestaCore's shared memory. The registry and \Windows
// are new at every boot: the driver is copied into \Windows (RAM: the card's copy stays free for the installer) and
// registered again at each start, as a user-mode driver (udevice.exe) in a process group of its own - a fault of it
// takes no other driver along.
// The port's number is the user's (gps.nmeacom, 2026-10-24: Navitel kept COM7 from GpsGate and its autodetect
// only tried that one): COM5..COM9, COM6 by default.
static int   s_nmeaState;   // 0 off, 1 on, -1 failed (s_nmeaErr)
static DWORD s_nmeaErr;

#ifdef UNDER_CE
static HANDLE NmeaActive()   // its device when loaded: Drivers\Active\* with Key = Drivers\VestaNMEA
{
    HKEY act;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"Drivers\\Active", 0, 0, &act) != ERROR_SUCCESS)
        return NULL;
    HANDLE h = NULL;
    wchar_t name[32];
    for (DWORD i = 0; !h; i++) {
        DWORD n = 32;
        if (RegEnumKeyEx(act, i, name, &n, NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
            break;
        HKEY k;
        if (RegOpenKeyEx(act, name, 0, 0, &k) != ERROR_SUCCESS)
            continue;
        wchar_t key[64];
        DWORD sz = sizeof(key), type, hnd = 0;
        if (RegQueryValueEx(k, L"Key", NULL, &type, (BYTE*)key, &sz) == ERROR_SUCCESS &&
            !_wcsicmp(key, L"Drivers\\VestaNMEA")) {
            sz = sizeof(hnd);
            RegQueryValueEx(k, L"Hnd", NULL, &type, (BYTE*)&hnd, &sz);
            h = (HANDLE)hnd;
        }
        RegCloseKey(k);
    }
    RegCloseKey(act);
    return h;
}

static void RegStr(HKEY k, const wchar_t* name, const wchar_t* v)
{
    RegSetValueEx(k, name, 0, REG_SZ, (const BYTE*)v, (wcslen(v) + 1) * sizeof(wchar_t));
}

static void RegDw(HKEY k, const wchar_t* name, DWORD v)
{
    RegSetValueEx(k, name, 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
}
#endif

#ifdef UNDER_CE
// Another device of that name already (HU 2026-10-25: COM7 failed with 1 in device.exe - the unit has a COM7 of its
// own, it seems): its registry key into owner, true when there is one and it is not ours.
static bool ComTaken(int index, wchar_t* owner, int cap)
{
    wchar_t want[8];
    _snwprintf(want, 8, L"COM%d:", index);
    owner[0] = 0;
    HKEY act;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"Drivers\\Active", 0, 0, &act) != ERROR_SUCCESS)
        return false;
    wchar_t name[32];
    for (DWORD i = 0; !owner[0]; i++) {
        DWORD n = 32;
        if (RegEnumKeyEx(act, i, name, &n, NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
            break;
        HKEY k;
        if (RegOpenKeyEx(act, name, 0, 0, &k) != ERROR_SUCCESS)
            continue;
        wchar_t dev[16], key[64];
        DWORD type, sz = sizeof(dev);
        if (RegQueryValueEx(k, L"Name", NULL, &type, (BYTE*)dev, &sz) == ERROR_SUCCESS && !_wcsicmp(dev, want)) {
            sz = sizeof(key);
            if (RegQueryValueEx(k, L"Key", NULL, &type, (BYTE*)key, &sz) != ERROR_SUCCESS)
                wcscpy(key, L"?");
            if (_wcsicmp(key, L"Drivers\\VestaNMEA")) {
                wcsncpy(owner, key, cap - 1);
                owner[cap - 1] = 0;
            }
        }
        RegCloseKey(k);
    }
    RegCloseKey(act);
    return owner[0] != 0;
}
#endif

static wchar_t s_nmeaOwner[64];   // the port's number taken by this driver (s_nmeaState -2)

static void NmeaPortSet(bool on)
{
#ifdef UNDER_CE
    HANDLE h = NmeaActive();
    if (!on) {
        if (h)
            VLog(L"порт NMEA: выключен (%s)", DeactivateDevice(h) ? L"выгружен" : L"не выгрузился");
        s_nmeaState = 0;
        return;
    }
    if (h) {   // loaded already (VestaHome started again; a new number: NmeaPortSet(false) first)
        s_nmeaState = 1;
        return;
    }
    if (ComTaken(s_cfg.nmeaCom, s_nmeaOwner, 64)) {
        s_nmeaState = -2;
        VLog(L"порт NMEA: COM%d уже занят (%s) - выберите другой номер", s_cfg.nmeaCom, s_nmeaOwner);
        return;
    }
    wchar_t src[MAX_PATH];
    _snwprintf(src, MAX_PATH, L"%sVestaNMEA.dll", s_dir);
    s_nmeaErr = 0;
    if (!CopyFile(src, L"\\Windows\\VestaNMEA.dll", FALSE) &&
        GetFileAttributes(L"\\Windows\\VestaNMEA.dll") == 0xFFFFFFFF) {   // (still held after a reload: the copy there)
        s_nmeaErr = GetLastError();
        VLog(L"порт NMEA: нет %s (%lu)", src, s_nmeaErr);
        s_nmeaState = -1;
        return;
    }
    HKEY k;
    DWORD disp;
    if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, L"Drivers\\ProcGroup_0020", 0, NULL, 0, 0, NULL, &k, &disp) ==
        ERROR_SUCCESS) {   // its own udevice.exe (the firmware's groups 2 and 3 are the same but shared)
        RegStr(k, L"ProcName", L"udevice.exe");
        RegStr(k, L"ProcVolPrefix", L"$udevice");
        RegCloseKey(k);
    }
    // Where it runs, tried in turn (HU 2026-10-24-2: the user-mode group failed with 2 - no udevice.exe to start, it
    // seems): its own udevice.exe group, the firmware's group 3, then in device.exe itself (no Flags).
    static const DWORD kGroup[3] = { 0x20, 3, 0 };
    static const wchar_t* kWhere[3] = { L"своя группа udevice", L"группа 3 прошивки", L"в device.exe" };
    for (int i = 0; i < 3 && !h; i++) {
        if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, L"Drivers\\VestaNMEA", 0, NULL, 0, 0, NULL, &k, &disp) != ERROR_SUCCESS)
            break;
        RegStr(k, L"Dll", L"VestaNMEA.dll");
        RegStr(k, L"Prefix", L"COM");
        RegDw(k, L"Index", s_cfg.nmeaCom);
        RegStr(k, L"FriendlyName", L"VestaHome NMEA port");
        if (kGroup[i]) {
            RegDw(k, L"Flags", 0x10);   // DEVFLAGS_LOAD_AS_USERPROC
            RegDw(k, L"UserProcGroup", kGroup[i]);
        } else {
            RegDeleteValue(k, L"Flags");
            RegDeleteValue(k, L"UserProcGroup");
        }
        RegCloseKey(k);
        h = ActivateDeviceEx(L"Drivers\\VestaNMEA", NULL, 0, NULL);
        s_nmeaErr = h ? 0 : GetLastError();
        VLog(L"порт NMEA: COM%d %s, %s (%lu)", s_cfg.nmeaCom, h ? L"загружен" : L"не загрузился", kWhere[i], s_nmeaErr);
    }
    s_nmeaState = h ? 1 : -1;
#else
    s_nmeaState = on ? 1 : 0;   // PC: the driver is the unit's only
    VLog(L"порт NMEA: ПК - только запомнено (%d)", on ? 1 : 0);
#endif
}

// ---- The USB modem's probe (2026-10-24, VestaModem/native): its client driver copied into \Windows and its keys
// written - USBD reads them when a device comes, so the modem goes in after this. Only Huawei's stick for now: its
// CD-ROM mode (12D1:155B) and the modem (12D1:1506), whole devices (Default\Default); the registry is new at every
// boot, so it lasts until the unit restarts. The driver logs into VestaLogs\VestaModem.log.
// (its state: s_modemState, s_modemErr - in vh_diag.inc, which shows it)

static void ModemUsbSet()
{
#ifdef UNDER_CE
    static const wchar_t* kKeys[2] = { L"Drivers\\USB\\LoadClients\\4817_5467\\Default\\Default\\VestaModem",
                                       L"Drivers\\USB\\LoadClients\\4817_5382\\Default\\Default\\VestaModem" };
    wchar_t src[MAX_PATH];
    _snwprintf(src, MAX_PATH, L"%sVestaModem.dll", s_dir);
    s_modemErr = 0;
    if (!CopyFile(src, L"\\Windows\\VestaModem.dll", FALSE) &&
        GetFileAttributes(L"\\Windows\\VestaModem.dll") == 0xFFFFFFFF) {   // (loaded already: the copy there)
        s_modemErr = GetLastError();
        s_modemState = -1;
        VLog(L"USB-модем: нет %s (%lu)", src, s_modemErr);
        return;
    }
    for (int i = 0; i < 2; i++) {
        HKEY k;
        DWORD disp;
        LONG r = RegCreateKeyEx(HKEY_LOCAL_MACHINE, kKeys[i], 0, NULL, 0, 0, NULL, &k, &disp);
        if (r != ERROR_SUCCESS) {
            s_modemErr = r;
            s_modemState = -1;
            VLog(L"USB-модем: ключ %s - ошибка %ld", kKeys[i], r);
            return;
        }
        RegStr(k, L"DLL", L"VestaModem.dll");
        RegCloseKey(k);
    }
    s_modemState = 1;
    VLog(L"USB-модем: драйвер зарегистрирован (Huawei 12D1:155B, 12D1:1506) - вставьте модем");
#else
    s_modemState = 1;   // PC: the driver is the unit's only
    VLog(L"USB-модем: ПК - только отмечено");
#endif
}

// The switch flips at once, the work (up to some seconds: three ways tried) in a thread of its own; a failure turns
// the setting off again (WM_APP_NMEA). reload: off first (a new number).
#define WM_APP_NMEA (WM_APP + 28)
static volatile LONG s_nmeaBusy;
static bool s_nmeaWant, s_nmeaReload;

static DWORD WINAPI NmeaThread(LPVOID)
{
    if (s_nmeaReload)
        NmeaPortSet(false);
    NmeaPortSet(s_nmeaWant);
    InterlockedExchange((LONG*)&s_nmeaBusy, 0);
    PostMessage(s_wnd, WM_APP_NMEA, s_nmeaWant && s_nmeaState <= 0, 0);
    return 0;
}

static void NmeaPortStart(bool on, bool reload)
{
    if (InterlockedExchange((LONG*)&s_nmeaBusy, 1))
        return;   // (one at a time: the switch shows the setting, the result comes with WM_APP_NMEA)
    s_nmeaWant = on;
    s_nmeaReload = reload;
    HANDLE t = CreateThread(NULL, 0, NmeaThread, NULL, 0, NULL);
    if (t)
        CloseHandle(t);
    else
        NmeaThread(NULL);
}

// The stream (G3NavHMI: rn2::CanMessageParser on COM4, a copy of COM3, 115200): '$' = NMEA, '@' = the CAN
// module ("@,<speed km/h>,<version>*hh", "@ANG,<steering angle | null>,*hh"). The monitor counts every type and
// can record the stream (can.log next to the exe): raw bytes while this program reads the port, the checked
// lines while another one does.
struct Sentence {
    char  name[12];              // "$GPRMC", "@", "@ANG"...
    DWORD count, tick;
    char  last[100];             // the last one, cut
};
enum { kSentMax = 24, kCanLogMax = 2 * 1024 * 1024 };
static Sentence s_sent[kSentMax];  // under s_gpsCs
static int      s_sentCount;
static DWORD    s_streamBin;                 // bytes outside printable ASCII and CR/LF (while we read the port)
static volatile DWORD s_rawTick;             // when the module last handed raw bytes (we read the port)
static HANDLE   s_canLog = INVALID_HANDLE_VALUE;   // the stream's record, opened by the monitor
static DWORD    s_canLogBytes;
static int      s_canMarks;
static CRITICAL_SECTION s_gpsCs;           // the table and can.log

static bool GpsFresh(const Gps& g, DWORD ms)
{
    return g.valid && GetTickCount() - g.fixTick < ms;
}

static bool GpsHasFix()
{
    Gps g;
    GpsGet(&g);
    return GpsFresh(g, 5000);
}

// Counts a sentence by its first field.
static void NoteSentence(const char* s)
{
    char name[12];
    int n = 0;
    while (s[n] && s[n] != ',' && s[n] != '*' && n < 11) {
        name[n] = (unsigned char)s[n] < 0x20 || (unsigned char)s[n] > 0x7E ? '?' : s[n];
        n++;
    }
    name[n] = 0;
    EnterCriticalSection(&s_gpsCs);
    int i = 0;
    while (i < s_sentCount && strcmp(s_sent[i].name, name))
        i++;
    if (i == s_sentCount && s_sentCount < kSentMax) {
        memset(&s_sent[i], 0, sizeof(Sentence));
        strcpy(s_sent[i].name, name);
        s_sentCount++;
    }
    if (i < s_sentCount) {
        s_sent[i].count++;
        s_sent[i].tick = GetTickCount();
        int k = 0;
        for (; s[k] && k < (int)sizeof(s_sent[i].last) - 1; k++)
            s_sent[i].last[k] = (unsigned char)s[k] < 0x20 || (unsigned char)s[k] > 0x7E ? '.' : s[k];
        s_sent[i].last[k] = 0;
    }
    LeaveCriticalSection(&s_gpsCs);
}

// The stream into can.log, up to 2 MB: the material for decoding what the CAN module sends. Only changes: a line
// the same as the last written one of its type (the text before the first comma) is left out - "@ANG" alone comes
// ~100 times a second. Marks ("#MARK n uptime") are put in from the monitors when something is done in the car.
struct CanLast {
    char key[12];
    char line[100];
};
static CanLast  s_canLast[kSentMax];   // under s_gpsCs
static int      s_canLastCount;
static char     s_canPart[128];        // a raw line not finished yet
static int      s_canPartLen;

static void CanLogWrite(const char* buf, DWORD len)
{
    EnterCriticalSection(&s_gpsCs);
    if (s_canLog != INVALID_HANDLE_VALUE) {
        DWORD w = 0;
        if (s_canLogBytes + len > kCanLogMax || !WriteFile(s_canLog, buf, len, &w, NULL)) {
            CloseHandle(s_canLog);
            s_canLog = INVALID_HANDLE_VALUE;
            VLog(L"can.log: закрыт, %u байт", (unsigned)s_canLogBytes);
        } else {
            s_canLogBytes += w;
        }
    }
    LeaveCriticalSection(&s_gpsCs);
}

// One line (no CR/LF) into can.log, unless it repeats the last one of its type.
static void CanLogLine(const char* s, int len)
{
    char key[12];
    int n = 0;
    while (n < len && s[n] != ',' && s[n] != '*' && n < 11) {
        key[n] = s[n];
        n++;
    }
    key[n] = 0;
    EnterCriticalSection(&s_gpsCs);
    if (s_canLog != INVALID_HANDLE_VALUE && len > 0) {
        int i = 0;
        while (i < s_canLastCount && strcmp(s_canLast[i].key, key))
            i++;
        if (i == s_canLastCount && s_canLastCount < kSentMax) {
            strcpy(s_canLast[i].key, key);
            s_canLast[i].line[0] = 0;
            s_canLastCount++;
        }
        bool same = false;
        if (i < s_canLastCount) {
            CanLast& c = s_canLast[i];
            same = len < (int)sizeof(c.line) && (int)strlen(c.line) == len && !memcmp(c.line, s, len);
            if (!same) {
                int k = len < (int)sizeof(c.line) - 1 ? len : (int)sizeof(c.line) - 1;
                memcpy(c.line, s, k);
                c.line[k] = 0;
            }
        }
        if (!same) {
            CanLogWrite(s, (DWORD)len);
            CanLogWrite("\r\n", 2);
        }
    }
    LeaveCriticalSection(&s_gpsCs);
}

// Raw bytes (we read the port): cut into lines for CanLogLine.
static void CanLogRaw(const char* buf, DWORD len)
{
    EnterCriticalSection(&s_gpsCs);
    if (s_canLog != INVALID_HANDLE_VALUE) {
        for (DWORD i = 0; i < len; i++) {
            char c = buf[i];
            if (c == '\n' || c == '\r') {
                CanLogLine(s_canPart, s_canPartLen);
                s_canPartLen = 0;
                continue;
            }
            if (s_canPartLen == (int)sizeof(s_canPart)) {   // too long for a line of the module: as it is
                CanLogLine(s_canPart, s_canPartLen);
                s_canPartLen = 0;
            }
            s_canPart[s_canPartLen++] = c;
        }
    }
    LeaveCriticalSection(&s_gpsCs);
}

static void CanLogToggle()
{
    EnterCriticalSection(&s_gpsCs);
    if (s_canLog != INVALID_HANDLE_VALUE) {
        CloseHandle(s_canLog);
        s_canLog = INVALID_HANDLE_VALUE;
        _snwprintf(s_status, 96, L"запись остановлена: can.log, %u КБ", (unsigned)(s_canLogBytes / 1024));
    } else {
        wchar_t path[MAX_PATH];
        if (!LogDir(path, s_dir)) {   // records go only to the SD card / USB disk
            wcscpy(s_status, L"нет SD-карты или USB-диска для записи");
            LeaveCriticalSection(&s_gpsCs);
            return;
        }
        wcscat(path, L"can.log");
        s_canLog = CreateFile(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        s_canLogBytes = 0;
        s_canMarks = 0;
        s_canLastCount = 0;
        s_canPartLen = 0;
        wcscpy(s_status, s_canLog != INVALID_HANDLE_VALUE ? L"запись потока в can.log" : L"can.log не создаётся");
    }
    LeaveCriticalSection(&s_gpsCs);
    VLog(L"can.log: %s", s_canLog != INVALID_HANDLE_VALUE ? L"запись" : L"стоп");
}

// The module's taps (its thread): raw bytes while this program reads the port, every checked line always.
static void GpsTapRaw(const char* buf, DWORD len)
{
    s_rawTick = GetTickCount();
    for (DWORD i = 0; i < len; i++) {
        unsigned char c = (unsigned char)buf[i];
        if (c > 0x7E || (c < 0x20 && c != '\r' && c != '\n'))
            s_streamBin++;
    }
    CanLogRaw(buf, len);
}

static void GpsTapLine(const char* line)
{
    NoteSentence(line);
    if (GetTickCount() - s_rawTick > 2000)   // another program reads the port: record its lines
        CanLogLine(line, (int)strlen(line));
}

static void GpsLogLine(const char* line)
{
    VLog(L"%S", line);
}

static void GpsStartCore()
{
    InitializeCriticalSection(&s_gpsCs);
    GpsConfig c;
    memset(&c, 0, sizeof(c));
    c.notify = s_wnd;
    c.msg = WM_APP_GPS;
    c.port = s_cfg.gpsPort;
    c.baud = s_cfg.gpsBaud;
    c.app = L"VestaHome";
    c.log = GpsLogLine;
    c.exeDir = s_dir;
    c.onLine = GpsTapLine;
    c.onRaw = GpsTapRaw;
    GpsStart(&c);
}

static void GpsReopen()
{
    GpsConfigure(s_cfg.gpsPort, s_cfg.gpsBaud);
}

static void GpsStopThread()
{
    GpsStop();
    if (s_canLog != INVALID_HANDLE_VALUE)
        CanLogToggle();   // close the record
}

// One line for the settings and the speed widget: what the port gives.
// brief: a short form for a row with a control on the right (no port name, no coordinates).
static void GpsStatusText(wchar_t* s, int n, bool brief)
{
    Gps g;
    GpsGet(&g);
    if (g.status == GPS_BUSY)
        _snwprintf(s, n, brief ? L"занят" : L"%s занят", g.port);
    else if (g.status != GPS_OPEN)
        _snwprintf(s, n, brief ? L"не открывается" : L"%s не открывается", g.port[0] ? g.port : s_cfg.gpsPort);
    else if (!g.lines || GetTickCount() - g.dataTick > 10000)
        wcsncpy(s, brief ? L"нет NMEA" : L"нет NMEA: выдачу включает навигация", n);
    else if (!GpsFresh(g, 5000))
        _snwprintf(s, n, brief ? L"нет фикса · спутников %d" : L"NMEA есть, нет фикса · спутников %d", g.sats);
    else if (brief)
        _snwprintf(s, n, L"фикс · спутников %d", g.sats);
    else
        _snwprintf(s, n, L"фикс · спутников %d · %.5f, %.5f", g.sats, g.lat, g.lon);
    s[n - 1] = 0;
    if (!g.owner && g.ownerApp[0]) {   // the port is read by another program (VestaCore shared memory)
        int l = (int)wcslen(s);
        _snwprintf(s + l, n - l, L" · через %s", g.ownerApp);
        s[n - 1] = 0;
    }
}

// The port and its speed on the "General" settings: arrows step through COM1..COM9 and kGpsBauds (as in
// YaMapsCE / OSMapsCE); every step is saved and the port reopened. A port that is not COMn (the PC's
// track.nmea) is shown as it is until an arrow is pressed.
static const DWORD kGpsBauds[] = { 0, 4800, 9600, 19200, 38400, 57600, 115200 };   // 0 = leave as is
enum { kGpsBaudCount = sizeof(kGpsBauds) / sizeof(kGpsBauds[0]) };

static int GpsPortNum()
{
    const wchar_t* p = s_cfg.gpsPort;
    return !_wcsnicmp(p, L"COM", 3) && p[3] >= L'1' && p[3] <= L'9' ? p[3] - L'0' : 0;
}

static void GpsPortText(wchar_t* s, int n)
{
    int k = GpsPortNum();
    if (k)
        _snwprintf(s, n, L"COM%d", k);
    else
        wcsncpy(s, s_cfg.gpsPort, n);
    s[n - 1] = 0;
}

static void GpsBaudText(wchar_t* s, int n)
{
    if (s_cfg.gpsBaud)
        _snwprintf(s, n, L"%u", (unsigned)s_cfg.gpsBaud);
    else
        wcsncpy(s, L"не менять", n);
    s[n - 1] = 0;
}

static void GpsPortStep(int dir)
{
    int k = GpsPortNum();
    k = k ? k + dir : dir > 0 ? 1 : 9;
    k = k < 1 ? 9 : k > 9 ? 1 : k;
    _snwprintf(s_cfg.gpsPort, 16, L"COM%d:", k);
    SaveCfg();
    GpsReopen();
}

static void GpsBaudStep(int dir)
{
    int i = 0;
    while (i < kGpsBaudCount && kGpsBauds[i] != s_cfg.gpsBaud)
        i++;
    i = i == kGpsBaudCount ? 0 : (i + dir + kGpsBaudCount) % kGpsBaudCount;
    s_cfg.gpsBaud = kGpsBauds[i];
    SaveCfg();
    GpsReopen();
}

// SYSTEMTIME moved by minutes (through FILETIME, 100 ns units).
static bool AddMinutes(SYSTEMTIME* t, int minutes)
{
    FILETIME ft;
    if (!SystemTimeToFileTime(t, &ft))
        return false;
    *(__int64*)&ft += (__int64)minutes * 600000000;
    return FileTimeToSystemTime(&ft, t) != 0;
}

// The clock from GPS (RMC is UTC): when it is off by more than 30 s, at most once an hour.
// Set as local time = UTC + time.tz: the unit's own time zone comes back to the CE default (Pacific, UTC-8)
// with the registry on every boot, so SetSystemTime(UTC) showed 22:00 for 09:00 Moscow.
static DWORD s_timeSyncTick;
static bool  s_timeSynced;

static bool McuSend6(const char* name, int a, int b, int c, int d, int e, int f);

// Sets the clock and gives it to the MCU too: at boot the unit's time comes from the MCU's clock, so a time set
// only with SetLocalTime was gone after a reboot (2026-10-17). As the stock shell does it (CSetupTimeDlg,
// Ghidra): SPApi_SetClockTime(hour, minute, second, year % 100, month, day) = MCU message 0x3C01.
static void ClockSet(const SYSTEMTIME& t)
{
    SetLocalTime(&t);
    McuSend6("SPApi_SetClockTime", t.wHour, t.wMinute, t.wSecond, t.wYear % 100, t.wMonth, t.wDay);
}

static void GpsTimeSync(const Gps& g)
{
    if (!s_cfg.gpsTime || !GpsFresh(g, 2000) || !g.date || (s_timeSynced && GetTickCount() - s_timeSyncTick < 3600000))
        return;
    SYSTEMTIME gt, now;
    memset(&gt, 0, sizeof(gt));
    gt.wYear = (WORD)(g.date / 10000);
    gt.wMonth = (WORD)(g.date / 100 % 100);
    gt.wDay = (WORD)(g.date % 100);
    gt.wHour = (WORD)(g.time / 10000);
    gt.wMinute = (WORD)(g.time / 100 % 100);
    gt.wSecond = (WORD)(g.time % 100);
    if (!AddMinutes(&gt, s_cfg.tzMin))
        return;
    GetLocalTime(&now);
    FILETIME a, b;
    if (!SystemTimeToFileTime(&gt, &a) || !SystemTimeToFileTime(&now, &b))
        return;
    __int64 d = (*(__int64*)&a - *(__int64*)&b) / 10000000;
    s_timeSynced = true;
    s_timeSyncTick = GetTickCount();
    if (d > 30 || d < -30) {
        VLog(L"часы: по GPS %02u:%02u:%02u (UTC%+d мин), было %02u:%02u:%02u, сдвиг %+d с", gt.wHour, gt.wMinute,
             gt.wSecond, s_cfg.tzMin, now.wHour, now.wMinute, now.wSecond, (int)d);
        ClockSet(gt);
        VLog(L"часы: выставлены");
        _snwprintf(s_status, 96, L"время по GPS (было %+d с)", (int)-d);
    }
}

static int TzMinutes()
{
    return s_cfg.tzMin;
}

// The zone arrows: one hour a step, UTC-12 .. UTC+14. The clock moves with the zone right away (as a phone's
// does), and the next GPS fix sets it again with the new zone.
static void TzStep(int dir)
{
    int m = s_cfg.tzMin + dir * 60;
    if (m < -12 * 60 || m > 14 * 60)
        return;
    SYSTEMTIME t;
    GetLocalTime(&t);
    if (AddMinutes(&t, m - s_cfg.tzMin))
        ClockSet(t);
    VLog(L"часовой пояс: %+d -> %+d мин", s_cfg.tzMin, m);
    s_cfg.tzMin = m;
    s_timeSynced = false;
    SaveCfg();
}

// The clock by hand ("General"): the arrows step the hour or the minute within the day, seconds to 0.
static void ClockStep(bool hour, int dir)
{
    SYSTEMTIME t;
    GetLocalTime(&t);
    if (hour)
        t.wHour = (WORD)((t.wHour + dir + 24) % 24);
    else
        t.wMinute = (WORD)((t.wMinute + dir + 60) % 60);
    t.wSecond = 0;
    t.wMilliseconds = 0;
    ClockSet(t);
    VLog(L"часы вручную: %02u:%02u", t.wHour, t.wMinute);
}
