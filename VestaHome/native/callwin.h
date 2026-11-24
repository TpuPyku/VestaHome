// callwin.cpp - the call's windows: what a change of the phone's call state does (the user 2026-11-17: an incoming
// call over any program - with OSMapsCE in front there was nothing to answer with on the unit).
#pragma once

enum CallAct {
    CA_NONE,
    CA_INCOMING,   // VestaHome forward (the program in front remembered), the incoming call's window
    CA_TAKEN,      // the incoming one answered (here or on the phone): the call's window
    CA_CLOSE,      // the call over: its window closed, the program that was in front back
    CA_CALL        // a new call that did not ring here (dialed on the phone, answered there first): VestaHome
                   // forward, the call's window
};

// call: the line's state now (btshm.h BT_CALL_*: 0 none, 1 incoming, 2 dialing, ... 4 active); onIncoming / onCall:
// the incoming call's / the call's window shown; phoneShows: Android Auto in front - the phone shows the call on its
// own screen, so no window of ours over it (the user 2026-11-18: not twice); fresh: no call before it (a call
// whose window was closed by hand is left so).
CallAct CallStep(int call, bool onIncoming, bool onCall, bool phoneShows, bool fresh);

// The call to follow (the widget and the window alike): line 1's, else line 2's (the user 2026-11-24: a call on
// the phone's second line showed in the widget, no window).
int CallLine(int line1, int line2);

// Android's virtual call (HU 2026-11-24, Alice): an app taking the car's microphone makes the phone open the call's
// audio (SCO, narrow-band - all its sound goes there, the music too) under a made-up number, "10000000". Not a call:
// no window, VestaHome puts it down (the phone goes back to its music channel). A hidden number is "", a call.
bool CallVirtual(const char* number);

// While a call lasts the panel's and the wheel's buttons are VestaHome's (answer, hang up), not an add-on's.
bool CallKeysOurs(int call);
