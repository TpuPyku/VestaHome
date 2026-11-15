// ndismp.cpp - the phone's tethering as a network adapter (ndismp.h): a serialized NDIS 5.1 miniport for 802.3 over
// the phone's RNDIS data pipes. Its frames go out wrapped as RNDIS packets, the phone's come in on a thread of its own
// and are indicated to TCP/IP. ARM only (the unit's NDIS).
#include <windows.h>
#include <winioctl.h>
extern "C" {   // (the SDK's ndis.h is C only)
#include <ndis.h>
#include <ntddndis.h>
}
#include "ndismp.h"
#include "rndis.h"

extern void NdisLog(const wchar_t* fmt, ...);   // (vtether.cpp's log)

enum { kRxBuf = 16384, kTxBuf = 1600, kMtu = 1500, kHeader = 14 };
static const wchar_t kMiniport[] = L"VestaRndis";
static const wchar_t kAdapter[] = L"VestaRndis1";

struct Adapter {
    NDIS_HANDLE   h;
    TetherPort    port;
    volatile bool halt;
    HANDLE        rx;
    ULONG         filter, lookahead, xmitOk, rcvOk, xmitErr;
    u8            rxBuf[kRxBuf];
    u8            txBuf[kTxBuf];
};

static TetherPort       s_port;      // the phone's, set before the adapter is registered
static Adapter* volatile s_ad;
static NDIS_HANDLE      s_wrapper;

// ---- receiving: the phone's transfers, each a run of RNDIS packets with one Ethernet frame each

static DWORD WINAPI RxThread(LPVOID p)
{
    Adapter* a = (Adapter*)p;
    while (!a->halt) {
        int n = a->port.recv(a->port.ctx, a->rxBuf, kRxBuf);
        if (n < 0)
            break;
        int pos = 0, len;
        const u8* frame;
        bool any = false;
        while (RndisNext(a->rxBuf, n, &pos, &frame, &len))
            if (len >= kHeader && !a->halt) {
                NdisMEthIndicateReceive(a->h, (NDIS_HANDLE)a, (PCHAR)frame, kHeader, (PVOID)(frame + kHeader), len - kHeader, len - kHeader);
                a->rcvOk++;
                any = true;
            }
        if (any)
            NdisMEthIndicateReceiveComplete(a->h);
    }
    if (!a->halt) {
        NdisMIndicateStatus(a->h, NDIS_STATUS_MEDIA_DISCONNECT, NULL, 0);
        NdisMIndicateStatusComplete(a->h);
    }
    return 0;
}

// ---- the miniport's handlers

static NDIS_STATUS MpInitialize(PNDIS_STATUS openErr, PUINT selected, PNDIS_MEDIUM media, UINT count, NDIS_HANDLE h,
                                NDIS_HANDLE)
{
    *openErr = NDIS_STATUS_SUCCESS;
    UINT i = 0;
    while (i < count && media[i] != NdisMedium802_3)
        i++;
    if (i == count)
        return NDIS_STATUS_UNSUPPORTED_MEDIA;
    *selected = i;
    Adapter* a = (Adapter*)LocalAlloc(LPTR, sizeof(Adapter));
    if (!a)
        return NDIS_STATUS_RESOURCES;
    a->h = h;
    a->port = s_port;
    a->lookahead = kMtu;
    NdisMSetAttributesEx(h, (NDIS_HANDLE)a, 0, NDIS_ATTRIBUTE_NO_HALT_ON_SUSPEND, NdisInterfaceInternal);
    s_ad = a;
    a->rx = CreateThread(NULL, 0, RxThread, a, 0, NULL);
    NdisLog(L"адаптер: NDIS принял, %02X:%02X:%02X:%02X:%02X:%02X", a->port.mac[0], a->port.mac[1], a->port.mac[2],
            a->port.mac[3], a->port.mac[4], a->port.mac[5]);
    return NDIS_STATUS_SUCCESS;
}

static VOID MpHalt(NDIS_HANDLE ctx)
{
    Adapter* a = (Adapter*)ctx;
    a->halt = true;
    if (a->rx) {
        WaitForSingleObject(a->rx, 5000);
        CloseHandle(a->rx);
    }
    s_ad = NULL;
    NdisLog(L"адаптер: остановлен (отправлено %u, принято %u, ошибок отправки %u)", a->xmitOk, a->rcvOk, a->xmitErr);
    LocalFree(a);
}

static NDIS_STATUS MpReset(PBOOLEAN addressingReset, NDIS_HANDLE)
{
    *addressingReset = FALSE;
    return NDIS_STATUS_SUCCESS;
}

static NDIS_STATUS MpSend(NDIS_HANDLE ctx, PNDIS_PACKET packet, UINT)
{
    Adapter* a = (Adapter*)ctx;
    PNDIS_BUFFER buf = packet->Private.Head;   // (NdisQueryPacket's macro needs the kernel's page size)
    UINT at = 0;
    while (buf) {
        PVOID va;
        UINT len;
        NdisQueryBuffer(buf, &va, &len);
        if (at + len > kTxBuf)
            return NDIS_STATUS_FAILURE;
        memcpy(a->txBuf + at, va, len);
        at += len;
        NdisGetNextBuffer(buf, &buf);
    }
    if (!a->port.send(a->port.ctx, a->txBuf, (int)at)) {
        a->xmitErr++;
        return NDIS_STATUS_FAILURE;
    }
    a->xmitOk++;
    return NDIS_STATUS_SUCCESS;
}

static const NDIS_OID kOids[] = {
    OID_GEN_SUPPORTED_LIST, OID_GEN_HARDWARE_STATUS, OID_GEN_MEDIA_SUPPORTED, OID_GEN_MEDIA_IN_USE,
    OID_GEN_MAXIMUM_LOOKAHEAD, OID_GEN_MAXIMUM_FRAME_SIZE, OID_GEN_LINK_SPEED, OID_GEN_TRANSMIT_BUFFER_SPACE,
    OID_GEN_RECEIVE_BUFFER_SPACE, OID_GEN_TRANSMIT_BLOCK_SIZE, OID_GEN_RECEIVE_BLOCK_SIZE, OID_GEN_VENDOR_ID,
    OID_GEN_VENDOR_DESCRIPTION, OID_GEN_CURRENT_PACKET_FILTER, OID_GEN_CURRENT_LOOKAHEAD, OID_GEN_DRIVER_VERSION,
    OID_GEN_MAXIMUM_TOTAL_SIZE, OID_GEN_MAC_OPTIONS, OID_GEN_MEDIA_CONNECT_STATUS, OID_GEN_MAXIMUM_SEND_PACKETS,
    OID_GEN_VENDOR_DRIVER_VERSION, OID_GEN_XMIT_OK, OID_GEN_RCV_OK, OID_GEN_XMIT_ERROR, OID_GEN_RCV_ERROR,
    OID_GEN_RCV_NO_BUFFER, OID_802_3_PERMANENT_ADDRESS, OID_802_3_CURRENT_ADDRESS, OID_802_3_MULTICAST_LIST,
    OID_802_3_MAXIMUM_LIST_SIZE, OID_802_3_RCV_ERROR_ALIGNMENT, OID_802_3_XMIT_ONE_COLLISION,
    OID_802_3_XMIT_MORE_COLLISIONS
};

static ULONG Word(Adapter* a, NDIS_OID oid, bool* known)   // the OIDs answered with one ULONG
{
    *known = true;
    switch (oid) {
    case OID_GEN_HARDWARE_STATUS:       return NdisHardwareStatusReady;
    case OID_GEN_MEDIA_SUPPORTED:
    case OID_GEN_MEDIA_IN_USE:          return NdisMedium802_3;
    case OID_GEN_MAXIMUM_LOOKAHEAD:
    case OID_GEN_MAXIMUM_FRAME_SIZE:    return kMtu;
    case OID_GEN_CURRENT_LOOKAHEAD:     return a->lookahead;
    case OID_GEN_LINK_SPEED:            return 1000000;   // 100 Mbit/s, in 100 bit/s
    case OID_GEN_TRANSMIT_BUFFER_SPACE:
    case OID_GEN_TRANSMIT_BLOCK_SIZE:   return kTxBuf;
    case OID_GEN_RECEIVE_BUFFER_SPACE:
    case OID_GEN_RECEIVE_BLOCK_SIZE:    return kRxBuf;
    case OID_GEN_VENDOR_ID:             return 0xFFFFFF;
    case OID_GEN_CURRENT_PACKET_FILTER: return a->filter;
    case OID_GEN_DRIVER_VERSION:        return 0x0501;
    case OID_GEN_VENDOR_DRIVER_VERSION: return 0x00010000;
    case OID_GEN_MAXIMUM_TOTAL_SIZE:    return kMtu + kHeader;
    case OID_GEN_MAC_OPTIONS:           return NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA | NDIS_MAC_OPTION_NO_LOOPBACK;
    case OID_GEN_MEDIA_CONNECT_STATUS:  return a->halt ? NdisMediaStateDisconnected : NdisMediaStateConnected;
    case OID_GEN_MAXIMUM_SEND_PACKETS:  return 1;
    case OID_GEN_XMIT_OK:               return a->xmitOk;
    case OID_GEN_RCV_OK:                return a->rcvOk;
    case OID_GEN_XMIT_ERROR:            return a->xmitErr;
    case OID_802_3_MAXIMUM_LIST_SIZE:   return 32;
    case OID_GEN_RCV_ERROR:
    case OID_GEN_RCV_NO_BUFFER:
    case OID_802_3_RCV_ERROR_ALIGNMENT:
    case OID_802_3_XMIT_ONE_COLLISION:
    case OID_802_3_XMIT_MORE_COLLISIONS: return 0;
    }
    *known = false;
    return 0;
}

static NDIS_STATUS MpQuery(NDIS_HANDLE ctx, NDIS_OID oid, PVOID buf, ULONG len, PULONG written, PULONG needed)
{
    Adapter* a = (Adapter*)ctx;
    static const char kDesc[] = "VestaHome USB tethering (RNDIS)";
    bool known;
    ULONG w = Word(a, oid, &known);
    const void* src = &w;
    ULONG size = sizeof(ULONG);
    if (oid == OID_GEN_SUPPORTED_LIST)
        src = kOids, size = sizeof(kOids);
    else if (oid == OID_GEN_VENDOR_DESCRIPTION)
        src = kDesc, size = sizeof(kDesc);
    else if (oid == OID_802_3_PERMANENT_ADDRESS || oid == OID_802_3_CURRENT_ADDRESS)
        src = a->port.mac, size = 6;
    else if (oid == OID_802_3_MULTICAST_LIST)
        size = 0;
    else if (!known)
        return NDIS_STATUS_NOT_SUPPORTED;
    *written = 0;
    *needed = size;
    if (len < size)
        return NDIS_STATUS_INVALID_LENGTH;
    memcpy(buf, src, size);
    *written = size;
    return NDIS_STATUS_SUCCESS;
}

static NDIS_STATUS MpSet(NDIS_HANDLE ctx, NDIS_OID oid, PVOID buf, ULONG len, PULONG read, PULONG needed)
{
    Adapter* a = (Adapter*)ctx;
    *read = len;
    *needed = 0;
    switch (oid) {
    case OID_GEN_CURRENT_PACKET_FILTER:
    case OID_GEN_CURRENT_LOOKAHEAD:
        if (len < sizeof(ULONG)) {
            *needed = sizeof(ULONG);
            return NDIS_STATUS_INVALID_LENGTH;
        }
        if (oid == OID_GEN_CURRENT_PACKET_FILTER)
            a->filter = *(ULONG*)buf;
        else
            a->lookahead = *(ULONG*)buf;
        return NDIS_STATUS_SUCCESS;
    case OID_802_3_MULTICAST_LIST:   // (the phone's filter stays as set: all it sends goes up)
    case OID_GEN_PROTOCOL_OPTIONS:
    case OID_GEN_NETWORK_LAYER_ADDRESSES:
        return NDIS_STATUS_SUCCESS;
    }
    return NDIS_STATUS_NOT_SUPPORTED;
}

// NDIS loads this DLL by Comm\VestaRndis's ImagePath and calls this
extern "C" NDIS_STATUS DriverEntry(PVOID driverObject, PVOID registryPath)
{
    NdisMInitializeWrapper(&s_wrapper, driverObject, registryPath, NULL);
    NDIS40_MINIPORT_CHARACTERISTICS chars;   // (4.0: nothing of 5.x needed)
    NdisZeroMemory(&chars, sizeof(chars));
    NDIS30_MINIPORT_CHARACTERISTICS& c = chars.Ndis30Chars;
    c.MajorNdisVersion = 4;
    c.MinorNdisVersion = 0;
    c.InitializeHandler = MpInitialize;
    c.HaltHandler = MpHalt;
    c.ResetHandler = MpReset;
    c.SendHandler = MpSend;
    c.QueryInformationHandler = MpQuery;
    c.SetInformationHandler = MpSet;
    NDIS_STATUS st = NdisMRegisterMiniport(s_wrapper, (PNDIS_MINIPORT_CHARACTERISTICS)&chars, sizeof(chars));
    if (st != NDIS_STATUS_SUCCESS) {
        NdisLog(L"адаптер: NdisMRegisterMiniport -> %08X", st);
        NdisTerminateWrapper(s_wrapper, NULL);
    }
    return st;
}

// ---- registration (as the firmware's Comm\BTCENDIS / BTCENDIS1)

static void SetSz(HKEY k, const wchar_t* name, const wchar_t* v)
{
    RegSetValueEx(k, name, 0, REG_SZ, (const BYTE*)v, (DWORD)(wcslen(v) + 1) * sizeof(wchar_t));
}

static void SetDw(HKEY k, const wchar_t* name, DWORD v)
{
    RegSetValueEx(k, name, 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
}

static HKEY Key(const wchar_t* path)
{
    HKEY k = NULL;
    DWORD disp;
    RegCreateKeyEx(HKEY_LOCAL_MACHINE, path, 0, NULL, 0, 0, NULL, &k, &disp);
    return k;
}

static void Keys()
{
    static const wchar_t* const kDriver[2] = { L"Comm\\VestaRndis", L"Comm\\VestaRndis1" };
    for (int i = 0; i < 2; i++) {
        HKEY k = Key(kDriver[i]);
        if (!k)
            continue;
        SetSz(k, L"ImagePath", L"VestaTether.dll");
        SetSz(k, L"Group", L"NDIS");
        SetSz(k, L"DisplayName", L"USB-модем телефона");
        RegCloseKey(k);
    }
    HKEY k = Key(L"Comm\\VestaRndis\\Linkage");
    if (k) {
        static const wchar_t route[] = L"VestaRndis1\0";
        RegSetValueEx(k, L"Route", 0, REG_MULTI_SZ, (const BYTE*)route, sizeof(route));
        RegCloseKey(k);
    }
    k = Key(L"Comm\\VestaRndis1\\Parms");
    if (k) {
        SetDw(k, L"BusType", 0);
        SetDw(k, L"BusNumber", 0);
        RegCloseKey(k);
    }
    k = Key(L"Comm\\VestaRndis1\\Parms\\TcpIp");
    if (k) {
        SetDw(k, L"EnableDHCP", 1);
        SetDw(k, L"AutoCfg", 0);
        RegCloseKey(k);
    }
}

static bool Ndis(DWORD ioctl, const wchar_t* names, DWORD bytes)
{
    HANDLE n = CreateFile(DD_NDIS_DEVICE_NAME, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                          OPEN_ALWAYS, 0, NULL);
    if (n == INVALID_HANDLE_VALUE)
        return false;
    DWORD got = 0;
    BOOL ok = DeviceIoControl(n, ioctl, (LPVOID)names, bytes, NULL, 0, &got, NULL);
    if (!ok)
        NdisLog(L"адаптер: NDS0: ioctl %08X -> ошибка %u", ioctl, GetLastError());
    CloseHandle(n);
    return ok != FALSE;
}

bool NdisUp(const TetherPort& port)
{
    if (s_ad)
        return true;
    s_port = port;
    Keys();
    static const wchar_t names[] = L"VestaRndis\0VestaRndis1\0";   // the miniport, its adapter
    bool ok = Ndis(IOCTL_NDIS_REGISTER_ADAPTER, names, sizeof(names));
    NdisLog(ok ? L"адаптер: зарегистрирован - TCP/IP просит адрес по DHCP" : L"адаптер: NDIS не зарегистрировал");
    return ok;
}

void NdisDown()
{
    static const wchar_t name[] = L"VestaRndis1\0";
    if (s_ad)
        s_ad->halt = true;   // (its thread ends at its next read)
    Ndis(IOCTL_NDIS_DEREGISTER_ADAPTER, name, sizeof(name));
}
