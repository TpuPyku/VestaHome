// tagtext.h - a music file's tag text in Windows-1251 that the tags library read as Latin-1 (the HU 2026-11-23:
// "В Пустоте" shown as "Â Ïóñòîòå"; ID3v1 and ID3v2 "ISO-8859-1" frames written by Russian programs).
// The text is taken for 1251 when it has nothing above U+00FF and two Cyrillic letters' codes side by side
// (C0..FF, A8 Ё, B8 ё): Russian words have them, Latin-1 names rarely do ("Café", "Mötley Crüe": single ones).
#pragma once
#include <windows.h>

inline bool TagIs1251Letter(wchar_t c)
{
    return (c >= 0xC0 && c <= 0xFF) || c == 0xA8 || c == 0xB8;
}

inline bool TagLooks1251(const wchar_t* s)
{
    bool pair = false;
    for (int i = 0; s[i]; i++) {
        if (s[i] > 0xFF)
            return false;
        if (i > 0 && TagIs1251Letter(s[i - 1]) && TagIs1251Letter(s[i]))
            pair = true;
    }
    return pair;
}

inline void TagText1251(wchar_t* s)
{
    static const unsigned short hi[64] = {   // 1251's 80..BF
        0x0402, 0x0403, 0x201A, 0x0453, 0x201E, 0x2026, 0x2020, 0x2021, 0x20AC, 0x2030, 0x0409, 0x2039, 0x040A,
        0x040C, 0x040B, 0x040F, 0x0452, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x0098, 0x2122,
        0x0459, 0x203A, 0x045A, 0x045C, 0x045B, 0x045F, 0x00A0, 0x040E, 0x045E, 0x0408, 0x00A4, 0x0490, 0x00A6,
        0x00A7, 0x0401, 0x00A9, 0x0404, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x0407, 0x00B0, 0x00B1, 0x0406, 0x0456,
        0x0491, 0x00B5, 0x00B6, 0x00B7, 0x0451, 0x2116, 0x0454, 0x00BB, 0x0458, 0x0405, 0x0455, 0x0457 };
    if (!TagLooks1251(s))
        return;
    for (int i = 0; s[i]; i++)
        if (s[i] >= 0xC0)
            s[i] = (wchar_t)(0x0410 + s[i] - 0xC0);
        else if (s[i] >= 0x80)
            s[i] = hi[s[i] - 0x80];
}
