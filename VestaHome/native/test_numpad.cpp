// numpad: the digits' keyboard - whole numbers, coordinates, a date typed as digits (dots put by itself).
#include "numpad.h"
#include <stdio.h>
#include <string.h>
#include <wchar.h>

static int s_checks, s_failed;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAIL: %s\n", what);
    }
}

static const wchar_t* Typed(int kind, const wchar_t* start, const wchar_t* keys, int cap = 23)
{
    static wchar_t t[64];
    wcscpy(t, start);
    for (const wchar_t* k = keys; *k; k++) {
        if (*k == L'<')
            NumBack(kind, t);
        else
            NumType(kind, t, cap, *k);
    }
    return t;
}

int main()
{
    Check(!wcscmp(Typed(NUM_INT, L"", L"12345"), L"12345"), "int: digits");
    Check(!wcscmp(Typed(NUM_INT, L"", L"1.2-3"), L"123"), "int: no dot, no minus");
    Check(!wcscmp(Typed(NUM_INT, L"", L"123456", 4), L"1234"), "int: up to cap");
    Check(!wcscmp(Typed(NUM_INT, L"987", L"<<"), L"9"), "int: back");
    Check(!wcscmp(Typed(NUM_INT, L"", L"<"), L""), "int: back on nothing");

    Check(!wcscmp(Typed(NUM_DEC, L"", L"55.75"), L"55.75"), "dec: digits and a dot");
    Check(!wcscmp(Typed(NUM_DEC, L"", L"55.7.5"), L"55.75"), "dec: one dot only");
    Check(!wcscmp(Typed(NUM_DEC, L"", L".5"), L"0.5"), "dec: a dot first - 0.");
    Check(!wcscmp(Typed(NUM_DEC, L"", L"37-"), L"-37"), "dec: minus - the sign in front");
    Check(!wcscmp(Typed(NUM_DEC, L"", L"37--"), L"37"), "dec: minus again - the sign off");
    Check(!wcscmp(Typed(NUM_DEC, L"5", L"-", 1), L"5"), "dec: no room for the sign");
    Check(!wcscmp(Typed(NUM_DEC, L"-1", L"<<"), L""), "dec: back over the sign");

    Check(!wcscmp(Typed(NUM_DATE, L"", L"1"), L"1"), "date: one digit");
    Check(!wcscmp(Typed(NUM_DATE, L"", L"14"), L"14"), "date: the day");
    Check(!wcscmp(Typed(NUM_DATE, L"", L"141"), L"14.1"), "date: the month begins - a dot");
    Check(!wcscmp(Typed(NUM_DATE, L"", L"14112"), L"14.11.2"), "date: the year begins - a dot");
    Check(!wcscmp(Typed(NUM_DATE, L"", L"141120261"), L"14.11.2026"), "date: 8 digits at most");
    Check(!wcscmp(Typed(NUM_DATE, L"", L"14.a11"), L"14.11"), "date: digits only");
    Check(!wcscmp(Typed(NUM_DATE, L"14.11", L"<"), L"14.1"), "date: back - a digit");
    Check(!wcscmp(Typed(NUM_DATE, L"14.1", L"<"), L"14"), "date: back - the dot goes with the digit");
    Check(!wcscmp(Typed(NUM_DATE, L"14.11.2025", L"<<<<<<"), L"14"), "date: back to the day");
    Check(!wcscmp(Typed(NUM_DATE, L"", L"<"), L""), "date: back on nothing");

    Check(NumKeyShown(NUM_DEC, L'.') && NumKeyShown(NUM_DEC, L'-'), "dec: the dot and the minus keys");
    Check(!NumKeyShown(NUM_INT, L'.') && !NumKeyShown(NUM_DATE, L'-'), "int, date: no dot, no minus keys");
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
