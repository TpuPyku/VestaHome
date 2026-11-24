// callwin.cpp - see callwin.h.
#include "callwin.h"

enum { kNone = 0, kIncoming = 1 };   // (btshm.h BT_CALL_*)

CallAct CallStep(int call, bool onIncoming, bool onCall, bool phoneShows, bool fresh)
{
    if (call == kNone)
        return onIncoming || onCall ? CA_CLOSE : CA_NONE;
    if (call == kIncoming)
        return onIncoming || phoneShows ? CA_NONE : CA_INCOMING;
    if (onIncoming)
        return CA_TAKEN;   // (dialing and on: answered)
    return fresh && !onCall && !phoneShows ? CA_CALL : CA_NONE;
}

bool CallKeysOurs(int call)
{
    return call != kNone;
}

int CallLine(int line1, int line2)
{
    return line1 != kNone ? line1 : line2;
}
