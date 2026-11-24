// test_camlimit.cpp - a camera's limit as read from speedcam.txt (camlimit.h), on the PC.
#include <windows.h>
#include <stdio.h>
#include "camlimit.h"

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
    // HU 2026-11-24: openspeedcam.net's red-light cameras "5 km/h" beeped all the way; its "no limit" is 160
    Check(CamLimit(5) == 0 && CamLimit(9) == 0, "under 10: a filler, unknown");
    Check(CamLimit(160) == 0 && CamLimit(250) == 0, "160 and above: a filler, unknown");
    Check(CamLimit(10) == 10 && CamLimit(60) == 60 && CamLimit(159) == 159, "real limits kept");
    Check(CamLimit(0) == 0 && CamLimit(-3) == 0, "none stays none");
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
