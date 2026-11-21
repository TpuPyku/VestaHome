// skinline.h: a line of skin.ini / user_skin.ini - its section, a colour "name = RRGGBB", a file "name = x.png",
// a comment or nothing.
#include <windows.h>
#include "skinline.h"
#include <stdio.h>

static int s_checks, s_failed;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAIL: %s\n", what);
    }
}

static int Line(const wchar_t* text, int* section, wchar_t* key, wchar_t* val, COLORREF* c)
{
    wchar_t line[200];
    wcscpy(line, text);
    return SkinLine(line, section, key, 32, val, 100, c);
}

int main()
{
    int sec = SKS_COLORS;
    wchar_t key[32], val[100];
    COLORREF c = 0;
    Check(Line(L"accent  = FF6B1A   ; pressed button", &sec, key, val, &c) == SKL_COLOR && !wcscmp(key, L"accent") &&
          c == RGB(0xFF, 0x6B, 0x1A), "a colour with its comment");
    Check(Line(L"panel=#1a1d22", &sec, key, val, &c) == SKL_COLOR && c == RGB(0x1A, 0x1D, 0x22), "'#', small letters, no spaces");
    Check(Line(L"ink = 12345", &sec, key, val, &c) == SKL_NONE, "five digits: not a colour");
    Check(Line(L"ink = 1234567", &sec, key, val, &c) == SKL_NONE, "seven: not either");
    Check(Line(L"ink = 12G456", &sec, key, val, &c) == SKL_NONE, "not hex");
    Check(Line(L"ink =", &sec, key, val, &c) == SKL_NONE, "no value");
    Check(Line(L"; just a comment", &sec, key, val, &c) == SKL_NONE && sec == SKS_COLORS, "a comment: nothing");
    Check(Line(L"", &sec, key, val, &c) == SKL_NONE, "empty");
    Check(Line(L"[Images]", &sec, key, val, &c) == SKL_SECTION && sec == SKS_IMAGES, "[images], any case");
    Check(Line(L"radio = radio.png ; the tile", &sec, key, val, &c) == SKL_FILE && !wcscmp(key, L"radio") &&
          !wcscmp(val, L"radio.png"), "a picture in [images]");
    Check(Line(L"accent = FF6B1A", &sec, key, val, &c) == SKL_FILE, "in [images] a hex is a file name");
    Check(Line(L"  [fonts]  ", &sec, key, val, &c) == SKL_SECTION && sec == SKS_FONTS, "[fonts]");
    Check(Line(L"face = Roboto Condensed", &sec, key, val, &c) == SKL_FILE && !wcscmp(val, L"Roboto Condensed"),
          "a font's name with a space");
    Check(Line(L"[colors]", &sec, key, val, &c) == SKL_SECTION && sec == SKS_COLORS, "back to [colors]");
    Check(Line(L"[other]", &sec, key, val, &c) == SKL_SECTION && sec == SKS_OTHER, "a section not known");
    Check(Line(L"ink = 000000", &sec, key, val, &c) == SKL_NONE, "... its lines: nothing");
    sec = SKS_COLORS;
    Check(Line(L"ink=112233", &sec, key, val, &c) == SKL_COLOR && c == RGB(0x11, 0x22, 0x33), "no spaces, no '#'");
    Check(Line(L"a = 112233\t\r", &sec, key, val, &c) == SKL_COLOR && !wcscmp(key, L"a"), "a one-letter name; a tab and "
          "CR at the end");
    Check(Line(L"ink = 112233 \t ", &sec, key, val, &c) == SKL_COLOR, "spaces and a tab at the end");
    Check(Line(L"abcdefghijklmnopqrstuvwxyz01234 = 112233", &sec, key, val, &c) == SKL_COLOR, "a name of 31: fits 32");
    Check(Line(L"abcdefghijklmnopqrstuvwxyz012345 = 112233", &sec, key, val, &c) == SKL_NONE, "of 32: not");
    Check(Line(L"noequals", &sec, key, val, &c) == SKL_NONE, "no '='");
    Check(Line(L"= 000000", &sec, key, val, &c) == SKL_NONE, "no name");
    Check(Line(L"verylongnamethatdoesnotfitinthekeybuffer = 000000", &sec, key, val, &c) == SKL_NONE,
          "a name longer than the room: nothing");
    sec = SKS_IMAGES;
    wchar_t lng[200];
    for (int i = 0; i < 150; i++)
        lng[i] = L'x';
    wcscpy(lng + 150, L"");
    wchar_t ln[200];
    _snwprintf(ln, 200, L"tile = %s", lng);
    Check(Line(ln, &sec, key, val, &c) == SKL_NONE, "a file name longer than the room: nothing");
    lng[99] = 0;
    _snwprintf(ln, 200, L"tile = %s", lng);
    Check(Line(ln, &sec, key, val, &c) == SKL_FILE, "of 99: fits 100");
    lng[99] = L'x';
    lng[100] = 0;
    _snwprintf(ln, 200, L"tile = %s", lng);
    Check(Line(ln, &sec, key, val, &c) == SKL_NONE, "of 100: not");
    printf("test_skinline: %d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
