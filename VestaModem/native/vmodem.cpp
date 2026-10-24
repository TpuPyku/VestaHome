// VestaModem.dll - a USB modem for the unit (2026-10-24, the user: "можно ли добавить драйвер usb 3g модема"). A
// Windows CE USB client driver (USBD loads it in device.exe for the VID/PID keys VestaHome writes at each start:
// vh_gps.inc ModemUsbSet). Step 1 (a probe): a Huawei stick in its CD-ROM mode (12D1:155B) is switched into the
// modem (the mass storage command usb_modeswitch calls HuaweiNewMode); in the modem mode (12D1:1506) its interfaces
// and endpoints go into the log, and each vendor-class one with a bulk pair is asked "AT" and "ATI" - which of them
// is the modem's port. No COM port yet. Log: VestaLogs\VestaModem.log on the SD card (or the USB disk), none when
// \SDMMC\VestaLogs.off is there. Imports COREDLL only.
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <usbdi.h>

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
static LPCUSB_INTERFACE ItfAt(LPCUSB_CONFIGURATION cfg, DWORD i)
{
    DWORD size = cfg->lpInterfaces[0].dwCount;
    return (LPCUSB_INTERFACE)((const BYTE*)cfg->lpInterfaces + i * (size ? size : sizeof(USB_INTERFACE)));
}

static LPCUSB_ENDPOINT EpAt(LPCUSB_INTERFACE itf, int k)
{
    DWORD size = itf->lpEndpoints[0].dwCount;
    return (LPCUSB_ENDPOINT)((const BYTE*)itf->lpEndpoints + k * (size ? size : sizeof(USB_ENDPOINT)));
}

struct Dev {
    USB_HANDLE    h;
    LPCUSB_FUNCS  f;
    HANDLE        thread;
    volatile bool gone;
};

static DWORD WINAPI Done(LPVOID ev)
{
    SetEvent((HANDLE)ev);
    return 1;
}

// One bulk transfer, waited for at most ms (then aborted); *got - the bytes moved. True when it completed fine.
static bool Bulk(Dev* d, USB_PIPE p, bool in, void* buf, DWORD len, DWORD* got, DWORD ms)
{
    *got = 0;
    HANDLE ev = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!ev)
        return false;
    USB_TRANSFER t = d->f->lpIssueBulkTransfer(p, Done, ev, (in ? USB_IN_TRANSFER : USB_OUT_TRANSFER) |
                                               USB_SHORT_TRANSFER_OK, len, buf, 0);
    if (!t) {
        CloseHandle(ev);
        return false;
    }
    bool timedOut = WaitForSingleObject(ev, ms) != WAIT_OBJECT_0;
    if (timedOut)
        d->f->lpAbortTransfer(t, 0);
    DWORD n = 0, err = 0;
    bool ok = d->f->lpGetTransferStatus(t, &n, &err) && err == USB_NO_ERROR && !timedOut;
    *got = n;
    d->f->lpCloseTransfer(t);
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

// The CD-ROM mode: the switch command in a mass storage command block (CBW) to its bulk out endpoint - the bytes
// usb_modeswitch sends for Huawei (HuaweiNewMode). The stick then leaves and comes back as the modem.
static void SwitchHuawei(Dev* d, LPCUSB_DEVICE dev)
{
    static const BYTE kCbw[31] = { 0x55, 0x53, 0x42, 0x43, 0x12, 0x34, 0x56, 0x78, 0, 0, 0, 0, 0, 0, 0,
                                   0x11, 0x06, 0x20, 0, 0, 0x01, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    LPCUSB_CONFIGURATION cfg = dev->lpActiveConfig;
    for (DWORD i = 0; cfg && i < cfg->dwNumInterfaces; i++) {
        LPCUSB_INTERFACE itf = ItfAt(cfg, i);
        if (itf->Descriptor.bInterfaceClass != 8)
            continue;
        LPCUSB_ENDPOINT out = FindEp(itf, false), in = FindEp(itf, true);
        if (!out)
            continue;
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
        if (ok && in) {   // the status (CSW), if it answers before leaving
            USB_PIPE pi = d->f->lpOpenPipe(d->h, &in->Descriptor);
            if (pi) {
                BYTE csw[13];
                bool r = Bulk(d, pi, true, csw, sizeof(csw), &got, 1000);
                Log(L"переключение: ответ %s, %u Б, статус %d", r ? L"есть" : L"нет", (unsigned)got, got == 13 ? csw[12] : -1);
                d->f->lpClosePipe(pi);
            }
        }
        d->f->lpClosePipe(po);
        return;
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

// The modem mode: every vendor-class interface with a bulk pair asked "AT" and "ATI"; the replies logged.
static void Probe(Dev* d);

static DWORD WINAPI ProbeThread(LPVOID p)   // (a fault in it into the log, not device.exe's end)
{
    __try {
        Probe((Dev*)p);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log(L"проба: исключение %08X", GetExceptionCode());
    }
    return 0;
}

static void Probe(Dev* d)
{
    LPCUSB_DEVICE dev = d->f->lpGetDeviceInfo(d->h);
    LPCUSB_CONFIGURATION cfg = dev ? dev->lpActiveConfig : NULL;
    for (DWORD i = 0; cfg && i < cfg->dwNumInterfaces && !d->gone; i++) {
        LPCUSB_INTERFACE itf = ItfAt(cfg, i);
        if (itf->Descriptor.bInterfaceClass != 0xFF || itf->Descriptor.bAlternateSetting)
            continue;
        LPCUSB_ENDPOINT out = FindEp(itf, false), in = FindEp(itf, true);
        if (!out || !in)
            continue;
        USB_PIPE po = d->f->lpOpenPipe(d->h, &out->Descriptor), pi = d->f->lpOpenPipe(d->h, &in->Descriptor);
        static const char* kCmd[2] = { "AT\r", "ATI\r" };
        for (int c = 0; c < 2 && po && pi && !d->gone; c++) {
            DWORD got;
            bool w = Bulk(d, po, false, (void*)kCmd[c], (DWORD)strlen(kCmd[c]), &got, 1000);
            BYTE buf[512];
            DWORD all = 0;
            for (int r = 0; r < 6 && w && all < sizeof(buf) - 64 && !d->gone; r++) {   // up to ~1.5 s of reply
                DWORD n;
                Bulk(d, pi, true, buf + all, in->Descriptor.wMaxPacketSize, &n, 250);
                all += n;
            }
            char t[400];
            Text(buf, all, t, sizeof(t));
            Log(L"  интерфейс %d: %s -> %s%u Б: %S", itf->Descriptor.bInterfaceNumber, c ? L"ATI" : L"AT",
                w ? L"" : L"(запись не прошла) ", (unsigned)all, t);
        }
        if (po)
            d->f->lpClosePipe(po);
        if (pi)
            d->f->lpClosePipe(pi);
    }
    Log(L"проба портов закончена");
}

static BOOL WINAPI Notify(LPVOID p, DWORD code, LPDWORD*, LPDWORD*, LPDWORD*, LPDWORD*)
{
    if (code != USB_CLOSE_DEVICE)
        return FALSE;
    Dev* d = (Dev*)p;
    d->gone = true;
    if (d->thread) {
        WaitForSingleObject(d->thread, 5000);
        CloseHandle(d->thread);
    }
    Log(L"устройство отключено");
    delete d;
    return TRUE;
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
    WORD vid = dev->Descriptor.idVendor, pid = dev->Descriptor.idProduct;
    Dev* d = new Dev;
    if (!d)
        return FALSE;
    d->h = h;
    d->f = f;
    d->thread = NULL;
    d->gone = false;
    if (vid == kHuawei && pid == kHuaweiCd) {
        __try {
            SwitchHuawei(d, dev);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Log(L"переключение: исключение %08X", GetExceptionCode());
        }
    } else if (vid == kHuawei && pid == kHuaweiModem) {
        d->thread = CreateThread(NULL, 0, ProbeThread, d, 0, NULL);
    } else {
        Log(L"не наш: %04X:%04X", vid, pid);
        delete d;
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

BOOL WINAPI DllMain(HANDLE, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
        InitializeCriticalSection(&s_logCs);
    else if (reason == DLL_PROCESS_DETACH)
        DeleteCriticalSection(&s_logCs);
    return TRUE;
}
