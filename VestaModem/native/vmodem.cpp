// VestaModem.dll - a USB modem for the unit (2026-10-24, the user: "можно ли добавить драйвер usb 3g модема"). A
// Windows CE USB client driver (USBD loads it in device.exe for the VID/PID keys VestaHome writes at each start:
// vh_gps.inc ModemUsbSet). A Huawei stick in its CD-ROM mode (12D1:155B) is switched into the modem (the mass storage
// command usb_modeswitch calls HuaweiNewMode). In the modem mode (12D1:1506, E3121: interface 0 FF/01/61 - the
// modem, 4 FF/01/62 - the PC UI, the only one that answered AT on the HU 2026-10-28) step 2 (2026-10-29, the user:
// "делай модем"): the PC UI gets the SIM's IMSI and the APN of its operator (AT+CGDCONT, the modem keeps it for its
// other ports), its lines (^RSSI, ^MODE, ^SIMST) are the status in HKLM\Software\Vesta\Modem for VestaHome; the
// modem's port (DTR raised first: SET_CONTROL_LINE_STATE, as Linux's option driver does; not answering - the PC UI
// itself) becomes a COM port, the stream driver of this DLL (key Drivers\VestaModemCom, written by VestaHome with
// Tsp = Unimodem.dll: a TAPI modem for RAS, *99#). Log: VestaLogs\VestaModem.log on the SD card (or the USB disk),
// none when \SDMMC\VestaLogs.off is there. Imports COREDLL only.
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <usbdi.h>
#include "mdm.h"

enum { kHuawei = 0x12D1, kHuaweiCd = 0x155B, kHuaweiModem = 0x1506 };

static CRITICAL_SECTION s_logCs;

static void Log(const wchar_t* fmt, ...)   // one line, UTF-8
{
    static const wchar_t* kDirs[2] = { L"\\SDMemory", L"\\USBDevice" };
    if (GetFileAttributes(L"\\SDMMC\\VestaLogs.off") != 0xFFFFFFFF)
        return;
    wchar_t w[400];
    int n = _snwprintf(w, 398, L"%10u ", (unsigned)GetTickCount());
    va_list a;
    va_start(a, fmt);
    int m = _vsnwprintf(w + n, 398 - n, fmt, a);
    va_end(a);
    n = m < 0 ? 398 : n + m;
    w[n++] = L'\r';
    w[n++] = L'\n';
    char line[1200];
    int len = WideCharToMultiByte(CP_UTF8, 0, w, n, line, sizeof(line), NULL, NULL);
    EnterCriticalSection(&s_logCs);
    for (int i = 0; i < 2; i++) {
        if (GetFileAttributes(kDirs[i]) == 0xFFFFFFFF)
            continue;
        wchar_t path[64];
        _snwprintf(path, 64, L"%s\\VestaLogs", kDirs[i]);
        CreateDirectory(path, NULL);
        wcscat(path, L"\\VestaModem.log");
        HANDLE f = CreateFile(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, 0, NULL);
        if (f != INVALID_HANDLE_VALUE) {
            DWORD wr;
            SetFilePointer(f, 0, NULL, FILE_END);
            WriteFile(f, line, len, &wr, NULL);
            CloseHandle(f);
        }
        break;
    }
    LeaveCriticalSection(&s_logCs);
}

static void Text(const BYTE* b, DWORD n, char* out, int cap)   // the reply, printable ("\r" "\n" shown)
{
    int o = 0;
    for (DWORD i = 0; i < n && o < cap - 3; i++) {
        BYTE c = b[i];
        if (c == '\r' || c == '\n') {
            out[o++] = '\\';
            out[o++] = c == '\r' ? 'r' : 'n';
        } else {
            out[o++] = c >= 32 && c < 127 ? (char)c : '.';
        }
    }
    out[o] = 0;
}

// The unit's USBD hands out its arrays of USB_INTERFACE / USB_ENDPOINT bigger than the SDK's structs (HU 2026-10-24-2:
// the second endpoint read 4 bytes off - "00:упр:1287" is the next descriptor's 07 05): each element's dwCount is
// its size, so the steps go by it.
static const void* Nth(const void* first, DWORD i, DWORD sdk)   // (each element begins with its dwCount)
{
    DWORD size = *(const DWORD*)first;
    return (const BYTE*)first + i * (size ? size : sdk);
}

static LPCUSB_INTERFACE ItfAt(LPCUSB_CONFIGURATION cfg, DWORD i)
{
    return (LPCUSB_INTERFACE)Nth(cfg->lpInterfaces, i, sizeof(USB_INTERFACE));
}

static LPCUSB_ENDPOINT EpAt(LPCUSB_INTERFACE itf, int k)
{
    return (LPCUSB_ENDPOINT)Nth(itf->lpEndpoints, k, sizeof(USB_ENDPOINT));
}

struct Port {   // one vendor interface with its bulk pair
    struct Dev*      dev;
    LPCUSB_INTERFACE itf;
    USB_PIPE         in, out;
    WORD             inMax;
};

enum { kRxSize = 16384 };

struct Dev {
    USB_HANDLE       h;
    LPCUSB_FUNCS     f;
    HANDLE           thread;      // the start (Start)
    HANDLE           uiThread;    // the PC UI's reader
    HANDLE           rxThread;    // the modem port's reader (none when the PC UI is the data port too)
    volatile bool    gone;
    Port             ui, data;    // the status and the COM port's; shared - the same one
    bool             shared;
    CRITICAL_SECTION cs;          // rx, st
    Fifo             rx;          // the data port's bytes for the COM port (while it is open)
    BYTE             rxBuf[kRxSize];
    HANDLE           rxEv;        // bytes came (or the device went)
    CRITICAL_SECTION wcs;         // one write at a time
    LineAcc          acc;
    MdmStat          st;
    HANDLE           com;         // the COM port (ActivateDeviceEx)
    DCB              dcb;         // what Unimodem / PPP set: given back as set
    WORD             lines;       // DTR 1, RTS 2 (SET_CONTROL_LINE_STATE)
    volatile LONG    opens;       // the COM port's handles
    volatile LONG    busy;        // COM calls inside (the detach waits for them)
    volatile LONG    maskSeq;     // a new mask, a purge, a close: the WaitCommEvent going on ends
};

static Dev* volatile s_dev;   // the modem of the COM port (one at a time)

static DWORD WINAPI Done(LPVOID ev)
{
    SetEvent((HANDLE)ev);
    return 1;
}

static bool Waited(Dev* d, HANDLE ev, DWORD ms)   // the transfer's end, in slices: the device's going ends it too
{
    for (DWORD waited = 0; waited < ms && !d->gone; waited += 200)
        if (WaitForSingleObject(ev, 200) == WAIT_OBJECT_0)
            return true;
    return false;
}

static bool Finish(Dev* d, USB_TRANSFER t, bool done, DWORD* got)   // not done - aborted; the result; closed
{
    if (!done)
        d->f->lpAbortTransfer(t, 0);
    DWORD err;
    bool ok = d->f->lpGetTransferStatus(t, got, &err) && err == USB_NO_ERROR && done;
    d->f->lpCloseTransfer(t);
    return ok;
}

// One bulk transfer, waited for at most ms (INFINITE: until the device goes), then aborted; *got - the bytes moved.
// True when it completed fine.
static bool Bulk(Dev* d, USB_PIPE p, bool in, void* buf, DWORD len, DWORD* got, DWORD ms)
{
    *got = 0;
    HANDLE ev = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!ev)
        return false;
    USB_TRANSFER t = d->f->lpIssueBulkTransfer(p, Done, ev, (in ? USB_IN_TRANSFER : USB_OUT_TRANSFER) |
                                               USB_SHORT_TRANSFER_OK, len, buf, 0);
    bool ok = t && Finish(d, t, Waited(d, ev, ms), got);
    CloseHandle(ev);
    return ok;
}

static LPCUSB_ENDPOINT FindEp(LPCUSB_INTERFACE itf, bool in)   // its bulk endpoint of that direction
{
    for (int i = 0; i < itf->Descriptor.bNumEndpoints; i++) {
        const USB_ENDPOINT_DESCRIPTOR& e = EpAt(itf, i)->Descriptor;
        if ((e.bmAttributes & USB_ENDPOINT_TYPE_MASK) == USB_ENDPOINT_TYPE_BULK &&
            ((e.bEndpointAddress & USB_ENDPOINT_DIRECTION_MASK) != 0) == in)
            return EpAt(itf, i);
    }
    return NULL;
}

static void ReadCsw(Dev* d, LPCUSB_ENDPOINT in)   // the command's status (CSW), if it answers before leaving
{
    USB_PIPE pi = d->f->lpOpenPipe(d->h, &in->Descriptor);
    if (!pi)
        return;
    BYTE csw[13];
    DWORD got;
    bool r = Bulk(d, pi, true, csw, sizeof(csw), &got, 1000);
    Log(L"переключение: ответ %s, %u Б, статус %d", r ? L"есть" : L"нет", (unsigned)got, got == 13 ? csw[12] : -1);
    d->f->lpClosePipe(pi);
}

static void SendCbw(Dev* d, LPCUSB_INTERFACE itf, LPCUSB_ENDPOINT out)
{
    static const BYTE kCbw[31] = { 0x55, 0x53, 0x42, 0x43, 0x12, 0x34, 0x56, 0x78, 0, 0, 0, 0, 0, 0, 0,
                                   0x11, 0x06, 0x20, 0, 0, 0x01, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    USB_PIPE po = d->f->lpOpenPipe(d->h, &out->Descriptor);
    if (!po) {
        Log(L"переключение: не открыть конец 0x%02X", out->Descriptor.bEndpointAddress);
        return;
    }
    BYTE cbw[31];
    memcpy(cbw, kCbw, sizeof(cbw));
    DWORD got;
    bool ok = Bulk(d, po, false, cbw, sizeof(cbw), &got, 2000);
    Log(L"переключение: команда Huawei в интерфейс %d (конец 0x%02X) - %s, %u Б", itf->Descriptor.bInterfaceNumber,
        out->Descriptor.bEndpointAddress, ok ? L"отправлена" : L"ошибка", (unsigned)got);
    LPCUSB_ENDPOINT in = FindEp(itf, true);
    if (ok && in)
        ReadCsw(d, in);
    d->f->lpClosePipe(po);
}

// The CD-ROM mode: the switch command in a mass storage command block (CBW) to its bulk out endpoint - the bytes
// usb_modeswitch sends for Huawei (HuaweiNewMode). The stick then leaves and comes back as the modem.
static void SwitchHuawei(Dev* d, LPCUSB_DEVICE dev)
{
    LPCUSB_CONFIGURATION cfg = dev->lpActiveConfig;
    for (DWORD i = 0; cfg && i < cfg->dwNumInterfaces; i++) {
        LPCUSB_INTERFACE itf = ItfAt(cfg, i);
        LPCUSB_ENDPOINT out = itf->Descriptor.bInterfaceClass == 8 ? FindEp(itf, false) : NULL;
        if (out) {
            SendCbw(d, itf, out);
            return;
        }
    }
    Log(L"переключение: нет интерфейса накопителя с выходом");
}

static void LogDevice(LPCUSB_DEVICE dev)
{
    const USB_DEVICE_DESCRIPTOR& dd = dev->Descriptor;
    Log(L"устройство %04X:%04X, версия %04X, класс %02X/%02X/%02X, конфигураций %d", dd.idVendor, dd.idProduct,
        dd.bcdDevice, dd.bDeviceClass, dd.bDeviceSubClass, dd.bDeviceProtocol, dd.bNumConfigurations);
    LPCUSB_CONFIGURATION cfg = dev->lpActiveConfig;
    if (!cfg)
        return;
    Log(L"  размеры USBD: устройство %u, конфигурация %u, интерфейс %u (SDK %u), конец %u (SDK %u); интерфейсов %u",
        dev->dwCount, cfg->dwCount, cfg->dwNumInterfaces ? cfg->lpInterfaces[0].dwCount : 0, sizeof(USB_INTERFACE),
        cfg->dwNumInterfaces && cfg->lpInterfaces[0].Descriptor.bNumEndpoints ? cfg->lpInterfaces[0].lpEndpoints[0].dwCount : 0,
        sizeof(USB_ENDPOINT), cfg->dwNumInterfaces);
    for (DWORD i = 0; i < cfg->dwNumInterfaces; i++) {
        LPCUSB_INTERFACE itf = ItfAt(cfg, i);
        const USB_INTERFACE_DESCRIPTOR& id = itf->Descriptor;
        char eps[160];
        int o = 0;
        eps[0] = 0;
        for (int k = 0; k < id.bNumEndpoints && o < 140; k++) {
            const USB_ENDPOINT_DESCRIPTOR& e = EpAt(itf, k)->Descriptor;
            static const char* kType[4] = { "упр", "изохр", "bulk", "прер" };
            o += _snprintf(eps + o, sizeof(eps) - o, " %02X:%s:%d", e.bEndpointAddress,
                           kType[e.bmAttributes & USB_ENDPOINT_TYPE_MASK], e.wMaxPacketSize);
        }
        Log(L"  интерфейс %d.%d: класс %02X/%02X/%02X, концы%S", id.bInterfaceNumber, id.bAlternateSetting,
            id.bInterfaceClass, id.bInterfaceSubClass, id.bInterfaceProtocol, eps);
    }
}

// ---- the modem mode: the ports, the status, the COM port

static bool LineState(Dev* d, const Port& p, WORD bits)   // DTR 1, RTS 2: CDC's SET_CONTROL_LINE_STATE
{
    USB_DEVICE_REQUEST r;
    r.bmRequestType = USB_REQUEST_HOST_TO_DEVICE | USB_REQUEST_CLASS | USB_REQUEST_FOR_INTERFACE;
    r.bRequest = 0x22;
    r.wValue = bits;
    r.wIndex = p.itf->Descriptor.bInterfaceNumber;
    r.wLength = 0;
    USB_TRANSFER t = d->f->lpIssueVendorTransfer(d->h, NULL, NULL, USB_OUT_TRANSFER | USB_SEND_TO_INTERFACE, &r,
                                                 NULL, 0);   // (no callback: it blocks)
    if (!t)
        return false;
    DWORD n, err;
    bool ok = d->f->lpGetTransferStatus(t, &n, &err) && err == USB_NO_ERROR;
    d->f->lpCloseTransfer(t);
    return ok;
}

// The vendor interface of Huawei's port kind (the protocol's low digit: 1 the modem, 2 the PC UI)
static LPCUSB_INTERFACE FindItf(LPCUSB_CONFIGURATION cfg, int kind)
{
    for (DWORD i = 0; cfg && i < cfg->dwNumInterfaces; i++) {
        LPCUSB_INTERFACE itf = ItfAt(cfg, i);
        const USB_INTERFACE_DESCRIPTOR& id = itf->Descriptor;
        if (id.bInterfaceClass == 0xFF && !id.bAlternateSetting && (id.bInterfaceProtocol & 0x0F) == kind &&
            FindEp(itf, false) && FindEp(itf, true))
            return itf;
    }
    return NULL;
}

static bool PortOpen(Dev* d, LPCUSB_INTERFACE itf, Port* p)
{
    LPCUSB_ENDPOINT out = FindEp(itf, false), in = FindEp(itf, true);
    p->dev = d;
    p->itf = itf;
    p->in = d->f->lpOpenPipe(d->h, &in->Descriptor);
    p->out = d->f->lpOpenPipe(d->h, &out->Descriptor);
    p->inMax = in->Descriptor.wMaxPacketSize;
    return p->in && p->out;
}

static void PortClose(Dev* d, Port* p)
{
    if (p->in)
        d->f->lpClosePipe(p->in);
    if (p->out)
        d->f->lpClosePipe(p->out);
    p->in = p->out = NULL;
}

static bool Send(Dev* d, const Port& p, const char* cmd)
{
    DWORD got;
    bool ok = Bulk(d, p.out, false, (void*)cmd, (DWORD)strlen(cmd), &got, 1000);
    char t[160];
    Text((const BYTE*)cmd, (DWORD)strlen(cmd), t, sizeof(t));
    Log(L"  интерфейс %d <- %S%s", p.itf->Descriptor.bInterfaceNumber, t, ok ? L"" : L" (запись не прошла)");
    return ok;
}

static HKEY StatusKey()
{
    HKEY k;
    DWORD disp;
    return RegCreateKeyEx(HKEY_LOCAL_MACHINE, L"Software\\Vesta\\Modem", 0, NULL, 0, 0, NULL, &k, &disp) ==
           ERROR_SUCCESS ? k : NULL;
}

static void StatusPut(const wchar_t* name, DWORD type, const void* v, DWORD size)
{
    HKEY k = StatusKey();
    if (k) {
        RegSetValueEx(k, name, 0, type, (const BYTE*)v, size);
        RegCloseKey(k);
    }
}

static void StatusSet(const wchar_t* name, int v)
{
    StatusPut(name, REG_DWORD, &v, sizeof(v));
}

static void StatusStr(const wchar_t* name, const wchar_t* v)
{
    StatusPut(name, REG_SZ, v, (DWORD)(wcslen(v) + 1) * sizeof(wchar_t));
}

// A line from the PC UI: the status (to VestaHome on a change), into the log but the periodic ones and the IMSI
// (only its operator: the first 5 digits)
static void OnUiLine(void* ctx, const char* line)
{
    Dev* d = (Dev*)ctx;
    EnterCriticalSection(&d->cs);
    bool changed = MdmLine(&d->st, line);
    MdmStat st = d->st;
    LeaveCriticalSection(&d->cs);
    if (changed) {
        StatusSet(L"Rssi", st.rssi);
        StatusSet(L"Mode", st.mode);
        StatusSet(L"Sim", st.sim);
    }
    if (!strcmp(line, st.imsi))
        Log(L"  IMSI оператора %.5S…", line);
    else if (strncmp(line, "^RSSI:", 6) && strncmp(line, "^DSFLOWRPT:", 11)) Log(L"  модем: %S", line);
}

static void Deliver(Dev* d, const BYTE* b, int n)   // to the COM port's queue; full - waits for its reader
{
    while (n > 0 && !d->gone && d->opens > 0) {
        EnterCriticalSection(&d->cs);
        int put = FifoPut(&d->rx, b, n);
        LeaveCriticalSection(&d->cs);
        if (put)
            SetEvent(d->rxEv);
        b += put;
        n -= put;
        if (n)
            Sleep(10);
    }
}

// A port's reader until the device goes: the data port's bytes to the COM port while it is open (the PC UI's,
// when shared), else the PC UI's lines to the status; the modem port's while closed are dropped
static void Rx(Dev* d, Port* p)
{
    BYTE buf[512];
    DWORD len = p->inMax && p->inMax < sizeof(buf) ? p->inMax : sizeof(buf);   // one packet: done at each
    while (!d->gone) {
        DWORD n;
        bool ok = Bulk(d, p->in, true, buf, len, &n, INFINITE);
        if (p->itf == d->data.itf && d->opens > 0)
            Deliver(d, buf, (int)n);
        else if (p->itf == d->ui.itf)
            LineFeed(&d->acc, buf, (int)n, OnUiLine, d);
        if (!ok && !d->gone)
            Sleep(200);   // (an error: not round and round)
    }
}

static DWORD WINAPI RxThread(LPVOID p)   // (p: the Port)
{
    Port* port = (Port*)p;
    __try {
        Rx(port->dev, port);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log(L"порт %d: исключение %08X", port->itf->Descriptor.bInterfaceNumber, GetExceptionCode());
    }
    return 0;
}

// The modem's port answers AT (its reader not started yet): it is the data port
static bool Answers(Dev* d, const Port& p)
{
    if (!Send(d, p, "AT\r"))
        return false;
    char all[1100] = "";   // (a packet: up to 512 B)
    DWORD len = 0;
    for (int r = 0; r < 6 && len < 512 && !d->gone && !strstr(all, "OK"); r++) {   // up to ~1.5 s
        DWORD n;
        Bulk(d, p.in, true, all + len, p.inMax, &n, 250);
        len += n;
        all[len] = 0;
    }
    char t[200];
    Text((const BYTE*)all, len, t, sizeof(t));
    Log(L"  интерфейс %d -> %S", p.itf->Descriptor.bInterfaceNumber, t);
    return strstr(all, "OK") != NULL;
}

static const char* Apn(Dev* d)   // the SIM operator's
{
    EnterCriticalSection(&d->cs);
    const char* apn = MdmApn(d->st.imsi);
    LeaveCriticalSection(&d->cs);
    return apn;
}

static bool OurPort(HKEY act, const wchar_t* sub, wchar_t* name, int cap)   // Drivers\Active\<sub> is ours: its Name
{
    HKEY k;
    if (RegOpenKeyEx(act, sub, 0, 0, &k) != ERROR_SUCCESS)
        return false;
    wchar_t key[64];
    DWORD type, size = sizeof(key);
    bool ours = RegQueryValueEx(k, L"Key", NULL, &type, (BYTE*)key, &size) == ERROR_SUCCESS &&
                !_wcsicmp(key, L"Drivers\\VestaModemCom");
    size = cap * sizeof(wchar_t);
    if (ours && RegQueryValueEx(k, L"Name", NULL, &type, (BYTE*)name, &size) != ERROR_SUCCESS)
        name[0] = 0;
    RegCloseKey(k);
    return ours;
}

static void PortName(wchar_t* name, int cap)   // the COM port's "COMn:" (Drivers\Active: its Key)
{
    name[0] = 0;
    HKEY act;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"Drivers\\Active", 0, 0, &act) != ERROR_SUCCESS)
        return;
    wchar_t sub[16];
    for (DWORD i = 0, n = 16; RegEnumKeyEx(act, i, sub, &n, NULL, NULL, NULL, NULL) == ERROR_SUCCESS; i++, n = 16)
        if (OurPort(act, sub, name, cap))
            break;
    RegCloseKey(act);
}

static void UiSetup(Dev* d)   // the PC UI: no echo, the SIM's operator and its APN, the status asked
{
    Send(d, d->ui, "ATE0\r");
    Sleep(300);
    Send(d, d->ui, "AT+CIMI\r");
    for (int i = 0; i < 20 && !d->st.imsi[0] && !d->gone; i++)
        Sleep(100);
    const char* apn = Apn(d);
    char cmd[96];
    _snprintf(cmd, sizeof(cmd), "AT+CGDCONT=1,\"IP\",\"%s\"\r", apn);
    Send(d, d->ui, cmd);
    Send(d, d->ui, "AT^SYSINFO\r");
    Send(d, d->ui, "AT+CSQ\r");
    wchar_t wapn[64];
    _snwprintf(wapn, sizeof(wapn) / sizeof(*wapn), L"%S", apn);
    StatusStr(L"Apn", wapn);
}

static void DataPort(Dev* d, LPCUSB_INTERFACE modem)   // the modem's port, if it answers (else the PC UI stays it)
{
    Port p;
    memset(&p, 0, sizeof(p));
    bool open = PortOpen(d, modem, &p);
    if (open && !LineState(d, p, 3))
        Log(L"  интерфейс %d: DTR не принят", modem->Descriptor.bInterfaceNumber);
    if (open && Answers(d, p)) {
        d->data = p;
        d->shared = false;
        d->rxThread = CreateThread(NULL, 0, RxThread, &d->data, 0, NULL);
        return;
    }
    Log(L"порт модема (интерфейс %d) не отвечает - данные через PC UI", modem->Descriptor.bInterfaceNumber);
    PortClose(d, &p);
}

static void ComUp(Dev* d)
{
    s_dev = d;
    d->com = ActivateDeviceEx(L"Drivers\\VestaModemCom", NULL, 0, d);
    wchar_t name[16] = L"";
    if (d->com)
        PortName(name, sizeof(name) / sizeof(*name));
    StatusStr(L"Port", name);
    if (d->com)
        Log(L"COM-порт модема: %s (данные - интерфейс %d)", name[0] ? name : L"без имени",
            d->data.itf->Descriptor.bInterfaceNumber);
    else
        Log(L"COM-порт модема НЕ поднят: ActivateDeviceEx - ошибка %lu (нет ключа Drivers\\VestaModemCom?)",
            GetLastError());
}

// The modem came: the PC UI read (status), the SIM's APN set, the data port found, the COM port up
static void Start(Dev* d)
{
    LPCUSB_DEVICE dev = d->f->lpGetDeviceInfo(d->h);
    LPCUSB_CONFIGURATION cfg = dev ? dev->lpActiveConfig : NULL;
    LPCUSB_INTERFACE modem = FindItf(cfg, 1), ui = FindItf(cfg, 2);
    if (!ui)
        ui = modem;
    if (!ui || !PortOpen(d, ui, &d->ui)) {
        Log(L"модем: нет порта PC UI / модема");
        return;
    }
    LineState(d, d->ui, 3);
    d->data = d->ui;   // (until the modem's port answers)
    d->shared = true;
    d->uiThread = CreateThread(NULL, 0, RxThread, &d->ui, 0, NULL);
    UiSetup(d);
    if (modem && modem != ui && !d->gone)
        DataPort(d, modem);
    StatusSet(L"Data", d->data.itf->Descriptor.bInterfaceNumber);
    if (!d->gone)
        ComUp(d);
}

static DWORD WINAPI StartThread(LPVOID p)   // (a fault in it into the log, not device.exe's end)
{
    __try {
        Start((Dev*)p);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log(L"запуск модема: исключение %08X", GetExceptionCode());
    }
    return 0;
}

static void Wait(HANDLE* t)
{
    if (*t) {
        WaitForSingleObject(*t, 5000);
        CloseHandle(*t);
        *t = NULL;
    }
}

static Dev* NewDev(USB_HANDLE h, LPCUSB_FUNCS f)
{
    Dev* d = new Dev;
    if (!d)
        return NULL;
    memset(d, 0, sizeof(*d));
    d->h = h;
    d->f = f;
    InitializeCriticalSection(&d->cs);
    InitializeCriticalSection(&d->wcs);
    d->rx.b = d->rxBuf;
    d->rx.cap = kRxSize;
    d->rxEv = CreateEvent(NULL, FALSE, FALSE, NULL);
    MdmStatInit(&d->st);
    d->dcb.DCBlength = sizeof(DCB);
    d->dcb.BaudRate = 115200;
    d->dcb.fBinary = TRUE;
    d->dcb.fDtrControl = DTR_CONTROL_ENABLE;
    d->dcb.fRtsControl = RTS_CONTROL_ENABLE;
    d->dcb.ByteSize = 8;
    d->dcb.Parity = NOPARITY;
    d->dcb.StopBits = ONESTOPBIT;
    return d;
}

static void FreeDev(Dev* d)
{
    DeleteCriticalSection(&d->cs);
    DeleteCriticalSection(&d->wcs);
    if (d->rxEv)
        CloseHandle(d->rxEv);
    delete d;
}

static BOOL WINAPI Notify(LPVOID p, DWORD code, LPDWORD*, LPDWORD*, LPDWORD*, LPDWORD*)
{
    if (code != USB_CLOSE_DEVICE)
        return FALSE;
    Dev* d = (Dev*)p;
    d->gone = true;
    SetEvent(d->rxEv);
    Wait(&d->thread);
    if (d->com)
        DeactivateDevice(d->com);
    if (s_dev == d)
        s_dev = NULL;
    Wait(&d->uiThread);
    Wait(&d->rxThread);
    for (int i = 0; i < 40 && d->busy; i++)   // a COM call still inside: at most 2 s
        Sleep(50);
    PortClose(d, &d->ui);
    if (!d->shared)
        PortClose(d, &d->data);
    StatusStr(L"Port", L"");
    Log(L"устройство отключено");
    if (!d->busy)   // (still inside: rather leak it than free it under them)
        FreeDev(d);
    return TRUE;
}

static bool Take(Dev* d, LPCUSB_DEVICE dev)   // ours: the CD-ROM switched, the modem started
{
    WORD vid = dev->Descriptor.idVendor, pid = dev->Descriptor.idProduct;
    if (vid == kHuawei && pid == kHuaweiCd) {
        __try {
            SwitchHuawei(d, dev);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Log(L"переключение: исключение %08X", GetExceptionCode());
        }
        return true;
    }
    if (vid == kHuawei && pid == kHuaweiModem && !s_dev) {
        d->thread = CreateThread(NULL, 0, StartThread, d, 0, NULL);
        return true;
    }
    Log(s_dev ? L"второй модем %04X:%04X - не наш" : L"не наш: %04X:%04X", vid, pid);
    return false;
}

extern "C" BOOL USBDeviceAttach(USB_HANDLE h, LPCUSB_FUNCS f, LPCUSB_INTERFACE itf, LPCWSTR id, LPBOOL accept,
                                LPCUSB_DRIVER_SETTINGS, DWORD)
{
    *accept = FALSE;
    LPCUSB_DEVICE dev = f->lpGetDeviceInfo(h);
    if (!dev || itf)
        return TRUE;   // (registered for whole devices only)
    __try {
        LogDevice(dev);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log(L"разбор устройства: исключение %08X", GetExceptionCode());
    }
    Dev* d = NewDev(h, f);
    if (!d)
        return FALSE;
    if (!Take(d, dev)) {
        FreeDev(d);
        return TRUE;
    }
    f->lpRegisterNotificationRoutine(h, Notify, d);
    *accept = TRUE;   // ours: the mass storage driver does not take the CD-ROM, nothing else the modem
    return TRUE;
}

extern "C" BOOL USBInstallDriver(LPCWSTR)   // (the keys are VestaHome's: written at each start)
{
    return TRUE;
}

extern "C" BOOL USBUnInstallDriver()
{
    return TRUE;
}

// ---- the COM port (Drivers\VestaModemCom: Prefix COM, Tsp Unimodem.dll): the data port's bytes as a serial
// port's for Unimodem and PPP. ReadFile by COMMTIMEOUTS, WaitCommEvent (EV_RXCHAR), ClearCommError; DTR / RTS go
// to the modem (DTR down - it hangs up: Unimodem's init has &D2); the other end is always "there" (CTS, DSR, RLSD).
// IOCTL_SERIAL_* (pegdser.h, not in the Pocket PC 2003 SDK): CTL_CODE(FILE_DEVICE_SERIAL_PORT, n, METHOD_BUFFERED,
// FILE_ANY_ACCESS)
#define SER(n) ((0x1B << 16) | ((n) << 2))
enum {
    IOCTL_SER_SET_DTR = SER(3), IOCTL_SER_CLR_DTR = SER(4), IOCTL_SER_SET_RTS = SER(5), IOCTL_SER_CLR_RTS = SER(6),
    IOCTL_SER_GET_WAIT_MASK = SER(9), IOCTL_SER_SET_WAIT_MASK = SER(10), IOCTL_SER_WAIT_ON_MASK = SER(11),
    IOCTL_SER_GET_COMMSTATUS = SER(12), IOCTL_SER_GET_MODEMSTATUS = SER(13), IOCTL_SER_GET_PROPERTIES = SER(14),
    IOCTL_SER_SET_TIMEOUTS = SER(15), IOCTL_SER_GET_TIMEOUTS = SER(16), IOCTL_SER_PURGE = SER(17),
    IOCTL_SER_GET_DCB = SER(20), IOCTL_SER_SET_DCB = SER(21)
};
struct SerialDevStatus { DWORD errors; COMSTAT stat; };   // SERIAL_DEV_STATUS

struct Open {   // one CreateFile of it
    Dev*          d;
    COMMTIMEOUTS  to;
    DWORD         mask;      // SetCommMask
    volatile bool closing;
};

static void Lines(Dev* d, WORD bits)   // the modem lines, logged on a change
{
    if (bits == d->lines)
        return;
    bool ok = LineState(d, d->data, bits);
    if (ok)   // (refused: the next call tries again)
        d->lines = bits;
    Log(L"COM: DTR %d RTS %d%s", bits & 1, (bits >> 1) & 1, ok ? L"" : L" - не принято");
}

static int Waiting(Dev* d)
{
    EnterCriticalSection(&d->cs);
    int n = d->rx.count;
    LeaveCriticalSection(&d->cs);
    return n;
}

extern "C" {

DWORD COM_Init(LPCTSTR, LPCVOID)   // (the device: the modem being started - Start sets it before activating)
{
    return (DWORD)s_dev;
}

BOOL COM_Deinit(DWORD)
{
    return TRUE;
}

DWORD COM_Open(DWORD dev, DWORD access, DWORD)
{
    Dev* d = (Dev*)dev;
    Open* o = d && !d->gone ? (Open*)LocalAlloc(LPTR, sizeof(Open)) : NULL;
    if (!o)
        return 0;
    o->d = d;
    if (access & (GENERIC_READ | GENERIC_WRITE)) {
        if (InterlockedIncrement((LONG*)&d->opens) == 1) {
            EnterCriticalSection(&d->cs);
            d->rx.count = 0;
            LeaveCriticalSection(&d->cs);
            Lines(d, 3);
        }
        Log(L"COM: открыт (%d)", d->opens);
    } else {
        o->closing = true;   // (a query handle: no reads)
    }
    return (DWORD)o;
}

BOOL COM_Close(DWORD h)
{
    Open* o = (Open*)h;
    if (!o)
        return FALSE;
    Dev* d = o->d;
    if (!o->closing) {
        o->closing = true;
        InterlockedIncrement((LONG*)&d->maskSeq);
        SetEvent(d->rxEv);
        if (!InterlockedDecrement((LONG*)&d->opens) && !d->gone)
            Lines(d, 0);   // the last one: the modem hangs up, as a real port's DTR going down
        Log(L"COM: закрыт (%d)", d->opens);
    }
    for (int i = 0; i < 40 && d->busy; i++)   // a read or a wait inside: let it end
        Sleep(50);
    LocalFree(o);
    return TRUE;
}

// ReadFile's wait by COMMTIMEOUTS: MAXDWORD / 0 / 0 - none; all 0 - until something comes (INFINITE); else
// multiplier x len + constant ms
static DWORD ReadLimit(const COMMTIMEOUTS& to, DWORD len)
{
    DWORD total = to.ReadTotalTimeoutMultiplier * len + to.ReadTotalTimeoutConstant;
    if (!total)
        return to.ReadIntervalTimeout == MAXDWORD ? 0 : INFINITE;
    return total;
}

static int ReadSome(Open* o, BYTE* buf, DWORD len)   // what waits, or what comes within the limit
{
    Dev* d = o->d;
    DWORD limit = ReadLimit(o->to, len), t0 = GetTickCount();
    LONG mseq = d->maskSeq;
    for (;;) {
        EnterCriticalSection(&d->cs);
        int got = FifoGet(&d->rx, buf, (int)len);
        LeaveCriticalSection(&d->cs);
        DWORD spent = GetTickCount() - t0;
        if (got || spent >= limit || d->gone || o->closing || d->maskSeq != mseq)
            return got;
        WaitForSingleObject(d->rxEv, limit - spent > 50 ? 50 : limit - spent);
    }
}

static DWORD WriteAll(Dev* d, const BYTE* buf, DWORD len);

static DWORD Transfer(DWORD h, void* buf, DWORD len, bool write)   // ReadFile / WriteFile: the bytes, -1 - failed
{
    Open* o = (Open*)h;
    if (!o || !buf || (write && o->d->gone))
        return (DWORD)-1;
    InterlockedIncrement((LONG*)&o->d->busy);
    DWORD n = write ? WriteAll(o->d, (const BYTE*)buf, len) : (DWORD)ReadSome(o, (BYTE*)buf, len);
    InterlockedDecrement((LONG*)&o->d->busy);
    return !write || n || !len ? n : (DWORD)-1;
}

DWORD COM_Read(DWORD h, LPVOID buf, DWORD len)
{
    return Transfer(h, buf, len, false);
}

static DWORD WriteAll(Dev* d, const BYTE* buf, DWORD len)   // to the data port, 4 KB a transfer; the bytes taken
{
    EnterCriticalSection(&d->wcs);
    DWORD done = 0;
    while (done < len && !d->gone) {
        DWORD n = len - done > 4096 ? 4096 : len - done, got;
        bool ok = Bulk(d, d->data.out, false, (void*)(buf + done), n, &got, 5000);
        done += got;
        if (!ok)
            break;
    }
    LeaveCriticalSection(&d->wcs);
    return done;
}

DWORD COM_Write(DWORD h, LPCVOID buf, DWORD len)
{
    return Transfer(h, (void*)buf, len, true);
}

DWORD COM_Seek(DWORD, long, DWORD)
{
    return (DWORD)-1;
}

static void CommProps(COMMPROP* cp)
{
    memset(cp, 0, sizeof(*cp));
    cp->wPacketLength = sizeof(*cp);
    cp->dwMaxBaud = BAUD_USER;
    cp->dwProvSubType = PST_MODEM;
    cp->dwProvCapabilities = PCF_DTRDSR | PCF_RTSCTS | PCF_RLSD | PCF_TOTALTIMEOUTS | PCF_INTTIMEOUTS;
    cp->dwSettableParams = SP_BAUD | SP_DATABITS | SP_PARITY | SP_STOPBITS | SP_HANDSHAKING | SP_RLSD;
    cp->dwSettableBaud = BAUD_9600 | BAUD_19200 | BAUD_38400 | BAUD_57600 | BAUD_115200 | BAUD_USER;
    cp->wSettableData = DATABITS_8;
    cp->wSettableStopParity = STOPBITS_10 | PARITY_NONE;
    cp->dwCurrentTxQueue = cp->dwCurrentRxQueue = kRxSize;
}

static DWORD WaitOnMask(Open* o)   // WaitCommEvent: EV_RXCHAR when bytes wait; 0 when the mask changed or it closed
{
    Dev* d = o->d;
    LONG mseq = d->maskSeq;
    while (!o->closing && !d->gone && d->maskSeq == mseq && o->mask) {
        if ((o->mask & EV_RXCHAR) && Waiting(d) > 0)
            return EV_RXCHAR;
        WaitForSingleObject(d->rxEv, 50);
    }
    return 0;
}

// The calls that give something back: its bytes into v (*n of them); false - not one of them
static bool IoValue(Open* o, DWORD code, BYTE* v, DWORD* n)
{
    Dev* d = o->d;
    switch (code) {
    case IOCTL_SER_GET_TIMEOUTS: *n = sizeof(COMMTIMEOUTS); memcpy(v, &o->to, *n); return true;
    case IOCTL_SER_GET_DCB:      *n = sizeof(DCB); memcpy(v, &d->dcb, *n); return true;
    case IOCTL_SER_GET_WAIT_MASK: *n = sizeof(DWORD); *(DWORD*)v = o->mask; return true;
    case IOCTL_SER_WAIT_ON_MASK: *n = sizeof(DWORD); *(DWORD*)v = WaitOnMask(o); return true;
    case IOCTL_SER_GET_MODEMSTATUS: *n = sizeof(DWORD); *(DWORD*)v = MS_CTS_ON | MS_DSR_ON | MS_RLSD_ON; return true;
    case IOCTL_SER_GET_COMMSTATUS:   // ClearCommError: no errors, the bytes waiting
        *n = sizeof(SerialDevStatus);
        memset(v, 0, *n);
        ((SerialDevStatus*)v)->stat.cbInQue = (DWORD)Waiting(d);
        return true;
    case IOCTL_SER_GET_PROPERTIES: *n = sizeof(COMMPROP); CommProps((COMMPROP*)v); return true;
    }
    return false;
}

static void Purge(Dev* d, DWORD how)   // PURGE_RXCLEAR: what waits goes; PURGE_RXABORT: the reads / waits going on end
{
    if (how & PURGE_RXCLEAR) {
        EnterCriticalSection(&d->cs);
        d->rx.count = 0;
        LeaveCriticalSection(&d->cs);
    }
    if (how & PURGE_RXABORT)
        InterlockedIncrement((LONG*)&d->maskSeq);
}

// The calls that set something (in: inLen bytes, at least what each needs - checked by the caller)
static void IoSet(Open* o, DWORD code, const BYTE* in)
{
    Dev* d = o->d;
    switch (code) {
    case IOCTL_SER_SET_TIMEOUTS: memcpy(&o->to, in, sizeof(COMMTIMEOUTS)); break;
    case IOCTL_SER_SET_DCB:   // kept to be given back; DTR / RTS from it to the modem
        memcpy(&d->dcb, in, sizeof(DCB));
        Lines(d, (d->dcb.fDtrControl != DTR_CONTROL_DISABLE ? 1 : 0) | (d->dcb.fRtsControl != RTS_CONTROL_DISABLE ? 2 : 0));
        break;
    case IOCTL_SER_SET_WAIT_MASK:
        o->mask = *(const DWORD*)in;
        InterlockedIncrement((LONG*)&d->maskSeq);   // ends a WaitCommEvent going on
        break;
    case IOCTL_SER_PURGE: Purge(d, *(const DWORD*)in); break;
    }
}

static DWORD SetSize(DWORD code)   // what a setting call needs in (0: not one)
{
    switch (code) {
    case IOCTL_SER_SET_TIMEOUTS: return sizeof(COMMTIMEOUTS);
    case IOCTL_SER_SET_DCB:      return sizeof(DCB);
    case IOCTL_SER_SET_WAIT_MASK: case IOCTL_SER_PURGE: return sizeof(DWORD);
    }
    return 0;
}

static void IoLines(Dev* d, DWORD code)   // EscapeCommFunction's DTR / RTS
{
    switch (code) {
    case IOCTL_SER_SET_DTR: Lines(d, d->lines | 1); break;
    case IOCTL_SER_CLR_DTR: Lines(d, d->lines & ~1); break;
    case IOCTL_SER_SET_RTS: Lines(d, d->lines | 2); break;
    case IOCTL_SER_CLR_RTS: Lines(d, d->lines & ~2); break;
    }
}

static BOOL Io(Open* o, DWORD code, PBYTE in, DWORD inLen, PBYTE out, DWORD outLen, PDWORD actual)
{
    BYTE v[sizeof(COMMPROP) + sizeof(DCB)];
    DWORD n;
    if (IoValue(o, code, v, &n)) {
        if (!out || outLen < n)
            return FALSE;
        memcpy(out, v, n);
        if (actual)
            *actual = n;
        return TRUE;
    }
    DWORD need = SetSize(code);
    if (need && in && inLen >= need)
        IoSet(o, code, in);
    else if (!need)
        IoLines(o->d, code);   // (and the rest - break, XON / XOFF, queue sizes...: taken)
    return TRUE;
}

BOOL COM_IOControl(DWORD h, DWORD code, PBYTE in, DWORD inLen, PBYTE out, DWORD outLen, PDWORD actual)
{
    Open* o = (Open*)h;
    if (!o)
        return FALSE;
    if (actual)
        *actual = 0;
    InterlockedIncrement((LONG*)&o->d->busy);
    BOOL ok = Io(o, code, in, inLen, out, outLen, actual);
    if (!ok)
        SetLastError(ERROR_INVALID_PARAMETER);
    InterlockedDecrement((LONG*)&o->d->busy);
    return ok;
}

void COM_PowerUp(DWORD)
{
}

void COM_PowerDown(DWORD)
{
}

}   // extern "C"

BOOL WINAPI DllMain(HANDLE, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
        InitializeCriticalSection(&s_logCs);
    else if (reason == DLL_PROCESS_DETACH)
        DeleteCriticalSection(&s_logCs);
    return TRUE;
}
