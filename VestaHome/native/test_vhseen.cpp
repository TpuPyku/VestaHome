// test_vhseen.cpp - whether VestaHome is what the user sees (vhseen.h), on the PC.
#include <stdio.h>
#include "vhseen.h"

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
    const long kScreen = 800L * 480;
    Check(VhSeenBy(true, true, true, kScreen, kScreen), "ours in front: seen");
    Check(!VhSeenBy(false, true, true, kScreen, kScreen), "hidden: not seen");
    Check(!VhSeenBy(true, false, true, kScreen, kScreen), "a full-screen program in front (a navigator): not seen");
    Check(VhSeenBy(true, false, true, 800L * 60, kScreen),
          "a bar in front (the CAN panel's, HU 2026-11-26 - the speed at 0 for 7 min of driving): seen");
    Check(VhSeenBy(true, false, false, kScreen, kScreen), "an invisible window in front: seen");
    Check(!VhSeenBy(true, false, true, kScreen / 2, kScreen), "half the screen: not seen");
    Check(VhSeenBy(true, false, true, kScreen / 2 - 1, kScreen), "less than half: seen");
    printf("vhseen: %d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
