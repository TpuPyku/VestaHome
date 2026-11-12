// adb.cpp - see adb.h. The protocol: Android's system/core/adb/protocol.txt; the key's text: adb's own adbkey.pub.
#include <string.h>
#include "adb.h"
#include "bearssl.h"

#define Put32 RndisPut32
#define Get32 RndisGet32

int AdbHeader(u8* out, u32 cmd, u32 a0, u32 a1, const u8* data, u32 len)
{
    u32 sum = 0;
    for (u32 i = 0; data && i < len; i++)
        sum += data[i];
    Put32(out, cmd);
    Put32(out + 4, a0);
    Put32(out + 8, a1);
    Put32(out + 12, len);
    Put32(out + 16, sum);
    Put32(out + 20, cmd ^ 0xFFFFFFFFu);
    return ADB_HEADER;
}

bool AdbParse(const u8* h, int n, AdbMsg* m)
{
    if (n < ADB_HEADER || Get32(h + 20) != (Get32(h) ^ 0xFFFFFFFFu))
        return false;
    m->cmd = Get32(h);
    m->a0 = Get32(h + 4);
    m->a1 = Get32(h + 8);
    m->len = Get32(h + 12);
    return m->len <= ADB_MAXDATA;
}

// ---- the public key's text

enum { kWords = ADB_KEY_BYTES / 4, kBlob = 4 + 4 + 2 * ADB_KEY_BYTES + 4 };

static void Words(const u8* be, u32* w)   // big endian bytes -> little endian words, the lowest first
{
    for (int i = 0; i < kWords; i++) {
        const u8* p = be + ADB_KEY_BYTES - 4 * (i + 1);
        w[i] = (u32)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3];
    }
}

static bool AtLeast(const u32* x, const u32* n)
{
    for (int i = kWords - 1; i >= 0; i--)
        if (x[i] != n[i])
            return x[i] > n[i];
    return true;
}

// R^2 mod n, R = 2^2048: 1 doubled 4096 times, each time below n again
static void Rr(const u32* n, u32* x)
{
    memset(x, 0, kWords * 4);
    x[0] = 1;
    for (int k = 0; k < 2 * 8 * ADB_KEY_BYTES; k++) {
        u32 carry = 0;
        for (int i = 0; i < kWords; i++) {
            u32 top = x[i] >> 31;
            x[i] = x[i] << 1 | carry;
            carry = top;
        }
        if (carry || AtLeast(x, n)) {
            u32 borrow = 0;
            for (int i = 0; i < kWords; i++) {
                u32 d = x[i] - n[i] - borrow;
                borrow = (x[i] < n[i] || (x[i] == n[i] && borrow)) ? 1 : 0;
                x[i] = d;
            }
        }
    }
}

static u32 N0Inv(u32 n0)   // -1/n0 mod 2^32 (n0 odd): Newton's steps double the right bits
{
    u32 inv = n0;
    for (int i = 0; i < 5; i++)
        inv *= 2 - n0 * inv;
    return 0 - inv;
}

static int Base64(const u8* in, int len, char* out)
{
    static const char kAbc[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int k = 0;
    for (int i = 0; i < len; i += 3) {
        u32 v = (u32)in[i] << 16 | (i + 1 < len ? in[i + 1] << 8 : 0) | (i + 2 < len ? in[i + 2] : 0);
        out[k++] = kAbc[v >> 18 & 63];
        out[k++] = kAbc[v >> 12 & 63];
        out[k++] = i + 1 < len ? kAbc[v >> 6 & 63] : '=';
        out[k++] = i + 2 < len ? kAbc[v & 63] : '=';
    }
    return k;
}

int AdbPubKey(const u8* n, u32 e, const char* name, char* out, int cap)
{
    int need = (kBlob + 2) / 3 * 4 + 1 + (int)strlen(name) + 1;
    if (cap < need)
        return 0;
    u32 w[kWords], rr[kWords];
    u8 blob[kBlob];
    Words(n, w);
    Rr(w, rr);
    Put32(blob, kWords);
    Put32(blob + 4, N0Inv(w[0]));
    for (int i = 0; i < kWords; i++) {
        Put32(blob + 8 + 4 * i, w[i]);
        Put32(blob + 8 + ADB_KEY_BYTES + 4 * i, rr[i]);
    }
    Put32(blob + 8 + 2 * ADB_KEY_BYTES, e);
    int k = Base64(blob, kBlob, out);
    out[k++] = ' ';
    strcpy(out + k, name);
    return need;
}

// ---- the key and its file

static void Take(AdbBlob* b, const unsigned char* p, size_t len)
{
    b->len = (int)len;
    memcpy(b->b, p, len);
}

bool AdbKeyMake(const u8* seed, int len, AdbKey* k)
{
    static unsigned char kpriv[BR_RSA_KBUF_PRIV_SIZE(2048)], kpub[BR_RSA_KBUF_PUB_SIZE(2048)];
    br_hmac_drbg_context rng;
    br_rsa_private_key sk;
    br_rsa_public_key pk;
    br_hmac_drbg_init(&rng, &br_sha256_vtable, seed, (size_t)len);
    if (!br_rsa_i31_keygen(&rng.vtable, &sk, kpriv, &pk, kpub, 2048, 65537))
        return false;
    memset(k, 0, sizeof(*k));
    Take(&k->n, pk.n, pk.nlen);
    Take(&k->e, pk.e, pk.elen);
    Take(&k->p, sk.p, sk.plen);
    Take(&k->q, sk.q, sk.qlen);
    Take(&k->dp, sk.dp, sk.dplen);
    Take(&k->dq, sk.dq, sk.dqlen);
    Take(&k->iq, sk.iq, sk.iqlen);
    return k->n.len == ADB_KEY_BYTES;
}

static const char kMagic[4] = { 'V', 'T', 'K', '1' };

static AdbBlob* Part(AdbKey* k, int i)
{
    AdbBlob* parts[7] = { &k->n, &k->e, &k->p, &k->q, &k->dp, &k->dq, &k->iq };
    return parts[i];
}

int AdbKeyPack(const AdbKey* k, u8* out, int cap)
{
    int pos = 4;
    if (cap < pos)
        return 0;
    memcpy(out, kMagic, 4);
    for (int i = 0; i < 7; i++) {
        const AdbBlob* b = Part((AdbKey*)k, i);
        if (pos + 2 + b->len > cap)
            return 0;
        out[pos] = (u8)b->len;
        out[pos + 1] = (u8)(b->len >> 8);
        memcpy(out + pos + 2, b->b, b->len);
        pos += 2 + b->len;
    }
    return pos;
}

bool AdbKeyParse(const u8* in, int len, AdbKey* k)
{
    if (len < 4 || memcmp(in, kMagic, 4))
        return false;
    memset(k, 0, sizeof(*k));
    int pos = 4;
    for (int i = 0; i < 7; i++) {
        if (pos + 2 > len)
            return false;
        int n = in[pos] | in[pos + 1] << 8;
        if (n > ADB_KEY_BYTES || pos + 2 + n > len)
            return false;
        Take(Part(k, i), in + pos + 2, (size_t)n);
        pos += 2 + n;
    }
    return pos == len && k->n.len == ADB_KEY_BYTES;
}

bool AdbSign(const AdbKey* k, const u8* token, u8* sig)
{
    br_rsa_private_key sk = { 2048, (unsigned char*)k->p.b, (size_t)k->p.len, (unsigned char*)k->q.b, (size_t)k->q.len,
                              (unsigned char*)k->dp.b, (size_t)k->dp.len, (unsigned char*)k->dq.b, (size_t)k->dq.len,
                              (unsigned char*)k->iq.b, (size_t)k->iq.len };
    return br_rsa_i31_pkcs1_sign(BR_HASH_OID_SHA1, token, ADB_TOKEN, &sk, sig) == 1;
}

// ---- the session

void AdbStart(Adb* a, u32 local)
{
    a->state = ADB_HELLO;
    a->local = local;
    a->remote = 0;
}

static int OnAuth(Adb* a, const AdbMsg& m)   // a token: signed, then the key offered, then refused
{
    if (m.a0 != ADB_AUTH_TOKEN || a->state > ADB_KEYSENT)
        return ADB_DO_WAIT;
    a->state = a->state == ADB_HELLO ? ADB_SIGNED : a->state == ADB_SIGNED ? ADB_KEYSENT : ADB_FAILED;
    return a->state == ADB_SIGNED ? ADB_DO_SIGN : a->state == ADB_KEYSENT ? ADB_DO_SENDKEY : ADB_DO_FAIL;
}

static int OnStream(Adb* a, const AdbMsg& m)   // our command's stream: opened, its output, closed
{
    if ((a->state != ADB_OPENING && a->state != ADB_STREAM) || m.a1 != a->local)
        return ADB_DO_WAIT;
    if (m.cmd == ADB_OKAY && a->state == ADB_OPENING) {
        a->state = ADB_STREAM;
        a->remote = m.a0;
    } else if (m.cmd == ADB_WRTE && a->state == ADB_STREAM) {
        return ADB_DO_OKAY;
    } else if (m.cmd == ADB_CLSE) {
        a->state = ADB_DONE;
        return ADB_DO_DONE;
    }
    return ADB_DO_WAIT;
}

int AdbOn(Adb* a, const AdbMsg& m)
{
    if (m.cmd == ADB_AUTH)
        return OnAuth(a, m);
    if (m.cmd != ADB_CNXN)
        return OnStream(a, m);
    if (a->state > ADB_KEYSENT)
        return ADB_DO_WAIT;
    a->state = ADB_OPENING;
    return ADB_DO_OPEN;
}
