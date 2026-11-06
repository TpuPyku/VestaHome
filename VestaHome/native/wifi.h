// Wi-Fi of the unit through Windows CE's Wireless Zero Configuration (wzcsapi.dll, loaded at run time; the
// exe still imports COREDLL + WS2 only) and the address through iphlpapi.dll. The work is done in a thread of
// its own (WZC answers through RPC); the window learns of changes by a posted message. The unit's registry is
// reset on every boot, and WZC's preferred networks with it: the known networks come from VestaHome's settings
// (WifiRemember) and are given to WZC again whenever the adapter is not connected.
#ifndef VESTA_WIFI_H
#define VESTA_WIFI_H

// WIFI_AP: the unit is an access point (its own network for a phone, see WifiApSetup)
enum { WIFI_NOWZC, WIFI_NOADAPTER, WIFI_OFF, WIFI_AP, WIFI_DISCONNECTED, WIFI_CONNECTING, WIFI_CONNECTED };
enum { WSEC_OPEN, WSEC_WEP, WSEC_WPA, WSEC_WPA2 };
enum { kWifiNets = 24, kWifiKnown = 16 };

struct WifiNet {
    wchar_t ssid[33];
    int     rssi;      // dBm
    int     sec;       // WSEC_*
    bool    known;     // its password is kept
    int     ch;        // its channel (of the strongest access point), 0 = unknown; the unit's radio: 2.4 GHz only
};

struct WifiInfo {
    int     state;     // WIFI_*
    wchar_t adapter[48];
    wchar_t ssid[33];  // connected to (or connecting to)
    int     rssi;      // of the connected network, 0 = unknown
    char    ip[16];    // "" = no address yet
    bool    scanning;
    int     count;     // the networks seen by the last scan, strongest first
    WifiNet nets[kWifiNets];
    wchar_t msg[96];   // the last action's result or error
    int     apClients; // WIFI_AP: the stations connected to it
    bool    apAuto;    // WIFI_AP: raised by the automatic hand-over (not by hand)
    wchar_t apSsid[33], apPass[65];   // the access point's network (WifiApSetup)
};

typedef void (*WifiLogFn)(const wchar_t* fmt, ...);

void WifiStart(HWND notify, UINT msg, WifiLogFn log);   // msg is posted when WifiInfo changed
void WifiStop();
void WifiGet(WifiInfo* out);
void WifiScan();
// A network to connect to (pass: "" for an open one); also kept for the next boots by the caller.
void WifiConnect(const wchar_t* ssid, const wchar_t* pass);
void WifiForget(const wchar_t* ssid);
void WifiRemember(const wchar_t* ssid, const wchar_t* pass);   // the known networks, from the settings
// The radio on / off (the settings' switch; also before WifiStart).
void WifiEnable(bool on);
// The radio off for 30 s and on again, on the Wi-Fi thread: what is left of a full power cut from a program.
void WifiRestart();
void WifiQuiet(bool on);   // no networks handed to WZC meanwhile (another internet is on: its link left alone)
// The unit's own access point (2.4 GHz, WPA2-AES), for a phone whose hotspot is on 5 GHz: a phone connected to a
// 2.4 GHz network puts its hotspot on 2.4 GHz and keeps it there. autoOn: with no known network in sight the unit
// raises it, waits for a phone, then goes back to a client and joins the phone's hotspot (a known network).
void WifiApSetup(const wchar_t* ssid, const wchar_t* pass, bool autoOn);
void WifiApManual(bool on);   // by hand: the access point until switched off
// 0..4 bars from dBm
int  WifiBars(int rssi);
// An adapter's IPv4 address by its name (e.g. "BTCENDIS1", Bluetooth PAN); "" none yet (ip: 16 chars)
void AdapterIp(const char* name, char* ip);

#endif
