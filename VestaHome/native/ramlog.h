// ramlog.h - VestaHome's log while no card is there (the user 2026-11-23: with Android Auto on, the SD card pulled -
// VestaHome fell, its crash note never written: logs go only to removable media, the phone takes the only USB port).
// As VestaStub does: the lines wait in RAM (\Temp - the object store, not the flash) and go onto the card's log as
// soon as it is there. Bounded: two files of kRamLogHalf at the most - the current one and the one before it.
#ifndef RAMLOG_H
#define RAMLOG_H

#include <windows.h>

enum { kRamLogHalf = 32 * 1024 };

// A line of `add` bytes for a RAM file of `size` bytes: true - it goes to a new file (the full one becomes the old)
static bool RamLogTurn(DWORD size, DWORD add)
{
    return size + add > (DWORD)kRamLogHalf;
}

// The line into the RAM log `ram` (`old` - the one before it)
void RamLogPut(const wchar_t* ram, const wchar_t* old, const char* line, DWORD n);
// The RAM log (the old one first) appended to the open file h, both files removed; the bytes copied
DWORD RamLogFlush(const wchar_t* ram, const wchar_t* old, HANDLE h);

#endif
