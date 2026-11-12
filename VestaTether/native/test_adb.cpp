// test_adb.cpp - adb.cpp on the PC: the messages, the key's file and text, the signature, a session with a phone.
#include <stdio.h>
#include <string.h>
#include "adb.h"
#include "bearssl.h"

static int s_checks, s_failed;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAIL: %s\n", what);
    }
}

static u32 G32(const u8* p)
{
    return p[0] | p[1] << 8 | p[2] << 16 | (u32)p[3] << 24;
}

static AdbMsg Msg(u32 cmd, u32 a0, u32 a1, u32 len)
{
    AdbMsg m = { cmd, a0, a1, len };
    return m;
}

static void TestHeader()
{
    u8 h[ADB_HEADER];
    const u8 data[] = "host::";
    Check(AdbHeader(h, ADB_CNXN, ADB_VERSION, ADB_MAXDATA, data, 7) == ADB_HEADER, "header: 24 bytes");
    Check(G32(h) == ADB_CNXN && G32(h + 4) == ADB_VERSION && G32(h + 8) == ADB_MAXDATA && G32(h + 12) == 7,
          "header: command, arguments, length, little endian");
    u32 sum = 0;
    for (int i = 0; i < 7; i++)
        sum += data[i];
    Check(G32(h + 16) == sum, "header: the data's checksum - its bytes added (old phones check it)");
    Check(G32(h + 20) == (ADB_CNXN ^ 0xFFFFFFFFu), "header: the magic - the command inverted");
    AdbMsg m;
    Check(AdbParse(h, ADB_HEADER, &m) && m.cmd == ADB_CNXN && m.a0 == ADB_VERSION && m.a1 == ADB_MAXDATA && m.len == 7,
          "parse: what was written");
    Check(!AdbParse(h, ADB_HEADER - 1, &m), "parse: a short one - none");
    h[20] ^= 1;
    Check(!AdbParse(h, ADB_HEADER, &m), "parse: a wrong magic - none");
    AdbHeader(h, ADB_WRTE, 1, 2, NULL, ADB_MAXDATA + 1);
    Check(!AdbParse(h, ADB_HEADER, &m), "parse: more data than we take - none");
    AdbHeader(h, ADB_WRTE, 1, 2, NULL, ADB_MAXDATA);
    Check(AdbParse(h, ADB_HEADER, &m) && m.len == ADB_MAXDATA, "parse: the most data we take - taken");
    const u8 more[] = { 1, 2, 3, 200 };
    AdbHeader(h, ADB_WRTE, 1, 2, more, 3);
    Check(G32(h + 16) == 6, "header: the checksum of its length only, not past it");
}

static int Hex(char c)
{
    return c <= '9' ? c - '0' : c - 'a' + 10;
}

static void TestPubKey()
{
    // (expected: Python's own arithmetic over the same modulus - scratch adb_oracle.py, 2026-11-12)
    static const char kN[] = "f9f950e57d58f374c9ad4f40680ec2d2560644de2acfe9ccd282f61974759028eab3ddf3264e51e9a92403cbd54d67b7b6318fadb952f2db6ff5235b2ac8b44f55125c55104f92952f72b5b5386d8102f404e0bd4b49da3d61bb674bdef1028162b14d0269e112615ed1d4d4f0c8bbd0e23dfdfc3b9e82e47dcd6dcf754be34ec0e9c9fa89744afb877cfe4a828b6da66177f19abfb60f24ac32d61d70151a42b3fd5c744135f21ee2b2d992ad1a05d6cf0dbdd8e3f52e78b327a1268b13658fa94d55ff7ed35775710e42091ac99ae42284629fb8ecd3c06d74723fe6762a1c583e82abf42ae64864d8b7a1e65343109e67f59611dd4fea20537686483f0bdf";
    static const char kPub[] = "QAAAAOGPqwXfCz9IhnZTIOpP3RGW9WeeEENT5qG32GRI5ir0q4I+WBwqduY/cnRtwNPsuJ9ihCLkmskaCUIOcXVX037/VU2pj2UTiyahJ7N4LvXj2L0Nz9YFGq2S2bLiHvI1QXRc/bNCGhVwHdYyrCQPtr+a8Xdhpm2Lgkr+fIf7SnSJ+snpwE7jS3XPbc195IKeO/z9PeLQu8jw1NTRXmES4WkCTbFigQLx3ktnu2E92klLveAE9AKBbTi1tXIvlZJPEFVcElVPtMgqWyP1b9vyUrmtjzG2t2dN1csDJKnpUU4m892z6iiQdXQZ9oLSzOnPKt5EBlbSwg5oQE+tyXTzWH3lUPn51UdTQfl+E+1LByrQMIBZr2fOBDmhHX5kcphO61JbOc+ejxMojm6VvdQHxS/40RMdxr1i2wu7HYcQ4lbtfuLjC/UhrdLIwJXj1lGFdFWik9S8X7L9tYt4xWr7wk+sN8VSYbRIjabccp7CQA/Abd15C2eRjpK3IGhUegDpunpagisfPJdXPNKuvjXhMDTIyPEJjFqSht72BQ+O9QBchfC2LziwLo+drmHAI5cdRjRbQ2o+cMW7vcF4POZ6w5jY1nijdeOF+rILOK2kYyyp+thBp/xGBGbkfww+UNgKcmWwX8flGbbxujcMuDW1puf520Tg/ShTjdlv8j3pb/tHOXJO3AEAAQA= vesta@hu";
    u8 n[ADB_KEY_BYTES];
    for (int i = 0; i < ADB_KEY_BYTES; i++)
        n[i] = (u8)(Hex(kN[2 * i]) * 16 + Hex(kN[2 * i + 1]));
    char out[1000];
    int k = AdbPubKey(n, 65537, "vesta@hu", out, sizeof(out));
    Check(k == (int)sizeof(kPub) && !strcmp(out, kPub), "public key: mincrypt in base64 and the name, as adb writes it");
    Check(AdbPubKey(n, 65537, "vesta@hu", out, (int)sizeof(kPub) - 1) == 0, "public key: one byte short - none");
    Check(AdbPubKey(n, 65537, "vesta@hu", out, (int)sizeof(kPub)) == (int)sizeof(kPub), "public key: just room");
}

static AdbKey s_key;

static void TestKey()
{
    u8 seed[32];
    for (int i = 0; i < 32; i++)
        seed[i] = (u8)(i * 7 + 1);
    Check(AdbKeyMake(seed, sizeof(seed), &s_key), "key: made");
    Check(s_key.n.len == ADB_KEY_BYTES && (s_key.n.b[0] & 0x80) && s_key.e.len >= 1, "key: 2048 bits");
    u8 file[2000];
    int len = AdbKeyPack(&s_key, file, sizeof(file));
    AdbKey back;
    Check(len > 0 && AdbKeyParse(file, len, &back) && !memcmp(&back, &s_key, sizeof(back)), "key: its file read back");
    Check(AdbKeyPack(&s_key, file, len - 1) == 0, "key: no room - nothing");
    Check(AdbKeyPack(&s_key, file, len) == len, "key: just room");
    Check(AdbKeyPack(&s_key, file, 4) == 0 && AdbKeyPack(&s_key, file, 3) == 0, "key: room for the mark only, or not even it");
    Check(!AdbKeyParse(file, 4, &back) && !AdbKeyParse(file, 3, &back), "key: the mark alone, or less - none");
    Check(!AdbKeyParse(file, 4 + 2 + ADB_KEY_BYTES + 1, &back), "key: cut in a part's length - none");
    AdbKey odd = s_key;
    odd.n.len = ADB_KEY_BYTES - 1;
    int oddLen = AdbKeyPack(&odd, file, sizeof(file));
    Check(oddLen > 0 && !AdbKeyParse(file, oddLen, &back), "key: a modulus not 2048 bits - none");
    len = AdbKeyPack(&s_key, file, sizeof(file));
    file[4 + 2 + ADB_KEY_BYTES] = 1;   // (e's length: 257)
    file[4 + 2 + ADB_KEY_BYTES + 1] = 1;
    Check(!AdbKeyParse(file, len, &back), "key: a part longer than a modulus - none");
    len = AdbKeyPack(&s_key, file, sizeof(file));
    Check(!AdbKeyParse(file, len - 1, &back), "key: a file cut short - none");
    file[0] ^= 1;
    Check(!AdbKeyParse(file, len, &back), "key: another file - none");
    AdbKey other;
    seed[0] ^= 1;
    Check(AdbKeyMake(seed, sizeof(seed), &other) && memcmp(other.n.b, s_key.n.b, ADB_KEY_BYTES),
          "key: other randomness - another key");
}

static void TestSign()
{
    u8 token[ADB_TOKEN], sig[ADB_KEY_BYTES], hash[ADB_TOKEN];
    for (int i = 0; i < ADB_TOKEN; i++)
        token[i] = (u8)(200 - i);
    Check(AdbSign(&s_key, token, sig), "sign: done");
    br_rsa_public_key pk = { s_key.n.b, (size_t)s_key.n.len, s_key.e.b, (size_t)s_key.e.len };
    Check(br_rsa_i31_pkcs1_vrfy(sig, ADB_KEY_BYTES, BR_HASH_OID_SHA1, ADB_TOKEN, &pk, hash) && !memcmp(hash, token, ADB_TOKEN),
          "sign: the phone's check - PKCS#1 with SHA-1's tag over the token as it is");
}

static void TestSession()
{
    Adb a;
    AdbStart(&a, 7);
    a.remote = 5;
    AdbStart(&a, 7);
    Check(a.state == ADB_HELLO && a.local == 7 && a.remote == 0, "session: starts at hello, no stream yet");
    Check(AdbOn(&a, Msg(ADB_AUTH, ADB_AUTH_TOKEN, 0, ADB_TOKEN)) == ADB_DO_SIGN, "session: a token - signed");
    Check(AdbOn(&a, Msg(ADB_AUTH, ADB_AUTH_TOKEN, 0, ADB_TOKEN)) == ADB_DO_SENDKEY, "session: the signature unknown - the key offered");
    Check(AdbOn(&a, Msg(ADB_CNXN, ADB_VERSION, ADB_MAXDATA, 10)) == ADB_DO_OPEN && a.state == ADB_OPENING,
          "session: the user allowed - the command opened");
    Check(AdbOn(&a, Msg(ADB_OKAY, 99, 8, 0)) == ADB_DO_WAIT && a.state == ADB_OPENING, "session: OKAY to another stream - no");
    Check(AdbOn(&a, Msg(ADB_OKAY, 99, 7, 0)) == ADB_DO_WAIT && a.state == ADB_STREAM && a.remote == 99,
          "session: the stream open - the phone's id kept");
    Check(AdbOn(&a, Msg(ADB_WRTE, 99, 8, 5)) == ADB_DO_WAIT, "session: output of another stream - not ours");
    Check(AdbOn(&a, Msg(ADB_WRTE, 99, 7, 5)) == ADB_DO_OKAY, "session: output - taken");
    Check(AdbOn(&a, Msg(ADB_CLSE, 99, 7, 0)) == ADB_DO_DONE && a.state == ADB_DONE, "session: closed - done");
    Check(AdbOn(&a, Msg(ADB_WRTE, 99, 7, 5)) == ADB_DO_WAIT, "session: after the end - nothing");

    AdbStart(&a, 1);
    Check(AdbOn(&a, Msg(ADB_CNXN, ADB_VERSION, ADB_MAXDATA, 10)) == ADB_DO_OPEN, "known key or no auth: straight to the command");
    Check(AdbOn(&a, Msg(ADB_CLSE, 0, 1, 0)) == ADB_DO_DONE, "the command refused: done too");
    AdbStart(&a, 1);
    AdbOn(&a, Msg(ADB_AUTH, ADB_AUTH_TOKEN, 0, ADB_TOKEN));
    Check(AdbOn(&a, Msg(ADB_CNXN, ADB_VERSION, ADB_MAXDATA, 10)) == ADB_DO_OPEN, "the signature known: the command");
    AdbStart(&a, 1);
    AdbOn(&a, Msg(ADB_AUTH, ADB_AUTH_TOKEN, 0, ADB_TOKEN));
    AdbOn(&a, Msg(ADB_AUTH, ADB_AUTH_TOKEN, 0, ADB_TOKEN));
    Check(AdbOn(&a, Msg(ADB_AUTH, ADB_AUTH_TOKEN, 0, ADB_TOKEN)) == ADB_DO_FAIL && a.state == ADB_FAILED,
          "a token after the key: refused");
    Check(AdbOn(&a, Msg(ADB_CNXN, ADB_VERSION, ADB_MAXDATA, 10)) == ADB_DO_WAIT, "after a refusal: nothing");
    AdbStart(&a, 1);
    Check(AdbOn(&a, Msg(ADB_AUTH, ADB_AUTH_SIGNATURE, 0, ADB_TOKEN)) == ADB_DO_WAIT && a.state == ADB_HELLO,
          "an AUTH not a token: nothing");
    Check(AdbOn(&a, Msg(ADB_OKAY, 5, 1, 0)) == ADB_DO_WAIT && a.state == ADB_HELLO, "OKAY before the command: nothing");
    Check(AdbOn(&a, Msg(ADB_CLSE, 5, 1, 0)) == ADB_DO_WAIT && a.state == ADB_HELLO, "CLSE before the command: nothing");
}

int main()
{
    TestHeader();
    TestPubKey();
    TestKey();
    TestSign();
    TestSession();
    printf("test_adb: %d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
