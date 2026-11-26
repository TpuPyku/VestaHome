// ttfname.h - a TrueType font's family name from its own file (user_skin.ini [fonts], the user 2026-11-26: "face"
// written as the file's name - GDI took another font silently). Pure: byte buffers in, tested by test_ttfname.bat.
#pragma once
#include <string.h>
#include <wchar.h>

static inline unsigned TtfU16(const unsigned char* p)
{
    return (unsigned)p[0] << 8 | p[1];
}

static inline unsigned TtfU32(const unsigned char* p)
{
    return TtfU16(p) << 16 | TtfU16(p + 2);
}

// Where the table tag is (offset, length in the file): dir = the file's first n bytes. False - not TrueType
// outlines (an .otf's "OTTO": CE has no CFF), the directory past n, or no such table.
static inline bool TtfTable(const unsigned char* dir, int n, const char* tag, unsigned* off, unsigned* len)
{
    if (n < 12)
        return false;
    unsigned ver = TtfU32(dir);
    if (ver != 0x00010000 && memcmp(dir, "true", 4))
        return false;
    int count = (int)TtfU16(dir + 4);
    if (12 + 16 * count > n)
        return false;
    for (int i = 0; i < count; i++) {
        const unsigned char* e = dir + 12 + 16 * i;
        if (!memcmp(e, tag, 4)) {
            *off = TtfU32(e + 8);
            *len = TtfU32(e + 12);
            return true;
        }
    }
    return false;
}

// How good a record is for the family name (0 - not one): Windows US English, any Windows (Unicode or symbol),
// Mac Roman - the order GDI itself would name the face in.
static inline int TtfNameRank(const unsigned char* r)
{
    unsigned plat = TtfU16(r), enc = TtfU16(r + 2), lang = TtfU16(r + 4);
    if (TtfU16(r + 6) != 1)
        return 0;
    if (plat == 3 && enc <= 1)
        return lang == 0x409 ? 3 : 2;
    return plat == 1 && enc == 0 ? 1 : 0;
}

// The family name (name ID 1) from the name table t (n bytes) into out (cap with its zero). False - none.
static inline bool TtfFamily(const unsigned char* t, int n, wchar_t* out, int cap)
{
    if (n < 6)
        return false;
    int count = (int)TtfU16(t + 2), store = (int)TtfU16(t + 4), best = -1, rank = 0;
    if (6 + 12 * count > n)
        return false;
    for (int i = 0; i < count; i++) {
        const unsigned char* r = t + 6 + 12 * i;
        int k = TtfNameRank(r);
        if (k > rank && store + (int)TtfU16(r + 10) + (int)TtfU16(r + 8) <= n) {
            rank = k;
            best = i;
        }
    }
    if (best < 0)
        return false;
    const unsigned char* r = t + 6 + 12 * best;
    const unsigned char* s = t + store + TtfU16(r + 10);
    int bytes = (int)TtfU16(r + 8), wide = TtfU16(r) == 3, len = wide ? bytes / 2 : bytes, k = 0;
    for (; k < len && k < cap - 1; k++)
        out[k] = wide ? (wchar_t)TtfU16(s + k * 2) : s[k] < 0x80 ? (wchar_t)s[k] : L'?';
    out[k] = 0;
    return k > 0;
}

// The face to take (into out, cap with its zero; empty - none): the regular file's family, face only when the file
// names none. What to say: face given and not the file's; a bold named another family (GDI then makes the bold
// from the regular). Names not read: "".
enum { TTF_FACE_DIFFERS = 1, TTF_BOLD_OTHER = 2 };

static inline int TtfFace(const wchar_t* face, const wchar_t* regular, const wchar_t* bold, wchar_t* out, int cap)
{
    const wchar_t* take = regular[0] ? regular : face;
    wcsncpy(out, take, cap - 1);
    out[cap - 1] = 0;
    int say = face[0] && regular[0] && wcscmp(face, regular) ? TTF_FACE_DIFFERS : 0;
    return say | (bold[0] && wcscmp(bold, take) ? TTF_BOLD_OTHER : 0);
}
