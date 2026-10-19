// photo.cpp - see photo.h.
#include "photo.h"
#include <stdlib.h>
#include <string.h>
#include "tjpgd.h"

// stb_image is compiled in home.cpp (PNG, JPEG, BMP from memory); only its declarations here.
#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#include "stb_image.h"

enum {
    kStbMaxPx = 2600000,         // stb decodes the whole picture (3 bytes a pixel, then 2): ~2.6 MP at most
    kDecMaxBytes = 6 * 1024 * 1024,   // the scaled JPEG in memory before fitting
    kPoolBytes = 16 * 1024,      // TJpgDec's working memory (JD_SZBUF 4096 + tables)
    kExifRead = 128 * 1024       // the head of the file looked at for EXIF
};

bool PicIsName(const wchar_t* n)
{
    static const wchar_t* ext[4] = { L".jpg", L".jpeg", L".png", L".bmp" };
    const wchar_t* dot = wcsrchr(n, L'.');
    if (!dot)
        return false;
    for (int i = 0; i < 4; i++)
        if (!_wcsicmp(dot, ext[i]))
            return true;
    return false;
}

void PicFree(Pic* p)
{
    if (p->px)
        free(p->px);
    p->px = NULL;
    p->w = p->h = 0;
}

static bool PicAlloc(Pic* p, int w, int h)
{
    p->px = (unsigned short*)malloc((size_t)w * h * 2);
    p->w = p->px ? w : 0;
    p->h = p->px ? h : 0;
    return p->px != NULL;
}

// ---- EXIF: the orientation (1 normal, 3 upside down, 6 a quarter clockwise, 8 counter-clockwise; the mirrored
// ones as the nearest turn) and the thumbnail (IFD1: JPEGInterchangeFormat / its length).

struct Exif { int orient; const unsigned char* thumb; int thumbLen; };

static unsigned Rd16(const unsigned char* p, bool le) { return le ? p[0] | p[1] << 8 : p[0] << 8 | p[1]; }
static unsigned Rd32(const unsigned char* p, bool le)
{
    return le ? p[0] | p[1] << 8 | p[2] << 16 | (unsigned)p[3] << 24 : (unsigned)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3];
}

static void ExifParse(const unsigned char* b, int n, Exif* e)
{
    e->orient = 1;
    e->thumb = NULL;
    e->thumbLen = 0;
    if (n < 4 || b[0] != 0xFF || b[1] != 0xD8)
        return;
    int i = 2;
    while (i + 4 <= n && b[i] == 0xFF) {
        int m = b[i + 1], len = b[i + 2] << 8 | b[i + 3];
        if (m == 0xDA || m == 0xD9)
            return;   // the image itself: no EXIF before it
        if (m == 0xE1 && i + 10 <= n && !memcmp(b + i + 4, "Exif\0\0", 6)) {
            const unsigned char* t = b + i + 10;   // the TIFF header
            int tn = (i + 2 + len <= n ? len : n - i - 2) - 8;
            if (tn < 8)
                return;
            bool le = t[0] == 'I';
            unsigned ifd = Rd32(t + 4, le);
            for (int pass = 0; pass < 2 && ifd && ifd + 2 <= (unsigned)tn; pass++) {
                int cnt = Rd16(t + ifd, le);
                unsigned thumbOfs = 0, thumbLen = 0;
                for (int k = 0; k < cnt && ifd + 2 + (k + 1) * 12 <= (unsigned)tn; k++) {
                    const unsigned char* en = t + ifd + 2 + k * 12;
                    unsigned tag = Rd16(en, le);
                    if (pass == 0 && tag == 0x0112)
                        e->orient = Rd16(en + 8, le);
                    else if (pass == 1 && tag == 0x0201)
                        thumbOfs = Rd32(en + 8, le);
                    else if (pass == 1 && tag == 0x0202)
                        thumbLen = Rd32(en + 8, le);
                }
                if (pass == 1 && thumbOfs && thumbLen && thumbOfs + thumbLen <= (unsigned)tn) {
                    e->thumb = t + thumbOfs;
                    e->thumbLen = (int)thumbLen;
                }
                unsigned next = ifd + 2 + cnt * 12;
                ifd = next + 4 <= (unsigned)tn ? Rd32(t + next, le) : 0;
            }
            return;
        }
        i += 2 + len;
    }
}

static int ExifTurn(int orient)   // quarter turns clockwise for an EXIF orientation
{
    switch (orient) {
    case 3: case 4: return 2;
    case 6: case 5: return 1;
    case 8: case 7: return 3;
    default: return 0;
    }
}

// ---- TJpgDec: the stream from a file or from memory into a Pic

struct JpgSrc {
    HANDLE               f;
    const unsigned char* mem;
    size_t               memLen, memPos;
    Pic*                 dst;
};

static size_t JpgIn(JDEC* jd, uint8_t* buf, size_t n)
{
    JpgSrc* s = (JpgSrc*)jd->device;
    if (s->mem) {
        size_t k = s->memLen - s->memPos < n ? s->memLen - s->memPos : n;
        if (buf)
            memcpy(buf, s->mem + s->memPos, k);
        s->memPos += k;
        return k;
    }
    if (!buf)
        return SetFilePointer(s->f, (LONG)n, NULL, FILE_CURRENT) != 0xFFFFFFFF ? n : 0;
    DWORD got = 0;
    if (!ReadFile(s->f, buf, (DWORD)n, &got, NULL))
        return 0;
    return got;
}

static int JpgOut(JDEC* jd, void* bitmap, JRECT* r)
{
    JpgSrc* s = (JpgSrc*)jd->device;
    Pic* d = s->dst;
    const unsigned short* src = (const unsigned short*)bitmap;
    int rw = r->right - r->left + 1;
    for (int y = r->top; y <= r->bottom; y++, src += rw) {
        if (y >= d->h)
            continue;
        int n = r->left + rw > d->w ? d->w - r->left : rw;
        if (n > 0)
            memcpy(d->px + y * d->w + r->left, src, n * 2);
    }
    return 1;
}

// What a box bw x bh needs of a picture w x h (fill: the box covered, else the picture inside it): its size
// scaled so - the decoder keeps at least that.
static void Need(int w, int h, int bw, int bh, bool fill, int* nw, int* nh)
{
    double fx = (double)bw / w, fy = (double)bh / h;
    double f = fill ? (fx > fy ? fx : fy) : (fx < fy ? fx : fy);
    *nw = (int)(w * f);
    *nh = (int)(h * f);
}

// Decodes at the largest scale (1/1..1/8) that keeps what the box bw x bh needs (the size known from the header,
// before any decoding).
static int JpgDecode(JpgSrc* s, int bw, int bh, bool fill, Pic* out, int* srcW, int* srcH)
{
    void* pool = malloc(kPoolBytes);
    if (!pool)
        return PIC_MEMORY;
    JDEC jd;
    if (jd_prepare(&jd, JpgIn, pool, kPoolBytes, s) != JDR_OK) {
        free(pool);
        return PIC_FORMAT;   // progressive, CMYK, damaged: the caller may try stb
    }
    if (srcW)
        *srcW = jd.width;
    if (srcH)
        *srcH = jd.height;
    int needW, needH;
    Need(jd.width, jd.height, bw, bh, fill, &needW, &needH);
    int sc = 3;
    while (sc > 0 && ((jd.width >> sc) < needW || (jd.height >> sc) < needH))
        sc--;
    while (sc < 3 && (size_t)((jd.width + (1 << sc) - 1) >> sc) * ((jd.height + (1 << sc) - 1) >> sc) * 2 >
                         (size_t)kDecMaxBytes)
        sc++;
    int w = (jd.width + (1 << sc) - 1) >> sc, h = (jd.height + (1 << sc) - 1) >> sc;
    if ((size_t)w * h * 2 > (size_t)kDecMaxBytes || !PicAlloc(out, w, h)) {
        free(pool);
        return (size_t)w * h * 2 > (size_t)kDecMaxBytes ? PIC_BIG : PIC_MEMORY;
    }
    memset(out->px, 0, (size_t)w * h * 2);
    s->dst = out;
    JRESULT r = jd_decomp(&jd, JpgOut, (uint8_t)sc);
    free(pool);
    if (r != JDR_OK && r != JDR_INTR) {
        PicFree(out);
        return PIC_FORMAT;
    }
    return PIC_OK;
}

// stb (PNG, BMP, progressive JPEG): the whole file in memory, then the picture, up to kStbMaxPx.
static int StbDecode(const wchar_t* path, Pic* out, int* srcW, int* srcH)
{
    HANDLE f = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return PIC_READ;
    DWORD size = GetFileSize(f, NULL), got = 0;
    if (size == 0xFFFFFFFF || size > 16 * 1024 * 1024) {
        CloseHandle(f);
        return PIC_BIG;
    }
    unsigned char* data = (unsigned char*)malloc(size ? size : 1);
    if (!data) {
        CloseHandle(f);
        return PIC_MEMORY;
    }
    BOOL ok = ReadFile(f, data, size, &got, NULL);
    CloseHandle(f);
    int w, h, comp;
    if (!ok || got != size || !stbi_info_from_memory(data, (int)size, &w, &h, &comp)) {
        free(data);
        return PIC_FORMAT;
    }
    if (srcW)
        *srcW = w;
    if (srcH)
        *srcH = h;
    if ((size_t)w * h > (size_t)kStbMaxPx) {
        free(data);
        return PIC_BIG;
    }
    unsigned char* rgb = stbi_load_from_memory(data, (int)size, &w, &h, &comp, 3);
    free(data);
    if (!rgb)
        return PIC_FORMAT;
    if (!PicAlloc(out, w, h)) {
        stbi_image_free(rgb);
        return PIC_MEMORY;
    }
    for (int i = 0; i < w * h; i++) {
        const unsigned char* p = rgb + i * 3;
        out->px[i] = (unsigned short)((p[0] & 0xF8) << 8 | (p[1] & 0xFC) << 3 | p[2] >> 3);
    }
    stbi_image_free(rgb);
    return PIC_OK;
}

// ---- fitting: each output pixel the average of 4 samples of the source (a quarter turn rot applied)

static unsigned short Avg4(unsigned a, unsigned b, unsigned c, unsigned d)
{
    unsigned r = ((a >> 11) + (b >> 11) + (c >> 11) + (d >> 11)) >> 2;
    unsigned g = (((a >> 5) & 63) + ((b >> 5) & 63) + ((c >> 5) & 63) + ((d >> 5) & 63)) >> 2;
    unsigned bl = ((a & 31) + (b & 31) + (c & 31) + (d & 31)) >> 2;
    return (unsigned short)(r << 11 | g << 5 | bl);
}

static int Fit(const Pic* src, int rot, int boxW, int boxH, bool fill, Pic* out)
{
    rot &= 3;
    int rw = rot & 1 ? src->h : src->w, rh = rot & 1 ? src->w : src->h;   // the turned picture's size
    // the scale (16.16): fit - all inside, fill - the box covered
    double fx = (double)boxW / rw, fy = (double)boxH / rh;
    double f = fill ? (fx > fy ? fx : fy) : (fx < fy ? fx : fy);
    int ow = fill ? boxW : (int)(rw * f + 0.5), oh = fill ? boxH : (int)(rh * f + 0.5);
    ow = ow < 1 ? 1 : ow > boxW ? boxW : ow;
    oh = oh < 1 ? 1 : oh > boxH ? boxH : oh;
    if (!PicAlloc(out, ow, oh))
        return PIC_MEMORY;
    double ox = (rw - ow / f) / 2, oy = (rh - oh / f) / 2;   // fill: the cut, centred
    int step = (int)(65536 / f), q = step / 4;
    int x0 = (int)(ox * 65536), y0 = (int)(oy * 65536);
    for (int y = 0; y < oh; y++) {
        int vy = y0 + y * step;
        for (int x = 0; x < ow; x++) {
            int vx = x0 + x * step;
            unsigned smp[4];
            for (int k = 0; k < 4; k++) {   // the turned picture's point -> the source's
                int tx = (vx + (k & 1 ? 3 * q : q)) >> 16, ty = (vy + (k & 2 ? 3 * q : q)) >> 16;
                tx = tx < 0 ? 0 : tx >= rw ? rw - 1 : tx;
                ty = ty < 0 ? 0 : ty >= rh ? rh - 1 : ty;
                int sx, sy;
                switch (rot) {
                case 1:  sx = ty;              sy = src->h - 1 - tx; break;
                case 2:  sx = src->w - 1 - tx; sy = src->h - 1 - ty; break;
                case 3:  sx = src->w - 1 - ty; sy = tx;              break;
                default: sx = tx;              sy = ty;              break;
                }
                smp[k] = src->px[sy * src->w + sx];
            }
            out->px[y * ow + x] = Avg4(smp[0], smp[1], smp[2], smp[3]);
        }
    }
    return PIC_OK;
}

// ---- the two entry points

static int ReadHead(const wchar_t* path, unsigned char** head, int* n)
{
    HANDLE f = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return PIC_READ;
    *head = (unsigned char*)malloc(kExifRead);
    DWORD got = 0;
    if (!*head) {
        CloseHandle(f);
        return PIC_MEMORY;
    }
    if (!ReadFile(f, *head, kExifRead, &got, NULL)) {
        CloseHandle(f);
        free(*head);
        *head = NULL;
        return PIC_READ;
    }
    CloseHandle(f);
    *n = (int)got;
    return PIC_OK;
}

static bool IsJpegName(const wchar_t* path)
{
    const wchar_t* dot = wcsrchr(path, L'.');
    return dot && (!_wcsicmp(dot, L".jpg") || !_wcsicmp(dot, L".jpeg"));
}

// The picture decoded at a scale that keeps what the box bw x bh needs (in the source's own orientation).
static int Decode(const wchar_t* path, int bw, int bh, bool fill, Pic* dec, int* srcW, int* srcH)
{
    int r = PIC_FORMAT;
    if (IsJpegName(path)) {
        HANDLE f = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        if (f == INVALID_HANDLE_VALUE)
            return PIC_READ;
        JpgSrc s = { f, NULL, 0, 0, NULL };
        r = JpgDecode(&s, bw, bh, fill, dec, srcW, srcH);
        CloseHandle(f);
        if (r != PIC_FORMAT)
            return r;
    }
    return StbDecode(path, dec, srcW, srcH);   // PNG, BMP, a JPEG TJpgDec can't read
}

int PicLoad(const wchar_t* path, int boxW, int boxH, bool fill, int rot, bool exifRot, Pic* out, int* srcW,
            int* srcH)
{
    out->px = NULL;
    out->w = out->h = 0;
    int turn = rot;
    if (exifRot && IsJpegName(path)) {
        unsigned char* head = NULL;
        int n = 0;
        if (ReadHead(path, &head, &n) == PIC_OK) {
            Exif e;
            ExifParse(head, n, &e);
            turn += ExifTurn(e.orient);
            free(head);
        }
    }
    // needed in the source's orientation: the box turned back; fill needs the box covered, fit inside it
    int bw = turn & 1 ? boxH : boxW, bh = turn & 1 ? boxW : boxH;
    Pic dec = { 0, 0, NULL };
    int r = Decode(path, bw, bh, fill, &dec, srcW, srcH);
    if (r != PIC_OK)
        return r;
    r = Fit(&dec, turn, boxW, boxH, fill, out);
    PicFree(&dec);
    return r;
}

int PicThumb(const wchar_t* path, int w, int h, bool exifRot, Pic* out)
{
    out->px = NULL;
    out->w = out->h = 0;
    int turn = 0;
    if (IsJpegName(path)) {
        unsigned char* head = NULL;
        int n = 0;
        if (ReadHead(path, &head, &n) == PIC_OK) {
            Exif e;
            ExifParse(head, n, &e);
            if (exifRot)
                turn = ExifTurn(e.orient);
            int r = PIC_FORMAT;
            if (e.thumb) {   // the camera's own small JPEG
                JpgSrc s = { INVALID_HANDLE_VALUE, e.thumb, (size_t)e.thumbLen, 0, NULL };
                Pic dec = { 0, 0, NULL };
                r = JpgDecode(&s, turn & 1 ? h : w, turn & 1 ? w : h, true, &dec, NULL, NULL);
                if (r == PIC_OK) {
                    r = Fit(&dec, turn, w, h, true, out);
                    PicFree(&dec);
                }
            }
            free(head);
            if (r == PIC_OK)
                return r;
        }
    }
    Pic dec = { 0, 0, NULL };
    int r = Decode(path, turn & 1 ? h : w, turn & 1 ? w : h, true, &dec, NULL, NULL);
    if (r != PIC_OK)
        return r;
    r = Fit(&dec, turn, w, h, true, out);
    PicFree(&dec);
    return r;
}
