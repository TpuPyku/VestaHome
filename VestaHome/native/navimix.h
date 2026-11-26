// navimix.h - Navitel's voice over the radio (the user 2026-11-26), as the CAN panel's launcher navione does
// (PROJECT.md, navione.exe): while navitel.exe / navitel7.exe runs, SPApi_SendRadioIsNaviSourceCouldIn(1) held -
// and nothing else: navione never sends NaviVoiceGuidance. Our earlier "sound with the radio" (removed 2026-11-08)
// held both, and the card's music played together with Bluetooth's; whether IsNaviSourceCouldIn alone keeps that
// away is the HU's to show. With the shell's window the navigation sound is the shell's.
#pragma once
#include <wchar.h>
#include <string.h>

// the first of the processes (their exe names) that is Navitel, -1 none
inline int NaviMixRunning(const wchar_t (*procs)[40], int n)
{
    for (int i = 0; i < n; i++)
        if (!_wcsicmp(procs[i], L"navitel.exe") || !_wcsicmp(procs[i], L"navitel7.exe"))
            return i;
    return -1;
}

inline bool NaviMixHold(bool on, bool shellWnd, bool navitel)
{
    return on && !shellWnd && navitel;
}
