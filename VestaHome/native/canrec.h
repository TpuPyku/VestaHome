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
