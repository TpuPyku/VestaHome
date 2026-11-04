// soundmix.h - the programs whose sound goes with the radio (the user 2026-11-04: a navigator's voice heard over
// the radio). Kept as their exe names in one line (ini sound.mix: "Navitel.exe;CityGuide.exe"); while one of them
// runs VestaHome holds the MCU's navigation channel open, as the stock shell does for its external navigator.
#pragma once
#include <windows.h>

enum { kMixList = 200, kMixProcName = 40 };

const wchar_t* MixExeOf(const wchar_t* path);   // "\SDMMC\Navitel\Navitel.exe" -> "Navitel.exe"
bool MixListHas(const wchar_t* list, const wchar_t* exe);   // case does not matter
// exe in (on) or out of the list; false: it did not fit (the list as it was)
bool MixListSet(wchar_t* list, int cap, const wchar_t* exe, bool on);
// the first of the processes (their exe names) that is in the list, -1 none
int MixRunning(const wchar_t* list, const wchar_t (*procs)[kMixProcName], int n);
