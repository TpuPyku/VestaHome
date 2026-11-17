// callwin.cpp - the call's windows: what a change of the phone's call state does (the user 2026-11-17: an incoming
// call over any program - with OSMapsCE in front there was nothing to answer with on the unit).
#pragma once

enum CallAct {
    CA_NONE,
    CA_INCOMING,   // VestaHome forward (the program in front remembered), the incoming call's window
    CA_TAKEN,      // the incoming one answered (here or on the phone): the call's window
    CA_CLOSE       // the call over: its window closed, the program that was in front back
};

// call: the line's state now (btshm.h BT_CALL_*: 0 none, 1 incoming, 2 dialing, ... 4 active); onIncoming / onCall:
// the incoming call's / the call's window shown.
CallAct CallStep(int call, bool onIncoming, bool onCall);

// While a call lasts the panel's and the wheel's buttons are VestaHome's (answer, hang up), not an add-on's.
bool CallKeysOurs(int call);
