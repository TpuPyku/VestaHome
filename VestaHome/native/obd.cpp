// OBD-II through a Bluetooth ELM327 adapter (obd.h). BTCELegacy.dll's own IOCTLs (Ghidra 2026-10-20 on its
// COM_IOControl): 0x1B0324 connect as a client, 0x1B0328 disconnect, 0x1B032C its state (1 idle, 5-8 connecting /
// connected). The client connection takes the address from \Drivers\BT\XCBTSER1 "BtAddress" (xBtProt.dll's
// WRF_ConnectToASerialProfile reads it there).
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
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

// One ELM327 command: its answer up to the prompt '>', the line breaks as spaces. false: the port failed.
static bool Elm(HANDLE h, const char* cmd, int waitMs, char* out, int cap)
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
                c = ' ';
            if (c && len < cap - 1 && !(c == ' ' && (len == 0 || out[len - 1] == ' ')))
                out[len++] = c;
        }
    }
    while (len > 0 && out[len - 1] == ' ')
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

static DWORD WINAPI ObdThread(LPVOID)
{
    SetState(OBD_CONNECTING);
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
    DWORD t0 = GetTickCount(), got = 0;
    BOOL ok = DeviceIoControl(h, IOCTL_BT_CONNECT, NULL, 0, NULL, 0, &got, NULL);
    Say(L"подключение: %s за %lu мс, состояние %ld", ok ? L"есть" : L"нет", GetTickCount() - t0, (long)PortState(h));
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
    static const char* kPoll[4] = { "010C", "010D", "0105", "ATRV" };
    char last[4][64] = { "", "", "", "" };
    bool alive = true;
    while (!s_stop && alive) {
        for (int i = 0; i < 4 && !s_stop; i++) {
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
            else
                _snwprintf(s_info.volt, 12, L"%.10S", r[0] && r[0] != '?' ? r : "");
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
    s_info.rpm = s_info.speed = s_info.coolant = -1;
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
    s_info.rpm = s_info.speed = s_info.coolant = -1;
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
