// btrf.cpp - a direct RFCOMM channel through btapi.dll, without SDP (btrf.h). The data comes on btapi's thread
// (BtRfOnEvent) into a ring; the reader takes it out and gives the device its credits back.
#include <windows.h>
#include <string.h>
#include "btrf.h"

enum {
    kRing = 2048,
    kFrame = 127,           // the channel's frame (the stack takes 24..2011)
    kCredits = 7,           // the packets the device may send before more credits come
    kSppSecurity = 4,       // the security of the stack's serial profile (its profile type 4, as the serial port's)
    kServerMax = 30,
    kConnMax = 16,
    kStatusPending = 2,
    kStatusKnown = 0x81,
};

static CRITICAL_SECTION s_cs;
static HANDLE           s_ev;                    // set on each event
static const BtRfApi*   s_api;                   // set: the stack started (BtRfOpen to the release)
static BtRfLogFn        s_log;
static int              s_dev, s_user, s_chan, s_waitMs;
static bool             s_bound, s_registered, s_asked, s_up;
static volatile bool    s_opened, s_closed;
static char             s_ring[kRing];
static int              s_head, s_len, s_owed;   // the ring's first byte and length; the packets not credited back
static void           (*s_wake)(const char* addr);
static char             s_addrText[18];

static int Hex(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    c = (char)(c | 0x20);
    return c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
}

bool BtRfAddr(const char* s, unsigned char bd[6])
{
    if (strlen(s) != 17)
        return false;
    for (int i = 0; i < 6; i++) {
        const char* p = s + i * 3;
        int hi = Hex(p[0]), lo = Hex(p[1]);
        if (hi < 0 || lo < 0 || (i < 5 && p[2] != ':'))
            return false;
        bd[5 - i] = (unsigned char)(hi * 16 + lo);
    }
    return true;
}

// Into the ring (what does not fit is lost: the reader is slow - not with an ELM327's short answers)
static void Put(const char* p, int n)
{
    for (int i = 0; i < n && s_len < kRing; i++, s_len++)
        s_ring[(s_head + s_len) % kRing] = p[i];
    s_owed++;
}

void BtRfOnEvent(const unsigned char* ev)
{
    EnterCriticalSection(&s_cs);
    if (ev[0] == BTRF_EV_OPEN)
        s_opened = true;
    else if (ev[0] == BTRF_EV_CLOSED)
        s_closed = true;
    else if (ev[0] == BTRF_EV_DATA)
        Put(*(const char* const*)(ev + 0x10), *(const unsigned short*)(ev + 0x0C));
    LeaveCriticalSection(&s_cs);
    SetEvent(s_ev);
}

// Up to ms for the flag; false when the channel closed first, or the time ran out
static bool WaitFor(volatile bool* flag, int ms)
{
    DWORD t0 = GetTickCount();
    while (!*flag && !s_closed) {
        DWORD el = GetTickCount() - t0;
        if ((int)el >= ms)
            return false;
        WaitForSingleObject(s_ev, ms - el);
    }
    return *flag;
}

static bool Linked(const unsigned char* bd)
{
    unsigned char bds[kConnMax * 6];
    int n = 0;
    if (s_api->GetConnections(bds, &n) != 0)
        return false;
    for (int i = 0; i < n && i < kConnMax; i++) {
        const unsigned char* b = bds + i * 6;
        bool rev = true;
        for (int j = 0; j < 6; j++)
            rev = rev && b[j] == bd[5 - j];
        if (!memcmp(b, bd, 6) || rev)   // (its order there not seen yet on the unit)
            return true;
    }
    return false;
}

// The link asked; refused - the stack's own SDP search asked to bring it up (s_wake), the link then not ours
static bool Bind()
{
    int r = s_api->BindToAcl(s_dev);
    if (r == 0 || r == kStatusPending) {
        s_bound = true;
        return true;
    }
    if (!s_wake) {
        s_log(L"rfcomm: связь не начата (%d)", r);
        return false;
    }
    s_log(L"rfcomm: связь не начата (%d) - прошу поиск SDP", r);
    s_wake(s_addrText);
    return true;
}

static bool Link(const unsigned char* bd, int ms)
{
    int r = s_api->AddDevice(bd, 0, &s_dev);
    if (r != 0 && r != kStatusKnown) {
        s_log(L"rfcomm: устройство не принято стеком (%d)", r);
        return false;
    }
    if (Linked(bd)) {
        s_log(L"rfcomm: связь уже есть");
        return true;
    }
    if (!Bind())
        return false;
    for (DWORD t0 = GetTickCount(); !Linked(bd); Sleep(100))
        if (GetTickCount() - t0 >= (DWORD)ms) {
            s_log(L"rfcomm: нет связи за %d мс (адаптер не ответил)", ms);
            return false;
        }
    s_log(L"rfcomm: связь есть");
    return true;
}

static bool Channel(int server, int ms)
{
    int r = s_api->RegisterUser(BtRfOnEvent, &s_user);
    if (r != 0) {
        s_log(L"rfcomm: пользователь не принят (%d)", r);
        return false;
    }
    s_registered = true;
    r = s_api->OpenClientChannelEx(s_user, s_dev, server, kFrame, 0, 0, &s_chan, kSppSecurity, kCredits);
    if (r != kStatusPending) {
        s_log(L"rfcomm: канал %d отклонён (%d)", server, r);
        return false;
    }
    s_asked = true;
    if (!WaitFor(&s_opened, ms)) {
        s_log(L"rfcomm: канал %d не открылся (%s)", server, s_closed ? L"адаптер закрыл" : L"нет ответа");
        return false;
    }
    return true;
}

// All let go, in the reverse order (the user 2026-11-03: a link left half done hung the stack)
static void Release()
{
    if (s_asked && !s_closed) {
        s_api->CloseChannel(s_chan);
        WaitFor(&s_closed, s_waitMs);
    }
    if (s_registered)
        s_api->DeregisterUser(s_user);
    if (s_bound)
        s_api->UnbindFromAcl(s_dev);
    s_api->Cleanup();
    s_api = NULL;
    s_bound = s_registered = s_asked = false;
}

static void Reset(const BtRfApi* api, BtRfLogFn log, int waitMs)
{
    if (!s_ev) {
        InitializeCriticalSection(&s_cs);
        s_ev = CreateEvent(NULL, FALSE, FALSE, NULL);
    }
    EnterCriticalSection(&s_cs);
    s_head = s_len = s_owed = 0;
    s_opened = s_closed = false;
    LeaveCriticalSection(&s_cs);
    s_api = api;
    s_log = log;
    s_waitMs = waitMs;
}

bool BtRfOpen(const BtRfApi* api, const char* addr, int server, BtRfLogFn log, int linkMs, int openMs)
{
    unsigned char bd[6];
    if (s_up || !BtRfAddr(addr, bd) || server < 1 || server > kServerMax)
        return false;
    int r = api->Startup();
    if (r != 0) {
        log(L"rfcomm: стек не запущен (BT_Startup %d)", r);
        return false;
    }
    Reset(api, log, openMs);
    strcpy(s_addrText, addr);
    if (!Link(bd, linkMs) || !Channel(server, openMs)) {
        Release();
        return false;
    }
    s_log(L"rfcomm: канал %d открыт", server);
    s_up = true;
    return true;
}

// The bytes in the ring into buf; the packets come credited back to the device (an empty one too: it took a credit)
static int Take(char* buf, int cap)
{
    EnterCriticalSection(&s_cs);
    int n = 0;
    for (; n < cap && s_len > 0; n++, s_len--, s_head = (s_head + 1) % kRing)
        buf[n] = s_ring[s_head];
    int owed = s_owed;
    s_owed = 0;
    LeaveCriticalSection(&s_cs);
    if (owed > 0 && !s_closed)
        s_api->AdvanceCredit(s_chan, owed);
    return n;
}

int BtRfRead(char* buf, int cap, int waitMs)
{
    if (!s_up)
        return -1;
    DWORD t0 = GetTickCount();
    for (;;) {
        int n = Take(buf, cap);
        if (n > 0)
            return n;
        if (s_closed)
            return -1;
        DWORD el = GetTickCount() - t0;
        if ((int)el >= waitMs)
            return 0;
        WaitForSingleObject(s_ev, waitMs - el);
    }
}

bool BtRfWrite(const char* p, int n)
{
    if (!s_up || s_closed)
        return false;
    int r = s_api->SendData(s_chan, p, n, 0);
    return r == 0 || r == kStatusPending;
}

void BtRfSetWake(void (*wake)(const char* addr))
{
    s_wake = wake;
}

void BtRfClose()
{
    if (!s_up)
        return;
    s_up = false;
    Release();
}

bool BtRfLoad(BtRfApi* api)
{
    // in the table's order (all its fields are function pointers)
    static const wchar_t* kNames[] = { L"BT_Startup", L"BT_Cleanup", L"BTDDB_AddDevice", L"BTDDB_BindToACL",
                                       L"BTDDB_UnbindFromACL", L"XCBM_GetConnections", L"BTRF_RegisterUser",
                                       L"BTRF_DeregisterUser", L"BTRF_OpenClientChannelEx", L"BTRF_SendData",
                                       L"BTRF_CloseChannel", L"BTRF_AdvanceCredit" };
    static HMODULE m;
    if (!m)
        m = LoadLibrary(L"btapi.dll");
    if (!m)
        return false;
    FARPROC* f = (FARPROC*)api;
    for (int i = 0; i < (int)(sizeof(kNames) / sizeof(kNames[0])); i++) {
#ifdef UNDER_CE
        f[i] = GetProcAddress(m, kNames[i]);
#else
        char a[40];
        WideCharToMultiByte(CP_ACP, 0, kNames[i], -1, a, sizeof(a), NULL, NULL);
        f[i] = GetProcAddress(m, a);
#endif
        if (!f[i])
            return false;
    }
    return true;
}
