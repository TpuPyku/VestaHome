// saverink.h - the screen saver's text colours (the user 2026-11-26: at night in full darkness the clock dazzled):
// at night each channel at 40 %, by day as they are.
#pragma once
#include <windows.h>

inline COLORREF SaverInk(COLORREF c, bool night)
{
    if (!night)
        return c;
    return RGB(GetRValue(c) * 40 / 100, GetGValue(c) * 40 / 100, GetBValue(c) * 40 / 100);
}
