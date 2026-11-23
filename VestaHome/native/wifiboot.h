// wifiboot.h - the Wi-Fi's start as the stock shell does it (wifi.cpp) and the watch over the Wi-Fi thread.
// The tester 2026-11-19 (PROJECT.md "Справочник Wi-Fi"): the SDK's InitApi tried once, the client switched on only from
// mode 0, the Wi-Fi thread stuck in WZC with the switch waiting on it. G3NavHMI (Ghidra): InitApi, SetMode(2) whatever
// the mode was, 3 s, then the scanner and the auto-connect back on.
#ifndef WIFIBOOT_H
#define WIFIBOOT_H

enum { kInitTries = 5, kInitGapMs = 5000,   // the SDK's InitApi at the start
       kBootSettleMs = 3000,                // after SetMode(2) the adapter comes up (0x80070037 before it)
       kStallMs = 15000,                    // a step of the Wi-Fi thread longer than that: into the log
       kBootClient = 2 };                   // WDK_DMW_SetMode's client

// InitApi failed `tries` times so far, hr - its last answer: ms to wait before the next try, 0 - no more tries
static unsigned InitRetryMs(int tries, long hr)
{
    return hr >= 0 || tries >= kInitTries ? 0 : kInitGapMs;
}

// The mode the SDK found at the start (-1 not read): the client switched on (SetMode(2)) or left. Mode 1 is an access
// point, 3 Wi-Fi Direct (it turns the scanner and the auto-connect off): the shell sets the client over both.
static bool BootSetsClient(int bootMode, bool wantOn)
{
    return wantOn && bootMode != kBootClient;
}

// The watch (the window's timer): the thread in a step (busy) since `since`, the step's number `step`, `logged` - the
// step already logged. true: log the stall now (once per step).
static bool StallDue(bool busy, unsigned since, unsigned now, unsigned step, unsigned logged)
{
    return busy && step != logged && now - since >= (unsigned)kStallMs;
}

#endif
