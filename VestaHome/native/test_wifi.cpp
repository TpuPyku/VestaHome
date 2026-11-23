// test_wifi.cpp - wifi.cpp's two threads on the PC (its imitation of the networks): the start, the switch served by its
// own thread (off at once, on again after the 30 s off), the poll's state back. About 35 s.
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include "wifi.h"

static int s_failed, s_checks;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAIL: %s\n", what);
    }
}

static CRITICAL_SECTION s_lock;
static wchar_t s_lines[4096];

static void TestLog(const wchar_t* fmt, ...)
{
    wchar_t s[400];
    va_list a;
    va_start(a, fmt);
    _vsnwprintf(s, 399, fmt, a);
    va_end(a);
    s[399] = 0;
    EnterCriticalSection(&s_lock);
    if (wcslen(s_lines) + wcslen(s) + 2 < 4096) {
        wcscat(s_lines, s);
        wcscat(s_lines, L"\n");
    }
    LeaveCriticalSection(&s_lock);
}

static bool Logged(const wchar_t* part)
{
    EnterCriticalSection(&s_lock);
    bool k = wcsstr(s_lines, part) != NULL;
    LeaveCriticalSection(&s_lock);
    return k;
}

// The published state becomes `state` (-1: any) with a message starting with `msg` (NULL: any) within ms
static bool Until(int state, const wchar_t* msg, DWORD ms, WifiInfo* w)
{
    DWORD t0 = GetTickCount();
    do {
        WifiGet(w);
        if ((state < 0 || w->state == state) && (!msg || !wcsncmp(w->msg, msg, wcslen(msg))))
            return true;
        Sleep(50);
    } while (GetTickCount() - t0 < ms);
    return false;
}

static bool Reaches(int state, DWORD ms, WifiInfo* w)
{
    return Until(state, NULL, ms, w);
}

int main()
{
    InitializeCriticalSection(&s_lock);
    WifiInfo w;
    WifiEnable(true);
    WifiStart(NULL, 0, TestLog);
    WifiGet(&w);
    Check(w.state == WIFI_DISCONNECTED || !wcscmp(w.msg, L"запуск Wi-Fi…"), "the start: \"запуск Wi-Fi…\", not \"нет службы\"");
    Check(Reaches(WIFI_DISCONNECTED, 3000, &w) && w.count == 3, "the imitation's 3 networks in sight");
    WifiEnable(false);
    Check(Reaches(WIFI_OFF, 1000, &w), "switched off: off within a second");
    WifiEnable(true);
    Check(Until(WIFI_OFF, L"включение Wi-Fi:", 2000, &w), "on again at once: the 30 s shown");
    Check(!Reaches(WIFI_DISCONNECTED, 25000, &w), "not on before the 30 s");
    Check(Reaches(WIFI_DISCONNECTED, 8000, &w) && w.count == 3, "on after the 30 s, the networks back");
    WifiConnect(L"AndroidAP", L"12345678");
    Check(Reaches(WIFI_CONNECTED, 4000, &w) && !wcscmp(w.ssid, L"AndroidAP"), "connected");
    WifiWatch();
    Check(!Logged(L"стоит"), "the watch: no stall");
    Check(Logged(L"поток выключателя Wi-Fi"), "the switch's thread started");
    WifiStop();
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
