// test_saverink.cpp - the screen saver's colours at night (saverink.h), on the PC (test_saverink.bat).
#include <windows.h>
#include <stdio.h>
#include "saverink.h"

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
    COLORREF white = RGB(0xf5, 0xf3, 0xee);
    Check(SaverInk(white, false) == white, "day: as it is");
    Check(SaverInk(white, true) == RGB(0x62, 0x61, 0x5f), "night: 40 % of each channel");
    Check(SaverInk(RGB(255, 0, 100), true) == RGB(102, 0, 40), "each channel apart");
    Check(SaverInk(RGB(0, 0, 0), true) == RGB(0, 0, 0), "black stays black");
    printf("saverink: %d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
