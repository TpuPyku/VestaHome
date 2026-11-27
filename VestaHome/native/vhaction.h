// vhaction.h - VestaHome's own actions by name (the user 2026-11-27): a user widget's tap=vh:<name> does what a
// VestaHome button does, an add-on asks the same with "do\tname=<name>" (VestaSmartHome: the phone's assistant for
// the gates). VHA_NONE: not one of ours (a script, a program); VHA_UNKNOWN: ours, no such name - told, nothing run.
#pragma once
#include <wchar.h>

enum { VHA_NONE, VHA_UNKNOWN, VHA_ASSISTANT, VHA_REFRESH };
enum { kVhRefreshMs = 15000 };   // a widget's vh:refresh: not more often (its url - often an unofficial one)

inline int VhActionName(const wchar_t* name)
{
    if (!_wcsicmp(name, L"refresh"))   // a widget's data now: its url fetched, its file read (probki.ini)
        return VHA_REFRESH;
    return !_wcsicmp(name, L"assistant") ? VHA_ASSISTANT : VHA_UNKNOWN;   // the phone's assistant by Bluetooth
}

inline int VhActionOf(const wchar_t* tap)
{
    return _wcsnicmp(tap, L"vh:", 3) ? VHA_NONE : VhActionName(tap + 3);
}

// vh:refresh again: last - the widget's last fetch (GetTickCount, 0 - none yet), now - the tick now
inline bool VhRefreshDue(unsigned long last, unsigned long now)
{
    return !last || now - last >= (unsigned long)kVhRefreshMs;
}
