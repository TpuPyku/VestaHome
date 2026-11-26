// test_wifiboot.cpp - the Wi-Fi's start as the stock shell's and the watch over the Wi-Fi thread (wifiboot.h), on the PC.
#include <windows.h>
#include <stdio.h>
#include "wifiboot.h"

static int s_failed, s_checks;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAIL: %s\n", what);
    }
}

int main()
{
    // the known network's auto join (HU 2026-11-26: WZC saw the hotspot first, the SDK's list was still empty -
    // "нет в списке", then a minute's wait; the boot never joined)
    Check(AutoJoinDue(0, 5000, false), "never tried: due");
    Check(!AutoJoinDue(1000, 1000 + kAutoUnlistedMs, true), "not in the SDK's list: not before 10 s");
    Check(AutoJoinDue(1000, 1001 + kAutoUnlistedMs, true), "not in the SDK's list: again after 10 s");
    Check(!AutoJoinDue(1000, 1000 + kAutoGapMs, false), "tried (refused or joining): not before a minute");
    Check(AutoJoinDue(1000, 1001 + kAutoGapMs, false), "tried: again after a minute");
    Check(kAutoUnlistedMs == 10000 && kAutoGapMs == 60000, "10 s / 60 s");
    Check(AutoJoinDue(0xFFFFF000u, 0x00002000u, false) == false, "the tick's wrap: 8 KB ms later, not yet");
    // the SDK's start (the tester 2026-11-19: InitApi once, its failure - no SDK until the next boot)
    Check(InitRetryMs(1, 0) == 0, "InitApi answered: no retry");
    Check(InitRetryMs(1, 1) == 0, "InitApi answered S_FALSE: no retry");
    Check(InitRetryMs(1, (long)0x80004005) == kInitGapMs, "the first try failed: again in 5 s");
    Check(InitRetryMs(kInitTries - 1, (long)0x80004005) == kInitGapMs, "the 4th failed: the 5th in 5 s");
    Check(InitRetryMs(kInitTries, (long)0x80004005) == 0, "the 5th failed: no more");
    Check(kInitTries == 5 && kInitGapMs == 5000, "5 tries, 5 s apart");
    // the client's mode at the start (G3NavHMI: SetMode(2) always; 3 - Wi-Fi Direct - had the scanner off)
    Check(BootSetsClient(0, true), "off at the start: the client on");
    Check(BootSetsClient(1, true), "an access point at the start: the client on");
    Check(BootSetsClient(3, true), "Wi-Fi Direct at the start: the client on");
    Check(BootSetsClient(-1, true), "the mode not read: the client on");
    Check(!BootSetsClient(2, true), "a client already: as it is");
    Check(!BootSetsClient(0, false), "the switch off: nothing");
    Check(!BootSetsClient(3, false), "the switch off, Wi-Fi Direct: nothing");
    Check(kBootSettleMs == 3000, "3 s after SetMode, as the shell");
    // the watch over the thread: a step longer than 15 s - once
    Check(!StallDue(true, 1000, 1000 + kStallMs - 1, 7, 0), "14.999 s in a step: not yet");
    Check(StallDue(true, 1000, 1000 + kStallMs, 7, 0), "15 s in a step: logged");
    Check(!StallDue(true, 1000, 1000 + 60000, 7, 7), "the same step already logged: not again");
    Check(StallDue(true, 70000, 70000 + kStallMs, 8, 7), "the next step stuck: logged again");
    Check(!StallDue(false, 1000, 1000 + 60000, 7, 0), "waiting, not in a step: nothing");
    unsigned late = 0xFFFFF000u;
    Check(StallDue(true, late, late + kStallMs, 2, 1), "the tick count wrapped meanwhile");
    Check(kStallMs == 15000, "15 s");
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
