// rndis.cpp - see rndis.h.
#include "rndis.h"
#include <string.h>

enum {
    MSG_PACKET = 1, MSG_INIT = 2, MSG_QUERY = 4, MSG_SET = 5, MSG_KEEPALIVE = 8,
    MSG_DONE = 0x80000000u,   // added to a request's type in its answer
    kEthLen = 14, kIpLen = 20, kUdpLen = 8, kBootLen = 240,   // (the last with the magic cookie)
    kDhcpAt = kEthLen + kIpLen + kUdpLen, kDataHeader = 44
};

static void Put32(u8* p, u32 v)
{
    p[0] = (u8)v;
    p[1] = (u8)(v >> 8);
    p[2] = (u8)(v >> 16);
    p[3] = (u8)(v >> 24);
}

u32 RndisGet32(const u8* p)
{
    return p[0] | p[1] << 8 | p[2] << 16 | (u32)p[3] << 24;
}
#define Get32 RndisGet32

// The header of a message: its type, length and id (the first words of all but data), the rest zero
static bool Begin(u8* out, int cap, int size, u32 type, u32 req)
{
    if (cap < size)
        return false;
    memset(out, 0, size);
    Put32(out, type);
    Put32(out + 4, size);
    Put32(out + 8, req);
    return true;
}

// An answer to request req of that type, whole within its first "least" bytes
static bool AnswerTo(const u8* m, int n, int least, u32 type, u32 req)
{
    return n >= least && Get32(m) == (type | MSG_DONE) && Get32(m + 8) == req;
}

static bool Answer(const u8* m, int n, int least, u32 type, u32 req)   // ... with a status of success
{
    return AnswerTo(m, n, least, type, req) && Get32(m + 12) == 0;
}

int RndisInit(u8* out, int cap, u32 req, u32 maxTransfer)
{
    if (!Begin(out, cap, 24, MSG_INIT, req))
        return 0;
    Put32(out + 12, 1);   // version 1.0
    Put32(out + 20, maxTransfer);
    return 24;
}

bool RndisInitDone(const u8* m, int n, u32 req, RndisInitInfo* info)
{
    if (!Answer(m, n, 52, MSG_INIT, req))
        return false;
    info->major = Get32(m + 16);
    info->minor = Get32(m + 20);
    info->medium = Get32(m + 28);
    info->maxPackets = Get32(m + 32);
    info->maxTransfer = Get32(m + 36);
    info->align = Get32(m + 40);
    return true;
}

int RndisQuery(u8* out, int cap, u32 req, u32 oid)
{
    if (!Begin(out, cap, 28, MSG_QUERY, req))
        return 0;
    Put32(out + 12, oid);
    return 28;
}

// A piece of off bytes from m + base, len long, within n >= base (the lengths come from the other side: 32 bits, any value)
static bool Within(int n, int base, u32 off, u32 len)
{
    u32 room = (u32)n - base;
    return off <= room && len <= room - off;
}

bool RndisQueryDone(const u8* m, int n, u32 req, const u8** data, int* len)
{
    if (!Answer(m, n, 24, MSG_QUERY, req))
        return false;
    u32 size = Get32(m + 16), off = Get32(m + 20);
    if (!Within(n, 8, off, size))
        return false;
    *data = m + 8 + off;
    *len = (int)size;
    return true;
}

int RndisSet32(u8* out, int cap, u32 req, u32 oid, u32 value)
{
    if (!Begin(out, cap, 32, MSG_SET, req))
        return 0;
    Put32(out + 12, oid);
    Put32(out + 16, 4);
    Put32(out + 20, 20);   // (from the id's field: right after this header)
    Put32(out + 28, value);
    return 32;
}

bool RndisSetDone(const u8* m, int n, u32 req)
{
    return Answer(m, n, 16, MSG_SET, req);
}

int RndisKeepAliveDone(u8* out, int cap, const u8* m, int n)
{
    if (n < 12 || Get32(m) != MSG_KEEPALIVE || !Begin(out, cap, 16, MSG_KEEPALIVE | MSG_DONE, Get32(m + 8)))
        return 0;
    return 16;
}

bool RndisAnswers(const u8* msg, const u8* m, int n)
{
    return AnswerTo(m, n, 12, Get32(msg), Get32(msg + 8));
}

int RndisWrap(u8* out, int cap, const u8* frame, int len)
{
    int size = kDataHeader + len;
    if (!Begin(out, cap, size, MSG_PACKET, 36))   // (the third word here: the data's offset from it)
        return 0;
    Put32(out + 12, len);
    memcpy(out + kDataHeader, frame, len);
    return size;
}

bool RndisNext(const u8* m, int n, int* pos, const u8** frame, int* len)
{
    while (*pos + 16 <= n) {
        const u8* p = m + *pos;
        u32 size = Get32(p + 4), off = Get32(p + 8), data = Get32(p + 12);
        if (size < 16 || size > (u32)(n - *pos))
            return false;
        *pos += (int)size;
        if (Get32(p) != MSG_PACKET)
            continue;
        if (!Within((int)size, 8, off, data))
            return false;
        *frame = p + 8 + off;
        *len = (int)data;
        return true;
    }
    return false;
}

// ---- DHCP

static void Checksum(u8* ip)
{
    u32 s = 0;
    for (int i = 0; i < kIpLen; i += 2)
        s += ip[i] << 8 | ip[i + 1];
    s = (s & 0xFFFF) + (s >> 16);
    s = ~(s + (s >> 16)) & 0xFFFF;
    ip[10] = (u8)(s >> 8);
    ip[11] = (u8)s;
}

int DhcpDiscover(u8* out, int cap, const u8* mac, u32 xid)
{
    static const u8 kOptions[] = { 53, 1, 1,                 // DISCOVER
                                   55, 4, 1, 3, 6, 54,       // asks: the mask, the router, DNS, the server
                                   255 };
    int size = kDhcpAt + kBootLen + (int)sizeof(kOptions);
    if (cap < size)
        return 0;
    memset(out, 0, size);
    memset(out, 0xFF, 6);
    memcpy(out + 6, mac, 6);
    out[12] = 8;
    u8* ip = out + kEthLen;
    int ipLen = size - kEthLen, udpLen = ipLen - kIpLen;
    ip[0] = 0x45;
    ip[2] = (u8)(ipLen >> 8);
    ip[3] = (u8)ipLen;
    ip[8] = 64;
    ip[9] = 17;
    memset(ip + 16, 0xFF, 4);
    Checksum(ip);
    u8* udp = ip + kIpLen;
    udp[1] = 68;
    udp[3] = 67;
    udp[4] = (u8)(udpLen >> 8);
    udp[5] = (u8)udpLen;
    u8* b = udp + kUdpLen;
    b[0] = 1;
    b[1] = 1;
    b[2] = 6;
    b[4] = (u8)(xid >> 24);
    b[5] = (u8)(xid >> 16);
    b[6] = (u8)(xid >> 8);
    b[7] = (u8)xid;
    b[10] = 0x80;   // (no address yet: answer by broadcast)
    memcpy(b + 28, mac, 6);
    memcpy(b + 236, "\x63\x82\x53\x63", 4);
    memcpy(b + kBootLen, kOptions, sizeof(kOptions));
    return size;
}

// A reply to port 68 of that xid, with the cookie
static bool IsReply(const u8* f, int len, u32 xid)
{
    if (len < kDhcpAt + kBootLen || f[12] != 8 || f[13] != 0 || f[kEthLen + 9] != 17)
        return false;
    const u8* udp = f + kEthLen + kIpLen, *b = udp + kUdpLen;
    u32 id = (u32)b[4] << 24 | b[5] << 16 | b[6] << 8 | b[7];
    return udp[2] == 0 && udp[3] == 68 && b[0] == 2 && id == xid && !memcmp(b + 236, "\x63\x82\x53\x63", 4);
}

static u8* LeaseField(DhcpLease* l, int option)
{
    switch (option) {
    case 1:  return l->mask;
    case 3:  return l->gateway;
    case 6:  return l->dns;
    case 54: return l->server;
    }
    return NULL;
}

// One whole option (code, length, value) into the lease; *type - the message's
static void Option(const u8* o, DhcpLease* l, int* type)
{
    u8* to = LeaseField(l, o[0]);
    if (to && o[1] >= 4)
        memcpy(to, o + 2, 4);
    if (o[0] == 53 && o[1] == 1)
        *type = o[2];
}

// The options from o, n bytes: the message's type (0 none), the addresses into the lease
static int Options(const u8* o, int n, DhcpLease* l)
{
    int type = 0, p = 0;
    while (p < n && o[p] != 255) {
        if (o[p] == 0) {
            p++;
        } else if (p + 2 > n || p + 2 + o[p + 1] > n) {
            break;
        } else {
            Option(o + p, l, &type);
            p += 2 + o[p + 1];
        }
    }
    return type;
}

bool DhcpOffer(const u8* frame, int len, u32 xid, DhcpLease* lease)
{
    if (!IsReply(frame, len, xid))
        return false;
    memset(lease, 0, sizeof(*lease));
    const u8* b = frame + kDhcpAt;
    memcpy(lease->ip, b + 16, 4);
    return Options(b + kBootLen, len - kDhcpAt - kBootLen, lease) == 2;
}
