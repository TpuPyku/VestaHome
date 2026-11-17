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
    Check(CallStep(1, false, false) == CA_INCOMING, "a call coming: its window, whatever is in front");
    Check(CallStep(1, true, false) == CA_NONE, "still ringing, its window shown: nothing");
    Check(CallStep(4, true, false) == CA_TAKEN, "answered (here or on the phone): the call's window");
    Check(CallStep(2, true, false) == CA_TAKEN, "the incoming one turned into a dialing state: the call's window");
    Check(CallStep(4, false, true) == CA_NONE, "talking, the call's window shown: nothing");
    Check(CallStep(0, true, false) == CA_CLOSE, "rang off unanswered: closed, the program back");
    Check(CallStep(0, false, true) == CA_CLOSE, "the call ended: closed, the program back");
    Check(CallStep(0, false, false) == CA_NONE, "no call, no window: nothing");
    Check(CallStep(2, false, false) == CA_NONE, "dialed from the phone: not pulled forward");
    Check(CallStep(4, false, false) == CA_NONE, "talking with no window of ours (closed by hand): left so");
    Check(CallStep(5, false, true) == CA_NONE, "held: the call's window stays");
    Check(CallKeysOurs(1) && CallKeysOurs(4), "ringing, talking: the buttons ours");
    Check(!CallKeysOurs(0), "no call: the buttons may go to the add-on in front");
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
