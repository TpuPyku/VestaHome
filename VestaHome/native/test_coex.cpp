// test_coex.cpp - coex.h on the PC: the Wi-Fi channel Bluetooth keeps off, and Wi-Fi restarted after Bluetooth's off.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "coex.h"

static int s_failed, s_checks;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAILED: %s\n", what);
    }
}

static void Net(WifiInfo* w, const wchar_t* ssid, int ch)
{
    wcscpy(w->nets[w->count].ssid, ssid);
    w->nets[w->count].ch = ch;
    w->count++;
}

int main()
{
    static WifiInfo w;
    memset(&w, 0, sizeof(w));
    Net(&w, L"Home", 11);
    Net(&w, L"Phone", 6);
    wcscpy(w.ssid, L"Phone");
    w.state = WIFI_CONNECTING;
    Check(CoexWifiChannel(&w) == 6, "joining: its network's channel");
    w.state = WIFI_CONNECTED;
    Check(CoexWifiChannel(&w) == 6, "joined: the same");
    w.state = WIFI_DISCONNECTED;
    Check(CoexWifiChannel(&w) == 0, "not joining: none");
    w.state = WIFI_CONNECTED;
    wcscpy(w.ssid, L"Gone");
    Check(CoexWifiChannel(&w) == 0, "its network not in the list: none");
    wcscpy(w.ssid, L"Phone");

    Coex c;
    CoexInit(&c);
    int ch = -1;
    Check(CoexStep(&c, true, false, &w, true, 1000, &ch) == CX_AFH && ch == 6, "BT on, Wi-Fi joined: the map sent");
    Check(CoexStep(&c, true, false, &w, true, 2000, &ch) == 0, "the same channel: nothing again");
    w.nets[1].ch = 1;
    Check(CoexStep(&c, true, false, &w, true, 3000, &ch) == CX_AFH && ch == 1, "the channel moved: sent again");
    w.state = WIFI_DISCONNECTED;
    Check(CoexStep(&c, true, false, &w, true, 4000, &ch) == CX_AFH && ch == 0, "Wi-Fi left: BT everywhere again");

    // Bluetooth off: going off, off; 10 s later Wi-Fi looked at
    Check(CoexStep(&c, false, false, &w, true, 5000, &ch) == 0, "going off: nothing");
    Check(CoexStep(&c, false, true, &w, true, 6000, &ch) == 0, "off: the wait begins");
    Check(CoexStep(&c, false, true, &w, true, 6000 + kCoexKickMs - 1, &ch) == 0, "not yet");
    Check(CoexStep(&c, false, true, &w, true, 6000 + kCoexKickMs, &ch) == CX_KICK, "10 s on, no link: restart");
    Check(CoexStep(&c, false, true, &w, true, 20000 + kCoexKickMs, &ch) == 0, "once only");

    // on again (the map sent again: the proxy's may be older), off with Wi-Fi joined - left alone
    w.state = WIFI_CONNECTED;
    Check(CoexStep(&c, true, false, &w, true, 30000, &ch) == CX_AFH && ch == 1, "BT on again: the map sent");
    CoexStep(&c, false, true, &w, true, 31000, &ch);
    Check(CoexStep(&c, false, true, &w, true, 31000 + kCoexKickMs, &ch) == CX_FINE, "joined: left alone");

    // off with Wi-Fi switched off by the user: left alone; joining counts as no link
    CoexStep(&c, true, false, &w, false, 40000, &ch);
    CoexStep(&c, false, true, &w, false, 41000, &ch);
    w.state = WIFI_OFF;
    Check(CoexStep(&c, false, true, &w, false, 41000 + kCoexKickMs, &ch) == CX_FINE, "Wi-Fi off by the user: alone");
    CoexStep(&c, true, false, &w, true, 50000, &ch);
    CoexStep(&c, false, true, &w, true, 51000, &ch);
    w.state = WIFI_CONNECTING;
    Check((CoexStep(&c, false, true, &w, true, 51000 + kCoexKickMs, &ch) & CX_KICK) != 0, "joining that long: restart");
    w.state = WIFI_AP;
    CoexStep(&c, true, false, &w, true, 60000, &ch);
    CoexStep(&c, false, true, &w, true, 61000, &ch);
    Check(CoexStep(&c, false, true, &w, true, 61000 + kCoexKickMs, &ch) == CX_FINE, "the unit's own access point: alone");

    // the first network of the list; a stale one past the count never read
    wcscpy(w.ssid, L"Home");
    w.state = WIFI_CONNECTED;
    Check(CoexWifiChannel(&w) == 11, "the list's first network");
    wcscpy(w.nets[2].ssid, L"Stale");
    w.nets[2].ch = 9;
    wcscpy(w.ssid, L"Stale");
    Check(CoexWifiChannel(&w) == 0, "past the list's count: not read");
    wcscpy(w.ssid, L"Phone");

    // a fresh start: no off made up, the first channel sent even when it is 1
    CoexInit(&c);
    Check(CoexStep(&c, true, false, &w, true, 20000, &ch) == CX_AFH && ch == 1, "the start: channel 1 sent, nothing else");
    Check(kCoexKickMs == 10000, "10 s after Bluetooth's off");
    Check((CX_AFH | CX_KICK | CX_FINE) == 7 && !(CX_AFH & CX_KICK) && !(CX_AFH & CX_FINE) && !(CX_KICK & CX_FINE),
          "the actions: three bits of their own");

    // the tick count wraps
    CoexInit(&c);
    CoexStep(&c, true, false, &w, true, 0xFFFFF000, &ch);
    w.state = WIFI_DISCONNECTED;
    CoexStep(&c, false, true, &w, true, 0xFFFFF000, &ch);
    Check(CoexStep(&c, false, true, &w, true, 0xFFFFF000 + kCoexKickMs, &ch) == CX_KICK, "over the wrap");
    printf("coex: %d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
