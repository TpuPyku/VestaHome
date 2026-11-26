// test_ttfname.cpp - the font's family name read from its .ttf (ttfname.h), on the PC: the real Golos Text files
// and name tables made here.
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "ttfname.h"

static int s_failed, s_checks;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAIL: %s\n", what);
    }
}

static unsigned char s_buf[400000];

static void Put16(unsigned char* p, int v);

static int ReadFont(const char* name)
{
    char path[260];
    _snprintf(path, sizeof(path), "..\\VestaHome\\Fonts\\%s", name);
    FILE* f = fopen(path, "rb");
    if (!f)
        return 0;
    int n = (int)fread(s_buf, 1, sizeof(s_buf), f);
    fclose(f);
    return n;
}

static void RealFiles()   // as VestaHome reads them: the directory first, then the name table alone
{
    const char* files[2] = { "GolosText-Regular.ttf", "GolosText-Bold.ttf" };
    for (int i = 0; i < 2; i++) {
        int n = ReadFont(files[i]);
        unsigned off = 0, len = 0;
        wchar_t fam[64];
        bool ok = n > 0 && TtfTable(s_buf, n < 1024 ? n : 1024, "name", &off, &len) && off + len <= (unsigned)n
                  && TtfFamily(s_buf + off, (int)len, fam, 64);
        Check(ok && !wcscmp(fam, L"Golos Text"), files[i]);
    }
    int n = ReadFont("GolosText-Regular.ttf");
    unsigned off, len;
    Check(!TtfTable(s_buf, n, "nope", &off, &len), "a table not there: none");
    int count = (int)TtfU16(s_buf + 4), at = -1;
    for (int i = 0; i < count; i++)
        if (!memcmp(s_buf + 12 + 16 * i, "name", 4))
            at = i;
    Check(at > 0 && TtfTable(s_buf, 12 + 16 * count, "name", &off, &len), "the directory exactly: found");
    char first[5] = { 0 };
    memcpy(first, s_buf + 12, 4);
    Check(TtfTable(s_buf, 12 + 16 * count, first, &off, &len) && off == TtfU32(s_buf + 20), "the first table");
    Put16(s_buf + 4, at);
    Check(!TtfTable(s_buf, n, "name", &off, &len), "tables past the count: not read");
    Put16(s_buf + 4, count);
    Check(!TtfTable(s_buf, 20, "name", &off, &len), "the directory cut short: none");
    memcpy(s_buf, "OTTO", 4);
    Check(!TtfTable(s_buf, 1024, "name", &off, &len), "OpenType with CFF outlines (.otf): not taken (CE can't)");
    memcpy(s_buf, "true", 4);
    Check(TtfTable(s_buf, 1024, "name", &off, &len), "'true' (old Apple TrueType): taken");
    Check(!TtfTable(s_buf, 11, "name", &off, &len), "shorter than the header: none");
}

// A name table: records {platform, encoding, language, nameID, text}; Windows ones UTF-16BE, Mac ones bytes.
struct Rec {
    int         plat, enc, lang, id;
    const char* text;
};

static unsigned char s_t[2000];

static void Put16(unsigned char* p, int v)
{
    p[0] = (unsigned char)(v >> 8);
    p[1] = (unsigned char)v;
}

static int MakeName(const Rec* r, int n)
{
    memset(s_t, 0, sizeof(s_t));
    int store = 6 + 12 * n, at = 0;
    Put16(s_t + 2, n);
    Put16(s_t + 4, store);
    for (int i = 0; i < n; i++) {
        unsigned char* e = s_t + 6 + 12 * i;
        int len = (int)strlen(r[i].text), bytes = r[i].plat == 3 ? len * 2 : len;
        Put16(e, r[i].plat);
        Put16(e + 2, r[i].enc);
        Put16(e + 4, r[i].lang);
        Put16(e + 6, r[i].id);
        Put16(e + 8, bytes);
        Put16(e + 10, at);
        for (int k = 0; k < len; k++)
            if (r[i].plat == 3)
                Put16(s_t + store + at + k * 2, (unsigned char)r[i].text[k]);
            else
                s_t[store + at + k] = (unsigned char)r[i].text[k];
        at += bytes;
    }
    return store + at;
}

static bool Family(const Rec* r, int n, const wchar_t* want, int cap = 64, int cut = 0)   // want NULL: none
{
    wchar_t fam[64];
    int len = MakeName(r, n);
    bool got = TtfFamily(s_t, len - cut, fam, cap);
    return want ? got && !wcscmp(fam, want) : !got;
}

static void NameTables()
{
    Rec mac[1] = { { 1, 0, 0, 1, "Shrift" } };
    Check(Family(mac, 1, L"Shrift"), "a Mac record alone: its name");
    Rec two[3] = { { 1, 0, 0, 1, "MacName" }, { 3, 1, 0x419, 1, "RuName" }, { 3, 1, 0x409, 1, "EnName" } };
    Check(Family(two, 3, L"EnName"), "Windows US English over Russian and Mac");
    Rec ru[2] = { { 3, 1, 0x419, 1, "RuName" }, { 1, 0, 0, 1, "MacName" } };
    Check(Family(ru, 2, L"RuName"), "any Windows over Mac");
    Rec sym[2] = { { 1, 0, 0, 1, "MacName" }, { 3, 0, 0x409, 1, "Symbol" } };
    Check(Family(sym, 2, L"Symbol"), "Windows symbol encoding: as Unicode");
    Rec other[2] = { { 3, 1, 0x409, 2, "Bold" }, { 3, 1, 0x409, 4, "Full Name" } };
    Check(Family(other, 2, NULL), "no family record: none");
    Rec skip[5] = { { 0, 3, 0, 1, "Unicode" }, { 3, 10, 0x409, 1, "Ucs4" }, { 3, 2, 0x409, 1, "Sjis" },
                    { 1, 1, 0, 1, "MacJp" }, { 2, 0, 0, 1, "Iso" } };
    Check(Family(skip, 5, NULL), "Unicode platform / other encodings: not read");
    Check(Family(mac, 1, L"Shr", 4), "cut to the room (with its zero)");
    Rec empty[1] = { { 3, 1, 0x409, 1, "" } };
    Check(Family(empty, 1, NULL), "an empty name: none");
    Rec one[1] = { { 3, 1, 0x409, 1, "A" } };
    Check(Family(one, 1, L"A"), "a one-letter name");
    Rec macRu[2] = { { 1, 0, 0, 1, "MacName" }, { 3, 1, 0x419, 1, "RuName" } };
    Check(Family(macRu, 2, L"RuName"), "Windows over Mac, Mac first");
    Rec same[2] = { { 3, 1, 0x409, 1, "First" }, { 3, 1, 0x409, 1, "Second" } };
    Check(Family(same, 2, L"First"), "two as good: the first");
    Rec cut[2] = { { 1, 0, 0, 1, "MacName" }, { 3, 1, 0x409, 1, "EnName" } };
    Check(Family(cut, 2, L"MacName", 64, 1), "the best one past the end: the next best");
    Rec b80[1] = { { 1, 0, 0, 1, "A\x80" } };
    Check(Family(b80, 1, L"A?"), "Mac byte 0x80: '?'");
    Rec hi[1] = { { 1, 0, 0, 1, "Caf\xE9" } };
    Check(Family(hi, 1, L"Caf?"), "Mac bytes past ASCII: '?'");
    wchar_t fam[8];
    int len = MakeName(mac, 1);
    Check(!TtfFamily(s_t, len - 1, fam, 8), "the text past the table's end: none");
    Check(!TtfFamily(s_t, 5, fam, 8), "shorter than the header: none");
    Put16(s_t + 2, 50);
    Check(!TtfFamily(s_t, len, fam, 8), "more records than the table holds: none");
    len = MakeName(two, 3);
    Put16(s_t + 2, 2);
    Check(TtfFamily(s_t, len, fam, 8) && !wcscmp(fam, L"RuName"), "records past the count: not read");
}

static void FaceChoice()   // which face VestaHome takes and what it says (user_skin.ini face vs the files' names)
{
    wchar_t f[48];
    Check(TtfFace(L"Shrift", L"Shrift Sans", L"Shrift Sans", f, 48) == TTF_FACE_DIFFERS && !wcscmp(f, L"Shrift Sans"),
          "face not the file's (the user's case): the file's, said");
    Check(TtfFace(L"Roboto", L"Roboto", L"Roboto", f, 48) == 0 && !wcscmp(f, L"Roboto"), "the same: nothing said");
    Check(TtfFace(L"", L"Roboto", L"Roboto", f, 48) == 0 && !wcscmp(f, L"Roboto"), "no face: the file's, quietly");
    Check(TtfFace(L"Roboto", L"", L"", f, 48) == 0 && !wcscmp(f, L"Roboto"), "no name in the file: face");
    Check(TtfFace(L"", L"", L"", f, 48) == 0 && !f[0], "neither: none");
    Check(TtfFace(L"", L"Roboto", L"Roboto Black", f, 48) == TTF_BOLD_OTHER, "a bold of another family: said");
    Check(TtfFace(L"X", L"Roboto", L"Roboto Black", f, 48) == (TTF_FACE_DIFFERS | TTF_BOLD_OTHER), "both said");
    Check(TtfFace(L"", L"Roboto", L"", f, 48) == 0, "no bold name: nothing said");
    Check(TtfFace(L"Z", L"A", L"B", f, 48) == (TTF_FACE_DIFFERS | TTF_BOLD_OTHER) && !wcscmp(f, L"A"),
          "one-letter names");
    Check(TtfFace(L"", L"Roboto Condensed", L"", f, 8) == 0 && !wcscmp(f, L"Roboto "), "cut to the room");
}

int main()
{
    RealFiles();
    NameTables();
    FaceChoice();
    printf("ttfname: %d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
