// VestaNMEA.dll - VestaHome's NMEA port for the navigators (2026-10-22, the user: "like GpsGate, only NMEA on
// COM6"). A Windows CE stream driver (prefix COM) loaded as a user-mode driver (udevice.exe) by VestaHome
// (vh_gps.inc: NmeaPortStart). The unit's GPS port (COM5 = a byte copy of the CAN module's stream) carries the CAN
// module's own '@' lines besides NMEA; VestaCore's reader (VestaHome or OSMapsCE) publishes every checked line in
// the shared memory "VestaGPS" (vgps.h: GpsShm). This port gives out only the '$' lines of it, each with CR LF;
// every program that opens it reads the whole stream from the moment it opened (several at once).
//
// A serial port to the programs: ReadFile with the COMMTIMEOUTS set, WaitCommEvent (EV_RXCHAR), ClearCommError
// (the bytes waiting), Get/SetCommState, the modem lines, purge: all taken, nothing to set up (no wire). Writes are
// swallowed. No data while no program reads the GPS port. Imports COREDLL only; no log (the user-mode driver's
// process is not ours to write files from).
#include <windows.h>
#include <string.h>
#include "vgps.h"

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

enum { kPend = 2048, kPollMs = 50 };

struct Port {                    // the device (COM_Init)
    HANDLE  map;
    GpsShm* shm;
    DCB     dcb;                 // what the programs set: given back as set
};

struct Open {                    // one CreateFile of it (COM_Open)
    Port*            port;
    CRITICAL_SECTION cs;
    LONG             seen;       // the shared lines taken so far (GpsShm.lineSeq)
    char             pend[kPend];   // taken, not read yet
    int              pendPos, pendLen;
    COMMTIMEOUTS     to;
    DWORD            mask;       // SetCommMask
    volatile LONG    maskSeq;    // a new mask (or a purge, the close) ends a WaitCommEvent
    volatile LONG    busy;       // calls inside (the close waits for them)
    volatile bool    closing;
};

static GpsShm* Shm(Port* p)   // the shared memory, once its reader made it
{
    if (!p->shm) {
        p->map = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(GpsShm), VGPS_SHM_NAME);
        if (p->map)
            p->shm = (GpsShm*)MapViewOfFile(p->map, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(GpsShm));
    }
    return p->shm && p->shm->magic == VGPS_MAGIC ? p->shm : NULL;
}

// The new '$' lines into the open's buffer (under its cs); the bytes waiting
static int Take(Open* o)
{
    GpsShm* s = Shm(o->port);
    if (!s)
        return o->pendLen - o->pendPos;
    LONG n = s->lineSeq;
    if (n - o->seen > kShmLines || n < o->seen)   // fell behind a lap, or the reader started again
        o->seen = n - kShmLines < 0 ? 0 : n - kShmLines;
    if (o->pendPos && o->pendPos == o->pendLen)
        o->pendPos = o->pendLen = 0;
    for (; o->seen < n; o->seen++) {
        const char* line = s->lines[o->seen % kShmLines];
        if (line[0] != '$')
            continue;   // the CAN module's "@" lines
        int len = 0;
        while (len < kShmLineLen - 1 && line[len])
            len++;
        if (o->pendLen + len + 2 > kPend) {
            if (o->pendPos == 0)
                break;   // full: the rest waits for a read
            memmove(o->pend, o->pend + o->pendPos, o->pendLen - o->pendPos);
            o->pendLen -= o->pendPos;
            o->pendPos = 0;
            if (o->pendLen + len + 2 > kPend)
                break;
        }
        memcpy(o->pend + o->pendLen, line, len);
        o->pend[o->pendLen + len] = '\r';
        o->pend[o->pendLen + len + 1] = '\n';
        o->pendLen += len + 2;
    }
    return o->pendLen - o->pendPos;
}

static int Waiting(Open* o)
{
    EnterCriticalSection(&o->cs);
    int n = Take(o);
    LeaveCriticalSection(&o->cs);
    return n;
}

// A trace of the loading (HU 2026-10-25: ActivateDeviceEx failed - 2 in udevice.exe, 1 in device.exe - and it is
// not known whether this DLL is loaded at all): one line per step into \SDMemory\VestaLogs\VestaNMEA.log, ASCII,
// none when \SDMMC\VestaLogs.off is there or the card is not.
static void Trace(const char* what, DWORD v)
{
#ifdef UNDER_CE
    if (GetFileAttributes(L"\\SDMMC\\VestaLogs.off") != 0xFFFFFFFF ||
        GetFileAttributes(L"\\SDMemory\\VestaLogs") == 0xFFFFFFFF)
        return;
    HANDLE f = CreateFile(L"\\SDMemory\\VestaLogs\\VestaNMEA.log", GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS,
                          0, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return;
    char line[96];
    int n = 0;
    DWORD t = GetTickCount(), pid = GetCurrentProcessId();
    const char* hex = "0123456789ABCDEF";
    for (int i = 9; i >= 0; i--, t /= 10)
        line[n + i] = (char)('0' + t % 10);
    n += 10;
    line[n++] = ' ';
    for (int i = 0; i < 8; i++)
        line[n++] = hex[pid >> (28 - 4 * i) & 15];
    line[n++] = ' ';
    while (*what && n < 70)
        line[n++] = *what++;
    line[n++] = ' ';
    for (int i = 0; i < 8; i++)
        line[n++] = hex[v >> (28 - 4 * i) & 15];
    line[n++] = '\r';
    line[n++] = '\n';
    DWORD w;
    SetFilePointer(f, 0, NULL, FILE_END);
    WriteFile(f, line, n, &w, NULL);
    CloseHandle(f);
#else
    (void)what;
    (void)v;
#endif
}

extern "C" {

BOOL WINAPI DllMain(HANDLE, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
        Trace("DllMain attach", reason);
    return TRUE;
}

DWORD COM_Init(LPCTSTR active, LPCVOID)
{
    Trace("COM_Init", (DWORD)active);
    Port* p = (Port*)LocalAlloc(LPTR, sizeof(Port));
    if (!p)
        return 0;
    p->dcb.DCBlength = sizeof(DCB);
    p->dcb.BaudRate = 115200;
    p->dcb.fBinary = TRUE;
    p->dcb.fDtrControl = DTR_CONTROL_ENABLE;
    p->dcb.fRtsControl = RTS_CONTROL_ENABLE;
    p->dcb.ByteSize = 8;
    p->dcb.Parity = NOPARITY;
    p->dcb.StopBits = ONESTOPBIT;
    return (DWORD)p;
}

BOOL COM_Deinit(DWORD dev)
{
    Port* p = (Port*)dev;
    if (!p)
        return FALSE;
    if (p->shm)
        UnmapViewOfFile(p->shm);
    if (p->map)
        CloseHandle(p->map);
    LocalFree(p);
    return TRUE;
}

DWORD COM_Open(DWORD dev, DWORD, DWORD)
{
    Port* p = (Port*)dev;
    Open* o = p ? (Open*)LocalAlloc(LPTR, sizeof(Open)) : NULL;
    if (!o)
        return 0;
    o->port = p;
    InitializeCriticalSection(&o->cs);
    GpsShm* s = Shm(p);
    o->seen = s ? s->lineSeq : 0;   // from now on
    return (DWORD)o;
}

// (CE 6) The device is going: nothing to stop before COM_Deinit. Exported because COM_PreClose is: the device manager
// takes a driver with one of the two pre-calls and not the other as broken - the reason this port never came up
// (HU 2026-10-24..27: the DLL loaded in udevice.exe / device.exe, COM_Init never called, ActivateDeviceEx 2 / 1;
// GpsGate's bizVSerialCE.dll, which worked on the unit, exports neither; found 2026-10-27).
BOOL COM_PreDeinit(DWORD dev)
{
    Trace("COM_PreDeinit", dev);
    return TRUE;
}

BOOL COM_PreClose(DWORD h)   // (CE 6) the handle is going: its waits end
{
    Open* o = (Open*)h;
    if (o) {
        o->closing = true;
        InterlockedIncrement((LONG*)&o->maskSeq);
    }
    return TRUE;
}

BOOL COM_Close(DWORD h)
{
    Open* o = (Open*)h;
    if (!o)
        return FALSE;
    COM_PreClose(h);
    for (int i = 0; i < 40 && o->busy; i++)   // a read or a wait inside: at most two polls more
        Sleep(kPollMs);
    if (o->busy)
        return TRUE;   // still inside: rather leak it than free it under them
    DeleteCriticalSection(&o->cs);
    LocalFree(o);
    return TRUE;
}

// ReadFile: what waits, else as COMMTIMEOUTS say: MAXDWORD / 0 / 0 - at once; all 0 - until something comes;
// else up to multiplier x len + constant ms. Something came - it is given at once (lines, not single bytes).
DWORD COM_Read(DWORD h, LPVOID buf, DWORD len)
{
    Open* o = (Open*)h;
    if (!o || !buf)
        return (DWORD)-1;
    InterlockedIncrement((LONG*)&o->busy);
    DWORD total = o->to.ReadTotalTimeoutMultiplier * len + o->to.ReadTotalTimeoutConstant;
    bool now = o->to.ReadIntervalTimeout == MAXDWORD && !o->to.ReadTotalTimeoutMultiplier &&
               !o->to.ReadTotalTimeoutConstant;
    bool forever = !now && !total;
    DWORD t0 = GetTickCount();
    LONG mseq = o->maskSeq;
    int got = 0;
    for (;;) {
        EnterCriticalSection(&o->cs);
        int n = Take(o);
        if (n > 0) {
            got = n < (int)len ? n : (int)len;
            memcpy(buf, o->pend + o->pendPos, got);
            o->pendPos += got;
        }
        LeaveCriticalSection(&o->cs);
        if (got || now || o->closing || o->maskSeq != mseq || (!forever && GetTickCount() - t0 >= total))
            break;
        Sleep(kPollMs);
    }
    InterlockedDecrement((LONG*)&o->busy);
    return (DWORD)got;
}

DWORD COM_Write(DWORD, LPCVOID, DWORD len)   // nothing listens: taken whole
{
    return len;
}

DWORD COM_Seek(DWORD, long, DWORD)
{
    return (DWORD)-1;
}

static bool Out(PBYTE out, DWORD outLen, PDWORD actual, const void* v, DWORD n)
{
    if (!out || outLen < n)
        return false;
    memcpy(out, v, n);
    if (actual)
        *actual = n;
    return true;
}

BOOL COM_IOControl(DWORD h, DWORD code, PBYTE in, DWORD inLen, PBYTE out, DWORD outLen, PDWORD actual)
{
    Open* o = (Open*)h;
    if (!o)
        return FALSE;
    if (actual)
        *actual = 0;
    InterlockedIncrement((LONG*)&o->busy);
    BOOL ok = TRUE;
    switch (code) {
    case IOCTL_SER_SET_TIMEOUTS:
        if (in && inLen >= sizeof(COMMTIMEOUTS))
            memcpy(&o->to, in, sizeof(COMMTIMEOUTS));
        break;
    case IOCTL_SER_GET_TIMEOUTS:
        ok = Out(out, outLen, actual, &o->to, sizeof(COMMTIMEOUTS));
        break;
    case IOCTL_SER_SET_DCB:   // kept to be given back; there is no wire to set
        if (in && inLen >= sizeof(DCB))
            memcpy(&o->port->dcb, in, sizeof(DCB));
        break;
    case IOCTL_SER_GET_DCB:
        ok = Out(out, outLen, actual, &o->port->dcb, sizeof(DCB));
        break;
    case IOCTL_SER_SET_WAIT_MASK:
        if (in && inLen >= sizeof(DWORD))
            o->mask = *(DWORD*)in;
        InterlockedIncrement((LONG*)&o->maskSeq);   // ends a WaitCommEvent going on
        break;
    case IOCTL_SER_GET_WAIT_MASK:
        ok = Out(out, outLen, actual, &o->mask, sizeof(DWORD));
        break;
    case IOCTL_SER_WAIT_ON_MASK: {   // WaitCommEvent: EV_RXCHAR when something waits; 0 when the mask changed
        DWORD ev = 0;
        LONG mseq = o->maskSeq;
        while (!o->closing && o->maskSeq == mseq) {
            if ((o->mask & EV_RXCHAR) && Waiting(o) > 0) {
                ev = EV_RXCHAR;
                break;
            }
            if (!(o->mask & EV_RXCHAR) && !o->mask)
                break;   // nothing to wait for
            Sleep(kPollMs);
        }
        ok = Out(out, outLen, actual, &ev, sizeof(DWORD));
        break;
    }
    case IOCTL_SER_GET_COMMSTATUS: {   // ClearCommError: no errors, the bytes waiting
        SerialDevStatus st;
        memset(&st, 0, sizeof(st));
        st.stat.cbInQue = (DWORD)Waiting(o);
        ok = Out(out, outLen, actual, &st, sizeof(st));
        break;
    }
    case IOCTL_SER_GET_MODEMSTATUS: {   // the other end "there"
        DWORD ms = MS_CTS_ON | MS_DSR_ON | MS_RLSD_ON;
        ok = Out(out, outLen, actual, &ms, sizeof(DWORD));
        break;
    }
    case IOCTL_SER_GET_PROPERTIES: {
        COMMPROP cp;
        memset(&cp, 0, sizeof(cp));
        cp.wPacketLength = sizeof(cp);
        cp.dwMaxBaud = BAUD_USER;
        cp.dwProvSubType = PST_RS232;
        cp.dwProvCapabilities = PCF_TOTALTIMEOUTS | PCF_INTTIMEOUTS;
        cp.dwSettableParams = SP_BAUD | SP_DATABITS | SP_PARITY | SP_STOPBITS;
        cp.dwSettableBaud = BAUD_4800 | BAUD_9600 | BAUD_19200 | BAUD_38400 | BAUD_57600 | BAUD_115200 | BAUD_USER;
        cp.wSettableData = DATABITS_8;
        cp.wSettableStopParity = STOPBITS_10 | PARITY_NONE;
        cp.dwCurrentTxQueue = cp.dwCurrentRxQueue = kPend;
        ok = Out(out, outLen, actual, &cp, sizeof(cp));
        break;
    }
    case IOCTL_SER_PURGE:   // PURGE_RXCLEAR: what waits goes; the reads / waits going on end
        if (in && inLen >= sizeof(DWORD) && (*(DWORD*)in & (PURGE_RXCLEAR | PURGE_RXABORT))) {
            EnterCriticalSection(&o->cs);
            Take(o);
            o->pendPos = o->pendLen = 0;
            LeaveCriticalSection(&o->cs);
            if (*(DWORD*)in & PURGE_RXABORT)
                InterlockedIncrement((LONG*)&o->maskSeq);
        }
        break;
    default:   // DTR / RTS, break, XON / XOFF, queue sizes...: taken
        break;
    }
    if (!ok)
        SetLastError(ERROR_INVALID_PARAMETER);
    InterlockedDecrement((LONG*)&o->busy);
    return ok;
}

void COM_PowerUp(DWORD)
{
}

void COM_PowerDown(DWORD)
{
}

}   // extern "C"
