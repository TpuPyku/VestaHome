// dialloc.h - the dialing location's area code and country (HKLM\ControlPanel\Dial\Locations\<n>, a multi-string:
// name, local format, long distance, international, area code, call waiting off, country, options). A RAS entry
// with these two dials as a local call: the local format "G" - the number as it is.
#pragma once
#include <windows.h>

// loc: the value's chars (its terminators too). false: fewer fields, the country not a number, the area past areaCap
bool DialLocation(const wchar_t* loc, int chars, wchar_t* area, int areaCap, DWORD* country);
