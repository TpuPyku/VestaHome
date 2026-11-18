// test_callwin.cpp - the call's windows (callwin.cpp) on the PC (test_callwin.bat).
#include <stdio.h>
#include "callwin.h"

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
    Check(CallStep(1, false, false, false) == CA_INCOMING, "a call coming: its window, whatever is in front");
    Check(CallStep(1, true, false, false) == CA_NONE, "still ringing, its window shown: nothing");
    Check(CallStep(4, true, false, false) == CA_TAKEN, "answered (here or on the phone): the call's window");
    Check(CallStep(2, true, false, false) == CA_TAKEN, "the incoming one turned into a dialing state: the call's window");
    Check(CallStep(4, false, true, false) == CA_NONE, "talking, the call's window shown: nothing");
    Check(CallStep(0, true, false, false) == CA_CLOSE, "rang off unanswered: closed, the program back");
    Check(CallStep(0, false, true, false) == CA_CLOSE, "the call ended: closed, the program back");
    Check(CallStep(0, false, false, false) == CA_NONE, "no call, no window: nothing");
    Check(CallStep(2, false, false, false) == CA_NONE, "dialed from the phone: not pulled forward");
    Check(CallStep(4, false, false, false) == CA_NONE, "talking with no window of ours (closed by hand): left so");
    Check(CallStep(5, false, true, false) == CA_NONE, "held: the call's window stays");
    // Android Auto in front (the user 2026-11-18: no double window) - the phone shows the call on its own screen
    Check(CallStep(1, false, false, true) == CA_NONE, "a call coming, Android Auto in front: no window of ours");
    Check(CallStep(4, false, false, true) == CA_NONE, "answered on Android Auto's screen: still none");
    Check(CallStep(0, false, false, true) == CA_NONE, "over: nothing to close");
    Check(CallStep(4, true, false, true) == CA_TAKEN, "our window already up (AA came forward later): it goes on");
    Check(CallStep(0, false, true, true) == CA_CLOSE, "our call's window up and the call over: closed");
    Check(CallKeysOurs(1) && CallKeysOurs(4), "ringing, talking: the buttons ours");
    Check(!CallKeysOurs(0), "no call: the buttons may go to the add-on in front");
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
