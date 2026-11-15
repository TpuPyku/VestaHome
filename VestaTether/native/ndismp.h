// ndismp.h - the phone's tethering as a network adapter (step 2, 2026-11-15): an NDIS 5.1 miniport in VestaTether.dll,
// as BTCENDIS (Bluetooth PAN) is - its keys Comm\VestaRndis, Comm\VestaRndis1 (the unit's registry is new at each
// start), registered through NDS0: once the RNDIS phone answers; TCP/IP and DHCP are the system's. Unit only: the PC's
// tests get the stubs below.
#ifndef NDISMP_H
#define NDISMP_H

typedef unsigned char u8;

struct TetherPort {   // the phone's data pipes, for the adapter
    void* ctx;
    u8    mac[6];                                          // the adapter's address (the phone's RNDIS answer)
    bool (*send)(void* ctx, const u8* frame, int len);     // one Ethernet frame
    int  (*recv)(void* ctx, u8* buf, int cap);             // a transfer of RNDIS packets: bytes, 0 none in time, -1 gone
};

#ifdef UNDER_CE
bool NdisUp(const TetherPort& port);   // the adapter registered: false - NDIS refused
void NdisDown();                       // the phone gone: the adapter away
#else
static bool NdisUp(const TetherPort&) { return false; }
static void NdisDown() {}
#endif

#endif
