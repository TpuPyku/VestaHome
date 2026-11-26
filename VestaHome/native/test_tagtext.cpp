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

// One-byte text (ID3v1, an ID3v2 frame "ISO-8859-1"): 1251 or 866, the one with more Russian letters (the user
// 2026-11-26: the stock shell shows them; 866 asked)
static bool Single(const char* in, const wchar_t* want, int cap = 96)
{
    wchar_t s[96];
    TagSingle((const unsigned char*)in, (int)strlen(in), s, cap);
    return !wcscmp(s, want);
}

static bool Decoded(const char* in, int n, int enc, const wchar_t* want)
{
    wchar_t s[96];
    s[0] = L'?';
    TagDecode((const unsigned char*)in, n, enc, s, 96);
    return !wcscmp(s, want);
}

static void TestSingle()
{
    Check(Single("\xC2 \xCF\xF3\xF1\xF2\xEE\xF2\xE5", L"\x0412 \x041F\x0443\x0441\x0442\x043E\x0442\x0435"),
          "single: 1251 (\"В Пустоте\")");
    Check(Single("\x8F\xE0\xA8\xA2\xA5\xE2", L"\x041F\x0440\x0438\x0432\x0435\x0442"), "single: 866 (\"Привет\")");
    Check(Single("\x8F\x85\x91\x8D\x9F", L"\x041F\x0415\x0421\x041D\x042F"), "single: 866 capitals (\"ПЕСНЯ\")");
    Check(Single("\xF0\xA6\xA8\xAA \xF1", L"\x0401\x0436\x0438\x043A \x0451"), "single: 866 Ё ё (\"Ёжик ё\")");
    Check(Single("\xDF", L"\x042F"), "single: one letter (\"Я\" - the old guess wanted a pair)");
    Check(Single("DJ \xC6", L"DJ \x0416"), "single: Latin with one Cyrillic letter (\"DJ Ж\")");
    Check(Single("\xE0", L"\x0430"), "single: as many Russian letters both ways - 1251");
    Check(Single("Song 2", L"Song 2"), "single: ASCII as is");
    Check(Single("", L""), "single: empty");
    Check(Single("\xA8\xEB\xEA\xE0 \x97 \xAB\xB8\xE6\xBB", L"\x0401\x043B\x043A\x0430 \x2014 \x00AB\x0451\x0436\x00BB"),
          "single: 1251's signs (Ё, the dash, the quotes)");
    Check(Single("\xC0\xC1\xC2\xC3", L"\x0410\x0411\x0412", 4), "single: cut to its room");
    Check(Single("\xFF\xC0", L"\x044F\x0410"), "single: 1251 FF (я) and C0 (А)");
    Check(Single("\x80\xAF\xE0\xEF", L"\x0410\x043F\x0440\x044F"), "single: 866's edges А (80) п (AF) р (E0) я (EF)");
}

static void TestDecode()
{
    Check(Decoded("\xC2\xE5\xF1\xED\xE0", 5, 0, L"\x0412\x0435\x0441\x043D\x0430"), "decode 0: one-byte (\"Весна\")");
    Check(Decoded("\xFF\xFE\x2F\x04\x41\x00", 6, 1, L"\x042F" L"A"), "decode 1: UTF-16, little-endian BOM");
    Check(Decoded("\xFE\xFF\x04\x2F\x00\x41", 6, 1, L"\x042F" L"A"), "decode 1: UTF-16, big-endian BOM");
    Check(Decoded("\x04\x2F\x00\x41", 4, 2, L"\x042F" L"A"), "decode 2: UTF-16BE");
    Check(Decoded("\xD0\xAF" "A", 3, 3, L"\x042F" L"A"), "decode 3: UTF-8");
    Check(Decoded("One\0Two", 7, 0, L"One"), "the first of several values (v2.4: zero between)");
    Check(Decoded("\xFF\xFE\x41\x00\x00\x00\x42\x00", 8, 1, L"A"), "UTF-16: the first value");
    Check(Decoded("\xFF\xFE\x41", 3, 1, L""), "UTF-16: an odd byte left - nothing of it");
    Check(Decoded("x", 1, 7, L""), "an unknown encoding: nothing");
}

static void TestV1()
{
    unsigned char t[128];
    memset(t, 0, sizeof(t));
    memcpy(t, "TAG", 3);
    memcpy(t + 3, "\xC2 \xCF\xF3\xF1\xF2\xEE\xF2\xE5   ", 12);   // (padded with spaces by some writers)
    memcpy(t + 33, "Artist", 6);
    memcpy(t + 63, "\x80\xAB\xEC", 3);   // 866 "Альм"
    memcpy(t + 93, "2004", 4);
    TagTexts x;
    memset(&x, 0, sizeof(x));
    wcscpy(x.artist, L"from v2");
    Check(TagV1(t, &x), "v1: read");
    Check(!wcscmp(x.title, L"\x0412 \x041F\x0443\x0441\x0442\x043E\x0442\x0435"), "v1: the title, its spaces cut");
    Check(!wcscmp(x.artist, L"from v2"), "v1: a field v2 gave stays");
    Check(!wcscmp(x.album, L"\x0410\x043B\x044C"), "v1: the album (866)");
    Check(!wcscmp(x.year, L"2004"), "v1: the year");
    t[0] = 'X';
    memset(&x, 0, sizeof(x));
    Check(!TagV1(t, &x) && !x.title[0], "v1: no \"TAG\" - nothing");
    memcpy(t, "TAG", 3);
    memset(t + 3, 0, 125);
    t[3 + 29] = 'Z';   // (a field filling all its 30 bytes)
    memset(&x, 0, sizeof(x));
    TagV1(t, &x);
    Check(!wcscmp(x.title, L""), "v1: a field from its zero - empty (the rest after a zero is not text)");
    memset(t + 3, 'a', 30);
    memcpy(t + 33, "Q", 1);
    memcpy(t + 63, "Al", 2);
    memcpy(t + 93, "1999", 4);
    memset(t + 97, 'c', 30);   // the comment
    memset(&x, 0, sizeof(x));
    wcscpy(x.album, L"v");   // (one letter given by v2)
    TagV1(t, &x);
    Check(wcslen(x.title) == 30 && !wcscmp(x.artist, L"Q"), "v1: a whole 30-byte field, the next one's not in it");
    Check(!wcscmp(x.album, L"v"), "v1: a one-letter field from v2 stays");
    Check(!wcscmp(x.year, L"1999"), "v1: the year's 4 bytes, the comment after it not");
    memset(t + 3, ' ', 30);
    memset(t + 33, 'b', 30);
    memset(t + 63, 'c', 30);
    memset(&x, 0, sizeof(x));
    TagV1(t, &x);
    Check(!x.title[0], "v1: a field of spaces - empty");
    Check(wcslen(x.artist) == 30 && wcslen(x.album) == 30 && x.album[29] == L'c',
          "v1: the artist and the album whole, each to its 30 bytes");
}

static bool Cut(const char* in, int n, int enc, int cap, const wchar_t* want)   // n bytes of in, out's room cap
{
    wchar_t s[17];
    for (int i = 0; i < 17; i++)
        s[i] = L'#';
    TagDecode((const unsigned char*)in, n, enc, s, cap);
    return !wcscmp(s, want) && s[cap] == L'#';
}

static void TestEdges()
{
    Check(Tag866(0xAF) == 0x043F && Tag866(0xB0) == L'?' && Tag866(0xDF) == L'?' && Tag866(0xE0) == 0x0440 &&
          Tag866(0xEF) == 0x044F && Tag866(0xF2) == L'?' && Tag866(0x7F) == 0x7F, "866: п | box ... box | р..я | ?");
    Check(TagRussian(0x0410) && TagRussian(0x044F) && TagRussian(0x0401) && TagRussian(0x0451) && !TagRussian(0x040F) &&
          !TagRussian(0x0450) && !TagRussian(0x0400) && !TagRussian(0x0452), "Russian letters: А..я, Ё, ё only");
    Check(TagNot1251(0x80) && TagNot1251(0x8F) && !TagNot1251(0x7F) && !TagNot1251(0xA8) && !TagNot1251(0x97),
          "1251's impossible: Ђ (80), Џ (8F); not ASCII, Ё, the dash");
    Check(TagScore(true, 0xB0) == -2 && TagScore(true, 0xDF) == -2 && TagScore(true, 0xAF) == 1 &&
          TagScore(true, 0xE0) == 1 && TagScore(true, 0xF2) == 0 && TagScore(true, 'a') == 0, "866's score");
    Check(TagScore(false, 0x80) == -2 && TagScore(false, 0xC0) == 1 && TagScore(false, 0x97) == 0, "1251's score");
    Check(Single("\x8F", L"\x041F"), "the first byte counted (866 П, not 1251 Џ)");
    Check(Single("\xA6\xE0", L"\x0436\x0440"), "866 better by one letter: 866 (\"жр\")");
    Check(Decoded("\xC0\x81" "\xE0\xA0\x80" "\xE2\x82\xAC" "B", 9, 3, L"\x0001\x0800\x20AC" L"B"),
          "UTF-8: the leads C0 and E0 at their edges; a 3-byte one before more text");
    Check(Decoded("\xEF\xBF\xBF", 3, 3, L"\xFFFF"), "UTF-8: U+FFFF is the BMP's");
    Check(Cut("abc", 2, 0, 16, L"ab"), "one-byte: n bytes only");
    Check(Cut("\xE0\x8F", 1, 0, 16, L"\x0430"), "one-byte: a byte past n neither shown nor counted");
    Check(Cut("x", 0, 0, 16, L""), "one-byte: n 0 - empty");
    Check(Cut("\xFF\xFE\x41\x00\x42\x00\x43\x00", 8, 1, 3, L"AB"), "UTF-16: cut to its room");
    Check(Cut("\x41\x00\x42\x00", 3, 1, 16, L"A"), "UTF-16: n bytes only");
    Check(Decoded("\x41\x00", 2, 1, L"A"), "UTF-16 without a BOM: little-endian");
    Check(Decoded("\xFF\x00", 2, 1, L"\x00FF"), "UTF-16: FF without FE is a letter");
    Check(Decoded("\xFE\x00", 2, 1, L"\x00FE"), "UTF-16: FE without FF is a letter");
    Check(Decoded("\xFF\xFE", 2, 1, L""), "UTF-16: the BOM alone");
    Check(Decoded("\xFE\xFF\x00\x41", 4, 2, L"\xFEFF" L"A"), "UTF-16BE: no BOM looked for");
    Check(Decoded("A\xD0\xAF\xE2\x82\xAC", 6, 3, L"A\x042F\x20AC"), "UTF-8: 1, 2 and 3 bytes");
    Check(Decoded("\xF0\x9F\x98\x80" "A", 5, 3, L"?A"), "UTF-8: past the BMP - ?");
    Check(Decoded("A\0B", 3, 3, L"A"), "UTF-8: the first value");
    Check(Decoded("A\xD0", 2, 3, L"A\x0010"), "UTF-8: a letter cut at the end - what is there");
    Check(Cut("ABCD", 4, 3, 3, L"AB"), "UTF-8: cut to its room");
    Check(Cut("ABCD", 2, 3, 16, L"AB"), "UTF-8: n bytes only");
    Check(Cut("\xD0\xAF", 1, 3, 16, L"\x0010"), "UTF-8: no byte read past n");
}

int main()
{
    TestEdges();
    TestSingle();
    TestDecode();
    TestV1();
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
