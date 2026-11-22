// vh_mcu.inc - part of home.cpp (included there, in order): MCU: the proxy's messages, the tuner, MCU calls.

// ---- MCU: what the head unit's microcontroller sends the shell (climate, keys, versions...), seen through our
// SPIPowerMgr proxy (VestaHome/SPIPower): it copies every inbound message into a named shared ring. Layout =
// SPIPower/native/mcushm.h (keep both identical). Without the proxy the ring stays empty (magic 0).
#define WM_APP_MCU (WM_APP + 20)
#define MCU_SHM_NAME   L"VestaMCU2"   // v2: 40 data bytes
#define MCU_SHM_MAGIC  0x55434D56
#define MCU_SHM_SLOTS  256
#define MCU_SHM_DATA   40   // v2, as SPIPower/native/mcushm.h

struct McuMsg {
    volatile LONG  seq;
    DWORD          tick;
    unsigned short id;
    unsigned char  len, skel;
    unsigned char  data[MCU_SHM_DATA];
};
struct McuShm {
    DWORD         magic, version;
    volatile LONG seq, attach, wraps;
    DWORD         reserved[3];
    McuMsg        msg[MCU_SHM_SLOTS];
};

// One line per message id for the monitor.
struct McuStat {
    unsigned short id;
    unsigned char  len, skel;
    DWORD          count, tick;
    unsigned char  data[MCU_SHM_DATA];
};
enum { kMcuStatMax = 96, kMcuLogMax = 4 * 1024 * 1024 };

// The climate as G3NavHMI decodes it (CHvac::MsgDecode, PROJECT.md): value = payload[1]; 0x6109 = kind
// payload[1] (1 outside, 2 right zone, 3 driver) with the value in payload[3]. -1 = not seen yet.
struct Hvac {
    int   outer;          // °C (raw - 40)
    int   temp;           // the driver's knob, 0..16
    int   blower;         // 1..7
    int   mode;           // air direction 0..4
    int   ac, autoMode, rear, cycle, power;
    DWORD tick;           // the last climate message
};

static CRITICAL_SECTION s_mcuCs;
static McuStat  s_mcuStat[kMcuStatMax];
static int      s_mcuStatCount;
static DWORD    s_mcuTotal;              // messages taken from the ring since start
static McuShm*  s_shm;
static HANDLE   s_shmMap;
static HANDLE   s_mcuThread;
static volatile bool s_mcuStop;
static HANDLE   s_mcuLog = INVALID_HANDLE_VALUE;
static DWORD    s_mcuLogBytes;
static Hvac     s_hvac = { -999, -1, -1, -1, -1, -1, -1, -1, -1, 0 };
static volatile LONG s_mcuPosted;

// The sound as the MCU reports it (mcu.log 2026-10-17, PROJECT.md): 0x3101 00 00 <n> = the volume of the current
// source (the stock shell's scale is 0..30 - user 2026-10-19; up to 50 taken), 0x3109 00 <0/1> = mute, 0x4904 00 <n> = the
// source (00 radio, 06 and 07 the media player - USB, SD, video alike (2026-10-18), 09 Bluetooth). -1 = not seen yet. A change of the volume or the mute opens
// the volume window (the same values sent again - the tuner repeats them - do not).
enum { kVolMax = 50, WP_MCU_VOLUME = 1, WP_MCU_MARK = 2, WP_MCU_TUNER = 3, WP_MCU_REBOOT = 4, WP_MCU_SOUND = 5,
       WP_MCU_DAYNIGHT = 6, WP_MCU_HWKEY = 7 };
// The screen (2026-10-17 photos + Ghidra on the shell's CSetUpDisplayDlg 2026-10-18): 0x4105 00 <0 day, 1 night>,
// 0x4107 00 <backlight 1..10> (10 by day, 1 at night as set); the shell's day / night buttons are keys 0x64 / 0x65,
// its brightness slider SPApi_SysSendRadioBackLight(0..10) (the current mode's level).
// The sound settings as the MCU reports them (SPIPower.log 2026-10-18, also after the stock shell's changes):
// 0x3102 / 0x3103 / 0x3104 00 <0..12> = low / mid / high (6 = 0), 0x3105 00 <balance> <fader> (8 = the centre).
// Ghidra 2026-11-10 (G3NavHMI CAudio::MsgDecode FUN_00666028): 0x310A 00 <v> = loudness (1 on; CAudioEffectDlg),
// 0x3108 00 <t> <v> (t 0, 1 or 4) = "Алгоритм изменения громкости" (v != 0 on; CAudioFieldDlg), 0x3101 00 <type> <v> =
// the volumes: type 0 the current one, 5 the start's, 1 the phone's (Bluetooth), 8 the traffic announcements' (TA).
// (HU 2026-11-11: the MCU sent neither 0x310A nor 0x3108 - the switches keep their own state, s_cfg.)
struct McuSound { int eq[3], bal, fad; };   // VestaHome's units (-6..6, -8..8); 99 = not seen
static McuSound s_mcuSnd = { { 99, 99, 99 }, 99, 99 };
static int      s_vol = -1, s_mute = -1, s_src = -1;
static volatile LONG s_volPosted;

static Tuner s_tun;

// "About the system -> Details" from the MCU (mcu.log, the engineering menu's photos; PROJECT.md): 0x7902 00 <00 SW |
// 03 HW> 00 00 <len> <text>, 0x7902 00 00 01 <CS hi lo>; 0x7922 00 06 <BT address>; the machine ID = 0x4118
// 00 <4 bytes> + 0x4111 00 20 <3 bytes> + 0x4112 00 <text>; System status 0x7905 00 <sub> <value> (5 bytes:
// one byte - 0A navigation board °C, 06 amplifier °C, 07 the tuner's field dB; 4 bytes: two - 03 / 0E the
// steering wheel buttons' voltages, as the shell shows them).
struct McuInfo {
    wchar_t sw[40], hw[16], id[16], idDate[8], idNum[12];
    int     cs;                 // -1 not seen
    BYTE    bt[6];
    bool    btHave;
    int     sys[16];            // by sub, -1 not seen
};
static McuInfo s_info;

static bool McuBtAddr(wchar_t* s, int n)   // the unit's Bluetooth address, "AA:BB:…"
{
    EnterCriticalSection(&s_mcuCs);
    bool have = s_info.btHave;
    if (have)
        _snwprintf(s, n, L"%02X:%02X:%02X:%02X:%02X:%02X", s_info.bt[0], s_info.bt[1], s_info.bt[2], s_info.bt[3],
                   s_info.bt[4], s_info.bt[5]);
    LeaveCriticalSection(&s_mcuCs);
    return have;
}
static void McuText(const unsigned char* p, int n, wchar_t* out, bool lead);

static void McuInfoTake(const McuMsg& m)   // under s_mcuCs
{
    const unsigned char* p = m.data;
    int len = m.len < MCU_SHM_DATA ? m.len : MCU_SHM_DATA;
    if (m.id == 0x7902 && len >= 5) {
        if (p[2] == 1)
            s_info.cs = p[3] << 8 | p[4];
        else if (len > 5) {
            wchar_t* out = p[1] == 3 ? s_info.hw : s_info.sw;
            int cap = p[1] == 3 ? 16 : 40, k = 0;
            for (int i = 5; i < len && k < cap - 1 && p[i] >= 0x20 && p[i] < 0x7F; i++)
                out[k++] = p[i];
            while (k > 0 && out[k - 1] == L'.')
                k--;
            out[k] = 0;
        }
    } else if (m.id == 0x7922 && len >= 8 && p[1] == 6) {
        memcpy(s_info.bt, p + 2, 6);
        s_info.btHave = true;
    } else if (m.id == 0x4118 && len >= 5) {
        _snwprintf(s_info.id, 16, L"%02X%02X%02X%02X", p[1], p[2], p[3], p[4]);
    } else if (m.id == 0x4111 && len >= 5) {
        _snwprintf(s_info.idDate, 8, L"%02X%02X%02X", p[2], p[3], p[4]);
    } else if (m.id == 0x4112 && len >= 3) {
        McuText(p + 1, len - 1 < 11 ? len - 1 : 11, s_info.idNum, true);
    } else if (m.id == 0x7905 && len >= 4 && p[1] < 16) {
        s_info.sys[p[1]] = len == 4 ? p[2] << 8 | p[3] : p[2];
    }
}

static bool s_mcuCsReady;   // McuStart has made s_mcuCs (SaveCfg may run before it)

// The radio's last state in the ini (the MCU sends it only on a change or when asked, HU 2026-10-18-2: VestaHome
// started after the shell showed nothing until the radio screen asked) and the stations' names: from the MCU's list
// and learned from RDS while listening - for the presets, which the MCU sends as frequencies only.
enum { kRadioNames = 48 };
static struct { int f10; wchar_t name[9]; } s_rnames[kRadioNames];
static int s_rnameCount;
static bool s_radioDirty;   // to be written (at most every 30 s)
static bool  s_cfgSoon;      // the ini to be written a minute after the first change not written (CfgSoon):
static DWORD s_cfgSoonTick;  // at most once a minute, and only when something changed

static void CfgSoon()
{
    if (!s_cfgSoon) {
        s_cfgSoon = true;
        s_cfgSoonTick = GetTickCount();
    }
}

static const wchar_t* RadioName(const Tuner& t, int f10)   // "" when not known
{
    for (int i = 0; i < t.listCount && i < kTunList; i++)
        if (t.list[i] == f10 && t.listName[i][0])
            return t.listName[i];
    for (int i = 0; i < s_rnameCount; i++)
        if (s_rnames[i].f10 == f10)
            return s_rnames[i].name;
    return L"";
}

static void RadioNameLearn(int f10, const wchar_t* ps)
{
    if (f10 <= 0 || !ps[0])
        return;
    int i = 0;
    while (i < s_rnameCount && s_rnames[i].f10 != f10)
        i++;
    if (i == s_rnameCount) {
        if (s_rnameCount == kRadioNames)
            i = s_rnameCount - 1;   // full: the last one goes
        else
            s_rnameCount++;
    } else if (!wcscmp(s_rnames[i].name, ps)) {
        return;
    }
    s_rnames[i].f10 = f10;
    wcsncpy(s_rnames[i].name, ps, 8);
    s_rnames[i].name[8] = 0;
    s_radioDirty = true;
}

static void RadioCacheLoad(const wchar_t* k, const wchar_t* v)
{
    if (!wcscmp(k, L"radio.cache")) {   // f10,band,cur,presets,p0..p23
        int a[28], n = 0;
        const wchar_t* q = v;
        while (n < 28 && *q) {
            a[n++] = _wtoi(q);
            while (*q && *q != L',')
                q++;
            if (*q)
                q++;
        }
        if (n >= 4) {
            s_tun.f10 = a[0];
            s_tun.band = a[1];
            s_tun.cur = a[2];
            s_tun.presets = a[3];
            for (int i = 0; i < 24 && 4 + i < n; i++)
                s_tun.preset[i] = a[4 + i];
        }
    } else if (!wcscmp(k, L"radio.cvol")) {   // volume, mute, source: the last ones the MCU sent
        int a[3] = { -1, -1, -1 }, n = 0;
        const wchar_t* q = v;
        while (n < 3 && *q) {
            a[n++] = _wtoi(q);
            while (*q && *q != L',')
                q++;
            if (*q)
                q++;
        }
        if (n == 3 && a[0] >= 0 && a[0] <= kVolMax) {
            s_vol = a[0];
            s_mute = a[1];
            s_src = a[2];
        }
    } else if (!wcscmp(k, L"radio.cset")) {   // known bits, value bits
        int known = _wtoi(v), val = 0;
        const wchar_t* c = wcschr(v, L',');
        if (c)
            val = _wtoi(c + 1);
        s_tun.setKnown = known;
        for (int i = 0; i < RSET_COUNT; i++)
            s_tun.set[i] = val >> i & 1;
    } else if (!wcscmp(k, L"radio.names")) {   // f10=NAME;...
        const wchar_t* q = v;
        while (*q && s_rnameCount < kRadioNames) {
            int f = _wtoi(q);
            const wchar_t* e = wcschr(q, L'=');
            if (!e)
                break;
            e++;
            int k2 = 0;
            wchar_t nm[9];
            while (e[k2] && e[k2] != L';' && k2 < 8) {
                nm[k2] = e[k2];
                k2++;
            }
            nm[k2] = 0;
            if (f > 0 && nm[0]) {
                s_rnames[s_rnameCount].f10 = f;
                wcscpy(s_rnames[s_rnameCount].name, nm);
                s_rnameCount++;
            }
            q = wcschr(e, L';');
            if (!q)
                break;
            q++;
        }
    }
}

static int RadioCacheText(wchar_t* out, int cap)
{
    Tuner t;
    if (s_mcuCsReady)
        EnterCriticalSection(&s_mcuCs);
    t = s_tun;
    if (s_mcuCsReady)
        LeaveCriticalSection(&s_mcuCs);
    int n = _snwprintf(out, cap, L"# radio.*: the tuner's last state and the stations' names (VestaHome's cache)\r\n"
                       L"radio.cache=%d,%d,%d,%d", t.f10, t.band, t.cur, t.presets);
    if (n < 0)
        return 0;
    for (int i = 0; i < 24 && n < cap - 16; i++)
        n += _snwprintf(out + n, cap - n, L",%d", t.preset[i]);
    int val = 0;
    for (int i = 0; i < RSET_COUNT; i++)
        val |= (t.set[i] ? 1 : 0) << i;
    int k = _snwprintf(out + n, cap - n, L"\r\nradio.cvol=%d,%d,%d\r\nradio.cset=%d,%d\r\nradio.names=", s_vol, s_mute,
                       s_src, t.setKnown, val);
    n += k > 0 ? k : 0;
    for (int i = 0; i < s_rnameCount && n < cap - 32; i++) {
        k = _snwprintf(out + n, cap - n, L"%d=%s;", s_rnames[i].f10, s_rnames[i].name);
        n += k > 0 ? k : 0;
    }
    k = _snwprintf(out + n, cap - n, L"\r\n");
    n += k > 0 ? k : 0;
    return n < cap ? n : cap - 1;
}

static void TunerSet(int i, int v)   // under s_mcuCs
{
    s_tun.set[i] = v != 0;
    s_tun.setKnown |= 1 << i;
}
static volatile LONG s_tunPosted;

// The MCU's ASCII, trailing spaces cut; lead = the leading ones too (the RDS name is centred in its 8 characters;
// a RadioText part is not: its second half may start with the space between two words).
static void McuText(const unsigned char* p, int n, wchar_t* out, bool lead)
{
    int k;
    while (lead && n > 0 && *p == ' ') {
        p++;
        n--;
    }
    for (k = 0; k < n; k++)
        out[k] = p[k] >= 0x20 && p[k] < 0x7F ? (wchar_t)p[k] : L' ';
    while (k > 0 && out[k - 1] == L' ')
        k--;
    out[k] = 0;
}

static void McuLogLine(const char* s)   // under s_mcuCs
{
    if (s_mcuLog == INVALID_HANDLE_VALUE)
        return;
    DWORD len = (DWORD)strlen(s), w = 0;
    if (s_mcuLogBytes + len > kMcuLogMax || !WriteFile(s_mcuLog, s, len, &w, NULL)) {
        CloseHandle(s_mcuLog);
        s_mcuLog = INVALID_HANDLE_VALUE;
        return;
    }
    s_mcuLogBytes += w;
}

// One message: the per-id table, the climate, the record ("<uptime s> <id> s<skeleton> <len> : hex |text|").
// The MCU's menu (Ghidra + the shell's logs, 2026-10-29): 0x1102 00 16 - its main menu, the only place it takes the
// main menu's keys (0x7F radio, 0x84 BT); a player's request (0x9101), Bluetooth's screens (0x8902, 0x8905) - a level
// below, left by "back" 0x30 one level a press (vh_screens.inc MainMenuKey)
static volatile bool  s_menuMain;
static volatile DWORD s_menuTick;   // the last of these messages (0: none yet)
// Its settings (Ghidra + the shell's logs, 2026-10-29): key 0x13 from the main menu -> 0x3903 00 00; then the sound
// pages, key 0xE6 -> 0x4102 00 10 (equalizer) and 0xE4 -> 0x4102 00 0C (balance) - only there it takes the values
static volatile DWORD s_settingsTick;   // 0x3903 came
static volatile int   s_sndPageNow;     // 0xE6 / 0xE4 - the MCU on that sound page; 0 elsewhere

static void McuTake(const McuMsg& m)
{
    const unsigned char* p = m.data;
    bool hvac = false;
    EnterCriticalSection(&s_mcuCs);
    s_mcuTotal++;
    int i = 0;
    while (i < s_mcuStatCount && s_mcuStat[i].id != m.id)
        i++;
    if (i == s_mcuStatCount && s_mcuStatCount < kMcuStatMax) {
        memset(&s_mcuStat[i], 0, sizeof(McuStat));
        s_mcuStat[i].id = m.id;
        s_mcuStatCount++;
    }
    if (i < s_mcuStatCount) {
        McuStat& st = s_mcuStat[i];
        st.count++;
        st.tick = m.tick;
        st.len = m.len;
        st.skel = m.skel;
        memcpy(st.data, m.data, MCU_SHM_DATA);
    }
    if ((m.id >> 8) == 0x61) {
        hvac = true;
        switch (m.id) {
        case 0x6109:
            if (p[1] == 1) s_hvac.outer = p[3] - 40;
            else if (p[1] == 3) s_hvac.temp = p[3];
            break;
        case 0x610C: s_hvac.blower = p[1]; break;
        case 0x610D: s_hvac.mode = p[1]; break;
        case 0x610B: s_hvac.ac = p[1]; break;
        case 0x610A: s_hvac.autoMode = p[1]; break;
        case 0x6107: s_hvac.rear = p[1]; break;
        case 0x610F: s_hvac.cycle = p[1]; break;
        case 0x6106: s_hvac.power = p[1]; break;
        default: hvac = false;
        }
        if (hvac)
            s_hvac.tick = GetTickCount();
    }
    bool volume = false, mark = false;
    if (m.id == 0x3101 && p[1] == 0 && p[2] <= kVolMax) {
        volume = s_vol >= 0 && s_vol != p[2];
        if (s_vol != p[2])
            s_radioDirty = true;   // the cache in the ini
        s_vol = p[2];
    } else if (m.id == 0x3109) {
        volume = s_mute >= 0 && s_mute != (p[1] != 0);
        mark = s_mute == 0 && p[1] != 0;   // MUTE pressed: a mark in the records (the hardware key - an on-screen
                                           // MARK would change the screen being tested; 2026-10-17)
        s_mute = p[1] != 0;
    } else if (m.id == 0x4904) {   // the source: the volume window with its name too (the user 2026-10-29: MODE
        volume = s_src >= 0 && s_src != p[1];   // media -> BT showed nothing - their levels are the same)
        s_src = p[1];
    }
    bool dayNight = false;
    if (m.id == 0x4105 && p[1] <= 1) {
        dayNight = s_mcuNight != p[1];
        s_mcuNight = p[1];
    } else if (m.id == 0x4107 && p[1] <= 10) {
        dayNight = s_bright != p[1];
        s_bright = p[1];
    }
    bool sound = false;
    if (m.id >= 0x3102 && m.id <= 0x3104 && p[1] <= 12) {
        sound = s_mcuSnd.eq[m.id - 0x3102] != p[1] - 6;
        s_mcuSnd.eq[m.id - 0x3102] = p[1] - 6;
    } else if (m.id == 0x3105 && p[1] <= 16 && p[2] <= 16) {
        sound = s_mcuSnd.bal != p[1] - 8 || s_mcuSnd.fad != p[2] - 8;
        s_mcuSnd.bal = p[1] - 8;
        s_mcuSnd.fad = 8 - p[2];   // (the MCU's fader is ours reversed: BalanceSend)
    }
    static bool s_infoInit;
    if (!s_infoInit) {
        s_infoInit = true;
        s_info.cs = -1;
        for (int k = 0; k < 16; k++)
            s_info.sys[k] = -1;
    }
    McuInfoTake(m);
    bool tuner = true;
    switch (m.id) {
    case 0x0901: {
        int f = p[2] << 8 | p[3];
        if (f != s_tun.f10) {   // another station: its RDS name and text are not the old one's
            s_tun.ps[0] = 0;
            s_tun.rt[0][0] = s_tun.rt[1][0] = 0;
            s_tun.fTick = GetTickCount();
        }
        s_tun.f10 = f;
        s_tun.band = p[1] <= 2 ? p[1] : 0;
        s_tunLive = true;
        s_tun0901++;
        break;
    }
    case 0x0904: if (p[1] < 8) s_tun.flags[p[1]] = p[2]; break;
    case 0x0930:   // station names, 4 a message
        for (int k = 0; k < 4 && p[1] + k <= p[2] && p[1] + k < kTunList; k++)
            McuText(p + 4 + k * 8, 8, s_tun.listName[p[1] + k], true);
        break;
    case 0x0907: s_tun.cur = PresetPlaying(p[2] == 0xFF ? -1 : p[2], s_presetSent); break;   // (a place 0..7)
    case 0x0908:
        if (p[1] == 0 && p[3] < kTunList && p[4] <= kTunList) {   // the station list
            s_tun.listCount = p[4];
            for (int k = 0; k < 8 && p[2] + k < kTunList; k++)
                s_tun.list[p[2] + k] = p[5 + 2 * k] << 8 | p[6 + 2 * k];
        }
        if (p[1] == 1 && p[3] < 24 && p[4] <= 24) {
            s_tun.presets = p[4];
            for (int k = 0; k < 8 && p[2] + k < 24; k++)
                s_tun.preset[p[2] + k] = p[5 + 2 * k] << 8 | p[6 + 2 * k];
        }
        break;
    case 0x0909:   // a preset saved: 00 01 <preset> <frequency BE> (no new 0x0908 follows - HU 2026-10-20)
        if (p[1] == 1 && p[2] < 24 && m.len >= 5)
            s_tun.preset[p[2]] = p[3] << 8 | p[4];
        break;
    case 0x090B: McuText(p + 3, 8, s_tun.ps, true); break;
    case 0x0931: if (p[3] < 2) McuText(p + 4, 32, s_tun.rt[p[3]], false); break;   // 2 x 32 = 64, RDS's RT
    // the radio's settings (SPIPower.log 2026-10-18: the reply to each settings key, and all of them to
    // the extended key 0x90): 0x0920 RDS, TA; 0x0921 -, AF; 0x0924 -, CT; 0x092D -, MS; 0x092E noise, MW
    case 0x0920: TunerSet(RSET_RDS, p[1]); TunerSet(RSET_TA, p[2]); break;
    case 0x0921: TunerSet(RSET_AF, p[2]); break;
    case 0x0924: TunerSet(RSET_CT, p[2]); break;
    case 0x092D: TunerSet(RSET_MS, p[2]); break;
    case 0x092E: TunerSet(RSET_NOISE, p[1]); TunerSet(RSET_MW, p[2]); break;
    default: tuner = false;
    }
    if (s_mcuLog != INVALID_HANDLE_VALUE) {
        char line[200];
        int n = _snprintf(line, 120, "%u.%03u %04X s%u %u :", (unsigned)(m.tick / 1000), (unsigned)(m.tick % 1000),
                          (unsigned)m.id, (unsigned)m.skel, (unsigned)m.len);
        int k, shown = m.len < MCU_SHM_DATA ? m.len : MCU_SHM_DATA;
        for (k = 0; k < shown && n < 180; k++)
            n += _snprintf(line + n, 200 - n, " %02X", p[k]);
        n += _snprintf(line + n, 200 - n, "  |");
        for (k = 0; k < shown && n < 196; k++)
            line[n++] = p[k] >= 0x20 && p[k] <= 0x7E ? (char)p[k] : '.';
        line[n++] = '|';
        line[n++] = '\r';
        line[n++] = '\n';
        line[n] = 0;
        McuLogLine(line);
    }
    LeaveCriticalSection(&s_mcuCs);
    if (hvac && !InterlockedExchange((LONG*)&s_mcuPosted, 1) && !PostMessage(s_wnd, WM_APP_MCU, 0, 0))
        s_mcuPosted = 0;
    if (volume && !InterlockedExchange((LONG*)&s_volPosted, 1) && !PostMessage(s_wnd, WM_APP_MCU, WP_MCU_VOLUME, 0))
        s_volPosted = 0;
    if (mark)
        PostMessage(s_wnd, WM_APP_MCU, WP_MCU_MARK, 0);
    if (sound)
        PostMessage(s_wnd, WM_APP_MCU, WP_MCU_SOUND, 0);
    if (dayNight)
        PostMessage(s_wnd, WM_APP_MCU, WP_MCU_DAYNIGHT, 0);
    if (m.id == 0x4C01)   // a panel / wheel button: key, pressed 0 / released 1, short 1 / long 2 / repeat 3 (HwKey)
        PostMessage(s_wnd, WM_APP_MCU, WP_MCU_HWKEY, p[1] | p[2] << 8 | p[3] << 16);
    if ((m.id == 0x1102 && p[1] == 0x16) || m.id == 0x9101 || m.id == 0x8902 || m.id == 0x8905 || m.id == 0x3903 ||
        m.id == 0x4102) {
        s_menuMain = m.id == 0x1102;
        s_menuTick = m.tick | 1;
        s_sndPageNow = m.id == 0x4102 && p[1] == 0x10 ? 0xE6 : m.id == 0x4102 && p[1] == 0x0C ? 0xE4 : 0;
        if (m.id == 0x3903)
            s_settingsTick = m.tick | 1;
    }
    if (m.id == 0x1102 && p[1] == 0x18)   // the knob held 5 s: the MCU reboots the unit in 5 s more (2026-10-18)
        PostMessage(s_wnd, WM_APP_MCU, WP_MCU_REBOOT, 0);
    if (tuner && !InterlockedExchange((LONG*)&s_tunPosted, 1) && !PostMessage(s_wnd, WM_APP_MCU, WP_MCU_TUNER, 0))
        s_tunPosted = 0;
}

static void TunerGet(Tuner* t)
{
    EnterCriticalSection(&s_mcuCs);
    *t = s_tun;
    LeaveCriticalSection(&s_mcuCs);
    s_tunPosted = 0;
}

// The volume, mute and source (see s_vol); false = no volume message yet.
static bool VolGet(int* vol, int* mute, int* src)
{
    EnterCriticalSection(&s_mcuCs);
    *vol = s_vol;
    *mute = s_mute > 0;
    *src = s_src;
    LeaveCriticalSection(&s_mcuCs);
    s_volPosted = 0;
    return *vol >= 0;
}

// Polls the ring 10 times a second. At the first look it takes what is there (up to 256 back): the climate and
// the versions sent at boot are in it.
static DWORD WINAPI McuThread(LPVOID)
{
    VLog(L"поток MCU: id %08X", (unsigned)GetCurrentThreadId());
    LONG seen = -1;
    DWORD lastTry = 0;
    while (!s_mcuStop) {
        if (!s_shm && GetTickCount() - lastTry > 2000) {
            lastTry = GetTickCount();
            s_shmMap = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(McuShm), MCU_SHM_NAME);
            if (s_shmMap)
                s_shm = (McuShm*)MapViewOfFile(s_shmMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(McuShm));
        }
        if (s_shm && s_shm->magic == MCU_SHM_MAGIC) {
            LONG seq = s_shm->seq;
            if (seen < 0 || seq < seen)
                seen = seq > MCU_SHM_SLOTS ? seq - MCU_SHM_SLOTS : 0;
            if (seq - seen > MCU_SHM_SLOTS)
                seen = seq - MCU_SHM_SLOTS;
            while (seen < seq) {
                LONG n = seen + 1;
                McuMsg m = s_shm->msg[(n - 1) & (MCU_SHM_SLOTS - 1)];
                if (m.seq == n)
                    McuTake(m);
                seen = n;
            }
        }
        Sleep(100);
    }
    return 0;
}

static void McuStart()
{
    s_info.cs = -1;   // nothing seen yet (before any message: the car's sensors say "—", not 0)
    for (int k = 0; k < 16; k++)
        s_info.sys[k] = -1;
    InitializeCriticalSection(&s_mcuCs);
    s_mcuCsReady = true;
    s_mcuThread = CreateThread(NULL, 0, McuThread, NULL, 0, NULL);
}

static void McuStopThread()
{
    if (!s_mcuThread)
        return;
    s_mcuStop = true;
    WaitForSingleObject(s_mcuThread, 2000);
    EnterCriticalSection(&s_mcuCs);
    if (s_mcuLog != INVALID_HANDLE_VALUE) {
        CloseHandle(s_mcuLog);
        s_mcuLog = INVALID_HANDLE_VALUE;
    }
    LeaveCriticalSection(&s_mcuCs);
}

// 1 = the proxy is in and writes, 0 = the ring exists but no proxy ever wrote it, -1 = no ring.
static int McuState(LONG* attach, LONG* wraps)
{
    *attach = *wraps = 0;
    if (!s_shm)
        return -1;
    if (s_shm->magic != MCU_SHM_MAGIC)
        return 0;
    *attach = s_shm->attach;
    *wraps = s_shm->wraps;
    return 1;
}

static void McuLogToggle()
{
    EnterCriticalSection(&s_mcuCs);
    if (s_mcuLog != INVALID_HANDLE_VALUE) {
        CloseHandle(s_mcuLog);
        s_mcuLog = INVALID_HANDLE_VALUE;
        _snwprintf(s_status, 96, L"запись остановлена: mcu.log, %u КБ", (unsigned)(s_mcuLogBytes / 1024));
    } else {
        wchar_t path[MAX_PATH];
        if (!LogDir(path, s_dir)) {   // records go only to the SD card / USB disk
            wcscpy(s_status, L"нет SD-карты или USB-диска для записи");
            LeaveCriticalSection(&s_mcuCs);
            return;
        }
        wcscat(path, L"mcu.log");
        s_mcuLog = CreateFile(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        s_mcuLogBytes = 0;
        if (s_mcuLog != INVALID_HANDLE_VALUE) {
            const char* head = "# VestaHome mcu.log: <uptime s> <MCU id> s<skeleton> <len> : payload hex |text|\r\n";
            McuLogLine(head);
            // what is known so far: one line per id, so a short record still has every id in it
            for (int i = 0; i < s_mcuStatCount; i++) {
                char line[160];
                int n = _snprintf(line, 100, "# seen %04X x%u len %u :", (unsigned)s_mcuStat[i].id,
                                  (unsigned)s_mcuStat[i].count, (unsigned)s_mcuStat[i].len);
                for (int k = 0; k < 12 && k < s_mcuStat[i].len && n < 150; k++)
                    n += _snprintf(line + n, 160 - n, " %02X", s_mcuStat[i].data[k]);
                _snprintf(line + n, 160 - n, "\r\n");
                line[159] = 0;
                McuLogLine(line);
            }
        }
        wcscpy(s_status, s_mcuLog != INVALID_HANDLE_VALUE ? L"запись MCU в mcu.log" : L"mcu.log не создаётся");
    }
    LeaveCriticalSection(&s_mcuCs);
    VLog(L"mcu.log: %s", s_mcuLog != INVALID_HANDLE_VALUE ? L"запись" : L"стоп");
}

// One mark into every record that is on (can.log and mcu.log): what was just done in the car.
static int s_marks;

static void MarkLogs()
{
    bool can = s_canLog != INVALID_HANDLE_VALUE, mcu = s_mcuLog != INVALID_HANDLE_VALUE;
    if (!can && !mcu) {
        wcscpy(s_status, L"сначала включите запись");
        return;
    }
    char m[48];
    _snprintf(m, 48, "\r\n#MARK %d %u\r\n", ++s_marks, (unsigned)(GetTickCount() / 1000));
    m[47] = 0;
    if (can)
        CanLogWrite(m, (DWORD)strlen(m));
    if (mcu) {
        EnterCriticalSection(&s_mcuCs);
        McuLogLine(m + 2);
        LeaveCriticalSection(&s_mcuCs);
    }
    s_canMarks = s_marks;
    _snwprintf(s_status, 96, L"метка %d%s%s", s_marks, can ? L" · can.log" : L"", mcu ? L" · mcu.log" : L"");
}

static bool HvacGet(Hvac* h)
{
    EnterCriticalSection(&s_mcuCs);
    *h = s_hvac;
    LeaveCriticalSection(&s_mcuCs);
    s_mcuPosted = 0;
    return h->tick != 0;
}

// ---- MCU commands from this process, through SPIPowerMgr.dll as AppLauncher does it (it opens PIC1: for
// writing on the first send; reading stays with the shell). Arguments as G3NavHMI sends them (Ghidra, PROJECT.md):
// balance/fader = CAudioFieldDlg (x, y -8..8 -> x + 8, y + 8), equalizer = CAudioEffectDlg (band 0 high,
// 1 mid, 2 low; -6..6 -> + 6), product info / BT address = requests answered as MCU messages (to the monitor).
typedef void (*SpmFn0)(void);
typedef void (*SpmFn2)(unsigned int, unsigned int);
typedef void (*SpmFn6)(unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int);
static HMODULE s_spm;
static int     s_spmTried;

// The calls other than keys are made in the shell by our proxy (SPIPower/native/mcucmd.h, keep identical): from
// this process the audio ones never reached the MCU (no 0x3102.. replies, SPIPower.log 2026-10-17..20).
#define MCU_CMD_NAME   L"VestaMCUCmd"
#define MCU_CMD_EVENT  L"VestaMCUCmd_Ev"
#define MCU_CMD_MAGIC  0x44434D56
#define MCU_CMD_SLOTS  16
struct McuCall { char name[48]; unsigned int arg[6]; };
struct McuCmd {
    DWORD         magic;
    volatile LONG ready, seq, done;
    McuCall       call[MCU_CMD_SLOTS];
};

static bool McuViaShell(const char* name, int a, int b, int c, int d, int e, int f)
{
    static McuCmd* s_mc;
    static HANDLE  s_mcEv;
    if (!s_mc) {
        HANDLE m = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(McuCmd), MCU_CMD_NAME);
        if (m)
            s_mc = (McuCmd*)MapViewOfFile(m, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(McuCmd));
        s_mcEv = CreateEvent(NULL, FALSE, FALSE, MCU_CMD_EVENT);
    }
    if (!s_mc || s_mc->magic != MCU_CMD_MAGIC || !s_mc->ready || !s_mcEv)
        return false;   // an older proxy: from here, as before
    LONG n = s_mc->seq + 1;
    McuCall& k = s_mc->call[n % MCU_CMD_SLOTS];
    strncpy(k.name, name, sizeof(k.name) - 1);
    k.name[sizeof(k.name) - 1] = 0;
    unsigned int v[6] = { (unsigned)a & 0xFF, (unsigned)b & 0xFF, (unsigned)c & 0xFF, (unsigned)d & 0xFF,
                          (unsigned)e & 0xFF, (unsigned)f & 0xFF };
    memcpy(k.arg, v, sizeof(v));
    s_mc->seq = n;
    SetEvent(s_mcEv);
    VLog(L"mcu (через оболочку): %S(%d, %d, %d)", name, a, b, c);
    return true;
}

static bool McuIsKey(const char* name)
{
    return !strcmp(name, "SPApi_SendKeyEvent") || !strcmp(name, "SPApi_SendExtendKeyEvent");
}

static FARPROC SpmProc(const char* name)
{
#ifdef UNDER_CE
    if (!s_spmTried) {
        s_spmTried = 1;
        LoadLibrary(L"\\ResidentFlash\\navigation\\app\\NCL_BaseCom.dll");   // its dependency, by full path
        s_spm = LoadLibrary(L"\\ResidentFlash\\navigation\\app\\SPIPowerMgr.dll");
        VLog(L"mcu: SPIPowerMgr.dll %s", s_spm ? L"загружена" : L"не грузится");
    }
    if (!s_spm)
        return NULL;
    wchar_t w[64];
    MultiByteToWideChar(CP_ACP, 0, name, -1, w, 64);
    return GetProcAddress(s_spm, w);
#else
    (void)name;
    return NULL;
#endif
}

#ifndef UNDER_CE
static bool DemoMcuCall(const char* name, int a, int b, int c);   // (vh_demo.inc) the PC: the demo unit answers
#endif

static bool McuSend2(const char* name, int a, int b)
{
#ifndef UNDER_CE
    if (DemoMcuCall(name, a, b, 0))
        return true;
#endif
    if (!McuIsKey(name) && McuViaShell(name, a, b, 0, 0, 0, 0))
        return true;
    SpmFn2 f = (SpmFn2)SpmProc(name);
    if (!f) {
        wcscpy(s_status, L"MCU: только на ГУ (нет SPIPowerMgr.dll)");
        return false;
    }
    f((unsigned)a & 0xFF, (unsigned)b & 0xFF);
    VLog(L"mcu: %S(%d, %d)", name, a, b);
    return true;
}

static bool McuSend3(const char* name, int a, int b, int c)
{
#ifndef UNDER_CE
    if (DemoMcuCall(name, a, b, c))
        return true;
#endif
    if (!McuIsKey(name) && McuViaShell(name, a, b, c, 0, 0, 0))
        return true;
    SpmFn6 fn = (SpmFn6)SpmProc(name);   // three byte arguments in r0..r2; the rest is not read
    if (!fn)
        return false;
    fn(a & 0xFF, b & 0xFF, c & 0xFF, 0, 0, 0);
    VLog(L"mcu: %S(%d, %d, %d)", name, a, b, c);
    return true;
}

static bool McuSend6(const char* name, int a, int b, int c, int d, int e, int f)
{
#ifndef UNDER_CE
    if (DemoMcuCall(name, a, b, c))
        return true;
#endif
    if (McuViaShell(name, a, b, c, d, e, f))
        return true;
    SpmFn6 fn = (SpmFn6)SpmProc(name);
    if (!fn)
        return false;   // the PC, or no SPIPowerMgr: only the local clock
    fn(a & 0xFF, b & 0xFF, c & 0xFF, d & 0xFF, e & 0xFF, f & 0xFF);
    VLog(L"mcu: %S(%d, %d, %d, %d, %d, %d)", name, a, b, c, d, e, f);
    return true;
}

static bool McuSend0(const char* name)
{
#ifndef UNDER_CE
    if (DemoMcuCall(name, 0, 0, 0))
        return true;
#endif
    SpmFn0 f = (SpmFn0)SpmProc(name);
    if (!f) {
        wcscpy(s_status, L"MCU: только на ГУ (нет SPIPowerMgr.dll)");
        return false;
    }
    f();
    VLog(L"mcu: %S()", name);
    return true;
}

static void Go(Screen s, bool push);
static void Back();
