// test_cover.cpp - the track's cover (cover.cpp) on the PC (test_cover.bat): the picture inside an mp3's ID3v2 tag
// and the folder's picture that stands for its album.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "cover.h"

static int s_failed, s_checks;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAIL: %s\n", what);
    }
}

// ---- building tags

static unsigned char s_tag[4096];
static int s_n;

static void Put(const void* p, int n)
{
    memcpy(s_tag + s_n, p, n);
    s_n += n;
}

static void Put8(int v) { s_tag[s_n++] = (unsigned char)v; }

static void PutSize(int v, bool safe)   // 4 bytes, syncsafe (7 bits a byte) or plain
{
    int sh = safe ? 7 : 8;
    for (int i = 3; i >= 0; i--)
        Put8(v >> (sh * i) & (safe ? 0x7F : 0xFF));
}

static void Head(int major, int flags)   // the tag's size patched by End
{
    s_n = 0;
    Put("ID3", 3);
    Put8(major);
    Put8(0);
    Put8(flags);
    PutSize(0, true);
}

static void End()
{
    int body = s_n - 10;
    for (int i = 0; i < 4; i++)
        s_tag[6 + i] = (unsigned char)(body >> (7 * (3 - i)) & 0x7F);
}

static const unsigned char kJpg[] = { 0xFF, 0xD8, 0xFF, 0xE0, 1, 2, 3, 4 };
static const unsigned char kPng[] = { 0x89, 'P', 'N', 'G', 13, 10, 26, 10, 9 };

// A frame v2.3 (plain size) or v2.4 (syncsafe): id, size, flags, then the body given.
static void Frame(const char* id, const void* body, int n, bool safe, int flags = 0)
{
    Put(id, 4);
    PutSize(n, safe);
    Put8(flags >> 8);
    Put8(flags & 255);
    Put(body, n);
}

// An APIC body: encoding, MIME, type, description (latin-1 or UTF-16 with its BOM), the picture.
static int Apic(unsigned char* b, int enc, int type, const char* desc, const unsigned char* pic, int pn)
{
    int k = 0;
    b[k++] = (unsigned char)enc;
    memcpy(b + k, "image/jpeg", 11);
    k += 11;
    b[k++] = (unsigned char)type;
    if (enc == 1) {
        b[k++] = 0xFF;
        b[k++] = 0xFE;
        for (const char* s = desc; *s; s++) {
            b[k++] = (unsigned char)*s;
            b[k++] = 0;
        }
        b[k++] = 0;
        b[k++] = 0;
    } else {
        int l = (int)strlen(desc) + 1;
        memcpy(b + k, desc, l);
        k += l;
    }
    memcpy(b + k, pic, pn);
    return k + pn;
}

static bool Found(const unsigned char* pic, int pn)
{
    int off = -1, len = -1;
    return Id3Picture(s_tag, s_n, &off, &len) && len == pn && !memcmp(s_tag + off, pic, pn);
}

static void TestTags()
{
    unsigned char b[256];
    // v2.3: a text frame first, then the picture
    Head(3, 0);
    Frame("TIT2", "\0Song", 5, false);
    Frame("APIC", b, Apic(b, 0, 3, "front", kJpg, sizeof(kJpg)), false);
    End();
    Check(Id3TagSize(s_tag, 10) == s_n, "the tag's size from its header");
    Check(Found(kJpg, sizeof(kJpg)), "v2.3: the APIC after a text frame");

    // v2.4: syncsafe frame sizes; a picture of 200 bytes (its size's low byte has the high bit)
    unsigned char big[200];
    memcpy(big, kPng, sizeof(kPng));
    memset(big + sizeof(kPng), 0xAB, sizeof(big) - sizeof(kPng));
    Head(4, 0);
    Frame("APIC", b, Apic(b, 3, 3, "", big, sizeof(big)), true);
    End();
    Check(Found(big, sizeof(big)), "v2.4: syncsafe sizes, a PNG");

    // the front cover preferred to the first picture
    Head(3, 0);
    Frame("APIC", b, Apic(b, 0, 4, "back", kPng, sizeof(kPng)), false);
    Frame("APIC", b, Apic(b, 0, 3, "front", kJpg, sizeof(kJpg)), false);
    End();
    Check(Found(kJpg, sizeof(kJpg)), "two pictures: the front cover (type 3)");
    Head(3, 0);
    Frame("APIC", b, Apic(b, 0, 4, "back", kPng, sizeof(kPng)), false);
    End();
    Check(Found(kPng, sizeof(kPng)), "no front cover: the one there is");

    // a UTF-16 description: its two-byte end, not the first zero byte
    Head(3, 0);
    Frame("APIC", b, Apic(b, 1, 3, "ab", kJpg, sizeof(kJpg)), false);
    End();
    Check(Found(kJpg, sizeof(kJpg)), "a UTF-16 description");

    // v2.2: PIC with a 3-letter format, 3-byte ids and sizes
    Head(2, 0);
    unsigned char p[64];
    int k = 0;
    p[k++] = 0;
    memcpy(p + k, "JPG", 3);
    k += 3;
    p[k++] = 3;
    p[k++] = 0;
    memcpy(p + k, kJpg, sizeof(kJpg));
    k += sizeof(kJpg);
    Put("PIC", 3);
    Put8(0);
    Put8(0);
    Put8(k);
    Put(p, k);
    End();
    Check(Found(kJpg, sizeof(kJpg)), "v2.2: PIC");

    // padding after the frames: the end
    Head(3, 0);
    Frame("TIT2", "\0Song", 5, false);
    memset(s_tag + s_n, 0, 64);
    s_n += 64;
    End();
    int off, len;
    Check(!Id3Picture(s_tag, s_n, &off, &len), "no picture, padding: none");

    // the data not a picture (a URL, "-->"): skipped
    Head(3, 0);
    static const unsigned char kUrl[] = { 'h', 't', 't', 'p', ':', '/', '/' };
    Frame("APIC", b, Apic(b, 0, 3, "", kUrl, sizeof(kUrl)), false);
    End();
    Check(!Id3Picture(s_tag, s_n, &off, &len), "a link instead of a picture: none");

    // a frame claiming more than the tag holds: no crash, none
    Head(3, 0);
    Put("APIC", 4);
    PutSize(5000, false);
    Put8(0);
    Put8(0);
    Put(b, Apic(b, 0, 3, "", kJpg, sizeof(kJpg)));
    End();
    Check(!Id3Picture(s_tag, s_n, &off, &len), "a frame bigger than the tag: none");

    // v2.3 extended header skipped
    Head(3, 0x40);
    PutSize(6, false);
    Put("\0\0\0\0\0\0", 6);
    Frame("APIC", b, Apic(b, 0, 3, "", kJpg, sizeof(kJpg)), false);
    End();
    Check(Found(kJpg, sizeof(kJpg)), "v2.3: the extended header skipped");

    // v2.4: the extended header's size counts itself
    Head(4, 0x40);
    PutSize(6, true);
    Put8(1);
    Put8(0);
    Frame("APIC", b, Apic(b, 0, 3, "", kJpg, sizeof(kJpg)), true);
    End();
    Check(Found(kJpg, sizeof(kJpg)), "v2.4: the extended header skipped");

    // v2.3 unsynchronisation of the whole tag: FF 00 back to FF (the frame's size: before it was applied)
    Head(3, 0x80);
    unsigned char u[] = { 0xFF, 0x00, 0xD8, 0xFF, 0x00, 0xE0, 7 };
    unsigned char want[] = { 0xFF, 0xD8, 0xFF, 0xE0, 7 };
    unsigned char raw[64];
    int real = Apic(b, 0, 3, "", want, sizeof(want)), rawN = Apic(raw, 0, 3, "", u, sizeof(u));
    Put("APIC", 4);
    PutSize(real, false);
    Put8(0);
    Put8(0);
    Put(raw, rawN);
    End();
    Check(Found(want, sizeof(want)), "v2.3 unsynchronised: the picture restored");

    // v2.4 frame flags: the data length (4 bytes) and the frame unsynchronised
    Head(4, 0);
    unsigned char f4[64];
    int n4 = 0;
    f4[n4++] = 0;
    f4[n4++] = 0;
    f4[n4++] = 0;
    f4[n4++] = 9;   // the data length indicator
    unsigned char raw4[64];
    int r4 = Apic(raw4, 0, 3, "", u, sizeof(u));
    memcpy(f4 + n4, raw4, r4);
    n4 += r4;
    Frame("APIC", f4, n4, true, 0x0003);
    End();
    Check(Found(want, sizeof(want)), "v2.4: the data length skipped, the frame's unsynchronisation undone");

    // v2.4 encrypted, v2.3 grouped
    Head(4, 0);
    Frame("APIC", b, Apic(b, 0, 3, "", kJpg, sizeof(kJpg)), true, 0x0004);
    End();
    Check(!Id3Picture(s_tag, s_n, &off, &len), "v2.4 encrypted: none");
    Head(3, 0);
    unsigned char g3[64];
    g3[0] = 7;   // the group
    Frame("APIC", g3, 1 + Apic(g3 + 1, 0, 3, "", kJpg, sizeof(kJpg)), false, 0x0020);
    End();
    Check(Found(kJpg, sizeof(kJpg)), "v2.3 grouped: the group byte skipped");

    // v2.4 with a UTF-16 description: the encoding byte read where it is
    Head(4, 0);
    Frame("APIC", b, Apic(b, 1, 3, "ab", kJpg, sizeof(kJpg)), true);
    End();
    Check(Found(kJpg, sizeof(kJpg)), "v2.4: a UTF-16 description");

    // two front covers: the first kept
    Head(3, 0);
    Frame("APIC", b, Apic(b, 0, 3, "a", kJpg, sizeof(kJpg)), false);
    Frame("APIC", b, Apic(b, 0, 3, "b", kPng, sizeof(kPng)), false);
    End();
    Check(Found(kJpg, sizeof(kJpg)), "two front covers: the first");

    // cut APIC bodies: only the encoding; a MIME without its end; nothing after the MIME
    static const unsigned char kCut1[] = { 0 };
    static const unsigned char kCut2[] = { 0, 'i', 'm', 'g' };
    static const unsigned char kCut3[] = { 0, 'i', 'm', 'g', 0 };
    const unsigned char* cuts[3] = { kCut1, kCut2, kCut3 };
    int cutN[3] = { 1, 4, 5 };
    for (int c = 0; c < 3; c++) {
        Head(3, 0);
        Frame("APIC", cuts[c], cutN[c], false);
        End();
        Check(!Id3Picture(s_tag, s_n, &off, &len), "a cut APIC: none");
    }
    // v2.2 PIC cut after its format
    Head(2, 0);
    static const unsigned char kPicCut[] = { 0, 'J', 'P', 'G' };
    Put("PIC", 3);
    Put8(0);
    Put8(0);
    Put8(sizeof(kPicCut));
    Put(kPicCut, sizeof(kPicCut));
    End();
    Check(!Id3Picture(s_tag, s_n, &off, &len), "a cut PIC: none");

    // the edges of the tag
    Head(3, 0);
    Frame("APIC", b, Apic(b, 0, 3, "", kJpg, sizeof(kJpg)), false);
    End();
    Check(Found(kJpg, sizeof(kJpg)), "a frame right up to the tag's end: read");
    Check(!Id3Picture(s_tag, s_n - 1, &off, &len), "the buffer shorter than the tag says: none");
    Head(3, 0);
    Frame("TIT2", "\0Song", 5, false);
    Frame("APIC", b, Apic(b, 0, 3, "", kJpg, sizeof(kJpg)), false);
    s_n--;   // the tag one byte shorter than its last frame says
    End();
    Check(!Id3Picture(s_tag, s_n, &off, &len), "a frame one byte past the tag's end: none");
    Head(3, 0);
    Frame("TIT2", "\0Song", 5, false);
    End();
    int tagEnd = s_n;
    Frame("APIC", b, Apic(b, 0, 3, "", kJpg, sizeof(kJpg)), false);   // after the tag's end: not its frame
    Check(!Id3Picture(s_tag, s_n, &off, &len) && tagEnd < s_n, "a frame after the tag's end: not read");
    Head(3, 0);
    Frame("TIT2", "x", 1, false);   // a one-byte frame first
    Frame("APIC", b, Apic(b, 0, 3, "", kJpg, sizeof(kJpg)), false);
    End();
    Check(Found(kJpg, sizeof(kJpg)), "a one-byte frame before the picture: passed over");

    // v2.4: the tag's unsynchronisation flag means each frame says so itself - the picture left as it is
    Head(4, 0x80);
    unsigned char keep[] = { 0xFF, 0xD8, 0xFF, 0x00, 0xE0 };
    Frame("APIC", b, Apic(b, 0, 3, "", keep, sizeof(keep)), true);
    End();
    Check(Found(keep, sizeof(keep)), "v2.4 tag flag without the frame's: FF 00 kept");

    // a compressed (zlib) frame: not read
    Head(3, 0);
    Frame("APIC", b, Apic(b, 0, 3, "", kJpg, sizeof(kJpg)), false, 0x0080);
    End();
    Check(!Id3Picture(s_tag, s_n, &off, &len), "a compressed frame: none");

    // the size of things that are not tags
    Check(Id3TagSize((const unsigned char*)"RIFF0000000000", 10) == 0, "not ID3: size 0");
    Check(Id3TagSize((const unsigned char*)"ID3", 3) == 0, "too short: size 0");
    unsigned char h[10] = { 'I', 'D', '3', 4, 0, 0x10, 0, 0, 1, 0 };
    Check(Id3TagSize(h, 10) == 10 + 128 + 10, "v2.4 with a footer: 10 more");
    unsigned char bad[10] = { 'I', 'D', '3', 3, 0, 0, 0x80, 0, 0, 0 };
    Check(Id3TagSize(bad, 10) == 0, "a size byte with the high bit: not a tag");
}

static int Pick(const wchar_t* a, const wchar_t* b = NULL, const wchar_t* c = NULL)
{
    const wchar_t* n[3] = { a, b, c };
    int k = c ? 3 : b ? 2 : 1;
    return CoverPick(n, k);
}

static void TestPick()
{
    Check(Pick(L"01 song.mp3", L"cover.jpg") == 1, "cover.jpg");
    Check(Pick(L"Folder.JPG", L"cover.png") == 1, "cover before folder");
    Check(Pick(L"front.jpg", L"folder.jpg") == 1, "folder before front");
    Check(Pick(L"AlbumArtSmall.jpg", L"AlbumArt_{X}_Large.jpg") == 1, "Windows' large album art before its small one");
    Check(Pick(L"scan.jpg", L"back.jpg", L"Cover (front).jpeg") == 2, "a name with 'cover' before other pictures");
    Check(Pick(L"01 song.mp3", L"photo.jpg") == 1, "the only picture there: it");
    Check(Pick(L"a.jpg", L"b.jpg") == -1, "two pictures, neither named: none");
    Check(Pick(L"01 song.mp3", L"cover.txt") == -1, "no picture: none");
    Check(Pick(L"cover.bmp") == 0, "BMP read too");
    Check(Pick(L"back.jpg", L"the_front.jpg") == 1, "a name with 'front' alone before other pictures");
    Check(Pick(L"cover.jpg", L"cover.png") == 0, "two equal names: the first");
    Check(Pick(L"AlbumArtSmall.jpg", L"scan.jpg") == 0, "Windows' small album art before a picture of no name");
    Check(CoverPick(NULL, 0) == -1, "an empty folder: none");
}

int main()
{
    TestTags();
    TestPick();
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
