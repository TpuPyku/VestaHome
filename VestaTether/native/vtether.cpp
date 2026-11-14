// VestaTether.dll - a phone's USB tethering, step 1 (2026-11-06, the user: "начни модем, если не будет конфликтовать с wifi
// BT драйверами"): a probe, no network adapter yet. A USB client driver (USBD loads it in device.exe for the keys
// TetherProbe.exe writes, until the unit restarts): it logs what the phone offers, and for an RNDIS phone goes through
// INITIALIZE, the queries, the packet filter and asks its DHCP server for an address over the data endpoints. So it shows
// whether a phone works on this unit before any NDIS is written, and it cannot touch Wi-Fi or Bluetooth: it binds nothing
// to TCP/IP, and takes no device with a driver of its own. Log: VestaLogs\VestaTether.log. Imports COREDLL only.
#include <windows.h>
#include <usbdi.h>
#define USBCLI_LOG L"\\VestaTether.log"
#include "../../VestaModem/native/usbcli.h"
#include "rndis.h"
#include "adb.h"

enum { kTransfer = 16384, kCtlSize = 1025, kAskTries = 30, kDhcpTries = 4 };
static DWORD s_askMs = 100;     // between reads of the phone's answers
static DWORD s_dhcpMs = 2000;   // one try's wait

// How far it came, for the log's last line (and the tests)
enum { STAGE_NONE, STAGE_PIPES, STAGE_INIT, STAGE_FILTER, STAGE_DHCP, STAGE_LEASE };
static const wchar_t* const kStage[] = { L"нет", L"открытие каналов", L"INITIALIZE", L"фильтр пакетов", L"DHCP", L"адрес получен" };

struct Dev {
    USB_HANDLE       h;
    LPCUSB_FUNCS     f;
    volatile bool    gone;
    LPCUSB_INTERFACE ctl, data;
    USB_PIPE         in, out;
    HANDLE           thread;
    u32              req;
    u8               mac[6];
    int              stage;
    u8               ctlBuf[kCtlSize];
    u8               buf[kTransfer];
};

static Dev* volatile s_dev;   // one phone at a time
static int s_lastStage;       // the last probe's
static DhcpLease s_lastLease;

static bool IsRndisClass(int cls, int sub, int proto)
{
    return (cls == 0xE0 && sub == 1 && proto == 3) || (cls == 0xEF && sub == 4 && proto == 1);
}

static bool HasBulkPair(LPCUSB_INTERFACE itf)
{
    return FindEp(itf, true) && FindEp(itf, false);
}

// The control interface and the CDC data one with its pair (data alternate settings without endpoints passed over)
static bool FindRndis(LPCUSB_CONFIGURATION cfg, LPCUSB_INTERFACE* ctl, LPCUSB_INTERFACE* data)
{
    *ctl = *data = NULL;
    for (DWORD i = 0; cfg && i < cfg->dwNumInterfaces; i++) {
        LPCUSB_INTERFACE itf = ItfAt(cfg, i);
        const USB_INTERFACE_DESCRIPTOR& d = itf->Descriptor;
        if (!*ctl && IsRndisClass(d.bInterfaceClass, d.bInterfaceSubClass, d.bInterfaceProtocol))
            *ctl = itf;
        else if (!*data && d.bInterfaceClass == 0x0A && HasBulkPair(itf))
            *data = itf;
    }
    return *ctl && *data;
}

// What else a phone's tethering may be, for the log: this probe speaks RNDIS only
static const wchar_t* OtherKind(LPCUSB_CONFIGURATION cfg)
{
    for (DWORD i = 0; cfg && i < cfg->dwNumInterfaces; i++) {
        const USB_INTERFACE_DESCRIPTOR& d = ItfAt(cfg, i)->Descriptor;
        if (d.bInterfaceClass == 2 && d.bInterfaceSubClass == 0x0D)
            return L"CDC NCM";
        if (d.bInterfaceClass == 2 && d.bInterfaceSubClass == 6)
            return L"CDC ECM";
    }
    return L"не сетевое: на телефоне включите «Отладка по USB» (модем включит ГУ) или «USB-модем»";
}

// One class request on the control interface, blocking; *got - bytes moved
static bool Control(Dev* d, bool in, BYTE request, void* buf, WORD len, DWORD* got)
{
    USB_DEVICE_REQUEST r;
    r.bmRequestType = (BYTE)((in ? USB_REQUEST_DEVICE_TO_HOST : USB_REQUEST_HOST_TO_DEVICE) | USB_REQUEST_CLASS | USB_REQUEST_FOR_INTERFACE);
    r.bRequest = request;
    r.wValue = 0;
    r.wIndex = d->ctl->Descriptor.bInterfaceNumber;
    r.wLength = len;
    USB_TRANSFER t = d->f->lpIssueVendorTransfer(d->h, NULL, NULL, (in ? USB_IN_TRANSFER : USB_OUT_TRANSFER) | USB_SEND_TO_INTERFACE |
                                                 USB_SHORT_TRANSFER_OK, &r, buf, 0);
    if (!t)
        return false;
    DWORD err = USB_NO_ERROR;
    bool ok = d->f->lpGetTransferStatus(t, got, &err) && err == USB_NO_ERROR;
    d->f->lpCloseTransfer(t);
    return ok;
}

// What came instead of the answer: a keep-alive is answered, the rest (status notes, a lone zero byte) only logged
static void OtherMessage(Dev* d, DWORD got)
{
    u8 reply[16];
    int k = RndisKeepAliveDone(reply, sizeof(reply), d->ctlBuf, (int)got);
    DWORD sent;
    if (k)
        Control(d, false, 0, reply, (WORD)k, &sent);
    else
        Log(L"телефон: пропущено %u Б, тип %08X", got, RndisGet32(d->ctlBuf));
}

// One read of the phone's queue: true - the answer to msg, in ctlBuf
static bool ReadAnswer(Dev* d, const u8* msg, int* n)
{
    DWORD got = 0;
    if (!Control(d, true, 1, d->ctlBuf, kCtlSize, &got) || !got)
        return false;
    *n = (int)got;
    if (RndisAnswers(msg, d->ctlBuf, *n))
        return true;
    OtherMessage(d, got);
    return false;
}

// A message sent, its answer waited for (SEND_ENCAPSULATED_COMMAND, GET_ENCAPSULATED_RESPONSE). The answer in ctlBuf, *n its bytes.
static bool Exchange(Dev* d, const u8* msg, int size, int* n)
{
    DWORD got;
    if (!Control(d, false, 0, (void*)msg, (WORD)size, &got))
        return false;
    for (int i = 0; i < kAskTries && !d->gone; i++) {
        if (ReadAnswer(d, msg, n))
            return true;
        Sleep(s_askMs);
    }
    return false;
}

static bool InitPhone(Dev* d)
{
    u8 m[32];
    int k = RndisInit(m, sizeof(m), ++d->req, kTransfer), n;
    RndisInitInfo info;
    if (!Exchange(d, m, k, &n) || !RndisInitDone(d->ctlBuf, n, d->req, &info))
        return false;
    Log(L"RNDIS %u.%u, среда %u, пакетов за передачу %u, передача до %u Б, выравнивание %u", info.major, info.minor,
        info.medium, info.maxPackets, info.maxTransfer, info.align);
    return true;
}

static bool Ask(Dev* d, u32 oid, const u8** data, int* len)
{
    u8 m[32];
    int k = RndisQuery(m, sizeof(m), ++d->req, oid), n;
    return Exchange(d, m, k, &n) && RndisQueryDone(d->ctlBuf, n, d->req, data, len);
}

static u32 Word32(const u8* p, int len)
{
    return len >= 4 ? RndisGet32(p) : 0;
}

// What the phone tells of itself; its address needed for DHCP
static bool AskPhone(Dev* d)
{
    const u8* v;
    int len;
    if (!Ask(d, RNDIS_OID_CURRENT_ADDRESS, &v, &len) || len < 6) {
        Log(L"адрес телефона не получен");
        return false;
    }
    memcpy(d->mac, v, 6);
    Log(L"адрес %02X:%02X:%02X:%02X:%02X:%02X", v[0], v[1], v[2], v[3], v[4], v[5]);
    if (Ask(d, RNDIS_OID_MEDIA_CONNECT, &v, &len))
        Log(L"связь: %u (0 есть, 1 нет)", Word32(v, len));
    if (Ask(d, RNDIS_OID_LINK_SPEED, &v, &len))
        Log(L"скорость: %u кбит/с", Word32(v, len) / 10);
    if (Ask(d, RNDIS_OID_MAX_FRAME, &v, &len))
        Log(L"наибольший кадр: %u Б", Word32(v, len));
    return true;
}

static bool SetFilter(Dev* d)
{
    u8 m[40];
    int k = RndisSet32(m, sizeof(m), ++d->req, RNDIS_OID_PACKET_FILTER, RNDIS_FILTER_NORMAL), n;
    return Exchange(d, m, k, &n) && RndisSetDone(d->ctlBuf, n, d->req);
}

static bool SendDiscover(Dev* d, u32 xid)
{
    u8 frame[400], m[460];
    int f = DhcpDiscover(frame, sizeof(frame), d->mac, xid), k = RndisWrap(m, sizeof(m), frame, f);
    DWORD got;
    return k && Bulk(d, d->out, false, m, (DWORD)k, &got, s_dhcpMs);
}

// The transfers within a try: the lease when an OFFER is among their frames
static bool ReadOffer(Dev* d, u32 xid, DhcpLease* lease)
{
    DWORD t0 = GetTickCount();
    while (GetTickCount() - t0 < s_dhcpMs && !d->gone) {
        DWORD got = 0;
        if (!Bulk(d, d->in, true, d->buf, kTransfer, &got, s_dhcpMs))
            return false;
        int pos = 0, len;
        const u8* frame;
        while (RndisNext(d->buf, (int)got, &pos, &frame, &len))
            if (DhcpOffer(frame, len, xid, lease))
                return true;
    }
    return false;
}

static bool AskLease(Dev* d, DhcpLease* lease)
{
    for (int i = 0; i < kDhcpTries && !d->gone; i++) {
        u32 xid = GetTickCount() ^ 0x56455354 ^ (u32)i;
        if (SendDiscover(d, xid) && ReadOffer(d, xid, lease))
            return true;
        Log(L"DHCP: нет ответа, попытка %d", i + 1);
    }
    return false;
}

static void LogLease(const DhcpLease& l)
{
    Log(L"DHCP: адрес %u.%u.%u.%u, маска %u.%u.%u.%u", l.ip[0], l.ip[1], l.ip[2], l.ip[3], l.mask[0], l.mask[1], l.mask[2], l.mask[3]);
    Log(L"DHCP: шлюз %u.%u.%u.%u, DNS %u.%u.%u.%u, сервер %u.%u.%u.%u", l.gateway[0], l.gateway[1], l.gateway[2], l.gateway[3],
        l.dns[0], l.dns[1], l.dns[2], l.dns[3], l.server[0], l.server[1], l.server[2], l.server[3]);
}

static bool OpenPipes(Dev* d)
{
    d->in = d->f->lpOpenPipe(d->h, &FindEp(d->data, true)->Descriptor);
    d->out = d->f->lpOpenPipe(d->h, &FindEp(d->data, false)->Descriptor);
    return d->in && d->out;
}

// The steps in turn; d->stage - the one reached
static void Steps(void* p)
{
    Dev* d = (Dev*)p;
    DhcpLease lease;
    d->stage = STAGE_PIPES;
    if (!OpenPipes(d))
        return;
    d->stage = STAGE_INIT;
    if (!InitPhone(d) || !AskPhone(d))
        return;
    d->stage = STAGE_FILTER;
    if (!SetFilter(d))
        return;
    d->stage = STAGE_DHCP;
    if (!AskLease(d, &lease))
        return;
    d->stage = STAGE_LEASE;
    s_lastLease = lease;
    LogLease(lease);
}

// A probe's steps on its thread: a fault in them logged, not device.exe's
static void Guarded(void (*steps)(void*), void* d, const wchar_t* who)
{
    __try {
        steps(d);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log(L"%s: исключение %08X", who, GetExceptionCode());
    }
}

static DWORD WINAPI Probe(LPVOID param)
{
    Dev* d = (Dev*)param;
    Guarded(Steps, d, L"проба");
    s_lastStage = d->stage;
    if (d->stage == STAGE_LEASE)
        Log(L"ПРОБА: успех - телефон раздаёт интернет по USB, можно делать адаптер");
    else
        Log(L"ПРОБА: остановилась на шаге «%s»", kStage[d->stage]);
    return 0;
}

static void FreeDev(Dev* d)
{
    d->gone = true;
    if (d->thread) {
        WaitForSingleObject(d->thread, INFINITE);
        CloseHandle(d->thread);
    }
    if (d->in)
        d->f->lpClosePipe(d->in);
    if (d->out)
        d->f->lpClosePipe(d->out);
    s_dev = NULL;
    free(d);
}

static BOOL WINAPI Notify(LPVOID param, DWORD code, LPDWORD*, LPDWORD*, LPDWORD*, LPDWORD*)
{
    if (code != USB_CLOSE_DEVICE)
        return FALSE;
    Log(L"телефон отключён");
    FreeDev((Dev*)param);
    return TRUE;
}

static void LogPower(LPCUSB_CONFIGURATION cfg)
{
    if (cfg)
        Log(L"  питание: просит %u мА, свойства %02X", cfg->Descriptor.MaxPower * 2u, cfg->Descriptor.bmAttributes);
}

// ---- ADB (2026-11-12, CheryLink's way): a phone in its ordinary mode with "USB debugging" on offers its debugging
// interface (FF/42/01). Its tethering is switched on by one shell command; the phone then comes again as RNDIS, and the
// probe above takes it. This unit's key - TetherProbe's file (its path: HKLM\Software\VestaTether "AdbKey"); the phone
// asks its user once to allow it.

static const char kAdbCmd[] = "shell:svc usb setFunctions rndis 2>&1 || svc usb setFunction rndis 2>&1";
static const char kAdbName[] = "VestaHome@HU";
static DWORD s_adbMs = 5000;      // a phone's answer
static DWORD s_adbAskMs = 60000;  // ... when its user is asked
static wchar_t s_keyPath[MAX_PATH];
static int s_adbEnd = -1;         // the last session's state (the tests)

struct AdbDev {
    USB_HANDLE       h;
    LPCUSB_FUNCS     f;
    volatile bool    gone;
    LPCUSB_INTERFACE itf;
    USB_PIPE         in, out;
    HANDLE           thread;
    AdbKey           key;
    Adb              s;
    u8               hdr[ADB_HEADER];
    u8               data[ADB_MAXDATA + 1];
};

static AdbDev* volatile s_adb;   // one at a time

static bool IsAdb(LPCUSB_INTERFACE itf)
{
    const USB_INTERFACE_DESCRIPTOR& d = itf->Descriptor;
    return d.bInterfaceClass == 0xFF && d.bInterfaceSubClass == 0x42 && d.bInterfaceProtocol == 1 && HasBulkPair(itf);
}

// The debugging interface of a device given whole (a VID_PID key, HU 2026-11-14: the class keys never brought the
// phone to us) - NULL none
static LPCUSB_INTERFACE FindAdb(LPCUSB_CONFIGURATION cfg)
{
    for (DWORD i = 0; cfg && i < cfg->dwNumInterfaces; i++)
        if (IsAdb(ItfAt(cfg, i)))
            return ItfAt(cfg, i);
    return NULL;
}

static bool LoadKey(AdbKey* k)
{
    if (!s_keyPath[0]) {
        HKEY r;
        DWORD size = sizeof(s_keyPath) - sizeof(wchar_t), type;
        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"Software\\VestaTether", 0, 0, &r) == ERROR_SUCCESS) {
            RegQueryValueEx(r, L"AdbKey", NULL, &type, (BYTE*)s_keyPath, &size);
            RegCloseKey(r);
        }
    }
    HANDLE f = CreateFile(s_keyPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return false;
    u8 buf[2000];
    DWORD got = 0;
    ReadFile(f, buf, sizeof(buf), &got, NULL);
    CloseHandle(f);
    return AdbKeyParse(buf, (int)got, k);
}

static bool AdbSend(AdbDev* d, u32 cmd, u32 a0, u32 a1, const void* data, u32 len)
{
    u8 h[ADB_HEADER];
    DWORD got;
    AdbHeader(h, cmd, a0, a1, (const u8*)data, len);
    return Bulk(d, d->out, false, h, ADB_HEADER, &got, s_adbMs) &&
           (!len || Bulk(d, d->out, false, (void*)data, len, &got, s_adbMs));
}

// The phone's next message: its header, then its data (in d->data, ended by a 0)
static bool AdbRead(AdbDev* d, AdbMsg* m, DWORD ms)
{
    DWORD got = 0, total = 0;
    if (!Bulk(d, d->in, true, d->hdr, ADB_HEADER, &got, ms) || !AdbParse(d->hdr, (int)got, m))
        return false;
    while (total < m->len) {
        if (!Bulk(d, d->in, true, d->data + total, m->len - total, &got, s_adbMs) || !got)
            return false;
        total += got;
    }
    d->data[total] = 0;
    return true;
}

static u32 KeyE(const AdbKey& k)
{
    u32 e = 0;
    for (int i = 0; i < k.e.len; i++)
        e = e << 8 | k.e.b[i];
    return e;
}

static void LogOutput(const AdbDev* d, u32 len)   // the command's words (ASCII), one line
{
    char t[160];
    u32 n = 0;
    for (u32 i = 0; i < len && n < sizeof(t) - 1; i++)
        t[n++] = d->data[i] >= ' ' && d->data[i] < 127 ? (char)d->data[i] : ' ';
    t[n] = 0;
    Log(L"ADB: телефон ответил: %S", t);
}

// What the session asks for, done; false - the end
static bool AdbAct(AdbDev* d, int act, const AdbMsg& m)
{
    u8 sig[ADB_KEY_BYTES];
    char pub[800];
    switch (act) {
    case ADB_DO_SIGN:
        return m.len == ADB_TOKEN && AdbSign(&d->key, d->data, sig) &&
               AdbSend(d, ADB_AUTH, ADB_AUTH_SIGNATURE, 0, sig, ADB_KEY_BYTES);
    case ADB_DO_SENDKEY:
        Log(L"ADB: телефон спрашивает «Разрешить отладку по USB?» - разрешите (лучше с галочкой «Всегда»)");
        return AdbSend(d, ADB_AUTH, ADB_AUTH_RSAPUBLICKEY, 0, pub, (u32)AdbPubKey(d->key.n.b, KeyE(d->key), kAdbName, pub, sizeof(pub)));
    case ADB_DO_OPEN:
        Log(L"ADB: подключено, включаю «USB-модем»: %S", kAdbCmd + 6);
        return AdbSend(d, ADB_OPEN, d->s.local, 0, kAdbCmd, sizeof(kAdbCmd));
    case ADB_DO_OKAY:
        LogOutput(d, m.len);
        return AdbSend(d, ADB_OKAY, d->s.local, d->s.remote, NULL, 0);
    case ADB_DO_FAIL:
        Log(L"ADB: ключ ГУ не принят");
        return false;
    case ADB_DO_DONE:
        return false;
    }
    return true;
}

static void AdbSession(AdbDev* d)
{
    static const char kHost[] = "host::";
    AdbStart(&d->s, 1);
    bool on = AdbSend(d, ADB_CNXN, ADB_VERSION, ADB_MAXDATA, kHost, sizeof(kHost));
    while (on && !d->gone) {
        AdbMsg m;
        if (!AdbRead(d, &m, d->s.state == ADB_KEYSENT ? s_adbAskMs : s_adbMs)) {
            Log(L"ADB: телефон не ответил");
            return;
        }
        on = AdbAct(d, AdbOn(&d->s, m), m);
    }
}

static void AdbSteps(void* p)
{
    AdbDev* d = (AdbDev*)p;
    AdbSession(d);
    if (d->s.state == ADB_DONE)
        Log(L"ADB: готово - телефон подключится заново как «USB-модем»");
}

static DWORD WINAPI AdbProbe(LPVOID param)
{
    AdbDev* d = (AdbDev*)param;
    Guarded(AdbSteps, d, L"ADB");
    s_adbEnd = d->s.state;
    return 0;
}

static BOOL WINAPI AdbNotify(LPVOID param, DWORD code, LPDWORD*, LPDWORD*, LPDWORD*, LPDWORD*)
{
    if (code != USB_CLOSE_DEVICE)
        return FALSE;
    AdbDev* d = (AdbDev*)param;
    d->gone = true;
    if (d->thread) {
        WaitForSingleObject(d->thread, INFINITE);
        CloseHandle(d->thread);
    }
    if (d->in)
        d->f->lpClosePipe(d->in);
    if (d->out)
        d->f->lpClosePipe(d->out);
    s_adb = NULL;
    free(d);
    return TRUE;
}

// The debugging interface taken when this unit has its key
static void StartAdb(USB_HANDLE h, LPCUSB_FUNCS f, LPCUSB_INTERFACE itf, LPBOOL accept)
{
    AdbDev* d = (AdbDev*)calloc(1, sizeof(AdbDev));
    if (!d)
        return;
    if (!LoadKey(&d->key)) {
        Log(L"ADB: нет ключа ГУ - сначала запустите TetherProbe.exe");
        free(d);
        return;
    }
    d->h = h;
    d->f = f;
    d->itf = itf;
    d->in = f->lpOpenPipe(h, &FindEp(itf, true)->Descriptor);
    d->out = f->lpOpenPipe(h, &FindEp(itf, false)->Descriptor);
    s_adb = d;
    f->lpRegisterNotificationRoutine(h, AdbNotify, d);
    *accept = TRUE;
    d->thread = d->in && d->out ? CreateThread(NULL, 0, AdbProbe, d, 0, NULL) : NULL;
}

static void StartRndis(USB_HANDLE h, LPCUSB_FUNCS f, LPCUSB_INTERFACE ctl, LPCUSB_INTERFACE data, LPBOOL accept)
{
    Dev* d = (Dev*)calloc(1, sizeof(Dev));
    if (!d)
        return;
    d->h = h;
    d->f = f;
    d->ctl = ctl;
    d->data = data;
    s_dev = d;
    f->lpRegisterNotificationRoutine(h, Notify, d);
    *accept = TRUE;
    d->thread = CreateThread(NULL, 0, Probe, d, 0, NULL);
}

// Each interface without a driver of its own comes here: an RNDIS phone - the probe; else its debugging interface - ADB
extern "C" BOOL USBDeviceAttach(USB_HANDLE h, LPCUSB_FUNCS f, LPCUSB_INTERFACE itf, LPCWSTR, LPBOOL accept, LPCUSB_DRIVER_SETTINGS, DWORD)
{
    static USB_HANDLE s_seen;   // (a device logged once, not for each of its interfaces)
    *accept = FALSE;
    LPCUSB_DEVICE dev = f->lpGetDeviceInfo(h);
    if (!dev)
        return TRUE;
    bool first = h != s_seen;
    s_seen = h;
    if (first) {
        LogDevice(dev);
        LogPower(dev->lpActiveConfig);
    }
    LPCUSB_INTERFACE ctl, data;
    if (!FindRndis(dev->lpActiveConfig, &ctl, &data)) {
        LPCUSB_INTERFACE adb = itf ? (IsAdb(itf) ? itf : NULL) : FindAdb(dev->lpActiveConfig);
        if (adb && !s_adb)
            StartAdb(h, f, adb, accept);
        else if (first)
            Log(L"RNDIS нет: %s", OtherKind(dev->lpActiveConfig));
        return TRUE;
    }
    if (!s_dev)   // (the second interface of the same phone comes here too)
        StartRndis(h, f, ctl, data, accept);
    return TRUE;
}

extern "C" BOOL USBInstallDriver(LPCWSTR)   // (the keys are TetherProbe's)
{
    return TRUE;
}

extern "C" BOOL USBUnInstallDriver()
{
    return TRUE;
}
