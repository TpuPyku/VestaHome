// test_wifiie.cpp - the beacon's and the association's IEs as log text (wifiie.cpp) on the PC (test_wifiie.bat).
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "wifiie.h"

static int s_failed, s_checks;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAIL: %s\n", what);
    }
}

static void Text(const BYTE* ies, int len, const char* want, const char* what)
{
    char out[160];
    memset(out, 'x', sizeof(out));
    IeText(ies, len, out, sizeof(out));
    bool ok = !strcmp(out, want);
    Check(ok, what);
    if (!ok)
        printf("  got  \"%s\"\n  want \"%s\"\n", out, want);
}

#define N(a) ((int)sizeof(a))

// RSN: version 1, group CCMP, 1 pairwise CCMP, 1 AKM PSK, capabilities 0x000C (an old phone, WPA2-Personal)
static const BYTE kWpa2[] = { 0, 3, 'A', 'P', '1', 1, 1, 0x82,
    48, 20, 1, 0, 0x00, 0x0F, 0xAC, 4, 1, 0, 0x00, 0x0F, 0xAC, 4, 1, 0, 0x00, 0x0F, 0xAC, 2, 0x0C, 0x00,
    221, 7, 0x00, 0x50, 0xF2, 2, 1, 1, 0 };
// WPA2/WPA3 transition: pairwise TKIP + CCMP, AKM PSK + SAE, MFPC; HE (255/35), RSNXE (244)
static const BYTE kMixed[] = { 48, 28, 1, 0, 0x00, 0x0F, 0xAC, 4, 2, 0, 0x00, 0x0F, 0xAC, 2, 0x00, 0x0F, 0xAC, 4,
    2, 0, 0x00, 0x0F, 0xAC, 2, 0x00, 0x0F, 0xAC, 8, 0x8C, 0x00, 255, 2, 35, 0, 244, 1, 0x20 };
// MFP required, an unknown AKM of another vendor, a cipher number not named
static const BYTE kOdd[] = { 48, 20, 1, 0, 0x00, 0x0F, 0xAC, 4, 1, 0, 0x00, 0x0F, 0xAC, 99, 1, 0, 0x00, 0x40, 0x96,
    1, 0xC0, 0x00 };
// WPA (vendor IE 00-50-F2 type 1): version 1, TKIP/TKIP, PSK; WPS (type 4); another vendor's IE
static const BYTE kWpa[] = { 221, 22, 0x00, 0x50, 0xF2, 1, 1, 0, 0x00, 0x50, 0xF2, 2, 1, 0, 0x00, 0x50, 0xF2, 2,
    1, 0, 0x00, 0x50, 0xF2, 2, 221, 5, 0x00, 0x50, 0xF2, 4, 0x10, 221, 4, 0x50, 0x6F, 0x9A, 2 };
// RSN with its version and group cipher alone (the rest defaulted, allowed); version 2
static const BYTE kShort[] = { 48, 6, 2, 0, 0x00, 0x0F, 0xAC, 2 };
// RSN claiming 3 pairwise ciphers, holding 1
static const BYTE kBad[] = { 48, 12, 1, 0, 0x00, 0x0F, 0xAC, 4, 3, 0, 0x00, 0x0F, 0xAC, 4 };
// an IE running past the data
static const BYTE kCut[] = { 0, 2, 'A', 'B', 3, 9, 1 };
// a WPA IE too short for its version, a vendor IE too short for its type
static const BYTE kStub[] = { 221, 4, 0x00, 0x50, 0xF2, 1, 221, 2, 0x00, 0x50 };
// RSN cut in the middle of the AKM list's count; then pairwise suites cut
static const BYTE kHalf[] = { 48, 13, 1, 0, 0x00, 0x0F, 0xAC, 4, 1, 0, 0x00, 0x0F, 0xAC, 4, 1 };
static const BYTE kHalf2[] = { 48, 9, 1, 0, 0x00, 0x0F, 0xAC, 4, 1, 0, 0x00 };
static const BYTE kCapHalf[] = { 48, 21, 1, 0, 0x00, 0x0F, 0xAC, 4, 1, 0, 0x00, 0x0F, 0xAC, 4, 1, 0, 0x00, 0x0F,
    0xAC, 2, 0x0C, 0x00, 0x00 };

// a cipher and AKMs at their tables' ends, a key management of the Wi-Fi Alliance (OUI 50-6F-9A)
static const BYTE kSuites[] = { 48, 26, 1, 0, 0x00, 0x0F, 0xAC, 8, 1, 0, 0x00, 0x0F, 0xAC, 9,
    3, 0, 0x00, 0x0F, 0xAC, 9, 0x00, 0x0F, 0xAC, 10, 0x50, 0x6F, 0x9A, 1 };
static const BYTE kNoPair[] = { 48, 8, 1, 0, 0x00, 0x0F, 0xAC, 4, 0, 0 };
static const BYTE kVer[] = { 48, 2, 1, 0 };
static const BYTE kOne[] = { 48, 1, 1 };
static const BYTE kEnds[] = { 255, 0, 255, 1, 35, 221, 4, 0x00, 0x50, 0xF2, 5, 0, 0 };
static const BYTE kOver[] = { 3, 3, 1, 2 };

static void TestIeText()
{
    Text(kWpa2, N(kWpa2), "RSN CCMP/CCMP PSK cap 000C; IE 0 1 48 221:WMM", "WPA2-Personal");
    Text(kMixed, N(kMixed), "RSN CCMP/TKIP,CCMP PSK,SAE cap 008C MFPC; IE 48 255:35 244", "WPA2/WPA3, MFPC");
    Text(kOdd, N(kOdd), "RSN CCMP/99 004096-1 cap 00C0 MFPC MFPR; IE 48", "MFPR, unknown suites");
    Text(kWpa, N(kWpa), "WPA TKIP/TKIP PSK; IE 221:WPA 221:WPS 221:506F9A", "WPA, WPS, a vendor");
    Text(kShort, N(kShort), "RSN v2 TKIP; IE 48", "RSN with its group alone");
    Text(kBad, N(kBad), "RSN bad; IE 48", "RSN's count past its length");
    Text(kCut, N(kCut), "IE 0 cut", "an IE past the data");
    Text(kStub, N(kStub), "WPA bad; IE 221:WPA 221", "short vendor IEs");
    Text(kHalf, N(kHalf), "RSN bad; IE 48", "RSN cut in the AKM count");
    Text(kHalf2, N(kHalf2), "RSN bad; IE 48", "RSN cut in the pairwise suites");
    Text(kCapHalf, N(kCapHalf), "RSN CCMP/CCMP PSK cap 000C; IE 48", "the PMKID count's half ignored");
    Text(kWpa2, 0, "IE", "nothing");
    Text(kSuites, N(kSuites), "RSN GCMP/9 FT-SAE,10,506F9A-1; IE 48", "the tables' ends, another vendor");
    Text(kNoPair, N(kNoPair), "RSN CCMP/; IE 48", "no pairwise ciphers");
    Text(kVer, N(kVer), "RSN; IE 48", "RSN's version alone");
    Text(kOne, N(kOne), "RSN bad; IE 48", "RSN of one byte");
    Text(kEnds, N(kEnds), "IE 255 255:35 221:0050F2 0", "an empty extension, MS type 5, an empty IE at the end");
    Text(kOver, N(kOver), "IE cut", "an IE one byte past the data");
    Text(kWpa2, 1, "IE cut", "a lone byte");
    char room[12];
    IeText(kWpa2, N(kWpa2), room, sizeof(room));
    Check(!strcmp(room, "RSN CCMP/CC"), "cut to its room");
}

// NDIS_WLAN_BSSID_EX: 116 bytes before its IEs (12 of them fixed)
static int PutBss(BYTE* p, const char* ssid, int rssi, const BYTE* ies, int ieLen, int pad)
{
    int len = 116 + 12 + ieLen + pad;
    memset(p, 0, len);
    *(ULONG*)p = len;
    for (int i = 0; i < 6; i++)
        p[4 + i] = (BYTE)(0xA0 + i);
    *(ULONG*)(p + 12) = (ULONG)strlen(ssid);
    memcpy(p + 16, ssid, strlen(ssid));
    *(LONG*)(p + 52) = rssi;
    *(ULONG*)(p + 112) = 12 + ieLen;
    memcpy(p + 128, ies, ieLen);
    return len;
}

static void TestBssList()
{
    static BYTE d[2048];
    int len = 4;
    len += PutBss(d + len, "MRNote13P", -57, kWpa2, N(kWpa2), 4);
    len += PutBss(d + len, "MiNote3", -60, kMixed, N(kMixed), 0);
    *(ULONG*)d = 2;
    BssEntry e[3];
    memset(e, 0, sizeof(e));
    Check(BssList(d, len, e, 3) == 2, "two entries");
    Check(!strcmp(e[0].ssid, "MRNote13P") && e[0].rssi == -57 && e[0].mac[0] == 0xA0 && e[0].mac[5] == 0xA5,
          "the first one's name, level, address");
    Check(e[0].ies == d + 4 + 128 && e[0].ieLen == N(kWpa2), "the first one's IEs past the fixed 12");
    Check(!strcmp(e[1].ssid, "MiNote3") && e[1].ieLen == N(kMixed) && e[1].ies == d + len - N(kMixed),
          "the second one past the first's padding");
    Check(BssList(d, len, e, 1) == 1, "at most max");
    Check(BssList(d, len - 1, e, 3) == 1, "the second past len");
    *(ULONG*)d = 5;
    Check(BssList(d, len, e, 3) == 2, "a count beyond the data");
    *(ULONG*)d = 2;
    Check(BssList(d, 3, e, 3) == 0, "no room for the count");
    *(ULONG*)d = 1;
    Check(BssList(d, len, e, 3) == 1, "the count heeded");
    *(ULONG*)d = 2;
    ULONG keep = *(ULONG*)(d + 4);
    *(ULONG*)(d + 4) = 100;
    Check(BssList(d, len, e, 3) == 0, "an entry shorter than its header");
    *(ULONG*)(d + 4) = keep;
    *(ULONG*)(d + 4 + 112) = keep;   // IELength past its entry
    Check(BssList(d, len, e, 3) == 0, "IEs past the entry");
    *(ULONG*)(d + 4 + 112) = 8;      // fewer than the fixed 12
    Check(BssList(d, len, e, 3) == 2 && e[0].ieLen == 0, "no variable IEs");
    *(ULONG*)(d + 4 + 112) = 12 + N(kWpa2);
    *(ULONG*)(d + 4 + 12) = 40;      // the SSID's length past its 32
    Check(BssList(d, len, e, 3) == 2 && strlen(e[0].ssid) == 9 && e[0].ssid[32] == 0, "a long SSID length cut");
    *(ULONG*)(d + 4 + 12) = 32;
    memset(d + 4 + 16, 'z', 32);
    d[4 + 48] = 'p';                 // Privacy's byte right past the name
    Check(BssList(d, len, e, 3) == 2 && strlen(e[0].ssid) == 32, "32 chars kept");
    *(ULONG*)(d + 4 + 12) = 9;       // the name's bytes past its length not taken
    Check(BssList(d, len, e, 3) == 2 && !strcmp(e[0].ssid, "zzzzzzzzz"), "the name's length heeded");
    BYTE bare[4 + 116];
    memset(bare, 0, sizeof(bare));
    *(ULONG*)bare = 1;
    *(ULONG*)(bare + 4) = 116;      // no IEs at all, the data's very end
    Check(BssList(bare, sizeof(bare), e, 3) == 1 && e[0].ieLen == 0, "an entry of its header alone");
}

// NDIS_802_11_ASSOCIATION_INFORMATION: 40 bytes, then the request's IEs and the answer's
static void TestAssoc()
{
    BYTE d[128];
    memset(d, 0, sizeof(d));
    *(ULONG*)d = 40;
    *(ULONG*)(d + 16) = 6;    // RequestIELength
    *(ULONG*)(d + 20) = 40;   // OffsetRequestIEs
    *(USHORT*)(d + 28) = 17;  // StatusCode
    *(ULONG*)(d + 32) = 3;    // ResponseIELength
    *(ULONG*)(d + 36) = 46;   // OffsetResponseIEs
    d[40] = 48;
    d[46] = 1;
    const BYTE *req = NULL, *resp = NULL;
    int rl = -1, sl = -1, st = -1;
    Check(AssocIes(d, 49, &req, &rl, &resp, &sl, &st) && req == d + 40 && rl == 6 && resp == d + 46 && sl == 3 &&
          st == 17, "the request's and the answer's IEs, the status");
    Check(!AssocIes(d, 48, &req, &rl, &resp, &sl, &st), "the answer's past len");
    *(ULONG*)(d + 16) = 7;
    Check(AssocIes(d, 49, &req, &rl, &resp, &sl, &st) && rl == 7, "the request may overlap the answer");
    *(ULONG*)(d + 16) = 10;
    Check(!AssocIes(d, 49, &req, &rl, &resp, &sl, &st), "the request's past len");
    *(ULONG*)(d + 16) = 6;
    *(ULONG*)(d + 20) = 0xFFFFFFF0;
    Check(!AssocIes(d, 49, &req, &rl, &resp, &sl, &st), "an offset wrapping round");
    Check(!AssocIes(d, 39, &req, &rl, &resp, &sl, &st), "shorter than its header");
    *(ULONG*)(d + 16) = 0;
    *(ULONG*)(d + 20) = 40;
    *(ULONG*)(d + 32) = 0;
    *(ULONG*)(d + 36) = 40;
    Check(AssocIes(d, 40, &req, &rl, &resp, &sl, &st) && rl == 0 && sl == 0, "its header alone, no IEs");
}

static void TestEnc()
{
    Check(!wcscmp(EncText(6), L"AES, ключ есть"), "AES with its key");
    Check(!wcscmp(EncText(7), L"AES, ключа нет"), "AES without");
    Check(!wcscmp(EncText(4), L"TKIP, ключ есть") && !wcscmp(EncText(5), L"TKIP, ключа нет"), "TKIP");
    Check(!wcscmp(EncText(1), L"без шифрования") && !wcscmp(EncText(0), L"WEP, ключ есть"), "none, WEP");
    Check(!wcscmp(EncText(2), L"WEP, ключа нет") && !wcscmp(EncText(3), L"не поддерживается"), "WEP absent");
    Check(!wcscmp(EncText(8), L"?") && !wcscmp(EncText(0xFFFFFFFF), L"?"), "out of range");
}

static void TestWpa2Psk()
{
    // the Galaxy A24's hotspot (sheet 24): RSN, CCMP/CCMP, PSK and SAE; before it SSID and the rates, after it WMM
    static const BYTE kBoth[] = { 0, 1, 'x', 1, 1, 0x82, 48, 24, 1, 0, 0, 0x0F, 0xAC, 4, 1, 0, 0, 0x0F, 0xAC, 4, 2, 0,
                                  0, 0x0F, 0xAC, 2, 0, 0x0F, 0xAC, 8, 0x8C, 0, 221, 2, 1, 2 };
    Check(IeWpa2Psk(kBoth, sizeof(kBoth)), "PSK and SAE: WPA2 with a password");
    static const BYTE kPsk[] = { 48, 20, 1, 0, 0, 0x0F, 0xAC, 4, 1, 0, 0, 0x0F, 0xAC, 4, 1, 0, 0, 0x0F, 0xAC, 2, 0, 0 };
    Check(IeWpa2Psk(kPsk, sizeof(kPsk)), "PSK alone");
    static const BYTE kSae[] = { 48, 20, 1, 0, 0, 0x0F, 0xAC, 4, 1, 0, 0, 0x0F, 0xAC, 4, 1, 0, 0, 0x0F, 0xAC, 8, 0, 0 };
    Check(!IeWpa2Psk(kSae, sizeof(kSae)), "SAE alone: the chip cannot");
    static const BYTE kEnt[] = { 48, 20, 1, 0, 0, 0x0F, 0xAC, 4, 1, 0, 0, 0x0F, 0xAC, 4, 1, 0, 0, 0x0F, 0xAC, 1, 0, 0 };
    Check(!IeWpa2Psk(kEnt, sizeof(kEnt)), "802.1X: no password of this kind");
    static const BYTE kTkip[] = { 48, 20, 1, 0, 0, 0x0F, 0xAC, 2, 1, 0, 0, 0x0F, 0xAC, 2, 1, 0, 0, 0x0F, 0xAC, 2, 0, 0 };
    Check(!IeWpa2Psk(kTkip, sizeof(kTkip)), "TKIP only: another cipher");
    static const BYTE kTwo[] = { 48, 24, 1, 0, 0, 0x0F, 0xAC, 2, 2, 0, 0, 0x0F, 0xAC, 2, 0, 0x0F, 0xAC, 4, 1, 0, 0, 0x0F,
                                 0xAC, 2, 0, 0 };
    Check(IeWpa2Psk(kTwo, sizeof(kTwo)), "CCMP second among the ciphers");
    static const BYTE kOui[] = { 48, 20, 1, 0, 0, 0x50, 0xF2, 4, 1, 0, 0, 0x50, 0xF2, 4, 1, 0, 0, 0x50, 0xF2, 2, 0, 0 };
    Check(!IeWpa2Psk(kOui, sizeof(kOui)), "another vendor's numbers are not RSN's");
    static const BYTE kOpen[] = { 0, 1, 'x', 221, 2, 1, 2 };
    Check(!IeWpa2Psk(kOpen, sizeof(kOpen)), "no RSN: open");
    Check(!IeWpa2Psk(kOpen, 0) && !IeWpa2Psk(NULL, 0), "nothing at all");
    for (int n = 0; n < (int)sizeof(kPsk) - 2; n++)
        Check(!IeWpa2Psk(kPsk, n), "cut anywhere: no reading past the end");
    static const BYTE kBig[] = { 48, 20, 1, 0, 0, 0x0F, 0xAC, 4, 200, 0, 0, 0x0F, 0xAC, 4, 1, 0, 0, 0x0F, 0xAC, 2, 0, 0 };
    Check(!IeWpa2Psk(kBig, sizeof(kBig)), "a count past the element's end");
    static const BYTE kAfterLong[] = { 221, 8, 0, 0x50, 0xF2, 1, 2, 3, 4, 5, 48, 20, 1, 0, 0, 0x0F, 0xAC, 4, 1, 0, 0, 0x0F,
                                       0xAC, 4, 1, 0, 0, 0x0F, 0xAC, 2, 0, 0 };
    Check(IeWpa2Psk(kAfterLong, sizeof(kAfterLong)), "another long element before RSN: passed over");
    static const BYTE kNoCaps[] = { 48, 18, 1, 0, 0, 0x0F, 0xAC, 4, 1, 0, 0, 0x0F, 0xAC, 4, 1, 0, 0, 0x0F, 0xAC, 2 };
    Check(IeWpa2Psk(kNoCaps, sizeof(kNoCaps)), "no capabilities: the lists end exactly at the element's end");
    static const BYTE kNoAkm[] = { 48, 12, 1, 0, 0, 0x0F, 0xAC, 4, 1, 0, 0, 0x0F, 0xAC, 4 };
    Check(!IeWpa2Psk(kNoAkm, sizeof(kNoAkm)), "no key managements: not PSK");
    static const BYTE kOver[] = { 48, 14, 1, 0, 0, 0x0F, 0xAC, 4, 1, 0, 0, 0x0F, 0xAC, 4, 1, 0, 0, 0x0F, 0xAC, 2 };
    Check(!IeWpa2Psk(kOver, sizeof(kOver)), "PSK just past the element's end is not its");
    static const BYTE kShort[] = { 48, 4, 1, 0, 0, 0x0F };
    Check(!IeWpa2Psk(kShort, sizeof(kShort)), "an RSN too short");
}

int main()
{
    TestWpa2Psk();
    TestIeText();
    TestBssList();
    TestAssoc();
    TestEnc();
    printf("%d of %d checks failed\n", s_failed, s_checks);
    return s_failed;
}
