// UartProbe - does the radio MCU's own UART (MFS ch3) reach one of the unit's ports? The current volume can be set
// only by that protocol's command 0x8010 (the firmware RdDnl.upg, Ghidra 2026-11-11; PROJECT.md) - for the volume by
// speed. COM1 and COM2 (the unit's UART0 / UART1; COM3 is the CAN module's, the others virtual or Bluetooth), at
// 38400 and 19200 8N1: listen 2 s, send the empty id 0FFF (an MCU answers A2 / A3), then 0x8010 with the bad value FF
// (B1 if the command is allowed, FF if locked; the volume not changed). Only after a B1 the volume is set to 8.
// Each port's settings are put back as they were. Every byte to UartProbe.log (VestaLogs on the SD card or a USB
// disk, not with \SDMMC\VestaLogs.off), the summary on the screen.
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "mcuframe.h"
#include "vlogdir.h"

enum { kListenMs = 2000, kReplyMs = 1500, kDoneMs = 3000, kSetVolume = 8 };

static FILE*   s_log;
static wchar_t s_sum[2048];

static void Log(const char* fmt, ...)
{
    if (!s_log)
        return;
    SYSTEMTIME t;
    GetLocalTime(&t);
    fprintf(s_log, "%02d:%02d:%02d ", t.wHour, t.wMinute, t.wSecond);
    va_list a;
    va_start(a, fmt);
    vfprintf(s_log, fmt, a);
    va_end(a);
    fprintf(s_log, "\r\n");
    fflush(s_log);
}

static void LogBytes(const char* what, const unsigned char* b, int n)
{
    char line[3 * 64 + 1];
    int k = 0;
    for (int i = 0; i < n && i < 64; i++)
        k += sprintf(line + k, "%02X ", b[i]);
    line[k] = 0;
    Log("%s (%d): %s%s", what, n, line, n > 64 ? "..." : "");
}

static void Sum(const wchar_t* fmt, ...)
{
    size_t at = wcslen(s_sum);
    va_list a;
    va_start(a, fmt);
    _vsnwprintf(s_sum + at, 2047 - at, fmt, a);
    va_end(a);
    s_sum[2047] = 0;
}

static void OpenLog()
{
    if (GetFileAttributes(L"\\SDMMC\\VestaLogs.off") != 0xFFFFFFFF)
        return;   // (the user's switch: no logs)
    wchar_t dir[MAX_PATH], path[MAX_PATH];
    if (!LogDir(dir, NULL))
        return;
    _snwprintf(path, MAX_PATH, L"%sUartProbe.log", dir);
    s_log = _wfopen(path, L"ab");
}

static int ReadFor(HANDLE h, unsigned char* buf, int cap, DWORD ms)   // all that comes within ms
{
    int n = 0;
    DWORD end = GetTickCount() + ms;
    while ((int)(end - GetTickCount()) > 0 && n < cap) {
        DWORD got = 0;
        if (!ReadFile(h, buf + n, cap - n, &got, NULL))
            break;
        n += (int)got;
        if (!got)
            Sleep(20);
    }
    return n;
}

// A reply in what came, not our own frame echoed back (a loopback); -1 none
static int FindReply(const unsigned char* b, int n, const unsigned char* sent, int sentLen, int* id, int* st)
{
    for (int at = 0; at < n;) {
        int i = McuReply(b + at, n - at, id, st);
        if (i < 0)
            return -1;
        if (sentLen != 7 || memcmp(b + at + i, sent, 7))
            return *st;
        at += i + 1;
    }
    return -1;
}

static int Ask(HANDLE h, int cmd, const unsigned char* data, int n, DWORD ms, const char* what)
{
    unsigned char f[16], in[512];
    int len = McuFrame(cmd, data, n, f), id, st;
    PurgeComm(h, PURGE_RXCLEAR);
    DWORD wr = 0;
    BOOL ok = WriteFile(h, f, len, &wr, NULL);
    LogBytes(what, f, len);
    if (!ok || (int)wr != len)
        Log("  write failed (%lu, %lu of %d)", GetLastError(), wr, len);
    int got = ReadFor(h, in, sizeof(in), ms);
    LogBytes("  in", in, got);
    int r = FindReply(in, got, f, len, &id, &st);
    if (r >= 0)
        Log("  MCU reply: id %04X status %02X", id, st);
    return r >= 0 && id == cmd ? st : -1;
}

static void Probe(const wchar_t* port, const char* name, DWORD baud)
{
    HANDLE h = CreateFile(port, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        Log("%s: not opened (%lu)", name, GetLastError());
        Sum(L"%s: не открылся (%lu)\n", port, GetLastError());
        return;
    }
    DCB was, d;
    memset(&was, 0, sizeof(was));
    was.DCBlength = sizeof(was);
    BOOL have = GetCommState(h, &was);
    d = was;
    d.BaudRate = baud;
    d.ByteSize = 8;
    d.Parity = NOPARITY;
    d.StopBits = ONESTOPBIT;
    d.fBinary = TRUE;
    d.fParity = FALSE;
    d.fOutxCtsFlow = d.fOutxDsrFlow = FALSE;
    d.fDtrControl = DTR_CONTROL_ENABLE;
    d.fRtsControl = RTS_CONTROL_ENABLE;
    d.fOutX = d.fInX = FALSE;
    COMMTIMEOUTS to = { MAXDWORD, 0, 0, 0, 500 };
    if (!have || !SetCommState(h, &d) || !SetCommTimeouts(h, &to)) {
        Log("%s: settings refused (%lu)", name, GetLastError());
        Sum(L"%s %lu: настройки не приняты\n", port, baud);
        if (have)
            SetCommState(h, &was);
        CloseHandle(h);
        return;
    }
    Log("%s %lu 8N1 (was %lu)", name, baud, was.BaudRate);
    unsigned char in[512];
    int got = ReadFor(h, in, sizeof(in), kListenMs);
    LogBytes("  heard by itself", in, got);
    int st = Ask(h, 0x0FFF, NULL, 0, kReplyMs, "  -> 0FFF (none)");
    const wchar_t* res = L"нет ответа";
    if (st >= 0) {
        unsigned char bad = 0xFF;
        int s2 = Ask(h, 0x8010, &bad, 1, kReplyMs, "  -> 8010 FF (bad value)");
        if (s2 == 0xB1) {
            unsigned char v = kSetVolume;
            int s3 = Ask(h, 0x8010, &v, 1, kDoneMs, "  -> 8010 08 (the volume 8)");
            res = s3 == 0 || s3 == 1 ? L"MCU ОТВЕТИЛ, громкость 8 послана" : L"MCU ответил, 8010 разрешена, громкость не подтверждена";
        } else {
            res = s2 == 0xFF ? L"MCU ответил, 8010 заблокирована (FF)" : L"MCU ответил на 0FFF, на 8010 - нет";
        }
    } else if (got) {
        res = L"что-то шлёт, но не MCU";
    }
    Sum(L"%s %lu: %s (слышно %d Б)\n", port, baud, res, got);
    SetCommState(h, &was);
    CloseHandle(h);
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPTSTR, int)
{
    OpenLog();
    Log("UartProbe: the radio MCU's UART on COM1 / COM2");
    static const wchar_t* ports[2] = { L"COM1:", L"COM2:" };
    static const char* names[2] = { "COM1", "COM2" };
    static const DWORD bauds[2] = { 38400, 19200 };
    for (int p = 0; p < 2; p++)
        for (int b = 0; b < 2; b++)
            Probe(ports[p], names[p], bauds[b]);
    Sum(s_log ? L"\nЖурнал: VestaLogs\\UartProbe.log" : L"\nЖурнала нет (нет SD/USB или журналы выключены)");
    Log("done");
    if (s_log)
        fclose(s_log);
    MessageBox(NULL, s_sum, L"Проба порта MCU", MB_OK | MB_TOPMOST);
    return 0;
}
