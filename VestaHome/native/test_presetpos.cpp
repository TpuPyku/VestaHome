// test_presetpos.cpp - the preset playing and the next one (presetpos.cpp) on the PC (test_presetpos.bat).
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "presetpos.h"

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
    Check(PresetPlaying(1, 9) == 9, "FM2's second asked, the MCU says place 1: 9");
    Check(PresetPlaying(3, 9) == 11, "another place (the wheel, the MCU itself): the same page's");
    Check(PresetPlaying(7, 7) == 7, "FM1's last: 7");
    Check(PresetPlaying(4, 20) == 20, "FM3: 20");
    Check(PresetPlaying(2, -1) == 2, "nothing asked yet: the first page");
    Check(PresetPlaying(-1, 9) == -1, "none playing (seek): none");
    Check(PresetPlaying(0, 8) == 8, "FM2's first: 8");
    Check(PresetPlaying(1, 17) == 17, "FM3's second: 17");
    Check(PresetNext(7, 1, 8) == 0, "FM1's last, next: its first (round)");
    Check(PresetNext(9, 1, 8) == 10, "FM2: 9 -> 10");
    Check(PresetNext(8, -1, 8) == 15, "FM2's first, back: its last");
    Check(PresetNext(20, -1, 8) == 19, "FM3: 20 -> 19");
    Check(PresetNext(-1, 1, 8) == 0, "none playing: the first");
    Check(PresetNext(2, 1, 3) == 0, "a page of 3 presets: round at 3");
    Check(PresetNext(0, 1, 8) == 1, "the first, next: the second");
    Check(PresetNext(17, 1, 8) == 18, "FM3: 17 -> 18");
    Check(PresetNext(16, 1, 8) == 17, "FM3's first: 16 -> 17");
    Check(PresetNext(9, 1, 3) == 10, "a second page of 3: 9 -> 10");
    {   // what the MCU changed in the presets (the log: who rewrote them)
        int was[8] = { 10120, 0, 9840, 10560, 0, 0, 0, 0 };
        int now[8] = { 10120, 8750, 10470, 10560, 0, 0, 0, 0 };
        wchar_t s[200];
        Check(PresetChanges(was, now, 0, 8, s, 200) == 1 && !wcscmp(s, L"3: 98.40 -> 104.70"),
              "only a known one that changed (an empty one filled is not a change)");
        Check(PresetChanges(was, was, 0, 8, s, 200) == 0 && !s[0], "nothing changed");
        int was2[8] = { 10120, 9840, 0, 0, 0, 0, 0, 0 };
        int now2[8] = { 9000, 9100, 0, 0, 0, 0, 0, 0 };
        Check(PresetChanges(was2, now2, 8, 8, s, 200) == 2 && !wcscmp(s, L"9: 101.20 -> 90.00, 10: 98.40 -> 91.00"),
              "numbered from the message's first (FM2), all of them");
        Check(PresetChanges(was2, now2, 0, 8, s, 12) == 2 && wcslen(s) == 11, "cut to the room, still counted");
        int now3[8] = { 0, 9840, 0, 0, 0, 0, 0, 0 };
        Check(PresetChanges(was2, now3, 0, 8, s, 200) == 1 && !wcscmp(s, L"1: 101.20 -> 0.00"), "a known one cleared");
    }
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
