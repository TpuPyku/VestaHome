// test_tagtext.cpp - a tag's text in Windows-1251 read as Latin-1 (tagtext.h), on the PC.
#include <windows.h>
#include <stdio.h>
#include "tagtext.h"

static int s_failed, s_checks;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAIL: %s\n", what);
    }
}

static bool Fixed(const wchar_t* in, const wchar_t* want)
{
    wchar_t s[100];
    wcscpy(s, in);
    TagText1251(s);
    return !wcscmp(s, want);
}

int main()
{
    // the HU 2026-11-23: "В Пустоте" (1251 bytes C2 20 CF F3 F1 F2 EE F2 E5) shown as "Â Ïóñòîòå"
    Check(Fixed(L"\x00C2 \x00CF\x00F3\x00F1\x00F2\x00EE\x00F2\x00E5",
                L"\x0412 \x041F\x0443\x0441\x0442\x043E\x0442\x0435"), "1251 read as Latin-1: Cyrillic");
    // Ё / ё, the dash, the quotes, the number sign, and ASCII between kept: "Ёлка — «ёж» №1 2004"
    Check(Fixed(L"\x00A8\x00EB\x00EA\x00E0 \x0097 \x00AB\x00B8\x00E6\x00BB \x00B9" L"1 2004",
                L"\x0401\x043B\x043A\x0430 \x2014 \x00AB\x0451\x0436\x00BB \x2116" L"1 2004"), "the upper half's signs");
    // Latin-1 that is Latin-1: single accented letters between ASCII ones stay
    Check(Fixed(L"Caf\x00E9 \x00E9t\x00E9", L"Caf\x00E9 \x00E9t\x00E9"), "French stays");
    Check(Fixed(L"M\x00F6tley Cr\x00FC" L"e", L"M\x00F6tley Cr\x00FC" L"e"), "German stays");
    Check(Fixed(L"Song", L"Song"), "ASCII stays");
    Check(Fixed(L"", L""), "empty");
    // real Unicode (UTF-8 / UTF-16 tags): untouched even with Latin-1 pairs in it
    Check(Fixed(L"\x0412\x00E0\x00E0", L"\x0412\x00E0\x00E0"), "Unicode stays");
    // the edges: А (C0) and я (FF) are letters, so are Ё (A8) and ё (B8); 80 is the table's first
    Check(Fixed(L"\x00C0\x00E0", L"\x0410\x0430"), "Аа: C0 and the first pair");
    Check(Fixed(L"\x00E0\x00FF", L"\x0430\x044F"), "ая: FF");
    Check(Fixed(L"\x00A8\x00E6", L"\x0401\x0436"), "Ёж");
    Check(Fixed(L"\x00B8\x00E6", L"\x0451\x0436"), "ёж");
    Check(Fixed(L"\x0080\x00E0\x00E0", L"\x0402\x0430\x0430"), "80: Ђ");
    wchar_t b[8] = { 0xE0, 0xE0, L' ', L'x', 0 };   // a letter just before the text is not the text's
    TagText1251(b + 1);
    Check(b[1] == 0xE0, "nothing read before the text");
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
