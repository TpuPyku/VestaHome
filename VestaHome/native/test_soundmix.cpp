// test_soundmix.cpp - the programs whose sound goes with the radio (soundmix.cpp) on the PC (test_soundmix.bat).
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "soundmix.h"

static int s_failed, s_checks;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAIL: %s\n", what);
    }
}

static void ExeOf()
{
    Check(!wcscmp(MixExeOf(L"\\SDMMC\\Navitel\\Navitel.exe"), L"Navitel.exe"), "exe: the file name of a path");
    Check(!wcscmp(MixExeOf(L"Navitel.exe"), L"Navitel.exe"), "exe: a bare name as it is");
    Check(!wcscmp(MixExeOf(L"\\a/b.exe"), L"b.exe"), "exe: a forward slash too");
    Check(!wcscmp(MixExeOf(L""), L""), "exe: empty");
}

static void Has()
{
    Check(MixListHas(L"Navitel.exe;CityGuide.exe", L"navitel.EXE"), "has: the first, any case");
    Check(MixListHas(L"Navitel.exe;CityGuide.exe", L"CityGuide.exe"), "has: the last");
    Check(!MixListHas(L"Navitel.exe;CityGuide.exe", L"Navi.exe"), "has: a part of a name is not it");
    Check(!MixListHas(L"Navitel.exe", L"Navitel.ex"), "has: a shorter name is not it");
    Check(!MixListHas(L"", L"Navitel.exe"), "has: an empty list");
    Check(!MixListHas(L"Navitel.exe", L""), "has: no name");
}

static void Set()
{
    wchar_t l[kMixList] = L"";
    Check(MixListSet(l, kMixList, L"Navitel.exe", true) && !wcscmp(l, L"Navitel.exe"), "set: into an empty list");
    Check(MixListSet(l, kMixList, L"CityGuide.exe", true) && !wcscmp(l, L"Navitel.exe;CityGuide.exe"),
          "set: added after a ';'");
    Check(MixListSet(l, kMixList, L"NAVITEL.exe", true) && !wcscmp(l, L"Navitel.exe;CityGuide.exe"),
          "set: there already - not twice");
    Check(MixListSet(l, kMixList, L"navitel.exe", false) && !wcscmp(l, L"CityGuide.exe"), "set: the first out");
    Check(MixListSet(l, kMixList, L"Navi.exe", false) && !wcscmp(l, L"CityGuide.exe"), "set: not there - as it was");
    Check(MixListSet(l, kMixList, L"CityGuide.exe", false) && !wcscmp(l, L""), "set: the last out - empty");
    wcscpy(l, L"A.exe;B.exe;C.exe");
    Check(MixListSet(l, kMixList, L"B.exe", false) && !wcscmp(l, L"A.exe;C.exe"), "set: the middle out");
    wchar_t s[12] = L"A.exe";
    Check(!MixListSet(s, 12, L"BB.exe", true) && !wcscmp(s, L"A.exe"), "set: does not fit - as it was");
    Check(MixListSet(s, 12, L"B.exe", true) && !wcscmp(s, L"A.exe;B.exe"), "set: just fits");
    Check(!MixListSet(s, 12, L"", true) && !wcscmp(s, L"A.exe;B.exe"), "set: no name - nothing");
    wchar_t e[6] = L"";
    Check(MixListSet(e, 6, L"A.exe", true) && !wcscmp(e, L"A.exe"), "set: the first just fits (no ';' counted)");
    wchar_t o[8] = L"";
    Check(MixListSet(o, 8, L"x", true) && !wcscmp(o, L"x"), "set: a one-letter name");
    wchar_t r[16] = L"A.exe;B.exe";
    r[12] = L'Z';   // past the end: not copied in
    Check(MixListSet(r, 16, L"A.exe", false) && !wcscmp(r, L"B.exe") && r[6] == L'B', "set: out - only the rest moved");
}

static void Running()
{
    wchar_t p[3][kMixProcName] = { L"NK.EXE", L"navitel.exe", L"VestaHome.exe" };
    Check(MixRunning(L"Navitel.exe", p, 3) == 1, "running: found, any case");
    Check(MixRunning(L"CityGuide.exe", p, 3) == -1, "running: not running");
    Check(MixRunning(L"", p, 3) == -1, "running: an empty list");
    Check(MixRunning(L"VestaHome.exe;Navitel.exe", p, 3) == 1, "running: the first process in the list");
    Check(MixRunning(L"Navitel.exe", p, 0) == -1, "running: no processes");
    Check(MixRunning(L"NK.EXE", p, 3) == 0, "running: the first process");
    Check(MixRunning(L"navitel.exe", p, 1) == -1, "running: only the n given");
}

int main()
{
    ExeOf();
    Has();
    Set();
    Running();
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
