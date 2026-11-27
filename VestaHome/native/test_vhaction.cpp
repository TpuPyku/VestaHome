// test_vhaction.cpp - VestaHome's own actions by name (vhaction.h), on the PC.
#include <windows.h>
#include <stdio.h>
#include "vhaction.h"

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
    // the user 2026-11-27: a user widget's tap= as the diagnostics' phone assistant button
    Check(VhActionOf(L"vh:assistant") == VHA_ASSISTANT, "vh:assistant: the phone's assistant");
    Check(VhActionOf(L"VH:Assistant") == VHA_ASSISTANT, "any case");
    Check(VhActionOf(L"fuel.mscr") == VHA_NONE && VhActionOf(L"") == VHA_NONE, "a script, nothing: not ours");
    Check(VhActionOf(L"\\SDMemory\\vh:x.exe") == VHA_NONE, "vh: inside a path: not ours");
    Check(VhActionOf(L"vh:") == VHA_UNKNOWN && VhActionOf(L"vh:nosuch") == VHA_UNKNOWN, "ours, unknown: told");
    Check(VhActionOf(L"vh:assistantx") == VHA_UNKNOWN, "the whole name only");
    // an add-on's "do\tname=assistant" (VestaSmartHome): the name alone
    Check(VhActionName(L"assistant") == VHA_ASSISTANT && VhActionName(L"x") == VHA_UNKNOWN, "by the name alone");
    // the user 2026-11-27: probki.ini's tap - its traffic again now, not OSMapsCE
    Check(VhActionOf(L"vh:refresh") == VHA_REFRESH, "vh:refresh: the widget's data now");
    Check(VhRefreshDue(0, 5000), "never fetched: now");
    Check(!VhRefreshDue(100000, 114999), "a tap again within 15 s: nothing");
    Check(VhRefreshDue(100000, 115000), "15 s on: again");
    Check(VhRefreshDue(0xFFFFF000, 0xFFFFF000 + 15000), "the tick wrapped: still counted");
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
