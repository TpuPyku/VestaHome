// afhmap.h - Bluetooth kept off the Wi-Fi channel (the users 2026-11-28: Wi-Fi and Bluetooth spoil each other; the
// unit's two CSR chips share 2.4 GHz and nothing in the firmware tells the BT stack where Wi-Fi is - PROJECT.md).
//
// The map for btapi.dll #254 XCBM_SetChannelClassification(BYTE map[10], DWORD) - the stack's
// Set_AFH_Host_Channel_Classification: the 79 BT channels at 2402 + k MHz, bit k (byte k / 8, the lowest bit first)
// 1 - usable, 0 - bad; bit 79 is reserved (0). Bad: within 11 MHz of the Wi-Fi channel's middle (its 22 MHz band).
#ifndef AFHMAP_H
#define AFHMAP_H

inline int AfhWifiMhz(int ch)   // a 2.4 GHz Wi-Fi channel's middle, 0 - no such channel
{
    return ch >= 1 && ch <= 13 ? 2407 + 5 * ch : ch == 14 ? 2484 : 0;
}

// The map for Wi-Fi channel ch (0: none - all usable); the channels marked bad
inline int AfhMap(int ch, unsigned char map[10])
{
    int mid = AfhWifiMhz(ch), bad = 0;
    for (int i = 0; i < 10; i++)
        map[i] = 0;
    for (int k = 0; k < 79; k++) {
        int d = 2402 + k - mid;
        if (mid && d >= -11 && d <= 11)
            bad++;
        else
            map[k / 8] |= (unsigned char)(1 << (k % 8));
    }
    return bad;
}

// VestaHome's command text (BT_CMD_AFH): "0".."14"; -1 - not a channel
inline int AfhChannel(const char* t)
{
    int ch = 0, n = 0;
    for (; t[n] >= '0' && t[n] <= '9' && n < 3; n++)
        ch = ch * 10 + (t[n] - '0');
    return n && !t[n] && ch <= 14 ? ch : -1;
}

inline void AfhHex(const unsigned char map[10], char out[21])   // for the log
{
    static const char kHex[] = "0123456789ABCDEF";
    for (int i = 0; i < 10; i++) {
        out[2 * i] = kHex[map[i] >> 4];
        out[2 * i + 1] = kHex[map[i] & 15];
    }
    out[20] = 0;
}

#endif
