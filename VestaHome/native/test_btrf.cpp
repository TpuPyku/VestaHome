// test_btrf.cpp - the direct RFCOMM channel (btrf.cpp) on the PC against a stand-in stack: the order of the calls,
// everything let go on each failure (test_btrf.bat).
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "btrf.h"

static int s_failed, s_checks;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAIL: %s\n", what);
    }
}

static void Log(const wchar_t*, ...)
{
}

// The stand-in: each call's letter into s_calls; its answers set by each case
static char s_calls[64];
static int  s_startup, s_add, s_bind, s_open, s_send;
static bool s_linkOnBind, s_linked, s_reversed, s_alone, s_openEvent, s_closeEvent;
static int  s_server, s_sec, s_credit, s_credits, s_creditCalls, s_sent, s_odd;   // s_odd: a 0 argument not 0
static char s_sentBuf[64];
static const unsigned char kBd[6] = { 0x9F, 0xD6, 0x98, 0x87, 0xE2, 0xA4 };

static void Called(char c)
{
    size_t n = strlen(s_calls);
    if (n < sizeof(s_calls) - 1) {
        s_calls[n] = c;
        s_calls[n + 1] = 0;
    }
}

static void Event(int code, const char* data)
{
    unsigned char ev[0x60];
    memset(ev, 0, sizeof(ev));
    ev[0] = (unsigned char)code;
    if (data) {
        *(unsigned short*)(ev + 0x0C) = (unsigned short)strlen(data);
        *(const char**)(ev + 0x10) = data;
    }
    BtRfOnEvent(ev);
}

static int Startup() { Called('S'); return s_startup; }
static int Cleanup() { Called('X'); return 0; }
static int AddDevice(const unsigned char* bd, int flags, int* dev)
{
    Called('A');
    s_odd += flags != 0;
    *dev = memcmp(bd, kBd, 6) ? -1 : 7;
    return s_add;
}
static int BindToAcl(int dev)
{
    Called('B');
    if (s_linkOnBind && dev == 7)
        s_linked = true;
    return s_bind;
}
static int UnbindFromAcl(int) { Called('U'); return 0; }
static int GetConnections(unsigned char* bds, int* n)
{
    // another device first (a phone): ours the other way round but its first byte; ours next, counted only when linked
    for (int i = 0; i < 6; i++) {
        bds[i] = kBd[5 - i] ^ (i == 0 ? 1 : 0);
        bds[6 + i] = s_reversed ? kBd[5 - i] : kBd[i];
    }
    *n = s_linked ? 2 : 1;
    if (s_alone && s_linked) {   // ours the only one
        memcpy(bds, kBd, 6);
        *n = 1;
    }
    return 0;
}
static BtRfEventFn s_cb;
static int RegisterUser(BtRfEventFn cb, int* user) { Called('R'); s_cb = cb; *user = 1; return 0; }
static int DeregisterUser(int) { Called('D'); return 0; }
static int OpenClientChannelEx(int user, int dev, int server, int, int prio, int ctx, int* chan, int sec, int credit)
{
    Called('O');
    s_odd += prio != 0 || ctx != 0;
    s_server = user == 1 && dev == 7 ? server : -1;
    s_sec = sec;
    s_credit = credit;
    if (s_open == 2) {
        *chan = 3;
        if (s_openEvent)
            Event(BTRF_EV_OPEN, NULL);   // (may come before the call returns: the stack's own thread)
    }
    return s_open;
}
static int SendData(int chan, const void* p, int n, int tag)
{
    Called('W');
    s_odd += tag != 0;
    if (chan == 3 && n < (int)sizeof(s_sentBuf)) {
        memcpy(s_sentBuf, p, n);
        s_sentBuf[n] = 0;
        s_sent = n;
    }
    return s_send;
}
static int CloseChannel(int) {
    Called('C');
    if (s_closeEvent)
        Event(BTRF_EV_CLOSED, NULL);
    return 2;
}
static int AdvanceCredit(int chan, int n)
{
    s_creditCalls++;
    if (chan == 3)
        s_credits += n;
    return 0;
}

static BtRfApi kApi = { Startup, Cleanup, AddDevice, BindToAcl, UnbindFromAcl, GetConnections, RegisterUser,
                        DeregisterUser, OpenClientChannelEx, SendData, CloseChannel, AdvanceCredit };

static void Reset()
{
    s_calls[0] = 0;
    s_startup = s_add = s_send = 0;
    s_bind = s_open = 2;
    s_linkOnBind = s_openEvent = s_closeEvent = true;
    s_linked = s_reversed = s_alone = false;
    s_server = s_sec = s_credit = s_credits = s_creditCalls = s_sent = s_odd = 0;
    s_sentBuf[0] = 0;
}

static bool Open()
{
    return BtRfOpen(&kApi, "A4:E2:87:98:D6:9F", 1, Log, 300, 300);
}

int main()
{
    unsigned char bd[6];
    Check(BtRfAddr("A4:E2:87:98:D6:9F", bd) && !memcmp(bd, kBd, 6), "addr: the last byte first");
    Check(BtRfAddr("a4:e2:87:98:d6:9f", bd) && !memcmp(bd, kBd, 6), "addr: lower case");
    Check(!BtRfAddr("", bd) && !BtRfAddr("A4:E2:87", bd) && !BtRfAddr("A4:E2:87:98:D6:9G", bd) &&
          !BtRfAddr("A4:E2:87:98:D6:9F:00", bd) && !BtRfAddr("A4-E2-87-98-D6-9F", bd), "addr: not an address");
    static const unsigned char kZero[6] = { 0x0B, 0x98, 0x00, 0x09, 0x1D, 0x00 };
    Check(BtRfAddr("00:1D:09:00:98:0B", bd) && !memcmp(bd, kZero, 6), "addr: zeros and the digits 0, 9");
    Check(!BtRfAddr("A4:E2:87:98:D6:/F", bd) && !BtRfAddr("A4:E2:87:98:D6:9:", bd), "addr: just below the digits");

    Reset();
    Check(Open() && !strcmp(s_calls, "SABRO"), "open: started, device, link, user, channel");
    Check(s_server == 1 && s_sec == 4 && s_credit > 0, "open: the server channel asked, SPP's security, credits");
    Check(BtRfWrite("ATZ\r", 4) && s_sent == 4 && !strcmp(s_sentBuf, "ATZ\r"), "write: the bytes sent");
    Event(BTRF_EV_DATA, "ELM327 ");
    Event(BTRF_EV_DATA, "v1.5\r>");
    char r[32];
    int n = BtRfRead(r, sizeof(r) - 1, 100);
    r[n > 0 ? n : 0] = 0;
    Check(n == 13 && !strcmp(r, "ELM327 v1.5\r>"), "read: both packets in order");
    Check(s_credits == 2, "read: a credit back for each packet taken");
    int calls = s_creditCalls;
    DWORD w0 = GetTickCount();
    Check(BtRfRead(r, sizeof(r), 50) == 0 && GetTickCount() - w0 >= 40, "read: nothing more - 0 after the wait");
    Check(s_creditCalls == calls, "read: nothing taken - no credit");
    Event(BTRF_EV_DATA, "0123456789");
    Check(BtRfRead(r, 4, 50) == 4 && !memcmp(r, "0123", 4) && BtRfRead(r, 32, 50) == 6 && !memcmp(r, "456789", 6),
          "read: a small buffer takes the rest next time");
    Check(s_credits == 3 && s_creditCalls == calls + 1, "read: one packet - one credit, once");
    Event(BTRF_EV_DATA, ">");
    Check(BtRfRead(r, sizeof(r), 50) == 1 && r[0] == '>' && s_credits == 4, "read: a one-byte packet");
    static char big[2101];
    memset(big, 'x', 2100);
    Event(BTRF_EV_DATA, big);
    int all = 0;
    for (int got; (got = BtRfRead(r, sizeof(r), 20)) > 0;)
        all += got;
    Check(all == 2048, "read: what does not fit the ring (2048) is lost");
    Event(BTRF_EV_DATA, "");
    Check(BtRfRead(r, sizeof(r), 20) == 0 && s_credits == 6, "read: an empty packet - its credit back too");
    Check(s_odd == 0, "calls: the zero arguments (flags, priority, context, tag) zero");
    s_send = 0x0C;
    Check(!BtRfWrite("0100\r", 5), "write: the stack refused");
    s_calls[0] = 0;
    BtRfClose();
    Check(!strcmp(s_calls, "CDUX"), "close: channel, user, link, stack");
    BtRfClose();
    Check(!strcmp(s_calls, "CDUX"), "close: twice - nothing more");
    Check(BtRfRead(r, sizeof(r), 10) == -1 && !BtRfWrite("x", 1), "closed: no reads, no writes");

    Reset();
    Check(Open(), "closed by the device: open");
    Event(BTRF_EV_DATA, "OK");
    Event(BTRF_EV_CLOSED, NULL);
    Check(BtRfRead(r, sizeof(r), 50) == 2 && BtRfRead(r, sizeof(r), 50) == -1, "closed by the device: the data, then -1");
    Check(s_creditCalls == 0 && !BtRfWrite("x", 1), "closed by the device: no credit, no write");
    s_calls[0] = 0;
    BtRfClose();
    Check(!strcmp(s_calls, "DUX"), "closed by the device: no close asked again");

    Reset();
    s_linked = true;
    Check(Open() && !strcmp(s_calls, "SARO"), "linked already: no link asked");
    s_calls[0] = 0;
    BtRfClose();
    Check(!strcmp(s_calls, "CDX"), "linked already: the link not ours - kept");

    Reset();
    s_reversed = true;
    Check(Open(), "link: the address the other way round also counts");
    BtRfClose();

    Reset();
    s_alone = true;
    Check(Open(), "link: ours the only (first) one");
    BtRfClose();

    Reset();
    s_bind = 0;
    Check(Open(), "link done at once (0): goes on");
    BtRfClose();

    Reset();
    Check(BtRfOpen(&kApi, "A4:E2:87:98:D6:9F", 30, Log, 300, 300) && s_server == 30, "channel 30: the last one");
    BtRfClose();
    Reset();
    Check(!BtRfOpen(&kApi, "A4:E2:87:98:D6:9F", 31, Log, 300, 300) &&
          !BtRfOpen(&kApi, "A4:E2:87:98:D6:9F", 0, Log, 300, 300) && !s_calls[0], "channels 0, 31: no call");

    Reset();
    s_startup = 0x0E;
    Check(!Open() && !strcmp(s_calls, "S"), "no stack: nothing else (BT_Startup cleans itself)");

    Reset();
    s_add = 0x81;
    Check(Open(), "device known already (0x81): goes on");
    BtRfClose();

    Reset();
    s_add = 0x0C;
    Check(!Open() && !strcmp(s_calls, "SAX"), "device refused: the stack let go");

    Reset();
    s_bind = 0x0D;
    Check(!Open() && !strcmp(s_calls, "SABX"), "link refused: the stack let go");

    Reset();
    s_linkOnBind = false;
    DWORD t0 = GetTickCount();
    Check(!Open() && !strcmp(s_calls, "SABUX"), "link not up in time: the link and the stack let go");
    Check(GetTickCount() - t0 >= 250, "link not up in time: waited");

    Reset();
    s_open = 0x11;
    Check(!Open() && !strcmp(s_calls, "SABRODUX"), "channel refused: user, link, stack let go - no close");

    Reset();
    s_openEvent = false;
    t0 = GetTickCount();
    Check(!Open() && !strcmp(s_calls, "SABROCDUX"), "channel not open in time: closed, then all let go");
    Check(GetTickCount() - t0 >= 250, "channel not open in time: waited");

    Reset();
    s_openEvent = false;
    s_open = 2;
    s_closeEvent = false;
    Check(!Open() && !strcmp(s_calls, "SABROCDUX"), "no closed event either: let go after the wait");

    Check(!BtRfOpen(&kApi, "nope", 1, Log, 300, 300) && !strcmp(s_calls, "SABROCDUX"), "bad address: no call");

    struct { BtRfApi a; void* guard; } g;   // the stand-in btapi.dll (test_btapi.cpp): each name in its own place
    g.guard = &g;
    BtRfApi& a = g.a;
    int x = 0;
    Check(BtRfLoad(&a) && g.guard == &g, "load: nothing written past the table");
    Check(a.Startup() == 101 && a.Cleanup() == 102 && a.AddDevice(kBd, 0, &x) == 103 &&
          a.BindToAcl(0) == 104 && a.UnbindFromAcl(0) == 105 && a.GetConnections(NULL, &x) == 106 &&
          a.RegisterUser(NULL, &x) == 107 && a.DeregisterUser(0) == 108 &&
          a.OpenClientChannelEx(0, 0, 0, 0, 0, 0, &x, 0, 0) == 109 && a.SendData(0, NULL, 0, 0) == 110 &&
          a.CloseChannel(0) == 111 && a.AdvanceCredit(0, 0) == 112, "load: btapi.dll's calls by their names");

    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
