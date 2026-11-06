// callroute.h against what G3NavHMI does (FUN_006666dc, FUN_00666a0c).
#include "callroute.h"
#include <stdio.h>

static int s_checks, s_failed;

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
    Check(CallDial(0, false) == CALL_IDLE && CallDial(0, true) == CALL_IDLE, "dial: no call");
    Check(CallDial(1, false) == CALL_RING, "dial: incoming, ringing");
    Check(CallDial(2, true) == CALL_DIAL && CallDial(3, true) == CALL_DIAL, "dial: dialing, alerting");
    Check(CallDial(4, true) == CALL_OUT, "dial: talking, ours");
    Check(CallDial(4, false) == CALL_IN, "dial: talking, the incoming one");
    Check(CallDial(9, false) == CALL_IDLE, "dial: an unknown state");

    Check(CallRouteTarget(CALL_IDLE, false) == 0 && CallRouteTarget(CALL_IDLE, true) == 0, "route: no call - none");
    Check(CallRouteTarget(CALL_RING, false) == 0 && CallRouteTarget(CALL_RING, true) == 0, "route: ringing - none here");
    Check(CallRouteTarget(CALL_DIAL, false) == 2, "route: dialing, no sound yet - 2");
    Check(CallRouteTarget(CALL_DIAL, true) == 0, "route: dialing, the sound in the car - none");
    Check(CallRouteTarget(CALL_OUT, false) == 1 && CallRouteTarget(CALL_OUT, true) == 1, "route: talking, ours - 1");
    Check(CallRouteTarget(CALL_IN, false) == 1 && CallRouteTarget(CALL_IN, true) == 1, "route: talking, incoming - 1");

    Check(CallRouteIncoming(false) == 2, "incoming window: no sound yet - 2");
    Check(CallRouteIncoming(true) == 0, "incoming window: the sound in the car - none");
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
