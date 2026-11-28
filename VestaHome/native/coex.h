// coex.h - Wi-Fi and Bluetooth side by side (the users 2026-11-28: Wi-Fi does not work with Bluetooth on and back;
// with Bluetooth off Wi-Fi works only after a reboot). The unit's two CSR chips share 2.4 GHz and nothing in the
// firmware tells one where the other is (PROJECT.md "Wi-Fi и BT мешают друг другу"). VestaHome's part, once a second:
// - the channel of the network being joined or joined goes to the BT proxy (BT_CMD_AFH): Bluetooth keeps off it;
// - Bluetooth switched off: kCoexKickMs later a Wi-Fi wanted on but still without a link is restarted.
#ifndef COEX_H
#define COEX_H

#include "wifi.h"

enum { kCoexKickMs = 10000 };
enum { CX_AFH = 1,     // send *ch to the proxy
       CX_KICK = 2,    // restart Wi-Fi (off, on)
       CX_FINE = 4 };  // Bluetooth's off looked at: Wi-Fi left alone (for the log)

// The Wi-Fi channel Bluetooth should keep off: the network being joined or joined; 0 - none
inline int CoexWifiChannel(const WifiInfo* w)
{
    if (w->state != WIFI_CONNECTING && w->state != WIFI_CONNECTED)
        return 0;
    for (int i = 0; i < w->count; i++)
        if (!wcscmp(w->nets[i].ssid, w->ssid))
            return w->nets[i].ch;
    return 0;
}

struct Coex {
    int   afhSent;   // the channel last sent, -1 - none since Bluetooth came on
    bool  btWasOn;   // Bluetooth seen on since the last off
    bool  kick;      // Bluetooth's off to look at
    DWORD offTick;
};

inline void CoexInit(Coex* c)
{
    c->afhSent = -1;
    c->btWasOn = c->kick = false;
    c->offTick = 0;
}

// Bluetooth's off kCoexKickMs ago: a Wi-Fi wanted on without a link restarted, else left alone
inline int CoexKickStep(Coex* c, const WifiInfo* w, bool wifiWanted, DWORD now)
{
    if (!c->kick || now - c->offTick < (DWORD)kCoexKickMs)
        return 0;
    c->kick = false;
    bool noLink = w->state == WIFI_DISCONNECTED || w->state == WIFI_CONNECTING;
    return wifiWanted && noLink ? CX_KICK : CX_FINE;
}

// One step: btOn / btOff - the proxy's power state (both false while it goes on or off); wifiWanted - the user's
// switch; *ch - the channel for CX_AFH. CX_* to do.
inline int CoexStep(Coex* c, bool btOn, bool btOff, const WifiInfo* w, bool wifiWanted, DWORD now, int* ch)
{
    int act = 0;
    *ch = CoexWifiChannel(w);
    if (btOn) {
        c->btWasOn = true;
        if (*ch != c->afhSent) {
            c->afhSent = *ch;
            act = CX_AFH;
        }
    } else {
        c->afhSent = -1;   // (on again: sent anew)
        if (btOff && c->btWasOn) {
            c->btWasOn = false;
            c->kick = true;
            c->offTick = now;
        }
    }
    return act | CoexKickStep(c, w, wifiWanted, now);
}

#endif
