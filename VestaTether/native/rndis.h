// rndis.h - a phone's USB tethering: the RNDIS messages (Microsoft's Remote NDIS, little endian) and one DHCP exchange,
// all on plain bytes: no USB here, so it runs in the PC's tests. Each builder returns the bytes written, 0 no room.
#ifndef RNDIS_H
#define RNDIS_H

typedef unsigned char u8;
typedef unsigned int u32;

enum {
    RNDIS_OID_PERMANENT_ADDRESS = 0x01010101,
    RNDIS_OID_CURRENT_ADDRESS   = 0x01010102,
    RNDIS_OID_MEDIA_CONNECT     = 0x00010114,
    RNDIS_OID_LINK_SPEED        = 0x00010107,
    RNDIS_OID_MAX_FRAME         = 0x00010106,
    RNDIS_OID_PACKET_FILTER     = 0x0001010E,
    RNDIS_FILTER_NORMAL         = 0x0000000B    // directed, multicast, broadcast
};

struct RndisInitInfo { u32 major, minor, medium, maxPackets, maxTransfer, align; };

u32 RndisGet32(const u8* p);   // little endian
int RndisInit(u8* out, int cap, u32 req, u32 maxTransfer);
bool RndisInitDone(const u8* m, int n, u32 req, RndisInitInfo* info);
int RndisQuery(u8* out, int cap, u32 req, u32 oid);
bool RndisQueryDone(const u8* m, int n, u32 req, const u8** data, int* len);   // the answer inside m
int RndisSet32(u8* out, int cap, u32 req, u32 oid, u32 value);
bool RndisSetDone(const u8* m, int n, u32 req);
int RndisKeepAliveDone(u8* out, int cap, const u8* m, int n);   // the phone's keep-alive answered; 0 - m is none
// m is the answer to our message msg: the phone's queue holds other things too (status notes, a lone zero byte)
bool RndisAnswers(const u8* msg, const u8* m, int n);

int RndisWrap(u8* out, int cap, const u8* frame, int len);      // an Ethernet frame as a data message
// The next data message of a transfer from *pos (a transfer may hold several): true - its frame; *pos past it
bool RndisNext(const u8* m, int n, int* pos, const u8** frame, int* len);

int DhcpDiscover(u8* out, int cap, const u8* mac, u32 xid);     // a whole Ethernet frame, broadcast
struct DhcpLease { u8 ip[4], server[4], gateway[4], dns[4], mask[4]; };
bool DhcpOffer(const u8* frame, int len, u32 xid, DhcpLease* lease);   // the OFFER for that xid

#endif
