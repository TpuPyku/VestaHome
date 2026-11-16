// usbcli.h - what a USB client driver of ours needs (VestaModem, VestaTether): its log on the card, the descriptors, a
// bulk transfer that ends when the device goes. Before it: <windows.h>, <usbdi.h>, and #define USBCLI_LOG L"\\Name.log".
// A driver's Dev has h, f and volatile gone.
#ifndef USBCLI_H
#define USBCLI_H
#include <stdio.h>
#include <stdarg.h>

static CRITICAL_SECTION s_logCs;

static void Log(const wchar_t* fmt, ...)   // one line, UTF-8
{
    static const wchar_t* kDirs[2] = { L"\\SDMemory", L"\\USBDevice" };
    if (GetFileAttributes(L"\\SDMMC\\VestaLogs.on") == 0xFFFFFFFF)
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
        wcscat(path, USBCLI_LOG);
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

BOOL WINAPI DllMain(HANDLE, DWORD reason, LPVOID)   // (the log's lock)
{
    if (reason == DLL_PROCESS_ATTACH)
        InitializeCriticalSection(&s_logCs);
    else if (reason == DLL_PROCESS_DETACH)
        DeleteCriticalSection(&s_logCs);
    return TRUE;
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

static DWORD WINAPI Done(LPVOID ev)
{
    SetEvent((HANDLE)ev);
    return 1;
}

template <class Dev> static bool Waited(Dev* d, HANDLE ev, DWORD ms)   // the transfer's end, in slices: the device's going ends it too
{
    for (DWORD waited = 0; waited < ms && !d->gone; waited += 200)
        if (WaitForSingleObject(ev, 200) == WAIT_OBJECT_0)
            return true;
    return false;
}

template <class Dev> static bool Finish(Dev* d, USB_TRANSFER t, bool done, DWORD* got)   // not done - aborted; the result; closed
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
template <class Dev> static bool Bulk(Dev* d, USB_PIPE p, bool in, void* buf, DWORD len, DWORD* got, DWORD ms)
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

#endif
