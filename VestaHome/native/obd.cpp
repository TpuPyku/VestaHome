// OBD-II through a Bluetooth ELM327 adapter (obd.h). BTCELegacy.dll's own IOCTLs (Ghidra 2026-10-20 on its
// COM_IOControl): 0x1B0324 connect as a client, 0x1B0328 disconnect, 0x1B032C its state (1 idle, 5-8 connecting /
// connected). The client connection takes the address from \Drivers\BT\XCBTSER1 "BtAddress" (xBtProt.dll's
// WRF_ConnectToASerialProfile reads it there).
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include "obd.h"

#ifdef UNDER_CE
static const REGSAM kRegRead = 0, kRegAll = 0;
#else
static const REGSAM kRegRead = KEY_READ, kRegAll = KEY_ALL_ACCESS;
#endif

enum { IOCTL_BT_CONNECT = 0x1B0324, IOCTL_BT_DISCONNECT = 0x1B0328, IOCTL_BT_STATE = 0x1B032C };

static CRITICAL_SECTION s_cs;
static ObdInfo        s_info;            // published (under s_cs)
static HWND           s_notify;
static UINT           s_msg;
static ObdLogFn       s_log;
static HANDLE         s_thread;
static volatile bool  s_stop;
static char           s_devAddr[18];
static volatile int   s_dtcWant;         // 1 read the trouble codes, 2 clear them (the thread does it)
static ObdSppAddrFn   s_sppAddr;

static void Changed()
{
    if (s_notify)
        PostMessage(s_notify, s_msg, 0, 0);
}

// A step: into the screen's lines and the log
static void Say(const wchar_t* fmt, ...)
{
    wchar_t s[96];
    va_list a;
    va_start(a, fmt);
    _vsnwprintf(s, 96, fmt, a);
    va_end(a);
    s[95] = 0;
    EnterCriticalSection(&s_cs);
    memmove(s_info.lines[0], s_info.lines[1], sizeof(s_info.lines[0]) * (kObdLines - 1));
    wcscpy(s_info.lines[kObdLines - 1], s);
    LeaveCriticalSection(&s_cs);
    if (s_log)
        s_log(L"obd: %s", s);
    Changed();
}

static void SetState(int st)
{
    EnterCriticalSection(&s_cs);
    s_info.state = st;
    LeaveCriticalSection(&s_cs);
    Changed();
}

// The port of \Drivers\BT\XCBTSER2: its name from Drivers\Active, else the names it may have
static HANDLE OpenPort(wchar_t* used, int cap)
{
    wchar_t names[3][40];
    int n = 0;
    HKEY act;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"Drivers\\Active", 0, kRegRead, &act) == ERROR_SUCCESS) {
        for (DWORD i = 0; n == 0; i++) {
            wchar_t sub[32];
            DWORD len = 32;
            if (RegEnumKeyEx(act, i, sub, &len, NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
                break;
            HKEY k;
            if (RegOpenKeyEx(act, sub, 0, kRegRead, &k) != ERROR_SUCCESS)
                continue;
            wchar_t key[128], name[40];
            DWORD t, cb = sizeof(key) - sizeof(wchar_t);
            memset(key, 0, sizeof(key));
            if (RegQueryValueEx(k, L"Key", NULL, &t, (BYTE*)key, &cb) == ERROR_SUCCESS && wcsstr(key, L"XCBTSER2")) {
                memset(name, 0, sizeof(name));
                cb = sizeof(name) - sizeof(wchar_t);
                if (RegQueryValueEx(k, L"Name", NULL, &t, (BYTE*)name, &cb) == ERROR_SUCCESS && name[0])
                    wcscpy(names[n++], name);
            }
            RegCloseKey(k);
        }
        RegCloseKey(act);
    }
    wcscpy(names[n++], L"\\$device\\COM12");
    wcscpy(names[n++], L"COM12:");
    for (int i = 0; i < n; i++) {
        HANDLE h = CreateFile(names[i], GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (h != INVALID_HANDLE_VALUE) {
            wcsncpy(used, names[i], cap - 1);
            used[cap - 1] = 0;
            return h;
        }
        Say(L"порт %s — ошибка %lu", names[i], GetLastError());
    }
    return INVALID_HANDLE_VALUE;
}

static DWORD PortState(HANDLE h)
{
    DWORD st = 0, got = 0;
    if (!DeviceIoControl(h, IOCTL_BT_STATE, NULL, 0, &st, sizeof(st), &got, NULL))
        return 0xFFFFFFFF;
    return st;
}

// One ELM327 command: its answer up to the prompt '>', the line breaks as spaces (or '\n' with lines). false: the
// port failed.
static bool Elm(HANDLE h, const char* cmd, int waitMs, char* out, int cap, bool lines = false)
{
    char line[32];
    int n = _snprintf(line, sizeof(line) - 1, "%s\r", cmd);
    DWORD w = 0;
    out[0] = 0;
    if (!WriteFile(h, line, n, &w, NULL) || (int)w != n)
        return false;
    int len = 0;
    DWORD t0 = GetTickCount();
    bool prompt = false;
    while (!prompt && !s_stop && GetTickCount() - t0 < (DWORD)waitMs) {
        char b[64];
        DWORD got = 0;
        if (!ReadFile(h, b, sizeof(b), &got, NULL))
            return false;
        for (DWORD i = 0; i < got; i++) {
            char c = b[i];
            if (c == '>') {
                prompt = true;
                break;
            }
            if (c == '\r' || c == '\n')
                c = lines ? '\n' : ' ';
            bool blank = c == ' ' || c == '\n';
            if (c && len < cap - 1 && !(blank && (len == 0 || out[len - 1] == ' ' || out[len - 1] == '\n')))
                out[len++] = c;
        }
    }
    while (len > 0 && (out[len - 1] == ' ' || out[len - 1] == '\n'))
        len--;
    out[len] = 0;
    return true;
}

// "41 0C 0B B8" -> the data bytes after "410C" (spaces ignored); the count found
static int PidBytes(const char* r, const char* want, int* v, int maxN)
{
    char s[128];
    int n = 0;
    for (int i = 0; r[i] && n < 127; i++)
        if (r[i] != ' ')
            s[n++] = r[i];
    s[n] = 0;
    const char* p = strstr(s, want);
    if (!p)
        return 0;
    p += strlen(want);
    int k = 0;
    while (k < maxN && p[0] && p[1]) {
        int x = 0;
        if (sscanf(p, "%2x", &x) != 1)
            break;
        v[k++] = x;
        p += 2;
    }
    return k;
}

// Each command's answer into the log only when it is new
static void Answer(const char* cmd, const char* r, char* last, int cap)
{
    if (!strcmp(last, r))
        return;
    strncpy(last, r, cap - 1);
    last[cap - 1] = 0;
    Say(L"%S → %S", cmd, r[0] ? r : "(нет ответа)");
}

int ObdParseDtc(const char* r, char (*out)[6], int maxN)
{
    int n = 0;
    bool any = false;   // a "43" frame seen
    unsigned char fr[64];
    int frN = 0;
    bool frCan = false;   // a long CAN answer's frame ("0:"): its count after 43 whatever its length
    for (const char* line = r; ; ) {   // each line: its bytes; a "43" starts a frame, "1:".."F:" go on with it
        const char* end = strchr(line, '\n');
        int len = end ? (int)(end - line) : (int)strlen(line);
        char s[160];
        if (len > 159)
            len = 159;
        memcpy(s, line, len);
        s[len] = 0;
        const char* p = s;
        while (*p == ' ')
            p++;
        int part = -1;   // "0:".."F:": a long CAN answer's part
        if (isxdigit((unsigned char)p[0]) && p[1] == ':') {
            part = isdigit((unsigned char)p[0]) ? p[0] - '0' : toupper(p[0]) - 'A' + 10;
            p += 2;
        }
        unsigned char b[64];
        int bn = 0;
        bool hex = true;
        while (*p && hex) {
            while (*p == ' ')
                p++;
            if (!*p)
                break;
            int x;
            if (isxdigit((unsigned char)p[0]) && isxdigit((unsigned char)p[1]) && (!p[2] || p[2] == ' ')
                && sscanf(p, "%2x", &x) == 1 && bn < 64) {
                b[bn++] = (unsigned char)x;
                p += 2;
            } else {
                hex = false;   // "SEARCHING...", "NO DATA", the length line "00A"
            }
        }
        bool starts = hex && bn && b[0] == 0x43 && part <= 0;
        if ((starts || (!end && !len)) && frN) {   // the frame before it ends (or the text): its codes
            int k = 1, count = -1;
            if (frCan || (frN - 1) % 2) {   // CAN: the count after 43
                count = fr[1];
                k = 2;
            }
            for (; k + 1 < frN && n < maxN && count != 0; k += 2) {
                int a = fr[k], c = fr[k + 1];
                if (!a && !c)
                    continue;
                char code[6];
                _snprintf(code, 6, "%c%d%X%X%X", "PCBU"[a >> 6], (a >> 4) & 3, a & 15, c >> 4, c & 15);
                code[5] = 0;
                bool dup = false;
                for (int i = 0; i < n; i++)
                    dup = dup || !strcmp(out[i], code);
                if (!dup)
                    strcpy(out[n++], code);
                if (count > 0)
                    count--;
            }
            frN = 0;
        }
        if (starts) {
            any = true;
            memcpy(fr, b, bn);
            frN = bn;
            frCan = part == 0;
        } else if (hex && part > 0 && frN) {
            for (int i = 0; i < bn && frN < 64; i++)
                fr[frN++] = b[i];
        }
        if (!end) {
            if (frN) {   // the last line was a frame's: once more through the end above
                line = "";
                continue;
            }
            break;
        }
        line = end + 1;
    }
    if (any)
        return n;
    return strstr(r, "NO DATA") ? 0 : -1;   // no codes stored: some cars answer so
}

// The trouble codes asked for (s_dtcWant): read (03) or clear (04, then read). Under the poll's thread.
static void DtcDo(HANDLE h)
{
    int want = s_dtcWant;
    s_dtcWant = 0;
    char r[400];
    if (want == 2) {
        bool ok = Elm(h, "04", 8000, r, sizeof(r)) && strstr(r, "44");
        Say(L"04 (сброс ошибок) → %S", r[0] ? r : "(нет ответа)");
        if (!ok) {
            EnterCriticalSection(&s_cs);
            s_info.dtcState = OBD_DTC_FAILED;
            LeaveCriticalSection(&s_cs);
            return;
        }
    }
    char codes[kObdDtcMax][6];
    int n = Elm(h, "03", 8000, r, sizeof(r), true) ? ObdParseDtc(r, codes, kObdDtcMax) : -1;
    for (char* c = r; *c; c++)
        if (*c == '\n')
            *c = '|';
    Say(L"03 (ошибки) → %.80S", r[0] ? r : "(нет ответа)");
    EnterCriticalSection(&s_cs);
    s_info.dtcState = n < 0 ? OBD_DTC_FAILED : OBD_DTC_READ;
    s_info.dtcN = n < 0 ? 0 : n;
    memcpy(s_info.dtc, codes, sizeof(codes));
    LeaveCriticalSection(&s_cs);
}

// How far the stack got (Ghidra 2026-10-26, BTCELegacy.dll): it posts thread messages to the thread id in
// HKLM\Software\Microsoft\Bluetooth\hci "ThreadinId" - 200 when the RFCOMM channel opens (and writes "SPPBtAddress"
// there), 201 on any close. A failure in under ~4.9 s comes after the link is up (the adapter's SPP service not found by
// the stack's 3-UUID search, or RFCOMM refused); 5 s - the adapter did not answer at all (no power, the phone holds it).
static const wchar_t* kHciKey = L"Software\\Microsoft\\Bluetooth\\hci";

static bool HciThreadSet(DWORD id, DWORD* was, bool* had)   // ours in, the previous one kept (restore: had / was)
{
    HKEY k;
    DWORD disp;
    if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, kHciKey, 0, NULL, 0, kRegAll, NULL, &k, &disp) != ERROR_SUCCESS)
        return false;
    if (had) {
        DWORD sz = sizeof(*was), type;
        *had = RegQueryValueEx(k, L"ThreadinId", NULL, &type, (BYTE*)was, &sz) == ERROR_SUCCESS;
    }
    LONG r = RegSetValueEx(k, L"ThreadinId", 0, REG_DWORD, (const BYTE*)&id, sizeof(id));
    RegCloseKey(k);
    return r == ERROR_SUCCESS;
}

static void HciThreadRestore(DWORD was, bool had)
{
    HKEY k;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, kHciKey, 0, 0, &k) != ERROR_SUCCESS)
        return;
    if (had)
        RegSetValueEx(k, L"ThreadinId", 0, REG_DWORD, (const BYTE*)&was, sizeof(was));
    else
        RegDeleteValue(k, L"ThreadinId");
    RegCloseKey(k);
}

static void HciEvents(DWORD t0, DWORD wait)   // the stack's 200 / 201 that came, with their times
{
    DWORD until = GetTickCount() + wait;
    for (;;) {
        MSG m;
        while (PeekMessage(&m, NULL, 0, 0, PM_REMOVE)) {
            if (m.message == 200)
                Say(L"  стек: канал RFCOMM открыт (+%lu мс)", GetTickCount() - t0);
            else if (m.message == 201)
                Say(L"  стек: связь закрыта (+%lu мс)", GetTickCount() - t0);
        }
        if ((LONG)(GetTickCount() - until) >= 0)
            break;
        Sleep(50);
    }
}

static DWORD WINAPI ObdThread(LPVOID)
{
    SetState(OBD_CONNECTING);
    if (s_sppAddr)
        Say(L"адрес через SDK оболочки (Spp_SetAddr): %d", s_sppAddr(s_devAddr));
    HKEY k;
    DWORD disp;
    LONG rr = RegCreateKeyEx(HKEY_LOCAL_MACHINE, L"Drivers\\BT\\XCBTSER1", 0, NULL, 0, kRegAll, NULL, &k, &disp);
    if (rr == ERROR_SUCCESS) {
        wchar_t w[18];
        MultiByteToWideChar(CP_ACP, 0, s_devAddr, -1, w, 18);
        w[17] = 0;
        rr = RegSetValueEx(k, L"BtAddress", 0, REG_SZ, (BYTE*)w, (wcslen(w) + 1) * sizeof(wchar_t));
        RegCloseKey(k);
    }
    Say(L"адрес %S в XCBTSER1: %s", s_devAddr, rr == ERROR_SUCCESS ? L"записан" : L"ошибка");
    wchar_t port[40];
    HANDLE h = OpenPort(port, 40);
    if (h == INVALID_HANDLE_VALUE) {
        Say(L"порт Bluetooth не открылся (Bluetooth оболочки включён?)");
        SetState(OBD_FAILED);
        return 0;
    }
    COMMTIMEOUTS to = { MAXDWORD, 0, 300, 0, 2000 };
    BOOL tok = SetCommTimeouts(h, &to);
    Say(L"порт %s открыт, состояние %ld%s", port, (long)PortState(h), tok ? L"" : L", без таймаутов");
    // The stack: up to 5 s for its device chooser, then the address of XCBTSER1, then the link (an adapter holds
    // one link: a phone still connected to it refuses ours). Three tries, each with its time class and the stack's
    // own events into the log ("error 0" is normal: its COM_IOControl does not set the last error).
    MSG qm;
    PeekMessage(&qm, NULL, 0, 0, PM_NOREMOVE);   // (this thread's queue: the stack posts to it)
    DWORD hciWas = 0;
    bool hciHad = false, hciOurs = HciThreadSet(GetCurrentThreadId(), &hciWas, &hciHad);
    DWORD got = 0;
    BOOL ok = FALSE;
    for (int t = 1; t <= 3 && !ok && !s_stop; t++) {
        DWORD t0 = GetTickCount();
        ok = DeviceIoControl(h, IOCTL_BT_CONNECT, NULL, 0, NULL, 0, &got, NULL);
        DWORD err = ok ? 0 : GetLastError(), ms = GetTickCount() - t0;
        Say(L"подключение %d: %s за %lu мс (%s), ошибка %lu, состояние %ld", t, ok ? L"есть" : L"нет", ms,
            ok ? L"связь" : ms >= 4990 ? L"адаптер не ответил: питание, зажигание, занят телефоном"
                          : L"отказ после связи: служба SPP адаптера или канал RFCOMM",
            err, (long)PortState(h));
        if (hciOurs)
            HciEvents(t0, ok ? 0 : 1000);
        if (!ok && t < 3)
            Sleep(2000);
    }
    if (hciOurs) {
        wchar_t a[40] = L"";
        HKEY k;
        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, kHciKey, 0, 0, &k) == ERROR_SUCCESS) {
            DWORD sz = sizeof(a), type;
            RegQueryValueEx(k, L"SPPBtAddress", NULL, &type, (BYTE*)a, &sz);
            RegCloseKey(k);
        }
        a[39] = 0;
        Say(L"  стек: SPPBtAddress «%s»; ThreadinId был %s%lu", a, hciHad ? L"" : L"не задан, ", hciWas);
        HciThreadRestore(hciWas, hciHad);
    }
    if (!ok || s_stop) {
        CloseHandle(h);
        SetState(s_stop ? OBD_IDLE : OBD_FAILED);
        return 0;
    }
    static const char* kInit[6] = { "ATZ", "ATE0", "ATL0", "ATI", "ATSP0", "0100" };
    char r[160];
    for (int i = 0; i < 6 && !s_stop; i++) {
        if (!Elm(h, kInit[i], i == 5 ? 12000 : 4000, r, sizeof(r))) {
            Say(L"%S: порт отказал (%lu)", kInit[i], GetLastError());
            break;
        }
        Say(L"%S → %S", kInit[i], r[0] ? r : "(нет ответа)");
        if (i == 0 && !r[0])
            Elm(h, "ATZ", 4000, r, sizeof(r));   // the first command may only wake it
    }
    SetState(OBD_ON);
    // the engine (2026-10-22: the car widget's lines): the PIDs most cars answer; one a car does not - "NO DATA", -1
    enum { kPolls = 11 };
    static const char* kPoll[kPolls] = { "010C", "010D", "0105", "ATRV", "0104", "0111", "010F", "0146", "012F",
                                         "0110", "0101" };
    char last[kPolls][64];
    memset(last, 0, sizeof(last));
    bool alive = true;
    while (!s_stop && alive) {
        if (s_dtcWant) {
            DtcDo(h);
            Changed();
        }
        for (int i = 0; i < kPolls && !s_stop; i++) {
            if (!Elm(h, kPoll[i], 3000, r, sizeof(r))) {
                Say(L"связь потеряна (%lu)", GetLastError());
                alive = false;
                break;
            }
            Answer(kPoll[i], r, last[i], 64);
            int v[2] = { 0, 0 };
            EnterCriticalSection(&s_cs);
            if (i == 0)
                s_info.rpm = PidBytes(r, "410C", v, 2) == 2 ? (v[0] * 256 + v[1]) / 4 : -1;
            else if (i == 1)
                s_info.speed = PidBytes(r, "410D", v, 1) == 1 ? v[0] : -1;
            else if (i == 2)
                s_info.coolant = PidBytes(r, "4105", v, 1) == 1 ? v[0] - 40 : -1;
            else if (i == 3)
                _snwprintf(s_info.volt, 12, L"%.10S", r[0] && r[0] != '?' ? r : "");
            else if (i == 4)
                s_info.load = PidBytes(r, "4104", v, 1) == 1 ? v[0] * 100 / 255 : -1;
            else if (i == 5)
                s_info.throttle = PidBytes(r, "4111", v, 1) == 1 ? v[0] * 100 / 255 : -1;
            else if (i == 6)
                s_info.intake = PidBytes(r, "410F", v, 1) == 1 ? v[0] - 40 : -1000;
            else if (i == 7)
                s_info.ambient = PidBytes(r, "4146", v, 1) == 1 ? v[0] - 40 : -1000;
            else if (i == 8)
                s_info.fuel = PidBytes(r, "412F", v, 1) == 1 ? v[0] * 100 / 255 : -1;
            else if (i == 9)
                s_info.maf100 = PidBytes(r, "4110", v, 2) == 2 ? v[0] * 256 + v[1] : -1;
            else if (PidBytes(r, "4101", v, 1) == 1) {   // A: bit 7 the lamp, 0..6 the codes stored
                s_info.mil = v[0] >> 7;
                s_info.dtcs = v[0] & 0x7F;
            } else {
                s_info.mil = s_info.dtcs = -1;
            }
            s_info.volt[11] = 0;
            LeaveCriticalSection(&s_cs);
        }
        Changed();
        Sleep(500);
    }
    DeviceIoControl(h, IOCTL_BT_DISCONNECT, NULL, 0, NULL, 0, &got, NULL);
    CloseHandle(h);
    Say(L"отключено");
    SetState(alive ? OBD_IDLE : OBD_FAILED);
    return 0;
}

void ObdInit(HWND notify, UINT msg, ObdLogFn log)
{
    InitializeCriticalSection(&s_cs);
    s_notify = notify;
    s_msg = msg;
    s_log = log;
    s_info.rpm = s_info.speed = s_info.coolant = s_info.load = s_info.throttle = s_info.fuel = s_info.maf100 = -1;
    s_info.mil = s_info.dtcs = -1;
    s_info.intake = s_info.ambient = -1000;
}

static void DtcAsk(int want)
{
    EnterCriticalSection(&s_cs);
    bool on = s_info.state == OBD_ON;
    s_info.dtcState = !on ? OBD_DTC_FAILED : want == 2 ? OBD_DTC_CLEARING : OBD_DTC_READING;
    LeaveCriticalSection(&s_cs);
    if (on)
        s_dtcWant = want;
    Changed();
}

void ObdReadDtc()
{
    DtcAsk(1);
}

void ObdClearDtc()
{
    DtcAsk(2);
}

void ObdSetSppAddrFn(ObdSppAddrFn fn)
{
    s_sppAddr = fn;
}

void ObdStop()
{
    if (!s_thread)
        return;
    s_stop = true;
    if (WaitForSingleObject(s_thread, 4000) == WAIT_OBJECT_0) {
        CloseHandle(s_thread);
        s_thread = NULL;
    }
}

void ObdConnect(const char* addr)
{
    ObdStop();
    if (s_thread)   // still stuck in the stack: leave it
        return;
    strncpy(s_devAddr, addr, 17);
    s_devAddr[17] = 0;
    EnterCriticalSection(&s_cs);
    s_info.rpm = s_info.speed = s_info.coolant = s_info.load = s_info.throttle = s_info.fuel = s_info.maf100 = -1;
    s_info.mil = s_info.dtcs = -1;
    s_info.intake = s_info.ambient = -1000;
    s_info.volt[0] = 0;
    LeaveCriticalSection(&s_cs);
    s_stop = false;
    s_thread = CreateThread(NULL, 0, ObdThread, NULL, 0, NULL);
}

void ObdGet(ObdInfo* out)
{
    EnterCriticalSection(&s_cs);
    *out = s_info;
    LeaveCriticalSection(&s_cs);
}
