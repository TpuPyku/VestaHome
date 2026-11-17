// radiowait.h - the Wi-Fi switch off and on again: the radio stays off kRadioOffMs in all before it is switched on
// (wifi.cpp). What the "Перезапустить" button did (sheet 24: the stuck driver cured only by a full power cut);
// the user 2026-11-16: the switch does it, no button of its own.
#ifndef RADIOWAIT_H
#define RADIOWAIT_H

enum { kRadioOffMs = 30000 };

// offTick - when the radio went off, now - the tick count: ms still to wait, 0 - on at once
static unsigned RadioOffLeft(unsigned offTick, unsigned now)
{
    unsigned off = now - offTick;   // (the tick count may wrap meanwhile)
    return off >= (unsigned)kRadioOffMs ? 0 : kRadioOffMs - off;
}

// What the radio's thread does with the switch (HU 2026-11-17, sheet 40 p. 8: off and on at once did nothing - the
// thread saw only where the switch is now, on again, and the switch-off was lost). offs - the switch-offs so far
// (WifiEnable counts them), seen - the ones the thread has acted on: one not acted on puts the radio off even when
// the switch is on again (then on after the 30 s, RadioOffLeft).
enum { RADIO_KEEP, RADIO_GO_OFF, RADIO_GO_ON };

static int RadioStep(bool wantOn, bool radioOn, unsigned offs, unsigned seen)
{
    if (radioOn)
        return !wantOn || offs != seen ? RADIO_GO_OFF : RADIO_KEEP;
    return wantOn ? RADIO_GO_ON : RADIO_KEEP;
}

#endif
