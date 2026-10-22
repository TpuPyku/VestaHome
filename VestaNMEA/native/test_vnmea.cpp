// test_vnmea.cpp - the NMEA port's logic on the PC: a made-up "VestaGPS" shared memory with NMEA and '@' lines,
// the port opened twice, read with the timeouts navigators use, WaitCommEvent, ClearCommError, a lap overrun.
#include "vnmea.cpp"
#include <stdio.h>

static int s_fails;

static void Check(bool ok, const char* what)
{
    printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok)
        s_fails++;
}

static GpsShm* s_shm;

static void Put(const char* line)   // as VestaCore's PublishLine
{
    LONG n = s_shm->lineSeq + 1;
    strcpy(s_shm->lines[(n - 1) % kShmLines], line);
    InterlockedExchange((LONG*)&s_shm->lineSeq, n);
}

static DWORD Ctl(DWORD h, DWORD code, const void* in, DWORD inLen, void* out, DWORD outLen)
{
    DWORD got = 0;
    COM_IOControl(h, code, (PBYTE)in, inLen, (PBYTE)out, outLen, &got);
    return got;
}

static DWORD WINAPI LateLine(LPVOID)
{
    Sleep(300);
    Put("$GPRMC,LATE*00");
    return 0;
}

int main()
{
    HANDLE map = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(GpsShm), VGPS_SHM_NAME);
    s_shm = (GpsShm*)MapViewOfFile(map, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(GpsShm));
    s_shm->magic = VGPS_MAGIC;
    s_shm->version = 1;
    Put("$GPGGA,OLD*00");   // before the open: not given

    DWORD dev = COM_Init(L"", NULL);
    DWORD a = COM_Open(dev, GENERIC_READ, 0), b = COM_Open(dev, GENERIC_READ, 0);
    COMMTIMEOUTS now = { MAXDWORD, 0, 0, 0, 0 };
    Ctl(a, IOCTL_SER_SET_TIMEOUTS, &now, sizeof(now), NULL, 0);
    char buf[4096];
    Check(COM_Read(a, buf, sizeof(buf)) == 0, "nothing yet: at once 0");

    Put("$GPGGA,1*00");
    Put("@,060,1.19*00");
    Put("@ANG,+012.3,*00");
    Put("$GPRMC,2*00");
    SerialDevStatus st;
    Ctl(a, IOCTL_SER_GET_COMMSTATUS, NULL, 0, &st, sizeof(st));
    Check(st.stat.cbInQue == 26, "ClearCommError: 26 bytes waiting (two lines with CR LF)");
    DWORD n = COM_Read(a, buf, sizeof(buf));
    buf[n] = 0;
    Check(!strcmp(buf, "$GPGGA,1*00\r\n$GPRMC,2*00\r\n"), "only the $ lines, CR LF, '@' left out");

    n = COM_Read(b, buf, 5);   // the other open: its own copy, in parts
    buf[n] = 0;
    Check(n == 5 && !strcmp(buf, "$GPGG"), "second open: its own stream, a short read");
    n = COM_Read(b, buf, sizeof(buf));
    buf[n] = 0;
    Check(!strcmp(buf, "A,1*00\r\n$GPRMC,2*00\r\n"), "second open: the rest");

    COMMTIMEOUTS wait = { 0, 0, 1000, 0, 0 };   // up to 1 s
    Ctl(a, IOCTL_SER_SET_TIMEOUTS, &wait, sizeof(wait), NULL, 0);
    DWORD t0 = GetTickCount();
    n = COM_Read(a, buf, sizeof(buf));
    Check(n == 0 && GetTickCount() - t0 >= 900, "timeout 1 s: waits, then 0");

    CloseHandle(CreateThread(NULL, 0, LateLine, NULL, 0, NULL));
    DWORD mask = EV_RXCHAR, ev = 0;
    Ctl(a, IOCTL_SER_SET_WAIT_MASK, &mask, sizeof(mask), NULL, 0);
    t0 = GetTickCount();
    Ctl(a, IOCTL_SER_WAIT_ON_MASK, NULL, 0, &ev, sizeof(ev));
    Check(ev == EV_RXCHAR && GetTickCount() - t0 >= 250, "WaitCommEvent: EV_RXCHAR when a line comes");
    n = COM_Read(a, buf, sizeof(buf));
    buf[n] = 0;
    Check(!strcmp(buf, "$GPRMC,LATE*00\r\n"), "the late line read");

    for (int i = 0; i < 200; i++) {   // more than a lap behind: the last 64 only
        char s[32];
        sprintf(s, "$GPGGA,%d*00", i);
        Put(s);
    }
    Ctl(a, IOCTL_SER_SET_TIMEOUTS, &now, sizeof(now), NULL, 0);
    int lines = 0;
    while ((n = COM_Read(a, buf, sizeof(buf))) > 0)
        for (DWORD i = 0; i < n; i++)
            lines += buf[i] == '\n';
    Check(lines == 64, "fell a lap behind: the last 64 lines");

    DCB d;
    memset(&d, 0, sizeof(d));
    Ctl(a, IOCTL_SER_GET_DCB, NULL, 0, &d, sizeof(d));
    Check(d.BaudRate == 115200 && d.ByteSize == 8, "GetCommState: 115200 8N1");
    d.BaudRate = 4800;
    Ctl(a, IOCTL_SER_SET_DCB, &d, sizeof(d), NULL, 0);
    memset(&d, 0, sizeof(d));
    Ctl(b, IOCTL_SER_GET_DCB, NULL, 0, &d, sizeof(d));
    Check(d.BaudRate == 4800, "SetCommState kept (4800)");
    Check(COM_Write(a, "x", 1) == 1, "write swallowed");

    COM_Close(a);
    COM_Close(b);
    COM_Deinit(dev);
    printf(s_fails ? "%d FAILED\n" : "all ok\n", s_fails);
    return s_fails;
}
