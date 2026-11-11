// test_presetpos.cpp - the preset playing and the next one (presetpos.cpp) on the PC (test_presetpos.bat).
#include <windows.h>
#include <stdio.h>
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
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
