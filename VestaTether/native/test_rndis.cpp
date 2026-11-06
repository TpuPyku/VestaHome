// rndis.cpp against the RNDIS specification's layouts and RFC 2131 / 791 / 768.
#include "rndis.h"
#include <stdio.h>
#include <string.h>

static int s_checks, s_failed;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAIL: %s\n", what);
    }
}

static u32 Get(const u8* p, int at)
{
    p += at;
    return p[0] | p[1] << 8 | p[2] << 16 | (u32)p[3] << 24;
}

static void Put(u8* p, int at, u32 v)
{
    p += at;
    p[0] = (u8)v;
    p[1] = (u8)(v >> 8);
    p[2] = (u8)(v >> 16);
    p[3] = (u8)(v >> 24);
}

static int Sum16(const u8* p, int n, u32 start)   // the Internet checksum over n bytes: 0xFFFF when it agrees
{
    u32 s = start;
    for (int i = 0; i + 1 < n; i += 2)
        s += p[i] << 8 | p[i + 1];
    if (n & 1)
        s += p[n - 1] << 8;
    while (s >> 16)
        s = (s & 0xFFFF) + (s >> 16);
    return (int)s;
}

static void TestInit()
{
    u8 o[64];
    int n = RndisInit(o, 64, 7, 0x4000);
    Check(n == 24 && Get(o, 0) == 2 && Get(o, 4) == 24 && Get(o, 8) == 7 && Get(o, 12) == 1 && Get(o, 16) == 0 &&
          Get(o, 20) == 0x4000, "init: type 2, 24 bytes, id, version 1.0, the transfer's size");
    Check(RndisInit(o, 23, 7, 0x4000) == 0 && RndisInit(o, 24, 7, 0x4000) == 24, "init: no room - 0, exact - fits");
    u8 m[52];
    memset(m, 0, sizeof(m));
    Put(m, 0, 0x80000002); Put(m, 4, 52); Put(m, 8, 7); Put(m, 12, 0); Put(m, 16, 1); Put(m, 20, 5);
    Put(m, 24, 1); Put(m, 28, 0); Put(m, 32, 8); Put(m, 36, 0x4000); Put(m, 40, 3);
    RndisInitInfo i;
    memset(&i, 0, sizeof(i));
    Check(RndisInitDone(m, 52, 7, &i) && i.major == 1 && i.minor == 5 && i.medium == 0 && i.maxPackets == 8 &&
          i.maxTransfer == 0x4000 && i.align == 3, "init done: its fields");
    Check(!RndisInitDone(m, 52, 8, &i), "init done: another request");
    Check(!RndisInitDone(m, 51, 7, &i), "init done: cut short");
    Put(m, 12, 1);
    Check(!RndisInitDone(m, 52, 7, &i), "init done: a failure status");
    Put(m, 12, 0);
    Put(m, 0, 0x80000004);
    Check(!RndisInitDone(m, 52, 7, &i), "init done: another message");
}

static void TestQuery()
{
    u8 o[64];
    int n = RndisQuery(o, 64, 9, RNDIS_OID_CURRENT_ADDRESS);
    Check(n == 28 && Get(o, 0) == 4 && Get(o, 4) == 28 && Get(o, 8) == 9 && Get(o, 12) == 0x01010102 && Get(o, 16) == 0 &&
          Get(o, 20) == 0 && Get(o, 24) == 0, "query: type 4, 28 bytes, id, the OID, no data");
    Check(RndisQuery(o, 27, 9, 1) == 0 && RndisQuery(o, 28, 9, 1) == 28, "query: room");
    u8 m[40];
    memset(m, 0, sizeof(m));
    Put(m, 0, 0x80000004); Put(m, 4, 30); Put(m, 8, 9); Put(m, 12, 0); Put(m, 16, 6); Put(m, 20, 16);
    memcpy(m + 24, "\x02\x11\x22\x33\x44\x55", 6);
    const u8* d = NULL;
    int len = 0;
    Check(RndisQueryDone(m, 30, 9, &d, &len) && len == 6 && d == m + 24, "query done: the offset counts from the id's field");
    Check(!RndisQueryDone(m, 30, 8, &d, &len), "query done: another request");
    Check(!RndisQueryDone(m, 29, 9, &d, &len), "query done: the data past the bytes got");
    Check(!RndisQueryDone(m, 23, 9, &d, &len), "query done: no header");
    Put(m, 16, 0x7FFFFFFF);
    Check(!RndisQueryDone(m, 30, 9, &d, &len), "query done: a huge length");
    Put(m, 16, 6);
    Put(m, 20, 0xFFFFFFF0);
    Check(!RndisQueryDone(m, 30, 9, &d, &len), "query done: a huge offset");
    Put(m, 20, 16);
    Put(m, 12, 0xC0000001);
    Check(!RndisQueryDone(m, 30, 9, &d, &len), "query done: a failure status");
    Put(m, 12, 0);
    Put(m, 0, 0x80000002);
    Check(!RndisQueryDone(m, 30, 9, &d, &len), "query done: another message");
}

static void TestSet()
{
    u8 o[64];
    int n = RndisSet32(o, 64, 3, RNDIS_OID_PACKET_FILTER, RNDIS_FILTER_NORMAL);
    Check(n == 32 && Get(o, 0) == 5 && Get(o, 4) == 32 && Get(o, 8) == 3 && Get(o, 12) == 0x0001010E && Get(o, 16) == 4 &&
          Get(o, 20) == 20 && Get(o, 24) == 0 && Get(o, 28) == 0x0B, "set: type 5, the OID, 4 bytes at 8 + 20");
    Check(RndisSet32(o, 31, 3, 1, 1) == 0 && RndisSet32(o, 32, 3, 1, 1) == 32, "set: room");
    u8 m[16];
    Put(m, 0, 0x80000005); Put(m, 4, 16); Put(m, 8, 3); Put(m, 12, 0);
    Check(RndisSetDone(m, 16, 3), "set done");
    Check(!RndisSetDone(m, 15, 3), "set done: cut short");
    Check(!RndisSetDone(m, 16, 4), "set done: another request");
    Put(m, 12, 1);
    Check(!RndisSetDone(m, 16, 3), "set done: a failure");
    Put(m, 12, 0);
    Put(m, 0, 0x80000006);
    Check(!RndisSetDone(m, 16, 3), "set done: another message");
    u8 asked[32];
    RndisQuery(asked, 32, 5, 1);
    Put(m, 0, 0x80000004); Put(m, 4, 16); Put(m, 8, 5); Put(m, 12, 1);
    Check(RndisAnswers(asked, m, 12), "answers: its type and id, whatever the status");
    Check(!RndisAnswers(asked, m, 11), "answers: cut short");
    Put(m, 8, 6);
    Check(!RndisAnswers(asked, m, 16), "answers: an old answer of another id");
    Put(m, 8, 5); Put(m, 0, 4);
    Check(!RndisAnswers(asked, m, 16), "answers: the request's own type, not its answer");
    Put(m, 0, 7);
    Check(!RndisAnswers(asked, m, 16), "answers: a status note");
    u8 k[12];
    Put(k, 0, 8); Put(k, 4, 12); Put(k, 8, 44);
    Check(RndisKeepAliveDone(o, 64, k, 12) == 16 && Get(o, 0) == 0x80000008 && Get(o, 4) == 16 && Get(o, 8) == 44 &&
          Get(o, 12) == 0, "keep-alive: answered with its id");
    Check(RndisKeepAliveDone(o, 15, k, 12) == 0 && RndisKeepAliveDone(o, 16, k, 12) == 16, "keep-alive: room");
    Check(RndisKeepAliveDone(o, 64, k, 11) == 0, "keep-alive: cut short");
    Put(k, 0, 7);
    Check(RndisKeepAliveDone(o, 64, k, 12) == 0, "keep-alive: another message");
}

static void TestData()
{
    u8 o[128], f[5] = { 1, 2, 3, 4, 5 };
    int n = RndisWrap(o, 128, f, 5);
    Check(n == 49 && Get(o, 0) == 1 && Get(o, 4) == 49 && Get(o, 8) == 36 && Get(o, 12) == 5 && !memcmp(o + 44, f, 5),
          "wrap: type 1, the frame at 8 + 36");
    bool zero = true;
    for (int i = 16; i < 44; i++)
        zero = zero && o[i] == 0;
    Check(zero, "wrap: the rest of the header zero");
    Check(RndisWrap(o, 48, f, 5) == 0 && RndisWrap(o, 49, f, 5) == 49, "wrap: room");
    // two messages in a transfer
    u8 t[200];
    int a = RndisWrap(t, 200, f, 5), b = RndisWrap(t + a, 200 - a, f, 3);
    int pos = 0, len = 0;
    const u8* fr = NULL;
    Check(RndisNext(t, a + b, &pos, &fr, &len) && len == 5 && fr == t + 44 && pos == a, "next: the first");
    Check(RndisNext(t, a + b, &pos, &fr, &len) && len == 3 && fr == t + a + 44 && pos == a + b, "next: the second");
    Check(!RndisNext(t, a + b, &pos, &fr, &len), "next: the end");
    pos = 0;
    Check(!RndisNext(t, 43, &pos, &fr, &len), "next: no whole header");
    Check(RndisNext(t, 49, &pos, &fr, &len), "next: exactly one");
    pos = 0;
    Check(!RndisNext(t, 48, &pos, &fr, &len), "next: the frame past the bytes got");
    u8 bad[60];
    memset(bad, 0, sizeof(bad));
    Put(bad, 0, 1); Put(bad, 4, 0); Put(bad, 8, 36); Put(bad, 12, 1);
    pos = 0;
    Check(!RndisNext(bad, 60, &pos, &fr, &len), "next: no length - no endless loop");
    Put(bad, 4, 60); Put(bad, 8, 0xFFFFFFF0); Put(bad, 12, 1);
    pos = 0;
    Check(!RndisNext(bad, 60, &pos, &fr, &len), "next: a huge offset");
    Put(bad, 8, 36); Put(bad, 12, 0x7FFFFFFF);
    pos = 0;
    Check(!RndisNext(bad, 60, &pos, &fr, &len), "next: a huge frame");
    Put(bad, 12, 1); Put(bad, 4, 1000);
    pos = 0;
    Check(!RndisNext(bad, 60, &pos, &fr, &len), "next: a message past the bytes got");
    Put(bad, 4, 60); Put(bad, 0, 7);
    pos = 0;
    Check(!RndisNext(bad, 60, &pos, &fr, &len) && pos == 60, "next: not data (its length skipped)");
}

static void TestDiscover()
{
    u8 o[600], mac[6] = { 2, 0x11, 0x22, 0x33, 0x44, 0x55 };
    int n = DhcpDiscover(o, 600, mac, 0xA1B2C3D4);
    Check(n > 14 + 20 + 8 + 240, "discover: a whole frame");
    Check(!memcmp(o, "\xFF\xFF\xFF\xFF\xFF\xFF", 6) && !memcmp(o + 6, mac, 6) && o[12] == 8 && o[13] == 0, "discover: Ethernet, broadcast, IPv4");
    const u8* ip = o + 14;
    Check(ip[0] == 0x45 && ip[9] == 17 && ip[8] > 0, "discover: IP v4, UDP, a TTL");
    Check((ip[2] << 8 | ip[3]) == n - 14, "discover: the IP length");
    Check(!memcmp(ip + 12, "\0\0\0\0", 4) && !memcmp(ip + 16, "\xFF\xFF\xFF\xFF", 4), "discover: from nowhere to everybody");
    Check(Sum16(ip, 20, 0) == 0xFFFF, "discover: the IP checksum");
    const u8* udp = ip + 20;
    Check((udp[0] << 8 | udp[1]) == 68 && (udp[2] << 8 | udp[3]) == 67, "discover: UDP 68 to 67");
    Check((udp[4] << 8 | udp[5]) == n - 34, "discover: the UDP length");
    Check(udp[6] == 0 && udp[7] == 0, "discover: no UDP checksum (allowed on IPv4)");
    const u8* b = udp + 8;
    Check(b[0] == 1 && b[1] == 1 && b[2] == 6, "discover: BOOTREQUEST, Ethernet, 6");
    Check(b[4] == 0xA1 && b[5] == 0xB2 && b[6] == 0xC3 && b[7] == 0xD4, "discover: xid");
    Check(b[10] == 0x80, "discover: asks for a broadcast answer");
    Check(!memcmp(b + 28, mac, 6), "discover: chaddr");
    Check(!memcmp(b + 236, "\x63\x82\x53\x63", 4), "discover: the magic cookie");
    Check(b[240] == 53 && b[241] == 1 && b[242] == 1, "discover: option 53 = 1");
    Check(o[n - 1] == 255, "discover: the options' end");
    Check(DhcpDiscover(o, n - 1, mac, 1) == 0 && DhcpDiscover(o, n, mac, 1) == n, "discover: room");
}

static int MakeOffer(u8* o, u32 xid, int type, bool withOpts)
{
    memset(o, 0, 400);
    o[12] = 8; o[13] = 0;
    u8* ip = o + 14;
    ip[0] = 0x45; ip[9] = 17;
    u8* udp = ip + 20;
    udp[1] = 67; udp[3] = 68;
    u8* b = udp + 8;
    b[0] = 2; b[1] = 1; b[2] = 6;
    b[4] = (u8)(xid >> 24); b[5] = (u8)(xid >> 16); b[6] = (u8)(xid >> 8); b[7] = (u8)xid;
    b[16] = 192; b[17] = 168; b[18] = 42; b[19] = 100;
    memcpy(b + 236, "\x63\x82\x53\x63", 4);
    int p = 240;
    b[p++] = 53; b[p++] = 1; b[p++] = (u8)type;
    if (withOpts) {
        b[p++] = 0;   // a pad
        u8 sv[6] = { 54, 4, 192, 168, 42, 129 }, rt[6] = { 3, 4, 192, 168, 42, 1 }, dn[6] = { 6, 4, 8, 8, 4, 4 }, ms[6] = { 1, 4, 255, 255, 255, 0 };
        memcpy(b + p, sv, 6); p += 6;
        memcpy(b + p, rt, 6); p += 6;
        memcpy(b + p, dn, 6); p += 6;
        memcpy(b + p, ms, 6); p += 6;
    }
    b[p++] = 255;
    return 14 + 20 + 8 + p;
}

static void TestOffer()
{
    u8 f[400];
    DhcpLease l;
    int n = MakeOffer(f, 0x01020304, 2, true);
    memset(&l, 0, sizeof(l));
    Check(DhcpOffer(f, n, 0x01020304, &l), "offer: taken");
    Check(!memcmp(l.ip, "\xC0\xA8\x2A\x64", 4), "offer: yiaddr");
    Check(!memcmp(l.server, "\xC0\xA8\x2A\x81", 4), "offer: the server");
    Check(!memcmp(l.gateway, "\xC0\xA8\x2A\x01", 4), "offer: the gateway");
    Check(!memcmp(l.dns, "\x08\x08\x04\x04", 4), "offer: DNS");
    Check(!memcmp(l.mask, "\xFF\xFF\xFF\x00", 4), "offer: the mask");
    Check(!DhcpOffer(f, n, 0x01020305, &l), "offer: another xid");
    Check(!DhcpOffer(f, 14 + 20 + 8 + 239, 0x01020304, &l), "offer: no options");
    n = MakeOffer(f, 1, 5, true);
    Check(!DhcpOffer(f, n, 1, &l), "offer: an ACK is not one");
    n = MakeOffer(f, 1, 2, false);
    memset(&l, 0xEE, sizeof(l));
    Check(DhcpOffer(f, n, 1, &l) && !memcmp(l.gateway, "\0\0\0\0", 4) && !memcmp(l.dns, "\0\0\0\0", 4), "offer: without options - empty");
    n = MakeOffer(f, 1, 2, true);
    f[12] = 0x86;
    Check(!DhcpOffer(f, n, 1, &l), "offer: not IPv4");
    f[12] = 8; f[14 + 9] = 6;
    Check(!DhcpOffer(f, n, 1, &l), "offer: not UDP");
    f[14 + 9] = 17; f[14 + 20 + 3] = 67;
    Check(!DhcpOffer(f, n, 1, &l), "offer: not to port 68");
    f[14 + 20 + 3] = 68; f[42 + 236] = 0;
    Check(!DhcpOffer(f, n, 1, &l), "offer: no cookie");
    f[42 + 236] = 0x63; f[42] = 1;
    Check(!DhcpOffer(f, n, 1, &l), "offer: not a reply");
    // options past the frame's end must not be read
    n = MakeOffer(f, 1, 2, false);
    f[n - 1] = 54; f[n] = 200;
    Check(!DhcpOffer(f, n, 1, &l) || !memcmp(l.server, "\0\0\0\0", 4), "offer: a cut option left out");
    // a 4-byte option given short
    n = MakeOffer(f, 1, 2, false);
    f[n - 1] = 3; f[n] = 2; f[n + 1] = 9; f[n + 2] = 9; f[n + 3] = 255;
    memset(&l, 0, sizeof(l));
    Check(DhcpOffer(f, n + 4, 1, &l) && !memcmp(l.gateway, "\0\0\0\0", 4), "offer: a short address option left out");
}

static void TestEdges()
{
    u8 w[4] = { 0x01, 0x02, 0x80, 0x90 };
    Check(RndisGet32(w) == 0x90800201, "get32: little endian, each byte in its place");
    // a query's answer of no data, its offset at the very end
    u8 m[24];
    memset(m, 0, sizeof(m));
    Put(m, 0, 0x80000004); Put(m, 4, 24); Put(m, 8, 9); Put(m, 16, 0); Put(m, 20, 16);
    const u8* d;
    int len = 7;
    Check(RndisQueryDone(m, 24, 9, &d, &len) && len == 0 && d == m + 24, "query done: no data, exactly the header");
    // data up to the very last byte
    u8 q[30];
    memset(q, 0, sizeof(q));
    Put(q, 0, 0x80000004); Put(q, 4, 30); Put(q, 8, 9); Put(q, 16, 6); Put(q, 20, 16);
    Check(RndisQueryDone(q, 30, 9, &d, &len) && len == 6, "query done: data to the last byte");
    // a 16 byte message that is not data, then a data one
    u8 t2[80], f[2] = { 1, 2 };
    memset(t2, 0, sizeof(t2));
    Put(t2, 0, 7); Put(t2, 4, 16);
    int k = RndisWrap(t2 + 16, 64, f, 2), pos = 0;
    const u8* fr;
    Check(RndisNext(t2, 16 + k, &pos, &fr, &len) && len == 2 && fr == t2 + 16 + 44, "next: a short other message passed over");
    pos = 0;
    Check(!RndisNext(t2, 16, &pos, &fr, &len) && pos == 16, "next: exactly one short message, no data");
    pos = 16;
    Put(t2 + 16, 4, 60);
    Check(!RndisNext(t2, 16 + 40, &pos, &fr, &len), "next: a later message past the bytes got");
    u8 s15[32];
    memset(s15, 0, sizeof(s15));
    Put(s15, 0, 1); Put(s15, 4, 15); Put(s15, 8, 0); Put(s15, 12, 0);
    pos = 0;
    Check(!RndisNext(s15, 32, &pos, &fr, &len), "next: a message under its header's size");
    // what it asks for, and where the options end
    u8 o[600], mac[7] = { 2, 3, 4, 5, 6, 7, 0xAA };
    int n = DhcpDiscover(o, 600, mac, 1);
    const u8* b = o + 42;
    Check(n == 42 + 240 + 10 && !memcmp(b + 243, "\x37\x04\x01\x03\x06\x36\xFF", 7), "discover: the list asked for");
    Check(b[34] == 0 && o[14 + 8] == 64, "discover: chaddr's six only, the TTL");
    // options cut by the frame's end
    u8 fr2[420];
    n = MakeOffer(fr2, 1, 2, false);
    DhcpLease l;
    fr2[n - 1] = 54;   // (code only, where the end was)
    fr2[n] = 4; fr2[n + 1] = 9; fr2[n + 2] = 9; fr2[n + 3] = 9; fr2[n + 4] = 9; fr2[n + 5] = 255;
    memset(&l, 0, sizeof(l));
    Check(DhcpOffer(fr2, n, 1, &l) && !memcmp(l.server, "\0\0\0\0", 4), "offer: an option's code at the very end");
    Check(DhcpOffer(fr2, n + 1, 1, &l) && !memcmp(l.server, "\0\0\0\0", 4), "offer: its length at the end, no value");
    Check(DhcpOffer(fr2, n + 4, 1, &l) && !memcmp(l.server, "\0\0\0\0", 4), "offer: its value one byte short");
    Check(DhcpOffer(fr2, n + 5, 1, &l) && !memcmp(l.server, "\x09\x09\x09\x09", 4), "offer: its value whole, nothing after");
    // nothing read past the end mark
    n = MakeOffer(fr2, 1, 2, false);
    fr2[n] = 53; fr2[n + 1] = 1; fr2[n + 2] = 5;
    Check(DhcpOffer(fr2, n + 3, 1, &l), "offer: what follows the end mark is not read");
    fr2[n] = 0; fr2[n + 1] = 53; fr2[n + 2] = 1; fr2[n + 3] = 5;
    Check(DhcpOffer(fr2, n + 4, 1, &l), "offer: nor a pad and an option after it");
    // another one-byte option after the type; an address followed by another
    n = MakeOffer(fr2, 1, 2, false);
    u8 more[] = { 12, 1, 5, 6, 4, 8, 8, 4, 4, 3, 4, 10, 0, 0, 1, 255 };
    memcpy(fr2 + n - 1, more, sizeof(more));
    memset(&l, 0, sizeof(l));
    Check(DhcpOffer(fr2, n - 1 + (int)sizeof(more), 1, &l), "offer: a one-byte option of another code is not the type");
    Check(l.dns[0] == 8 && l.gateway[0] == 10, "offer: an address's four bytes only");
    Check(!memcmp(l.gateway, "\x0A\0\0\x01", 4) && l.dns[0] == 8, "offer: the next field untouched");
    // the frame's exact least size has no options
    n = MakeOffer(fr2, 1, 2, false);
    Check(!DhcpOffer(fr2, 42 + 240, 1, &l), "offer: the header alone");
    fr2[13] = 1;
    Check(!DhcpOffer(fr2, n, 1, &l), "offer: the EtherType's low byte too");
    fr2[13] = 0; fr2[42 + 16] = 10; fr2[42 + 17] = 21; fr2[42 + 18] = 132; fr2[42 + 19] = 143; fr2[42 + 20] = 200;
    memset(&l, 0, sizeof(l));
    Check(DhcpOffer(fr2, n, 1, &l) && l.ip[3] == 143 && l.server[0] == 0, "offer: yiaddr's four only");
}

int main()
{
    TestInit();
    TestEdges();
    TestQuery();
    TestSet();
    TestData();
    TestDiscover();
    TestOffer();
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
