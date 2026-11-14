// test_vtether.cpp - the probe on the PC over a made-up USBD and a made-up phone. (The SDK's usbdi.h after the desktop's.)
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vtether.cpp"
#include "bearssl.h"

static int s_checks, s_failed;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAIL: %s\n", what);
    }
}

// ---- the phone

static u8 s_reply[1100];
static int s_replyLen;
static u8 s_mac[6] = { 2, 0x11, 0x22, 0x33, 0x44, 0x55 };
static bool s_failInit, s_noOffer, s_keepAlive, s_failSet, s_noMac;
static u32 s_filter, s_xid;
static int s_sends, s_discovers;
static u8 s_inData[600];
static int s_inLen;
static u8 s_wrongCtlIndex;
static int s_noise, s_reads, s_failPipe, s_pipeOpens;
static bool s_badCtl, s_silent, s_failSend, s_shortMac, s_junkIn;
static UCHAR s_pipeAddr[2];

static u32 G32(const u8* p)
{
    return p[0] | p[1] << 8 | p[2] << 16 | (u32)p[3] << 24;
}

static void P32(u8* p, u32 v)
{
    p[0] = (u8)v;
    p[1] = (u8)(v >> 8);
    p[2] = (u8)(v >> 16);
    p[3] = (u8)(v >> 24);
}

static void AnswerQuery(u32 oid, u32 req)
{
    u8* r = s_reply;
    memset(r, 0, 60);
    P32(r, 0x80000004);
    P32(r + 8, req);
    P32(r + 20, 16);
    int len = 4;
    if (oid == RNDIS_OID_CURRENT_ADDRESS) {
        len = 6;
        memcpy(r + 24, s_mac, 6);
        if (s_noMac)
            P32(r + 12, 1);
        if (s_shortMac)
            len = 5;
    } else if (oid == RNDIS_OID_LINK_SPEED) {
        P32(r + 24, 4250000);
    } else if (oid == RNDIS_OID_MAX_FRAME) {
        P32(r + 24, 1500);
    }
    P32(r + 16, len);
    P32(r + 4, 24 + len);
    s_replyLen = 24 + len;
}

static void PhoneGets(const u8* m)
{
    u32 type = G32(m), req = G32(m + 8);
    s_sends++;
    if (type & 0x80000000u)
        return;   // (the keep-alive's answer: the reply waiting stays)
    memset(s_reply, 0, 60);
    if (type == 2) {
        P32(s_reply, 0x80000002);
        P32(s_reply + 4, 52);
        P32(s_reply + 8, req);
        P32(s_reply + 12, s_failInit ? 1 : 0);
        P32(s_reply + 16, 1);
        P32(s_reply + 36, 16384);
        s_replyLen = 52;
    } else if (type == 4) {
        AnswerQuery(G32(m + 12), req);
    } else if (type == 5) {
        s_filter = G32(m + 28);
        P32(s_reply, 0x80000005);
        P32(s_reply + 4, 16);
        P32(s_reply + 8, req);
        P32(s_reply + 12, s_failSet ? 1 : 0);
        s_replyLen = 16;
    } else {
        return;   // (the keep-alive's answer: nothing back)
    }
}

static void MakeOffer(u32 xid)
{
    u8 f[400];
    memset(f, 0, sizeof(f));
    f[12] = 8;
    f[14 + 9] = 17;
    f[14 + 20 + 3] = 68;
    u8* b = f + 42;
    b[0] = 2;
    b[4] = (u8)(xid >> 24);
    b[5] = (u8)(xid >> 16);
    b[6] = (u8)(xid >> 8);
    b[7] = (u8)xid;
    b[16] = 192; b[17] = 168; b[18] = 42; b[19] = 7;
    memcpy(b + 236, "\x63\x82\x53\x63", 4);
    u8 o[] = { 53, 1, 2, 3, 4, 192, 168, 42, 129, 255 };
    memcpy(b + 240, o, sizeof(o));
    s_inLen = RndisWrap(s_inData, sizeof(s_inData), f, 42 + 240 + (int)sizeof(o));
}

// ---- USBD

struct FTr { bool done; DWORD n; DWORD err; };

static USB_TRANSFER NewTr(DWORD n, DWORD err)
{
    FTr* t = new FTr;
    t->done = true;
    t->n = n;
    t->err = err;
    return t;
}

// What a phone's queue holds before the answer: an old answer of another id, a lone zero byte ("nothing yet"), a status note
static USB_TRANSFER Noise(u8* buf)
{
    int kind = s_noise--;
    if (kind == 2) {
        buf[0] = 0;
        return NewTr(1, USB_NO_ERROR);
    }
    P32(buf, kind == 1 ? 7 : 0x80000002);
    P32(buf + 4, 16);
    P32(buf + 8, 12345);
    P32(buf + 12, 0);
    return NewTr(16, USB_NO_ERROR);
}

static USB_TRANSFER FVendor(USB_HANDLE, LPTRANSFER_NOTIFY_ROUTINE, LPVOID, DWORD flags, LPCUSB_DEVICE_REQUEST r, LPVOID buf, ULONG phys)
{
    s_wrongCtlIndex = (u8)(s_wrongCtlIndex | (r->wIndex != 0));
    bool in = (r->bmRequestType & 0x80) != 0;
    if (phys || r->wValue || in != ((flags & USB_IN_TRANSFER) != 0) || r->bRequest != (in ? 1 : 0))
        s_badCtl = true;
    if (!in) {
        if (s_failSend)
            return NewTr(0, USB_STALL_ERROR);
        PhoneGets((const u8*)buf);
        return NewTr(r->wLength, USB_NO_ERROR);
    }
    s_reads++;
    if (s_silent)
        return NewTr(0, USB_STALL_ERROR);   // (a phone with nothing to say stalls the request)
    if (s_noise > 0)
        return Noise((u8*)buf);
    if (s_keepAlive) {
        s_keepAlive = false;
        u8 k[12];
        P32(k, 8);
        P32(k + 4, 12);
        P32(k + 8, 99);
        memcpy(buf, k, 12);
        return NewTr(12, USB_NO_ERROR);   // (the reply kept for the next read)
    }
    int n = s_replyLen;
    memcpy(buf, s_reply, n);
    s_replyLen = 0;
    return NewTr(n, USB_NO_ERROR);
}

// ---- a phone in its ordinary mode with "USB debugging" on: its ADB end (a header, then its data, each a transfer)

static bool s_adbPhone, s_adbAuth, s_adbKnows, s_adbAllows, s_sigOk;
static bool s_adbRefuses, s_adbSplit, s_adbBadArgs, s_adbCut;   // (cut: a header whose data never comes)   // the key refused after it is offered; output in two pieces; ours wrong
static u32 s_adbLocal;                                 // our stream's id (OPEN's)
static char s_adbCmd[200], s_adbPub[1000];
static int s_adbOkays, s_adbChunks, s_adbChunkAt;
static u8 s_adbOut[8][300];
static int s_adbOutLen[8];
static AdbMsg s_adbPending;   // a header of ours waiting for its data
static bool s_adbHasPending;
static AdbKey s_testKey;

static void AdbQueue(u32 cmd, u32 a0, u32 a1, const void* data, int len)
{
    AdbHeader(s_adbOut[s_adbChunks], cmd, a0, a1, (const u8*)data, (u32)len);
    s_adbOutLen[s_adbChunks++] = ADB_HEADER;
    if (len) {
        memcpy(s_adbOut[s_adbChunks], data, len);
        s_adbOutLen[s_adbChunks++] = len;
    }
}

static void AdbToken()
{
    u8 token[ADB_TOKEN];
    for (int i = 0; i < ADB_TOKEN; i++)
        token[i] = (u8)(i * 13 + s_adbChunks);
    memcpy(s_adbOut[7], token, ADB_TOKEN);   // (kept for the signature's check)
    if (s_adbCut) {
        AdbHeader(s_adbOut[s_adbChunks], ADB_AUTH, ADB_AUTH_TOKEN, 0, token, ADB_TOKEN);
        s_adbOutLen[s_adbChunks++] = ADB_HEADER;
        return;
    }
    if (!s_adbSplit) {
        AdbQueue(ADB_AUTH, ADB_AUTH_TOKEN, 0, token, ADB_TOKEN);
        return;
    }
    AdbHeader(s_adbOut[s_adbChunks], ADB_AUTH, ADB_AUTH_TOKEN, 0, token, ADB_TOKEN);   // (its data as 8 + 12)
    s_adbOutLen[s_adbChunks++] = ADB_HEADER;
    memcpy(s_adbOut[s_adbChunks], token, 8);
    s_adbOutLen[s_adbChunks++] = 8;
    memcpy(s_adbOut[s_adbChunks], token + 8, 12);
    s_adbOutLen[s_adbChunks++] = 12;
}

static void AdbGot(const AdbMsg& m, const u8* data)
{
    if (m.cmd == ADB_CNXN) {
        if (s_adbAuth)
            AdbToken();
        else
            AdbQueue(ADB_CNXN, ADB_VERSION, ADB_MAXDATA, "device::", 9);
    } else if (m.cmd == ADB_AUTH && m.a1 != 0) {
        s_adbBadArgs = true;
    } else if (m.cmd == ADB_AUTH && m.a0 == ADB_AUTH_SIGNATURE) {
        br_rsa_public_key pk = { s_testKey.n.b, (size_t)s_testKey.n.len, s_testKey.e.b, (size_t)s_testKey.e.len };
        u8 hash[ADB_TOKEN];
        s_sigOk = m.len == ADB_KEY_BYTES && br_rsa_i31_pkcs1_vrfy(data, m.len, BR_HASH_OID_SHA1, ADB_TOKEN, &pk, hash) &&
                  !memcmp(hash, s_adbOut[7], ADB_TOKEN);
        if (s_adbKnows && s_sigOk)
            AdbQueue(ADB_CNXN, ADB_VERSION, ADB_MAXDATA, "device::", 9);
        else
            AdbToken();
    } else if (m.cmd == ADB_AUTH && m.a0 == ADB_AUTH_RSAPUBLICKEY) {
        memcpy(s_adbPub, data, m.len);
        if (s_adbRefuses)
            AdbToken();
        else if (s_adbAllows)
            AdbQueue(ADB_CNXN, ADB_VERSION, ADB_MAXDATA, "device::", 9);
    } else if (m.cmd == ADB_OPEN) {
        memcpy(s_adbCmd, data, m.len);
        s_adbLocal = m.a0;
        s_adbBadArgs = s_adbBadArgs || m.a1 != 0;
        AdbQueue(ADB_OKAY, 5, m.a0, NULL, 0);
        if (!s_adbSplit) {
            AdbQueue(ADB_WRTE, 5, m.a0, "done\n", 5);
        } else {   // (the data: its header told 5 bytes, they come as 2 + 3)
            AdbHeader(s_adbOut[s_adbChunks], ADB_WRTE, 5, m.a0, (const u8*)"done\n", 5);
            s_adbOutLen[s_adbChunks++] = ADB_HEADER;
            memcpy(s_adbOut[s_adbChunks], "do", 2);
            s_adbOutLen[s_adbChunks++] = 2;
            memcpy(s_adbOut[s_adbChunks], "ne\n", 3);
            s_adbOutLen[s_adbChunks++] = 3;
        }
        AdbQueue(ADB_CLSE, 5, m.a0, NULL, 0);
    } else if (m.cmd == ADB_OKAY) {
        s_adbOkays++;
        s_adbBadArgs = s_adbBadArgs || m.a0 != s_adbLocal || m.a1 != 5 || m.len != 0;
    }
}

static USB_TRANSFER AdbBulk(LPTRANSFER_NOTIFY_ROUTINE cb, LPVOID param, DWORD flags, DWORD len, LPVOID buf)
{
    if (!(flags & USB_IN_TRANSFER)) {
        AdbMsg m;
        if (s_adbHasPending) {
            s_adbHasPending = false;
            AdbGot(s_adbPending, (const u8*)buf);
        } else if (AdbParse((const u8*)buf, (int)len, &m)) {
            if (m.len) {
                s_adbPending = m;
                s_adbHasPending = true;
            } else {
                AdbGot(m, NULL);
            }
        }
        cb(param);
        return NewTr(len, USB_NO_ERROR);
    }
    if (s_adbChunkAt >= s_adbChunks)
        return NewTr(0, USB_NOT_COMPLETE_ERROR);   // (nothing to say: the wait ends)
    int n = s_adbOutLen[s_adbChunkAt];
    memcpy(buf, s_adbOut[s_adbChunkAt++], n);
    if (s_adbChunkAt == s_adbChunks)
        s_adbChunkAt = s_adbChunks = 0;
    cb(param);
    return NewTr(n, USB_NO_ERROR);
}

static USB_TRANSFER FBulk(USB_PIPE pipe, LPTRANSFER_NOTIFY_ROUTINE cb, LPVOID param, DWORD flags, DWORD len, LPVOID buf, ULONG)
{
    (void)pipe;
    if (s_adbPhone)
        return AdbBulk(cb, param, flags, len, buf);
    if (!(flags & USB_IN_TRANSFER)) {
        const u8* m = (const u8*)buf;
        s_discovers++;
        const u8* b = m + 44 + 42;
        s_xid = (u32)b[4] << 24 | b[5] << 16 | b[6] << 8 | b[7];
        if (!s_noOffer)
            MakeOffer(s_xid);
        cb(param);
        return NewTr(len, USB_NO_ERROR);
    }
    if (s_junkIn) {   // (a phone's other traffic, without end)
        u8 junk[60];
        memset(junk, 0, sizeof(junk));
        int k = RndisWrap((u8*)buf, 200, junk, sizeof(junk));
        cb(param);
        return NewTr(k, USB_NO_ERROR);
    }
    if (!s_inLen)
        return NewTr(0, USB_NOT_COMPLETE_ERROR);   // (never done: the wait ends)
    memcpy(buf, s_inData, s_inLen);
    int n = s_inLen;
    s_inLen = 0;
    cb(param);
    return NewTr(n, USB_NO_ERROR);
}

static BOOL FAbort(USB_TRANSFER, DWORD) { return TRUE; }

static BOOL FStatus(USB_TRANSFER tr, LPDWORD n, LPDWORD err)
{
    FTr* t = (FTr*)tr;
    *n = t->n;
    *err = t->err;
    return TRUE;
}

static BOOL FCloseTr(USB_TRANSFER tr)
{
    delete (FTr*)tr;
    return TRUE;
}

static int s_pipes;

static USB_PIPE FOpenPipe(USB_HANDLE, LPCUSB_ENDPOINT_DESCRIPTOR e)
{
    s_pipeAddr[s_pipeOpens++ & 1] = e->bEndpointAddress;
    if (s_failPipe == s_pipeOpens)
        return NULL;
    s_pipes++;
    return (USB_PIPE)(INT_PTR)s_pipes;
}

static BOOL FClosePipe(USB_PIPE)
{
    s_pipes--;
    return TRUE;
}

static LPDEVICE_NOTIFY_ROUTINE s_notify;
static LPVOID s_notifyParam;

static BOOL FRegister(USB_HANDLE, LPDEVICE_NOTIFY_ROUTINE fn, LPVOID param)
{
    s_notify = fn;
    s_notifyParam = param;
    return TRUE;
}

// (usbdi.h's are all const: the same layouts to fill in)
struct MEp { DWORD dwCount; USB_ENDPOINT_DESCRIPTOR Descriptor; LPVOID lpvExtended; };
struct MItf { DWORD dwCount; USB_INTERFACE_DESCRIPTOR Descriptor; LPVOID lpvExtended; MEp* lpEndpoints; };
struct MCfg { DWORD dwCount; USB_CONFIGURATION_DESCRIPTOR Descriptor; LPVOID lpvExtended; DWORD dwNumInterfaces; MItf* lpInterfaces; };
struct MDev { DWORD dwCount; USB_DEVICE_DESCRIPTOR Descriptor; MCfg* lpConfigs; MCfg* lpActiveConfig; };
static MDev s_devd;
static MCfg s_cfg;
static MItf s_itfs[4];
static MEp s_eps[8];
static int s_epUsed;

static LPCUSB_DEVICE FInfo(USB_HANDLE)
{
    return (LPCUSB_DEVICE)&s_devd;
}

static void Itf(int i, int cls, int sub, int proto, int bulkPairs)
{
    MItf& it = s_itfs[i];
    memset(&it, 0, sizeof(it));
    it.dwCount = sizeof(MItf);
    it.Descriptor.bInterfaceNumber = (UCHAR)i;
    it.Descriptor.bInterfaceClass = (UCHAR)cls;
    it.Descriptor.bInterfaceSubClass = (UCHAR)sub;
    it.Descriptor.bInterfaceProtocol = (UCHAR)proto;
    it.Descriptor.bNumEndpoints = (UCHAR)(bulkPairs * 2);
    it.lpEndpoints = s_eps + s_epUsed;
    for (int k = 0; k < bulkPairs * 2; k++) {
        MEp& e = s_eps[s_epUsed++];
        memset(&e, 0, sizeof(e));
        e.dwCount = sizeof(MEp);
        e.Descriptor.bEndpointAddress = (UCHAR)(k ? 0x01 : 0x81);
        e.Descriptor.bmAttributes = USB_ENDPOINT_TYPE_BULK;
        e.Descriptor.wMaxPacketSize = 512;
    }
    s_cfg.dwNumInterfaces = i + 1;
}

static void Phone()
{
    memset(&s_cfg, 0, sizeof(s_cfg));
    memset(&s_devd, 0, sizeof(s_devd));
    s_epUsed = 0;
    s_cfg.dwCount = sizeof(MCfg);
    s_cfg.lpInterfaces = s_itfs;
    s_cfg.Descriptor.MaxPower = 250;
    s_devd.dwCount = sizeof(MDev);
    s_devd.lpConfigs = s_devd.lpActiveConfig = &s_cfg;
    s_failInit = s_noOffer = s_keepAlive = s_failSet = s_noMac = false;
    s_sends = s_discovers = s_noise = s_reads = s_failPipe = s_pipeOpens = 0;
    s_badCtl = s_silent = s_failSend = s_shortMac = s_junkIn = false;
    s_wrongCtlIndex = 0;
    s_filter = 0;
    s_replyLen = s_inLen = 0;
    s_lastStage = STAGE_NONE;
    memset(&s_lastLease, 0, sizeof(s_lastLease));
    s_adbPhone = s_adbAuth = s_adbKnows = s_adbAllows = s_sigOk = s_adbHasPending = false;
    s_adbRefuses = s_adbSplit = s_adbBadArgs = s_adbCut = false;
    s_adbOkays = s_adbChunks = s_adbChunkAt = 0;
    s_adbCmd[0] = s_adbPub[0] = 0;
    s_adbEnd = -1;
}

static USB_FUNCS s_funcs;

// A phone comes, its probe runs to the end; left attached (Gone takes it away)
static BOOL Comes()
{
    BOOL accept = TRUE;
    USBDeviceAttach((USB_HANDLE)1, &s_funcs, NULL, L"x", &accept, NULL, 0);
    if (accept)
        WaitForSingleObject(s_dev->thread, 30000);
    return accept;
}

static void Gone()
{
    DWORD code = USB_CLOSE_DEVICE;
    s_notify(s_notifyParam, code, NULL, NULL, NULL, NULL);
}

static void TestWhole()
{
    Phone();
    Itf(0, 0xE0, 1, 3, 0);
    Itf(1, 0x0A, 0, 0, 1);
    Check(Comes() == TRUE, "phone: taken");
    Check(s_lastStage == STAGE_LEASE, "phone: the whole way to a lease");
    Check(s_filter == RNDIS_FILTER_NORMAL, "phone: the packet filter set");
    Check(s_discovers == 1, "phone: one discover enough");
    Check(!memcmp(s_lastLease.ip, "\xC0\xA8\x2A\x07", 4) && !memcmp(s_lastLease.gateway, "\xC0\xA8\x2A\x81", 4), "phone: the lease kept");
    Check(!s_wrongCtlIndex, "phone: asked on its control interface");
    Check(!s_badCtl, "phone: SEND_ENCAPSULATED_COMMAND out, GET_ENCAPSULATED_RESPONSE in, as the class says");
    Check(s_pipes == 2, "phone: both pipes open");
    Check(s_pipeAddr[0] == 0x81 && s_pipeAddr[1] == 0x01, "phone: in and out each on its endpoint");
    BOOL second = TRUE;
    USBDeviceAttach((USB_HANDLE)1, &s_funcs, NULL, L"x", &second, NULL, 0);
    Check(!second, "phone: its second interface declined while it is here");
    Gone();
    Check(s_pipes == 0 && s_dev == NULL, "phone: gone - closed, free");
}

static void TestMisc()
{
    Phone();
    Itf(0, 0xEF, 4, 1, 0);
    Itf(1, 0x0A, 0, 0, 0);   // (an empty alternate first)
    Itf(2, 0x0A, 0, 0, 1);
    Check(Comes() == TRUE && s_lastStage == STAGE_LEASE, "misc class EF/04/01, the data's empty setting passed over");
    Gone();
    Phone();
    Itf(0, 0xE0, 1, 3, 0);
    Itf(1, 0x0A, 0, 0, 1);
    s_keepAlive = true;
    Check(Comes() == TRUE && s_lastStage == STAGE_LEASE, "a keep-alive on the way: answered, the answer still awaited");
    Check(s_sends == 7, "a keep-alive on the way: its answer sent (init, 4 queries, the filter + 1)");
    Check(!s_badCtl, "a keep-alive on the way: its answer as SEND_ENCAPSULATED_COMMAND");
    Gone();
    Phone();
    Itf(0, 0xE0, 1, 3, 0);
    Itf(1, 0x0A, 0, 0, 1);
    s_noise = 3;
    Check(Comes() == TRUE && s_lastStage == STAGE_LEASE, "a status note, a zero byte, an old answer on the way: the answer still awaited");
    Check(s_sends == 6, "the noise answered with nothing");
    Gone();
    u8 w[4] = { 1, 2, 0, 0 };
    Check(Word32(w, 4) == 0x201 && Word32(w, 3) == 0, "a word of the phone's: whole, or none");
}

static void TestWhichInterface()
{
    Phone();
    Itf(0, 0xFF, 0, 0, 1);   // (a vendor's own pair comes first)
    Itf(1, 0xE0, 1, 3, 0);
    Itf(2, 0x0A, 0, 0, 1);
    Itf(3, 0x0A, 0, 0, 1);
    Check(Comes() == TRUE && s_dev->data->Descriptor.bInterfaceNumber == 2, "data: the first CDC data interface, no other's");
    Check(s_dev->ctl->Descriptor.bInterfaceNumber == 1, "control: the RNDIS one");
    Gone();
    Phone();
    Itf(0, 0xE0, 1, 3, 0);
    Itf(1, 0x0A, 0, 0, 1);
    s_itfs[1].Descriptor.bNumEndpoints = 1;   // (only in)
    Check(Comes() == FALSE, "data: only in - no pair");
    Phone();
    Itf(0, 0xE0, 1, 3, 0);
    Itf(1, 0x0A, 0, 0, 1);
    s_itfs[1].Descriptor.bNumEndpoints = 1;
    s_itfs[1].lpEndpoints++;   // (only out)
    Check(Comes() == FALSE, "data: only out - no pair");
    Phone();
    Itf(0, 6, 6, 1, 0);
    Check(OtherKind((LPCUSB_CONFIGURATION)&s_cfg)[0] == L'н', "subclass 6 of another class is no ECM");
}

static void RndisPhone()
{
    Phone();
    Itf(0, 0xE0, 1, 3, 0);
    Itf(1, 0x0A, 0, 0, 1);
}

static void TestSilences()
{
    RndisPhone();
    s_failSend = true;
    Check(Comes() && s_lastStage == STAGE_INIT && s_reads == 0, "a message refused: stopped, no answer awaited");
    Gone();
    RndisPhone();
    s_silent = true;
    s_askMs = 1;
    Check(Comes() && s_lastStage == STAGE_INIT && s_reads == kAskTries, "a silent phone: its tries, stopped");
    Gone();
    s_askMs = 100;
    RndisPhone();
    s_shortMac = true;
    Check(Comes() && s_lastStage == STAGE_INIT, "an address cut short: stopped");
    Gone();
    RndisPhone();
    s_junkIn = true;
    s_dhcpMs = 20;
    Check(Comes() && s_lastStage == STAGE_DHCP && s_discovers == kDhcpTries, "other traffic without end: each try still ends");
    Gone();
    s_dhcpMs = 2000;
    for (int which = 1; which <= 2; which++) {
        RndisPhone();
        s_failPipe = which;
        Check(Comes() && s_lastStage == STAGE_PIPES && s_sends == 0, "a pipe not opened: stopped");
        Gone();
        Check(s_pipes == 0, "a pipe not opened: the other closed");
    }
    RndisPhone();
    s_silent = true;
    BOOL accept = TRUE;
    USBDeviceAttach((USB_HANDLE)1, &s_funcs, NULL, L"x", &accept, NULL, 0);
    DWORD t0 = GetTickCount();
    Gone();
    Check(GetTickCount() - t0 < 1500, "pulled out during the wait: the probe ends at once, not after its tries");
}

// An ADB phone: its debugging interface offered (the others of it are another driver's or none)
static BOOL AdbComes()
{
    BOOL accept = TRUE;
    USBDeviceAttach((USB_HANDLE)2, &s_funcs, (LPCUSB_INTERFACE)&s_itfs[1], L"x", &accept, NULL, 0);
    if (accept)
        WaitForSingleObject(s_adb->thread, 30000);
    return accept;
}

static void AdbPhone()
{
    Phone();
    Itf(0, 6, 1, 1, 1);         // MTP
    Itf(1, 0xFF, 0x42, 1, 1);   // ADB
    s_adbPhone = true;
}

static void TestAdb()
{
    AdbPhone();
    s_adbAuth = true;
    s_adbAllows = true;
    DWORD t0 = GetTickCount();
    Check(AdbComes() == TRUE, "adb: its interface taken");
    Check(GetTickCount() - t0 < 2000, "adb: over once the command closes, no wait after");
    Check(!s_adbBadArgs, "adb: AUTH and OPEN with 0 in their second argument, OKAY with both streams' ids");
    Check(s_sigOk, "adb: the token signed with this unit's key");
    char pub[1000];
    AdbPubKey(s_testKey.n.b, 65537, "VestaHome@HU", pub, sizeof(pub));
    Check(!strcmp(s_adbPub, pub), "adb: the key unknown - this unit's key offered to the phone's user");
    Check(!strcmp(s_adbCmd, kAdbCmd + 0) && !strncmp(s_adbCmd, "shell:svc usb setFunctions rndis", 32),
          "adb: allowed - tethering switched on by the shell command");
    Check(s_adbOkays == 1, "adb: the command's output taken");
    Check(s_adbEnd == ADB_DONE, "adb: done");
    Check(s_pipes == 2 && s_pipeAddr[0] == 0x81 && s_pipeAddr[1] == 0x01, "adb: its own interface's pipes");
    BOOL again = TRUE;
    USBDeviceAttach((USB_HANDLE)2, &s_funcs, (LPCUSB_INTERFACE)&s_itfs[1], L"x", &again, NULL, 0);
    Check(!again, "adb: a second one while it is here - declined");
    Gone();
    Check(s_pipes == 0 && s_adb == NULL, "adb: gone - closed, free");

    AdbPhone();
    s_adbAuth = s_adbKnows = true;
    Check(AdbComes() && s_adbEnd == ADB_DONE && !s_adbPub[0] && s_adbCmd[0], "adb: the key known - straight to the command");
    Gone();
    AdbPhone();   // a whole-device key (the phone's VID_PID, HU 2026-11-14): no interface given - its ADB found
    s_adbAuth = s_adbKnows = true;
    BOOL whole = TRUE;
    USBDeviceAttach((USB_HANDLE)2, &s_funcs, NULL, L"x", &whole, NULL, 0);
    if (whole && s_adb)
        WaitForSingleObject(s_adb->thread, 30000);
    Check(whole && s_adbEnd == ADB_DONE && s_adbCmd[0], "adb: the whole device given (its VID_PID key) - its ADB found");
    Gone();
    AdbPhone();
    Check(AdbComes() && s_adbEnd == ADB_DONE && s_adbCmd[0], "adb: a phone without auth - the command");
    Gone();
    AdbPhone();
    s_adbAuth = true;
    s_adbAskMs = 300;
    t0 = GetTickCount();
    Check(AdbComes() && s_adbEnd == ADB_KEYSENT && !s_adbCmd[0], "adb: the user not answering - waited, stopped");
    Check(GetTickCount() - t0 < 2000, "adb: the user's wait is its own (not a phone's answer's)");
    s_adbAskMs = 60000;
    Gone();
    AdbPhone();
    s_adbAuth = s_adbRefuses = true;
    t0 = GetTickCount();
    Check(AdbComes() && s_adbEnd == ADB_FAILED && !s_adbCmd[0] && GetTickCount() - t0 < 2000,
          "adb: the key refused - stopped at once");
    Gone();
    AdbPhone();
    s_adbSplit = s_adbAuth = s_adbAllows = true;
    Check(AdbComes() && s_adbEnd == ADB_DONE && s_adbOkays == 1 && s_sigOk && !s_adbBadArgs,
          "adb: a message's data in two transfers - read whole (the token signed right)");
    Gone();
    AdbPhone();
    s_adbAuth = s_adbCut = true;
    s_adbMs = 300;
    Check(AdbComes() && s_adbEnd == ADB_HELLO && !s_adbCmd[0], "adb: a message's data not coming - stopped, nothing signed");
    s_adbMs = 5000;
    Gone();
    AdbPhone();
    s_failPipe = 1;
    BOOL taken = TRUE;
    USBDeviceAttach((USB_HANDLE)2, &s_funcs, (LPCUSB_INTERFACE)&s_itfs[1], L"x", &taken, NULL, 0);
    Sleep(100);
    Check(!s_adbCmd[0] && s_adb && !s_adb->thread, "adb: a pipe not opened - no session");
    Gone();
    Check(s_pipes == 0 && s_adb == NULL, "adb: a pipe not opened - the other closed when it goes");
    for (int k = 0; k < 2; k++) {
        AdbPhone();
        Itf(1, 0xFF, k ? 0x42 : 0x43, 1, k ? 0 : 1);   // another vendor's; ADB's without its pipes
        BOOL got = TRUE;
        USBDeviceAttach((USB_HANDLE)2, &s_funcs, (LPCUSB_INTERFACE)&s_itfs[1], L"x", &got, NULL, 0);
        Check(!got && s_adb == NULL, k ? "adb: no bulk pair - not taken" : "adb: FF/43/01 - not ADB, not taken");
    }
}

static void TestAdbNot()
{
    AdbPhone();
    wchar_t keep[MAX_PATH];
    wcscpy(keep, s_keyPath);
    wcscpy(s_keyPath, L"obj\\pc\\no-such-key.bin");
    Check(AdbComes() == FALSE && s_adb == NULL, "adb: no key of this unit (TetherProbe not run) - not taken");
    wcscpy(s_keyPath, keep);
    AdbPhone();
    BOOL accept = TRUE;
    USBDeviceAttach((USB_HANDLE)2, &s_funcs, (LPCUSB_INTERFACE)&s_itfs[0], L"x", &accept, NULL, 0);
    Check(!accept && s_adb == NULL, "adb: its MTP interface - not taken");
    Phone();
    Itf(0, 0xE0, 1, 3, 0);
    Itf(1, 0x0A, 0, 0, 1);
    Itf(2, 0xFF, 0x42, 1, 1);
    accept = TRUE;
    USBDeviceAttach((USB_HANDLE)3, &s_funcs, (LPCUSB_INTERFACE)&s_itfs[2], L"x", &accept, NULL, 0);
    if (accept)
        WaitForSingleObject(s_dev->thread, 30000);
    Check(accept && s_dev && s_adb == NULL && s_lastStage == STAGE_LEASE, "adb: tethering already on - the RNDIS probe, no ADB");
    Gone();
    AdbPhone();
    s_adbAuth = true;
    accept = TRUE;
    USBDeviceAttach((USB_HANDLE)2, &s_funcs, (LPCUSB_INTERFACE)&s_itfs[1], L"x", &accept, NULL, 0);
    Sleep(50);
    DWORD t0 = GetTickCount();
    Gone();
    Check(GetTickCount() - t0 < 1500, "adb: pulled out while asking the user - ends at once");
}

static void TestNotOurs()
{
    Phone();
    Itf(0, 0xE0, 1, 3, 0);
    Check(Comes() == FALSE && s_dev == NULL, "no data interface: not taken");
    Phone();
    Itf(0, 0x0A, 0, 0, 1);
    Check(Comes() == FALSE, "no control interface: not taken");
    Phone();
    Itf(0, 2, 0x0D, 0, 0);
    Itf(1, 0x0A, 0, 0, 1);
    Check(Comes() == FALSE, "NCM: not taken");
    Check(!wcscmp(OtherKind((LPCUSB_CONFIGURATION)&s_cfg), L"CDC NCM"), "NCM: told");
    Phone();
    Itf(0, 2, 6, 0, 0);
    Check(!wcscmp(OtherKind((LPCUSB_CONFIGURATION)&s_cfg), L"CDC ECM"), "ECM: told");
    Phone();
    Itf(0, 6, 1, 1, 1);
    Check(Comes() == FALSE && OtherKind((LPCUSB_CONFIGURATION)&s_cfg)[0] == L'н', "a phone in MTP: not taken, the hint");
    Check(!IsRndisClass(0xE0, 1, 1) && !IsRndisClass(0xE0, 2, 3) && !IsRndisClass(0xEF, 4, 2), "the Bluetooth class E0/01/01 and others are not it");
}

static void TestFails()
{
    Phone();
    Itf(0, 0xE0, 1, 3, 0);
    Itf(1, 0x0A, 0, 0, 1);
    s_failInit = true;
    Check(Comes() && s_lastStage == STAGE_INIT, "init refused: stopped there");
    Gone();
    Phone();
    Itf(0, 0xE0, 1, 3, 0);
    Itf(1, 0x0A, 0, 0, 1);
    s_noMac = true;
    Check(Comes() && s_lastStage == STAGE_INIT, "no address: stopped");
    Gone();
    Phone();
    Itf(0, 0xE0, 1, 3, 0);
    Itf(1, 0x0A, 0, 0, 1);
    s_failSet = true;
    Check(Comes() && s_lastStage == STAGE_FILTER, "filter refused: stopped there");
    Gone();
    Phone();
    Itf(0, 0xE0, 1, 3, 0);
    Itf(1, 0x0A, 0, 0, 1);
    s_noOffer = true;
    s_dhcpMs = 20;
    Check(Comes() && s_lastStage == STAGE_DHCP && s_discovers == kDhcpTries, "no DHCP answer: its tries, stopped there");
    Gone();
    s_dhcpMs = 2000;
}

int main()
{
    DllMain(NULL, DLL_PROCESS_ATTACH, NULL);
    s_funcs.lpGetDeviceInfo = FInfo;
    s_funcs.lpOpenPipe = FOpenPipe;
    s_funcs.lpClosePipe = FClosePipe;
    s_funcs.lpIssueBulkTransfer = FBulk;
    s_funcs.lpAbortTransfer = FAbort;
    s_funcs.lpGetTransferStatus = FStatus;
    s_funcs.lpCloseTransfer = FCloseTr;
    s_funcs.lpIssueVendorTransfer = FVendor;
    s_funcs.lpRegisterNotificationRoutine = FRegister;
    TestWhole();
    TestMisc();
    TestNotOurs();
    TestFails();
    TestWhichInterface();
    TestSilences();
    u8 seed[32] = { 1, 2, 3 };
    AdbKeyMake(seed, sizeof(seed), &s_testKey);
    u8 file[2000];
    int flen = AdbKeyPack(&s_testKey, file, sizeof(file));
    FILE* kf = fopen("obj\\pc\\test_adbkey.bin", "wb");
    fwrite(file, 1, flen, kf);
    fclose(kf);
    wcscpy(s_keyPath, L"obj\\pc\\test_adbkey.bin");
    TestAdb();
    TestAdbNot();
    DllMain(NULL, DLL_PROCESS_DETACH, NULL);
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
