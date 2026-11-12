// FLC_BLUETOOTH_SDK.dll proxy (Windows CE / ARM, VS2005 -- no C++11): the stock shell's Bluetooth SDK, passed
// through, so that VestaHome sees the shell's Bluetooth and can steer its music (btshm.h).
//
// Replaces \ResidentFlash\navigation\app\FLC_BLUETOOTH_SDK.dll; the original sits beside it as
// FLC_BLUETOOTH_SDK_orig.dll. Every export is an ARM thunk into g_real[] (thunks.s, gen_thunks_bt.py), only
// Flc_Bluetooth_Init is ours: it hands the SDK our callback, which publishes each event and calls the shell's.
// Safety first: if the original does not load, every export answers "failed" (1) instead of jumping to nothing -
// the shell then runs without Bluetooth rather than not at all.
#include <windows.h>
#include <string.h>
#include "g_btproxy.h"
#include "btshm.h"
#include "panstate.h"

extern "C" void* g_real[PROXY_COUNT] = {0};

typedef void (*FlcCb)(int* ev);
typedef int (*FlcFn0)(void);

static HMODULE s_real;
static BtShm*  s_shm;
static HANDLE  s_map, s_cmdEv, s_thread;
static FlcCb   s_appCb;
static int     s_logging;

static int Failed()   // stands in for every export when the original is missing
{
    return 1;
}

// \SDMemory\VestaLogs\BTProxy.log, only while \SDMMC\SPIPower.on exists (the MCU proxy's flag; the shell loads us
// at boot, maybe before the cards are mounted: looked at again every 10 s, both ways - with \SDMMC\VestaLogs.off,
// VestaHome's switch of all our logs)
static int LogOn()
{
    static DWORD s_checked;
    if (GetTickCount() - s_checked > 10000) {
        s_checked = GetTickCount();
        s_logging = GetFileAttributesW(L"\\SDMMC\\SPIPower.on") != 0xFFFFFFFF &&
                    GetFileAttributesW(L"\\SDMMC\\VestaLogs.off") == 0xFFFFFFFF;
    }
    return s_logging;
}

static void LogLine(const wchar_t* s)
{
    if (!LogOn())
        return;
    CreateDirectoryW(L"\\SDMemory\\VestaLogs", NULL);
    HANDLE f = CreateFileW(L"\\SDMemory\\VestaLogs\\BTProxy.log", GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return;
    SetFilePointer(f, 0, NULL, FILE_END);
    DWORD n = 0, t = GetTickCount();
    wchar_t ts[16];   // seconds since boot, as the other logs
    wsprintfW(ts, L"%u.%03u ", t / 1000, t % 1000);
    WriteFile(f, ts, wcslen(ts) * sizeof(wchar_t), &n, NULL);
    WriteFile(f, s, wcslen(s) * sizeof(wchar_t), &n, NULL);
    static const wchar_t nl[] = L"\r\n";
    WriteFile(f, nl, sizeof(nl) - sizeof(wchar_t), &n, NULL);
    CloseHandle(f);
}

static void CopyText(char* dst, const char* src, int cap)
{
    int i = 0;
    for (; i < cap - 1 && src[i]; i++)
        dst[i] = src[i];
    dst[i] = 0;
}

// One event into the ring and the state. The event is the SDK's buffer: read with a guard, only the fields the
// shell itself reads for that code.
static void Publish(int* ev)
{
    if (!s_shm)
        return;
    __try {
        int code = ev[0];
        LONG n = InterlockedIncrement((LONG*)&s_shm->evSeq);
        BtEvent* e = &s_shm->ev[(n - 1) & (BT_EV_SLOTS - 1)];
        e->seq = 0;
        e->tick = GetTickCount();
        e->code = code;
        memcpy(e->data, ev + 1, BT_EV_DATA);
        e->seq = n;

        BtState& st = s_shm->st;
        InterlockedIncrement((LONG*)&s_shm->seq);   // odd: being written
        switch (code) {
        case 0x01: st.ready = 1; break;
        case 0x02: st.ready = 0; break;   // (Deinit's end)
        case 0x0A:
            CopyText(st.devAddr, (char*)ev + 4, 18);
            CopyText(st.devName, (char*)ev + 0x16, BT_TEXT);
            st.devTick = e->tick;
            break;
        case 0x08: {   // a paired device: kept once per address
            const char* a = (const char*)ev + 4;
            int i = 0;
            while (i < st.paired && strncmp(st.pairedAddr[i], a, 17))
                i++;
            if (i == st.paired && st.paired < BT_PAIRED)
                st.paired++;
            if (i < st.paired) {
                CopyText(st.pairedAddr[i], a, 18);
                CopyText(st.pairedName[i], (const char*)ev + 0x16, 48);
            }
            break;
        }
        case 0x11: st.hf = ev[1]; break;
        case 0x0E: if (ev[1] == 1) CopyText(st.callNumber, (char*)ev + 9, 32); break;
        case 0x10:
            if (ev[2] == 1 || ev[2] == 2) {
                st.call[ev[2] - 1] = ev[1];
                st.callTick = e->tick;
            }
            break;
        case 0x14: CopyText(st.operatorName, (char*)ev + 4, 32); break;
        case 0x12: st.hfAudio = ev[1]; break;
        case 0x15: st.battery = (char)ev[1]; break;
        case 0x16: st.signal = (char)ev[1]; break;
        case 0x2B: st.av = ev[1]; st.avTick = e->tick; break;
        case 0x2C: st.lengthMs = ev[1]; st.posMs = ev[2]; st.playing = ev[3]; st.playTick = e->tick; break;
        case 0x30: st.avrcp = ev[1]; break;
        case 0x45: st.pan = BT_PAN_ONLINE; st.panTick = e->tick; break;   // (PAN: the phone's internet)
        case 0x4B: st.pan = BT_PAN_FAILED; st.panTick = e->tick; break;
        case 0x46: st.pan = BT_PAN_IDLE; st.panTick = e->tick; break;
        case 0x2E: {
            int items = ev[1] < 0 ? 0 : ev[1] > 7 ? 7 : ev[1];
            st.title[0] = st.artist[0] = st.album[0] = 0;
            st.trackNo = st.trackCount = st.durationMs = 0;
            for (int i = 0; i < items; i++) {
                int attr = ev[2 + i * 0x42];
                const char* t = (const char*)(ev + 4 + i * 0x42);
                if (attr == 1)
                    CopyText(st.title, t, BT_TEXT);
                else if (attr == 2)
                    CopyText(st.artist, t, BT_TEXT);
                else if (attr == 3)
                    CopyText(st.album, t, BT_TEXT);
                else if (attr == 4)
                    st.trackNo = atoi(t);
                else if (attr == 5)
                    st.trackCount = atoi(t);
                else if (attr == 7)
                    st.durationMs = atoi(t);
            }
            st.mediaTick = e->tick;
            break;
        }
        }
        InterlockedIncrement((LONG*)&s_shm->seq);   // even: done
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        if (s_shm->seq & 1)
            InterlockedIncrement((LONG*)&s_shm->seq);
    }
}

static void LogEvent(int* ev)
{
    if (!LogOn())
        return;
    wchar_t b[260];
    __try {
        int k = wsprintfW(b, L"EV 0x%02X:", (unsigned)ev[0]);
        const unsigned char* p = (const unsigned char*)(ev + 1);
        int all = ev[0] == 0x0B || ev[0] == 0x4F ? BT_EV_DATA : 24;   // an SDP answer: whole (OBD 2026-10-29)
        for (int i = 0; i < all && k < 240; i++)
            k += wsprintfW(b + k, L" %02X", (unsigned)p[i]);
        const char* t = ev[0] == 0x2E && ev[1] > 0 ? (const char*)(ev + 4)   // the first text of a track's info
                      : ev[0] == 0x0A || (((char*)ev)[6] == ':' && ((char*)ev)[9] == ':')
                      ? (const char*)ev + 0x16 : NULL;            // the device's name (an address at byte 4)
        if (t) {
            char a[41];
            int i = 0;
            for (; i < 40 && t[i]; i++)
                a[i] = t[i];
            a[i] = 0;
            wcscpy(b + k, L" | ");
            k += 3;
            if (!MultiByteToWideChar(CP_UTF8, 0, a, -1, b + k, 260 - k))
                b[k] = 0;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        wcscpy(b, L"EV ?");
    }
    LogLine(b);
}

// A pairing asked by VestaHome (BT_CMD_PAIR): its address and PIN, and the shell's PIN to put back. The stack's
// request (its code unknown: an event with that address at byte 4, not a list's 0x08 / 0x09 / 0x0A) is answered
// by CmdThread - not here, in the SDK's callback.
static char          s_pairAddr[18], s_oldPin[17];
static volatile LONG s_pairOn, s_pairAsk;
static DWORD         s_pairTick;

static BtPair* s_pairShm;   // a phone's pairing request for VestaHome (btshm.h BtPair)
static HANDLE  s_pairMap;

static void PairWatch(int* ev)
{
    if (s_pairShm && (ev[0] == 4 || ev[0] == 5)) {   // a phone asks to pair (4) / a pairing ended (5)
        __try {
            const char* e = (const char*)ev;
            if (ev[0] == 4) {
                strncpy(s_pairShm->addr, e + 4, 17);
                s_pairShm->addr[17] = 0;
                strncpy(s_pairShm->name, e + 0x16, 63);
                s_pairShm->name[63] = 0;
                strncpy(s_pairShm->code, e + 0x116, 15);
                s_pairShm->code[15] = 0;
                s_pairShm->tick = GetTickCount();
                InterlockedIncrement((LONG*)&s_pairShm->seq);
            } else {
                s_pairShm->result = (unsigned char)e[0x116];
                InterlockedIncrement((LONG*)&s_pairShm->doneSeq);
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }
    if (!s_pairOn || ev[0] == 0x08 || ev[0] == 0x09 || ev[0] == 0x0A)
        return;
    __try {
        if (!_strnicmp((const char*)ev + 4, s_pairAddr, 17)) {
            s_pairAsk = ev[0];
            SetEvent(s_cmdEv);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

// ---- the phone book and the call lists (PBAP, btshm.h BtPb). The shell starts a pull itself when PBAP connects
// (NCL_BT, event 0x38) and files every entry into its own book, so VestaHome's pulls are hidden from it: their events
// 0x38..0x3E do not reach its callback, and the pull is started in our callback, before the shell's (as NCL_BT
// itself does) - the shell's own pull is then refused by the SDK (one at a time).
static BtPb*         s_pb;
static HANDLE        s_pbMap;
static int           s_pbStorage, s_pbList;     // VestaHome's pull asked for
static volatile LONG s_pbConnecting, s_pbSession, s_pbOurs, s_pbDisc;
static DWORD         s_pbConnTick;

typedef int (*PullFn)(int, int, short, short, char);

static void PbState(int state)
{
    s_pb->state = state;
    s_pb->tick = GetTickCount();
    if (state == BT_PBS_DONE || state == BT_PBS_FAILED)
        InterlockedIncrement((LONG*)&s_pb->seq);
}

extern "C" int proxy_Flc_Bluetooth_PbapClient_PullPhonebook(int storage, int list, short from, short to, char photo)
{
    int r = ((PullFn)g_real[IDX_Flc_Bluetooth_PbapClient_PullPhonebook])(storage, list, from, to, photo);
    if (s_pb && r == 2) {   // started (the SDK's "pending"): its entries come next
        s_pb->storage = storage;
        s_pb->list = list;
        s_pb->count = s_pb->total = 0;
        PbState(BT_PBS_PULLING);
    }
    wchar_t b[96];
    wsprintfW(b, L"PBAP pull %d/%d %d..%d%s -> %d", storage, list, (int)from, (int)to, s_pbOurs ? L" (VH)" : L"", r);
    LogLine(b);
    return r;
}

static void PbapPull()   // VestaHome's pull, PBAP connected
{
    s_pbOurs = 1;
    int r = proxy_Flc_Bluetooth_PbapClient_PullPhonebook(s_pbStorage, s_pbList, 0,
                                                         s_pbList == BT_PB_PB ? BT_PB_MAX - 1 : 49, 0);
    if (r != 2) {
        s_pbOurs = 0;
        PbState(BT_PBS_FAILED);
        if (s_pbSession) {   // connected for this pull only
            s_pbDisc = 1;
            SetEvent(s_cmdEv);
        }
    }
}

static void PbEntry(int* ev)
{
    s_pb->total++;
    if (s_pb->count >= BT_PB_MAX)
        return;
    BtPbEntry& e = s_pb->e[s_pb->count];
    memset(&e, 0, sizeof(e));
    CopyText(e.name, (const char*)ev + 4, sizeof(e.name));
    typedef int (*CountFn)(int);
    typedef void** (*GetFn)(int, int);
    int k = 0;
    for (int l = 0; l < 6 && k < BT_PB_NUMS; l++) {   // the six lists of numbers
        int h = ev[0x41 + l];
        if (!h)
            continue;
        int n = ((CountFn)g_real[IDX_LIST_Count])(h);
        for (int i = 0; i < n && k < BT_PB_NUMS; i++) {
            void** it = ((GetFn)g_real[IDX_LIST_Get])(h, i);
            if (it && *it) {
                CopyText(e.num[k], (const char*)*it, sizeof(e.num[k]));
                e.numKind[k++] = (unsigned char)l;
            }
        }
    }
    CopyText(e.time, (const char*)(ev + 0x47), sizeof(e.time));
    e.callType = (unsigned char)ev[0x4E];
    s_pb->count++;
}

// A PBAP event: into BtPb; true = VestaHome's, not for the shell
static bool PbEvent(int* ev)
{
    if (!s_pb)
        return false;
    bool ours = s_pbOurs || s_pbSession;
    __try {
        switch (ev[0]) {   // (the SDK's codes = its case labels + 1, as NCL_BT's: HU sheet 30 - 0x39 02 connected,
        case 0x39:         // 0x3A the start, 0x3B an entry each, 0x3C the end; were taken one lower: one empty contact)
            s_pb->pbap = ev[1];
            if (ev[1] == 2 && s_pbConnecting) {   // connected for VestaHome: its pull, the shell not told
                s_pbConnecting = 0;
                s_pbSession = 1;
                PbapPull();
                return true;
            }
            if (ev[1] == 0) {
                if (s_pb->state == BT_PBS_PULLING || s_pb->state == BT_PBS_CONNECTING)
                    PbState(BT_PBS_FAILED);
                s_pbConnecting = s_pbSession = s_pbOurs = 0;
            }
            return ours;
        case 0x3A:
            if (s_pb->state == BT_PBS_PULLING)
                s_pb->count = s_pb->total = 0;
            return s_pbOurs != 0;
        case 0x3B:
            if (s_pb->state == BT_PBS_PULLING)
                PbEntry(ev);
            return s_pbOurs != 0;
        case 0x3E:   // (after a SIM pull, nothing more came: HU sheet 30 - taken as failed, the next pull free)
            if (s_pb->state == BT_PBS_PULLING)
                PbState(BT_PBS_FAILED);
            if (s_pbOurs) {
                s_pbOurs = 0;
                return true;
            }
            return ours;
        case 0x3C:
            if (s_pb->state == BT_PBS_PULLING)
                PbState(BT_PBS_DONE);
            if (s_pbOurs) {
                s_pbOurs = 0;
                if (s_pbSession) {   // connected for this pull only: disconnected again (not in the callback)
                    s_pbDisc = 1;
                    SetEvent(s_cmdEv);
                }
                return true;
            }
            return ours;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        LogLine(L"PBAP event: exception");
    }
    return ours;
}

static HANDLE s_powEv;   // the stack's 0x01 / 0x02 seen: Init / Deinit done (BT_CMD_POWER)
static HANDLE s_panEv;   // the stack's 0x46 seen: PAN disconnected (BT_CMD_PAN)
static DWORD s_panAsked;  // the last connect / disconnect asked of the stack, its tick

static void ProxyCb(int* ev)
{
    Publish(ev);
    if (ev[0] == 0x01 || ev[0] == 0x02)
        SetEvent(s_powEv);
    if (ev[0] == 0x46)
        SetEvent(s_panEv);
    LogEvent(ev);
    PairWatch(ev);
    bool hide = ev[0] >= 0x39 && ev[0] <= 0x3F && PbEvent(ev);
    if (s_appCb && !hide)
        s_appCb(ev);
}

// BT_CMD_PBAP_PULL in the command thread: pull now if PBAP is connected, else connect (the pull follows in the
// callback, PbEvent)
static int PbapCmd(const char* t)
{
    if (!s_pb)
        return -1;
    s_pbStorage = t[0] == '1' ? 1 : 0;
    s_pbList = atoi(t + 1);
    if (s_pbOurs || s_pbConnecting || (s_pb->state == BT_PBS_PULLING && GetTickCount() - s_pb->tick < 60000))
        return -2;   // a pull runs (the shell's or ours)
    int st = 0;
    ((int (*)(int*))g_real[IDX_Flc_Bluetooth_PbapClient_GetState])(&st);
    if (st == 2) {
        PbapPull();
        return s_pb->state == BT_PBS_FAILED ? 1 : 0;
    }
    char addr[18];
    CopyText(addr, s_shm->st.devAddr, 18);   // (written in this process, by the callback)
    if (!addr[0])
        return 3;   // no phone connected
    s_pbConnecting = 1;
    s_pbConnTick = GetTickCount();
    PbState(BT_PBS_CONNECTING);
    int r = ((int (*)(char*))g_real[IDX_Flc_Bluetooth_PbapClient_Connect])(addr);
    if (r == 11)   // (on its way: 0x39 02 followed, HU sheet 30 - not a refusal)
        r = 0;
    if (r != 0) {
        s_pbConnecting = 0;
        PbState(BT_PBS_FAILED);
    }
    return r;
}

static void PbapTick()   // the command thread, every second at most: our session's end, a connect that never came
{
    if (s_pbDisc) {
        s_pbDisc = 0;
        int r = ((FlcFn0)g_real[IDX_Flc_Bluetooth_PbapClient_Disconnect])();
        wchar_t b[48];
        wsprintfW(b, L"PBAP disconnect -> %d", r);
        LogLine(b);
    }
    if (s_pbConnecting && GetTickCount() - s_pbConnTick > 20000) {
        s_pbConnecting = 0;
        PbState(BT_PBS_FAILED);
        LogLine(L"PBAP connect: no answer in 20 s");
    }
}

// The pairing's end: its request answered with our PIN (once), or a minute gone; the shell's PIN back
static void PairEnd()
{
    wchar_t b[96];
    if (s_pairAsk) {
        int r = ((int (*)(int))g_real[IDX_Flc_Bluetooth_Conn_Pair_Confirm])(1);
        wsprintfW(b, L"PAIR request ev 0x%02X: Pair_Confirm(1) -> %d", (unsigned)s_pairAsk, r);
        LogLine(b);
    }
    int r = ((int (*)(char*))g_real[IDX_Flc_Bluetooth_Conn_SetPinCode])(s_oldPin);
    wsprintfW(b, L"PAIR end: PIN back -> %d", r);
    LogLine(b);
    s_pairOn = 0;
    s_pairAsk = 0;
}

// ---- Bluetooth off / on (BT_CMD_POWER, btshm.h): as the stock shell's switch, Deinit / Init; while off the
// exports answer "failed" - the stub's Av_Play / Av_Pause and Deinit at its end too (the stack's locks are gone)
static void* s_orig[PROXY_COUNT];   // the original's functions (g_real: Failed while off)

static void Exports(bool on)
{
    for (int i = 0; i < PROXY_COUNT; i++)
        g_real[i] = on ? s_orig[i] : (void*)Failed;
}

static void StateOff()   // what the stack no longer has: no phone, no call, no internet
{
    BtState& st = s_shm->st;
    InterlockedIncrement((LONG*)&s_shm->seq);
    st.ready = 0;
    st.av = st.avrcp = st.hf = st.hfAudio = 0;
    st.playing = 0;
    st.devAddr[0] = st.devName[0] = 0;
    st.call[0] = st.call[1] = BT_CALL_NONE;
    st.pan = BT_PAN_IDLE;
    InterlockedIncrement((LONG*)&s_shm->seq);
}

// The stack's PAN state variable, if this DLL is the known build (panstate.h); NULL otherwise: nothing is touched.
static volatile LONG* PanStateVar()
{
    static bool s_looked;
    static volatile LONG* s_var;
    if (s_looked)
        return s_var;
    s_looked = true;
    DWORD base = (DWORD)s_orig[IDX_Flc_Bluetooth_PanServer_Connect] - kPanConnectRva;
    bool known = false;
    __try {
        known = PanLayoutOk((const unsigned char*)base, base);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    if (known)
        s_var = (volatile LONG*)(base + kPanStateRva);
    wchar_t b[80];
    wsprintfW(b, L"PAN state: %s (base %08X)", known ? L"known build" : L"unknown build, left alone", base);
    LogLine(b);
    return s_var;
}

// A request the stack will never answer: "ready" again, so the next connect goes out (panstate.h).
static void PanUnstick(const wchar_t* why)
{
    volatile LONG* v = PanStateVar();
    if (!v)
        return;
    LONG was = *v;
    if (!PanWaiting(was))
        return;
    InterlockedCompareExchange((LONG*)v, PAN_ST_READY, was);
    wchar_t b[96];
    wsprintfW(b, L"PAN state: %d -> ready (%s, asked %u ms ago)", was, why, GetTickCount() - s_panAsked);
    LogLine(b);
}

// The layer under the SDK still holding the phone's PAN (HU sheet 30: after the state put back to "ready" every
// XCBPAN_UserConnect answered 1, failed, till a reboot): XCBPAN_UserDisconnect (btapi.dll #162, as the SDK's own
// Disconnect calls it - BD_ADDR, panstate.h PanAddr), its end the 0x46 awaited up to 3 s. Its answer, -1: not called.
static int PanLowDisconnect(const char* addr)
{
    unsigned char bd[6];
    HMODULE m = GetModuleHandleW(L"btapi.dll");
    typedef int (*DiscFn)(unsigned char*);
    DiscFn f = m ? (DiscFn)GetProcAddressW(m, (LPCWSTR)162) : NULL;
    if (!f || !PanAddr(addr, bd))
        return -1;
    ResetEvent(s_panEv);
    int r = -1;
    __try {
        r = f(bd);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        r = -2;
    }
    bool ended = (r == 0 || r == 2) && WaitForSingleObject(s_panEv, 3000) == WAIT_OBJECT_0;
    wchar_t b[96];
    wsprintfW(b, L"PAN low disconnect %S -> %d%s", addr, r, ended ? L", 0x46" : L"");
    LogLine(b);
    return r;
}

static int PowerCmd(bool on)
{
    if (s_shm->power == (on ? BT_POW_ON : BT_POW_OFF))
        return 0;
    wchar_t b[96];
    DWORD t0 = GetTickCount(), w = WAIT_FAILED;
    ResetEvent(s_powEv);
    int r;
    if (!on) {
        s_shm->power = BT_POW_GOING_OFF;
        if (s_pairOn)
            PairEnd();   // (the shell's PIN back while the stack still runs)
        // Flc_Bluetooth_Deinit's worker calls PanServer_Disconnect and PanServer_Deinit itself, but that Deinit
        // resets the state only from "ready": a request stuck in it would survive the off and on (panstate.h).
        PanUnstick(L"power off");
        r = ((FlcFn0)s_orig[IDX_Flc_Bluetooth_Deinit])();   // 2: on its way (its thread), the end - event 0x02
        bool going = r == 0 || r == 2;
        if (going) {
            w = WaitForSingleObject(s_powEv, 12000);
            Exports(false);
            StateOff();
        }
        s_shm->power = going ? BT_POW_OFF : BT_POW_ON;
        wsprintfW(b, L"POWER off: Deinit -> %d, event 0x02 %s, %u ms", r, w == WAIT_OBJECT_0 ? L"yes" : L"NO",
                  GetTickCount() - t0);
    } else {
        s_shm->power = BT_POW_GOING_ON;
        r = ((int (*)(FlcCb))s_orig[IDX_Flc_Bluetooth_Init])(ProxyCb);   // (s_appCb still the stub's)
        if (r == 0) {
            Exports(true);
            ((int (*)(int))s_orig[IDX_Flc_Bluetooth_Conn_EnablePairIndForApp])(1);
            w = WaitForSingleObject(s_powEv, 10000);
        }
        s_shm->power = r == 0 ? BT_POW_ON : BT_POW_OFF;
        wsprintfW(b, L"POWER on: Init -> %d, event 0x01 %s, %u ms", r, w == WAIT_OBJECT_0 ? L"yes" : L"NO",
                  GetTickCount() - t0);
    }
    LogLine(b);
    return r;
}

// VestaHome's commands, carried out in the shell's process by the SDK itself
static DWORD WINAPI CmdThread(LPVOID)
{
    LONG done = s_shm ? s_shm->cmdSeq : 0;
    while (s_shm) {
        WaitForSingleObject(s_cmdEv, 1000);
        if (s_pairOn && (s_pairAsk || GetTickCount() - s_pairTick > 60000))
            PairEnd();
        PbapTick();
        LONG n = s_shm->cmdSeq;
        if (n == done)
            continue;
        done = n;
        int cmd = s_shm->cmd, idx = -1;
        if (cmd == BT_CMD_POWER || s_shm->power != BT_POW_ON) {   // (off: nothing else reaches the stack)
            int r = cmd == BT_CMD_POWER ? PowerCmd(s_shm->cmdText[0] == '1') : -3;
            s_shm->cmdResult = r;
            s_shm->cmdDone = n;
            wchar_t b[64];
            wsprintfW(b, L"CMD %d -> %d", cmd, r);
            LogLine(b);
            continue;
        }
        switch (cmd) {
        case BT_CMD_PLAYPAUSE:  idx = IDX_Flc_Bluetooth_Av_PlayPause; break;
        case BT_CMD_PLAY:       idx = IDX_Flc_Bluetooth_Av_Play; break;
        case BT_CMD_PAUSE:      idx = IDX_Flc_Bluetooth_Av_Pause; break;
        case BT_CMD_NEXT:       idx = IDX_Flc_Bluetooth_Av_Forward; break;
        case BT_CMD_PREV:       idx = IDX_Flc_Bluetooth_Av_Backward; break;
        case BT_CMD_MEDIAINFO:  idx = IDX_Flc_Bluetooth_Av_GetMediaInfo; break;
        case BT_CMD_PLAYSTATUS: idx = IDX_Flc_Bluetooth_Av_GetPlayStatus; break;
        case BT_CMD_AVSTATE:    idx = IDX_Flc_Bluetooth_Av_GetConnectState; break;
        case BT_CMD_FF_PRESS:   idx = IDX_Flc_Bluetooth_Av_FF_Press; break;
        case BT_CMD_FF_RELEASE: idx = IDX_Flc_Bluetooth_Av_FF_Release; break;
        case BT_CMD_REW_PRESS:  idx = IDX_Flc_Bluetooth_Av_Rewind_Press; break;
        case BT_CMD_REW_RELEASE: idx = IDX_Flc_Bluetooth_Av_Rewind_Release; break;
        }
        int r = -1;
        if (idx >= 0)
            r = ((FlcFn0)g_real[idx])();
        else if (cmd == BT_CMD_ANSWER)   // the first phone (line 0), as the shell's
            r = ((int (*)(int))g_real[IDX_Flc_Bluetooth_Hf_Answer])(0);
        else if (cmd == BT_CMD_END)
            r = ((int (*)(int))g_real[IDX_Flc_Bluetooth_Hf_End])(0);
        else if (cmd == BT_CMD_CONNECT) {   // as NCL_BT connects a device (FUN_100057fc, Ghidra 2026-10-19)
            char addr[40];
            CopyText(addr, s_shm->cmdText, 18);
            r = ((int (*)(char*, int))g_real[IDX_Flc_Bluetooth_Hf_Connect])(addr, 0);
            int r2 = ((FlcFn0)g_real[IDX_Flc_Bluetooth_Av_Connect])();
            r = r * 100 + r2;
        } else if (cmd == BT_CMD_AVCONNECT) {
            r = ((FlcFn0)g_real[IDX_Flc_Bluetooth_Av_Connect])();
        } else if (cmd == BT_CMD_PLAYON) {   // as NCL_BT's play (FUN_10004a5c): unmute, then play
            ((int (*)(int))g_real[IDX_Flc_Bluetooth_Av_Mute])(0);
            r = ((FlcFn0)g_real[IDX_Flc_Bluetooth_Av_Play])();
        } else if (cmd == BT_CMD_SEARCH) {
            r = ((FlcFn0)g_real[IDX_Flc_Bluetooth_Conn_Search])();
        } else if (cmd == BT_CMD_SEARCHSTOP) {
            r = ((FlcFn0)g_real[IDX_Flc_Bluetooth_Conn_Cancel_Search])();
        } else if (cmd == BT_CMD_PAIR) {   // "addr pin": our PIN while it pairs, the shell's kept in s_oldPin
            char t[40];
            CopyText(t, s_shm->cmdText, 40);
            if (s_pairOn)
                PairEnd();
            CopyText(s_pairAddr, t, 18);
            memset(s_oldPin, 0, sizeof(s_oldPin));
            ((int (*)(char*))g_real[IDX_Flc_Bluetooth_Conn_GetPinCode])(s_oldPin);
            s_oldPin[16] = 0;
            if (strlen(t) > 18)
                ((int (*)(char*))g_real[IDX_Flc_Bluetooth_Conn_SetPinCode])(t + 18);
            s_pairAsk = 0;
            s_pairTick = GetTickCount();
            s_pairOn = 1;
            r = ((int (*)(char*))g_real[IDX_Flc_Bluetooth_Conn_Pair])(s_pairAddr);
        } else if (cmd == BT_CMD_LISTPAIRED || cmd == BT_CMD_UNPAIR) {   // the paired list anew (after a removal)
            if (cmd == BT_CMD_UNPAIR) {
                char a[40];
                CopyText(a, s_shm->cmdText, 18);
                r = ((int (*)(char*))g_real[IDX_Flc_Bluetooth_Conn_Delete_Paired])(a);
            }
            InterlockedIncrement((LONG*)&s_shm->seq);   // the list from scratch: the events refill it
            s_shm->st.paired = 0;
            InterlockedIncrement((LONG*)&s_shm->seq);
            int r2 = ((FlcFn0)g_real[IDX_Flc_Bluetooth_Conn_List_History])();
            if (cmd == BT_CMD_LISTPAIRED)
                r = r2;
        } else if (cmd == BT_CMD_SPPADDR) {
            char a[40];
            CopyText(a, s_shm->cmdText, 18);
            r = ((int (*)(char*))g_real[IDX_Flc_Bluetooth_Spp_SetAddr])(a);
        } else if (cmd == BT_CMD_PAN) {   // the phone's internet: connect to its NAP (0x1116), or let it go
            char a[40];
            CopyText(a, s_shm->cmdText, 18);
            bool busy = false;
            if (a[0]) {
                volatile LONG* v = PanStateVar();
                if (v && PanDeadRequest(*v, GetTickCount() - s_panAsked))
                    PanUnstick(L"connect");
                s_panAsked = GetTickCount();
                r = ((int (*)(char*, unsigned short))g_real[IDX_Flc_Bluetooth_PanServer_Connect])(a, 0x1116);
                if (r == 1 && PanLowDisconnect(a) >= 0) {   // refused below the SDK: let the old link go, once more
                    s_panAsked = GetTickCount();
                    r = ((int (*)(char*, unsigned short))g_real[IDX_Flc_Bluetooth_PanServer_Connect])(a, 0x1116);
                }
                // 0 / 2: on its way - the event tells the end; but 0 is also the SDK's silent "not ready": nothing
                // went out then, and the state tells which
                busy = r == 0 && v && *v != PAN_ST_CONNECTING;
                InterlockedIncrement((LONG*)&s_shm->seq);
                s_shm->st.pan = (r == 0 || r == 2) && !busy ? BT_PAN_CONNECTING : BT_PAN_FAILED;
                s_shm->st.panTick = GetTickCount();
                InterlockedIncrement((LONG*)&s_shm->seq);
            } else {   // (sheet 22: a phone gone quiet sends no event - the state stayed "online", VestaHome never
                ResetEvent(s_panEv);
                bool online = s_shm->st.pan == BT_PAN_ONLINE;
                s_panAsked = GetTickCount();
                r = ((FlcFn0)g_real[IDX_Flc_Bluetooth_PanServer_Disconnect])();   // connected again)
                bool sent = r == 0 || r == 2;   // (from "connected" it answers 2 and its end is 0x46: logs 2026-11-05)
                if (sent && online && WaitForSingleObject(s_panEv, 3000) != WAIT_OBJECT_0)
                    LogLine(L"PAN off: no 0x46 in 3 s");
                PanUnstick(L"off");   // (no answer to a connect, a silent phone: the stack waits for ever)
                if (sent) {
                    InterlockedIncrement((LONG*)&s_shm->seq);
                    s_shm->st.pan = BT_PAN_IDLE;
                    s_shm->st.panTick = GetTickCount();
                    InterlockedIncrement((LONG*)&s_shm->seq);
                }
            }
            wchar_t b[80];
            wsprintfW(b, L"PAN %S -> %d%s", a[0] ? a : "off", r, busy ? L" (not ready: nothing sent)" : L"");
            LogLine(b);
        } else if (cmd == BT_CMD_PHONE) {   // the phone off for the OBD adapter / the stack's connects back
            if (s_shm->cmdText[0] == '0') {
                ((int (*)(int))g_real[IDX_Flc_Bluetooth_Conn_EnablePowerOnConnect])(0);
                ((int (*)(int))g_real[IDX_Flc_Bluetooth_Conn_EnableLinkLostConnect])(0);
                int p = ((FlcFn0)g_real[IDX_Flc_Bluetooth_PanServer_Disconnect])();
                int a = ((FlcFn0)g_real[IDX_Flc_Bluetooth_Av_Disconnect])();
                r = ((int (*)(int))g_real[IDX_Flc_Bluetooth_Hf_Disconnect])(0);
                wchar_t b[64];
                wsprintfW(b, L"PHONE off: PAN %d, AV %d, HF %d", p, a, r);
                LogLine(b);
            } else {   // both on (HU 2026-11-04: the state kept from after a restart was off - the phone
                ((int (*)(int))g_real[IDX_Flc_Bluetooth_Conn_EnablePowerOnConnect])(1);   // no longer came back)
                r = ((int (*)(int))g_real[IDX_Flc_Bluetooth_Conn_EnableLinkLostConnect])(1);
                LogLine(L"PHONE on: power-on 1, link lost 1");
            }
        } else if (cmd == BT_CMD_SDP) {
            char a[40];
            CopyText(a, s_shm->cmdText, 18);
            r = ((int (*)(char*))g_real[IDX_Flc_Bluetooth_Conn_Search_SC_IAP])(a);
        } else if (cmd == BT_CMD_SETOPT) {   // as the shell's CSetUpBTMenuDlg: on / off, the answer after 5 s
            char t[8];
            CopyText(t, s_shm->cmdText, 8);
            int on = t[2] == '1';
            if (t[0] == 'c')
                r = ((int (*)(int))g_real[IDX_Flc_Bluetooth_Conn_EnablePowerOnConnect])(on);
            else
                r = ((int (*)(int, int))g_real[IDX_Flc_Bluetooth_Conn_EnableAutoAnswerIncomingCall])(on, 5);
        } else if (cmd == BT_CMD_VISIBLE) {   // the scan mode: seen by phones / only connectable
            r = s_shm->cmdText[0] == '1' ? ((FlcFn0)g_real[IDX_Flc_Bluetooth_Conn_Enter_Pairing])()
                                         : ((FlcFn0)g_real[IDX_Flc_Bluetooth_Conn_Exit_Pairing])();
            wchar_t b[48];
            wsprintfW(b, L"VISIBLE %c -> %d", s_shm->cmdText[0] == '1' ? L'1' : L'0', r);
            LogLine(b);
        } else if (cmd == BT_CMD_PAIRCONFIRM) {   // a phone's request (BtPair) accepted / refused
            r = ((int (*)(int))g_real[IDX_Flc_Bluetooth_Conn_Pair_Confirm])(s_shm->cmdText[0] == '1' ? 1 : 0);
            wchar_t b[48];
            wsprintfW(b, L"PAIR_CONFIRM %c -> %d", s_shm->cmdText[0] == '1' ? L'1' : L'0', r);
            LogLine(b);
        } else if (cmd == BT_CMD_GETOPTS) {
            int c = ((FlcFn0)g_real[IDX_Flc_Bluetooth_Conn_GetPowerOnConnectStatus])() & 0xFF;
            int a = ((FlcFn0)g_real[IDX_Flc_Bluetooth_Conn_GetAutoAnswerIncomingCallStatus])() & 0xFF;
            r = (c ? 1 : 0) | (a ? 2 : 0);
        } else if (cmd == BT_CMD_PBAP_PULL) {
            char t[40];
            CopyText(t, s_shm->cmdText, 40);
            r = PbapCmd(t);
        } else if (cmd == BT_CMD_DIAL) {   // Hf_DialNumber(number, its length, line)
            char num[40];
            CopyText(num, s_shm->cmdText, 40);
            r = ((int (*)(const char*, unsigned short, int))g_real[IDX_Flc_Bluetooth_Hf_DialNumber])(
                num, (unsigned short)strlen(num), 0);
        }
        s_shm->cmdResult = r;
        s_shm->cmdDone = n;
        wchar_t b[64];
        wsprintfW(b, L"CMD %d -> %d", cmd, r);
        LogLine(b);
    }
    return 0;
}

extern "C" int proxy_Flc_Bluetooth_Init(FlcCb cb)
{
    s_appCb = cb;
    if (s_shm)
        InterlockedIncrement((LONG*)&s_shm->inits);
    int r = ((int (*)(FlcCb))g_real[IDX_Flc_Bluetooth_Init])(cb ? ProxyCb : NULL);
    wchar_t b[64];
    wsprintfW(b, L"Flc_Bluetooth_Init -> %d", r);
    LogLine(b);
    if (!s_thread && s_shm && s_cmdEv)   // not in DllMain (the loader lock): here, once the shell uses us
        s_thread = CreateThread(NULL, 0, CmdThread, NULL, 0, NULL);
    return r;
}

BOOL WINAPI DllMain(HANDLE hInstDLL, DWORD fdwReason, LPVOID)
{
    if (fdwReason == DLL_PROCESS_ATTACH) {
        wchar_t path[MAX_PATH];
        GetModuleFileNameW((HMODULE)hInstDLL, path, MAX_PATH);
        wchar_t* slash = wcsrchr(path, L'\\');
        if (slash)
            slash[1] = 0;
        else
            path[0] = 0;
        wcscat(path, L"FLC_BLUETOOTH_SDK_orig.dll");
        s_real = LoadLibraryW(path);
        int resolved = 0;
        for (int i = 0; i < PROXY_COUNT; i++) {
            g_real[i] = s_real ? (void*)GetProcAddressW(s_real, kProxyNames[i]) : NULL;
            if (g_real[i])
                resolved++;
            else
                g_real[i] = (void*)Failed;
            s_orig[i] = g_real[i];
        }
        s_powEv = CreateEventW(NULL, FALSE, FALSE, NULL);
        s_panEv = CreateEventW(NULL, FALSE, FALSE, NULL);
        s_map = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(BtShm), BT_SHM_NAME);
        if (s_map) {
            s_shm = (BtShm*)MapViewOfFile(s_map, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(BtShm));
            if (s_shm && s_shm->magic != BT_SHM_MAGIC) {
                memset(&s_shm->st, 0, sizeof(s_shm->st));
                s_shm->st.av = s_shm->st.avrcp = s_shm->st.hf = s_shm->st.hfAudio = -1;
                s_shm->st.battery = s_shm->st.signal = -1;
                s_shm->version = 5;
                s_shm->magic = BT_SHM_MAGIC;
            }
            if (s_shm) {
                InterlockedIncrement((LONG*)&s_shm->attach);
                s_shm->origOk = s_real != NULL;
            }
        }
        s_pbMap = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(BtPb), BT_PB_NAME);
        if (s_pbMap) {
            s_pb = (BtPb*)MapViewOfFile(s_pbMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(BtPb));
            if (s_pb && s_pb->magic != BT_SHM_MAGIC) {   // a new mapping comes zeroed
                s_pb->pbap = -1;
                s_pb->magic = BT_SHM_MAGIC;
            }
        }
        s_pairMap = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(BtPair), BT_PAIR_NAME);
        if (s_pairMap) {
            s_pairShm = (BtPair*)MapViewOfFile(s_pairMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(BtPair));
            if (s_pairShm)
                s_pairShm->magic = BT_SHM_MAGIC;
        }
        s_cmdEv = CreateEventW(NULL, FALSE, FALSE, BT_CMD_EVENT);
        wchar_t b[96];
        wsprintfW(b, L"BTPROXY %s orig=%d exports=%d/%d shm=%d", s_real ? L"OK" : L"NO ORIGINAL", s_real ? 1 : 0,
                  resolved, PROXY_COUNT, s_shm ? 1 : 0);
        LogLine(b);
    }
    // DLL_PROCESS_DETACH: nothing is undone - the SDK may still call our callback while the process goes down
    return TRUE;
}
