// test_zonepart.cpp - a partial frame's buttons against the full frame's (zonepart.cpp) on the PC (test_zonepart.bat).
#include <windows.h>
#include <stdio.h>
#include "zonepart.h"

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
    RECT rc[2] = { { 24, 124, 50, 150 }, { 82, 124, 108, 150 } };   // the radio widget's arrows (WRadio)
    Check(ZoneSame(&rc[1], 82, 124, 26, 26), "the same place: kept (its preset taken from the frame now)");
    Check(ZoneSame(&rc[0], 24, 124, 26, 26), "the first one, the same place: kept");
    Check(!ZoneSame(&rc[1], 83, 124, 26, 26), "moved right: the layout changed - the whole frame");
    Check(!ZoneSame(&rc[1], 82, 125, 26, 26), "moved down: the whole frame");
    Check(!ZoneSame(&rc[1], 82, 124, 27, 26), "wider: the whole frame");
    Check(!ZoneSame(&rc[1], 82, 124, 26, 27), "taller: the whole frame");
    Check(!ZoneSame(NULL, 24, 124, 26, 26), "more buttons than the full frame had (none there): the whole frame");
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
