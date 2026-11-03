// A direct RFCOMM channel to a Bluetooth device, without the SDP search (Ghidra 2026-11-03 on btapi.dll and
// xBtProt.dll: the unit's stack is iAnywhere Blue SDK). The stack's serial port (obd.cpp) asks SDP for the SPP
// channel and gives up when the search fails - many ELM327 clones answer no SDP but listen on channel 1. btapi.dll
// sends each call to the driver XBT1: (IOCTL 0x222404); the channel's events come back on btapi's own thread to the
// callback given to BTRF_RegisterUser. Statuses are the SDK's: 0 done, 2 pending, 0x81 (the device) already known.
#ifndef VESTA_BTRF_H
#define VESTA_BTRF_H

// btapi.dll's calls used (the table lets the PC test stand in for the stack)
typedef void (*BtRfEventFn)(const unsigned char* ev);
struct BtRfApi {
    int (*Startup)();
    int (*Cleanup)();
    int (*AddDevice)(const unsigned char* bd, int flags, int* dev);       // BTDDB_AddDevice: the address -> a handle
    int (*BindToAcl)(int dev);                                            // the link (ME_CreateLink)
    int (*UnbindFromAcl)(int dev);
    int (*GetConnections)(unsigned char* bds, int* n);                    // XCBM_GetConnections: up to 7 x 6 bytes
    int (*RegisterUser)(BtRfEventFn cb, int* user);
    int (*DeregisterUser)(int user);
    // user, device, server channel 1..30, frame size, priority, context, -> channel, security profile, credits
    int (*OpenClientChannelEx)(int user, int dev, int server, int frame, int prio, int ctx, int* chan, int sec,
                               int credit);
    int (*SendData)(int chan, const void* p, int n, int tag);
    int (*CloseChannel)(int chan);
    int (*AdvanceCredit)(int chan, int n);
};

// The channel's events (Blue SDK RFEVENT_*): the code in byte 0, for data its length (16 bits) at 0x0C and a
// pointer to it at 0x10
enum { BTRF_EV_OPEN = 2, BTRF_EV_CLOSED = 4, BTRF_EV_DATA = 5 };

typedef void (*BtRfLogFn)(const wchar_t* fmt, ...);

bool BtRfLoad(BtRfApi* api);                         // btapi.dll (false: none, or a call missing)
// "A4:E2:87:98:D6:9F" -> the stack's order (the last byte first); false when not an address
bool BtRfAddr(const char* s, unsigned char bd[6]);
// The link, then the channel: up to linkMs for the link, openMs for the channel. false: all let go again.
bool BtRfOpen(const BtRfApi* api, const char* addr, int server, BtRfLogFn log, int linkMs, int openMs);
int  BtRfRead(char* buf, int cap, int waitMs);       // the bytes come (0 none in time), -1 the channel closed
bool BtRfWrite(const char* p, int n);
void BtRfClose();
void BtRfOnEvent(const unsigned char* ev);           // the stack's callback (public for the test)

#endif
