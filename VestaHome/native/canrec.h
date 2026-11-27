// canrec.h - the CAN panel's COM3 recorded raw (branch canrec, the user 2026-11-26): its frames come only from COM3,
// which can_panel.exe holds while it runs, and its exe cannot be renamed then - so VestaHome ends the panel, reads
// COM3 for a few minutes into the SD card, and starts the panel again from its place (\ResidentFlash\CAN). Never
// SwUpdate.flag (Init.exe skips the panel while it exists, but Main.exe / SwDnl read it as "an update runs").
// The file: chunks as read, each [ms since the start: u32 LE][length: u16 LE][bytes], to line the frames up with
// can.log (the panel's NMEA on COM4, recorded after it is back).
#pragma once
#include <windows.h>
#include <stdio.h>
#include <wchar.h>
#include <string.h>
#include "vcan3.h"   // VestaCore: the frames decoded (the live values)

// the panel among the processes (their exe names), -1 none
inline int CanRecPanelAt(const wchar_t (*procs)[40], int n)
{
    for (int i = 0; i < n; i++)
        if (!_wcsicmp(procs[i], L"can_panel.exe"))
            return i;
    return -1;
}

// one chunk into out (6 + len bytes); 0 when nothing was read
inline int CanRecPack(unsigned char* out, DWORD ms, const unsigned char* data, int len)
{
    if (len <= 0)
        return 0;
    out[0] = (unsigned char)ms;
    out[1] = (unsigned char)(ms >> 8);
    out[2] = (unsigned char)(ms >> 16);
    out[3] = (unsigned char)(ms >> 24);
    out[4] = (unsigned char)len;
    out[5] = (unsigned char)(len >> 8);
    memcpy(out + 6, data, len);
    return 6 + len;
}

inline void CanRecName(wchar_t* out, int cap, const SYSTEMTIME* t)
{
    _snwprintf(out, cap, L"com3_%02d%02d%02d_%02d%02d%02d.bin", t->wYear % 100, t->wMonth, t->wDay, t->wHour,
               t->wMinute, t->wSecond);
    out[cap - 1] = 0;
}

// The user 2026-11-27: the recording till "stop" (a drive with the doors, the wheel, the reverse), 30 min at most
// (~3 MB at the panel's 1.7 KB/s).
enum { kCanRecMs = 30 * 60 * 1000 };

// Marks pressed while recording - what was done when - into <recording>_marks.txt beside it, one line each:
// "<ms since the start> <kind>"; the recording's own format stays.
enum { CRM_DOORS, CRM_WHEEL, CRM_REVERSE, CRM_DRIVE, CRM_COUNT };

inline void CanRecMarkPath(wchar_t* out, int cap, const wchar_t* bin)
{
    int n = (int)wcslen(bin);
    if (n >= 4 && !_wcsicmp(bin + n - 4, L".bin"))
        n -= 4;
    _snwprintf(out, cap, L"%.*s_marks.txt", n, bin);
    out[cap - 1] = 0;
}

inline void CanRecMarkLine(char* out, int cap, DWORD ms, int kind)
{
    static const char* const names[CRM_COUNT] = { "doors", "wheel", "reverse", "drive" };
    _snprintf(out, cap, "%lu %s\r\n", ms, names[kind]);
    out[cap - 1] = 0;
}

// The decoded values as two lines for the screen; a dash: not come yet. The GPS speed from knots x 100 (x 1.852).
inline void CanRecLive(wchar_t* a, int na, wchar_t* b, int nb, const VCan3* c)
{
    wchar_t sp[24] = L"—", st[16] = L"—", ge[8] = L"—", dr[8] = L"—", ss[8] = L"—", gps[48] = L"нет";
    if (c->hasSpeed)
        _snwprintf(sp, 24, L"%d,%d км/ч", c->speed10 / 10, c->speed10 % 10);
    if (c->hasSteer)
        _snwprintf(st, 16, L"%d", c->steer);
    if (c->hasGear)
        _snwprintf(ge, 8, L"%d", c->gear);
    if (c->hasDoors)
        _snwprintf(dr, 8, L"%02X", c->doors);
    if (c->hasStatus)
        _snwprintf(ss, 8, L"%04X", c->status);
    int kmh10 = c->kn100 * 1852 / 10000;
    if (c->hasPos)
        _snwprintf(gps, 48, c->hasMove ? L"%d,%d км/ч, курс %d°" : L"есть", kmh10 / 10, kmh10 % 10, c->course100 / 100);
    _snwprintf(a, na, L"Скорость %s · руль %s · передача %s", sp, st, ge);
    a[na - 1] = 0;
    _snwprintf(b, nb, L"Двери %s · статус %s · GPS %s · кадров %lu", dr, ss, gps, c->frames);
    b[nb - 1] = 0;
}
