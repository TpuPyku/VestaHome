// md5.h - self-contained MD5 (RFC 1321) used by SPIPowerInstall.exe to fingerprint
// the firmware files it is about to touch, and by tools/md5_hosttest.c to prove on the
// PC that this implementation matches the vendor's own md5.txt manifest values.
//
// Portable C (no windows.h, no stdio) so the very same source runs in the ARM build,
// in the x86 host test and in the CE emulator. `static` on purpose: it is included by
// exactly one translation unit.
#ifndef SPIPOWER_MD5_H
#define SPIPOWER_MD5_H

#include <string.h>

typedef struct {
    unsigned int  count[2];   // message length in bits, low/high
    unsigned int  state[4];   // A, B, C, D
    unsigned char buf[64];    // partial block
} md5_ctx;

static const unsigned char md5_pad[64] = { 0x80 };

static void md5_transform(unsigned int st[4], const unsigned char blk[64])
{
    static const unsigned int K[64] = {
        0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a,
        0xa8304613, 0xfd469501, 0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
        0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821, 0xf61e2562, 0xc040b340,
        0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
        0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8,
        0x676f02d9, 0x8d2a4c8a, 0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
        0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70, 0x289b7ec6, 0xeaa127fa,
        0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
        0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92,
        0xffeff47d, 0x85845dd1, 0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
        0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391 };
    static const unsigned char S[64] = {
        7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
        5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20,
        4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
        6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21 };

    unsigned int a = st[0], b = st[1], c = st[2], d = st[3], x[16], f, g, t;
    int i;
    for (i = 0; i < 16; i++)
        x[i] = (unsigned int)blk[i * 4]           | ((unsigned int)blk[i * 4 + 1] << 8)
             | ((unsigned int)blk[i * 4 + 2] << 16) | ((unsigned int)blk[i * 4 + 3] << 24);

    for (i = 0; i < 64; i++) {
        if (i < 16)      { f = (b & c) | (~b & d);  g = i; }
        else if (i < 32) { f = (d & b) | (~d & c);  g = (5 * i + 1) & 15; }
        else if (i < 48) { f = b ^ c ^ d;           g = (3 * i + 5) & 15; }
        else             { f = c ^ (b | ~d);        g = (7 * i) & 15; }
        t = d;
        d = c;
        c = b;
        b = b + (((a + f + K[i] + x[g]) << S[i]) | ((a + f + K[i] + x[g]) >> (32 - S[i])));
        a = t;
    }
    st[0] += a; st[1] += b; st[2] += c; st[3] += d;
}

static void md5_init(md5_ctx* ctx)
{
    ctx->count[0] = ctx->count[1] = 0;
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xefcdab89;
    ctx->state[2] = 0x98badcfe;
    ctx->state[3] = 0x10325476;
    memset(ctx->buf, 0, sizeof(ctx->buf));
}

static void md5_update(md5_ctx* ctx, const void* data, unsigned int len)
{
    const unsigned char* p = (const unsigned char*)data;
    unsigned int idx = (ctx->count[0] >> 3) & 0x3f;   // bytes already buffered
    unsigned int part = 64 - idx;
    unsigned int add  = len << 3;

    if ((ctx->count[0] += add) < add) ctx->count[1]++;
    ctx->count[1] += (len >> 29);

    while (len >= part) {
        memcpy(&ctx->buf[idx], p, part);
        md5_transform(ctx->state, ctx->buf);
        p += part;
        len -= part;
        idx = 0;
        part = 64;
    }
    memcpy(&ctx->buf[idx], p, len);
}

static void md5_final(md5_ctx* ctx, unsigned char out[16])
{
    unsigned char bits[8];
    unsigned int  idx = (ctx->count[0] >> 3) & 0x3f;
    unsigned int  padLen = (idx < 56) ? (56 - idx) : (120 - idx);
    int i;

    for (i = 0; i < 8; i++) bits[i] = (unsigned char)(ctx->count[i >> 2] >> ((i & 3) * 8));
    md5_update(ctx, md5_pad, padLen);
    md5_update(ctx, bits, 8);
    for (i = 0; i < 16; i++) out[i] = (unsigned char)(ctx->state[i >> 2] >> ((i & 3) * 8));
}

static void md5_hex(const unsigned char d[16], char out[33])
{
    static const char* hx = "0123456789abcdef";
    int i;
    for (i = 0; i < 16; i++) {
        out[i * 2]     = hx[(d[i] >> 4) & 15];
        out[i * 2 + 1] = hx[d[i] & 15];
    }
    out[32] = 0;
}

#endif // SPIPOWER_MD5_H
