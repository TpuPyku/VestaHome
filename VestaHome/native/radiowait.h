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

#endif
