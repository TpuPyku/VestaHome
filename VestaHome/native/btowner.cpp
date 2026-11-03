// btowner.cpp - Bluetooth for one device at a time (btowner.h)
#include "btowner.h"

static bool PhoneToLetGo(const BtOwnerIn& in)
{
    if (in.lastAct == BTA_NONE)
        return true;   // the first step: the stack's own connects off, whether the phone is there yet or not
    return in.phone && (in.lastAct != BTA_PHONE_OFF || in.sinceAct >= kBtoPhoneOffMs);
}

static bool ObdToConnect(const BtOwnerIn& in)
{
    if (in.phone || in.obdBusy || !in.obdAddr || in.hold || in.tries >= kBtoObdTries)
        return false;
    return in.sinceAct >= (DWORD)(in.lastAct == BTA_OBD_CONNECT ? kBtoObdRetryMs : kBtoObdWaitMs);
}

int BtOwnerStep(const BtOwnerIn& in)
{
    if (in.owner != BTO_OBD)
        return in.obdBusy ? BTA_OBD_STOP : BTA_NONE;
    if (PhoneToLetGo(in))
        return BTA_PHONE_OFF;
    return ObdToConnect(in) ? BTA_OBD_CONNECT : BTA_NONE;
}
