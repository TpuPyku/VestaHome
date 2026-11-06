// The call's sound: which route the shell asks for when the MCU sends 0x8905 (Ghidra 2026-11-06: CBtPhone::MsgDecode
// FUN_00666ac8 -> FUN_006666dc / FUN_00666a0c). 0 - it asks for none.
#ifndef CALLROUTE_H
#define CALLROUTE_H

enum { CALL_IDLE, CALL_DIAL, CALL_OUT, CALL_IN, CALL_RING };

// The SDK's event 0x10 ev[1] (0 idle, 1 incoming, 2 dialing, 3 alerting, 4 active) and whose call it is
static int CallDial(int sdkState, bool ours)
{
    switch (sdkState) {
    case 1: return CALL_RING;
    case 2:
    case 3: return CALL_DIAL;
    case 4: return ours ? CALL_OUT : CALL_IN;
    }
    return CALL_IDLE;
}

// 0x8905 00 00: by the call's state; hfAudio - the call's sound is in the car already
static int CallRouteTarget(int dial, bool hfAudio)
{
    if (dial == CALL_OUT || dial == CALL_IN)
        return 1;
    return dial == CALL_DIAL && !hfAudio ? 2 : 0;
}

// 0x8905 01 00: the incoming call's window
static int CallRouteIncoming(bool hfAudio)
{
    return hfAudio ? 0 : 2;
}

#endif
