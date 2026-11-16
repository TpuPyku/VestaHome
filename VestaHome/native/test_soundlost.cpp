// test_soundlost.cpp - soundlost.h on the PC: when an add-on's sound (Android Auto) is lost to another MCU source.
#include <stdio.h>
#include "soundlost.h"

static int s_failed, s_checks;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAILED: %s\n", what);
    }
}

int main()
{
    Check(SoundLost(7, 0), "media -> the radio: lost");
    Check(SoundLost(6, 9), "media (6) -> Bluetooth: lost");
    Check(SoundLost(7, 1), "media -> another source: lost");
    Check(!SoundLost(7, 7) && !SoundLost(6, 6), "media stays: not lost");
    Check(!SoundLost(7, 6) && !SoundLost(6, 7), "media to media (6 / 7): not lost");
    Check(!SoundLost(0, 9), "not media before (the add-on's sound not there yet): not lost");
    Check(!SoundLost(-1, 0) && !SoundLost(7, -1), "unknown before or now: not lost");
    printf("test_soundlost: %d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
