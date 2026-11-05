// dialloc.h - the dialing location's area code and country (HKLM\ControlPanel\Dial\Locations\<n>, a multi-string:
// name, local format, long distance, international, area code, call waiting off, country, options). A RAS entry
// with these two dials as a local call: the local format "G" - the number as it is.
#pragma once
#include <windows.h>

// loc: the value's chars (its terminators too). false: fewer fields, the country not a number, the area past areaCap
bool DialLocation(const wchar_t* loc, int chars, wchar_t* area, int areaCap, DWORD* country);

// The modem ready to dial (the driver's Rssi 0..31, 99 unknown; Huawei's sys_mode, 0 no service; Sim 255 none):
// a signal and a network found (HU 2026-11-05: dialled at "99, 0" the link hung in PPP for good)
bool DialReady(int rssi, int mode, int sim);
