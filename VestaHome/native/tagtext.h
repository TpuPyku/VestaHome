// tagtext.h - a music file's tag text. One-byte text (ID3v1, ID3v2 frames "ISO-8859-1") written by Russian programs
// is Windows-1251, sometimes DOS 866 (the HU 2026-11-23: "В Пустоте" shown as "Â Ïóñòîòå"; the user 2026-11-26: the
// stock shell shows them right, 866 asked): read both ways, the one with more Russian letters and fewer impossible
// signs taken (TagScore; as good: 1251).
// Read by our own ID3 parsing (cover.cpp Id3Texts, TagV1) the encoding byte is known; what the tags library read
// (other formats) is still guessed by TagText1251.
#pragma once
#include <windows.h>
#include <string.h>

struct TagTexts {   // a track's texts, empty: none
    wchar_t title[96], artist[96], album[96], year[8];
};
#define TAG_CAP(a) ((int)(sizeof(a) / sizeof((a)[0])))

inline wchar_t Tag1251(unsigned char b)
{
    static const unsigned short hi[64] = {   // 1251's 80..BF
        0x0402, 0x0403, 0x201A, 0x0453, 0x201E, 0x2026, 0x2020, 0x2021, 0x20AC, 0x2030, 0x0409, 0x2039, 0x040A,
        0x040C, 0x040B, 0x040F, 0x0452, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x0098, 0x2122,
        0x0459, 0x203A, 0x045A, 0x045C, 0x045B, 0x045F, 0x00A0, 0x040E, 0x045E, 0x0408, 0x00A4, 0x0490, 0x00A6,
        0x00A7, 0x0401, 0x00A9, 0x0404, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x0407, 0x00B0, 0x00B1, 0x0406, 0x0456,
        0x0491, 0x00B5, 0x00B6, 0x00B7, 0x0451, 0x2116, 0x0454, 0x00BB, 0x0458, 0x0405, 0x0455, 0x0457 };
    return b < 0x80 ? b : b < 0xC0 ? (wchar_t)hi[b - 0x80] : (wchar_t)(0x0410 + b - 0xC0);
}

inline wchar_t Tag866(unsigned char b)   // its letters; the box drawing and the rest: '?'
{
    if (b < 0x80)
        return b;
    if (b < 0xB0)
        return (wchar_t)(0x0410 + b - 0x80);   // А..п
    if (b >= 0xE0 && b < 0xF0)
        return (wchar_t)(0x0440 + b - 0xE0);   // р..я
    return b == 0xF0 ? 0x0401 : b == 0xF1 ? 0x0451 : L'?';
}

inline bool TagRussian(wchar_t c)
{
    return (c >= 0x0410 && c <= 0x044F) || c == 0x0401 || c == 0x0451;
}

// What the other reading's text never has: 866 read as 1251 gives Serbian letters (Ђ Џ Ќ Љ..., 866's А..Я and
// а..п); 1251 read as 866 gives box drawing (B0..DF).
inline bool TagNot1251(unsigned char b)
{
    static const wchar_t serb[] = L"\x0402\x0403\x0453\x0409\x040A\x040C\x040B\x040F\x0452\x0459\x045A\x045C\x045B"
                                  L"\x045F\x0408\x0458\x0405\x0455";
    return b >= 0x80 && wcschr(serb, Tag1251(b)) != NULL;
}

inline int TagScore(bool is866, unsigned char b)   // a Russian letter +1, an impossible sign -2
{
    if (is866)
        return TagRussian(Tag866(b)) ? 1 : b >= 0xB0 && b < 0xE0 ? -2 : 0;
    return TagRussian(Tag1251(b)) ? 1 : TagNot1251(b) ? -2 : 0;
}

// One-byte text (to its first zero or n) into out (cap with its zero): 1251 or 866, see above.
inline void TagSingle(const unsigned char* b, int n, wchar_t* out, int cap)
{
    int len = 0, r1 = 0, r8 = 0;
    while (len < n && b[len])
        len++;
    for (int i = 0; i < len; i++) {
        r1 += TagScore(false, b[i]);
        r8 += TagScore(true, b[i]);
    }
    int k = 0;
    for (int i = 0; i < len && k < cap - 1; i++)
        out[k++] = r8 > r1 ? Tag866(b[i]) : Tag1251(b[i]);
    out[k] = 0;
}

inline void TagUtf16(const unsigned char* b, int n, bool be, wchar_t* out, int cap)   // to its first zero
{
    int k = 0;
    for (int i = 0; i + 1 < n && k < cap - 1; i += 2) {
        wchar_t c = be ? (wchar_t)(b[i] << 8 | b[i + 1]) : (wchar_t)(b[i] | b[i + 1] << 8);
        if (!c)
            break;
        out[k++] = c;
    }
    out[k] = 0;
}

// UTF-8 to its first zero, cut to cap; past the BMP: '?'
inline void TagUtf8(const unsigned char* b, int n, wchar_t* out, int cap)
{
    int k = 0;
    for (int i = 0; i < n && b[i] && k < cap - 1;) {
        int c = b[i++], more = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
        c &= more ? 0x3F >> more : 0x7F;
        for (; more > 0 && i < n; more--)
            c = c << 6 | (b[i++] & 0x3F);
        out[k++] = c > 0xFFFF ? L'?' : (wchar_t)c;
    }
    out[k] = 0;
}

// An ID3v2 text frame's text (after its encoding byte enc): 0 one-byte, 1 UTF-16 with its BOM, 2 UTF-16BE, 3 UTF-8;
// its first value only (v2.4 puts a zero between values). Another encoding: empty.
inline void TagDecode(const unsigned char* b, int n, int enc, wchar_t* out, int cap)
{
    bool bom = enc == 1 && n >= 2 && ((b[0] == 0xFF && b[1] == 0xFE) || (b[0] == 0xFE && b[1] == 0xFF));
    out[0] = 0;
    if (enc == 0)
        TagSingle(b, n, out, cap);
    else if (enc == 1 || enc == 2)
        TagUtf16(b + (bom ? 2 : 0), n - (bom ? 2 : 0), enc == 2 || (bom && b[0] == 0xFE), out, cap);
    else if (enc == 3)
        TagUtf8(b, n, out, cap);
}

inline void TagV1Field(const unsigned char* b, int n, wchar_t* out, int cap)   // an empty one filled, its spaces cut
{
    if (out[0])
        return;
    TagSingle(b, n, out, cap);
    int k = (int)wcslen(out);
    while (k && out[k - 1] == L' ')
        out[--k] = 0;
}

// ID3v1 (the file's last 128 bytes): "TAG", title, artist, album 30 bytes each, the year 4; only the fields still
// empty filled (ID3v2's first). false: not a v1 tag.
inline bool TagV1(const unsigned char* last, TagTexts* t)
{
    if (memcmp(last, "TAG", 3))
        return false;
    TagV1Field(last + 3, 30, t->title, TAG_CAP(t->title));
    TagV1Field(last + 33, 30, t->artist, TAG_CAP(t->artist));
    TagV1Field(last + 63, 30, t->album, TAG_CAP(t->album));
    TagV1Field(last + 93, 4, t->year, TAG_CAP(t->year));
    return true;
}

// ---- what the tags library read (Vorbis, APE, MP4...): 1251 read as Latin-1 guessed - nothing above U+00FF and two
// Cyrillic letters' codes side by side (C0..FF, A8 Ё, B8 ё): Russian words have them, Latin-1 names rarely do
// ("Café", "Mötley Crüe": single ones).

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
    if (!TagLooks1251(s))
        return;
    for (int i = 0; s[i]; i++)
        if (s[i] >= 0x80)
            s[i] = Tag1251((unsigned char)s[i]);
}
