// test_trackend.cpp - whether the next track starts when one ends (trackend.cpp) on the PC (test_trackend.bat).
#include <windows.h>
#include <stdio.h>
#include "trackend.h"

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
    Check(NextTrackPlays(6, false), "the media (06): on");
    Check(NextTrackPlays(7, false), "the media (07): on");
    Check(NextTrackPlays(9, false), "Bluetooth's path (ours sound there): on");
    Check(!NextTrackPlays(0, false), "the radio taken (MODE, HU 2026-11-07): not on - it would take the media back");
    Check(!NextTrackPlays(1, false), "another source: not on");
    Check(!NextTrackPlays(8, false), "the source after the media's: not on");
    Check(NextTrackPlays(0, true), "our own switch to the media still going: on");
    Check(NextTrackPlays(-1, false), "the source not known (the PC, no MCU): on, as before");
    Check(CardOutRadio(7, false), "the card out, the media on (sheet 28): the radio");
    Check(CardOutRadio(6, false), "the card out, the media (06): the radio");
    Check(!CardOutRadio(7, true), "the card out under the internet radio (it needs no card): kept");
    Check(!CardOutRadio(0, false), "the card out on the radio: nothing to do");
    Check(!CardOutRadio(9, false), "the card out under Bluetooth: kept");
    Check(!CardOutRadio(-1, false), "the source not known (the PC): kept");
    Check(MusicBackOnTab(true, false), "our music paused by the radio, Music opened (sheet 28 p. 1-2): it plays");
    Check(!MusicBackOnTab(false, false), "paused by the user: stays paused");
    Check(!MusicBackOnTab(true, true), "already playing: left alone");
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
