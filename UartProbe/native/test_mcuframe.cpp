// The MCU's UART frames (mcuframe.cpp): the bytes from the firmware's decompilation (PROJECT.md, 2026-11-11).
#include <stdio.h>
#include <string.h>
#include "mcuframe.h"

static int s_checks, s_failed;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAIL: %s\n", what);
    }
}

static bool Same(const unsigned char* a, const unsigned char* b, int n)
{
    return !memcmp(a, b, n);
}

int main()
{
    unsigned char out[16], v = 10;
    static const unsigned char vol10[] = { 0x06, 0xFF, 0x80, 0x10, 0xFF, 0x0A, 0x9C };
    Check(McuFrame(0x8010, &v, 1, out) == 7 && Same(out, vol10, 7), "the volume 10: 06 FF 80 10 FF 0A 9C");
    static const unsigned char probe[] = { 0x05, 0xFF, 0x0F, 0xFF, 0xFF, 0xF5 };
    Check(McuFrame(0x0FFF, NULL, 0, out) == 6 && Same(out, probe, 6), "no data, the id 0FFF: 05 FF 0F FF FF F5");
    unsigned char two[2] = { 0x01, 0x02 };
    Check(McuFrame(0x1234, two, 2, out) == 8 && out[0] == 7 && out[5] == 1 && out[6] == 2 &&
          out[7] == (0x07 ^ 0xFF ^ 0x12 ^ 0x34 ^ 0xFF ^ 0x01 ^ 0x02), "two bytes: L 7, the xor over all");

    int id = -1, st = -1;
    static const unsigned char bad[] = { 0x06, 0xFF, 0x80, 0x10, 0xFF, 0xB1, 0x27 };
    Check(McuReply(bad, 7, &id, &st) == 0 && id == 0x8010 && st == 0xB1, "the reply B1 to 8010 at 0");
    static const unsigned char noise[] = { 0x00, 0x55, 0x06, 0xFF, 0x0F, 0xFF, 0xFF, 0xA2, 0x54, 0x00 };
    Check(McuReply(noise, 10, &id, &st) == 2 && id == 0x0FFF && st == 0xA2, "after two other bytes: at 2");
    unsigned char wrong[7];
    memcpy(wrong, bad, 7);
    wrong[6] ^= 1;
    Check(McuReply(wrong, 7, &id, &st) == -1, "a wrong xor: none");
    Check(McuReply(bad, 6, &id, &st) == -1, "cut short: none");
    memcpy(wrong, bad, 7);
    wrong[1] = 0xFE;
    wrong[6] ^= 0x01;
    Check(McuReply(wrong, 7, &id, &st) == -1, "no FF after L (xor right): none");
    memcpy(wrong, bad, 7);
    wrong[4] = 0xFE;
    wrong[6] ^= 0x01;
    Check(McuReply(wrong, 7, &id, &st) == -1, "no FF after the id (xor right): none");
    static const unsigned char echo[] = { 0x06, 0xFF, 0x80, 0x10, 0xFF, 0x0A, 0x9C, 0x06, 0xFF, 0x80, 0x10, 0xFF, 0x00, 0x96 };
    Check(McuReply(echo, 14, &id, &st) == 0 && st == 0x0A, "our own frame echoed reads as a reply (status = data)");
    static const unsigned char seven[] = { 0x07, 0xFF, 0x80, 0x10, 0xFF, 0x00, 0x00, 0x91 };
    Check(McuReply(seven, 8, &id, &st) == -1, "L other than 6: not a reply");
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
