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

static void TestUnstack()   // (the user 2026-11-27: hung up on the other end - the call's window stayed till red)
{
    int h[6] = { 0, 7, 30, 5, 31, 9 };   // 30, 31: the incoming one's and the call's windows under others
    Check(CallUnstack(h, 6, 30, 31) == 4 && h[0] == 0 && h[1] == 7 && h[2] == 5 && h[3] == 9,
          "unstack: the call's windows out of the history, the rest in its order");
    int g[3] = { 1, 2, 3 };
    Check(CallUnstack(g, 3, 30, 31) == 3 && g[0] == 1 && g[1] == 2 && g[2] == 3, "unstack: none there - as it was");
    int f[2] = { 31, 30 };
    Check(CallUnstack(f, 2, 30, 31) == 0, "unstack: only theirs - empty");
}

int main()
{
    TestUnstack();
    // Android's virtual call (an app taking the car's microphone - Alice; WhatsApp's, Telegram's calls too): put
    // down only when "Settings -> Bluetooth" asks so (the user 2026-11-24)
    Check(CallVirtual("10000000"), "virtual: Android's made-up number");
    Check(!CallVirtual("") && !CallVirtual("100000000") && !CallVirtual("1000000") && !CallVirtual("+79161234567") &&
          !CallVirtual("10000001"), "virtual: a hidden number, a real one, anything else - a call");
    Check(!CallVirtual(0), "virtual: no number - a call");
    Check(CallStep(1, false, false, false, false) == CA_INCOMING, "a call coming: its window, whatever is in front");
    Check(CallStep(1, true, false, false, false) == CA_NONE, "still ringing, its window shown: nothing");
    Check(CallStep(4, true, false, false, false) == CA_TAKEN, "answered (here or on the phone): the call's window");
    Check(CallStep(2, true, false, false, false) == CA_TAKEN,
          "the incoming one turned into a dialing state: the call's window");
    Check(CallStep(4, false, true, false, false) == CA_NONE, "talking, the call's window shown: nothing");
    Check(CallStep(0, true, false, false, false) == CA_CLOSE, "rang off unanswered: closed, the program back");
    Check(CallStep(0, false, true, false, false) == CA_CLOSE, "the call ended: closed, the program back");
    Check(CallStep(0, false, false, false, false) == CA_NONE, "no call, no window: nothing");
    // a call that did not start ringing here (dialed on the phone, answered there before the ring reached us, on the
    // phone's second line): its window too (the user 2026-11-24: "not every call shows its window, the widget does")
    Check(CallStep(2, false, false, false, true) == CA_CALL, "dialed on the phone: the call's window, VestaHome forward");
    Check(CallStep(4, false, false, false, true) == CA_CALL, "already talking when first seen: the call's window");
    Check(CallStep(3, false, false, false, true) == CA_CALL, "ringing there: the call's window");
    Check(CallStep(2, false, false, true, true) == CA_NONE, "dialed, Android Auto in front: the phone shows it");
    Check(CallStep(4, false, true, false, true) == CA_NONE, "a new call, the call's window up already: nothing");
    Check(CallStep(1, false, false, false, true) == CA_INCOMING, "a new incoming one: as before");
    Check(CallStep(2, false, false, false, false) == CA_NONE, "a call going on, no window of ours: left so");
    // the line: the first one's call, else the second's (the widget and the window - the same)
    Check(CallLine(4, 0) == 4 && CallLine(0, 1) == 1 && CallLine(5, 6) == 5 && CallLine(0, 0) == 0,
          "line: the first, else the second");
    Check(CallStep(4, false, false, false, false) == CA_NONE, "talking with no window of ours (closed by hand): left so");
    Check(CallStep(5, false, true, false, false) == CA_NONE, "held: the call's window stays");
    // Android Auto in front (the user 2026-11-18: no double window) - the phone shows the call on its own screen
    Check(CallStep(1, false, false, true, false) == CA_NONE, "a call coming, Android Auto in front: no window of ours");
    Check(CallStep(4, false, false, true, false) == CA_NONE, "answered on Android Auto's screen: still none");
    Check(CallStep(0, false, false, true, false) == CA_NONE, "over: nothing to close");
    Check(CallStep(4, true, false, true, false) == CA_TAKEN, "our window already up (AA came forward later): it goes on");
    Check(CallStep(0, false, true, true, false) == CA_CLOSE, "our call's window up and the call over: closed");
    Check(CallKeysOurs(1) && CallKeysOurs(4), "ringing, talking: the buttons ours");
    Check(!CallKeysOurs(0), "no call: the buttons may go to the add-on in front");
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
