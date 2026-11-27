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
    Check(CardOutRadio(7, false, 1, 1), "the card out, the media on, played from it (sheet 28): the radio");
    Check(CardOutRadio(6, false, 2, 2), "the USB disk out, played from it (sheet 29 p. 4): the radio");
    Check(CardOutRadio(7, false, 3, 2), "both out at once, played from the USB disk: the radio");
    Check(!CardOutRadio(7, false, 1, 0), "the card out, played from the built-in card (\\SDMMC): kept");
    Check(!CardOutRadio(7, false, 1, 2), "the card out, played from the USB disk: kept");
    Check(!CardOutRadio(7, true, 1, 1), "the card out under the internet radio (it needs no card): kept");
    Check(!CardOutRadio(0, false, 1, 1), "the card out on the radio: nothing to do");
    Check(!CardOutRadio(9, false, 1, 1), "the card out under Bluetooth: kept");
    Check(!CardOutRadio(-1, false, 1, 1), "the source not known (the PC): kept");
    Check(MusicBackOnTab(true, false), "our music paused by the radio, Music opened (sheet 28 p. 1-2): it plays");
    Check(!MusicBackOnTab(false, false), "paused by the user: stays paused");
    Check(!MusicBackOnTab(true, true), "already playing: left alone");
    Check(TabOpened(1, 7, 7), "another screen -> the tab: opened");
    Check(!TabOpened(7, 7, 7), "the tab again (its own button): not opened anew");
    Check(!TabOpened(7, 1, 7), "the tab left: not opened");
    Check(!TabOpened(1, 2, 7), "another screen: not the tab");
    Check(LearnSource(-1, 1500, 0, 11) == 11, "the source changed 1.5 s after the key: its number");
    Check(LearnSource(-1, 7000, 0, 11) == -1, "changed long after the key: not its");
    Check(LearnSource(5, 7000, 0, 11) == 5, "long after: the number known stays");
    Check(LearnSource(-1, 500, 11, 11) == -1, "no change (already on it): nothing learned");
    Check(LearnSource(-1, 500, 0, -1) == -1, "the source not known: nothing learned");
    Check(LearnSource(-1, 5999, 0, 11) == 11, "within 6 s: its");
    Check(LearnSource(-1, 6000, 0, 11) == -1, "6 s: too late");
    Check(LearnSource(-1, 500, 11, 0) == -1, "the radio (0) after the key: not AUX's");
    Check(LearnSource(-1, 500, 0, 6) == -1, "the media (6) after the key: not AUX's");
    Check(LearnSource(-1, 500, 0, 7) == -1, "the media (7) after the key: not AUX's");
    Check(LearnSource(-1, 500, 0, 9) == -1, "Bluetooth (9) after the key: not AUX's");
    Check(LearnSource(-1, 500, 0, 1) == 1, "1: an unknown number - learned");
    Check(SrcJustTurned(9, 7, false, 60000), "the MCU on Bluetooth, the tick not there yet, not ours: just turned (MODE)");
    Check(SrcJustTurned(9, 9, false, 2999), "turned to it by MODE 3 s ago: just turned");
    Check(!SrcJustTurned(9, 9, false, 3000), "3 s on it: the user's play takes the media again");
    Check(!SrcJustTurned(9, 7, true, 60000), "our own asking: not the user's turn");
    Check(!SrcJustTurned(0, 0, false, 60000), "long on the radio: not just turned");
    Check(AuxOutOnSource(true, 11, 0, 60000), "AUX in, the source turned (MODE): out at once");
    Check(AuxOutOnSource(true, 0, 9, 60000), "AUX in, any turn - its number not needed: out");
    Check(!AuxOutOnSource(true, 0, 11, 5999), "within 6 s of our AUX key: our own turn to AUX - stays in");
    Check(AuxOutOnSource(true, 11, 0, 6000), "6 s after the key: a turn is the user's - out");
    Check(!AuxOutOnSource(false, 11, 0, 60000), "AUX out already: nothing to tell");
    Check(!AuxOutOnSource(true, 11, 11, 60000), "no turn: stays");
    Check(!AuxOutOnSource(true, -2, 0, 60000), "the first tick (nothing seen before): not a turn");
    Check(!AuxOutOnSource(true, 0, -1, 60000), "the source not known: not a turn");
    Check(AuxOutOnSource(true, -1, 0, 60000), "from an unknown source to a known one: a turn");
    Check(AuxOutOnScreen(true, true, true), "AUX's page -> another audio page: out at once");
    Check(!AuxOutOnScreen(true, true, false), "AUX's page -> home / settings: AUX keeps playing");
    Check(!AuxOutOnScreen(true, false, true), "not from AUX's page: nothing");
    Check(!AuxOutOnScreen(false, true, true), "out already: nothing");
    Check(PhonePaused(0, 60000, 3000, true), "the phone plays by itself under the radio 3 s: paused (as the shell)");
    Check(PhonePaused(7, 60000, 3000, true), "under the media: paused");
    Check(!PhonePaused(9, 60000, 3000, true), "under Bluetooth: it is its music - plays");
    Check(!PhonePaused(0, 5000, 3000, true), "5 s after our own switching (to Bluetooth, the source still to come): plays");
    Check(PhonePaused(0, 5001, 3000, true), "just over 5 s after ours: paused");
    Check(!PhonePaused(0, 60000, 3000, false), "under the shell (it pauses itself): left alone");
    Check(!PhonePaused(0, 60000, 2999, true), "under 3 s (a notification's sound, HU sheet 30: dozens an hour): left alone");
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
