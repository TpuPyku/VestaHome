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
       WZCCTL_WEPK_PRESENT = 1, WZCCTL_WEPK_XFORMAT = 2,
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
static HANDLE     s_wake, s_thread;
static HWND       s_notify;
static UINT       s_msg;
static WifiLogFn  s_log;
static wchar_t    s_guid[64];          // the adapter
static volatile bool s_wantOn = true;  // WifiEnable
static volatile bool s_wantAp, s_apAuto;   // WifiApManual, WifiApSetup
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
static DkModeFn    s_dkSetMode;
static DkGetModeFn s_dkGetMode;
static DkApFn      s_dkApGetDef, s_dkApSetDef, s_dkApSetCur;
static DkAssocFn   s_dkAssoc;
static DelArrFn    s_delArr;      // coredll's operator delete[]: the association list is the DLL's new[]
enum { DK_OFF = 0, DK_AP = 1, DK_STA = 2,
       APS_SIZE = 0x140, APS_SSID = 0x04, APS_BEACON = 0x28, APS_CHANNEL = 0x30, APS_CIPHER = 0x34, APS_WPA = 0x38,
       APS_WEPKEYID = 0x40, APS_KEY = 0xB0, APS_HIDE = 0x134, APS_AUTHTO = 0x138, APS_ASSOCTO = 0x13C,
       CIPHER_AES = 0x20, WPA_MODE2 = 2, kApChannel = 6,
       kAutoIdleMs = 30000,    // not connected, no known network in sight that long: the access point
       kApWaitMs = 180000,     // no phone joins it: back to a client
       kHandoverMs = 20000,    // a phone joined: time for it to raise its hotspot (on 2.4 GHz now)
       kStaWaitMs = 40000,     // back as a client: the phone's hotspot not joined that long - again
       kAttempts = 3, kRetryMs = 600000 };

static void Log(const wchar_t* fmt, ...)
{
    if (!s_log)
        return;
    wchar_t s[200];
    va_list a;
    va_start(a, fmt);
    _vsnwprintf(s, 200, fmt, a);
    va_end(a);
    s[199] = 0;
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
        c->ctlFlags = WZCCTL_WEPK_PRESENT | WZCCTL_WEPK_XFORMAT;
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

static void ReadIp(char* ip)
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
        char guid[64];
        WideCharToMultiByte(CP_ACP, 0, s_guid, -1, guid, sizeof(guid), NULL, NULL);
        for (AdapterInfo* p = a; p; p = p->next)
            if (!_stricmp(p->name, guid) && strcmp(p->ipList.ip, "0.0.0.0")) {
                strncpy(ip, p->ipList.ip, 15);
                ip[15] = 0;
            }
    }
    free(a);
}
#endif

static void Publish(const WifiInfo& w)
{
    EnterCriticalSection(&s_cs);
    bool changed = memcmp(&w, &s_info, sizeof(w)) != 0;
    s_info = w;
    LeaveCriticalSection(&s_cs);
    if (changed && s_notify)
        PostMessage(s_notify, s_msg, 0, 0);
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
    if (!init || !s_dkSetMode || !s_dkGetMode || !s_dkApGetDef || !s_dkApSetDef || !s_dkApSetCur || !s_dkAssoc) {
        s_dkSetMode = NULL;
        return false;
    }
    LONG hr = init();
    int mode = -1;
    s_dkGetMode(&mode);
    Log(L"wifi: WifiDKCore, InitApi 0x%08X, режим %d", (unsigned)hr, mode);
    if (hr < 0)
        s_dkSetMode = NULL;
    s_dkBootMode = mode;
    return s_dkSetMode != NULL;
}

static bool DkMode(int mode)
{
    LONG hr = s_dkSetMode(mode);
    Log(L"wifi: режим %s (SetMode 0x%08X)", mode == DK_AP ? L"точка" : mode == DK_STA ? L"клиент" : L"выкл.",
        (unsigned)hr);
    return hr >= 0;
}

// The access point with our network: its settings as the adapter's defaults (the registry, read when it starts)
// and, once started, as its current ones.
static bool ApStart()
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
    // the firmware's registry has the access point's radio off (Comm\ndcufmp1\Parms EnableRadio = 0; HU 2026-10-18-2:
    // every call answered S_OK, no phone saw the network); the adapter reads it when it starts. Reset on boot anyway.
    HKEY key;
    DWORD one = 1, rr = (DWORD)-1;
    if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, L"Comm\\ndcufmp1\\Parms", 0, NULL, 0, 0, NULL, &key, NULL) == ERROR_SUCCESS) {
        rr = RegSetValueEx(key, L"EnableRadio", 0, REG_DWORD, (const BYTE*)&one, sizeof(one));
        RegCloseKey(key);
    }
    Log(L"wifi: точка: EnableRadio=1 (%lu)", rr);
    if (!DkMode(DK_AP))
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
static void RadioPower(bool on)
{
#ifdef UNDER_CE
    if (s_dkSetMode) {
        DkMode(on ? DK_STA : DK_OFF);
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
#endif
}

static DWORD WINAPI WifiThread(LPVOID)
{
    Log(L"поток Wi-Fi: id %08X", (unsigned)GetCurrentThreadId());
    WifiInfo w;
    memset(&w, 0, sizeof(w));
#ifdef UNDER_CE
    if (!LoadWzc()) {
        w.state = WIFI_NOWZC;
        wcscpy(w.msg, L"нет службы Wi-Fi (wzcsapi.dll)");
        Publish(w);
        return 0;
    }
    // Without the stock shell (HU 2026-10-27: "режим 0", no Wi-Fi at all, not even the system's manager) the
    // adapters stay off: the shell switched the client on at its start. Then VestaHome does it.
    if (LoadDk() && s_dkBootMode == DK_OFF && s_wantOn)
        DkMode(DK_STA);
    DWORD linkTick = 0;    // the link up without an address yet: since when (the log of a slow phone's hotspot)
    bool hadIp = false;
    DWORD lastAuto = 0, connectTick = 0;
    bool haveAdapter = false, radioOn = true;   // the unit boots with it on
    // the access point: ap = it is up; autoAp = raised by the hand-over (s_apAuto), not by hand (s_wantAp)
    bool ap = false, autoAp = false, handover = false;
    DWORD apTick = 0, assocTick = 0, staTick = GetTickCount(), giveUp = 0;
    int attempts = 0;
    while (!s_stop) {
        if (s_wantOn != radioOn) {
            radioOn = s_wantOn;
            RadioPower(radioOn);
            haveAdapter = false;
            connectTick = lastAuto = 0;
            ap = autoAp = handover = false;
            staTick = GetTickCount();
        }
        if (!radioOn) {
            w.state = WIFI_OFF;
            w.count = 0;
            w.ssid[0] = 0;
            w.ip[0] = 0;
            w.msg[0] = 0;
            Publish(w);
            WaitForSingleObject(s_wake, 10000);
            continue;
        }
        if (s_dkSetMode) {
            DWORD now = GetTickCount();
            if (!s_apAuto)
                autoAp = false;
            bool wantAp = s_wantAp || autoAp;
            if (wantAp && !ap) {
                ap = ApStart();
                apTick = now;
                assocTick = 0;
                if (!ap) {   // could not: no more tries for now
                    s_wantAp = autoAp = false;
                    giveUp = now;
                    DkMode(DK_STA);
                    wcscpy(w.msg, L"точка ГУ не включилась");
                }
            } else if (!wantAp && ap) {
                DkMode(DK_STA);
                ap = false;
                haveAdapter = false;
                staTick = now;
                connectTick = lastAuto = 0;
            }
            if (ap) {
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
                Publish(w);
                WaitForSingleObject(s_wake, 2000);
                continue;
            }
        }
        if (!haveAdapter) {
            haveAdapter = FindAdapter(w.adapter, 48);
            if (!haveAdapter) {
                w.state = WIFI_NOADAPTER;
                w.count = 0;
                w.ssid[0] = 0;
                Publish(w);
                WaitForSingleObject(s_wake, 10000);
                continue;
            }
            Log(L"wifi: адаптер %s", w.adapter);
        }
        IntfEntry e;
        IntfInit(&e);
        DWORD outFlags = 0;
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

        // requests from the window
        EnterCriticalSection(&s_cs);
        bool scan = s_scanReq, conn = s_connReq, forget = s_forgetReq;
        wchar_t cs[33], cp[64], fs[33];
        wcscpy(cs, s_connSsid);
        wcscpy(cp, s_connPass);
        wcscpy(fs, s_forgetSsid);
        s_scanReq = s_connReq = s_forgetReq = false;
        LeaveCriticalSection(&s_cs);
        if (forget) {
            SetPrefs(e, NULL, 0, fs);
            _snwprintf(w.msg, 96, L"забыта сеть %s", fs);
        }
        if (conn) {
            int i = 0;
            for (; i < seen; i++)
                if (SameSsid(list[i], cs))
                    break;
            WlanConfig c;
            if (i == seen) {
                _snwprintf(w.msg, 96, L"сети %s не видно", cs);
            } else if (MakeConfig(list[i], cp, &c, w.msg, 96) && SetPrefs(e, &c, 1, NULL)) {
                _snwprintf(w.msg, 96, L"подключение к %s…", cs);
                Log(L"wifi: подключение к %s", cs);
                connectTick = GetTickCount();
                wcscpy(w.ssid, cs);
            }
        }
        // not connected: the known networks in sight go to WZC (the registry was reset on boot)
        if (!connected && !conn && seen && (!lastAuto || GetTickCount() - lastAuto > 60000)) {
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
                    Log(L"wifi: известные сети отданы WZC: %d", na);
                }
            }
        }
        if (scan)
            s_refresh(NULL, INTF_ALL, &e, &outFlags);
        s_delete(&e);

        {   // the connection's steps into the log: up, the address, lost
            DWORD now = GetTickCount();
            if (connected && !linkTick) {
                linkTick = now;
                hadIp = false;
                Log(L"wifi: связь с %s есть, жду адрес", w.ssid);
            }
            if (connected && !hadIp && w.ip[0]) {
                hadIp = true;
                Log(L"wifi: адрес %S получен за %u с", w.ip, (unsigned)((now - linkTick) / 1000));
            }
            if (!connected && linkTick) {
                Log(L"wifi: связь потеряна через %u с%s", (unsigned)((now - linkTick) / 1000),
                    hadIp ? L"" : L", адреса не было");
                linkTick = 0;
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
            if (knownSeen && !handover)
                staTick = now;   // WZC joins it
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
        if (connected)
            ReadIp(w.ip);
        else
            w.ip[0] = 0;
        if (connected && !wcsncmp(w.msg, L"подключение", 11))
            w.msg[0] = 0;   // done
        w.scanning = scan;
        Publish(w);
        WaitForSingleObject(s_wake, scan ? 3000 : connectTick && GetTickCount() - connectTick < 30000 ? 2000 : 5000);
    }
#else
    // PC test build: no WZC; three networks to try the screens, "connecting" takes 2 s
    static const wchar_t* kNames[3] = { L"Keenetic-4213", L"AndroidAP", L"Vesta_Garage" };
    static const int kRssi[3] = { -52, -68, -80 }, kSec[3] = { WSEC_WPA2, WSEC_WPA2, WSEC_OPEN };
    wcscpy(w.adapter, L"ПК: имитация");
    DWORD connectTick = 0;
    while (!s_stop) {
        w.count = s_wantOn ? 3 : 0;
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
        if (!s_wantOn)
            w.ssid[0] = 0;
        static DWORD s_apTick;
        if (s_wantOn && s_wantAp) {
            if (!s_apTick)
                s_apTick = GetTickCount();
            w.state = WIFI_AP;
            w.apClients = GetTickCount() - s_apTick > 5000 ? 1 : 0;
            w.apAuto = false;
            w.count = 0;
            Publish(w);
            WaitForSingleObject(s_wake, 1000);
            continue;
        }
        s_apTick = 0;
        w.state = !s_wantOn ? WIFI_OFF : !w.ssid[0] ? WIFI_DISCONNECTED : GetTickCount() - connectTick < 2000 ? WIFI_CONNECTING : WIFI_CONNECTED;
        w.rssi = w.state == WIFI_CONNECTED ? -52 : 0;
        strcpy(w.ip, w.state == WIFI_CONNECTED ? "192.168.1.57" : "");
        if (w.state == WIFI_CONNECTED && !wcsncmp(w.msg, L"подключение", 11))
            w.msg[0] = 0;
        w.scanning = scan;
        Publish(w);
        WaitForSingleObject(s_wake, 1000);
    }
#endif
    return 0;
}

void WifiStart(HWND notify, UINT msg, WifiLogFn log)
{
    if (s_thread)
        return;
    InitializeCriticalSection(&s_cs);
    s_notify = notify;
    s_msg = msg;
    s_log = log;
    s_wake = CreateEvent(NULL, FALSE, FALSE, NULL);
    s_thread = CreateThread(NULL, 0, WifiThread, NULL, 0, NULL);
}

void WifiStop()
{
    if (!s_thread)
        return;
    s_stop = true;
    SetEvent(s_wake);
    WaitForSingleObject(s_thread, 3000);
    CloseHandle(s_thread);
    s_thread = NULL;
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
        SetEvent(s_wake);
}

void WifiApManual(bool on)
{
    s_wantAp = on;
    if (s_thread)
        SetEvent(s_wake);
}

void WifiEnable(bool on)
{
    s_wantOn = on;
    if (s_thread)
        SetEvent(s_wake);
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
    SetEvent(s_wake);
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
    SetEvent(s_wake);
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
    SetEvent(s_wake);
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
