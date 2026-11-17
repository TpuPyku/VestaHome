// test_radiowait.cpp - how long the Wi-Fi radio still stays off before it comes on again (radiowait.h), on the PC.
#include <windows.h>
#include <stdio.h>
#include "radiowait.h"

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
    Check(RadioOffLeft(1000, 1000) == 30000, "just switched off - the whole 30 s left");
    Check(RadioOffLeft(1000, 1000 + 29999) == 1, "1 ms before the 30 s - 1 ms left");
    Check(RadioOffLeft(1000, 1000 + 30000) == 0, "off 30 s - on at once");
    Check(RadioOffLeft(1000, 1000 + 600000) == 0, "off long ago - on at once");
    Check(RadioOffLeft(0xFFFFFF00u, 0x100) == 30000 - 0x200, "the tick count wrapped meanwhile");
    // the switch (HU 2026-11-17, sheet 40 p. 8: off and on at once - nothing happened): what the radio's thread does
    Check(RadioStep(true, true, 0, 0) == RADIO_KEEP, "on and on, no switch-off: as it is");
    Check(RadioStep(false, true, 1, 0) == RADIO_GO_OFF, "switched off: off");
    Check(RadioStep(true, true, 1, 0) == RADIO_GO_OFF, "off and on again before the thread looked: still off first");
    Check(RadioStep(true, true, 3, 3) == RADIO_KEEP, "the switch-offs already done: as it is");
    Check(RadioStep(true, false, 1, 1) == RADIO_GO_ON, "off, switched on: on");
    Check(RadioStep(true, false, 2, 1) == RADIO_GO_ON, "off, switched off again meanwhile, now on: on");
    Check(RadioStep(false, false, 2, 1) == RADIO_KEEP, "off and switched off: as it is");
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
