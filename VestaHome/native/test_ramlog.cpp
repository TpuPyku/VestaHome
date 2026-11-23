// test_ramlog.cpp - VestaHome's log kept in RAM while no card is there (ramlog.cpp), on the PC: files in obj\.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ramlog.h"

static int s_failed, s_checks;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAIL: %s\n", what);
    }
}

static const wchar_t* kRam = L"obj\\ramlog_t.log";
static const wchar_t* kOld = L"obj\\ramlog_t.old.log";
static const wchar_t* kCard = L"obj\\ramlog_card.log";

static DWORD SizeOf(const wchar_t* p)   // -1: none
{
    WIN32_FIND_DATA fd;
    HANDLE ff = FindFirstFile(p, &fd);
    if (ff == INVALID_HANDLE_VALUE)
        return (DWORD)-1;
    FindClose(ff);
    return fd.nFileSizeLow;
}

static int ReadAll(const wchar_t* p, char* buf, int cap)
{
    HANDLE h = CreateFile(p, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    DWORD got = 0;
    if (h != INVALID_HANDLE_VALUE) {
        ReadFile(h, buf, cap - 1, &got, NULL);
        CloseHandle(h);
    }
    buf[got] = 0;
    return (int)got;
}

static DWORD FlushTo(const wchar_t* card)
{
    HANDLE h = CreateFile(card, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, 0, NULL);
    SetFilePointer(h, 0, NULL, FILE_END);
    DWORD n = RamLogFlush(kRam, kOld, h);
    CloseHandle(h);
    return n;
}

static int Num(const char* s)   // a line's number: its first 5 digits
{
    char d[6];
    memcpy(d, s, 5);
    d[5] = 0;
    return atoi(d);
}

static void Clean()   // (a mutant may have left them read-only)
{
    const wchar_t* all[3] = { kRam, kOld, kCard };
    for (int i = 0; i < 3; i++) {
        SetFileAttributes(all[i], FILE_ATTRIBUTE_NORMAL);
        DeleteFile(all[i]);
    }
}

int main()
{
    CreateDirectory(L"obj", NULL);
    Clean();
    // the rule: a line that would take the RAM file past half the room turns it into the old one
    Check(!RamLogTurn(0, 100), "empty: no turn");
    Check(!RamLogTurn(kRamLogHalf - 100, 100), "exactly full: no turn");
    Check(RamLogTurn(kRamLogHalf - 99, 100), "one byte over: turn");
    Check(kRamLogHalf == 32 * 1024, "32 KB each, 64 KB in all");
    // nothing kept: nothing copied, the card's log as it was
    Check(FlushTo(kCard) == 0, "no RAM log: 0 bytes");
    Check(SizeOf(kCard) == 0, "no RAM log: the card's log empty");
    // two lines, then the card: copied in order, the RAM files gone
    RamLogPut(kRam, kOld, "one\r\n", 5);
    RamLogPut(kRam, kOld, "two\r\n", 5);
    HANDLE h = CreateFile(kCard, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(h, "had\r\n", 5, &(DWORD&)*(new DWORD), NULL);
    CloseHandle(h);
    Check(FlushTo(kCard) == 10, "two lines: 10 bytes");
    char buf[80000];
    ReadAll(kCard, buf, sizeof(buf));
    Check(!strcmp(buf, "had\r\none\r\ntwo\r\n"), "appended after what the card had, in order");
    Check(SizeOf(kRam) == (DWORD)-1 && SizeOf(kOld) == (DWORD)-1, "the RAM files removed");
    // many lines: bounded - each file half the room at the most, the newest kept, the order kept at the flush
    DeleteFile(kCard);
    char line[101];
    for (int i = 0; i < 2000; i++) {   // 200 KB
        _snprintf(line, sizeof(line), "%05d%094d\r\n", i, 0);
        RamLogPut(kRam, kOld, line, 101);
    }
    Check(SizeOf(kRam) <= kRamLogHalf && SizeOf(kOld) <= kRamLogHalf, "each RAM file within half the room");
    Check(SizeOf(kOld) > kRamLogHalf - 101, "the old one full");
    DWORD kept = SizeOf(kRam) + SizeOf(kOld);
    Check(FlushTo(kCard) == kept, "all that was kept copied");
    int n = ReadAll(kCard, buf, sizeof(buf));
    Check(n == (int)kept && n % 101 == 0, "whole lines");
    Check(!strncmp(buf + n - 101, "01999", 5), "the newest line last");
    bool order = true;
    for (int k = 101; k < n && order; k += 101)
        order = Num(buf + k - 101) + 1 == Num(buf + k);
    Check(order, "the lines in their order");
    Clean();
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
