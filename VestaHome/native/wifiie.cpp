// wifiie.cpp - the beacon's and the association's IEs as log text. See wifiie.h.
#include "wifiie.h"
#include <stdio.h>
#include <string.h>

enum { kBssHead = 116, kFixedIes = 12, kAssocHead = 40, IE_RSN = 48, IE_VENDOR = 221, IE_EXT = 255 };

struct Out { char* p; int cap, n; };

static void Put(Out* o, const char* s)
{
    while (*s && o->n < o->cap - 1)
        o->p[o->n++] = *s++;
    o->p[o->n] = 0;
}

static void PutNum(Out* o, const char* fmt, unsigned v)
{
    char s[16];
    sprintf(s, fmt, v);
    Put(o, s);
}

static unsigned Le16(const BYTE* p)
{
    return p[0] | p[1] << 8;
}

static ULONG Le32(const BYTE* p)   // (by bytes: a driver's entry need not be aligned, ARM faults on it)
{
    return Le16(p) | (ULONG)Le16(p + 2) << 16;
}

// A cipher or key management suite (00-0F-AC for RSN, 00-50-F2 for WPA: the same numbers): its name, else its
// number, else the vendor's OUI and number.
static void PutSuite(Out* o, const BYTE* s, const BYTE* oui, bool akm)
{
    static const char* kCipher[] = { "-", "WEP40", "TKIP", "?3", "CCMP", "WEP104", "BIP", "?7", "GCMP" };
    static const char* kAkm[] = { "-", "802.1X", "PSK", "FT-802.1X", "FT-PSK", "802.1X-256", "PSK-256", "TDLS",
                                  "SAE", "FT-SAE" };
    if (memcmp(s, oui, 3)) {
        PutNum(o, "%06X", s[0] << 16 | s[1] << 8 | s[2]);
        PutNum(o, "-%u", s[3]);
    } else if (!akm && s[3] < sizeof(kCipher) / sizeof(kCipher[0]))
        Put(o, kCipher[s[3]]);
    else if (akm && s[3] < sizeof(kAkm) / sizeof(kAkm[0]))
        Put(o, kAkm[s[3]]);
    else
        PutNum(o, "%u", s[3]);
}

// A list of suites (its count, 2 bytes, then 4 each) at *p: false when it runs past end.
static bool PutSuites(Out* o, const BYTE** p, const BYTE* end, const BYTE* oui, bool akm)
{
    if (end - *p < 2)
        return false;
    unsigned n = Le16(*p);
    if ((unsigned)(end - *p - 2) < n * 4)
        return false;
    *p += 2;
    for (unsigned i = 0; i < n; i++, *p += 4) {
        if (i)
            Put(o, ",");
        PutSuite(o, *p, oui, akm);
    }
    return true;
}

static void PutCaps(Out* o, unsigned cap)
{
    PutNum(o, " cap %04X", cap);
    if (cap & 0x80)
        Put(o, " MFPC");
    if (cap & 0x40)
        Put(o, " MFPR");
}

// RSN (and WPA, the same after its OUI and type): version, group cipher, pairwise ciphers, key managements,
// capabilities - each part may be missing from there on (its default meant), none may be cut (false).
static bool RsnParts(Out* o, const BYTE* p, const BYTE* end, const BYTE* oui)
{
    if (Le16(p) != 1)
        PutNum(o, " v%u", Le16(p));
    p += 2;
    if (end - p >= 4) {
        Put(o, " ");
        PutSuite(o, p, oui, false);
        p += 4;
    }
    if (p < end) {
        Put(o, "/");
        if (!PutSuites(o, &p, end, oui, false))
            return false;
    }
    if (p < end) {
        Put(o, " ");
        if (!PutSuites(o, &p, end, oui, true))
            return false;
    }
    if (end - p >= 2)
        PutCaps(o, Le16(p));
    return true;
}

static void PutRsn(Out* o, const char* name, const BYTE* p, int len, const BYTE* oui)
{
    Put(o, name);
    Out tmp = *o;
    if (len >= 2 && RsnParts(o, p, p + len, oui)) {
        Put(o, "; ");
        return;
    }
    *o = tmp;   // what was put of it goes (Put ends the text again)
    Put(o, " bad; ");
}

static const BYTE kRsnOui[3] = { 0x00, 0x0F, 0xAC }, kMsOui[3] = { 0x00, 0x50, 0xF2 };

static bool IeFits(const BYTE* p, const BYTE* end)
{
    return end - p >= 2 && end - p - 2 >= p[1];
}

static void PutSecurity(Out* o, const BYTE* p, const BYTE* end)
{
    for (; IeFits(p, end); p += 2 + p[1])
        if (p[0] == IE_RSN)
            PutRsn(o, "RSN", p + 2, p[1], kRsnOui);
        else if (p[0] == IE_VENDOR && p[1] >= 4 && !memcmp(p + 2, kMsOui, 3) && p[5] == 1)
            PutRsn(o, "WPA", p + 6, p[1] - 4, kMsOui);
}

// An IE's id; an extension's own id (255:35 - HE), a vendor's OUI or Microsoft's type (221:WMM)
static void PutIeId(Out* o, const BYTE* p)
{
    static const char* kMsType[] = { "?0", "WPA", "WMM", "?3", "WPS" };
    PutNum(o, " %u", p[0]);
    if (p[0] == IE_EXT && p[1] >= 1)
        PutNum(o, ":%u", p[2]);
    if (p[0] != IE_VENDOR || p[1] < 4)
        return;
    if (!memcmp(p + 2, kMsOui, 3) && p[5] < 5)
        Put(o, ":"), Put(o, kMsType[p[5]]);
    else
        PutNum(o, ":%06X", p[2] << 16 | p[3] << 8 | p[4]);
}

void IeText(const BYTE* ies, int len, char* out, int cap)
{
    Out o = { out, cap, 0 };
    const BYTE *p = ies, *end = ies + len;
    PutSecurity(&o, ies, end);   // the security first, then every IE's id
    Put(&o, "IE");
    for (; p < end; p += 2 + p[1]) {
        if (!IeFits(p, end)) {
            Put(&o, " cut");
            break;
        }
        PutIeId(&o, p);
    }
}

// One NDIS_WLAN_BSSID_EX (room bytes left): its size, 0 when it does not fit.
static unsigned BssOne(const BYTE* e, int room, BssEntry* b)
{
    if (room < kBssHead)
        return 0;
    unsigned size = Le32(e), ies = Le32(e + 112);
    if (size < kBssHead || size > (unsigned)room || ies > size - kBssHead)
        return 0;
    memcpy(b->mac, e + 4, 6);
    unsigned sl = Le32(e + 12);
    sl = sl < 32 ? sl : 32;
    memcpy(b->ssid, e + 16, sl);
    b->ssid[sl] = 0;
    b->rssi = (LONG)Le32(e + 52);
    b->ies = e + kBssHead + kFixedIes;
    b->ieLen = ies > kFixedIes ? (int)ies - kFixedIes : 0;
    return size;
}

int BssList(const BYTE* data, int len, BssEntry* out, int max)
{
    if (len < 4)
        return 0;
    unsigned items = Le32(data), size;
    int n = 0, pos = 4;
    for (; (unsigned)n < items && n < max && (size = BssOne(data + pos, len - pos, out + n)) != 0; n++)
        pos += size;
    return n;
}

// [off, off + n) inside len
static bool Inside(ULONG off, ULONG n, int len)
{
    return off <= (ULONG)len && n <= (ULONG)len - off;
}

bool AssocIes(const BYTE* data, int len, const BYTE** req, int* reqLen, const BYTE** resp, int* respLen,
              int* status)
{
    if (len < kAssocHead)
        return false;
    ULONG rl = Le32(data + 16), ro = Le32(data + 20);
    ULONG sl = Le32(data + 32), so = Le32(data + 36);
    if (!Inside(ro, rl, len) || !Inside(so, sl, len))
        return false;
    *req = data + ro;
    *reqLen = (int)rl;
    *resp = data + so;
    *respLen = (int)sl;
    *status = Le16(data + 28);
    return true;
}

// A list of RSN's suites (count, then 4 bytes each): is the one with this number there; moves past it. False: cut.
static bool RsnHas(const BYTE** p, const BYTE* end, BYTE number, bool* has)
{
    if (end - *p < 2)
        return false;
    int n = Le16(*p);
    *p += 2;
    if (n > (end - *p) / 4)
        return false;
    *has = false;
    for (int i = 0; i < n; i++, *p += 4)
        if (!memcmp(*p, kRsnOui, 3) && (*p)[3] == number)
            *has = true;
    return true;
}

bool IeWpa2Psk(const BYTE* ies, int len)
{
    enum { kCcmp = 4, kPsk = 2, kRsnHead = 6 };   // (the head: the version and the group's cipher)
    const BYTE* end = ies + len;
    for (const BYTE* e = ies; end - e >= 2 && e[1] <= end - e - 2; e += 2 + e[1]) {
        if (e[0] != IE_RSN || e[1] < kRsnHead)
            continue;
        const BYTE* p = e + 2 + kRsnHead;
        const BYTE* rsnEnd = e + 2 + e[1];
        bool ccmp, psk;
        return RsnHas(&p, rsnEnd, kCcmp, &ccmp) && RsnHas(&p, rsnEnd, kPsk, &psk) && ccmp && psk;
    }
    return false;
}

const wchar_t* EncText(DWORD status)
{
    static const wchar_t* kText[] = { L"WEP, ключ есть", L"без шифрования", L"WEP, ключа нет", L"не поддерживается",
                                      L"TKIP, ключ есть", L"TKIP, ключа нет", L"AES, ключ есть", L"AES, ключа нет" };
    return status < sizeof(kText) / sizeof(kText[0]) ? kText[status] : L"?";
}
