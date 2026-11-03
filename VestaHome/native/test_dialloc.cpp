// test_dialloc.cpp - the dialing location's fields (dialloc.cpp) on the PC (test_dialloc.bat).
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "dialloc.h"

static int s_failed, s_checks;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAIL: %s\n", what);
    }
}

// The unit's location "1" (its registry 2026-11-03), "Car" ("0") and broken ones (the fields one literal each:
// "\0" before a digit would be an octal escape)
static const wchar_t kUnit[] = L"Home\0" L"G\0" L"8,FG\0" L"8,10,EFG\0" L"095\0" L" \0" L"7\0" L"0\0";
static const wchar_t kCar[] = L"Car\0" L"G\0" L"G\0" L"G\0" L"\0" L"\0" L"0\0";
static const wchar_t kSix[] = L"A\0" L"G\0" L"G\0" L"G\0" L"1\0" L" \0";
static const wchar_t kWord[] = L"A\0" L"G\0" L"G\0" L"G\0" L"1\0" L" \0" L"x7\0";
static const wchar_t kEmpty[] = L"A\0" L"G\0" L"G\0" L"G\0" L"1\0" L" \0" L"\0";
static const wchar_t kTail[] = L"A\0" L"G\0" L"G\0" L"G\0" L"1\0" L" \0" L"44";
static const wchar_t kThree[] = L"A\0" L"G\0" L"G\0" L"G\0" L"\0" L" \0" L"390\0";
static const wchar_t kNoName[] = L"\0" L"G\0" L"G\0" L"G\0" L"12\0" L" \0" L"7\0";

static bool Parse(const wchar_t* loc, int chars, wchar_t* area, int cap, DWORD* country)
{
    wcscpy(area, L"x");
    *country = 99;
    return DialLocation(loc, chars, area, cap, country);
}

#define N(a) ((int)(sizeof(a) / sizeof(a[0])))

int main()
{
    wchar_t area[8];
    DWORD c;
    Check(Parse(kUnit, N(kUnit), area, 8, &c) && !wcscmp(area, L"095") && c == 7, "the unit's: 095, 7");
    Check(Parse(kCar, N(kCar), area, 8, &c) && !wcscmp(area, L"") && c == 0, "Car: no area, 0");
    Check(Parse(kUnit, N(kUnit), area, 4, &c) && !wcscmp(area, L"095"), "the area just fits");
    Check(!Parse(kUnit, N(kUnit), area, 3, &c), "the area past its room");
    Check(!Parse(kSix, N(kSix), area, 8, &c), "no country field");
    Check(!Parse(kWord, N(kWord), area, 8, &c), "the country not a number");
    Check(!Parse(kEmpty, N(kEmpty), area, 8, &c), "the country empty");
    Check(Parse(kTail, N(kTail) - 1, area, 8, &c) && c == 44, "the last field without its terminator");
    Check(!Parse(kUnit, 20, area, 8, &c), "cut short: the chars counted");
    Check(Parse(kThree, N(kThree), area, 8, &c) && c == 390 && !area[0], "three digits, a 9 among them");
    Check(Parse(kTail, N(kTail) - 2, area, 8, &c) && c == 4, "cut in the number: only the chars counted");
    Check(Parse(kNoName, N(kNoName), area, 8, &c) && !wcscmp(area, L"12") && c == 7, "the name empty");
    Check(Parse(kUnit, N(kUnit), area, 8, &c) && !wcscmp(area, L"095"), "the area whole, its end there");
    printf("%d of %d checks failed\n", s_failed, s_checks);
    return s_failed;
}
