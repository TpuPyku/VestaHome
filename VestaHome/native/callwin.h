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

// The call over: its windows (incoming, call) out of the screens' history, wherever they are in it (the user
// 2026-11-27: hung up on the other end, the incoming window stayed under the call's - answered by the wheel - and
// came back). The rest keep their order; the new length.
int CallUnstack(int* hist, int n, int incoming, int call);

// The call to follow (the widget and the window alike): line 1's, else line 2's (the user 2026-11-24: a call on
// the phone's second line showed in the widget, no window).
int CallLine(int line1, int line2);

// Android's virtual call (HU 2026-11-24): an app taking the car's microphone (Alice) makes the phone open the call's
// audio (SCO, narrow-band - all its sound, the music too) under a made-up number, "10000000"; WhatsApp's and
// Telegram's calls come under it as well. VestaHome puts it down only when the Bluetooth setting asks it to.
bool CallVirtual(const char* number);

// While a call lasts the panel's and the wheel's buttons are VestaHome's (answer, hang up), not an add-on's.
bool CallKeysOurs(int call);
