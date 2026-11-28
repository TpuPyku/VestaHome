// test_stubcan.cpp - stubcan.h on the PC: does the loader's file know the update from "О системе".
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "stubcan.h"

static int s_failed, s_checks;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAILED: %s\n", what);
    }
}

static int Put(unsigned char* buf, int at, const wchar_t* s)   // UTF-16LE, as a PE's strings
{
    int n = (int)wcslen(s);
    for (int i = 0; i < n; i++) {
        buf[at + 2 * i] = (unsigned char)(s[i] & 255);
        buf[at + 2 * i + 1] = (unsigned char)(s[i] >> 8);
    }
    return at + 2 * n;
}

int main()
{
    static unsigned char buf[4096];
    memset(buf, 0x41, sizeof(buf));
    Check(!StubKnowsUpdate(buf, sizeof(buf)), "an old loader: no job's name");
    int end = Put(buf, 1000, L"\\SDMMC\\VestaUpdate.job");
    Check(StubKnowsUpdate(buf, sizeof(buf)), "the new one: the job's name in it");
    Check(StubKnowsUpdate(buf, end), "the name at the very end");
    Check(!StubKnowsUpdate(buf, end - 1), "cut short: not there");
    memset(buf, 0x41, sizeof(buf));
    Put(buf, 7, L"VestaUpdate.job");
    Check(StubKnowsUpdate(buf, sizeof(buf)), "at an odd offset too");
    memset(buf, 0x41, sizeof(buf));
    Put(buf, 10, L"VestaUpdate.jo");
    Check(!StubKnowsUpdate(buf, sizeof(buf)), "a part of it: no");
    memcpy(buf + 100, "VestaUpdate.job", 15);   // (ASCII: not the stub's wide string)
    Check(!StubKnowsUpdate(buf, sizeof(buf)), "ASCII: no");
    Check(!StubKnowsUpdate(buf, 0) && !StubKnowsUpdate(NULL, 0), "nothing read: no");
    memset(buf, 0x41, sizeof(buf));
    Put(buf, 0, L"VestaUpdate.job");
    Check(StubKnowsUpdate(buf, sizeof(buf)), "at the file's very start");
    memset(buf, 0, sizeof(buf));   // (a PE's string: its NUL after it)
    Put(buf, 64, L"VestaUpdate.job");
    Check(StubKnowsUpdate(buf, sizeof(buf)), "ended by its NUL, zeros around");
    memset(buf, 0x41, sizeof(buf));
    Put(buf, 64, L"XestaUpdate.job");
    Check(!StubKnowsUpdate(buf, sizeof(buf)), "its first letter other: no");
    memset(buf, 0x41, sizeof(buf));
    Put(buf, 64, L"\x0156\x0165\x0173\x0174\x0161\x0155\x0170\x0164\x0161\x0174\x0165\x012E\x016A\x016F\x0162");
    Check(!StubKnowsUpdate(buf, sizeof(buf)), "the low bytes alike, not the high ones: no");
    printf("stubcan: %d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
