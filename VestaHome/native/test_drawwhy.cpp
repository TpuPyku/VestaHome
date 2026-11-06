// test_drawwhy.cpp - drawwhy.cpp on the PC: the frames' callers as the log shows them. Exit code: the failed checks.
#include <stdio.h>
#include <string.h>
#include "drawwhy.h"

static int s_failed, s_checks;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAILED: %s\n", what);
    }
}

static void Times(const char* file, int line, int n)
{
    for (int i = 0; i < n; i++)
        WhyCount(file, line);
}

int main()
{
    wchar_t s[128];
    WhyTake(s, 128);
    Check(!s[0], "nothing asked: empty");

    Times(".\\home.cpp", 812, 60);
    Times("c:\\a\\b\\vh_gps.inc", 88, 12);
    Times("x/vh_draw.inc", 5, 30);
    Times("home.cpp", 900, 1);
    WhyTake(s, 128);
    Check(!wcscmp(s, L"home.cpp:812 x60, vh_draw.inc:5 x30, vh_gps.inc:88 x12"), "the top three, the most first, no folders");
    WhyTake(s, 128);
    Check(!s[0], "taking starts a new count");

    Times("home.cpp", 1, 2);
    Times("vh_a.inc", 1, 3);
    WhyTake(s, 128);
    Check(!wcscmp(s, L"vh_a.inc:1 x3, home.cpp:1 x2"), "the same line of two files: apart");

    for (int i = 0; i < 100; i++)
        WhyCount("many.cpp", i);
    Times("late.cpp", 7, 5);
    WhyTake(s, 128);
    Check(wcsstr(s, L"many.cpp:") != NULL, "more callers than the table holds: no harm");

    Times("long_name_of_a_file.cpp", 12345, 2);
    wchar_t tiny[10];
    WhyTake(tiny, 10);
    Check(wcslen(tiny) == 9, "a short buffer: cut and closed");

    Times("first_file_name.cpp", 11, 3);
    Times("second_file_name.cpp", 22, 2);
    wchar_t mid[32];
    wmemset(mid, L'#', 32);
    WhyTake(mid, 32);
    Check(!wcsncmp(mid, L"first_file_name.cpp:11 x3", 25), "the second one cut: the first one whole");
    Check(wcslen(mid) == 31, "the second one cut: closed at the buffer's end");
    WhyTake(mid, 32);
    Check(!mid[0], "after a cut the count starts anew");

    Times("exact.cpp", 1, 1);
    wchar_t fit[14];   // "exact.cpp:1 x1": 14 characters, no room for the zero
    WhyTake(fit, 14);
    Check(wcslen(fit) == 13, "just no room for the zero: closed");
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
