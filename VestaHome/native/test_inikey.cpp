// test_inikey.cpp - one ini key set (inikey.cpp) on the PC (test_inikey.bat).
#include <stdio.h>
#include <string.h>
#include "inikey.h"

static int s_failed, s_checks;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAIL: %s\n", what);
    }
}

static void Put(const char* in, const char* want, bool wantChanged, const char* what)
{
    char out[256];
    memset(out, 'x', sizeof(out));
    bool changed = !wantChanged;
    int n = IniPut(in, (int)strlen(in), "GPSEpoch", "2", out, sizeof(out), &changed);
    bool ok = n == (int)strlen(want) && !memcmp(out, want, n) && changed == wantChanged;
    Check(ok, what);
    if (!ok)
        printf("  got %d \"%.*s\" changed %d\n  want \"%s\"\n", n, n > 0 ? n : 0, out, changed, want);
}

int main()
{
    Put("Area = 1\r\nGPSEpoch = 1\r\nZoom = 3\r\n", "Area = 1\r\nGPSEpoch = 2\r\nZoom = 3\r\n", true, "replaced in place");
    Put("Area = 1\r\nZoom = 3\r\n", "Area = 1\r\nZoom = 3\r\nGPSEpoch = 2\r\n", true, "added at the end");
    Put("Area = 1", "Area = 1\r\nGPSEpoch = 2\r\n", true, "added after a last line without its end");
    Put("", "GPSEpoch = 2\r\n", true, "an empty file");
    Put("A = 1\r\nGPSEpoch = 2\r\n", "A = 1\r\nGPSEpoch = 2\r\n", false, "already so");
    Put("GPSEpoch=2\r\n", "GPSEpoch = 2\r\n", true, "its spacing made Navitel's");
    Put("GPSEpoch = 1\r\nB = 2\r\n", "GPSEpoch = 2\r\nB = 2\r\n", true, "the first line");
    Put("A = 1\r\nGPSEpoch = 1", "A = 1\r\nGPSEpoch = 2", true, "the last line without its end");
    Put("A = 1\nGPSEpoch = 1\nB = 2\n", "A = 1\nGPSEpoch = 2\nB = 2\n", true, "LF line ends");
    Put("GPSEpochX = 1\r\nXGPSEpoch = 1\r\n", "GPSEpochX = 1\r\nXGPSEpoch = 1\r\nGPSEpoch = 2\r\n", true,
        "other keys starting or ending alike");
    Put("A = GPSEpoch = 1\r\n", "A = GPSEpoch = 1\r\nGPSEpoch = 2\r\n", true, "the key in a value");
    Put("GPSEpoch = 1\r\nGPSEpoch = 3\r\n", "GPSEpoch = 2\r\nGPSEpoch = 3\r\n", true, "the first of two");
    Put("GPSEpoch\r\n", "GPSEpoch = 2\r\n", true, "the key without '='");
    Put("A = 1\r\nGPSEpoch", "A = 1\r\nGPSEpoch = 2", true, "the key alone at the file's end");
    char out[32];
    bool changed;
    Check(IniPut("A = 1\r\n", 7, "GPSEpoch", "2", out, 16, &changed) == -1, "no room");
    Check(IniPut("A = 1\r\n", 7, "GPSEpoch", "2", out, 21, &changed) == 21, "just the room");
    printf("%d of %d checks failed\n", s_failed, s_checks);
    return s_failed;
}
