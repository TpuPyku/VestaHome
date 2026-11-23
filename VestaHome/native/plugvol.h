// plugvol.h - the volumes VestaHome starts its add-ons from (the user 2026-11-23): only the unit's own, \ResidentFlash
// and \SDMMC. An add-on started from the SD card or a USB disk dies when it is taken out - CE pages a program's code in
// from its file (the tester 2026-11-23: VestaAuto under the wrong folder on \ResidentFlash, the card's copy ran;
// the card pulled during Android Auto - VestaHome fell).
#pragma once
#include <windows.h>
#include <string.h>

inline bool PlugVolumeOk(const wchar_t* exe)
{
    static const wchar_t* ok[2] = { L"\\ResidentFlash\\", L"\\SDMMC\\" };
    for (int i = 0; i < 2; i++)
        if (!_wcsnicmp(exe, ok[i], wcslen(ok[i])))
            return true;
    return false;
}
