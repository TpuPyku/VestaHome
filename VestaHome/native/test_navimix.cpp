// test_navimix.cpp - Navitel's voice over the radio (navimix.h), on the PC.
#include <stdio.h>
#include "navimix.h"

static int s_failed, s_checks;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAIL: %s\n", what);
    }
}

int main()
{
    wchar_t procs[4][40] = { L"nk.exe", L"VestaHome.exe", L"NAVITEL.EXE", L"explorer.exe" };
    Check(NaviMixRunning(procs, 4) == 2, "navitel.exe found in any case");
    Check(NaviMixRunning(procs, 2) == -1, "no Navitel: -1");
    wcscpy(procs[0], L"navitel7.exe");
    Check(NaviMixRunning(procs, 4) == 0, "navitel7.exe (navione starts either) found first");
    wcscpy(procs[0], L"navitel.exe.bak");
    Check(NaviMixRunning(procs, 1) == -1, "a longer name is not Navitel");
    wcscpy(procs[0], L"navitel");
    Check(NaviMixRunning(procs, 1) == -1, "a shorter name is not Navitel");
    Check(NaviMixHold(true, false, true), "on, no shell window, Navitel runs: held");
    Check(!NaviMixHold(false, false, true), "the switch off: not held");
    Check(!NaviMixHold(true, true, true), "the shell's window: the shell's business");
    Check(!NaviMixHold(true, false, false), "Navitel not running: not held");
    printf("navimix: %d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
