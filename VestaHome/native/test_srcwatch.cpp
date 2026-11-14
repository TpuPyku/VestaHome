// srcwatch.h: a change of the MCU's source - ours (we asked for that source a moment ago) or the user's (MODE).
#include "srcwatch.h"
#include <stdio.h>

static int s_checks, s_failed;

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
    const unsigned radio = SrcBit(0), media = SrcBit(6) | SrcBit(7);
    Check(SrcOurs(1000, 1000 - 3000, radio, 0), "the radio asked 3 s ago, the radio came - ours");
    Check(!SrcOurs(1000 + 7000, 1000, radio, 0), "after the window - not ours");
    Check(!SrcOurs(5000, 1000, radio, 7), "the radio asked 4 s ago, the media came - MODE (HU 2026-11-14-3)");
    Check(SrcOurs(3000, 1000, media, 7) && SrcOurs(3000, 1000, media, 6), "the media asked - 6 or 7 ours");
    Check(!SrcOurs(3000, 1000, media, 9), "the media asked, Bluetooth came - not ours");
    Check(SrcOurs(3000, 1000, kSrcAny, 3), "any asked (AUX: its number unknown) - ours");
    Check(!SrcOurs(3000, 0, radio, 0), "nothing asked yet - not ours");
    Check(SrcBit(-1) == 0 && SrcBit(40) == 0, "a source out of range - no bit");
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
