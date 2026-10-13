// drilltag_hosttest.c - PC-side proof that install/drilltag.h recognises the drill package:
// it compiles the very same header the ARM installer uses and runs it against the real
// rescue\drill\SwDnl_drill.upg plus synthetic regressions.
//
// The bug it pins down (2026-10-11): the .upg package is BINARY, its TLV TOC contains NUL
// bytes (the "S2" record header is 'S','2',0x00,...) and "VestaDrill" only appears later in
// the S4 record. The old strstr()-based scan stopped at the first NUL and refused a
// byte-identical, md5-matched package on the HU with "не содержит маркер VestaDrill".
//
//   tools\drilltag_hosttest.bat
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "../install/drilltag.h"

#define SYNTH   L"..\\obj\\host\\drilltag_synth.bin"
#define HOSTDIR "..\\obj\\host\\"
#define PKG     "..\\rescue\\drill\\SwDnl_drill.upg"
#define MAXLEN  (256 * 1024)

static int gBad = 0;

static void Report(const char* what, int got, int want)
{
    if (got != want) gBad++;
    printf("%-4s %-44s got=%d want=%d\n", got == want ? "ok" : "FAIL", what, got, want);
}

static void WriteSynth(const void* data, int len)
{
    FILE* f = fopen(HOSTDIR "drilltag_synth.bin", "wb");
    if (!f) { printf("FAIL cannot write synthetic package\n"); gBad++; return; }
    fwrite(data, 1, (size_t)len, f);
    fclose(f);
}

static int Find(const unsigned char* p, int n, const char* what)
{
    int i, len = (int)strlen(what);
    for (i = 0; i + len <= n; i++)
        if (memcmp(p + i, what, len) == 0) return i;
    return -1;
}

static void CheckRealPackage(void)
{
    unsigned char d[4096];
    FILE* f = fopen(PKG, "rb");
    size_t n;
    int nulAt, tagAt;

    if (!f) { printf("FAIL cannot open %s\n", PKG); gBad++; return; }
    n = fread(d, 1, sizeof(d), f);
    fclose(f);
    nulAt = -1;
    {   int i;
        for (i = 0; i < (int)n; i++) if (d[i] == 0) { nulAt = i; break; }
    }
    tagAt = Find(d, (int)n, kDrillTag);
    printf("package %s: %u bytes, first NUL @ %d, %s @ %d\n",
           PKG, (unsigned)n, nulAt, kDrillTag, tagAt);
    if (nulAt >= 0 && tagAt > nulAt)
        printf("         NUL before the tag -> a strstr()-style scan stops early (the bug)\n");
    Report("real drill package is recognised", FileHasDrillTag(L"..\\rescue\\drill\\SwDnl_drill.upg", MAXLEN), 1);
}

int main(void)
{
    static unsigned char buf[8192];
    int tagLen = (int)strlen(kDrillTag);

    CheckRealPackage();

    // NULs first, tag after - the real package's layout, the exact regression case
    memset(buf, 0, 64);
    memcpy(buf + 64, kDrillTag, (size_t)tagLen);
    WriteSynth(buf, 64 + tagLen);
    Report("64 NULs then the tag", FileHasDrillTag(SYNTH, MAXLEN), 1);

    // tag at the very start
    memcpy(buf, kDrillTag, (size_t)tagLen);
    WriteSynth(buf, tagLen);
    Report("tag at offset 0", FileHasDrillTag(SYNTH, MAXLEN), 1);

    // one letter short - must not match
    memcpy(buf, "VestaDril", 10);
    WriteSynth(buf, 9);
    Report("'VestaDril' (short) is not the tag", FileHasDrillTag(SYNTH, MAXLEN), 0);

    // long enough, but no tag at all
    memset(buf, 'X', 200);
    WriteSynth(buf, 200);
    Report("no tag in 200 'X'", FileHasDrillTag(SYNTH, MAXLEN), 0);

    // NULs only
    memset(buf, 0, 64);
    WriteSynth(buf, 64);
    Report("NULs only", FileHasDrillTag(SYNTH, MAXLEN), 0);

    // empty file
    WriteSynth(buf, 0);
    Report("empty file", FileHasDrillTag(SYNTH, MAXLEN), 0);

    // does not exist
    Report("missing file", FileHasDrillTag(L"..\\obj\\host\\drilltag_absent.bin", MAXLEN), 0);

    // tag beyond the first 4096-byte read -> the multi-read loop must still find it
    memset(buf, 'X', 8192);
    memcpy(buf + 4500, kDrillTag, (size_t)tagLen);
    WriteSynth(buf, 8192);
    Report("tag after the first read (4500)", FileHasDrillTag(SYNTH, MAXLEN), 1);

    // maxLen bounds how much of an unknown file is scanned (tag at 4500, budget 100)
    Report("maxLen=100 stops before the tag", FileHasDrillTag(SYNTH, 100), 0);

    printf(gBad ? "=== %d FAILURES\n" : "=== all OK\n", gBad);
    return gBad ? 1 : 0;
}
