// cover.cpp - see cover.h.
#include "cover.h"
#include <string.h>
#include <wchar.h>

static int Syncsafe(const unsigned char* p)   // 4 bytes of 7 bits; -1 when a byte has the high bit
{
    if ((p[0] | p[1] | p[2] | p[3]) & 0x80)
        return -1;
    return p[0] << 21 | p[1] << 14 | p[2] << 7 | p[3];
}

static int Plain(const unsigned char* p, int bytes)
{
    int v = 0;
    for (int i = 0; i < bytes; i++)
        v = v << 8 | p[i];
    return v;
}

int Id3TagSize(const unsigned char* h, int n)
{
    if (n < 10 || memcmp(h, "ID3", 3) || h[3] < 2 || h[3] > 4)
        return 0;
    int body = Syncsafe(h + 6);
    if (body < 0)
        return 0;
    return 10 + body + (h[3] == 4 && (h[5] & 0x10) ? 10 : 0);
}

// FF 00 -> FF over n bytes in place; the new length.
static int Unsync(unsigned char* p, int n)
{
    int k = 0;
    for (int i = 0; i < n; i++) {
        p[k++] = p[i];
        if (p[i] == 0xFF && i + 1 < n && p[i + 1] == 0)
            i++;
    }
    return k;
}

static bool IsPicture(const unsigned char* p, int n)
{
    return (n >= 3 && p[0] == 0xFF && p[1] == 0xD8 && p[2] == 0xFF) ||
           (n >= 8 && !memcmp(p, "\x89PNG\r\n\x1A\n", 8));
}

// The end of a description in encoding enc (0, 3: one zero byte; 1, 2: UTF-16, a zero pair at an even place):
// the index after it, -1 none.
static int TextEnd(const unsigned char* p, int n, int enc)
{
    if (enc == 1 || enc == 2) {
        for (int i = 0; i + 1 < n; i += 2)
            if (!p[i] && !p[i + 1])
                return i + 2;
        return -1;
    }
    const unsigned char* z = (const unsigned char*)memchr(p, 0, n);
    return z ? (int)(z - p) + 1 : -1;
}

// An APIC (v2: PIC) frame's body: its picture's type and where the picture is in it; false when not a picture.
static bool FramePicture(const unsigned char* b, int n, bool v22, int* type, int* off, int* len)
{
    if (n < 2)
        return false;
    int enc = b[0], k = 1;
    if (v22) {
        k += 3;   // "JPG" / "PNG"
    } else {
        const unsigned char* z = (const unsigned char*)memchr(b + k, 0, n - k);
        if (!z)
            return false;
        k = (int)(z - b) + 1;
    }
    if (k >= n)
        return false;
    *type = b[k++];
    int e = TextEnd(b + k, n - k, enc);
    if (e < 0)
        return false;
    k += e;
    if (!IsPicture(b + k, n - k))
        return false;
    *off = k;
    *len = n - k;
    return true;
}

// Where the frames start: after the header and the extended header (its size: v2.4 counts itself); -1 damaged.
static int FramesStart(const unsigned char* tag, int end, int major)
{
    if (major < 3 || !(tag[5] & 0x40))
        return 10;
    if (14 > end)
        return -1;
    int ext = major == 4 ? Syncsafe(tag + 10) : 4 + Plain(tag + 10, 4);
    return ext < 0 ? -1 : 10 + ext;
}

// A frame header's body size (v2.2: 3 bytes; v2.3: plain; v2.4: syncsafe, plain from some writers).
static int FrameSize(const unsigned char* h, int major)
{
    if (major == 2)
        return Plain(h + 3, 3);
    int fs = major == 4 ? Syncsafe(h + 4) : -1;
    return fs >= 0 ? fs : Plain(h + 4, 4);
}

// A frame's body (b, bn) past its own extras (v2.4: grouping, the data length, unsynchronised; v2.3: grouping);
// false when it can't be read (compressed, encrypted).
static void Skip(unsigned char** b, int* bn, int n)
{
    *b += n;
    *bn -= n;
}

static bool FrameBody23(int ff, unsigned char** b, int* bn)   // compressed, encrypted: no; grouped: its byte
{
    if (ff & 0xC0)
        return false;
    Skip(b, bn, ff & 0x20 ? 1 : 0);
    return *bn > 0;
}

static bool FrameBody24(int ff, unsigned char** b, int* bn)   // + the data length, the frame unsynchronised
{
    if (ff & 0x0C)
        return false;
    Skip(b, bn, (ff & 0x40 ? 1 : 0) + (ff & 0x01 ? 4 : 0));
    if (*bn > 0 && (ff & 0x02))
        *bn = Unsync(*b, *bn);
    return *bn > 0;
}

static bool FrameBody(int major, int ff, unsigned char** b, int* bn)
{
    return major == 4 ? FrameBody24(ff, b, bn) : major == 3 ? FrameBody23(ff, b, bn) : *bn > 0;
}

// The tag's frames' end within its n bytes read (its unsynchronisation of v2.2 / v2.3 undone in place).
static int TagEnd(unsigned char* tag, int n)
{
    int end = 10 + Syncsafe(tag + 6);
    end = end < n ? end : n;
    return tag[3] < 4 && (tag[5] & 0x80) ? 10 + Unsync(tag + 10, end - 10) : end;
}

// The frame at tag + k (size fs) as a picture: its type and where in the tag; false when it is not one.
static bool FramePic(unsigned char* tag, int k, int fs, int* type, int* off, int* len)
{
    int major = tag[3];
    bool v22 = major == 2;
    if (memcmp(tag + k, v22 ? "PIC" : "APIC", v22 ? 3 : 4))
        return false;
    unsigned char* b = tag + k + (v22 ? 6 : 10);
    int bn = fs, po;
    if (!FrameBody(major, v22 ? 0 : tag[k + 9], &b, &bn) || !FramePicture(b, bn, v22, type, &po, len))
        return false;
    *off = (int)(b - tag) + po;
    return true;
}

// The size of the frame at k, 0 at the end (padding, past the tag, a size beyond it).
static int NextFrame(const unsigned char* tag, int k, int end, int head)
{
    if (k < 0 || k + head > end || !tag[k])
        return 0;
    int fs = FrameSize(tag + k, tag[3]);
    return fs > 0 && fs <= end - k - head ? fs : 0;
}

bool Id3Picture(unsigned char* tag, int n, int* off, int* len)
{
    int size = Id3TagSize(tag, n);
    if (!size || size > n)
        return false;
    int major = tag[3], end = TagEnd(tag, n), head = major == 2 ? 6 : 10, found = 0;   // 1 a picture, 2 the front cover
    for (int k = FramesStart(tag, end, major), fs; found < 2 && (fs = NextFrame(tag, k, end, head)) > 0;) {
        int type, po, pl;
        if (FramePic(tag, k, fs, &type, &po, &pl) && (!found || type == 3)) {
            *off = po;
            *len = pl;
            found = type == 3 ? 2 : 1;
        }
        k += head + fs;
    }
    return found > 0;
}

// ---- the texts

// The field a text frame fills (0 title, 1 artist, 2 album, 3 the year), -1 another frame.
static int TextField(const unsigned char* id, bool v22)
{
    static const char* const ids[] = { "TIT2", "TPE1", "TALB", "TYER", "TDRC" };
    static const char* const ids22[] = { "TT2", "TP1", "TAL", "TYE" };
    for (int i = 0; i < 5; i++)
        if (v22 ? i < 4 && !memcmp(id, ids22[i], 3) : !memcmp(id, ids[i], 4))
            return i < 4 ? i : 3;
    return -1;
}

// The frame at tag + k (size fs) into its field of t, when it is a text one and that field is still empty.
static bool FrameText(unsigned char* tag, int k, int fs, TagTexts* t)
{
    int major = tag[3];
    bool v22 = major == 2;
    wchar_t* dst[4] = { t->title, t->artist, t->album, t->year };
    const int caps[4] = { TAG_CAP(t->title), TAG_CAP(t->artist), TAG_CAP(t->album), TAG_CAP(t->year) };
    int f = TextField(tag + k, v22);
    unsigned char* b = tag + k + (v22 ? 6 : 10);
    int bn = fs;
    if (f < 0 || dst[f][0] || !FrameBody(major, v22 ? 0 : tag[k + 9], &b, &bn))
        return false;
    TagDecode(b + 1, bn - 1, b[0], dst[f], caps[f]);
    if (f == 3)
        dst[f][4] = 0;   // (TDRC: "2019-05-01")
    return dst[f][0] != 0;
}

bool Id3Texts(unsigned char* tag, int n, TagTexts* t)
{
    if (!Id3TagSize(tag, n))
        return false;
    int major = tag[3], end = TagEnd(tag, n), head = major == 2 ? 6 : 10;
    bool any = false;
    for (int k = FramesStart(tag, end, major), fs; (fs = NextFrame(tag, k, end, head)) > 0; k += head + fs)
        any = FrameText(tag, k, fs, t) || any;
    return any;
}

// ---- the folder's picture

static bool PictureName(const wchar_t* n, wchar_t* base, int cap)   // base: the name without its extension, lower case
{
    const wchar_t* dot = wcsrchr(n, L'.');
    if (!dot || (_wcsicmp(dot, L".jpg") && _wcsicmp(dot, L".jpeg") && _wcsicmp(dot, L".png") && _wcsicmp(dot, L".bmp")))
        return false;
    int l = (int)(dot - n) < cap - 1 ? (int)(dot - n) : cap - 1;
    for (int i = 0; i < l; i++)
        base[i] = towlower(n[i]);
    base[l] = 0;
    return true;
}

static int Rank(const wchar_t* b)   // lower is better; 9: a picture of no telling name
{
    if (!wcscmp(b, L"cover"))
        return 0;
    if (!wcscmp(b, L"folder"))
        return 1;
    if (!wcscmp(b, L"front"))
        return 2;
    if (!wcsncmp(b, L"albumart", 8))
        return wcsstr(b, L"small") ? 4 : 3;
    if (wcsstr(b, L"cover") || wcsstr(b, L"front"))
        return 5;
    return 9;
}

int CoverPick(const wchar_t* const* names, int n)
{
    int best = -1, bestRank = 99, pictures = 0;
    for (int i = 0; i < n; i++) {
        wchar_t base[MAX_PATH];
        if (!names[i] || !PictureName(names[i], base, MAX_PATH))
            continue;
        pictures++;
        int r = Rank(base);
        if (r < bestRank) {
            bestRank = r;
            best = i;
        }
    }
    return bestRank < 9 || pictures == 1 ? best : -1;
}
