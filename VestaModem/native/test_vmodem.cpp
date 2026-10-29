// test_vmodem.cpp - the USB modem's driver on the PC over a made-up USBD: a Huawei stick in its CD-ROM mode, then
// the modem (E3121: interface 0 the modem's port, 1 interrupt only, 4 the PC UI) answering AT; its COM port read,
// written and waited on as Unimodem and PPP do; the modem's port silent (the PC UI carries the data); the stick out.
// USBD's arrays are made wider than the SDK's structs, as the unit's are (HU 2026-10-24). The status goes to HKCU
// here (HKLM on the unit), removed at the end.
#include <windows.h>
#include <stdio.h>
#undef HKEY_LOCAL_MACHINE
#define HKEY_LOCAL_MACHINE HKEY_CURRENT_USER

// (CE takes no access rights - samDesired 0; the desktop refuses a key opened so)
static LONG TCreate(HKEY k, LPCWSTR sub, DWORD r, LPWSTR cls, DWORD opt, REGSAM, LPSECURITY_ATTRIBUTES sa, PHKEY out,
                    LPDWORD disp)
{
    return RegCreateKeyExW(k, sub, r, cls, opt, KEY_ALL_ACCESS, sa, out, disp);
}

static LONG TOpen(HKEY k, LPCWSTR sub, DWORD opt, REGSAM, PHKEY out)
{
    return RegOpenKeyExW(k, sub, opt, KEY_READ, out);
}
#undef RegCreateKeyEx
#define RegCreateKeyEx TCreate
#undef RegOpenKeyEx
#define RegOpenKeyEx TOpen

static HANDLE ActivateDeviceEx(LPCWSTR key, LPCVOID, DWORD, LPVOID param);
static BOOL   DeactivateDevice(HANDLE h);
#include "vmodem.cpp"

static int s_fails;

static void Check(bool ok, const char* what)
{
    printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok)
        s_fails++;
}

// ---- the made-up USBD
struct FTr;
struct FPipe {
    BYTE  addr;
    int   itf;
    char  in[4096];   // what the device has to send
    int   inLen;
    char  out[16384]; // what it was sent
    int   outLen;
    FTr*  pending;    // an IN transfer waiting for data
    bool  open;
};
struct FTr {
    FPipe*                    p;
    LPTRANSFER_NOTIFY_ROUTINE cb;
    LPVOID                    param;
    BYTE*                     buf;
    DWORD                     len, n, err;
    bool                      done;
};

static CRITICAL_SECTION s_usbCs;
static FPipe s_pipes[8];
static int   s_pipeCount;
static bool  s_modemAnswers;
static int   s_ctlItf[16], s_ctlBits[16], s_ctlCount;
static LPDEVICE_NOTIFY_ROUTINE s_notify;
static LPVOID s_notifyParam;
static wchar_t s_activated[64];
static int s_deactivated;
static volatile LONG s_live;     // transfers not closed
static int  s_badClose;          // a pipe closed twice
static bool s_failIn;            // IN transfers fail at once (a stalled pipe)
static bool s_failOut;           // OUT transfers fail at once, nothing taken
static volatile LONG s_inIssued;
static bool s_vendorFail;        // the modem refuses SET_CONTROL_LINE_STATE
static bool s_noActive;          // the device manager writes no Drivers\Active key
static bool s_onlyModem;         // a stick with the modem's port only

static FPipe* PipeOf(int itf, bool in)
{
    for (int i = 0; i < s_pipeCount; i++)
        if (s_pipes[i].itf == itf && ((s_pipes[i].addr & 0x80) != 0) == in)
            return &s_pipes[i];
    return NULL;
}

static void Complete(FTr* t)   // (under s_usbCs) the pending IN transfer from what the device has
{
    FPipe* p = t->p;
    DWORD n = (DWORD)p->inLen < t->len ? (DWORD)p->inLen : t->len;
    memcpy(t->buf, p->in, n);
    memmove(p->in, p->in + n, p->inLen - n);
    p->inLen -= n;
    t->n = n;
    t->done = true;
    p->pending = NULL;
}

static void FeedB(int itf, const char* s, int n)   // the device sends n bytes on the interface's IN endpoint
{
    EnterCriticalSection(&s_usbCs);
    FPipe* p = PipeOf(itf, true);
    memcpy(p->in + p->inLen, s, n);
    p->inLen += n;
    FTr* t = p->pending;
    if (t)
        Complete(t);
    LeaveCriticalSection(&s_usbCs);
    if (t)
        t->cb(t->param);
}

static void Feed(int itf, const char* s)
{
    FeedB(itf, s, (int)strlen(s));
}

static DWORD WINAPI LateImsi(LPVOID)   // (the SIM answers after a while)
{
    Sleep(300);
    Feed(4, "\r\n250011234567890\r\n\r\nOK\r\n");
    return 0;
}

static void Answer(int itf, const char* cmd)   // the modem's replies
{
    if (itf == 4 && strstr(cmd, "AT+CIMI"))
        CloseHandle(CreateThread(NULL, 0, LateImsi, NULL, 0, NULL));
    else if (itf == 0 && s_modemAnswers && !strcmp(cmd, "AT\r"))   // longer than a packet: "OK" in the second
        Feed(0, "AT\r\r\n+ZZZZ: 0123456789012345678901234567890123456789012345678901234567890123456789\r\nOK\r\n");
    else if (itf == 4)
        Feed(itf, "\r\nOK\r\n");
}

static USB_TRANSFER FBulk(USB_PIPE pipe, LPTRANSFER_NOTIFY_ROUTINE cb, LPVOID param, DWORD flags, DWORD len,
                          LPVOID buf, ULONG)
{
    FPipe* p = (FPipe*)pipe;
    FTr* t = new FTr;
    memset(t, 0, sizeof(*t));
    InterlockedIncrement(&s_live);
    t->p = p;
    t->cb = cb;
    t->param = param;
    t->buf = (BYTE*)buf;
    t->len = len;
    if (!(flags & USB_IN_TRANSFER) && s_failOut) {
        t->done = true;
        t->err = USB_STALL_ERROR;
        cb(param);
        return t;
    }
    if (!(flags & USB_IN_TRANSFER)) {
        char cmd[128];
        int c = len < sizeof(cmd) - 1 ? (int)len : (int)sizeof(cmd) - 1;
        memcpy(cmd, buf, c);
        cmd[c] = 0;
        EnterCriticalSection(&s_usbCs);
        memcpy(p->out + p->outLen, buf, len);
        p->outLen += len;
        t->n = len;
        t->done = true;
        LeaveCriticalSection(&s_usbCs);
        cb(param);
        Answer(p->itf, cmd);
        return t;
    }
    InterlockedIncrement(&s_inIssued);
    if (s_failIn) {
        t->done = true;
        t->err = USB_STALL_ERROR;
        cb(param);
        return t;
    }
    EnterCriticalSection(&s_usbCs);
    p->pending = t;
    bool now = p->inLen > 0;
    if (now)
        Complete(t);
    LeaveCriticalSection(&s_usbCs);
    if (now)
        cb(param);
    return t;
}

static BOOL FAbort(USB_TRANSFER tr, DWORD)
{
    FTr* t = (FTr*)tr;
    EnterCriticalSection(&s_usbCs);
    if (!t->done) {
        t->done = true;
        t->err = USB_CANCELED_ERROR;
    }
    if (t->p->pending == t)
        t->p->pending = NULL;
    LeaveCriticalSection(&s_usbCs);
    return TRUE;
}

static BOOL FStatus(USB_TRANSFER tr, LPDWORD n, LPDWORD err)
{
    FTr* t = (FTr*)tr;
    *n = t->n;
    *err = t->err;
    return TRUE;
}

static BOOL FCloseTr(USB_TRANSFER tr)
{
    InterlockedDecrement(&s_live);
    delete (FTr*)tr;
    return TRUE;
}

static USB_TRANSFER FVendor(USB_HANDLE, LPTRANSFER_NOTIFY_ROUTINE, LPVOID, DWORD, LPCUSB_DEVICE_REQUEST r, LPVOID,
                            ULONG)
{
    FTr* t = new FTr;
    memset(t, 0, sizeof(*t));
    InterlockedIncrement(&s_live);
    t->done = true;
    t->err = s_vendorFail ? USB_STALL_ERROR : USB_NO_ERROR;
    EnterCriticalSection(&s_usbCs);
    if (r->bRequest == 0x22 && s_ctlCount < 16) {
        s_ctlItf[s_ctlCount] = r->wIndex;
        s_ctlBits[s_ctlCount++] = r->wValue;
    }
    LeaveCriticalSection(&s_usbCs);
    return t;
}

static USB_PIPE FOpenPipe(USB_HANDLE, LPCUSB_ENDPOINT_DESCRIPTOR e)
{
    for (int i = 0; i < s_pipeCount; i++)
        if (s_pipes[i].addr == e->bEndpointAddress) {
            s_pipes[i].open = true;
            return &s_pipes[i];
        }
    return NULL;
}

static BOOL FClosePipe(USB_PIPE p)
{
    if (!((FPipe*)p)->open)
        s_badClose++;
    ((FPipe*)p)->open = false;
    return TRUE;
}

static BOOL FRegister(USB_HANDLE, LPDEVICE_NOTIFY_ROUTINE fn, LPVOID param)
{
    s_notify = fn;
    s_notifyParam = param;
    return TRUE;
}

// the device's descriptors, each element 4 bytes wider than the SDK's
struct MEp  { DWORD count; USB_ENDPOINT_DESCRIPTOR d; LPCVOID ext; DWORD pad; };
struct MItf { DWORD count; USB_INTERFACE_DESCRIPTOR d; LPCVOID ext; MEp* eps; DWORD pad; };
struct MCfg { DWORD count; USB_CONFIGURATION_DESCRIPTOR d; LPCVOID ext; DWORD num; MItf* itfs; };
struct MDev { DWORD count; USB_DEVICE_DESCRIPTOR d; MCfg* cfgs; MCfg* active; };

static MEp  s_eps[8][2];
static MItf s_itfs[8];
static MCfg s_cfg;
static MDev s_devd;

static void AddItf(int num, BYTE cls, BYTE proto, BYTE epIn, BYTE epOut)   // epOut 0: an interrupt IN only
{
    int i = (int)s_cfg.num++;
    MItf& f = s_itfs[i];
    memset(&f, 0, sizeof(f));
    f.count = sizeof(MItf);
    f.d.bInterfaceNumber = (BYTE)num;
    f.d.bInterfaceClass = cls;
    f.d.bInterfaceSubClass = cls == 8 ? 6 : 1;
    f.d.bInterfaceProtocol = proto;
    f.d.bNumEndpoints = epOut ? 2 : 1;
    f.eps = s_eps[i];
    for (int k = 0; k < f.d.bNumEndpoints; k++) {
        MEp& e = s_eps[i][k];
        memset(&e, 0, sizeof(e));
        e.count = sizeof(MEp);
        e.d.bEndpointAddress = k ? epOut : epIn;
        e.d.bmAttributes = epOut ? USB_ENDPOINT_TYPE_BULK : USB_ENDPOINT_TYPE_INTERRUPT;
        e.d.wMaxPacketSize = 64;
        if (epOut) {
            FPipe& p = s_pipes[s_pipeCount++];
            memset(&p, 0, sizeof(p));
            p.addr = e.d.bEndpointAddress;
            p.itf = num;
        }
    }
}

static void MakeDevice(WORD pid)
{
    memset(&s_cfg, 0, sizeof(s_cfg));
    memset(&s_devd, 0, sizeof(s_devd));
    s_pipeCount = 0;
    s_ctlCount = 0;
    s_cfg.count = sizeof(MCfg);
    s_cfg.itfs = s_itfs;
    s_devd.count = sizeof(MDev);
    s_devd.d.idVendor = kHuawei;
    s_devd.d.idProduct = pid;
    s_devd.cfgs = s_devd.active = &s_cfg;
    if (pid == kHuaweiCd) {
        AddItf(0, 8, 0x50, 0x81, 0x01);
        return;
    }
    AddItf(0, 0xFF, 0x61, 0x81, 0x01);
    if (s_onlyModem)
        return;
    AddItf(1, 0xFF, 0x69, 0x82, 0);
    AddItf(4, 0xFF, 0x62, 0x85, 0x05);
}

static LPCUSB_DEVICE FInfo(USB_HANDLE)
{
    return (LPCUSB_DEVICE)&s_devd;
}

static USB_FUNCS s_funcs;

static HANDLE ActivateDeviceEx(LPCWSTR key, LPCVOID, DWORD, LPVOID)   // as the device manager: the port's Init
{
    wcscpy(s_activated, key);
    if (s_noActive)
        return COM_Init(L"Drivers\\Active\\99", NULL) ? (HANDLE)1 : NULL;
    HKEY k;   // its active key, a stranger's before it
    DWORD disp;
    RegCreateKeyExW(HKEY_CURRENT_USER, L"Drivers\\Active\\98", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &k, &disp);
    RegSetValueExW(k, L"Key", 0, REG_SZ, (const BYTE*)L"Drivers\\BuiltIn\\Serial0", 48);
    RegCloseKey(k);
    RegCreateKeyExW(HKEY_CURRENT_USER, L"Drivers\\Active\\99", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &k, &disp);
    RegSetValueExW(k, L"Key", 0, REG_SZ, (const BYTE*)L"Drivers\\VestaModemCom", 44);
    RegSetValueExW(k, L"Name", 0, REG_SZ, (const BYTE*)L"COM7:", 12);
    RegCloseKey(k);
    return COM_Init(L"Drivers\\Active\\99", NULL) ? (HANDLE)1 : NULL;
}

static void ActiveGone()
{
    RegDeleteKeyW(HKEY_CURRENT_USER, L"Drivers\\Active\\98");
    RegDeleteKeyW(HKEY_CURRENT_USER, L"Drivers\\Active\\99");
    RegDeleteKeyW(HKEY_CURRENT_USER, L"Drivers\\Active");
    RegDeleteKeyW(HKEY_CURRENT_USER, L"Drivers");
}

static BOOL DeactivateDevice(HANDLE)
{
    s_deactivated++;
    return TRUE;
}

static int RegInt(const wchar_t* name)
{
    HKEY k;
    DWORD v = (DWORD)-2, type, size = sizeof(v);
    if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Vesta\\Modem", 0, KEY_READ, &k) == ERROR_SUCCESS) {
        RegQueryValueEx(k, name, NULL, &type, (BYTE*)&v, &size);
        RegCloseKey(k);
    }
    return (int)v;
}

static bool RegIs(const wchar_t* name, const wchar_t* want)
{
    HKEY k;
    wchar_t v[64] = L"?";
    DWORD type, size = sizeof(v) - 2;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Vesta\\Modem", 0, KEY_READ, &k) == ERROR_SUCCESS) {
        RegQueryValueEx(k, name, NULL, &type, (BYTE*)v, &size);
        RegCloseKey(k);
    }
    return !wcscmp(v, want);
}

static bool Until(bool (*cond)(), int ms)
{
    for (int i = 0; i < ms / 20 && !cond(); i++)
        Sleep(20);
    return cond();
}

static bool Activated() { return s_activated[0] != 0; }
static bool Rssi20() { return RegInt(L"Rssi") == 20; }
static bool Mode5() { return RegInt(L"Mode") == 5; }
static bool Mode3() { return RegInt(L"Mode") == 3; }

static DWORD Ctl(DWORD h, DWORD code, const void* in, DWORD inLen, void* out, DWORD outLen)
{
    DWORD got = 0;
    COM_IOControl(h, code, (PBYTE)in, inLen, (PBYTE)out, outLen, &got);
    return got;
}

static DWORD Queued(DWORD h)
{
    SerialDevStatus st;
    memset(&st, 0, sizeof(st));
    Ctl(h, IOCTL_SER_GET_COMMSTATUS, NULL, 0, &st, sizeof(st));
    return st.stat.cbInQue;
}

static void SetTimeouts(DWORD h, DWORD interval, DWORD constant)
{
    COMMTIMEOUTS to;
    memset(&to, 0, sizeof(to));
    to.ReadIntervalTimeout = interval;
    to.ReadTotalTimeoutConstant = constant;
    Ctl(h, IOCTL_SER_SET_TIMEOUTS, &to, sizeof(to), NULL, 0);
}

static DWORD s_waitCom;

static DWORD WINAPI LateModemBytes(LPVOID)
{
    Sleep(200);
    Feed(0, "x");
    return 0;
}

static DWORD WINAPI LateMask(LPVOID)
{
    Sleep(200);
    DWORD m = EV_RXCHAR;
    Ctl(s_waitCom, IOCTL_SER_SET_WAIT_MASK, &m, sizeof(m), NULL, 0);
    return 0;
}

static BOOL Attach(WORD pid, WORD vid = kHuawei)
{
    MakeDevice(pid);
    s_devd.d.idVendor = vid;
    s_activated[0] = 0;
    s_notify = NULL;
    if (pid == kHuaweiCd)   // the stick's status (CSW) to the switch command, waiting to be read
        FeedB(0, "USBS\x12\x34\x56\x78\0\0\0\0\0", 13);
    BOOL accept = TRUE;
    USBDeviceAttach((USB_HANDLE)1, &s_funcs, NULL, L"x", &accept, NULL, 0);
    return accept;
}

static DWORD WINAPI LatePurge(LPVOID)
{
    Sleep(200);
    DWORD how = PURGE_RXABORT;
    Ctl(s_waitCom, IOCTL_SER_PURGE, &how, sizeof(how), NULL, 0);
    return 0;
}

static void Detach()
{
    s_notify(s_notifyParam, USB_CLOSE_DEVICE, NULL, NULL, NULL, NULL);
}

static bool LastCtl(int itf, int bits)
{
    return s_ctlCount && s_ctlItf[s_ctlCount - 1] == itf && s_ctlBits[s_ctlCount - 1] == bits;
}

int main()
{
    InitializeCriticalSection(&s_usbCs);
    DllMain(NULL, DLL_PROCESS_ATTACH, NULL);
    s_funcs.lpGetDeviceInfo = FInfo;
    s_funcs.lpOpenPipe = FOpenPipe;
    s_funcs.lpClosePipe = FClosePipe;
    s_funcs.lpIssueBulkTransfer = FBulk;
    s_funcs.lpAbortTransfer = FAbort;
    s_funcs.lpGetTransferStatus = FStatus;
    s_funcs.lpCloseTransfer = FCloseTr;
    s_funcs.lpIssueVendorTransfer = FVendor;
    s_funcs.lpRegisterNotificationRoutine = FRegister;
    RegDeleteKey(HKEY_CURRENT_USER, L"Software\\Vesta\\Modem");

    Check(Attach(0x1506, 0x1234) == FALSE, "another device: not taken");
    BOOL acc = TRUE;
    USBDeviceAttach((USB_HANDLE)1, &s_funcs, (LPCUSB_INTERFACE)s_itfs, L"x", &acc, NULL, 0);
    Check(!acc, "an interface of a device (not the whole one): not taken");
    BOOL cd = Attach(kHuaweiCd);
    FPipe* cbw = PipeOf(0, false);
    Check(cd && cbw->outLen == 31 && !memcmp(cbw->out, "USBC", 4) && cbw->out[15] == 0x11 && cbw->out[16] == 0x06,
          "CD-ROM mode: Huawei's switch command sent");
    Check(PipeOf(0, true)->inLen == 0, "CD-ROM mode: its status read");
    Detach();

    // ---- the modem, its port answering
    s_modemAnswers = true;
    Check(Attach(kHuaweiModem) != FALSE, "modem: taken");
    Check(Until(Activated, 5000) && !wcscmp(s_activated, L"Drivers\\VestaModemCom"), "modem: its COM port activated");
    FPipe* uiOut = PipeOf(4, false), *mdOut = PipeOf(0, false);
    uiOut->out[uiOut->outLen] = 0;
    Check(strstr(uiOut->out, "AT+CIMI\r") && strstr(uiOut->out, "AT+CGDCONT=1,\"IP\",\"internet.mts.ru\"\r"),
          "modem: the SIM's operator asked, its APN set on the PC UI");
    Check(RegIs(L"Apn", L"internet.mts.ru") && RegInt(L"Data") == 0, "modem: the APN and the data port in the status");
    Check(s_ctlCount >= 2 && s_ctlItf[0] == 4 && s_ctlBits[0] == 3 && s_ctlItf[1] == 0 && s_ctlBits[1] == 3,
          "modem: DTR + RTS on the PC UI and the modem's port");
    Feed(4, "\r\n^RSSI:20\r\n");
    Check(Until(Rssi20, 2000), "status: ^RSSI from the PC UI");

    DWORD dev = (DWORD)s_dev;
    DWORD q = COM_Open(dev, 0, 0);
    Check(q != 0 && s_dev->opens == 0, "COM: a query handle - not an open");
    COM_Close(q);
    int before = s_ctlCount;
    DWORD h = COM_Open(dev, GENERIC_READ | GENERIC_WRITE, 0);
    Check(h && s_ctlCount == before + 1 && LastCtl(0, 3) && s_dev->lines == 3, "COM: open - DTR + RTS up");
    mdOut->outLen = 0;
    Check(COM_Write(h, "ATDT*99#\r", 9) == 9 && mdOut->outLen == 9 && !memcmp(mdOut->out, "ATDT*99#\r", 9),
          "COM: a write goes to the modem's port");
    static char big[5000];
    for (int i = 0; i < 5000; i++)
        big[i] = (char)(i % 251);
    mdOut->outLen = 0;
    Check(COM_Write(h, big, 5000) == 5000 && mdOut->outLen == 5000 && !memcmp(mdOut->out, big, 5000),
          "COM: a long write - in pieces, in order");
    s_failOut = true;
    Check(COM_Write(h, "AT\r", 3) == (DWORD)-1 && COM_Write(h, "", 0) == 0, "COM: a write the modem refuses - -1 (none: 0)");
    s_failOut = false;
    char buf[64];
    Check(COM_Read(h, NULL, 5) == (DWORD)-1, "COM: a read into no buffer - refused");
    SetTimeouts(h, MAXDWORD, 0);
    Check(COM_Read(h, buf, sizeof(buf)) == 0, "COM: nothing there - an immediate read gives 0");
    Feed(0, "\r\nCONNECT\r\n");
    for (int i = 0; i < 50 && Queued(h) < 11; i++)
        Sleep(20);
    Check(Queued(h) == 11, "COM: ClearCommError counts the bytes waiting");
    memset(buf, 0, sizeof(buf));
    Check(COM_Read(h, buf, sizeof(buf)) == 11 && !strcmp(buf, "\r\nCONNECT\r\n"), "COM: the modem's bytes read");
    SetTimeouts(h, 0, 300);
    DWORD t0 = GetTickCount();
    Check(COM_Read(h, buf, sizeof(buf)) == 0 && GetTickCount() - t0 >= 250, "COM: a read with a timeout waits it out");
    DWORD mask = EV_RXCHAR, ev = 0;
    Ctl(h, IOCTL_SER_SET_WAIT_MASK, &mask, sizeof(mask), NULL, 0);
    CloseHandle(CreateThread(NULL, 0, LateModemBytes, NULL, 0, NULL));
    Ctl(h, IOCTL_SER_WAIT_ON_MASK, NULL, 0, &ev, sizeof(ev));
    Check(ev == EV_RXCHAR, "COM: WaitCommEvent ends on bytes coming");
    DWORD purge = PURGE_RXCLEAR;
    Ctl(h, IOCTL_SER_PURGE, &purge, sizeof(purge), NULL, 0);
    Check(Queued(h) == 0, "COM: a purge drops what waits");
    s_waitCom = h;
    ev = 1;
    CloseHandle(CreateThread(NULL, 0, LateMask, NULL, 0, NULL));
    Ctl(h, IOCTL_SER_WAIT_ON_MASK, NULL, 0, &ev, sizeof(ev));
    Check(ev == 0, "COM: a new mask ends the wait with 0");
    DWORD ms = 0;
    Ctl(h, IOCTL_SER_GET_MODEMSTATUS, NULL, 0, &ms, sizeof(ms));
    Check((ms & MS_RLSD_ON) && (ms & MS_CTS_ON), "COM: the other end there (RLSD, CTS)");
    COMMTIMEOUTS to;
    DWORD m2 = 0;
    Check(Ctl(h, IOCTL_SER_GET_TIMEOUTS, NULL, 0, &to, sizeof(to)) == sizeof(to) && to.ReadTotalTimeoutConstant == 300 &&
          Ctl(h, IOCTL_SER_GET_WAIT_MASK, NULL, 0, &m2, sizeof(m2)) == 4 && m2 == EV_RXCHAR,
          "COM: the timeouts and the mask given back as set");
    COMMPROP cp;
    Check(Ctl(h, IOCTL_SER_GET_PROPERTIES, NULL, 0, &cp, sizeof(cp)) == sizeof(cp) && cp.dwProvSubType == PST_MODEM,
          "COM: GetCommProperties - a modem");
    Check(!COM_IOControl(h, IOCTL_SER_GET_DCB, NULL, 0, (PBYTE)&m2, sizeof(m2), NULL) &&
          GetLastError() == ERROR_INVALID_PARAMETER, "COM: too small a buffer - refused");
    Check(RegIs(L"Port", L"COM7:"), "COM: its name (Drivers\\Active) in the status");
    DCB dcb;
    Ctl(h, IOCTL_SER_GET_DCB, NULL, 0, &dcb, sizeof(dcb));
    Check(dcb.BaudRate == 115200 && dcb.ByteSize == 8, "COM: GetCommState - 115200 8N1 till set");
    dcb.fDtrControl = DTR_CONTROL_DISABLE;
    Ctl(h, IOCTL_SER_SET_DCB, &dcb, sizeof(dcb), NULL, 0);
    Check(LastCtl(0, 2), "COM: SetCommState without DTR - DTR down");
    Ctl(h, IOCTL_SER_SET_DTR, NULL, 0, NULL, 0);
    Check(LastCtl(0, 3), "COM: EscapeCommFunction SETDTR");
    s_vendorFail = true;
    Ctl(h, IOCTL_SER_CLR_RTS, NULL, 0, NULL, 0);
    s_vendorFail = false;
    Check(s_dev->lines == 3, "COM: the modem refuses the lines - kept as they were");
    Ctl(h, IOCTL_SER_CLR_RTS, NULL, 0, NULL, 0);
    Check(s_dev->lines == 1 && LastCtl(0, 1), "COM: and taken when it accepts");
    Feed(4, "\r\n^MODE:5,4\r\n");
    Check(Until(Mode5, 2000), "status: the PC UI read while the COM port is open");
    SetTimeouts(h, 0, 0);   // a read till something comes - ended by PURGE_RXABORT
    ev = 0;
    CloseHandle(CreateThread(NULL, 0, LatePurge, NULL, 0, NULL));
    t0 = GetTickCount();
    Check(COM_Read(h, buf, sizeof(buf)) == 0 && GetTickCount() - t0 < 1500, "COM: PURGE_RXABORT ends a waiting read");
    s_failIn = true;   // a stalled pipe: errors, not round and round, nothing to the port
    s_inIssued = 0;
    Feed(0, "x");
    Sleep(400);
    LONG issued = s_inIssued;
    s_failIn = false;
    Check(issued >= 1 && issued < 10, "a failing read: retried slowly");
    Sleep(300);
    Feed(0, "yz");
    for (int i = 0; i < 50 && Queued(h) < 3; i++)
        Sleep(20);
    Check(Queued(h) == 3, "after the errors: the reader goes on");
    t0 = GetTickCount();
    COM_Close(h);
    Check(LastCtl(0, 0) && s_dev->opens == 0 && GetTickCount() - t0 < 500,
          "COM: the last close - DTR down (the modem hangs up), at once");
    s_dev->gone = true;
    Check(!COM_Open(dev, GENERIC_READ, 0), "COM: no open once the modem is going");
    Detach();
    Check(s_deactivated == 1 && !s_dev && RegIs(L"Port", L""), "out: the COM port gone, the status cleared");

    // ---- the modem's port silent: the PC UI carries the data
    s_modemAnswers = false;
    ActiveGone();
    s_noActive = true;
    Attach(kHuaweiModem);
    Check(Until(Activated, 6000) && RegInt(L"Data") == 4, "silent modem port: the data through the PC UI");
    Check(RegIs(L"Port", L""), "no Drivers\\Active key: no name");
    h = COM_Open((DWORD)s_dev, GENERIC_READ | GENERIC_WRITE, 0);
    uiOut = PipeOf(4, false);
    uiOut->outLen = 0;
    COM_Write(h, "AT\r", 3);
    Check(uiOut->outLen == 3, "silent modem port: writes to the PC UI");
    Sleep(100);   // (the PC UI's own "OK" to it)
    Ctl(h, IOCTL_SER_PURGE, &purge, sizeof(purge), NULL, 0);
    Feed(4, "\r\nCONNECT\r\n");
    for (int i = 0; i < 50 && Queued(h) < 11; i++)
        Sleep(20);
    SetTimeouts(h, MAXDWORD, 0);
    memset(buf, 0, sizeof(buf));
    Check(COM_Read(h, buf, sizeof(buf)) == 11 && !strcmp(buf, "\r\nCONNECT\r\n"), "silent modem port: the PC UI read");
    COM_Close(h);
    Feed(4, "\r\n^MODE:3,0\r\n");
    Check(Until(Mode3, 2000), "silent modem port: closed - the PC UI is the status again");
    Detach();
    Check(s_deactivated == 2, "silent modem port: out");

    // ---- a stick with the modem's port only: it is the PC UI and the data port at once
    s_onlyModem = true;
    s_modemAnswers = true;
    Attach(kHuaweiModem);
    Check(Until(Activated, 6000) && RegInt(L"Data") == 0 && s_ctlCount == 1 && s_dev->shared,
          "modem port only: one port for both, raised once");
    Detach();
    Check(s_live == 0 && s_badClose == 0, "every transfer closed, no pipe closed twice");

    RegDeleteKey(HKEY_CURRENT_USER, L"Software\\Vesta\\Modem");
    ActiveGone();
    DllMain(NULL, DLL_PROCESS_DETACH, NULL);
    printf(s_fails ? "%d FAILED\n" : "all ok\n", s_fails);
    return s_fails ? 1 : 0;
}
