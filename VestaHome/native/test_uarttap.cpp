// test_uarttap.cpp - the COM2 listener's log lines and budget (uarttap.h), on the PC.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "uarttap.h"

static int s_failed, s_checks;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAIL: %s\n", what);
    }
}

static bool Same(const wchar_t* got, const wchar_t* want)
{
    if (!wcscmp(got, want))
        return true;
    wprintf(L"  got  [%s]\n  want [%s]\n", got, want);
    return false;
}

static void TestLine()
{
    wchar_t s[200];
    const unsigned char b[] = { 0x05, 'A', 'b', 0xFF, ' ', 0x7E, 0x7F };
    TapLine(b, 7, s, 200);
    Check(Same(s, L"05 41 62 FF 20 7E 7F | .Ab. ~."), "hex, then the printable ones, dots for the rest");
    TapLine(b, 0, s, 200);
    Check(Same(s, L" | "), "nothing");
    TapLine(b, 7, s, 12);
    Check(wcslen(s) == 11 && !wcsncmp(s, L"05 41 62 FF", 11), "cut to the room there is, ended");
    TapLine(b, 7, s, 11);
    Check(Same(s, L"05 41 62 |"), "no room for a whole byte: the hex stops, the bar cut");
    TapLine(b + 1, 2, s, 10);
    Check(Same(s, L"41 62 | A"), "the text cut to the room left");
}

static void TestErrors()
{
    wchar_t s[TAP_ERR_CAP];
    TapErrors(0, s);
    Check(Same(s, L""), "no errors - empty");
    TapErrors(TAP_CE_FRAME, s);
    Check(Same(s, L"кадр"), "frame");
    TapErrors(TAP_CE_FRAME | TAP_CE_RXPARITY | TAP_CE_OVERRUN | TAP_CE_RXOVER | TAP_CE_BREAK, s);
    Check(Same(s, L"кадр, чётность, переполнение, очередь, break"), "all of them, in order");
    TapErrors(TAP_CE_BREAK, s);
    Check(Same(s, L"break"), "break alone");
}

static void TestBudget()
{
    TapBudget t;
    TapBudgetInit(&t, 1000);
    for (int i = 0; i < TAP_LINES_MIN; i++)
        Check(TapAllow(&t, 5), "within the minute's lines");
    Check(!TapAllow(&t, 7), "over: refused");
    Check(!TapAllow(&t, 3), "still over");
    Check(t.skippedBytes == 10 && t.skippedLines == 2, "the refused counted");
    unsigned lines = 0, bytes = 0;
    Check(!TapRoll(&t, 60999, &lines, &bytes), "the minute not over yet");
    Check(TapRoll(&t, 61000, &lines, &bytes) && lines == 2 && bytes == 10, "a new minute: what was skipped");
    Check(t.lines == 0 && t.skippedLines == 0 && t.skippedBytes == 0, "the counts start again");
    Check(TapAllow(&t, 1), "lines again");
    for (int i = 1; i < TAP_LINES_MIN; i++)
        TapAllow(&t, 1);
    Check(!TapAllow(&t, 4), "one refused");
    Check(TapRoll(&t, 121001, &lines, &bytes) && lines == 1 && bytes == 4, "a single skipped line is told");
    Check(!TapRoll(&t, 181001, &lines, &bytes), "a minute with nothing skipped: nothing to say");
}

int main()
{
    TestLine();
    TestErrors();
    TestBudget();
    printf("uarttap: %d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
