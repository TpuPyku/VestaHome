// privacy.h: what VHCheck's log leaves out (the unit's serial, its BT address, the position in a GPS line).
#include "privacy.h"
#include <stdio.h>
#include <string.h>

static int s_checks, s_failed;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAIL: %s\n", what);
    }
}

static void Hidden(int code, const unsigned char* d, int len, char* out)   // '1' hidden, '0' shown, per byte
{
    for (int k = 0; k < len; k++)
        out[k] = InfoHidden(code, d, len, k) ? '1' : '0';
    out[len] = 0;
}

int main()
{
    char h[64];
    // VHCheck 1.1 on 4125 (2026-11-20): 0x4112 00 «156368» + 8 spaces - the last 2 digits only
    const unsigned char sn[] = { 0, '1', '5', '6', '3', '6', '8', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ' };
    Hidden(0x4112, sn, sizeof(sn), h);
    Check(strcmp(h, "011110000000000") == 0, "serial: the 2 last digits and the spaces shown");
    const unsigned char sn2[] = { 0, '9', '3', '1', '5', '1' };   // 4120's «93151» with no trailing spaces
    Hidden(0x4112, sn2, sizeof(sn2), h);
    Check(strcmp(h, "011100") == 0, "serial with no spaces: the same");
    const unsigned char sn3[] = { 0, '7', ' ' };
    Hidden(0x4112, sn3, sizeof(sn3), h);
    Check(strcmp(h, "000") == 0, "one digit: nothing to hide");
    // 0x7922 00 06 04 F8 C2 5A 95 6E: the maker (3 bytes) shown, the unit's 3 bytes hidden
    const unsigned char bt[] = { 0, 6, 0x04, 0xF8, 0xC2, 0x5A, 0x95, 0x6E };
    Hidden(0x7922, bt, sizeof(bt), h);
    Check(strcmp(h, "00000111") == 0, "BT address: the last 3 bytes hidden");
    // the rest as they are: the versions, the model, the date
    const unsigned char md[] = { 0, 0x41, 0x80, 0x05, 0x14, 0x20 };
    Hidden(0x4118, md, sizeof(md), h);
    Check(strcmp(h, "000000") == 0, "0x4118 shown");
    Hidden(0x7902, sn, sizeof(sn), h);
    Check(strcmp(h, "000000000000000") == 0, "0x7902 shown (digits there are the version)");

    char k[16];
    LineKind("$GPRMC,123519,A,5545.1234,N,03737.5678,E,0.0,0.0,201126,,*6A", k, sizeof(k));
    Check(strcmp(k, "$GPRMC") == 0, "RMC: the type only, no position");
    LineKind("$GPGGA*47", k, sizeof(k));
    Check(strcmp(k, "$GPGGA") == 0, "a line with no comma: up to '*'");
    LineKind("G,-13.0,*79", k, sizeof(k));
    Check(strcmp(k, "G") == 0, "a broken piece (4125): its head");
    LineKind("@ABCDEFGHIJKLMNOPQRSTUVWXYZ", k, 8);
    Check(strcmp(k, "@ABCDEF") == 0, "a long head cut to the room");
    LineKind(",5545.1234,N", k, sizeof(k));
    Check(k[0] == 0, "a line from its comma on: nothing of it");
    LineKind("", k, sizeof(k));
    Check(k[0] == 0, "empty");

    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
