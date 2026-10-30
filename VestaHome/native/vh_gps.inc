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
    {   // the DLL in this process first (HU 2026-10-25: 2 in udevice.exe, 1 in device.exe - does it load at all?)
        HMODULE m = LoadLibrary(L"\\Windows\\VestaNMEA.dll");
        DWORD le = m ? 0 : GetLastError();
        FARPROC init = m ? GetProcAddress(m, L"COM_Init") : NULL;
        DWORD size = 0;
        HANDLE f = CreateFile(L"\\Windows\\VestaNMEA.dll", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        if (f != INVALID_HANDLE_VALUE) {
            size = GetFileSize(f, NULL);
            CloseHandle(f);
        }
        VLog(L"порт NMEA: проба DLL здесь - %s (%lu), COM_Init %s; размер %lu Б", m ? L"загружена" : L"НЕ загружена", le,
             init ? L"есть" : L"нет", size);
        if (m)
            FreeLibrary(m);
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

// ---- The USB modem (2026-10-24, VestaModem/native): its client driver copied into \Windows and its keys written -
// USBD reads them when a device comes, so the modem goes in after this. Only Huawei's stick for now: its CD-ROM mode
// (12D1:155B) and the modem (12D1:1506), whole devices (Default\Default); the registry is new at every boot, so it
// lasts until the unit restarts. With the modem in, the driver brings up its COM port from the third key (step 2,
// 2026-10-29): a TAPI modem for Unimodem (an external one: AT, ATE0V1&C1&D2, then ATDT*99#). The driver logs into
// VestaLogs\VestaModem.log. (its state: s_modemState, s_modemErr - in vh_diag.inc, which shows it)
static const wchar_t kModemLine[] = L"VestaModem USB";   // Unimodem's line: the RAS entry's device

static void ModemUsbSet()
{
#ifdef UNDER_CE
    static const wchar_t* kKeys[3] = { L"Drivers\\USB\\LoadClients\\4817_5467\\Default\\Default\\VestaModem",
                                       L"Drivers\\USB\\LoadClients\\4817_5382\\Default\\Default\\VestaModem",
                                       L"Drivers\\VestaModemCom" };
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
    for (int i = 0; i < 3; i++) {
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
        if (i == 2) {   // the COM port (no Index: the first free number)
            RegStr(k, L"Prefix", L"COM");
            RegStr(k, L"Tsp", L"Unimodem.dll");
            RegDw(k, L"DeviceType", 1);
            RegStr(k, L"FriendlyName", kModemLine);
        }
        RegCloseKey(k);
    }
    s_modemState = 1;
    VLog(L"USB-модем: драйвер зарегистрирован (Huawei 12D1:155B, 12D1:1506) - вставьте модем");
#else
    s_modemState = 1;   // PC: the driver is the unit's only
    VLog(L"USB-модем: ПК - только отмечено");
#endif
}

// Internet through it: RAS over the driver's COM port (the APN is the driver's: by the SIM's operator); the status -
// the driver's HKLM\Software\Vesta\Modem (Port, Rssi, Mode, Sim)
#define WM_APP_MODEM (WM_APP + 29)
#ifdef UNDER_CE
static HRASCONN      s_ras;

static int ModemDw(HKEY k, const wchar_t* name)
{
    DWORD v, type, size = sizeof(v);
    return RegQueryValueEx(k, name, NULL, &type, (BYTE*)&v, &size) == ERROR_SUCCESS && type == REG_DWORD ? (int)v : -1;
}
#endif

static void ModemGet(ModemInfo* m)
{
    memset(m, 0, sizeof(*m));
    m->rssi = m->mode = m->sim = -1;
#ifdef UNDER_CE
    HKEY k;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"Software\\Vesta\\Modem", 0, 0, &k) == ERROR_SUCCESS) {
        DWORD type, size = sizeof(m->port) - sizeof(wchar_t);
        if (RegQueryValueEx(k, L"Port", NULL, &type, (BYTE*)m->port, &size) != ERROR_SUCCESS || type != REG_SZ)
            m->port[0] = 0;
        m->rssi = ModemDw(k, L"Rssi");
        m->mode = ModemDw(k, L"Mode");
        m->sim = ModemDw(k, L"Sim");
        RegCloseKey(k);
    }
    if (!s_ras)
        return;
    RASCONNSTATUS st;
    memset(&st, 0, sizeof(st));
    st.dwSize = sizeof(st);
    m->online = !RasGetConnectStatus(s_ras, &st) && st.rasconnstate == RASCS_Connected;
    if (!m->online) {   // the link went (the modem out, the network)
        VLog(L"модем: связь пропала (состояние %d)", (int)st.rasconnstate);
        RasHangUp(s_ras);
        s_ras = NULL;
    }
#endif
}

#ifdef UNDER_CE
// The chosen APN onto the modem (its COM port, free before RAS takes it): AT+CGDCONT - the dial's *99# uses it.
// By the SIM (netOp 0): the driver set its own already.
static void ModemApnSet(const wchar_t* port)
{
    char apn[64], cmd[96];
    WideCharToMultiByte(CP_ACP, 0, s_cfg.netApn, -1, apn, sizeof(apn), NULL, NULL);
    if (!s_cfg.netOp || !MdmApnCommand(apn, cmd, sizeof(cmd)))
        return;
    HANDLE h = CreateFile(port, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        VLog(L"модем: APN не задан - порт %s не открылся (%lu)", port, GetLastError());
        return;
    }
    COMMTIMEOUTS to = { 0, 0, 2000, 0, 1000 };
    SetCommTimeouts(h, &to);
    DWORD n = 0;
    char ans[64] = "";
    WriteFile(h, cmd, (DWORD)strlen(cmd), &n, NULL);
    ReadFile(h, ans, sizeof(ans) - 1, &n, NULL);
    ans[n < sizeof(ans) ? n : sizeof(ans) - 1] = 0;
    CloseHandle(h);
    VLog(L"модем: APN %S - %S", apn, strstr(ans, "OK") ? "OK" : "нет ответа OK");
}

static DWORD WINAPI ModemDial(LPVOID)
{
    RASENTRY e;
    memset(&e, 0, sizeof(e));
    e.dwSize = sizeof(e);
    e.dwfOptions = RASEO_RemoteDefaultGateway;   // the internet through it (Wi-Fi's route stays for its own net)
    e.dwfNetProtocols = RASNP_Ip;
    e.dwFramingProtocol = RASFP_Ppp;
    wcscpy(e.szLocalPhoneNumber, L"*99#");
    wcscpy(e.szDeviceType, RASDT_Modem);
    wcscpy(e.szDeviceName, kModemLine);
    DWORD r = RasSetEntryProperties(NULL, L"VestaModem", &e, sizeof(e), NULL, 0);
    HRASCONN c = NULL;
    if (!r) {
        RASDIALPARAMS p;
        memset(&p, 0, sizeof(p));
        p.dwSize = sizeof(p);
        wcscpy(p.szEntryName, L"VestaModem");
        wcsncpy(p.szUserName, s_cfg.netUser, UNLEN);
        wcsncpy(p.szPassword, s_cfg.netPass, PWLEN);
        r = RasDial(NULL, NULL, &p, 0, NULL, &c);   // (no notifier: it waits for the end)
    }
    if (r && c) {
        RasHangUp(c);
        c = NULL;
    }
    s_ras = c;
    s_rasErr = r;
    if (r)
        VLog(L"модем: дозвон *99# - ошибка RAS %lu", r);
    else
        VLog(L"модем: в сети");
    InterlockedExchange((LONG*)&s_rasBusy, 0);
    PostMessage(s_wnd, WM_APP_MODEM, 0, 0);
    return 0;
}
#endif

static bool s_netHeld;   // hung up by hand: not dialled by itself till the modem is out and in again

static void ModemNet()   // the diagnostics' button: the driver registered, then dial / hang up
{
    if (s_modemState <= 0) {
        ModemUsbSet();
        return;
    }
    if (s_rasBusy)
        return;
    ModemInfo m;
    ModemGet(&m);
#ifdef UNDER_CE
    if (s_ras) {
        RasHangUp(s_ras);
        s_ras = NULL;
        s_netHeld = true;
        VLog(L"модем: отключено");
        return;
    }
    if (!m.port[0]) {
        VLog(L"модем: нет COM-порта модема (вставлен? см. VestaModem.log)");
        return;
    }
    VLog(L"модем: дозвон через %s (сигнал %d, сеть %d, SIM %d)", m.port, m.rssi, m.mode, m.sim);
    s_netHeld = false;
    ModemApnSet(m.port);
    s_rasErr = 0;
    s_rasBusy = 1;
    HANDLE t = CreateThread(NULL, 0, ModemDial, NULL, 0, NULL);
    if (t)
        CloseHandle(t);
    else
        s_rasBusy = 0;
#else
    VLog(L"модем: ПК - дозвона нет");
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

// The port's speed. "0" = as the stock shell set it - but without the shell (VestaStub in its place) nobody sets
// it: COM4 / COM5 are copies of COM3 (VirtualSerial) whose speed is COM3's, left at its default, and no NMEA came
// (HU 2026-10-27: COM5 silent, data only after the search picked COM3 at 115200). Then the CAN module's 115200.
// The port read (the user 2026-10-27: VestaHome on COM4, CityGuide keeps its default COM5 - its default.ini:
// subset_COM_port=5 - from which GpsGate used to copy the NMEA to COM6). COM3, COM4, COM5 all mean "the car's GPS":
// without the stock shell COM4; with it COM5, as the shell reads COM4 itself (rn2::CanMessageParser). Never COM3: the
// copies (VirtualSerial) open the real COM3 alone - VestaHome there would leave the navigators on COM4 / COM5 without
// data (and "Найти GPS" saved COM3 on the unit). Other ports - as chosen.
static bool NoShell()
{
    return !FindWindow(NULL, L"RadioCommWindow");
}

static bool GpsCanPort(const wchar_t* p)
{
    return !wcsncmp(p, L"COM3", 4) || !wcsncmp(p, L"COM4", 4) || !wcsncmp(p, L"COM5", 4);
}

static const wchar_t* GpsPortNow()
{
    if (!GpsCanPort(s_cfg.gpsPort))
        return s_cfg.gpsPort;
    return NoShell() ? L"COM4:" : L"COM5:";
}

static DWORD GpsBaud()
{
    if (s_cfg.gpsBaud)
        return s_cfg.gpsBaud;
    return GpsCanPort(s_cfg.gpsPort) && NoShell() ? 115200 : 0;
}

static void GpsStartCore()
{
    InitializeCriticalSection(&s_gpsCs);
    GpsConfig c;
    memset(&c, 0, sizeof(c));
    c.notify = s_wnd;
    c.msg = WM_APP_GPS;
    c.port = GpsPortNow();
    c.baud = GpsBaud();
    c.app = L"VestaHome";
    c.log = GpsLogLine;
    c.exeDir = s_dir;
    c.onLine = GpsTapLine;
    c.onRaw = GpsTapRaw;
    GpsStart(&c);
}

static void GpsReopen()
{
    GpsConfigure(GpsPortNow(), GpsBaud());
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
        _snwprintf(s, n, brief ? L"не открывается" : L"%s не открывается", g.port[0] ? g.port : GpsPortNow());
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
    if (GpsCanPort(s_cfg.gpsPort))   // the car's GPS: the port actually read
        _snwprintf(s, n, L"%.4s · машина", GpsPortNow());
    else if (k)
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
    if (k >= 3 && k <= 5)   // COM3..COM5 are one stop: "the car's GPS" (saved as COM4)
        k = dir > 0 ? 6 : 2;
    else {
        k = k ? k + dir : dir > 0 ? 1 : 9;
        k = k < 1 ? 9 : k > 9 ? 1 : k;
        if (k >= 3 && k <= 5)
            k = 4;
    }
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

// ---------------------------------------------------------------- Settings -> Network

// "Network" (the user 2026-10-30): the USB modem - its state and the button, the operator's preset (by the SIM,
// Russia's and the CIS's, own), its APN / login / password typed, dialing by itself
static int NetOwn()   // the "own" operator's number (after the presets)
{
    int n = 0;
    MdmOps(&n);
    return n + 1;
}

static void NetOpName(int op, wchar_t* s, int cap)
{
    int n = 0;
    const MdmOp* ops = MdmOps(&n);
    if (op <= 0)
        wcsncpy(s, L"Авто (по SIM)", cap);
    else if (op <= n)
        wcsncpy(s, ops[op - 1].name, cap);
    else
        wcsncpy(s, L"Свой", cap);
    s[cap - 1] = 0;
}

static void NetOpStep(int d)   // the operator stepped: its preset's APN, login, password (by the SIM: none)
{
    int n = 0;
    const MdmOp* ops = MdmOps(&n);
    s_cfg.netOp = (s_cfg.netOp + d + n + 2) % (n + 2);
    if (s_cfg.netOp >= 1 && s_cfg.netOp <= n) {
        const MdmOp& o = ops[s_cfg.netOp - 1];
        _snwprintf(s_cfg.netApn, 64, L"%S", o.apn);
        _snwprintf(s_cfg.netUser, 32, L"%S", o.user);
        _snwprintf(s_cfg.netPass, 32, L"%S", o.pass);
    } else if (!s_cfg.netOp)
        s_cfg.netApn[0] = s_cfg.netUser[0] = s_cfg.netPass[0] = 0;
    SaveCfg();
}

static void ScrSetNet(HDC dc)
{
    Settings(dc, S_SET_NET);
    wchar_t st[64], op[40];
    ModemLabel(st, 64);   // (the diagnostics' button's text: its "USB-модем:" and "· подключить" off for the row)
    wchar_t* c = wcsstr(st, L": ");
    if (c)
        memmove(st, c + 2, (wcslen(c + 2) + 1) * sizeof(wchar_t));
    c = wcsrchr(st, L'·');
    if (c && (wcsstr(c, L"подключить") || wcsstr(c, L"отключить") || wcsstr(c, L"ещё раз")))
        c[c > st ? -1 : 0] = 0;
    ModemInfo m;
    ModemGet(&m);
    SetRow(dc, 64, L"USB-модем", st);
    Button(dc, 600, 70, 160, 44, s_rasBusy ? L"…" : m.online ? L"Отключить" : L"Подключить", m.online ? B_SEL : B_N,
           IC_NONE, 16, 0, s_rasBusy ? A_NONE : A_MODEMTEST);
    SetRow(dc, 120, L"Оператор");
    NetOpName(s_cfg.netOp, op, 40);
    Stepper(dc, 480, 124, 280, op, 48, 15, A_NETOP);
    bool sim = s_cfg.netOp == 0;
    SetRow(dc, 176, L"APN", sim ? L"по SIM-карте (нажмите, чтобы задать)" : s_cfg.netApn[0] ? s_cfg.netApn : L"—",
           false, A_KEYS, K_NETAPN);
    SetRow(dc, 232, L"Логин", s_cfg.netUser[0] ? s_cfg.netUser : L"—", false, A_KEYS, K_NETUSER);
    SetRow(dc, 288, L"Пароль", s_cfg.netPass[0] ? L"••••" : L"—", false, A_KEYS, K_NETPASS);
    SetRow(dc, 344, L"Автоподключение", L"при вставке модема и после обрыва", true);
    Toggle(dc, 688, 354, T_NET_AUTO);
}


// Dialing by itself (Settings -> Network: on by default): the driver registered at the start; the modem in, a SIM,
// not online - dial, again after a failure in 30 s, then doubling up to 5 minutes; hung up by hand - not till the
// modem is out and in again
static void NetAutoStep()   // every second
{
#ifdef UNDER_CE
    static DWORD s_next, s_wait = 30000;
    if (!s_cfg.netAuto || s_modemState < 0 || s_rasBusy)
        return;
    if (!s_modemState) {
        ModemUsbSet();
        return;
    }
    ModemInfo m;
    ModemGet(&m);
    if (!m.port[0] || m.online) {   // out (the next one in dials), or online
        s_netHeld = s_netHeld && m.port[0];
        s_wait = 30000;
        return;
    }
    if (s_netHeld || m.sim == 255 || (LONG)(GetTickCount() - s_next) < 0)
        return;
    s_next = GetTickCount() + s_wait;
    s_wait = s_wait < 150000 ? s_wait * 2 : 300000;
    VLog(L"модем: автоподключение");
    ModemNet();
#endif
}
