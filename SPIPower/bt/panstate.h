// panstate.h - the PAN state machine inside the stock FLC_BLUETOOTH_SDK.dll (Ghidra and its code, 2026-11-06).
//
// PanServer_Connect does nothing and returns 0 unless the state is "ready"; a connect the stack never answers leaves it
// "connecting" and a disconnect of a silent phone leaves it "disconnecting", for good. Flc_Bluetooth_Deinit's worker
// calls PanServer_Disconnect and PanServer_Deinit itself, but Deinit resets only from "ready", so even switching
// Bluetooth off and on keeps it stuck (HU 2026-11-05: "-> 0" with no event, until a reboot). Nothing public resets it:
// the proxy puts "ready" back into the variable - only if the code around it is the very code known here. Pure: tested
// on the PC against the real DLL (test_panstate.bat).
#ifndef PANSTATE_H
#define PANSTATE_H

enum { PAN_ST_DEINIT, PAN_ST_CONNECTING, PAN_ST_CONNECTED, PAN_ST_DISCONNECTING, PAN_ST_READY };
enum { kPanConnectRva = 0x17CF4, kPanStateRva = 0x2110C, kPanStuckMs = 15000 };

// The DLL's image in memory, loaded at that address, is the known build: the state machine's checks and its literal.
inline bool PanLayoutOk(const unsigned char* image, unsigned long loadedAt)
{
    struct Word { unsigned long rva, value; };
    static const Word kCode[] = {
        { 0x105A8, 0xE3510002 },   // cmp r1, #2 (Disconnect: "connected"?)
        { 0x10610, 0xE3510004 },   // cmp r1, #4 (Connect: "ready"?)
        { 0x1065C, 0xE585300C },   // str r3, [r5, #0xC] (the state stored)
    };
    for (unsigned i = 0; i < sizeof(kCode) / sizeof(kCode[0]); i++) {
        const unsigned char* p = image + kCode[i].rva;
        unsigned long w = p[0] | (unsigned long)p[1] << 8 | (unsigned long)p[2] << 16 | (unsigned long)p[3] << 24;
        if (w != kCode[i].value)
            return false;
    }
    const unsigned char* lit = image + 0x1068C;   // the machine's address: relocated, so it tells the base is right
    unsigned long a = lit[0] | (unsigned long)lit[1] << 8 | (unsigned long)lit[2] << 16 | (unsigned long)lit[3] << 24;
    return a == loadedAt + kPanStateRva - 0xC;
}

// Waiting for an event from the stack (it may never come).
inline bool PanWaiting(long state)
{
    return state == PAN_ST_CONNECTING || state == PAN_ST_DISCONNECTING;
}

// A new connect asked for while the last request has waited this long: that request is dead (a younger one: wait).
inline bool PanDeadRequest(long state, unsigned long ageMs)
{
    return PanWaiting(state) && ageMs >= kPanStuckMs;
}

// The phone's address as the stack's PAN calls take it (BD_ADDR, 6 bytes; the SDK's FUN_100068f0, Ghidra 2026-11-12):
// "A4:E2:01:02:03:04" - its pairs from the last, out[0] = 0x04 ... out[5] = 0xA4. False: not 6 hex pairs.
inline bool PanAddr(const char* text, unsigned char out[6])
{
    if (!text)
        return false;
    int i = 0;
    for (; i < 6; i++) {
        const char* p = text + i * 3;
        int v = 0;
        for (int k = 0; k < 2; k++) {
            char c = p[k];
            int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 :
                    c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
            if (d < 0)
                return false;
            v = v * 16 + d;
        }
        out[5 - i] = (unsigned char)v;
        if (p[2] != (i < 5 ? ':' : 0))
            return false;
    }
    return true;
}

#endif
