// wifiie.h - what a network's beacon says of its security, and what the unit's driver answered it, as log text
// (sheet 22: MRNote13P drops the link in 2-10 s without an address, MiNote3 holds; the SDK shows both alike).
// The data are NDIS's (OID_802_11_BSSID_LIST, OID_802_11_ASSOCIATION_INFORMATION through NDISUIO, wifi.cpp);
// here only their parsing, checked against their sizes. Pure: no driver, no files.
#ifndef VESTA_WIFIIE_H
#define VESTA_WIFIIE_H
#include <windows.h>

struct BssEntry {
    BYTE        mac[6];
    char        ssid[33];
    int         rssi;
    const BYTE* ies;     // the variable IEs (past the beacon's 12 fixed bytes), into the list's data
    int         ieLen;
};

// NDIS_802_11_BSSID_LIST_EX: its entries (at most max); stops at the first one not fitting len.
int  BssList(const BYTE* data, int len, BssEntry* out, int max);
// IEs as one line: "RSN CCMP/CCMP PSK cap 008C MFPC; WPA TKIP/TKIP PSK; IE 0 1 48 221:WMM 255:35"
// (cipher group/pairwise, the key managements, RSN capabilities; every IE's id; "bad" / "cut" where broken).
void IeText(const BYTE* ies, int len, char* out, int cap);
// The beacon offers WPA2 with a password: RSN with CCMP among its pairwise ciphers and PSK among its key managements
// (a phone's hotspot with SAE beside PSK too: the chip knows no SAE, PSK is taken).
bool IeWpa2Psk(const BYTE* ies, int len);
// NDIS_802_11_ASSOCIATION_INFORMATION: the IEs the unit sent with its association request, those of the answer,
// the answer's status (0 = accepted); false when its offsets point out of len.
bool AssocIes(const BYTE* data, int len, const BYTE** req, int* reqLen, const BYTE** resp, int* respLen,
              int* status);
// NDIS_802_11_ENCRYPTION_STATUS as text: "AES, ключ есть" / "AES, ключа нет" (the 4-way handshake not done) ...
const wchar_t* EncText(DWORD status);

#endif
