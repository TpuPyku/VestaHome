// callwin.cpp - see callwin.h.
#include "callwin.h"
#include <string.h>

enum { kNone = 0, kIncoming = 1, kDialing = 2 };   // (btshm.h BT_CALL_*)

CallAct CallStep(int call, bool onIncoming, bool onCall, bool phoneShows)
{
    if (call == kNone)
        return onIncoming || onCall ? CA_CLOSE : CA_NONE;
    if (call == kIncoming)
        return onIncoming || phoneShows ? CA_NONE : CA_INCOMING;
    return onIncoming && call >= kDialing ? CA_TAKEN : CA_NONE;
}

bool CallKeysOurs(int call)
{
    return call != kNone;
}

bool CallVirtual(const char* number)
{
    return number && !strcmp(number, "10000000");
}
