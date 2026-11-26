// Wi-Fi through Windows CE's Wireless Zero Configuration (wzcsapi.dll): the structures of the CE 6.0 SDK's
// wzcsapi.h (Microsoft's CE 6.0 reference: INTF_ENTRY, WZC_WLAN_CONFIG, WZC_EAPOL_PARAMS, the INTF_* flags),
// declared here because the SDK of this build (Pocket PC 2003) has no wzcsapi.h. The unit's radio is a CSR6031
// (UniFi, 2.4 GHz 802.11b/g/n, WPA/WPA2 TKIP/AES) behind the Neesus Datacom driver ndcufmp.dll, dual: a client
// and an access point adapter; the client side is run by WZC as on any CE device.
// WZC keeps the key material of its configurations XOR-ed with a fixed table (wzcsvc.dll of firmware 4120 holds
// that table at 0x11180): a configuration given to it is "encrypted" the same way.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "wifi.h"
#include "wifiie.h"
#include "radiowait.h"
#include "wifiboot.h"

#ifdef UNDER_CE
#define PROC(m, n) GetProcAddress(m, L##n)
#else
#define PROC(m, n) GetProcAddress(m, n)
#endif

struct RawData { DWORD len; BYTE* data; };
struct IntfKey { LPWSTR guid; };
struct IntfTable { DWORD count; IntfKey* intfs; };
struct IntfEntry {
    LPWSTR  guid;
    LPWSTR  descr;
    ULONG   mediaState;          // 1 connected, 0 disconnected
    ULONG   mediaType, physicalMediaType;
    INT     infraMode, authMode, wepStatus;
    DWORD   ctlFlags, capabilities;
    RawData ssid;                // the SSID's bytes (len = their number; not an NDIS_802_11_SSID)
    RawData bssid;
    RawData bssidList;           // WZC_802_11_CONFIG_LIST: the visible networks
    RawData prefList;            // WZC_802_11_CONFIG_LIST: the preferred networks
    RawData ctrlData;
    BOOL    initialized;
    DWORD   room[16];            // whatever a newer service may write after them
};
struct Ssid { ULONG len; UCHAR ssid[32]; };
static volatile bool s_quiet;   // WifiQuiet

struct WlanConfig {              // WZC_WLAN_CONFIG (CE 6.0)
    ULONG   length;
    DWORD   ctlFlags;
    UCHAR   mac[6];
    UCHAR   reserved[2];
    Ssid    ssid;
    ULONG   privacy;             // NDIS_802_11_WEP_STATUS
    LONG    rssi;
    ULONG   networkType;
    ULONG   configuration[8];    // NDIS_802_11_CONFIGURATION
    ULONG   infraMode;
    UCHAR   rates[8];
    ULONG   keyIndex, keyLength;
    UCHAR   keyMaterial[32];     // WZCCTL_MAX_WEPK_MATERIAL
    ULONG   authMode;            // NDIS_802_11_AUTHENTICATION_MODE
    RawData userData;
    BOOL    enable8021x;         // WZC_EAPOL_PARAMS
    DWORD   eapFlags, eapType, authDataLen;
    BYTE*   authData;
    RawData networkData;
    ULONG   wpaMCastCipher;
    ULONG   mediaType;
};
typedef char kWlanConfigSize[sizeof(WlanConfig) == 196 ? 1 : -1];
struct ConfigList { ULONG count; ULONG index; };   // + count WlanConfig

enum { INTF_ALL = 0xFFFFFFFF, INTF_PREFLIST = 0x00040000, INTF_SSID = 0x01000000, INTF_BSSIDLIST = 0x04000000,
       WZCCTL_WEPK_PRESENT = 1, WZCCTL_WEPK_XFORMAT = 2, WZCCTL_ONEX_ENABLED = 0x10, EAP_TYPE_TLS = 13,
       AUTH_OPEN = 0, AUTH_WPA = 3, AUTH_WPAPSK = 4, AUTH_WPA2 = 6, AUTH_WPA2PSK = 7,
       PRIV_WEP = 0, PRIV_NONE = 1, PRIV_TKIP = 4, PRIV_AES = 6, INFRA_BSS = 1 };

typedef DWORD (WINAPI *EnumFn)(LPWSTR, IntfTable*);
typedef DWORD (WINAPI *QueryFn)(LPWSTR, DWORD, IntfEntry*, LPDWORD);
typedef VOID  (WINAPI *DeleteFn)(IntfEntry*);
typedef VOID  (WINAPI *Pass2KeyFn)(WlanConfig*, LPCSTR);

// iphlpapi: the adapter's address (IP_ADAPTER_INFO up to its address list)
struct IpAddrString { IpAddrString* next; char ip[16]; char mask[16]; DWORD context; };
struct AdapterInfo {
    AdapterInfo* next;
    DWORD        comboIndex;
    char         name[260];
    char         descr[132];
    UINT         addrLen;
    BYTE         addr[8];
    DWORD        index;
    UINT         type;
    UINT         dhcp;
    IpAddrString* current;
    IpAddrString ipList;
};
typedef DWORD (WINAPI *AdaptersFn)(AdapterInfo*, ULONG*);

static EnumFn     s_enum;
static QueryFn    s_query, s_set, s_refresh;
static DeleteFn   s_delete;
static Pass2KeyFn s_pass2key;
static AdaptersFn s_adapters;

struct Known { wchar_t ssid[33]; wchar_t pass[64]; };

static CRITICAL_SECTION s_cs;
static WifiInfo   s_info;              // published (under s_cs)
static Known      s_known[kWifiKnown]; // under s_cs
static int        s_knownCount;
static wchar_t    s_connSsid[33], s_connPass[64], s_forgetSsid[33];
static bool       s_connReq, s_forgetReq, s_scanReq;
static volatile bool s_stop;
// Two threads (the tester 2026-11-19: the Wi-Fi thread stuck in a WZC call, the switch pressed three times in 3 minutes
// did nothing - one thread served both): the poll (WifiThread: WZC, the SDK's client and access point, NDISUIO, the
// address - any of them may hang in the driver) and the switch (SwitchThread: only the radio's SetMode, bounded by
// RadioGuarded). s_wake wakes the switch, s_pollWake the poll.
static HANDLE     s_wake, s_thread, s_pollWake, s_switch;
static CRITICAL_SECTION s_modeCs;          // the SDK's SetMode: one at a time (DkModeAt)
static volatile LONG s_radioGen;           // the switch's radio changes so far: the poll's work begun before one is void
static volatile bool s_radioOn = true;     // the switch's radio (the unit boots with it on)
static volatile bool s_dkReady;            // the SDK looked for at the start: the switch may act
// The watch (WifiWatch, the window's timer): the poll's current step that may block, since when, its number
static const wchar_t* volatile s_step;
static volatile DWORD s_stepTick;
static volatile LONG s_stepNo, s_stallLogged;
static HWND       s_notify;
static UINT       s_msg;
static WifiLogFn  s_log;
static wchar_t    s_guid[64];          // the adapter
static volatile bool s_wantOn = true;  // WifiEnable
static volatile LONG s_offs;           // WifiEnable(false) so far: a quick off - on is still a restart (RadioStep)
static volatile bool s_wantAp, s_apAuto;   // WifiApManual, WifiApSetup
static wchar_t s_joined[33];               // the network the SDK was last told to join (the worker's thread)
static char       s_apSsid[33], s_apPass[65];   // under s_cs
static wchar_t    s_apSsidW[33], s_apPassW[65]; // the same for the screens

// The Wi-Fi vendor's API, WifiDKCore.dll (Qualnetics WDK in the firmware's \Windows; Ghidra 2026-10-18 on it and
// on its tools WdkModeSwitch.exe, WdkApCtrl.exe, WdkHealthSvc_WifiACS.dll): WDK_DMW_SetMode 0 = off (both
// adapters powered down), 1 = access point (the AP adapter ndcufmp1 powered, the chip soft-booted with ap.xbv),
// 2 = client (ndcufmp2, run by WZC), 3 = Wi-Fi Direct. The AP's settings: DMW_AP_SETTINGS, 0x140 bytes.
typedef LONG (*DkInitFn)(void);
typedef LONG (*DkModeFn)(int);
typedef LONG (*DkGetModeFn)(int*);
typedef LONG (*DkApFn)(BYTE*);
typedef LONG (*DkAssocFn)(void**, DWORD*);
typedef void (*DelArrFn)(void*);
// The client as the stock shell joins a network (Ghidra 2026-11-03, G3NavHMI's WifiThreadConnectManage and the
// DLL's WDK_STA_*; the user 2026-11-03: Wi-Fi worked with the shell, never through WZC alone): the networks in
// sight (GetAvailableApList: entries of 0x74 bytes, the network's own description - 0x54 bytes: its size, the
// SSID's 34 wide chars, three words of its security - at +4), that description and a security block (0x630
// bytes: the key's wide chars at +4, the WEP key's index at +0x204, 1) to AddPreferredAp, then
// ConnectToPreferredApByName(SSID).
typedef LONG (*DkListFn)(BYTE*, int*);
typedef LONG (*DkAddFn)(BYTE*, BYTE*, DWORD);
typedef LONG (*DkNameFn)(const wchar_t*);
// The saved ones (GetPreferredApList, Ghidra 2026-11-03: entries of that description alone, 0x54 bytes).
static DkListFn    s_dkList, s_dkPrefs;
static DkAddFn     s_dkAdd;
static DkNameFn    s_dkJoin, s_dkDrop;
enum { DKE_SIZE = 0x74, DKE_AP = 4, DKE_SSID = 8, DKAP_SIZE = 0x54, DKAP_SSID = 4, DKAP_AUTH = 0x48,
       DKAP_CIPHER = 0x4C, DKAP_PRIV = 0x50, DKSEC_SIZE = 0x630, DKSEC_KEY = 4, DKSEC_KEYID = 0x204, kDkSeen = 48 };
static DkModeFn    s_dkSetMode;
static DkGetModeFn s_dkGetMode;
static DkApFn      s_dkApGetDef, s_dkApSetDef, s_dkApSetCur;
static DkAssocFn   s_dkAssoc;
static DelArrFn    s_delArr;      // coredll's operator delete[]: the association list is the DLL's new[]
// The scanner's switch and the driver's own verdict (Ghidra 2026-11-06 on WifiDKCore.dll and on the stock tools
// that call them: the shell's start - WIFI_STA_GetScannerEnabled(&on), then WIFI_STA_SetScannerEnabled(1) when it
// was off; WdkStaCtrl.exe - WDK_DMW_GetStaConnFailReason(2, buf), printed as "sta connect status"; wfdtool.exe -
// WDK_DMW_ResetDriver(0)). WIFI_STA_SetBackgroundScannerEnabled also exists, but no module of the unit calls it.
typedef LONG (*DkFailFn)(int, BYTE*);   // the client's mode (2), the reason: one byte
typedef LONG (*DkScanFn)(int);          // the scanner on (1) / off (0)
typedef LONG (*DkResetFn)(int);         // 0, as wfdtool does
static DkFailFn    s_dkFail;
static DkScanFn    s_dkScan;
static DkResetFn   s_dkReset;
// The rest of the shell's start (FUN_004ca86c, PROJECT.md "Справочник Wi-Fi"): WIFI_STA_GetScannerEnabled (#551),
// WDK_DMW_GetAdapterName(2, buf, 0x80) (#106), WIFI_STA_Get/SetEnableAutoConnect(name, ...) (#553 / #552)
typedef LONG (*DkScanGetFn)(int*);
typedef LONG (*DkAdapterFn)(int, wchar_t*, int);
typedef LONG (*DkAutoGetFn)(const wchar_t*, int*);
typedef LONG (*DkAutoSetFn)(const wchar_t*, int);
static DkScanGetFn s_dkScanGet;
static DkAdapterFn s_dkAdapter;
static DkAutoGetFn s_dkAutoGet;
static DkAutoSetFn s_dkAutoSet;
enum { DK_OFF = 0, DK_AP = 1, DK_STA = 2,
       APS_SIZE = 0x140, APS_SSID = 0x04, APS_BEACON = 0x28, APS_CHANNEL = 0x30, APS_CIPHER = 0x34, APS_WPA = 0x38,
       APS_WEPKEYID = 0x40, APS_KEY = 0xB0, APS_HIDE = 0x134, APS_AUTHTO = 0x138, APS_ASSOCTO = 0x13C,
       CIPHER_AES = 0x20, WPA_MODE2 = 2, kApChannel = 6,
       kAutoIdleMs = 30000,    // not connected, no known network in sight that long: the access point
       kApWaitMs = 180000,     // no phone joins it: back to a client
       kHandoverMs = 20000,    // a phone joined: time for it to raise its hotspot (on 2.4 GHz now)
       kStaWaitMs = 40000,     // back as a client: the phone's hotspot not joined that long - again
       kAttempts = 3, kRetryMs = 600000,
       kQuietMs = 30000 };   // the periodic scan held off while a connection is made (RefreshQuiet)

static void Log(const wchar_t* fmt, ...)
{
    if (!s_log)
        return;
    wchar_t s[400];   // (VLog's room: a beacon's IEs)
    va_list a;
    va_start(a, fmt);
    _vsnwprintf(s, 400, fmt, a);
    va_end(a);
    s[399] = 0;
    s_log(L"%s", s);
}

int WifiBars(int rssi)
{
    return rssi >= -55 ? 4 : rssi >= -65 ? 3 : rssi >= -75 ? 2 : rssi >= -85 ? 1 : 0;
}

static int SecOf(const WlanConfig& c)
{
    if (c.privacy == PRIV_NONE)
        return WSEC_OPEN;
    if (c.authMode == AUTH_WPA2 || c.authMode == AUTH_WPA2PSK)
        return WSEC_WPA2;
    if (c.authMode == AUTH_WPA || c.authMode == AUTH_WPAPSK)
        return WSEC_WPA;
    return WSEC_WEP;
}

static void SsidText(const Ssid& s, wchar_t* w)
{
    char a[33];
    int n = s.len < 32 ? (int)s.len : 32;
    memcpy(a, s.ssid, n);
    a[n] = 0;
    if (!MultiByteToWideChar(CP_UTF8, 0, a, -1, w, 33))
        MultiByteToWideChar(CP_ACP, 0, a, -1, w, 33);
    w[32] = 0;
}

// The configurations of a WZC_802_11_CONFIG_LIST, checked against its size (the layout is ours, not the SDK's).
// The channel from NDIS_802_11_CONFIGURATION's DSConfig (kHz): 2.4 GHz 1..14, 5 GHz 36..; 0 unknown
static int ChannelOf(const WlanConfig& c)
{
    int mhz = (int)(c.configuration[3] / 1000);
    if (mhz == 2484)
        return 14;
    if (mhz >= 2412 && mhz < 2484)
        return (mhz - 2407) / 5;
    if (mhz >= 5000 && mhz < 6000)
        return (mhz - 5000) / 5;
    return 0;
}

static WlanConfig* ListItems(const RawData& rd, int* n)
{
    *n = 0;
    if (!rd.data || rd.len < sizeof(ConfigList))
        return NULL;
    ConfigList* l = (ConfigList*)rd.data;
    if (!l->count || rd.len < sizeof(ConfigList) + l->count * sizeof(WlanConfig))
        return NULL;
    *n = (int)l->count;
    return (WlanConfig*)(rd.data + sizeof(ConfigList));
}

#ifdef UNDER_CE
static bool LoadWzc()
{
    HMODULE m = LoadLibrary(L"wzcsapi.dll");
    if (!m)
        return false;
    s_enum = (EnumFn)PROC(m, "WZCEnumInterfaces");
    s_query = (QueryFn)PROC(m, "WZCQueryInterface");
    s_set = (QueryFn)PROC(m, "WZCSetInterface");
    s_refresh = (QueryFn)PROC(m, "WZCRefreshInterface");
    s_delete = (DeleteFn)PROC(m, "WZCDeleteIntfObj");
    s_pass2key = (Pass2KeyFn)PROC(m, "WZCPassword2Key");
    HMODULE ip = LoadLibrary(L"iphlpapi.dll");
    if (ip)
        s_adapters = (AdaptersFn)PROC(ip, "GetAdaptersInfo");
    return s_enum && s_query && s_set && s_refresh && s_delete && s_pass2key;
}

static bool FindAdapter(wchar_t* descr, int cap)
{
    IntfTable t;
    memset(&t, 0, sizeof(t));
    if (s_enum(NULL, &t) != 0 || !t.count) {
        if (t.intfs)
            LocalFree(t.intfs);
        return false;
    }
    // the adapter is dual: NDCUFMP2 = the client (station), NDCUFMP1 = the access point (registry of 4120)
    DWORD pick = 0;
    for (DWORD i = 0; i < t.count; i++) {
        if (wcsstr(t.intfs[i].guid, L"NDCUFMP2")) {
            pick = i;
            break;
        }
        if (!wcsstr(t.intfs[i].guid, L"NDCUFMP1") && wcsstr(t.intfs[pick].guid, L"NDCUFMP1"))
            pick = i;
    }
    wcsncpy(s_guid, t.intfs[pick].guid, 63);
    s_guid[63] = 0;
    wcsncpy(descr, t.intfs[pick].guid, cap - 1);
    descr[cap - 1] = 0;
    for (DWORD i = 0; i < t.count; i++)
        LocalFree(t.intfs[i].guid);
    LocalFree(t.intfs);
    return true;
}

// The interface's object for a call: its name in memory WZCDeleteIntfObj may free.
static void IntfInit(IntfEntry* e)
{
    memset(e, 0, sizeof(*e));
    size_t n = (wcslen(s_guid) + 1) * sizeof(wchar_t);
    e->guid = (LPWSTR)LocalAlloc(LMEM_FIXED, n);
    if (e->guid)
        memcpy(e->guid, s_guid, n);
}

static void Encrypt(WlanConfig* c)
{
    static const BYTE kFake[13] = { 0x56, 0x09, 0x08, 0x98, 0x4D, 0x08, 0x11, 0x66, 0x42, 0x03, 0x01, 0x67, 0x66 };
    for (int i = 0; i < 32; i++)
        c->keyMaterial[i] ^= kFake[(7 * i) % 13];
}

static bool HexBytes(const char* s, BYTE* out, int n)
{
    for (int i = 0; i < n; i++) {
        int v = 0;
        for (int k = 0; k < 2; k++) {
            char ch = s[i * 2 + k];
            int d = ch >= '0' && ch <= '9' ? ch - '0' : ch >= 'a' && ch <= 'f' ? ch - 'a' + 10 :
                    ch >= 'A' && ch <= 'F' ? ch - 'A' + 10 : -1;
            if (d < 0)
                return false;
            v = v * 16 + d;
        }
        out[i] = (BYTE)v;
    }
    return true;
}

static const DWORD kEapolEnabled = 0x80000000;   // EAPOL_ENABLED (WZC_EAPOL_PARAMS.dwEapFlags)

// A preferred configuration for a visible network (its scan entry gives the security) and a password.
static bool MakeConfig(const WlanConfig& seen, const wchar_t* pass, WlanConfig* c, wchar_t* why, int cap)
{
    *c = seen;
    c->length = sizeof(WlanConfig);
    c->ctlFlags = 0;
    memset(c->mac, 0, sizeof(c->mac));
    c->rssi = 0;
    c->infraMode = INFRA_BSS;
    c->keyIndex = c->keyLength = 0;
    memset(c->keyMaterial, 0, sizeof(c->keyMaterial));
    memset(&c->userData, 0, sizeof(c->userData));
    c->enable8021x = FALSE;
    c->eapFlags = c->eapType = c->authDataLen = 0;
    c->authData = NULL;
    memset(&c->networkData, 0, sizeof(c->networkData));
    char p[64];
    WideCharToMultiByte(CP_UTF8, 0, pass, -1, p, sizeof(p), NULL, NULL);
    p[63] = 0;
    int n = (int)strlen(p);
    int sec = SecOf(seen);
    if (sec == WSEC_OPEN) {
        c->authMode = AUTH_OPEN;
        c->privacy = PRIV_NONE;
        return true;
    }
    if (sec == WSEC_WEP) {
        c->authMode = AUTH_OPEN;
        c->privacy = PRIV_WEP;
        if (n == 5 || n == 13) {
            memcpy(c->keyMaterial, p, n);
            c->keyLength = n;
            c->ctlFlags = WZCCTL_WEPK_PRESENT;
        } else if ((n == 10 || n == 26) && HexBytes(p, c->keyMaterial, n / 2)) {
            c->keyLength = n / 2;
            c->ctlFlags = WZCCTL_WEPK_PRESENT | WZCCTL_WEPK_XFORMAT;
        } else {
            wcsncpy(why, L"ключ WEP: 5 или 13 символов (10 или 26 hex)", cap);
            return false;
        }
    } else {   // WPA / WPA2 personal: the passphrase -> the 256-bit key
        c->authMode = sec == WSEC_WPA2 ? AUTH_WPA2PSK : AUTH_WPAPSK;
        if (c->privacy != PRIV_TKIP && c->privacy != PRIV_AES)
            c->privacy = sec == WSEC_WPA2 ? PRIV_AES : PRIV_TKIP;
        if (n == 64 && HexBytes(p, c->keyMaterial, 32)) {
            c->keyLength = 32;
        } else if (n >= 8 && n <= 63) {
            s_pass2key(c, p);
            c->keyLength = 32;
        } else {
            wcsncpy(why, L"пароль WPA: от 8 до 63 символов", cap);
            return false;
        }
        // the 4-way handshake of WPA-PSK is EAPOL's (eapol.dll): 802.1X on, as CE's wzctool sets it (HU 2026-10-31:
        // without the stock shell's WifiDK no phone ever gave an address - the link dropped after 2 s)
        c->ctlFlags = WZCCTL_WEPK_PRESENT | WZCCTL_WEPK_XFORMAT | WZCCTL_ONEX_ENABLED;
        c->enable8021x = TRUE;
        c->eapFlags = kEapolEnabled;
        c->eapType = EAP_TYPE_TLS;
    }
    Encrypt(c);
    return true;
}

static bool SameSsid(const WlanConfig& c, const wchar_t* ssid)
{
    wchar_t w[33];
    SsidText(c.ssid, w);
    return !wcscmp(w, ssid);
}

// WZC's preferred list = `add` (first) + what it had, without `drop` and without the networks of `add`.
static bool SetPrefs(const IntfEntry& cur, const WlanConfig* add, int addCount, const wchar_t* drop)
{
    int had = 0;
    WlanConfig* old = ListItems(cur.prefList, &had);
    int total = addCount + had;
    DWORD size = sizeof(ConfigList) + (total ? total : 1) * sizeof(WlanConfig);
    BYTE* buf = (BYTE*)LocalAlloc(LPTR, size);
    if (!buf)
        return false;
    ConfigList* l = (ConfigList*)buf;
    WlanConfig* out = (WlanConfig*)(buf + sizeof(ConfigList));
    int n = 0;
    for (int i = 0; i < addCount; i++)
        out[n++] = add[i];
    for (int i = 0; i < had; i++) {
        bool skip = drop && SameSsid(old[i], drop);
        for (int k = 0; k < addCount && !skip; k++) {
            wchar_t w[33];
            SsidText(add[k].ssid, w);
            skip = SameSsid(old[i], w);
        }
        if (!skip)
            out[n++] = old[i];
    }
    l->count = n;
    l->index = 0;
    IntfEntry e;
    memset(&e, 0, sizeof(e));
    e.guid = s_guid;
    e.prefList.len = sizeof(ConfigList) + n * sizeof(WlanConfig);
    e.prefList.data = buf;
    DWORD outFlags = 0;
    DWORD r = s_set(NULL, INTF_PREFLIST, &e, &outFlags);
    LocalFree(buf);
    if (r)
        Log(L"wifi: WZCSetInterface: ошибка %u", (unsigned)r);
    return r == 0;
}

static void IpOf(const char* name, char* ip)   // the adapter's address by its name ("" none yet)
{
    ip[0] = 0;
    if (!s_adapters)
        return;
    ULONG size = 0;
    s_adapters(NULL, &size);
    if (!size || size > 64 * 1024)
        return;
    AdapterInfo* a = (AdapterInfo*)malloc(size);
    if (a && s_adapters(a, &size) == 0) {
        for (AdapterInfo* p = a; p; p = p->next)
            if (!_stricmp(p->name, name) && strcmp(p->ipList.ip, "0.0.0.0")) {
                strncpy(ip, p->ipList.ip, 15);
                ip[15] = 0;
            }
    }
    free(a);
}

static void ReadIp(char* ip)
{
    char guid[64];
    WideCharToMultiByte(CP_ACP, 0, s_guid, -1, guid, sizeof(guid), NULL, NULL);
    IpOf(guid, ip);
}
#endif

void WifiQuiet(bool on)
{
    s_quiet = on;
}

void AdapterIp(const char* name, char* ip)
{
    ip[0] = 0;
#ifdef UNDER_CE
    if (!s_adapters) {
        HMODULE m = LoadLibrary(L"iphlpapi.dll");
        if (m)
            s_adapters = (AdaptersFn)PROC(m, "GetAdaptersInfo");
    }
    IpOf(name, ip);
#else
    (void)name;
#endif
}

static void Publish(const WifiInfo& w)
{
    EnterCriticalSection(&s_cs);
    bool changed = memcmp(&w, &s_info, sizeof(w)) != 0;
    s_info = w;
    LeaveCriticalSection(&s_cs);
    if (changed && s_notify)
        PostMessage(s_notify, s_msg, 0, 0);
}

// The poll's state, unless the switch moved the radio since its round began (gen): then the switch's is shown
static void PublishAt(const WifiInfo& w, LONG gen)
{
    if (gen == s_radioGen)
        Publish(w);
}

// The poll enters a step that may block (NULL: waiting, nothing to watch)
static void Step(const wchar_t* what)
{
    s_stepTick = GetTickCount();
    s_step = what;
    InterlockedIncrement((LONG*)&s_stepNo);
}

static void PollWait(DWORD ms)
{
    Step(NULL);
    WaitForSingleObject(s_pollWake, ms);
}

static int CmpRssi(const void* a, const void* b)
{
    return ((const WifiNet*)b)->rssi - ((const WifiNet*)a)->rssi;
}

static bool IsKnown(const wchar_t* ssid, wchar_t* pass)
{
    bool k = false;
    EnterCriticalSection(&s_cs);
    for (int i = 0; i < s_knownCount && !k; i++)
        if (!wcscmp(s_known[i].ssid, ssid)) {
            k = true;
            if (pass)
                wcscpy(pass, s_known[i].pass);
        }
    LeaveCriticalSection(&s_cs);
    return k;
}

static int s_dkBootMode = -1;   // the mode found at the start (0: both adapters off - nobody switched the client on)

static bool LoadDk()
{
    HMODULE m = LoadLibrary(L"WifiDKCore.dll");
    if (!m)
        return false;
    DkInitFn init = (DkInitFn)PROC(m, "WDK_DMW_InitApi");
    s_dkSetMode = (DkModeFn)PROC(m, "WDK_DMW_SetMode");
    s_dkGetMode = (DkGetModeFn)PROC(m, "WDK_DMW_GetMode");
    s_dkApGetDef = (DkApFn)PROC(m, "WDK_AP_GetDefaultSettings");
    s_dkApSetDef = (DkApFn)PROC(m, "WDK_AP_SetDefaultSettings");
    s_dkApSetCur = (DkApFn)PROC(m, "WDK_AP_SetCurrentSettings");
    s_dkAssoc = (DkAssocFn)PROC(m, "WDK_AP_GetAssociationList");
    s_delArr = (DelArrFn)PROC(GetModuleHandle(L"coredll.dll"), "??_V@YAXPAX@Z");
    s_dkList = (DkListFn)PROC(m, "WDK_STA_GetAvailableApList");
    s_dkAdd = (DkAddFn)PROC(m, "WDK_STA_AddPreferredAp");
    s_dkJoin = (DkNameFn)PROC(m, "WDK_STA_ConnectToPreferredApByName");
    s_dkDrop = (DkNameFn)PROC(m, "WDK_STA_RemovePreferredApByName");
    s_dkPrefs = (DkListFn)PROC(m, "WDK_STA_GetPreferredApList");
    s_dkFail = (DkFailFn)PROC(m, "WDK_DMW_GetStaConnFailReason");
    s_dkScan = (DkScanFn)PROC(m, "WIFI_STA_SetScannerEnabled");
    s_dkReset = (DkResetFn)PROC(m, "WDK_DMW_ResetDriver");
    s_dkScanGet = (DkScanGetFn)PROC(m, "WIFI_STA_GetScannerEnabled");
    s_dkAdapter = (DkAdapterFn)PROC(m, "WDK_DMW_GetAdapterName");
    s_dkAutoGet = (DkAutoGetFn)PROC(m, "WIFI_STA_GetEnableAutoConnect");
    s_dkAutoSet = (DkAutoSetFn)PROC(m, "WIFI_STA_SetEnableAutoConnect");
    if (!s_dkList || !s_dkAdd || !s_dkJoin || !s_dkDrop)
        s_dkAdd = NULL;   // (the client through WZC then)
    if (!init || !s_dkSetMode || !s_dkGetMode || !s_dkApGetDef || !s_dkApSetDef || !s_dkApSetCur || !s_dkAssoc) {
        s_dkSetMode = NULL;
        return false;
    }
    // (the tester 2026-11-19: tried once, a failure left no SDK until the next boot) a few tries, the stop cuts them
    LONG hr;
    for (int tries = 1;; tries++) {
        Step(L"SDK: InitApi");
        hr = init();
        unsigned gap = InitRetryMs(tries, hr);
        if (!gap || s_stop)
            break;
        Log(L"wifi: WifiDKCore, InitApi 0x%08X - попытка %d из %d, снова через %u с", (unsigned)hr, tries, kInitTries,
            gap / 1000);
        PollWait(gap);
    }
    int mode = -1;
    s_dkGetMode(&mode);
    Log(L"wifi: WifiDKCore, InitApi 0x%08X, режим %d", (unsigned)hr, mode);
    if (hr < 0)
        s_dkSetMode = NULL;
    s_dkBootMode = mode;
    return s_dkSetMode != NULL;
}

// A phone's hotspot with SAE beside PSK (Galaxy A24, sheet 24): the SDK lists it as open (its security 2/1/0) and the
// unit associates with no encryption: the hotspot drops it in a second. Its beacon says what it is; the values are those
// the SDK itself gives a WPA2 hotspot (MRNote13P, 2/3/7 with the same beacon without SAE, its key set up).
enum { DK_AUTH_WPA2 = 2, DK_CIPHER_AES = 3, DK_PRIV_WPA2 = 7 };
#ifdef UNDER_CE
static bool BeaconWpa2Psk(const wchar_t* ssid);
#endif

// A network joined as the shell joins it (see DkListFn); false: not in sight or refused (why: the message)
static bool s_dkUnlisted;   // the last DkJoin found the network not yet in the SDK's list (AutoJoinDue)

static bool DkJoin(const wchar_t* ssid, const wchar_t* pass, wchar_t* why, int cap)
{
    int n = kDkSeen;
    BYTE* list = (BYTE*)malloc(kDkSeen * DKE_SIZE);
    LONG hr = list ? s_dkList(list, &n) : E_OUTOFMEMORY;
    BYTE* ap = NULL;
    for (int i = 0; hr >= 0 && i < n && i < kDkSeen && !ap; i++)
        if (!wcsncmp((const wchar_t*)(list + i * DKE_SIZE + DKE_SSID), ssid, 33))
            ap = list + i * DKE_SIZE + DKE_AP;
    s_dkUnlisted = !ap;
    if (!ap) {
        Log(L"wifi: SDK: сети %s нет в списке (0x%08X, сетей %d)", ssid, (unsigned)hr, n);
        _snwprintf(why, cap, L"сети %s не видно", ssid);
        free(list);
        return false;
    }
    static BYTE sec[DKSEC_SIZE];
    memset(sec, 0, sizeof(sec));
    wcsncpy((wchar_t*)(sec + DKSEC_KEY), pass, 255);
    *(DWORD*)(sec + DKSEC_KEYID) = 1;
#ifdef UNDER_CE
    if (!*(DWORD*)(ap + DKAP_PRIV) && BeaconWpa2Psk(ssid)) {
        Log(L"wifi: SDK: %s открыта, в маяке WPA2 - защита из маяка", ssid);
        *(DWORD*)(ap + DKAP_AUTH) = DK_AUTH_WPA2;
        *(DWORD*)(ap + DKAP_CIPHER) = DK_CIPHER_AES;
        *(DWORD*)(ap + DKAP_PRIV) = DK_PRIV_WPA2;
    }
#endif
    LONG drop = s_dkDrop(ssid);   // (a stale one with an older password)
    LONG add = s_dkAdd(ap, sec, 0);
    LONG join = add >= 0 ? s_dkJoin(ssid) : add;
    int plen = (int)wcslen(pass), wide = 0;   // (the password's shape, never itself: the user 2026-11-26, 9 characters
    for (int i = 0; i < plen; i++)             // did not join, 8 did - WifiDKCore takes both alike)
        wide += pass[i] > 0x7E || pass[i] < 0x20;
    Log(L"wifi: SDK: %s (защита %u/%u/%u, пароль %d симв., не латиница/цифры %d) - Remove 0x%08X, Add 0x%08X, "
        L"Connect 0x%08X", ssid, *(DWORD*)(ap + DKAP_AUTH), *(DWORD*)(ap + DKAP_CIPHER), *(DWORD*)(ap + DKAP_PRIV),
        plen, wide, (unsigned)drop, (unsigned)add, (unsigned)join);
    free(list);
    if (join < 0)
        _snwprintf(why, cap, L"%s: отказ 0x%08X", ssid, (unsigned)join);
    return join >= 0;
}

// The SDK's saved networks into the log and dropped - as a power cut leaves them (the user 2026-11-03: after
// joining a phone's hotspot no network at all, an ordinary restart did not help - it keeps the registry - a power
// cut did). VestaHome joins its known ones itself (DkJoin, their passwords its own).
static void DkClearPrefs(const wchar_t* why)
{
    if (!s_dkPrefs || !s_dkAdd)
        return;
    int n = kDkSeen;
    BYTE* list = (BYTE*)malloc(kDkSeen * DKAP_SIZE);
    LONG hr = list ? s_dkPrefs(list, &n) : E_OUTOFMEMORY;
    Log(L"wifi: SDK: сохранённых сетей %d (0x%08X), удаляю - %s", hr >= 0 ? n : 0, (unsigned)hr, why);
    for (int i = 0; hr >= 0 && i < n && i < kDkSeen; i++) {
        BYTE* ap = list + i * DKAP_SIZE;
        wchar_t ssid[34];
        wcsncpy(ssid, (const wchar_t*)(ap + DKAP_SSID), 33);
        ssid[33] = 0;
        LONG r = s_dkDrop(ssid);
        Log(L"wifi: SDK:   %s (защита %u/%u/%u) - Remove 0x%08X", ssid, *(DWORD*)(ap + DKAP_AUTH),
            *(DWORD*)(ap + DKAP_CIPHER), *(DWORD*)(ap + DKAP_PRIV), (unsigned)r);
    }
    free(list);
}

static bool DkMode(int mode)
{
    LONG hr = s_dkSetMode(mode);
    Log(L"wifi: режим %s (SetMode 0x%08X)", mode == DK_AP ? L"точка" : mode == DK_STA ? L"клиент" : L"выкл.",
        (unsigned)hr);
    return hr >= 0;
}

// SetMode one at a time, and the poll's only while the switch has not moved since its round began (gen; -1: the
// switch's own): the switch's SetMode, after its gen, always comes last (it takes the lock after the poll's).
static bool DkModeAt(int mode, LONG gen)
{
    EnterCriticalSection(&s_modeCs);
    bool ok = false;
    if (gen < 0 || gen == s_radioGen)
        ok = DkMode(mode);
    LeaveCriticalSection(&s_modeCs);
    return ok;
}

// The access point's radio in the registry (Comm\ndcufmp1\Parms EnableRadio, the firmware's 0; HU 2026-10-18-2: with
// it no phone saw our network): 1 for our access point, 0 again when it goes and at the start - the stock shell never
// has it on (PROJECT.md "Справочник Wi-Fi", the tester 2026-11-19). The adapter reads it when it starts.
static void ApRadio(DWORD v)
{
    HKEY key;
    DWORD was = (DWORD)-1, size = sizeof(was);
    LONG rr = -1;
    if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, L"Comm\\ndcufmp1\\Parms", 0, NULL, 0, 0, NULL, &key, NULL) == ERROR_SUCCESS) {
        RegQueryValueEx(key, L"EnableRadio", NULL, NULL, (LPBYTE)&was, &size);
        rr = RegSetValueEx(key, L"EnableRadio", 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
        RegCloseKey(key);
    }
    Log(L"wifi: точка: EnableRadio %d -> %u (%ld)", (int)was, (unsigned)v, rr);
}

// The access point with our network: its settings as the adapter's defaults (the registry, read when it starts)
// and, once started, as its current ones.
static bool ApStart(LONG gen)
{
    BYTE s[APS_SIZE];
    memset(s, 0, sizeof(s));
    *(DWORD*)s = APS_SIZE;
    LONG hr = s_dkApGetDef(s);
    if (hr < 0) {   // what the firmware's registry holds for it otherwise
        memset(s, 0, sizeof(s));
        *(DWORD*)s = APS_SIZE;
        *(DWORD*)(s + APS_BEACON) = 100;
        *(DWORD*)(s + APS_WEPKEYID) = 1;
        *(DWORD*)(s + APS_AUTHTO) = 1;
        *(DWORD*)(s + APS_ASSOCTO) = 5;
    }
    EnterCriticalSection(&s_cs);
    memset(s + APS_SSID, 0, 33);
    strncpy((char*)s + APS_SSID, s_apSsid, 32);
    memset(s + APS_KEY, 0, 65);
    strncpy((char*)s + APS_KEY, s_apPass, 64);
    LeaveCriticalSection(&s_cs);
    *(DWORD*)(s + APS_CHANNEL) = kApChannel;
    *(DWORD*)(s + APS_CIPHER) = CIPHER_AES;
    *(DWORD*)(s + APS_WPA) = WPA_MODE2;
    *(DWORD*)(s + APS_HIDE) = 0;
    LONG def = s_dkApSetDef(s);
    ApRadio(1);
    if (!DkModeAt(DK_AP, gen))
        return false;
    LONG cur = s_dkApSetCur(s);
    Log(L"wifi: точка %S, канал %d (GetDefault 0x%08X, SetDefault 0x%08X, SetCurrent 0x%08X)", (char*)s + APS_SSID,
        kApChannel, (unsigned)hr, (unsigned)def, (unsigned)cur);
    return true;
}

static int ApClients()
{
    void* list = NULL;
    DWORD n = 0;
    if (s_dkAssoc(&list, &n) < 0)
        return -1;
    if (list && s_delArr)
        s_delArr(list);
    return (int)n;
}

// The radio on / off: through the vendor's API (mode off / client), otherwise through the power manager, as
// Windows Mobile's wireless manager does it (the NDIS miniport's power class + the client adapter's name).
static void RadioPower(bool on, LONG gen)
{
#ifdef UNDER_CE
    if (s_dkSetMode) {
        DkModeAt(on ? DK_STA : DK_OFF, gen);
        return;
    }
    typedef DWORD (WINAPI *SetPowFn)(PVOID, DWORD, int);
    SetPowFn fn = (SetPowFn)GetProcAddress(GetModuleHandle(L"coredll.dll"), L"SetDevicePower");
    if (!fn) {
        Log(L"wifi: нет SetDevicePower");
        return;
    }
    // POWER_NAME = 1; D0 = on, D4 = off
    DWORD r = fn((PVOID)L"{98C5250D-C29A-4985-AE5F-AFE5367E5006}\\NDCUFMP2", 1, on ? 0 : 4);
    Log(L"wifi: радио %s (SetDevicePower %lu)", on ? L"включено" : L"выключено", r);
#else
    (void)on;
    (void)gen;
#endif
}

// The driver's answer can take long or never come (the SDK's mode change, 2026-11-05: BT's off took 12 s): the switch on
// a thread of its own, its wait bounded, so the Wi-Fi thread always goes on and the radio's state is in the log.
// Its argument: on in bit 0, (gen + 1) above it (gen -1: the switch's own, see DkModeAt).
static DWORD WINAPI RadioThread(LPVOID arg)
{
    size_t a = (size_t)arg;
    RadioPower((a & 1) != 0, (LONG)(a >> 1) - 1);
    return 0;
}

static void RadioGuarded(bool on, LONG gen)
{
    enum { kRadioWaitMs = 10000 };
    HANDLE h = CreateThread(NULL, 0, RadioThread, (LPVOID)(((size_t)(gen + 1) << 1) | (on ? 1 : 0)), 0, NULL);
    if (!h) {
        RadioPower(on, gen);
        return;
    }
    if (WaitForSingleObject(h, kRadioWaitMs) != WAIT_OBJECT_0)
        Log(L"wifi: радио: драйвер не ответил за %d с (%s)", kRadioWaitMs / 1000, on ? L"вкл." : L"выкл.");
    CloseHandle(h);
}

// The switch on again before the radio was off 30 s in all (radiowait.h): the seconds left shown, a second waited
// (the switch may go back meanwhile). false - on now.
static bool RadioOnTooSoon(WifiInfo& w, DWORD offTick)
{
    DWORD left = RadioOffLeft(offTick, GetTickCount());
    if (!left)
        return false;
    w.state = WIFI_OFF;
    w.count = 0;
    w.ssid[0] = 0;
    w.ip[0] = 0;
    _snwprintf(w.msg, 96, L"включение Wi-Fi: %u с…", (unsigned)(left + 999) / 1000);
    Publish(w);
    WaitForSingleObject(s_wake, left < 1000 ? left : 1000);
    return true;
}

#ifdef UNDER_CE
// The client's scanner and auto-connect on, as the shell's start (FUN_004ca86c) puts them: Wi-Fi Direct (mode 3) turns
// both off, and a VestaHome that fell while connecting (RefreshQuiet) leaves the scanner off. Set only when off (or
// unknown); the old values into the log.
static void DkScannerOn()
{
    int was = -1;
    LONG get = s_dkScanGet ? s_dkScanGet(&was) : E_NOTIMPL;
    LONG set = s_dkScan && was != 1 ? s_dkScan(1) : S_FALSE;
    Log(L"wifi: SDK: сканер был %d (0x%08X), включаю 0x%08X", was, (unsigned)get, (unsigned)set);
}

static void DkAutoConnectOn()
{
    wchar_t name[0x80];
    name[0] = 0;
    LONG gn = s_dkAdapter ? s_dkAdapter(DK_STA, name, 0x80) : E_NOTIMPL;
    name[0x7F] = 0;
    int was = -1;
    LONG get = gn >= 0 && s_dkAutoGet ? s_dkAutoGet(name, &was) : E_NOTIMPL;
    LONG set = gn >= 0 && s_dkAutoSet && was != 1 ? s_dkAutoSet(name, 1) : S_FALSE;
    Log(L"wifi: SDK: адаптер %s (0x%08X), автоподключение было %d (0x%08X), включаю 0x%08X", name, (unsigned)gn, was,
        (unsigned)get, (unsigned)set);
}

// The start as the stock shell's (the tester 2026-11-19, PROJECT.md "Справочник Wi-Fi"): the SDK (InitApi tried a few
// times), then the switch may act; the access point's radio off; the client switched on over any mode but a client
// (SetMode(2), bounded), 3 s for the adapter to come up (the switch cuts them short), the scanner and the auto-connect
// back on. The SDK's saved networks and WZC only after that (0x80070037 - the adapter not there yet).
// The client switched on at the start, bounded, then its 3 s; false - the switch moved meanwhile (its radio then)
static bool BootClient(LONG gen)
{
    Step(L"SDK: SetMode(2)");
    RadioGuarded(true, gen);
    Step(NULL);
    DWORD t0 = GetTickCount();
    for (DWORD gone = 0; !s_stop && gen == s_radioGen && gone < (DWORD)kBootSettleMs; gone = GetTickCount() - t0)
        WaitForSingleObject(s_pollWake, kBootSettleMs - gone);
    return gen == s_radioGen && !s_stop;
}

static void WifiBoot()
{
    bool dk = LoadDk();
    s_dkReady = true;
    SetEvent(s_wake);
    if (!s_wantAp)
        ApRadio(0);
    LONG gen = s_radioGen;
    if (!dk || !s_wantOn)
        return;
    if (BootSetsClient(s_dkBootMode, true) && !BootClient(gen))
        return;
    Step(L"SDK: сканер и автоподключение");
    DkScannerOn();
    DkAutoConnectOn();
}
#endif

#ifdef UNDER_CE
// The WdkHealthSvc's WzcRefresh rescans the networks every 6 s while the client is not associated (its
// ScanOnRefresh=1, RefreshFast=6000; the service's loop is WIFI_STA_GetRawScanResults, #310, next to InitApi and
// GetAdapterName in its import table): the radio leaves the channel in the middle of the 4-way handshake or of the
// DHCP exchange, and the link drops 1-2 s after the association (sheet 24, p. 6-7). While a connection is being made
// that scan is held off (PLAN.md, the Wi-Fi item, step 3) through two switches - the SDK's own WIFI_STA_SetScannerEnabled
// (the one the shell sets at its start) and the service's registry values - and put back once the address is there or
// the attempt is over. Never the service's stop (WHE1: answers a stop with a reboot, 2026-11-01).
static void RefreshQuiet(bool quiet)
{
    static int s_quietNow = -1;   // what was written last (-1: nothing yet)
    if (s_quietNow == (int)quiet)
        return;
    s_quietNow = (int)quiet;
    // The SDK's own switch first: the one the shell sets at its start, and it acts at once (WIFI_STA_SetScannerEnabled,
    // one int - Ghidra 2026-11-06). WzcRefresh's 6 s round asks WIFI_STA_GetRawScanResults, and that scan is what the
    // driver takes the radio away for; the registry below is what the service re-reads on its own next round.
    wchar_t api[48];
    if (s_dkScan)
        _snwprintf(api, 48, L"0x%08X", (unsigned)s_dkScan(quiet ? 0 : 1));
    else
        wcscpy(api, L"нет");
    HKEY key;
    DWORD disp = 0, scan = quiet ? 0 : 1, fast = quiet ? kQuietMs : 6000, wasScan = 0, wasFast = 0, size = 4;
    LONG rr = RegCreateKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Qualnetics\\WdkHealthSvc\\WzcRefresh", 0, NULL, 0, 0, NULL,
                             &key, &disp);
    if (rr != ERROR_SUCCESS) {
        Log(L"wifi: WzcRefresh: ключ не открылся (%ld), скан SDK %s", rr, api);
        return;
    }
    RegQueryValueEx(key, L"ScanOnRefresh", NULL, NULL, (LPBYTE)&wasScan, &size);
    size = 4;
    RegQueryValueEx(key, L"RefreshFast", NULL, NULL, (LPBYTE)&wasFast, &size);
    LONG a = RegSetValueEx(key, L"ScanOnRefresh", 0, REG_DWORD, (const BYTE*)&scan, 4);
    LONG b = RegSetValueEx(key, L"RefreshFast", 0, REG_DWORD, (const BYTE*)&fast, 4);
    RegCloseKey(key);
    Log(L"wifi: WzcRefresh: сканирование %s (SDK %s, ScanOnRefresh %u -> %u, RefreshFast %u -> %u, %ld/%ld)",
        quiet ? L"приглушено на время подключения" : L"как было", api, (unsigned)wasScan, (unsigned)scan,
        (unsigned)wasFast, (unsigned)fast, a, b);
}

// An attempt to connect starts: the periodic scan off, the tick of its start kept (0: already held off)
static DWORD QuietStart(DWORD tick)
{
    RefreshQuiet(true);
    return tick ? tick : GetTickCount();
}
#endif

#ifdef UNDER_CE
// What WZC and the SDK do not show (sheet 22: MRNote13P's link drops in 2-10 s without an address, MiNote3's holds;
// the SDK shows both as 2/3/7): the beacons' IEs, what the unit's driver sent at the association, whether the
// 4-way handshake gave it its key. NDISUIO (nuiouser.h): on CE an OID is queried by the adapter's name, no open
// needed; only queries, WZC keeps the adapter.
enum { IOCTL_UIO_QUERY = 0x120804,   // _NDISUIO_CTL_CODE(0x201, METHOD_BUFFERED, FILE_ANY_ACCESS)
       OID_BSSID_LIST = 0x0D010217, OID_ENC_STATUS = 0x0D01011B, OID_ASSOC_INFO = 0x0D01011F, kUioHead = 8 };
static HANDLE  s_uio = INVALID_HANDLE_VALUE;
static BYTE    s_uioBuf[16384];   // the thread's alone
static int     s_uioName = -1;    // the name the driver answers to: 0 WZC's (WFIL\NDCUFMP2), 1 past its '\'
static wchar_t s_beaconLogged[kWifiNets][33];
static int     s_beaconCount;

// An OID of the client adapter: its data (in s_uioBuf) and their length, -1 none
static int UioQuery(DWORD oid, const BYTE** data)
{
    if (s_uio == INVALID_HANDLE_VALUE)
        s_uio = CreateFile(L"UIO1:", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (s_uio == INVALID_HANDLE_VALUE)
        return -1;
    const wchar_t* tail = wcsrchr(s_guid, L'\\');
    const wchar_t* names[2] = { s_guid, tail ? tail + 1 : s_guid };
    DWORD err = 0;
    for (int i = s_uioName < 0 ? 0 : s_uioName; i < 2; i++) {
        memset(s_uioBuf, 0, kUioHead);
        *(DWORD*)s_uioBuf = oid;
        *(const wchar_t**)(s_uioBuf + 4) = names[i];
        DWORD got = 0;
        if (DeviceIoControl(s_uio, IOCTL_UIO_QUERY, s_uioBuf, sizeof(s_uioBuf), s_uioBuf, sizeof(s_uioBuf), &got,
                            NULL) && got >= kUioHead) {
            if (s_uioName < 0)
                Log(L"wifi: NDISUIO отвечает по имени %s", names[i]);
            s_uioName = i;
            *data = s_uioBuf + kUioHead;
            return (int)(got - kUioHead);
        }
        err = GetLastError();
        if (s_uioName >= 0)
            break;
    }
    static DWORD s_loggedErr = 0;
    if (err != s_loggedErr) {
        s_loggedErr = err;
        Log(L"wifi: NDISUIO OID %08X: ошибка %u", (unsigned)oid, (unsigned)err);
    }
    return -1;
}

static bool BeaconLogged(const wchar_t* ssid)
{
    for (int i = 0; i < s_beaconCount; i++)
        if (!wcscmp(s_beaconLogged[i], ssid))
            return true;
    return false;
}

// A network's name from the driver's list
static void BssName(const BssEntry& b, wchar_t* ssid)
{
    if (!MultiByteToWideChar(CP_UTF8, 0, b.ssid, -1, ssid, 33))
        MultiByteToWideChar(CP_ACP, 0, b.ssid, -1, ssid, 33);
    ssid[32] = 0;
}

static bool BeaconWpa2Psk(const wchar_t* ssid)
{
    const BYTE* d;
    int n = UioQuery(OID_BSSID_LIST, &d);
    BssEntry b[kWifiNets];
    int m = n > 0 ? BssList(d, n, b, kWifiNets) : 0;
    for (int i = 0; i < m; i++) {
        wchar_t name[33];
        BssName(b[i], name);
        if (!wcscmp(name, ssid))
            return IeWpa2Psk(b[i].ies, b[i].ieLen);
    }
    return false;
}

// The beacons of the networks in sight not logged yet (or of one again: again = its name), one line each
static void LogBeacons(const WifiInfo& w, const wchar_t* again)
{
    bool fresh = again != NULL;
    for (int k = 0; k < w.count && !fresh; k++)
        fresh = !BeaconLogged(w.nets[k].ssid) && s_beaconCount < kWifiNets;
    const BYTE* d;
    int n = fresh ? UioQuery(OID_BSSID_LIST, &d) : -1;
    BssEntry b[kWifiNets];
    int m = n > 0 ? BssList(d, n, b, kWifiNets) : 0;
    for (int i = 0; i < m; i++) {
        wchar_t ssid[33];
        BssName(b[i], ssid);
        bool want = again ? !wcscmp(ssid, again) : !BeaconLogged(ssid);
        if (!ssid[0] || !want)
            continue;
        if (!again && s_beaconCount < kWifiNets)
            wcscpy(s_beaconLogged[s_beaconCount++], ssid);
        char t[300];
        IeText(b[i].ies, b[i].ieLen, t, sizeof(t));
        Log(L"wifi: маяк %s %02X:%02X:%02X:%02X:%02X:%02X %d дБм: %S", ssid, b[i].mac[0], b[i].mac[1], b[i].mac[2],
            b[i].mac[3], b[i].mac[4], b[i].mac[5], b[i].rssi, t);
    }
}

// The link just up: what the driver sent with its association request and what the access point answered
static void LogAssoc()
{
    const BYTE *d, *req, *resp;
    int n = UioQuery(OID_ASSOC_INFO, &d), rl, sl, status;
    if (n < 0)
        return;
    if (!AssocIes(d, n, &req, &rl, &resp, &sl, &status)) {
        Log(L"wifi: ассоциация: данные не разобрать (%d байт)", n);
        return;
    }
    char t[300];
    IeText(req, rl, t, sizeof(t));
    Log(L"wifi: ассоциация (ответ точки %d): ГУ послало %S", status, t);
    IeText(resp, sl, t, sizeof(t));
    Log(L"wifi: ассоциация: точка ответила %S", t);
}

// The key's state (the 4-way handshake done or not) into the log when it changes
static void LogEncryption(DWORD* last, DWORD sinceLink)
{
    const BYTE* d;
    if (UioQuery(OID_ENC_STATUS, &d) < 4)
        return;
    DWORD st = *(const DWORD*)d;
    if (st == *last)
        return;
    *last = st;
    Log(L"wifi: шифрование: %s (%u с после связи)", EncText(st), (unsigned)(sinceLink / 1000));
}
#endif

static DWORD WINAPI WifiThread(LPVOID)
{
    Log(L"поток Wi-Fi: id %08X", (unsigned)GetCurrentThreadId());
    WifiInfo w;
    memset(&w, 0, sizeof(w));
    w.state = WIFI_NOWZC;
    wcscpy(w.msg, L"запуск Wi-Fi…");
#ifdef UNDER_CE
    Step(L"wzcsapi.dll");
    if (!LoadWzc()) {
        wcscpy(w.msg, L"нет службы Wi-Fi (wzcsapi.dll)");
        Publish(w);
        Step(NULL);
        return 0;
    }
    WifiBoot();
    Step(L"SDK: сохранённые сети");
    if (s_wantOn)   // (the radio not switched on: nothing to ask)
        DkClearPrefs(L"старт");
    DWORD linkTick = 0;    // the link up without an address yet: since when (the log of a slow phone's hotspot)
    bool hadIp = false;
    DWORD encLogged = (DWORD)-1;   // LogEncryption's last
    DWORD lastAuto = 0, connectTick = 0;
    bool haveAdapter = false;
    // the access point: ap = it is up; autoAp = raised by the hand-over (s_apAuto), not by hand (s_wantAp)
    bool ap = false, autoAp = false, handover = false;
    DWORD apTick = 0, assocTick = 0, staTick = GetTickCount(), giveUp = 0;
    DWORD quietTick = 0;   // the periodic scan held off since then (RefreshQuiet), 0: scanning as usual
    int attempts = 0;
    LONG gen = s_radioGen;   // the switch's radio this round is for
    while (!s_stop) {
        if (gen != s_radioGen) {   // the switch moved the radio meanwhile (SwitchThread): all from the start
            gen = s_radioGen;
            if (ap)
                ApRadio(0);
            haveAdapter = false;
            connectTick = lastAuto = 0;
            ap = autoAp = handover = false;
            staTick = GetTickCount();
        }
        if (!s_radioOn) {   // (the switch shows it)
            PollWait(10000);
            continue;
        }
        if (s_dkSetMode) {
            DWORD now = GetTickCount();
            if (!s_apAuto)
                autoAp = false;
            bool wantAp = s_wantAp || autoAp;
            if (wantAp && !ap) {
                Step(L"SDK: точка");
                ap = ApStart(gen);
                apTick = now;
                assocTick = 0;
                if (!ap && gen != s_radioGen)
                    continue;   // (the switch went off meanwhile)
                if (!ap) {   // could not: no more tries for now
                    s_wantAp = autoAp = false;
                    giveUp = now;
                    ApRadio(0);
                    DkModeAt(DK_STA, gen);
                    wcscpy(w.msg, L"точка ГУ не включилась");
                }
            } else if (!wantAp && ap) {
                Step(L"SDK: клиент после точки");
                ApRadio(0);
                DkModeAt(DK_STA, gen);
                ap = false;
                haveAdapter = false;
                staTick = now;
                connectTick = lastAuto = 0;
            }
            if (ap) {
                Step(L"SDK: телефоны у точки");
                int n = ApClients();
                if (n > 0 && !assocTick) {
                    assocTick = now;
                    Log(L"wifi: к точке ГУ подключился телефон (%d)", n);
                }
                if (autoAp && !s_wantAp) {
                    if (assocTick && now - assocTick > kHandoverMs) {
                        Log(L"wifi: переход в клиент — к точке телефона");
                        autoAp = false;
                        handover = true;
                        continue;   // the next round goes back to a client; staTick there
                    }
                    if (!assocTick && now - apTick > kApWaitMs) {
                        Log(L"wifi: к точке ГУ никто не подключился");
                        autoAp = false;
                        giveUp = now;
                        continue;
                    }
                }
                w.state = WIFI_AP;
                w.apClients = n;
                w.apAuto = autoAp && !s_wantAp;
                w.count = 0;
                w.ssid[0] = 0;
                w.ip[0] = 0;
                if (!w.apAuto)
                    w.msg[0] = 0;
                else if (assocTick)
                    _snwprintf(w.msg, 96, L"телефон подключился — через %u с перехожу на его точку",
                               (unsigned)((kHandoverMs - (now - assocTick)) / 1000));
                else
                    _snwprintf(w.msg, 96, L"жду телефон: подключите его к сети ГУ (%u с)",
                               (unsigned)((kApWaitMs - (now - apTick)) / 1000));
                PublishAt(w, gen);
                PollWait(2000);
                continue;
            }
        }
        if (!haveAdapter) {
            Step(L"WZCEnumInterfaces");
            haveAdapter = FindAdapter(w.adapter, 48);
            if (!haveAdapter) {
                w.state = WIFI_NOADAPTER;
                w.count = 0;
                w.ssid[0] = 0;
                PublishAt(w, gen);
                PollWait(10000);
                continue;
            }
            Log(L"wifi: адаптер %s", w.adapter);
        }
        IntfEntry e;
        IntfInit(&e);
        DWORD outFlags = 0;
        Step(L"WZCQueryInterface");
        if (s_query(NULL, INTF_ALL, &e, &outFlags) != 0) {
            s_delete(&e);
            haveAdapter = false;
            continue;
        }
        // rdSSID holds the name's bytes alone (read as NDIS_802_11_SSID its first 4 bytes went for the length:
        // "Redmi" showed as "i", HU 2026-10-20)
        bool connected = e.mediaState == 1 && e.ssid.data && e.ssid.len > 0;
        w.ssid[0] = 0;
        w.rssi = 0;
        if (connected) {
            Ssid cs;
            cs.len = e.ssid.len < 32 ? e.ssid.len : 32;
            memcpy(cs.ssid, e.ssid.data, cs.len);
            SsidText(cs, w.ssid);
        }
        // WZC lost track of a link the SDK made (HU 2026-11-08-2: after a tap on another network and back, Xperia's
        // link carried the internet, WZC said none - "подключение к Xperia…" stayed, the icon grey, a new join each
        // minute): the adapter's own address says it is up
        if (!connected && s_joined[0]) {
            char ip[20];
            Step(L"адрес (iphlpapi)");
            ReadIp(ip);
            if (ip[0] && strcmp(ip, "0.0.0.0") && strncmp(ip, "169.254.", 8)) {
                connected = true;
                wcscpy(w.ssid, s_joined);
            }
        }
        int seen = 0;
        WlanConfig* list = ListItems(e.bssidList, &seen);
        w.count = 0;
        for (int i = 0; i < seen; i++) {
            wchar_t s[33];
            SsidText(list[i].ssid, s);
            if (!s[0])
                continue;   // a hidden network
            if (connected && !wcscmp(s, w.ssid) && (!w.rssi || list[i].rssi > w.rssi)) {
                w.rssi = list[i].rssi;
                static int s_loggedCh = -1;   // the connected network's channel into the log when it changes
                int ch = ChannelOf(list[i]);
                if (ch != s_loggedCh) {
                    s_loggedCh = ch;
                    Log(L"wifi: %s на канале %d (%lu кГц)", w.ssid, ch, list[i].configuration[3]);
                }
            }
            int k = 0;
            while (k < w.count && wcscmp(w.nets[k].ssid, s))
                k++;
            if (k < w.count) {   // the same network from another access point: the strongest
                if (list[i].rssi > w.nets[k].rssi) {
                    w.nets[k].rssi = list[i].rssi;
                    w.nets[k].ch = ChannelOf(list[i]);
                }
                continue;
            }
            if (w.count == kWifiNets)
                continue;
            WifiNet& n = w.nets[w.count++];
            wcscpy(n.ssid, s);
            n.rssi = list[i].rssi;
            n.sec = SecOf(list[i]);
            n.known = IsKnown(s, NULL);
            n.ch = ChannelOf(list[i]);
        }
        qsort(w.nets, w.count, sizeof(WifiNet), CmpRssi);
        Step(L"NDISUIO: маяки");
        LogBeacons(w, NULL);
        {   // how many in sight and how weak the weakest (the user 2026-11-03: dozens around, only the nearest seen)
            static int s_loggedNets = -1;
            if (w.count != s_loggedNets && (!w.count || s_loggedNets <= 0 || abs(w.count - s_loggedNets) >= 3)) {
                s_loggedNets = w.count;
                Log(L"wifi: видно сетей %d (записей WZC %d), уровни %d..%d дБм", w.count, seen,
                    w.count ? w.nets[0].rssi : 0, w.count ? w.nets[w.count - 1].rssi : 0);
            }
        }
        // requests from the window
        EnterCriticalSection(&s_cs);
        bool scan = s_scanReq, conn = s_connReq, forget = s_forgetReq;
        wchar_t cs[33], cp[64], fs[33];
        wcscpy(cs, s_connSsid);
        wcscpy(cp, s_connPass);
        wcscpy(fs, s_forgetSsid);
        s_scanReq = s_connReq = s_forgetReq = false;
        LeaveCriticalSection(&s_cs);
        Step(L"WZC / SDK: просьбы окна");
        if (forget) {
            SetPrefs(e, NULL, 0, fs);
            if (s_dkAdd)
                s_dkDrop(fs);   // (the SDK's list too)
            _snwprintf(w.msg, 96, L"забыта сеть %s", fs);
        }
        if (conn) {
            int i = 0;
            for (; i < seen; i++)
                if (SameSsid(list[i], cs))
                    break;
            WlanConfig c;
            if (s_dkAdd) {
                if (DkJoin(cs, cp, w.msg, 96)) {
                    wcscpy(s_joined, cs);
                    _snwprintf(w.msg, 96, L"подключение к %s…", cs);
                    connectTick = GetTickCount();
                    quietTick = QuietStart(quietTick);
                    wcscpy(w.ssid, cs);
                }
            } else if (i == seen) {
                _snwprintf(w.msg, 96, L"сети %s не видно", cs);
            } else if (MakeConfig(list[i], cp, &c, w.msg, 96) && SetPrefs(e, &c, 1, NULL)) {
                _snwprintf(w.msg, 96, L"подключение к %s…", cs);
                Log(L"wifi: подключение к %s", cs);
                connectTick = GetTickCount();
                quietTick = QuietStart(quietTick);
                wcscpy(w.ssid, cs);
            }
        }
        // not connected: the known networks in sight go to WZC (the registry was reset on boot)
        if (!connected && !conn && seen && !s_quiet && s_dkAdd && AutoJoinDue(lastAuto, GetTickCount(), s_dkUnlisted)) {
            wchar_t pass[64], why[96];   // (the networks are strongest first: the first known one)
            for (int k = 0; k < w.count; k++)
                if (w.nets[k].known && IsKnown(w.nets[k].ssid, pass)) {
                    lastAuto = GetTickCount();
                    if (DkJoin(w.nets[k].ssid, pass, why, 96)) {
                        wcscpy(s_joined, w.nets[k].ssid);
                        connectTick = lastAuto;
                        quietTick = QuietStart(quietTick);
                    }
                    break;
                }
        } else if (!connected && !conn && seen && !s_quiet && (!lastAuto || GetTickCount() - lastAuto > 60000)) {
            WlanConfig add[kWifiKnown];
            int na = 0;
            for (int k = 0; k < w.count && na < kWifiKnown; k++) {
                wchar_t pass[64];
                if (!w.nets[k].known || !IsKnown(w.nets[k].ssid, pass))
                    continue;
                for (int i = 0; i < seen; i++)
                    if (SameSsid(list[i], w.nets[k].ssid)) {
                        wchar_t why[96];
                        if (MakeConfig(list[i], pass, &add[na], why, 96))
                            na++;
                        break;
                    }
            }
            if (na) {
                lastAuto = GetTickCount();
                if (SetPrefs(e, add, na, NULL)) {
                    connectTick = lastAuto;
                    quietTick = QuietStart(quietTick);
                    Log(L"wifi: известные сети отданы WZC: %d (auth %u, шифр %u / %u, 802.1X %d, флаги %X)", na,
                        add[0].authMode, add[0].privacy, add[0].wpaMCastCipher, add[0].enable8021x, add[0].ctlFlags);
                }
            }
        }
        Step(L"WZCRefreshInterface");
        if (scan)
            s_refresh(NULL, INTF_ALL, &e, &outFlags);
        s_delete(&e);

        Step(L"NDISUIO / SDK: связь");
        {   // the connection's steps into the log: up, the address, lost
            DWORD now = GetTickCount();
            if (connected && !linkTick) {
                linkTick = now;
                hadIp = false;
                Log(L"wifi: связь с %s есть, жду адрес", w.ssid);
                LogBeacons(w, w.ssid);   // (its beacon now)
                LogAssoc();
                encLogged = (DWORD)-1;
            }
            if (connected && !hadIp && now - linkTick <= 15000)
                LogEncryption(&encLogged, now - linkTick);
            if (connected && !hadIp && w.ip[0]) {
                hadIp = true;
                Log(L"wifi: адрес %S получен за %u с", w.ip, (unsigned)((now - linkTick) / 1000));
            }
            if (!connected && linkTick) {
                // The driver's own verdict at the drop (WDK_DMW_GetStaConnFailReason: the client's mode, one byte
                // - Ghidra 2026-11-06) and the key's state at the same moment: a failed 4-way handshake has no
                // key, a scan that took the radio away has it
                wchar_t why[48];
                if (s_dkFail) {
                    BYTE code = 0;
                    LONG hr = s_dkFail(DK_STA, &code);
                    _snwprintf(why, 48, L", причина %u (0x%08X)", (unsigned)code, (unsigned)hr);
                } else
                    why[0] = 0;
                Log(L"wifi: связь потеряна через %u с%s (уровень %d дБм%s)", (unsigned)((now - linkTick) / 1000),
                    hadIp ? L"" : L", адреса не было", w.rssi, why);
                LogEncryption(&encLogged, now - linkTick);
                linkTick = 0;
            }
            if (quietTick && (hadIp || now - quietTick > kQuietMs)) {
                RefreshQuiet(false);   // the address is there (or the attempt is over): the periodic scan back
                quietTick = 0;
            }
        }
        w.state = connected ? WIFI_CONNECTED : connectTick && GetTickCount() - connectTick < 30000 ?
                  WIFI_CONNECTING : WIFI_DISCONNECTED;
        // the hand-over: no known network for a while (a phone's hotspot on 5 GHz is not seen) -> the access point
        if (connected) {
            if (handover)
                Log(L"wifi: подключено к точке телефона %s", w.ssid);
            handover = false;
            attempts = 0;
            staTick = GetTickCount();
        } else if (s_dkSetMode && s_apAuto && !s_wantAp) {
            DWORD now = GetTickCount();
            bool knownSeen = false;
            for (int k = 0; k < w.count; k++)
                knownSeen = knownSeen || w.nets[k].known;
            // WZC joins it; the BT modem online - no access point of ours meanwhile (HU 2026-11-05: the phone
            // joined it and dropped PAN, which then came back only after pairing anew)
            if ((knownSeen && !handover) || s_quiet)
                staTick = now;
            if (now - staTick > (DWORD)(handover ? kStaWaitMs : kAutoIdleMs) && (!giveUp || now - giveUp > kRetryMs)) {
                if (attempts >= kAttempts) {
                    Log(L"wifi: точка телефона не нашлась после %d попыток — пауза 10 мин", attempts);
                    giveUp = now;
                    attempts = 0;
                    handover = false;
                } else {
                    attempts++;
                    giveUp = 0;
                    autoAp = true;
                    Log(L"wifi: известных сетей нет %u с — точка ГУ для телефона (попытка %d)",
                        (unsigned)((now - staTick) / 1000), attempts);
                }
                staTick = now;
            }
        }
        Step(L"адрес (iphlpapi)");
        if (connected)
            ReadIp(w.ip);
        else
            w.ip[0] = 0;
        if (connected && !wcsncmp(w.msg, L"подключение", 11))
            w.msg[0] = 0;   // done
        w.scanning = scan;
        PublishAt(w, gen);
        bool watch = linkTick && !hadIp && GetTickCount() - linkTick < 15000;   // the handshake: each second
        PollWait(scan ? 3000 : watch ? 1000 : connectTick && GetTickCount() - connectTick < 30000 ? 2000 : 5000);
    }
#else
    // PC test build: no WZC; three networks to try the screens, "connecting" takes 2 s
    static const wchar_t* kNames[3] = { L"Keenetic-4213", L"AndroidAP", L"Vesta_Garage" };
    static const int kRssi[3] = { -52, -68, -80 }, kSec[3] = { WSEC_WPA2, WSEC_WPA2, WSEC_OPEN };
    wcscpy(w.adapter, L"ПК: имитация");
    s_dkReady = true;   // (no SDK: the switch acts at once)
    SetEvent(s_wake);
    DWORD connectTick = 0;
    while (!s_stop) {
        LONG gen = s_radioGen;
        if (!s_radioOn) {   // (the switch shows it)
            w.ssid[0] = 0;
            PollWait(1000);
            continue;
        }
        w.count = 3;
        for (int i = 0; i < w.count; i++) {
            wcscpy(w.nets[i].ssid, kNames[i]);
            w.nets[i].rssi = kRssi[i];
            w.nets[i].sec = kSec[i];
            w.nets[i].ch = 1 + i * 5;
            w.nets[i].known = IsKnown(kNames[i], NULL);
        }
        EnterCriticalSection(&s_cs);
        if (s_connReq) {
            wcscpy(w.ssid, s_connSsid);
            connectTick = GetTickCount();
            _snwprintf(w.msg, 96, L"подключение к %s…", s_connSsid);
        }
        if (s_forgetReq && !wcscmp(s_forgetSsid, w.ssid)) {
            w.ssid[0] = 0;
            connectTick = 0;
        }
        bool scan = s_scanReq;
        s_scanReq = s_connReq = s_forgetReq = false;
        LeaveCriticalSection(&s_cs);
        static DWORD s_apTick;
        if (s_wantAp) {
            if (!s_apTick)
                s_apTick = GetTickCount();
            w.state = WIFI_AP;
            w.apClients = GetTickCount() - s_apTick > 5000 ? 1 : 0;
            w.apAuto = false;
            w.count = 0;
            PublishAt(w, gen);
            PollWait(1000);
            continue;
        }
        s_apTick = 0;
        w.state = !w.ssid[0] ? WIFI_DISCONNECTED : GetTickCount() - connectTick < 2000 ? WIFI_CONNECTING : WIFI_CONNECTED;
        w.rssi = w.state == WIFI_CONNECTED ? -52 : 0;
        strcpy(w.ip, w.state == WIFI_CONNECTED ? "192.168.1.57" : "");
        if (w.state == WIFI_CONNECTED && !wcsncmp(w.msg, L"подключение", 11))
            w.msg[0] = 0;
        w.scanning = scan;
        PublishAt(w, gen);
        PollWait(1000);
    }
#endif
    return 0;
}

// The switch (see s_wake): the radio on / off by RadioStep, never waiting on the poll - only on its own SetMode, bounded
// (RadioGuarded). Each change of its radio is a new s_radioGen: the poll's round begun before it is void (PublishAt,
// DkModeAt) and starts anew. Off: s_radioOn false first, then SetMode(0); on: SetMode(2) first, then s_radioOn.
// The switch's step (RadioStep's answer not RADIO_KEEP): the radio off or on; false - on too soon, the 30 s shown
static bool SwitchStep(int step, WifiInfo& w, bool& radioOn, DWORD& offTick)
{
    if (step == RADIO_GO_ON && RadioOnTooSoon(w, offTick))
        return false;
    radioOn = step == RADIO_GO_ON;
    if (!radioOn) {   // (SetMode(0) takes up to 10 s: the line says so meanwhile)
        s_radioOn = false;
        w.state = WIFI_OFF;
        wcscpy(w.msg, L"выключаю Wi-Fi…");
    }
    InterlockedIncrement((LONG*)&s_radioGen);
    if (!radioOn)
        Publish(w);
    RadioGuarded(radioOn, -1);
    if (!radioOn)
        offTick = GetTickCount();
    s_radioOn = radioOn;
    SetEvent(s_pollWake);
    return true;
}

static DWORD WINAPI SwitchThread(LPVOID offsAtStart)
{
    LONG offsSeen = (LONG)(size_t)offsAtStart;   // the switch-offs acted on
    while (!s_stop && !s_dkReady)   // the SDK looked for first (WifiBoot): SetMode is its
        WaitForSingleObject(s_wake, 1000);
    Log(L"поток выключателя Wi-Fi: id %08X", (unsigned)GetCurrentThreadId());
    WifiInfo w;
    memset(&w, 0, sizeof(w));
    bool radioOn = true;   // the unit boots with it on
    DWORD offTick = 0;     // when the switch put the radio off
    while (!s_stop) {
        int step = RadioStep(s_wantOn, radioOn, (unsigned)s_offs, (unsigned)offsSeen);
        if (step != RADIO_KEEP) {
            LONG offs = s_offs;   // (off: this one done; on: the ones while it was off do not count)
            if (!SwitchStep(step, w, radioOn, offTick))
                continue;
            offsSeen = offs;
        }
        if (!radioOn) {
            w.state = WIFI_OFF;
            w.msg[0] = 0;
            Publish(w);
        }
        WaitForSingleObject(s_wake, 10000);
    }
    return 0;
}

void WifiStart(HWND notify, UINT msg, WifiLogFn log)
{
    if (s_thread)
        return;
    InitializeCriticalSection(&s_cs);
    InitializeCriticalSection(&s_modeCs);
    s_notify = notify;
    s_msg = msg;
    s_log = log;
    s_info.state = WIFI_NOWZC;   // (until the poll's first word)
    wcscpy(s_info.msg, L"запуск Wi-Fi…");
    s_wake = CreateEvent(NULL, FALSE, FALSE, NULL);
    s_pollWake = CreateEvent(NULL, FALSE, FALSE, NULL);
    s_thread = CreateThread(NULL, 0, WifiThread, NULL, 0, NULL);
    s_switch = CreateThread(NULL, 0, SwitchThread, (LPVOID)(size_t)s_offs, 0, NULL);
}

void WifiStop()
{
    if (!s_thread)
        return;
    s_stop = true;
    SetEvent(s_wake);
    SetEvent(s_pollWake);
    HANDLE both[2] = { s_thread, s_switch };
    for (int i = 0; i < 2; i++)
        if (both[i]) {
            WaitForSingleObject(both[i], 3000);
            CloseHandle(both[i]);
        }
    s_thread = s_switch = NULL;
}

void WifiWatch()
{
    const wchar_t* step = s_step;
    DWORD since = s_stepTick, now = GetTickCount();
    LONG no = s_stepNo;
    if (!s_thread || !StallDue(step != NULL, since, now, (unsigned)no, (unsigned)s_stallLogged))
        return;
    s_stallLogged = no;
    Log(L"wifi: поток Wi-Fi стоит %u с в %s", (unsigned)((now - since) / 1000), step);
}

void WifiApSetup(const wchar_t* ssid, const wchar_t* pass, bool autoOn)
{
    if (s_thread)
        EnterCriticalSection(&s_cs);
    WideCharToMultiByte(CP_UTF8, 0, ssid, -1, s_apSsid, 33, NULL, NULL);
    s_apSsid[32] = 0;
    wcsncpy(s_apSsidW, ssid, 32);
    wcsncpy(s_apPassW, pass, 64);
    WideCharToMultiByte(CP_UTF8, 0, pass, -1, s_apPass, 65, NULL, NULL);
    s_apPass[64] = 0;
    if (s_thread)
        LeaveCriticalSection(&s_cs);
    s_apAuto = autoOn;
    if (s_thread)
        SetEvent(s_pollWake);
}

void WifiApManual(bool on)
{
    s_wantAp = on;
    if (s_thread)
        SetEvent(s_pollWake);
}

void WifiEnable(bool on)
{
    Log(L"wifi: выключатель %s", on ? L"вкл." : L"выкл.");
    if (!on)
        InterlockedIncrement((LONG*)&s_offs);   // (CE's takes no volatile)
    s_wantOn = on;
    if (s_thread) {
        SetEvent(s_wake);
        SetEvent(s_pollWake);   // (the start's 3 s cut short)
    }
}

void WifiGet(WifiInfo* out)
{
    if (!s_thread) {
        memset(out, 0, sizeof(*out));
        return;
    }
    EnterCriticalSection(&s_cs);
    *out = s_info;
    wcscpy(out->apSsid, s_apSsidW);
    wcscpy(out->apPass, s_apPassW);
    LeaveCriticalSection(&s_cs);
}

void WifiScan()
{
    if (!s_thread)
        return;
    EnterCriticalSection(&s_cs);
    s_scanReq = true;
    LeaveCriticalSection(&s_cs);
    SetEvent(s_pollWake);
}

void WifiConnect(const wchar_t* ssid, const wchar_t* pass)
{
    if (!s_thread)
        return;
    EnterCriticalSection(&s_cs);
    wcsncpy(s_connSsid, ssid, 32);
    s_connSsid[32] = 0;
    wcsncpy(s_connPass, pass, 63);
    s_connPass[63] = 0;
    s_connReq = true;
    LeaveCriticalSection(&s_cs);
    SetEvent(s_pollWake);
}

void WifiForget(const wchar_t* ssid)
{
    if (!s_thread)
        return;
    EnterCriticalSection(&s_cs);
    for (int i = 0; i < s_knownCount; i++)
        if (!wcscmp(s_known[i].ssid, ssid)) {
            s_known[i] = s_known[--s_knownCount];
            break;
        }
    wcsncpy(s_forgetSsid, ssid, 32);
    s_forgetSsid[32] = 0;
    s_forgetReq = true;
    LeaveCriticalSection(&s_cs);
    SetEvent(s_pollWake);
}

void WifiRemember(const wchar_t* ssid, const wchar_t* pass)
{
    if (!s_thread || !ssid[0])
        return;
    EnterCriticalSection(&s_cs);
    int i = 0;
    while (i < s_knownCount && wcscmp(s_known[i].ssid, ssid))
        i++;
    if (i < kWifiKnown) {
        wcsncpy(s_known[i].ssid, ssid, 32);
        s_known[i].ssid[32] = 0;
        wcsncpy(s_known[i].pass, pass, 63);
        s_known[i].pass[63] = 0;
        if (i == s_knownCount)
            s_knownCount++;
    }
    LeaveCriticalSection(&s_cs);
}
