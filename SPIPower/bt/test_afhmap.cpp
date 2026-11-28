// test_afhmap.cpp - afhmap.h on the PC: the Bluetooth channels kept off a Wi-Fi channel. Exit code: the failed checks.
#include <stdio.h>
#include <string.h>
#include "afhmap.h"

static int s_failed, s_checks;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAILED: %s\n", what);
    }
}

static bool Bit(const unsigned char* m, int k)
{
    return (m[k / 8] >> (k % 8) & 1) != 0;
}

int main()
{
    unsigned char m[10];
    Check(AfhMap(0, m) == 0, "no Wi-Fi channel: none bad");
    Check(m[0] == 0xFF && m[8] == 0xFF && m[9] == 0x7F, "all 79 usable, bit 79 (reserved) 0");

    Check(AfhMap(1, m) == 22, "channel 1 (2412 MHz): BT 0..21 (2402..2423) bad");
    Check(!Bit(m, 0) && !Bit(m, 21) && Bit(m, 22) && Bit(m, 78) && !Bit(m, 79), "channel 1: its edges");

    Check(AfhMap(6, m) == 23, "channel 6 (2437 MHz): BT 24..46 bad");
    Check(Bit(m, 23) && !Bit(m, 24) && !Bit(m, 46) && Bit(m, 47), "channel 6: its edges");

    Check(AfhMap(13, m) == 20, "channel 13 (2472 MHz): BT 59..78 bad");
    Check(Bit(m, 58) && !Bit(m, 59) && !Bit(m, 78) && !Bit(m, 79), "channel 13: its edges, 79 still 0");

    Check(AfhMap(14, m) == 8, "channel 14 (2484 MHz): BT 71..78 and none past 78");
    Check(Bit(m, 70) && !Bit(m, 71) && !Bit(m, 78), "channel 14: its edges");

    Check(AfhMap(15, m) == 0 && AfhMap(-1, m) == 0, "no such channel: none bad");

    Check(AfhChannel("6") == 6 && AfhChannel("0") == 0 && AfhChannel("14") == 14 && AfhChannel("9") == 9,
          "the command's channel");
    Check(AfhChannel("15") == -1 && AfhChannel("") == -1 && AfhChannel("x") == -1 && AfhChannel("-1") == -1 &&
          AfhChannel("0014") == -1, "a bad command: -1");
    Check(AfhWifiMhz(1) == 2412 && AfhWifiMhz(13) == 2472 && AfhWifiMhz(14) == 2484 && AfhWifiMhz(0) == 0 &&
          AfhWifiMhz(15) == 0, "Wi-Fi channels' middles");

    unsigned char guard[11];   // nothing written past the map
    guard[10] = 0xA5;
    AfhMap(6, guard);
    Check(guard[10] == 0xA5, "the map: 10 bytes only");

    char hex[22];
    hex[21] = '#';
    AfhMap(1, m);
    AfhHex(m, hex);
    Check(!strcmp(hex, "0000C0FFFFFFFFFFFF7F") && hex[21] == '#', "the map in the log: its bytes in order, 21 chars");
    printf("afhmap: %d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
