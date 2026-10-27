// vh_screens.inc - part of home.cpp (included there, in order): the shell's Bluetooth (proxy), the radio, media, phone and settings screens, OBD, Wi-Fi screens.

// ---- the shell's Bluetooth through our proxy of FLC_BLUETOOTH_SDK.dll (SPIPower/bt, btshm.h): its state as the
// stack reports it, and commands the proxy carries out in the shell's process. Without the proxy the mapping is
// ours and empty (magic 0): the screens say so.
static BtShm* s_bt;

static BtShm* BtShmGet()
{
    if (!s_bt) {
        HANDLE m = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(BtShm), BT_SHM_NAME);
        if (m)
            s_bt = (BtShm*)MapViewOfFile(m, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(BtShm));
    }
    return s_bt;
}

// The state; false = no proxy (or its original SDK did not load). seq: the state's version, for "changed?".
static bool BtGet(BtState* st, LONG* seq)
{
    BtShm* b = BtShmGet();
    memset(st, 0, sizeof(*st));
    st->av = st->avrcp = st->hf = st->hfAudio = st->battery = st->signal = -1;
    if (!b || b->magic != BT_SHM_MAGIC || !b->origOk)
        return false;
    for (int tries = 0; tries < 10; tries++) {
        LONG a = b->seq;
        if (a & 1) {
            Sleep(1);
            continue;
        }
        *st = b->st;
        if (b->seq == a) {
            if (seq)
                *seq = a + b->evSeq;
            return true;
        }
    }
    return true;   // a copy taken while it changed: good enough for a screen
}

// One command at a time: the one sent just before, not yet carried out, must not be overwritten (its text neither):
// up to 0.3 s, and not when it is older - a proxy whose thread does not run would hold every command up.
static DWORD s_btSent;

static void BtCmdWait(BtShm* b)
{
    for (int i = 0; i < 30 && b->cmdDone != b->cmdSeq && GetTickCount() - s_btSent < 1000; i++)
        Sleep(10);
}

static void BtCmd(int cmd)
{
    BtShm* b = BtShmGet();
    if (!b || b->magic != BT_SHM_MAGIC)
        return;
    BtCmdWait(b);
    s_btSent = GetTickCount();
    b->cmd = cmd;
    InterlockedIncrement((LONG*)&b->cmdSeq);
    static HANDLE s_ev;
    if (!s_ev)
        s_ev = CreateEvent(NULL, FALSE, FALSE, BT_CMD_EVENT);
    if (s_ev)
        SetEvent(s_ev);
}

static void BtText(const char* u8, wchar_t* w, int cap)   // the stack's texts are UTF-8
{
    if (!MultiByteToWideChar(CP_UTF8, 0, u8, -1, w, cap))
        MultiByteToWideChar(CP_ACP, 0, u8, -1, w, cap);
    w[cap - 1] = 0;
}

// ---- screens

enum { RSET_NOISE, RSET_MW, RSET_RDS, RSET_AF, RSET_CT, RSET_TA, RSET_MS, RSET_COUNT };

// The tuner as the MCU reports it (mcu.log 2026-10-16/17, PROJECT.md): 0x0901 00 00 <freq BE, 10 kHz>; 0x0908 00 01
// <from> <to> <count> + 8 frequencies (presets in blocks of 8: FM1..FM3 = 24, the low band "УКВ" from 65 MHz = 8);
// 0x0907 00 01 <the preset playing, FF none>; 0x090B 00 00 00 + RDS PS (8 chars); 0x0931 00 00 00 <part> + RadioText.
// More (SPIPower.log 2026-10-18-2): 0x0901 00 <band: 0 FM, 1 УКВ> <freq>; the station list after the list key 0x3C -
// 0x0908 00 00 <from> <to> <count> + frequencies (up to 30) and 0x0930 00 <from> <to> <count> + 4 names x 8 chars;
// 0x0904 00 <flag> <0/1> (all of them after 0x30; flag 01 is set when a search starts).
enum { kTunList = 30 };
struct Tuner {
    int     f10;              // MHz * 100, 0 = not seen yet
    int     band;             // 0 FM, 1 УКВ, -1 not seen
    DWORD   fTick;            // when the frequency last changed (a search runs while it keeps changing)
    int     list[kTunList], listCount;   // the station list (after 0x3C), MHz * 100
    wchar_t listName[kTunList][9];
    int     flags[8];         // 0x0904
    int     preset[24];       // the same units, 0 = empty
    int     presets;          // how many the band has (24 FM, 8 УКВ), 0 = not seen
    int     cur;              // the preset playing, -1 none
    wchar_t ps[9], rt[2][33];
    int     set[7];           // the radio's settings (RSET_*): 0 / 1, as the MCU reports them
    int     setKnown;         // which of them came (bit 1 << RSET_*)
};
static void TunerGet(Tuner* t);
static const wchar_t* RadioName(const Tuner& t, int f10);   // a station's name, "" when not known
static void McuKey(int code, bool ext);
static void RadioPreset(int i);
static bool McuSend3(const char* name, int a, int b, int c);
static bool VolGet(int* vol, int* mute, int* src);

static void FreqText(int f10, wchar_t* s, int n)   // 9840 -> "98.4", 10625 -> "106.25"; СВ: kHz, 1053 -> "1053"
{
    if (f10 <= 0)
        wcscpy(s, L"—");
    else if (f10 < 3000)   // the MW band's kHz (522..1710)
        _snwprintf(s, n, L"%d", f10);
    else if (f10 % 10)
        _snwprintf(s, n, L"%d.%02d", f10 / 100, f10 % 100);
    else
        _snwprintf(s, n, L"%d.%d", f10 / 100, f10 % 100 / 10);
}

static void RadioDock(HDC dc, bool listOn)
{
    Dock(dc);
    IconBtn(dc, IC_PREV, 268, 417, 44, kText, A_RKEY, 0x13B);
    IconBtn(dc, IC_FWD, 488, 417, 44, kText, A_RKEY, 0x13C);
    if (listOn) {   // the list is open: its button is selected and closes it
        bool d = Zone(590, 410, 64, 58, A_BACK);
        R(dc, 590, 410, 64, 58, 10, d ? kOrange : kPanel2);
        Ico(dc, IC_LIST, 600, 417, 44, d ? kInk : kOrange);
    } else {
        IconBtn(dc, IC_LIST, 600, 417, 44, kText, A_SCREEN, S_RADIO_LIST);
    }
    IconBtn(dc, IC_GEAR, 712, 417, 44, kText, A_SCREEN, S_SET_RADIO);
}

// Radio, a main screen, on the MCU's tuner (s_tun): the frequency, RDS name and text, the presets. Four pages of
// 8 presets as the shell's band button steps them: FM1..FM3 (FM's 24 presets, local pages: the MCU sends all 24 at
// once and does not care) and УКВ (its band: switched in the MCU, RadioBand). One button named after the page
// steps them round (as the shell's), a swipe steps to the next / previous, dots under the presets show where.
// A preset tap tunes it (SPApi_SendListItemControl(1, n)), a long press saves the station there ((1, n, 1), the
// shell's own, MCU 0x0909); seek - the extended keys. Opening the screen with nothing known asks the MCU (0x30,
// as the shell's screen does).
static void OursQuiet();
static void BtQuiet();
static bool McuHostOn();   // (vh_photo.inc) our MCU host runs: the stock shell does not
static void MediaSource();

// The tuner's state comes only on a change or when asked (key 0x30). Right after the unit starts the MCU may not
// answer for 20-30 s (HU 2026-10-20: the radio key at 187 s - nothing, the same at 207 s - the tuner at once).
// The shell gets it at once at its start: SendRadioNaviVoiceGuidance(0) + SendRadioIsNaviSourceCouldIn(0), and
// the MCU sends the whole tuner (SPIPower.log 2026-10-20-2, 36.3 s) - so the same first; then, while the radio
// is wanted and the tuner has not spoken in this run, 0x30 again every 3 s, for a minute.
static volatile bool s_tunLive;   // a 0x0901 came since the start
static volatile LONG s_tun0901;   // 0x0901 replies so far: one a band key press (RadioBandStep)
static DWORD s_radioAsk;          // the radio wanted (RadioOn, its screen), 0 = not

// The source switches and how they are checked (HU 2026-10-20-3): the radio's keys 0x30 + 0x7F switched to the
// radio only sometimes - a preset (SendListItemControl) always does; SendMediaSourceSelect(5, 3) is acked (0x9101
// 00 05 06) but the MCU goes "media" only when the shell's own player plays - while the Bluetooth source (09, keys
// 0x84 + 0x54) is the CE's own audio path too: our waveOut sounds with it (BT -> our music worked). So ours take
// that path; the radio's keys are checked a moment later and a preset taken if they did nothing.
static DWORD s_srcByUs;      // we switched the source: the watcher (MODE, the shell) leaves our players alone
static DWORD s_radioSrcAsk;  // RadioOn's keys sent (checked 1.5 s later)
static DWORD s_btPathTick;   // the Bluetooth path taken for our player: the phone paused once it is up
static DWORD s_mediaAsk;     // MediaSource's SendMediaSourceSelect sent (checked 2 s later: else Bluetooth's path)

// СВ (MW, 2026-10-20): the shell's AM key 0x3A, band 2 in 0x0901 with the frequency in kHz (522, 531, ... a 9 kHz
// step), 8 presets; 0x3B from it goes back to FM1. The band takes the "СВ" radio setting (0x092E) to be on.
enum { kRadioPages = 5, RP_UKV = 3, RP_MW = 4 };

static int PageBand(int page)   // the MCU's band of a page: 0 FM, 1 УКВ, 2 СВ
{
    return page == RP_UKV ? 1 : page == RP_MW ? 2 : 0;
}

static int BandPage(int band)   // the first page of a band
{
    return band == 1 ? RP_UKV : band == 2 ? RP_MW : 0;
}
static int s_radioPage;          // 0..2 FM1..FM3, 3 УКВ
static int s_bandWant = -1;      // RadioBand: the band asked for, -1 none
static int s_bandPresses;
static DWORD s_bandTick;
static LONG s_bandSeq;           // s_tun0901 at the last press
static bool s_bandFailed;        // given up with the band not the page's: the page goes back to the band's
static void RadioBand(int want);

// A band switch under way (the MCU not answering for 8 s: given up).
static bool BandPending()
{
    if (s_bandWant >= 0 && GetTickCount() - s_bandTick > 8000) {
        s_bandWant = -1;
        s_bandFailed = true;
    }
    return s_bandWant >= 0;
}

// The radio's page to, always (the page is the user's): FM's pages locally; the MCU's band follows the page -
// switched when it differs, the target changed (or the switch dropped) when one is still under way.
static void RadioPageTo(int page)
{
    Tuner t;
    TunerGet(&t);
    page = (page + kRadioPages) % kRadioPages;
    int want = PageBand(page);
    if (BandPending()) {
        if (want != s_bandWant) {
            if (t.band == want)
                s_bandWant = -1;   // left before the band changed: nothing to switch back
            else
                RadioBand(want);
        }
    } else if (t.band >= 0 && t.band != want) {
        RadioBand(want);
    }
    s_radioPage = page;
}

static void ScrRadio(HDC dc)
{
    Tuner t;
    TunerGet(&t);
    static DWORD s_seen;
    if (s_seen != s_screenTick) {   // opened: the page of the preset playing; nothing known - ask
        s_seen = s_screenTick;
        if (t.band == 1 || t.band == 2)
            s_radioPage = BandPage(t.band);
        else if (t.cur >= 0 && t.cur < 24)
            s_radioPage = t.cur / 8;
        if (!t.f10 || !t.presets || !s_tunLive) {
            McuKey(0x30, false);
            if (!s_tunLive)
                s_radioAsk = GetTickCount();
        }
    }
    wchar_t f[16], band[8];
    FreqText(t.f10, f, 16);
    bool low = t.band == 1 || (t.band < 0 && t.f10 >= 6500 && t.f10 < 8750);   // the OIRT band, 65..74 MHz
    bool mw = t.band == 2 || (t.band < 0 && t.f10 > 0 && t.f10 < 3000);
    bool pending = BandPending();
    if (s_bandFailed && !pending) {   // the MCU did not switch: the page of the band it is in
        s_bandFailed = false;
        if (t.band >= 0 && t.band != PageBand(s_radioPage)) {
            s_radioPage = BandPage(t.band);
            wcscpy(s_status, L"диапазон не переключился - ещё раз");
        }
    }
    static int s_lastBand = -2;
    if (t.band != s_lastBand) {   // the band changed elsewhere (the shell): its page
        if (!pending && s_lastBand != -2 && t.band >= 0 && t.band != PageBand(s_radioPage))
            s_radioPage = BandPage(t.band);
        s_lastBand = t.band;
    }
    int pb = PageBand(s_radioPage);
    bool ready = pb == (mw ? 2 : low ? 1 : 0);   // the MCU is in the page's band (not while it switches)
    int page = pb ? 0 : s_radioPage;              // the presets' page in the band
    if (pb)
        wcscpy(band, pb == 1 ? L"УКВ" : L"СВ");
    else
        _snwprintf(band, 8, L"FM%d", page + 1);
    const wchar_t* name = t.ps[0] ? t.ps : RadioName(t, t.f10);
    // design 0.6: the frequency the largest, "МГц · FM1" after it (the page's name; a tap - the next page, a swipe
    // too), PTY and the stations at the right; the name and RDS text; the scale with the seek arrows at its ends
    // (an arrow or the scale's end: seek down / up); the page's presets; the page dots
    Frame(dc, L"Радио");
    int end = T(dc, 24, 128, f, 76, t.f10 ? kText : kDim, 700, AL_START, 300);
    if (t.f10) {
        wchar_t pg[12];
        _snwprintf(pg, 12, L"%s%s", band, pending ? L"…" : L"");
        int ux = T(dc, end + 14, 126, mw ? L"кГц · " : L"МГц · ", 20, kMuted, 400);
        int ue = T(dc, ux, 126, pg, 20, kOrange, 700);
        Zone(end + 6, 64, ue - end + 8, 72, A_RPAGE);   // the next page
    } else {
        T(dc, end + 14, 126, L"нет данных — включаю радио…", 18, kDim, 700, AL_START, 470 - end);
    }
    Button(dc, 556, 76, 76, 44, L"PTY", B_N, IC_NONE, 16, 0, A_SCREEN, S_RADIO_PTY);   // a programme type
    Button(dc, 640, 76, 144, 44, L"Станции", B_N, IC_LIST, 16, 20, A_SCREEN, S_RADIO_LIST);
    Marquee(dc, 24, 166, 752, name, 26, kText, 700);
    wchar_t rt[68];
    _snwprintf(rt, 68, L"%s%s", t.rt[0], t.rt[1]);
    rt[67] = 0;
    Marquee(dc, 24, 190, 752, rt, 15, kMuted, 400);
    if (s_marqOn)
        SetTimer(s_wnd, TIMER_MARQ, kMarqMs, NULL);   // (again: the same timer, restarted)
    bool sd = Zone(8, 200, 112, 48, A_RKEY, 0x13B), su = Zone(680, 200, 112, 48, A_RKEY, 0x13C);   // seek down / up
    if (sd)
        Disc(dc, 40, 224, 22, kOrange);
    if (su)
        Disc(dc, 760, 224, 22, kOrange);
    Ico(dc, IC_PREV, 24, 208, 32, sd ? kInk : kText);
    Ico(dc, IC_FWD, 744, 208, 32, su ? kInk : kText);
    for (int k = 0; k < 3; k++) {   // the scale's ends lead into the arrows
        Fill(dc, 70 + k * 9, 236 - (k % 2 ? 8 : 12), 72 + k * 9, 236, kMuted);
        Fill(dc, 728 - k * 9, 236 - (k % 2 ? 8 : 12), 730 - k * 9, 236, kMuted);
    }
    int lo = mw ? 522 : low ? 6500 : 8750, hi = mw ? 1620 : low ? 7400 : 10800;
    for (int i = 0; i <= 48; i++) {   // the tuning scale over x 112..688
        int x = 112 + i * 12, h = i % 5 ? 8 : 16;
        Fill(dc, x - 1, 236 - h, x + 1, 236, kDim);
    }
    if (t.f10 >= lo && t.f10 <= hi) {
        int mark = 112 + (t.f10 - lo) * 576 / (hi - lo);
        Fill(dc, mark - 1, 216, mark + 2, 236, kOrange);
    }
    for (int i = 0; i < 8; i++) {   // the page's presets, 4 x 2: the frequency and the name, no borders
        int x = 16 + (i % 4) * 194, y = 256 + (i / 4) * 72, n = page * 8 + i;
        int pf = n < 24 && ready ? t.preset[n] : 0;
        bool have = t.presets > 0 && pf > 0;
        bool down = Zone(x, y, 184, 64, t.presets > 0 ? A_PRESET : A_NONE, n);
        bool cur = !down && have && n == t.cur;   // the one playing: soft orange
        wchar_t pfs[16];
        FreqText(pf, pfs, 16);
        RSel(dc, x, y, 184, 64, 16, cur, down);
        COLORREF fg = down ? kInk : cur ? kOrange : have ? kText : kDim;
        T(dc, x + 16, y + 29, have ? pfs : L"—", 21, fg, 700);
        const wchar_t* nm = have ? RadioName(t, pf) : L"";
        T(dc, x + 16, y + 51, nm[0] ? nm : have ? L"" : L"удерживать — сохранить", 13, down ? kInk : cur ? kOrange
          : kMuted, 400, AL_START, 156);
    }
    PageDots(dc, s_radioPage, kRadioPages);
}

// The band: the MCU's band key 0x3B steps FM1 -> FM2 -> FM3 -> УКВ (СВ -> FM1), and only the band shows in its
// replies (0x0901's band): pressed again on each reply until the band is the one wanted, at most 4 times; СВ is
// its own key 0x3A.
static void RadioBand(int want)
{
    s_bandWant = want;
    s_bandPresses = 1;
    s_bandTick = GetTickCount();
    s_bandSeq = s_tun0901;
    s_bandFailed = false;
    Tuner t;
    TunerGet(&t);
    if (want == 2 && (t.setKnown >> RSET_MW & 1) && !t.set[RSET_MW])
        McuKey(0xA9, true);   // СВ off in the MCU's settings: on first (kRsetKey[RSET_MW][1]; no switch of ours)
    McuKey(want == 2 ? 0x3A : 0x3B, false);
}

static void RadioBandStep(const Tuner& t)   // a tuner reply came
{
    if (s_bandWant < 0)
        return;
    if (s_tun0901 == s_bandSeq)
        return;   // the press's 0x0901 not here yet (the other replies of a press do not count)
    s_bandSeq = s_tun0901;
    if (t.band == s_bandWant) {
        s_bandWant = -1;
        return;
    }
    if (s_bandPresses >= 4 || GetTickCount() - s_bandTick > 8000) {
        s_bandWant = -1;
        s_bandFailed = true;
        return;
    }
    s_bandPresses++;
    s_bandTick = GetTickCount();
    McuKey(s_bandWant == 2 ? 0x3A : 0x3B, false);
}

static void RadioPresetSave(int i)   // a long press: the station playing goes to preset i (the shell's (1, n, 1))
{
    if (McuSend3("SPApi_SendListItemControl", 1, i, 1))
        _snwprintf(s_status, 96, L"сохранено в пресет %d", i % 8 + 1);
}

// A preset of the radio screen or the widget: as CRadioMainDlg::SlotListItemSelect (Ghidra 2026-10-18).
static void RadioPreset(int i)
{
    McuSend3("SPApi_SendListItemControl", 1, i, 0);
}

// A key to the MCU as the stock shell sends it: pressed, released; ext = SPApi_SendExtendKeyEvent.
static void McuKey(int code, bool ext)
{
    const char* fn = ext ? "SPApi_SendExtendKeyEvent" : "SPApi_SendKeyEvent";
    if (McuSend3(fn, code, 0, 0))
        McuSend3(fn, code, 1, 1);
}


// The radio as the MCU's source: the stock shell's main menu "Radio" is key 0x7F. The shell sends 0x30 before
// every source change (SPIPower.log 2026-10-18); without it our 0x7F was ignored at first - so the same while the
// shell runs. Under our MCU host the main menu's 0x7F alone (G3NavHMI FUN_005f7cdc, Ghidra 2026-10-27), and the
// radio stayed silent after media until a preset was pressed (HU 2026-10-27-2) - the check below picks it.
// One source at a time: our players and the phone's music stop.
static void RadioOn()
{
    ResumeByUser();
    OursQuiet();
    BtQuiet();
    if (!s_tunLive)
        s_radioAsk = GetTickCount();
    int vol, mute, src;
    VolGet(&vol, &mute, &src);
    if (src == 0)
        return;   // already the radio
    if (!McuHostOn())
        McuKey(0x30, false);
    McuKey(0x7F, false);
    s_srcByUs = s_radioSrcAsk = GetTickCount();
}

// The radio screen's keys as the shell's (SPIPower.log 2026-10-18): seek down / up = extended 0x3B / 0x3C,
// the band FM1 -> FM2 -> FM3 -> УКВ = 0x3B, the search (scan; again = stop) = 0x3F. arg: code | 0x100 = extended.
static void RadioKey(int arg)
{
    McuKey(arg & 0xFF, (arg & 0x100) != 0);
}

// Radio -> list: the MCU's station list. Opening it is the shell's list key 0x3C - the MCU then sends the stations
// with their names, and only in this mode does the search key 0x3F work (HU 2026-10-18-2). A tap tunes the row
// (SPApi_SendListItemControl(0, row)); "Автопоиск" sweeps the band, then the list is asked for again.
static int   s_rlistPage;
static DWORD s_scanStart;   // the search started (0 = none)
static int   s_fillI = -1;  // "В пресеты": the station being written (-1 = none), its step, when it began
static int   s_fillStep, s_fillN, s_fillOk;
static DWORD s_fillTick, s_fillArm;   // (s_fillArm: the first tap, the second within 4 s writes)

static void ScrRadioList(HDC dc)
{
    static DWORD s_seen;
    if (s_seen != s_screenTick) {
        s_seen = s_screenTick;
        McuKey(0x3C, false);
    }
    Tuner t;
    TunerGet(&t);
    Frame(dc, L"Радио · Список");
    Panel(dc, 16, 68, 600, 322);
    int n = t.listCount < kTunList ? t.listCount : kTunList, pages = (n + 4) / 5;
    if (s_rlistPage >= pages)
        s_rlistPage = pages ? pages - 1 : 0;
    bool scanning = s_scanStart != 0;
    if (scanning) {   // the search: the frequency running, dots moving
        wchar_t f[16], s[40];
        FreqText(t.f10, f, 16);
        static const wchar_t* dots[4] = { L"", L".", L"..", L"..." };
        _snwprintf(s, 40, L"Ищу станции%s", dots[GetTickCount() / 400 % 4]);
        T(dc, 40, 180, s, 26, kText, 700);
        T(dc, 40, 250, f, 60, kOrange, 700);
        int x = 40 + (t.f10 > 8750 && t.f10 <= 10800 ? (t.f10 - 8750) * 540 / 2050 : 0);
        R(dc, 40, 290, 540, 6, 3, kLine);
        R(dc, x, 286, 14, 14, 7, kOrange);
    } else if (!n) {
        T(dc, 40, 180, L"Список пуст или ещё не пришёл.", 20, kMuted, 600, AL_START, 560);
        T(dc, 40, 214, L"«АВТОПОИСК» найдёт станции заново.", 17, kDim, 400, AL_START, 560);
    }
    for (int r = 0; r < 5 && !scanning; r++) {
        int i = s_rlistPage * 5 + r;
        if (i >= n)
            break;
        int y = 76 + r * 62;
        bool cur = t.list[i] == t.f10;
        bool d = Zone(24, y, 568, 56, A_RLIST, i);
        R(dc, 24, y, 568, 56, 10, d || cur ? kOrange : kPanel);
        wchar_t f[16];
        FreqText(t.list[i], f, 16);
        T(dc, 40, y + 38, f, 26, d || cur ? kInk : kText, 700);
        const wchar_t* nm = t.listName[i][0] ? t.listName[i] : RadioName(t, t.list[i]);
        T(dc, 180, y + 36, nm, 19, d || cur ? kInk : kMuted, 700, AL_START, 400);
    }
    if (pages > 1 && !scanning) {
        Button(dc, 632, 232, 72, 48, NULL, s_rlistPage ? B_N : B_DIS, IC_BACK, 18, 28, A_RLISTPAGE, -1);
        Button(dc, 712, 232, 72, 48, NULL, s_rlistPage + 1 < pages ? B_N : B_DIS, IC_NEXT, 18, 28, A_RLISTPAGE, 1);
    }
    Button(dc, 632, 68, 152, 72, scanning ? L"Стоп" : L"Автопоиск", scanning ? B_SEL : B_N, IC_SEARCH, 15, 0, A_RSCAN);
    wchar_t fl[24];   // the stations into the presets: the second tap writes, a tap while writing stops
    bool armed = s_fillArm && GetTickCount() - s_fillArm < 4000;
    if (s_fillI >= 0)
        _snwprintf(fl, 24, L"Пишу %d из %d", s_fillI + 1, s_fillN);
    else
        wcscpy(fl, armed ? L"Заменить пресеты?" : L"В пресеты");
    bool can = n > 0 && !scanning;
    Button(dc, 632, 148, 152, 72, fl, s_fillI >= 0 || armed ? B_SEL : can ? B_N : B_DIS, IC_NONE, 13, 0,
           can || s_fillI >= 0 ? A_RFILL : A_NONE);
    R(dc, 632, 292, 152, 98, 12, kPanel);
    wchar_t c[8];
    _snwprintf(c, 8, L"%d", n);
    T(dc, 708, 322, L"Найдено", 13, kMuted, 600, AL_MID);
    T(dc, 708, 362, c, 36, kText, 700, AL_MID);
    RadioDock(dc, true);
}

static void RadioScan()   // the list's "Автопоиск" (again = stop, the same key)
{
    s_scanStart = s_scanStart ? 0 : GetTickCount();
    McuKey(0x3F, false);
}

// Every second on the list: the search is over once the frequency stood still 2 s (and 3 s passed) - the list again.
static void RadioScanTick()
{
    if (!s_scanStart)
        return;
    Tuner t;
    TunerGet(&t);
    DWORD now = GetTickCount();
    if (now - s_scanStart > 3000 && now - t.fTick > 2000) {
        s_scanStart = 0;
        s_rlistPage = 0;
        McuKey(0x3C, false);
    }
}

// "В пресеты" (the user 2026-10-24): the list's stations one by one into the presets of the same numbers (FM: up to
// 24 = FM1..FM3; УКВ: 8) - each tuned from the list (0, row, 0), then saved where it plays (1, n, 1), as a long press
// does. A step waits for the MCU: the frequency, then the preset's new value (or a timeout - noted in the log).
enum { TIMER_RFILL = 22 };
static void Redraw();

static void RadioFill()
{
    if (s_fillI >= 0) {   // a tap while writing: stop
        KillTimer(s_wnd, TIMER_RFILL);
        _snwprintf(s_status, 96, L"в пресеты: остановлено, записано %d", s_fillOk);
        s_fillI = -1;
        return;
    }
    if (!s_fillArm || GetTickCount() - s_fillArm >= 4000) {   // the first tap: asks
        s_fillArm = GetTickCount();
        return;
    }
    s_fillArm = 0;
    Tuner t;
    TunerGet(&t);
    int max = t.presets ? t.presets : t.band == 1 ? 8 : 24;
    s_fillN = t.listCount < max ? t.listCount : max;
    if (s_fillN > kTunList)
        s_fillN = kTunList;
    if (s_fillN <= 0)
        return;
    s_fillI = 0;
    s_fillStep = 0;
    s_fillOk = 0;
    VLog(L"радио: в пресеты %d станций", s_fillN);
    SetTimer(s_wnd, TIMER_RFILL, 250, NULL);
}

static void RadioFillTick()
{
    if (s_fillI < 0) {
        KillTimer(s_wnd, TIMER_RFILL);
        return;
    }
    Tuner t;
    TunerGet(&t);
    DWORD now = GetTickCount(), age = now - s_fillTick;
    int f = t.list[s_fillI];
    if (s_fillStep == 0) {   // tune it from the list
        McuSend3("SPApi_SendListItemControl", 0, s_fillI, 0);
        s_fillStep = 1;
        s_fillTick = now;
    } else if (s_fillStep == 1) {   // playing (and settled a moment): save
        if ((t.f10 == f && age >= 700) || age >= 3000) {
            if (t.f10 != f)
                VLog(L"радио: в пресеты %d - частота %d вместо %d", s_fillI + 1, t.f10, f);
            McuSend3("SPApi_SendListItemControl", 1, s_fillI, 1);
            s_fillStep = 2;
            s_fillTick = now;
        }
    } else if (t.preset[s_fillI] == f || age >= 2500) {   // the preset changed (or not): the next
        if (t.preset[s_fillI] == f)
            s_fillOk++;
        else
            VLog(L"радио: в пресеты %d - пресет %d, ждали %d", s_fillI + 1, t.preset[s_fillI], f);
        if (++s_fillI >= s_fillN) {
            KillTimer(s_wnd, TIMER_RFILL);
            VLog(L"радио: в пресеты записано %d из %d", s_fillOk, s_fillN);
            _snwprintf(s_status, 96, L"в пресеты записано %d из %d", s_fillOk, s_fillN);
            s_fillI = -1;
        } else {
            s_fillStep = 0;
        }
    }
    if (s_screen == S_RADIO_LIST && IsWindowVisible(s_wnd))
        Redraw();
}

// Radio -> PTY: the RDS programme type (its codes 0..15 in order; 0 = all) chosen, then "search" as the stock shell
// does it - its ext key 0xA4 (the PTY list), then SPApi_SendCurrentAdjustPty(type) (CRadioListDlg, Ghidra
// 2026-10-22; to verify on the unit).
static const wchar_t* const kPtyTypes[16] = { L"Все", L"Новости", L"События", L"Информация", L"Спорт", L"Образование",
                                              L"Драма", L"Культура", L"Наука", L"Разное", L"Поп", L"Рок", L"Лёгкая",
                                              L"Лёгкая классика", L"Классика", L"Другая" };
static int s_ptySel = 11;

static void PtySearch()
{
    McuKey(0xA4, true);
    McuSend3("SPApi_SendCurrentAdjustPty", s_ptySel, 0, 0);
    _snwprintf(s_status, 96, L"поиск станций: %s", kPtyTypes[s_ptySel]);
    VLog(L"радио: PTY %d (%s)", s_ptySel, kPtyTypes[s_ptySel]);
}

static void ScrRadioPty(HDC dc)
{
    Frame(dc, L"Радио · Тип программы");
    for (int i = 0; i < 16; i++)
        Button(dc, 16 + (i % 4) * 196, 68 + (i / 4) * 80, 186, 70, kPtyTypes[i], i == s_ptySel ? B_SEL : B_FILL,
               IC_NONE, 16, 0, A_PTYSEL, i);
    Dock(dc);
    wchar_t s[64];
    _snwprintf(s, 64, L"Ищет станции с типом «%s»", kPtyTypes[s_ptySel]);
    T(dc, 400, 446, s, 17, kMuted, 400, AL_MID, 390);
    Button(dc, 604, 410, 180, 58, L"Искать", B_PRI, IC_SEARCH, 18, 0, A_PTYGO);
}

// Media -> music and its list: the player (BASS), defined with it below.
static void ScrAudio(HDC dc);
static void ScrAudioList(HDC dc);

// Bluetooth music: the stock shell's Bluetooth stack plays it (FLC SDK in G3NavHMI). The MCU's source (0x4904 =
// 09) and switching to it with the shell's key 0x54 (after 0x30, SPIPower.log 2026-10-18); with our proxy of the
// SDK (btshm.h) also the phone, the track (AVRCP) and the buttons (the SDK's own Av_* calls, in the shell).
enum { TIMER_BTSRC = 12 };
static int s_btHold;   // ⏮ / ⏭ held: the release command to send when the finger goes up

static void BtMusicOn()   // the shell's "Phone" key 0x84 (the MCU answers 0x8905), then its BT music key 0x54
{
    ResumeByUser();                         // (HU 2026-10-18-2: 0x54 alone did nothing; after 0x84 the source turned 09)
    OursQuiet();
    s_srcByUs = GetTickCount();
    McuKey(0x84, false);
    SetTimer(s_wnd, TIMER_BTSRC, 800, NULL);
}

static void ScrAudioBt(HDC dc)
{
    Frame(dc, L"Медиа · Bluetooth");
    MediaTabs(dc, S_AUDIO_BT, IC_PHONE, A_SCREEN, S_PHONE);
    Cover(dc, IC_BT, 16, 124, 232);
    int vol, mute, src;
    VolGet(&vol, &mute, &src);
    bool on = src == 9;
    BtState bt;
    bool proxy = BtGet(&bt, NULL);
    static DWORD s_asked;   // the track and the play state, asked for on opening (the shell does the same)
    if (proxy && bt.av >= 1 && (!s_asked || GetTickCount() - s_asked > 10000)) {
        s_asked = GetTickCount();
        BtCmd(BT_CMD_MEDIAINFO);
    }
    static DWORD s_seen;   // opened with the phone connected: its music becomes the source at once
    if (s_seen != s_screenTick) {
        s_seen = s_screenTick;
        if (proxy && bt.av >= 1 && !on)
            BtMusicOn();
    }
    wchar_t cap[160], title[BT_TEXT], artist[BT_TEXT], album[BT_TEXT], line[300];
    if (proxy && bt.devName[0]) {
        BtText(bt.devName, title, BT_TEXT);
        _snwprintf(cap, 160, L"Bluetooth · %s", title);
    } else {
        wcscpy(cap, L"Bluetooth-музыка");
    }
    T(dc, 272, 150, cap, 14, kMuted, 600, AL_START, 512);
    if (proxy && bt.title[0]) {   // the track, as the phone tells it (AVRCP)
        BtText(bt.title, title, BT_TEXT);
        BtText(bt.artist, artist, BT_TEXT);
        BtText(bt.album, album, BT_TEXT);
        T(dc, 272, 192, title, 30, kText, 700, AL_START, 512);
        if (artist[0] && album[0])
            _snwprintf(line, 300, L"%s · %s", artist, album);
        else
            wcscpy(line, artist[0] ? artist : album);
        T(dc, 272, 222, line, 18, kMuted, 400, AL_START, 512);
        int len = bt.lengthMs > 0 ? bt.lengthMs : bt.durationMs;
        if (len > 0) {   // the bar: the position the stack reports every 2 s while playing, moved on in between
            int pos = bt.posMs;
            if (bt.playing == 1 && bt.playTick)
                pos += (int)(GetTickCount() - bt.playTick);
            pos = pos < 0 ? 0 : pos > len ? len : pos;
            int px = 512 * (pos / 1000) / (len / 1000 > 0 ? len / 1000 : 1);
            R(dc, 272, 252, 512, 8, 4, kLine);
            R(dc, 272, 252, px, 8, 4, kOrange);
            Disc(dc, 272 + px, 256, 10, kText);   // the knob; ⏮ / ⏭ held - rewind / fast forward
            _snwprintf(line, 32, L"%d:%02d", pos / 60000, pos / 1000 % 60);
            T(dc, 272, 284, line, 17, kMuted, 600);
            _snwprintf(line, 32, L"%d:%02d", len / 60000, len / 1000 % 60);
            T(dc, 784, 284, line, 17, kMuted, 600, AL_END);
        }
    } else {
        const wchar_t* head = on ? L"Играет с телефона" : L"Сейчас другой источник";
        if (proxy && bt.av == 0)
            head = L"Телефон не подключён";
        T(dc, 272, 192, head, 30, on ? kText : kMuted, 700, AL_START, 512);
        T(dc, 272, 222, on ? L"Звук идёт с подключённого телефона." : L"Телефон должен быть подключён по Bluetooth.",
          18, kMuted, 400, AL_START, 512);
    }
    if (!on)   // not the source (the phone not connected, or the switch did not take): by hand
        Button(dc, 584, 124, 200, 40, L"Включить звук", B_N, IC_BT, 14, 20, A_BTMUSIC);
    bool ctl = proxy && bt.av >= 1;   // the buttons: the SDK's AVRCP keys, through the proxy
    bool playing = bt.playing ? bt.playing == 1 : bt.av == 2;
    // as the music's: bare arrows, the big orange play (the user 2026-10-24); without the phone dim and dead
    IconBtn(dc, IC_PREV, 392, 312, 48, ctl ? kText : kOff, ctl ? A_BTCMD : A_NONE, BT_CMD_PREV);
    bool down = ctl && Zone(486, 294, 84, 84, A_BTCMD, BT_CMD_PLAYPAUSE);
    Disc(dc, 528, 336, 42, !ctl ? kPanel2 : down ? kFacet : kOrange);
    Ico(dc, playing ? IC_PAUSE : IC_PLAY, 504, 312, 48, ctl ? kInk : kOff);
    IconBtn(dc, IC_FWD, 616, 312, 48, ctl ? kText : kOff, ctl ? A_BTCMD : A_NONE, BT_CMD_NEXT);
    if (!proxy)
        T(dc, 272, 392, L"Установите proxy Bluetooth", 14, kDim, 400, AL_START, 512);
}

static void ScrAux(HDC dc)
{
    Frame(dc, L"Медиа · AUX");
    MediaTabs(dc, S_AUX, IC_NONE, A_NONE, 0);
    Cover(dc, IC_AUX, 16, 124, 232);
    T(dc, 272, 150, L"Внешний источник", 14, kMuted, 600);
    T(dc, 272, 200, L"AUX", 52, kText, 700);
    T(dc, 272, 236, L"Звук идёт с разъёма 3,5 мм. Управляйте", 18, kMuted, 400, AL_START, 512);
    T(dc, 272, 260, L"воспроизведением на самом устройстве.", 18, kMuted, 400, AL_START, 512);
    // (no input level: the MCU has no AUX gain - the shell has none either, Ghidra 2026-10-24; the mockup's stepper
    // did nothing)
}

static void ScrPhoto(HDC dc);       // (with the photos' worker, below)
static void ScrPhotoView(HDC dc);

// Phone, a main screen: the number, recent calls and the way to the contacts on the left, the keypad on the right.
// Phone: the stock shell's Bluetooth stack through our proxy (btshm.h): the connected phone, its operator, battery
// and network; the keypad dials (Flc_Bluetooth_Hf_DialNumber), an incoming call opens its window by itself (the
// 1 s timer), answer / end - the SDK's Hf_Answer / Hf_End; the phone book and the call log - PBAP (below).
static void BtCmd(int cmd);
static void BtText(const char* u8, wchar_t* w, int cap);

static void BtCmdText(int cmd, const wchar_t* text)
{
    BtShm* b = BtShmGet();
    if (!b || b->magic != BT_SHM_MAGIC)
        return;
    BtCmdWait(b);
    WideCharToMultiByte(CP_ACP, 0, text, -1, b->cmdText, sizeof(b->cmdText), NULL, NULL);
    b->cmdText[sizeof(b->cmdText) - 1] = 0;
    BtCmd(cmd);
}

static const wchar_t* BtLastAddr();   // (vh_settings: bt.last)
static int FavFind(const wchar_t* num);

static void ScrPhone(HDC dc)
{
    static const wchar_t* keys = L"123456789*0#";
    BtState bt;
    bool proxy = BtGet(&bt, NULL);
    bool on = proxy && bt.hf == 2;
    wchar_t title[160], name[BT_TEXT], s[120];
    if (on && bt.devName[0]) {
        BtText(bt.devName, name, BT_TEXT);
        _snwprintf(title, 160, L"Телефон · %s", name);
    } else {
        wcscpy(title, L"Телефон");
    }
    Frame(dc, title);
    Panel(dc, 16, 64, 356, 72, 12);
    int end = T(dc, 32, 112, s_dial, 28, kText, 700, AL_START, 270);
    R(dc, end + 4, 86, 3, 28, 0, kOrange);
    IconBtn(dc, IC_BKSP, 320, 81, 38, kMuted, A_DEL, K_DIAL);
    Panel(dc, 16, 148, 356, 196, 12);
    if (!proxy) {
        T(dc, 32, 190, L"Нет proxy Bluetooth", 20, kText, 700, AL_START, 324);
        T(dc, 32, 220, L"VestaHomeInstall, кнопка 2", 16, kMuted, 400, AL_START, 324);
    } else if (!on) {   // the phone last connected (bt.last) with its name from the paired list
        T(dc, 32, 190, L"Телефон не подключён", 20, kText, 700, AL_START, 324);
        wchar_t last[48] = L"";
        char a[18];
        WideCharToMultiByte(CP_ACP, 0, BtLastAddr(), -1, a, 18, NULL, NULL);
        for (int i = 0; i < bt.paired && i < BT_PAIRED && a[0]; i++)
            if (!_stricmp(bt.pairedAddr[i], a))
                BtText(bt.pairedName[i], last, 48);
        if (BtLastAddr()[0]) {
            _snwprintf(s, 120, L"последний: %s", last[0] ? last : BtLastAddr());
            T(dc, 32, 218, s, 16, kMuted, 400, AL_START, 324);
            Button(dc, 32, 240, 150, 44, L"Подключить", B_PRI, IC_NONE, 14, 0, A_BTCONNECT);
        }
        Button(dc, BtLastAddr()[0] ? 194 : 32, 240, 162, 44, L"Сопряжённые", B_N, IC_NONE, 14, 0, A_SCREEN, S_BT_PAIRED);
        T(dc, 32, 316, L"Новый телефон: найдите «VESTA» в его Bluetooth", 14, kDim, 400, AL_START, 324);
    } else {
        T(dc, 32, 190, bt.devName[0] ? name : L"Телефон", 22, kText, 700, AL_START, 324);
        BtText(bt.operatorName, name, BT_TEXT);
        T(dc, 32, 220, name[0] ? name : L"оператор —", 17, kMuted, 600, AL_START, 324);
        s[0] = 0;
        if (bt.battery >= 0)
            _snwprintf(s, 120, L"батарея %d/5", bt.battery);
        if (bt.signal >= 0) {
            wchar_t t[40];
            _snwprintf(t, 40, L"%sсеть %d/5", s[0] ? L" · " : L"", bt.signal);
            wcscat(s, t);
        }
        T(dc, 32, 250, s, 17, kMuted, 400, AL_START, 324);
        T(dc, 32, 300, bt.av >= 1 ? L"музыка: подключена" : L"музыка: нет", 15, kDim, 400, AL_START, 324);
    }
    Button(dc, 16, 354, 172, 44, L"Контакты", B_N, IC_USERS, 15, 24, A_SCREEN, S_CONTACTS);
    Button(dc, 200, 354, 172, 44, L"Журнал", B_N, IC_CLOCK, 15, 24, A_SCREEN, S_CALLS);
    for (int i = 0; i < 12; i++) {   // keypad 3 x 4
        int x = 388 + (i % 3) * 136, y = 64 + (i / 3) * 62;
        wchar_t k[2] = { keys[i], 0 };
        bool d = Zone(x, y, 124, 56, A_KEY, K_DIAL * 65536 + keys[i]);
        R(dc, x, y, 124, 56, 16, d ? kOrange : kPanel);
        T(dc, x + 62, y + 39, k, 30, d ? kInk : kText, 700, AL_MID);
        if (keys[i] == L'0')   // held: "+"
            T(dc, x + 100, y + 38, L"+", 16, d ? kInk : kMuted, 700, AL_MID);
    }
    bool can = on && s_dial[0];
    bool d = Zone(388, 316, 192, 82, can ? A_PHONECALL : A_NONE);
    R(dc, 388, 316, 192, 82, 12, !can ? kPanel2 : d ? Mix(kOk, kInk, 160) : kOk);
    Ico(dc, IC_CALL, 462, 335, 44, can ? kInk : kDim);
    d = Zone(592, 316, 192, 82, s_dial[0] ? A_PHONECLR : A_NONE);
    R(dc, 592, 316, 192, 82, 12, d ? Mix(kStop, kInk, 160) : kStop);
    Ico(dc, IC_HANGUP, 666, 335, 44, kInk);
}

// ---- the phone book and the call log (PBAP through the proxy Bluetooth, btshm.h BtPb): pulled from the connected
// phone when their screen opens (once per phone, "ОБНОВИТЬ" again), kept in memory only (never on the flash). A pull
// the shell made itself (its book, when PBAP connects) is taken too.
static void Redraw();
static void Go(Screen s, bool push);
static BtPb*      s_pbShm;
static BtPbEntry* s_book;           // the phone book, s_bookN entries (-1: not pulled yet)
static BtPbEntry* s_calls;          // all calls (cch), newest first
static int        s_bookN = -1, s_callsN = -1;
static char       s_pbPhone[18];    // the phone they came from
static int        s_pbWant = -1;    // the list asked for (BT_PB_*), -1 none
static LONG       s_pbSeq;          // BtPb's seq when it was asked
static DWORD      s_pbTick;
static bool       s_pbFail;         // the last pull failed
static int        s_pbContact = -1; // the contact open (S_CONTACT)
static int        s_pbStorage;      // the book from: 0 the phone, 1 its SIM ("ТЕЛ." / "SIM")
static wchar_t    s_pbFind[24];     // the contacts' search: a part of the name or the number (the keyboard)
enum { kPbTop = 120, kPbBottom = 386, kPbPitch = 62 };
static Scroll     s_bookList = { 0, 0, 4 * kPbPitch }, s_callList = { 0, 0, 4 * kPbPitch };

static BtPb* PbShm()
{
    if (!s_pbShm) {
        HANDLE m = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(BtPb), BT_PB_NAME);
        if (m)
            s_pbShm = (BtPb*)MapViewOfFile(m, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(BtPb));
    }
    return s_pbShm && s_pbShm->magic == BT_SHM_MAGIC ? s_pbShm : NULL;   // (0: a proxy without the book)
}

static void PbAsk(int list)
{
    if (!PbShm())
        return;
    s_pbWant = list;
    s_pbSeq = s_pbShm->seq;
    s_pbTick = GetTickCount();
    s_pbFail = false;
    wchar_t t[8];
    _snwprintf(t, 8, L"%d %d", list == BT_PB_PB ? s_pbStorage : 0, list);
    BtCmdText(BT_CMD_PBAP_PULL, t);
}

// The finished pull of list into our copy; false while it is not there (or it changed while copied)
static bool PbTake(int list)
{
    BtPb* pb = PbShm();
    if (!pb || pb->state != BT_PBS_DONE || pb->list != list || pb->storage != (list == BT_PB_PB ? s_pbStorage : 0))
        return false;
    LONG seq = pb->seq;
    int n = pb->count;
    BtPbEntry* e = n ? (BtPbEntry*)malloc(n * sizeof(BtPbEntry)) : NULL;
    if (n && !e)
        return false;
    if (n)
        memcpy(e, pb->e, n * sizeof(BtPbEntry));
    if (pb->seq != seq || pb->state != BT_PBS_DONE) {
        free(e);
        return false;
    }
    BtPbEntry*& to = list == BT_PB_PB ? s_book : s_calls;
    free(to);
    to = e;
    (list == BT_PB_PB ? s_bookN : s_callsN) = n;
    VLog(L"телефон: %s — %d записей (из %d)", list == BT_PB_PB ? L"книга" : L"журнал", n, pb->total);
    return true;
}

// The screens' own list: the phone changed - all again; not there yet - the shell's pull or ours
static void PbNeed(int list)
{
    BtState bt;
    if (!BtGet(&bt, NULL) || bt.hf != 2 || !PbShm())
        return;
    if (strcmp(bt.devAddr, s_pbPhone)) {
        strcpy(s_pbPhone, bt.devAddr);
        free(s_book);
        free(s_calls);
        s_book = s_calls = NULL;
        s_bookN = s_callsN = -1;
        s_bookList.pos = s_callList.pos = 0;
    }
    if ((list == BT_PB_PB ? s_bookN : s_callsN) >= 0 || s_pbWant >= 0 || s_pbFail)
        return;
    if (!PbTake(list))
        PbAsk(list);
}

static void PbTick()   // the 1 s timer: a pull asked for has ended
{
    BtPb* pb = PbShm();
    if (s_pbWant < 0 || !pb)
        return;
    BtShm* b = BtShmGet();
    bool busy = b && b->cmd == BT_CMD_PBAP_PULL && b->cmdDone == b->cmdSeq && b->cmdResult == -2;
    if (busy && GetTickCount() - s_pbTick > 3000) {   // another pull ran: ask again
        PbAsk(s_pbWant);
        return;
    }
    bool ended = pb->seq != s_pbSeq && pb->list == s_pbWant;
    if (ended && pb->state == BT_PBS_DONE && PbTake(s_pbWant)) {
        s_pbWant = -1;
    } else if ((ended && pb->state == BT_PBS_FAILED) || GetTickCount() - s_pbTick > 90000) {
        VLog(L"телефон: список %d не получен (%s)", s_pbWant, ended ? L"отказ" : L"нет ответа");
        s_pbWant = -1;
        s_pbFail = true;
    } else {
        if (pb->seq != s_pbSeq)   // another list's pull ended (the shell's): ours still to come
            s_pbSeq = pb->seq;
        return;
    }
    if (s_screen == S_CONTACTS || s_screen == S_CONTACT || s_screen == S_CALLS)
        Redraw();
}

static void PbDial(const char* num)
{
    wchar_t w[20];
    BtText(num, w, 20);
    wcscpy(s_callOut, w);
    BtCmdText(BT_CMD_DIAL, w);
    Go(S_CALL, true);
}

// The panel's head: what there is, the pull going on or failed, "update"; false = no list to show
static bool PbHead(HDC dc, int list, int n, const wchar_t* what)
{
    BtState bt;
    bool proxy = BtGet(&bt, NULL);
    Panel(dc, 192, 68, 528, 322);
    const wchar_t* why = !proxy ? L"Нет proxy Bluetooth (VestaHomeInstall, кнопка 2)"
                       : !PbShm() ? L"Proxy Bluetooth старый — переустановите (кнопка 2)"
                       : bt.hf != 2 ? L"Телефон не подключён" : NULL;
    if (why) {
        T(dc, 216, 140, why, 20, kText, 700, AL_START, 480);
        return false;
    }
    wchar_t s[64];
    if (s_pbWant == list)
        _snwprintf(s, 64, L"Загрузка с телефона…");
    else if (s_pbFail)
        wcscpy(s, L"Телефон не отдал список");
    else
        _snwprintf(s, 64, L"%s: %d", what, n < 0 ? 0 : n);
    T(dc, 216, 100, s, 17, s_pbFail ? kStop : kMuted, 600, AL_START, list == BT_PB_PB ? 160 : 300);
    if (list == BT_PB_PB) {   // the book: the phone's / the SIM's, the search
        Button(dc, 384, 76, 80, 36, s_pbStorage ? L"SIM" : L"Тел.", s_pbStorage ? B_SEL : B_N, IC_NONE, 14, 0,
               s_pbWant >= 0 ? A_NONE : A_PBSIM);
        Button(dc, 472, 76, 80, 36, s_pbFind[0] ? s_pbFind : NULL, s_pbFind[0] ? B_SEL : B_N, IC_SEARCH, 14, 20,
               A_PBFIND);
    }
    Button(dc, 560, 76, 152, 36, L"Обновить", s_pbWant >= 0 ? B_DIS : B_N, IC_REFRESH, 14, 20,
           s_pbWant >= 0 ? A_NONE : A_PBRELOAD, list);
    if (s_pbFail && n < 0) {
        T(dc, 216, 160, L"Разрешите на телефоне доступ к контактам и журналу", 16, kMuted, 400, AL_START, 480);
        T(dc, 216, 184, L"для «VESTA» и нажмите «Обновить».", 16, kMuted, 400, AL_START, 480);
    }
    return n > 0;
}

static void ScrContacts(HDC dc)
{
    Frame(dc, L"Телефон · Контакты");
    PhoneNav(dc, 1);
    PbNeed(BT_PB_PB);
    if (PbHead(dc, BT_PB_PB, s_bookN, s_pbStorage ? L"На SIM" : L"Контактов")) {
        static int idx[BT_PB_MAX];   // the ones the search leaves (all without it)
        int n = 0;
        wchar_t find[24];
        wcscpy(find, s_pbFind);
        CharLowerBuff(find, wcslen(find));
        for (int i = 0; i < s_bookN && n < BT_PB_MAX; i++) {
            if (find[0]) {
                wchar_t w[64], num[24];
                BtText(s_book[i].name, w, 64);
                CharLowerBuff(w, wcslen(w));
                BtText(s_book[i].num[0], num, 24);
                if (!wcsstr(w, find) && !wcsstr(num, find))
                    continue;
            }
            idx[n++] = i;
        }
        if (!n)
            T(dc, 216, 160, L"Ничего не найдено", 18, kMuted, 600);
        s_bookList.content = n * kPbPitch - 6 + 8;
        ScrollBegin(dc, s_bookList, 196, kPbTop, 716, kPbBottom);
        for (int k = 0; k < n; k++) {
            int i = idx[k], y = kPbTop + 4 + k * kPbPitch - s_bookList.pos;
            if (y + 56 <= kPbTop || y >= kPbBottom)
                continue;
            const BtPbEntry& e = s_book[i];
            wchar_t name[64], num[24], more[8];
            BtText(e.name[0] ? e.name : e.num[0], name, 64);
            BtText(e.name[0] ? e.num[0] : "", num, 24);
            _snwprintf(more, 8, L"+%d", e.num[2][0] ? 2 : 1);
            ListRow(dc, 200, y, 512, 56, name, e.num[1][0] ? more : NULL, L_N, IC_USER, num[0] ? num : NULL, NULL,
                    A_PBCONTACT, i);
        }
        ScrollEnd(dc, s_bookList, 728, 68, 56, 322, 96);
    }
}

static void ScrContact(HDC dc)
{
    Frame(dc, L"Телефон · Контакт");
    PhoneNav(dc, 1);
    Panel(dc, 192, 68, 592, 322);
    if (s_pbContact < 0 || s_pbContact >= s_bookN) {
        T(dc, 216, 140, L"Контакта нет", 20, kText, 700);
        return;
    }
    const BtPbEntry& e = s_book[s_pbContact];
    wchar_t w[64];
    BtText(e.name, w, 64);
    T(dc, 216, 112, w[0] ? w : L"Без имени", 26, kText, 700, AL_START, 480);
    wchar_t num0[24];
    BtText(e.num[0], num0, 24);
    bool fav = num0[0] && FavFind(num0) >= 0;
    Button(dc, 712, 78, 56, 48, NULL, fav ? B_SEL : B_N, fav ? IC_STAR_F : IC_STAR, 15, 26, A_PBFAV, s_pbContact);
    for (int k = 0; k < BT_PB_NUMS && e.num[k][0]; k++) {
        BtText(e.num[k], w, 64);
        ListRow(dc, 208, 140 + k * 64, 560, 56, w, L"Позвонить", L_N, IC_CALL, NULL, NULL, A_PBDIAL,
                s_pbContact * 4 + k);
    }
}

static void ScrCalls(HDC dc)
{
    Frame(dc, L"Телефон · Журнал");
    PhoneNav(dc, 3);
    PbNeed(BT_PB_CCH);
    if (PbHead(dc, BT_PB_CCH, s_callsN, L"Звонков")) {
        s_callList.content = s_callsN * kPbPitch - 6 + 8;
        ScrollBegin(dc, s_callList, 196, kPbTop, 716, kPbBottom);
        for (int i = 0; i < s_callsN; i++) {
            int y = kPbTop + 4 + i * kPbPitch - s_callList.pos;
            if (y + 56 <= kPbTop || y >= kPbBottom)
                continue;
            const BtPbEntry& e = s_calls[i];
            wchar_t name[64], num[24], when[16] = L"";
            BtText(e.name[0] ? e.name : e.num[0][0] ? e.num[0] : "?", name, 64);
            BtText(e.name[0] ? e.num[0] : "", num, 24);
            if (strlen(e.time) >= 13)   // "20261021T153000" -> "21.10 15:30"
                _snwprintf(when, 16, L"%c%c.%c%c %c%c:%c%c", e.time[6], e.time[7], e.time[4], e.time[5], e.time[9],
                           e.time[10], e.time[11], e.time[12]);
            Icon ic = e.callType == BT_PBC_MISSED ? IC_CALL_MISS : e.callType == BT_PBC_RECEIVED ? IC_CALL_IN
                    : IC_CALL_OUT;
            ListRow(dc, 200, y, 512, 56, name, when[0] ? when : NULL, L_N, ic, num[0] ? num : NULL, NULL,
                    e.num[0][0] ? A_PBDIAL : A_NONE, 4000 + i, e.callType == BT_PBC_MISSED ? kStop : CLR_INVALID);
        }
        ScrollEnd(dc, s_callList, 728, 68, 56, 322, 96);
    }
}

// A call: its number, its state as the stack reports it, the time talking; "end" - Hf_End.
static void ScrCall(HDC dc)
{
    BtState bt;
    BtGet(&bt, NULL);
    int c = bt.call[0];
    const wchar_t* st = c == BT_CALL_INCOMING ? L"Входящий вызов" : c == BT_CALL_DIALING ? L"Набор номера"
                      : c == BT_CALL_ALERTING ? L"Вызов" : c == BT_CALL_ACTIVE ? L"Разговор"
                      : c == BT_CALL_HELD ? L"На удержании" : c == BT_CALL_WAITING ? L"Ожидает" : L"Звонок";
    wchar_t num[40];
    if (bt.callNumber[0] && c != BT_CALL_DIALING && c != BT_CALL_ALERTING)
        BtText(bt.callNumber, num, 40);
    else
        wcscpy(num, s_callOut[0] ? s_callOut : L"—");
    Frame(dc, L"Телефон");
    Disc(dc, 128, 168, 88, kPanel2);
    Ico(dc, IC_CALL, 96, 136, 64, c == BT_CALL_ACTIVE ? kOk : kMuted);
    T(dc, 256, 112, st, 14, kMuted, 700);
    T(dc, 256, 168, num, 40, kText, 700, AL_START, 528);
    if (c == BT_CALL_ACTIVE) {
        DWORD sec = (GetTickCount() - s_callTick) / 1000;
        wchar_t s[16];
        _snwprintf(s, 16, L"%02u:%02u", (unsigned)(sec / 60), (unsigned)(sec % 60));
        T(dc, 256, 236, s, 34, kText, 700);
    }
    bool d = Zone(216, 296, 368, 96, A_PHONEEND);
    R(dc, 216, 296, 368, 96, 12, d ? Mix(kStop, kInk, 160) : kStop);
    Ico(dc, IC_HANGUP, 376, 320, 48, kInk);
    Dock(dc);   // in a call the volume is the main control: the MCU's, as it is now
    IconBtn(dc, IC_VOL, 104, 417, 44, kMuted, A_SCREEN, S_VOLUME);   // (right of "back")
    int vol, mute, src;
    wchar_t v[16] = L"—";
    if (VolGet(&vol, &mute, &src))
        _snwprintf(v, 16, mute ? L"без звука" : L"%d", vol);
    T(dc, 168, 452, v, 22, kText, 700);
}

static int TzMinutes();
static void GpsPortText(wchar_t* s, int n);
static void GpsBaudText(wchar_t* s, int n);
static void GpsStatusText(wchar_t* s, int n, bool brief = false);

// Six rows of 48 px: the zone, the clock by hand (hours, minutes), its sync from GPS. The GPS port went to
// "Car" (it is the CAN module's port), "Factory settings" to "About the system".
static const wchar_t* AccentName();   // (with the settings' data)

static void ScrSetGeneral(HDC dc)
{
    static const wchar_t* fmt[2] = { L"12", L"24" };
    enum { H = 48 };
    SYSTEMTIME t;
    GetLocalTime(&t);
    wchar_t now[40], time[8], tz[16], hh[8], mm[8];
    Now(time, 8);
    _snwprintf(now, 40, L"сейчас %02d.%02d.%04d  %s", t.wDay, t.wMonth, t.wYear, time);
    int m = TzMinutes();
    if (m % 60)
        _snwprintf(tz, 16, L"UTC%c%d:%02d", m < 0 ? L'-' : L'+', abs(m) / 60, abs(m) % 60);
    else
        _snwprintf(tz, 16, L"UTC%c%d", m < 0 ? L'-' : L'+', abs(m) / 60);
    _snwprintf(hh, 8, L"%02d", t.wHour);
    _snwprintf(mm, 8, L"%02d", t.wMinute);
    Settings(dc, S_SETTINGS);
    SetRow(dc, 64, L"Часовой пояс", now, false, A_NONE, 0, H);
    Stepper(dc, 512, 68, 248, tz, 40, 19, A_TZ);
    SetRow(dc, 112, L"Часы", NULL, false, A_NONE, 0, H);
    Stepper(dc, 560, 116, 200, hh, 40, 19, A_HOUR);
    SetRow(dc, 160, L"Минуты", NULL, false, A_NONE, 0, H);
    Stepper(dc, 560, 164, 200, mm, 40, 19, A_MINUTE);
    SetRow(dc, 208, L"Синхронизация по GPS", NULL, false, A_NONE, 0, H);
    Toggle(dc, 688, 214, T_GPS_TIME);
    SetRow(dc, 256, L"Формат времени", NULL, false, A_NONE, 0, H);
    Seg(dc, 560, 260, 200, 40, fmt, 2, G_TIME_FMT);
    SetRow(dc, 304, L"Виджеты главной", NULL, false, A_SCREEN, S_SET_HOME, H);
    Chevron(dc, 728, 312);
    SetRow(dc, 352, L"Дополнения", NULL, true, A_SCREEN, S_SET_PLUGINS, H);   // (the theme: in "Screen", 2026-10-26)
    Chevron(dc, 728, 360);
}


static void ScrSetSound(HDC dc);
static void ScrSetBalance(HDC dc);
static void ScrSetVolume(HDC dc);

static void ScrSetScreen(HDC dc);   // (after the MCU's part: brightness)

// Settings -> Radio: the MCU's tuner settings. The switches show what the MCU reports (Tuner.set); a tap sends the
// stock shell's key for on / off (extended keys, SPIPower.log 2026-10-18), the MCU's reply moves the switch.
// Opening the screen asks for all of them (extended key 0x90, as the shell's screen does).
static const int kRsetToggle[RSET_COUNT] = { T_NOISE, T_MW, T_RDS, T_AF, T_CT, T_TA, T_MS };
static const int kRsetKey[RSET_COUNT][2] = { { 0xA8, 0xA7 }, { 0xAA, 0xA9 }, { 0x87, 0x86 }, { 0x8B, 0x8A },
                                             { 0x8F, 0x8E }, { 0x89, 0x88 }, { 0xAC, 0xAB } };   // off, on

static int RsetOfToggle(int id)
{
    for (int i = 0; i < RSET_COUNT; i++)
        if (kRsetToggle[i] == id)
            return i;
    return -1;
}

static void ScrSetRadio(HDC dc)
{
    static const wchar_t* rows[RSET_COUNT] = { L"Шумоподавление", L"Диапазон СВ", L"RDS",
                                               L"AF · альтернативные частоты", L"CT · время по радио",
                                               L"TA · дорожные сообщения", L"MS · музыка / речь" };
    static DWORD s_asked;
    if (!s_asked || GetTickCount() - s_asked > 10000) {   // once on opening (the screen redraws often)
        s_asked = GetTickCount();
        McuKey(0x90, true);
    }
    Tuner t;
    TunerGet(&t);
    Settings(dc, S_SET_RADIO);
    for (int i = 0, k = 0; i < RSET_COUNT; i++) {
        if (i == RSET_MW)
            continue;   // СВ: turned on by its page of the radio (2026-10-20, the user)
        bool known = (t.setKnown >> i & 1) != 0;
        if (known)
            s_on[kRsetToggle[i]] = t.set[i] != 0;
        SetRow(dc, 64 + k * 48, rows[i], NULL, i == RSET_COUNT - 1, A_NONE, 0, 48);
        if (known)
            Toggle(dc, 688, 70 + k * 48, kRsetToggle[i]);
        else
            T(dc, 760, 96 + k * 48, L"нет данных MCU", 14, kDim, 400, AL_END);
        k++;
    }
}

// Settings -> Bluetooth (the phone, the OBD adapter): the stock shell's Bluetooth stack (FLC SDK in G3NavHMI) owns pairing and the
// connected phone; with our proxy of FLC_BLUETOOTH_SDK.dll (btshm.h) VestaHome shows the phone and its links.
// Pairing and the options (auto-connect, auto-answer) - the next step, through the same proxy.
static bool McuBtAddr(wchar_t* s, int n);

static const wchar_t* BtUnitName();   // (vh_settings.inc: bt.name, "VESTA" without one)

static void ScrSetBt(HDC dc)
{
    Settings(dc, S_SET_BT);
    enum { H = 42 };   // 8 rows (the unit's name added 2026-10-27)
    wchar_t a[24];
    int vol, mute, src;
    VolGet(&vol, &mute, &src);
    BtState bt;
    if (BtGet(&bt, NULL)) {
        static DWORD s_seen;   // opened: the stack's options (the proxy's BT_CMD_GETOPTS, its answer waited for)
        if (s_seen != s_screenTick) {
            s_seen = s_screenTick;
            BtShm* b = BtShmGet();
            BtCmd(BT_CMD_GETOPTS);
            for (int i = 0; i < 30 && b && b->cmdDone != b->cmdSeq; i++)
                Sleep(10);
            if (b && b->cmdDone == b->cmdSeq && b->cmdResult >= 0) {
                s_on[T_AUTOCONNECT] = (b->cmdResult & 1) != 0;
                s_on[T_AUTOANSWER] = (b->cmdResult & 2) != 0;
            }
        }
        wchar_t name[BT_TEXT], s[200];
        if (bt.devName[0]) {
            BtText(bt.devName, name, BT_TEXT);
            _snwprintf(s, 200, L"%s · %S", name, bt.devAddr);
        } else {
            wcscpy(s, bt.ready ? L"нет" : L"Bluetooth оболочки ещё не запущен");
        }
        SetRow(dc, 64, L"Телефон", s, false, A_NONE, 0, H);
        const wchar_t* av = bt.av == 2 ? L"играет" : bt.av == 1 ? L"подключена" : bt.av == 0 ? L"нет" : L"—";
        const wchar_t* hf = bt.hf == 2 ? L"подключены" : bt.hf == 0 ? L"нет" : L"—";
        wchar_t bs[40] = L"", ss[40] = L"";
        if (bt.battery >= 0)
            _snwprintf(bs, 40, L" · батарея %d", bt.battery);
        if (bt.signal >= 0)
            _snwprintf(ss, 40, L" · сеть %d", bt.signal);
        _snwprintf(s, 200, L"музыка: %s · звонки: %s%s%s · ГУ %s", av, hf, bs, ss, McuBtAddr(a, 24) ? a : L"—");
        SetRow(dc, 106, L"Связь", s, false, A_NONE, 0, H);
        _snwprintf(s, 200, L"%s · видно телефонам, меняется после перезагрузки", BtUnitName());
        SetRow(dc, 148, L"Имя ГУ", s, true, A_KEYS, K_BTNAME, H);
        SetRow(dc, 190, L"Музыка с телефона", src == 9 ? L"играет сейчас" : L"другой источник", false, A_NONE, 0, H);
        if (src != 9)
            Button(dc, 560, 191, 200, 38, L"Включить", B_N, IC_BT, 15, 22, A_BTMUSIC);
        SetRow(dc, 232, L"Автосоединение", L"последний телефон — при включении ГУ", false, A_NONE, 0, H);
        Toggle(dc, 688, 235, T_AUTOCONNECT);
        SetRow(dc, 274, L"Автоответ", L"входящий звонок принимается через 5 с", false, A_NONE, 0, H);
        Toggle(dc, 688, 277, T_AUTOANSWER);
        s[0] = 0;   // the paired ones (0x08), by name
        for (int i = 0; i < bt.paired && i < BT_PAIRED && wcslen(s) < 150; i++) {
            wchar_t nm[48];
            BtText(bt.pairedName[i], nm, 48);
            if (s[0])
                wcscat(s, L", ");
            wcsncat(s, nm, 40);
        }
        SetRow(dc, 316, L"Сопряжённые", s[0] ? s : L"—", false, A_SCREEN, S_BT_PAIRED, H);
        SetRow(dc, 358, L"OBD-адаптер (проверка)", L"поиск, сопряжение и опрос ELM327", true, A_SCREEN, S_SET_OBD, H);
    } else {
        SetRow(dc, 64, L"Адрес Bluetooth ГУ", McuBtAddr(a, 24) ? a : L"MCU ещё не прислал (нужен proxy MCU)");
        SetRow(dc, 120, L"Музыка с телефона", src == 9 ? L"играет сейчас" : L"другой источник");
        if (src != 9)
            Button(dc, 560, 126, 200, 44, L"Включить", B_N, IC_BT, 15, 22, A_BTMUSIC);
        SetRow(dc, 176, L"Сопряжение и подключение", L"пока в штатной оболочке: Телефон → Bluetooth", true);
        T(dc, 264, 268, L"Подключённый телефон и его связь появятся здесь с proxy Bluetooth", 15, kMuted, 400,
          AL_START, 500);
        T(dc, 264, 290, L"(VestaHomeInstall, кнопка 2: ставится вместе с proxy MCU).", 15, kMuted, 400, AL_START, 500);
    }
}

// Settings -> Bluetooth -> paired: the stack's list (asked anew on opening: the proxy's BT_CMD_LISTPAIRED), 7 a
// page (the dots turn it); "УДАЛИТЬ" asks once more ("ТОЧНО?"), then Conn_Delete_Paired (BT_CMD_UNPAIR).
enum { kPairedRows = 6 };   // under them: "+ НОВЫЙ ТЕЛЕФОН"
static int s_pairedPage, s_pairedAsk = -1;   // s_pairedAsk: the row whose removal waits for the second press

static void ScrBtPaired(HDC dc)
{
    Settings(dc, S_BT_PAIRED);
    static DWORD s_seen;
    if (s_seen != s_screenTick) {
        s_seen = s_screenTick;
        s_pairedAsk = -1;
        BtCmd(BT_CMD_LISTPAIRED);
    }
    BtState bt;
    if (!BtGet(&bt, NULL)) {
        SetRow(dc, 64, L"Сопряжённые", L"нужен proxy Bluetooth (VestaHomeInstall, кнопка 2)", true);
        return;
    }
    int n = bt.paired < BT_PAIRED ? bt.paired : BT_PAIRED, pages = (n + kPairedRows - 1) / kPairedRows;
    if (s_pairedPage >= pages)
        s_pairedPage = 0;   // (the dots go round)
    if (!n)
        SetRow(dc, 64, L"Сопряжённых нет", L"новый телефон — кнопка ниже", true);
    Button(dc, 256, 340, 220, 40, L"+ Новый телефон", B_N, IC_NONE, 15, 0, A_SCREEN, S_BT_PAIRNEW);
    for (int k = 0; k < kPairedRows; k++) {
        int i = s_pairedPage * kPairedRows + k, y = 64 + k * 44;
        if (i >= n)
            break;
        wchar_t nm[48], sub[64];
        BtText(bt.pairedName[i], nm, 48);
        bool on = !strcmp(bt.pairedAddr[i], bt.devAddr) && bt.hf >= 1;
        _snwprintf(sub, 64, L"%S%s", bt.pairedAddr[i], on ? L" · подключён" : L"");
        sub[63] = 0;
        T(dc, 264, y + 20, nm[0] ? nm : L"без имени", 17, kText, 700, AL_START, 360);
        T(dc, 264, y + 38, sub, 13, on ? kOrange : kMuted, 400, AL_START, 360);
        Button(dc, 640, y + 4, 128, 36, s_pairedAsk == i ? L"Точно?" : L"Удалить", s_pairedAsk == i ? B_SEL : B_N,
               IC_NONE, 14, 0, A_BTUNPAIR, i);
        if (k + 1 < kPairedRows && i + 1 < n)
            Sep(dc, 256, y + 43, 512);
    }
    PageDots(dc, s_pairedPage, pages, 512, 386);
}

static int PairedPages()
{
    BtState bt;
    int n = BtGet(&bt, NULL) ? (bt.paired < BT_PAIRED ? bt.paired : BT_PAIRED) : 0;
    return n ? (n + kPairedRows - 1) / kPairedRows : 1;
}

// Settings -> Bluetooth -> OBD (a test, 2026-10-20): an ELM327 adapter found by the stack's search (the proxy's
// BT_CMD_SEARCH; the devices come as events with their address at byte 4 and name at 0x16), paired with its PIN
// (BT_CMD_PAIR), then connected over the stack's serial port and asked by ELM327 commands (obd.cpp). Every step
// shows here and goes to the log.
enum { kObdFound = 6 };
struct ObdDev { char addr[18]; char name[48]; };
static ObdDev s_obdFound[kObdFound];
static int    s_obdFoundN, s_obdPin;   // s_obdPin: kObdPins
static wchar_t s_obdAddr[18];        // the adapter chosen (ini obd.addr), "" none
static LONG   s_obdEvSeen = -1;        // the proxy's events read so far; -1: no search asked
static DWORD  s_obdSearchTick;
static const char* const kObdPins[2] = { "1234", "0000" };

// obd.cpp's thread: the adapter's address given as the shell's SDK gives it (Spp_SetAddr in the shell's process,
// the proxy's BT_CMD_SPPADDR); the SDK's result, -1 no proxy (or an old one), -2 no answer in 2 s.
static int ObdSppAddr(const char* addr)
{
    BtShm* b = BtShmGet();
    if (!b || b->magic != BT_SHM_MAGIC || !b->origOk)
        return -1;
    wchar_t w[18];
    MultiByteToWideChar(CP_ACP, 0, addr, -1, w, 18);
    w[17] = 0;
    BtCmdText(BT_CMD_SPPADDR, w);
    LONG want = b->cmdSeq;
    DWORD t0 = GetTickCount();
    while (b->cmdDone != want && GetTickCount() - t0 < 2000)
        Sleep(50);
    return b->cmdDone == want ? b->cmdResult : -2;
}

static bool ObdScanEvents()
{
    BtShm* b = BtShmGet();
    if (s_obdEvSeen < 0 || !b || b->magic != BT_SHM_MAGIC)
        return false;
    bool changed = false;
    LONG last = b->evSeq;
    if (last - s_obdEvSeen > BT_EV_SLOTS)
        s_obdEvSeen = last - BT_EV_SLOTS;
    for (LONG n = s_obdEvSeen + 1; n <= last; n++) {
        BtEvent e = b->ev[(n - 1) & (BT_EV_SLOTS - 1)];
        const char* a = (const char*)e.data;
        if (e.seq != n || e.code == 0x0A || a[2] != ':' || a[5] != ':' || a[14] != ':')
            continue;
        int i = 0;
        while (i < s_obdFoundN && strncmp(s_obdFound[i].addr, a, 17))
            i++;
        if (i == kObdFound)
            continue;
        if (i == s_obdFoundN)
            s_obdFoundN++;
        memcpy(s_obdFound[i].addr, a, 17);
        s_obdFound[i].addr[17] = 0;
        int k = 0;
        for (; k < 47 && 18 + k < BT_EV_DATA && a[18 + k]; k++)
            s_obdFound[i].name[k] = a[18 + k];
        s_obdFound[i].name[k] = 0;
        VLog(L"obd: ev 0x%02X %S %S", (unsigned)e.code, s_obdFound[i].addr, s_obdFound[i].name);
        changed = true;
    }
    s_obdEvSeen = last;
    return changed;
}

static void ScrSetObd(HDC dc)
{
    Settings(dc, S_SET_OBD);
    BtState bt;
    bool proxy = BtGet(&bt, NULL);
    ObdInfo o;
    ObdGet(&o);
    wchar_t s[160], nm[48];
    char addr[18];
    WideCharToMultiByte(CP_ACP, 0, s_obdAddr, -1, addr, 18, NULL, NULL);
    addr[17] = 0;
    wcscpy(s, L"не выбран: НАЙТИ, затем нажмите на адаптер");
    if (addr[0]) {
        _snwprintf(s, 160, L"%s", s_obdAddr);
        for (int i = 0; i < s_obdFoundN; i++)
            if (!strcmp(s_obdFound[i].addr, addr) && s_obdFound[i].name[0]) {
                BtText(s_obdFound[i].name, nm, 48);
                _snwprintf(s, 160, L"%s · %s", nm, s_obdAddr);
            }
    }
    s[159] = 0;
    SetRow(dc, 64, L"OBD-адаптер (ELM327)", s);
    bool searching = s_obdEvSeen >= 0 && GetTickCount() - s_obdSearchTick < 15000;
    Button(dc, 656, 70, 112, 44, searching ? L"Поиск…" : L"Найти", !proxy ? B_DIS : searching ? B_SEL : B_N, IC_NONE,
           15, 0, proxy ? A_OBDSCAN : A_NONE);
    for (int i = 0; i < s_obdFoundN; i++) {
        if (s_obdFound[i].name[0])
            BtText(s_obdFound[i].name, nm, 48);
        else
            MultiByteToWideChar(CP_ACP, 0, s_obdFound[i].addr, -1, nm, 48);
        Button(dc, 256 + (i % 2) * 260, 124 + (i / 2) * 46, 252, 40, nm,
               !strcmp(s_obdFound[i].addr, addr) ? B_SEL : B_N, IC_NONE, 15, 0, A_OBDPICK, i);
    }
    if (!s_obdFoundN)
        T(dc, 264, 168, !proxy ? L"нужен proxy Bluetooth (VestaHomeInstall, кнопка 2)"
                     : searching ? L"ищу устройства (около 10 с)…"
                     : L"адаптер в разъём OBD, зажигание включено, затем НАЙТИ", 15, kMuted, 400, AL_START, 500);
    T(dc, 264, 296, L"PIN", 15, kMuted, 700);
    for (int i = 0; i < 2; i++) {
        wchar_t p[8];
        MultiByteToWideChar(CP_ACP, 0, kObdPins[i], -1, p, 8);
        Button(dc, 304 + i * 88, 270, 80, 40, p, i == s_obdPin ? B_SEL : B_N, IC_NONE, 15, 0, A_OBDPIN, i);
    }
    bool can = addr[0] != 0;
    bool busy = o.state == OBD_CONNECTING || o.state == OBD_ON;
    Button(dc, 488, 270, 136, 40, L"Сопрячь", can && proxy ? B_N : B_DIS, IC_NONE, 15, 0,
           can && proxy ? A_OBDPAIR : A_NONE);
    Button(dc, 632, 270, 136, 40, busy ? L"Отключить" : L"Подключить", can || busy ? (busy ? B_SEL : B_N) : B_DIS,
           IC_NONE, 15, 0, can || busy ? A_OBDCONN : A_NONE);
    int y = 336, first = 1;   // four lines fit, or the values and three
    if (o.state == OBD_ON) {
        wchar_t v[3][16];
        _snwprintf(v[0], 16, o.rpm >= 0 ? L"%d" : L"—", o.rpm);
        _snwprintf(v[1], 16, o.speed >= 0 ? L"%d" : L"—", o.speed);
        _snwprintf(v[2], 16, o.coolant != -1 ? L"%d" : L"—", o.coolant);
        _snwprintf(s, 160, L"обороты %s · скорость %s км/ч · ОЖ %s °C · %s", v[0], v[1], v[2],
                   o.volt[0] ? o.volt : L"—");
        s[159] = 0;
        T(dc, 264, y, s, 17, kOrange, 700, AL_START, 504);
        y += 22;
        first = 2;
    }
    for (int i = first; i < kObdLines; i++, y += 18)
        if (o.lines[i][0])
            T(dc, 264, y, o.lines[i], 13, kMuted, 400, AL_START, 504);
}

// Settings -> Bluetooth -> Paired -> "+ НОВЫЙ ТЕЛЕФОН" (2026-10-22, the user: pairing in VestaHome): the stack's
// search (the OBD's: the same list of devices in sight), a phone picked, paired from here with a PIN (BT_CMD_PAIR;
// a phone with secure pairing shows a code to confirm - the proxy confirms on our side), then connected as the shell
// connects one (BT_CMD_CONNECT). Without the proxy - nothing to do here.
static char s_pnAddr[18];   // the phone picked
static DWORD s_pnPairTick;  // our BT_CMD_PAIR (the proxy confirms its request itself)

// A phone pairing from its side (2026-10-23): the unit seen for 2 min ("ВИДИМОСТЬ", BT_CMD_VISIBLE); the phone's
// request (the proxy's BtPair: name, code) asked here - when we made the unit seen, or when no stock shell is there
// to ask it (its own window); the answer BT_CMD_PAIRCONFIRM. Its end (event 5) - in the status line.
enum { kVisibleMs = 120000 };
static DWORD   s_visUntil;      // the unit seen till then (0 not)
static BtPair* s_btPair;
static LONG    s_pairSeen = -1, s_pairDoneSeen = -1;
static BtPair  s_pairAsk;       // the request being asked

static BtPair* PairShm()
{
    if (!s_btPair) {
        HANDLE m = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(BtPair), BT_PAIR_NAME);
        if (m)
            s_btPair = (BtPair*)MapViewOfFile(m, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(BtPair));
    }
    return s_btPair && s_btPair->magic == BT_SHM_MAGIC ? s_btPair : NULL;
}

static void BtVisible(bool on)
{
    s_visUntil = on ? GetTickCount() + kVisibleMs : 0;
    if (!s_visUntil && on)
        s_visUntil = 1;
    BtCmdText(BT_CMD_VISIBLE, on ? L"1" : L"0");
    VLog(L"bt: видимость %s", on ? L"вкл (2 мин)" : L"выкл");
}

static void PairTick()   // once a second: the visibility's end, a phone's request, a pairing's end
{
    if (s_visUntil && (int)(GetTickCount() - s_visUntil) > 0)
        BtVisible(false);
    BtPair* p = PairShm();
    if (!p)
        return;
    if (s_pairSeen < 0) {   // the first look: what was there before is not ours
        s_pairSeen = p->seq;
        s_pairDoneSeen = p->doneSeq;
        return;
    }
    if (p->seq != s_pairSeen) {
        s_pairSeen = p->seq;
        s_pairAsk = *p;
        s_pairAsk.addr[17] = s_pairAsk.name[63] = s_pairAsk.code[15] = 0;
        bool ours = s_pnPairTick && GetTickCount() - s_pnPairTick < 60000 && !_stricmp(s_pairAsk.addr, s_pnAddr);
        bool shell = FindWindow(NULL, L"RadioCommWindow") != NULL;
        VLog(L"bt: запрос сопряжения %S «%S» код %S%s", s_pairAsk.addr, s_pairAsk.name, s_pairAsk.code,
             ours ? L" (наш, подтверждает proxy)" : !s_visUntil && shell ? L" (спросит оболочка)" : L"");
        if (!ours && (s_visUntil || !shell)) {
            Go(S_BT_PAIRASK, true);
            if (!IsWindowVisible(s_wnd) || GetForegroundWindow() != s_wnd) {
                ShowWindow(s_wnd, SW_SHOW);
                SetForegroundWindow(s_wnd);
            }
            Redraw();
        }
    }
    if (p->doneSeq != s_pairDoneSeen) {
        s_pairDoneSeen = p->doneSeq;
        _snwprintf(s_status, 96, p->result ? L"телефон сопряжён" : L"сопряжение не удалось");
        s_status[95] = 0;
        VLog(L"bt: сопряжение закончено, результат %d", p->result);
        BtCmd(BT_CMD_LISTPAIRED);
        if (IsWindowVisible(s_wnd))
            Redraw();
    }
}

// A phone's request: its name, address and the code it shows; ПРИНЯТЬ / ОТКЛОНИТЬ
static void ScrBtPairAsk(HDC dc)
{
    Frame(dc, L"Сопряжение Bluetooth");
    Panel(dc, 120, 90, 560, 280);
    wchar_t s[96];
    BtText(s_pairAsk.name, s, 96);
    T(dc, 400, 140, s[0] ? s : L"телефон", 26, kText, 700, AL_MID, 520);
    MultiByteToWideChar(CP_ACP, 0, s_pairAsk.addr, -1, s, 96);
    T(dc, 400, 170, s, 15, kMuted, 400, AL_MID);
    if (s_pairAsk.code[0]) {
        T(dc, 400, 210, L"просит сопряжение. Код на телефоне тот же?", 17, kText, 400, AL_MID, 520);
        MultiByteToWideChar(CP_ACP, 0, s_pairAsk.code, -1, s, 96);
        T(dc, 400, 262, s, 40, kOrange, 700, AL_MID);
    } else {
        T(dc, 400, 230, L"просит сопряжение", 17, kText, 400, AL_MID, 520);
    }
    Button(dc, 160, 296, 220, 56, L"Отклонить", B_N, IC_NONE, 18, 0, A_PAIRANSWER, 0);
    Button(dc, 420, 296, 220, 56, L"Принять", B_PRI, IC_NONE, 18, 0, A_PAIRANSWER, 1);
}

static void ScrBtPairNew(HDC dc)
{
    Settings(dc, S_BT_PAIRNEW);
    BtState bt;
    bool proxy = BtGet(&bt, NULL);
    bool searching = s_obdEvSeen >= 0 && GetTickCount() - s_obdSearchTick < 15000;
    SetRow(dc, 64, L"Новый телефон", searching ? L"ищу устройства (около 10 с)…"
                   : L"на телефоне — Bluetooth и видимость");
    Button(dc, 656, 70, 112, 44, searching ? L"Поиск…" : L"Найти", !proxy ? B_DIS : searching ? B_SEL : B_N, IC_NONE,
           15, 0, proxy ? A_OBDSCAN : A_NONE);
    wchar_t nm[48];
    for (int i = 0; i < s_obdFoundN; i++) {
        if (s_obdFound[i].name[0])
            BtText(s_obdFound[i].name, nm, 48);
        else
            MultiByteToWideChar(CP_ACP, 0, s_obdFound[i].addr, -1, nm, 48);
        Button(dc, 256 + (i % 2) * 260, 124 + (i / 2) * 46, 252, 40, nm,
               !strcmp(s_obdFound[i].addr, s_pnAddr) ? B_SEL : B_N, IC_NONE, 15, 0, A_PNPICK, i);
    }
    if (!proxy)
        T(dc, 264, 168, L"Установите proxy Bluetooth", 15, kMuted, 400, AL_START, 500);
    else if (!s_obdFoundN && !searching)
        T(dc, 264, 168, L"Устройства не найдены", 15, kMuted, 400, AL_START, 500);
    bool can = proxy && s_pnAddr[0];
    T(dc, 264, 296, L"PIN", 15, kMuted, 700);
    for (int i = 0; i < 2; i++) {
        wchar_t p[8];
        MultiByteToWideChar(CP_ACP, 0, kObdPins[i], -1, p, 8);
        Button(dc, 304 + i * 88, 270, 80, 40, p, i == s_obdPin ? B_SEL : B_N, IC_NONE, 15, 0, A_OBDPIN, i);
    }
    Button(dc, 488, 270, 136, 40, L"Сопрячь", can ? B_N : B_DIS, IC_NONE, 15, 0, can ? A_PNPAIR : A_NONE);
    Button(dc, 632, 270, 136, 40, L"Подключить", can ? B_N : B_DIS, IC_NONE, 15, 0, can ? A_PNCONN : A_NONE);
    T(dc, 264, 336, L"Телефон спросит подтверждение — согласитесь, потом «ПОДКЛЮЧИТЬ».", 14, kDim, 400, AL_START,
      504);
    // or from the phone's side: the unit seen for 2 min, the phone finds "it" and asks (ScrBtPairAsk)
    wchar_t vis[64];
    if (s_visUntil)
        _snwprintf(vis, 64, L"ГУ ВИДНА ЕЩЁ %d С", (int)(s_visUntil - GetTickCount()) / 1000 + 1);
    else
        wcscpy(vis, L"Сделать ГУ видимой");
    Button(dc, 256, 350, 260, 40, vis, !proxy ? B_DIS : s_visUntil ? B_SEL : B_N, IC_NONE, 14, 0,
           proxy ? A_BTVISIBLE : A_NONE);
    T(dc, 528, 376, L"найти ГУ с телефона", 13, kDim, 400, AL_START, 240);
}

// Settings -> Wi-Fi (wifi.cpp: Windows CE's WZC): the state, the networks in sight (strongest first, kWifiRows a
// page) with their signal and security; a tap connects (a new secured one asks its password on the keyboard),
// "Забыть" drops a known one. The known networks live in VestaHome.ini: WZC forgets them on every boot.
enum { kWifiRows = 3 };   // under them: the message and the access point's controls
static int     s_wifiPage;

static int WifiPages()   // (kPagers)
{
    WifiInfo w;
    WifiGet(&w);
    return w.count ? (w.count + kWifiRows - 1) / kWifiRows : 1;
}
static wchar_t s_wifiPass[64];     // typed on the keyboard
static wchar_t s_wifiTarget[33];   // the network it is for
static wchar_t s_wifiTitle[64];

static void WifiStateText(const WifiInfo& w, wchar_t* s, int n)
{
    switch (w.state) {
    case WIFI_NOWZC:      _snwprintf(s, n, L"%s", w.msg[0] ? w.msg : L"нет службы Wi-Fi"); break;
    case WIFI_NOADAPTER:  _snwprintf(s, n, L"адаптер Wi-Fi не найден"); break;
    case WIFI_OFF:        _snwprintf(s, n, L"выключено"); break;
    case WIFI_AP:         _snwprintf(s, n, L"точка ГУ «%s» · телефонов: %d", w.apSsid, w.apClients); break;
    case WIFI_CONNECTING: _snwprintf(s, n, L"подключение к %s…", w.ssid); break;
    case WIFI_CONNECTED:
        _snwprintf(s, n, L"%s · %S", w.ssid, w.ip[0] ? w.ip : "получение адреса…");
        break;
    default:              _snwprintf(s, n, L"не подключено"); break;
    }
    s[n - 1] = 0;
}

static int WifiSavedCount();   // (vh_widgetset.inc: the saved networks' screen)

static void ScrSetWifi(HDC dc)
{
    static const wchar_t* kSec[4] = { L"открытая", L"WEP", L"WPA", L"WPA2" };
    WifiInfo w;
    WifiGet(&w);
    Settings(dc, S_SET_WIFI);
    wchar_t s[120];
    WifiStateText(w, s, 120);
    SetRow(dc, 64, L"Wi-Fi", s);
    Toggle(dc, 696, 74, T_WIFI);
    if (w.state >= WIFI_DISCONNECTED)
        Button(dc, 556, 70, 124, 44, w.scanning ? L"Поиск…" : L"Найти", w.scanning ? B_SEL : B_N, IC_NONE, 15, 0,
               A_WIFISCAN);
    if (w.state == WIFI_AP) {   // the unit is the access point: what to type on the phone
        Panel(dc, 256, 124, 512, 168, 12);
        T(dc, 276, 160, w.apAuto ? L"Точка доступа (фикс Xiaomi)" : L"Точка доступа", 14, kMuted, 700);
        _snwprintf(s, 120, L"Сеть: %s", w.apSsid);
        T(dc, 276, 198, s, 22, kText, 700, AL_START, 470);
        _snwprintf(s, 120, L"Пароль: %s", w.apPass);
        T(dc, 276, 232, s, 22, kText, 700, AL_START, 470);
        _snwprintf(s, 120, L"Подключено телефонов: %d (2,4 ГГц, канал 6)", w.apClients < 0 ? 0 : w.apClients);
        T(dc, 276, 270, s, 15, w.apClients > 0 ? kOrange : kMuted, 600, AL_START, 470);
    }
    int pages = (w.count + kWifiRows - 1) / kWifiRows;
    if (s_wifiPage >= pages)
        s_wifiPage = pages ? pages - 1 : 0;
    for (int r = 0; r < kWifiRows; r++) {
        int i = s_wifiPage * kWifiRows + r;
        if (i >= w.count)
            break;
        const WifiNet& n = w.nets[i];
        int y = 124 + r * 56;
        bool cur = (w.state == WIFI_CONNECTED || w.state == WIFI_CONNECTING) && !wcscmp(n.ssid, w.ssid);
        bool down = Zone(256, y, 512, 52, A_WIFINET, i);
        bool hl = down || cur;
        R(dc, 256, y, 512, 52, 10, hl ? kOrange : kPanel2);
        int bars = WifiBars(n.rssi);
        Ico(dc, IC_WIFI, 268, y + 10, 32, hl ? kInk : bars >= 3 ? kText : bars >= 2 ? kMuted : kDim);
        T(dc, 312, y + 23, n.ssid, 18, hl ? kInk : kText, 700, AL_START, 280);
        wchar_t ch[16] = L"";   // the channel (the unit's radio: 2.4 GHz only, channels 1..14)
        if (n.ch)
            _snwprintf(ch, 16, L" · кан. %d", n.ch);
        if (cur && w.state == WIFI_CONNECTED)
            _snwprintf(s, 120, L"подключено · %d дБм%s", n.rssi, ch);
        else if (cur)
            _snwprintf(s, 120, L"подключение…%s", ch);
        else
            _snwprintf(s, 120, L"%s%s · %d дБм%s", n.known ? L"сохр. · " : L"", kSec[n.sec & 3], n.rssi, ch);
        T(dc, 312, y + 44, s, 14, hl ? kInk : kMuted, 400, AL_START, 280);
        if (n.sec != WSEC_OPEN)
            Ico(dc, IC_LOCK, 724, y + 12, 28, hl ? kInk : kDim);
        if (n.known)   // drawn last: its button wins over the row's
            Button(dc, 604, y + 9, 108, 34, L"Забыть", hl ? B_FILL : B_N, IC_NONE, 13, 0, A_WIFIFORGET, i);
    }
    if (!w.count && w.state >= WIFI_DISCONNECTED)
        T(dc, 272, 160, w.scanning ? L"Ищу сети…" : L"Сетей не видно — нажмите «НАЙТИ»", 16, kMuted, 400);
    if (w.msg[0] && w.state != WIFI_NOWZC)
        T(dc, 256, 312, w.msg, 14, kDim, 400, AL_START, 370);
    // the unit's own access point: by hand, or automatically when no known network is in sight
    if (w.state >= WIFI_AP) {
        bool apOn = w.state == WIFI_AP && !w.apAuto;
        Button(dc, 256, 352, 196, 40, apOn ? L"Точка доступа: вкл" : L"Точка доступа", apOn ? B_SEL : B_N, IC_NONE,
               14, 0, A_WIFIAP);
        T(dc, 466, 378, L"Фикс Xiaomi", 14, kMuted, 600);
        Toggle(dc, 572, 354, T_WIFI_AP_AUTO);
    }
    if (WifiSavedCount()) {   // all the remembered ones, in sight or not: to see and to forget (the user 2026-10-26)
        _snwprintf(s, 120, L"Сохранённые (%d)", WifiSavedCount());
        Button(dc, 652, 352, 116, 40, s, B_N, IC_NONE, 13, 0, A_SCREEN, S_WIFI_SAVED);
    }
    if (pages > 1) {
        _snwprintf(s, 16, L"%d/%d", s_wifiPage + 1, pages);
        Button(dc, 644, 298, 56, 40, NULL, s_wifiPage ? B_N : B_DIS, IC_BACK, 18, 26, A_PAGESTEP, -1);
        T(dc, 638, 324, s, 14, kMuted, 600, AL_END);
        Button(dc, 708, 298, 56, 40, NULL, s_wifiPage + 1 < pages ? B_N : B_DIS, IC_NEXT, 18, 26, A_PAGESTEP, 1);
    }
}

// The player's picture: kept in proportion (black bands) or stretched over the window; the picture while the
// car moves (off: only the sound, the rules of the road). Both kept in the ini.
static void ScrSetVideo(HDC dc)
{
    static const wchar_t* scale[2] = { L"Вписать", L"Растянуть" };
    Settings(dc, S_SET_VIDEO);
    SetRow(dc, 64, L"Масштаб");
    Seg(dc, 512, 70, 248, 44, scale, 2, G_VIDEO_SCALE, 15);
    SetRow(dc, 120, L"Видео в движении", L"выключено: в движении видео на паузе", true);
    Toggle(dc, 688, 130, T_VIDEO_MOVING);
}

static void ScrSetPhoto(HDC dc);   // (with the photos, after the settings' data)

static void CanStatusText(wchar_t* s, int n);
static void Size(wchar_t* s, int n, unsigned __int64 bytes);

// "About the system", the last section of the settings: what the unit can tell without the MCU. The stock
// "Info" (SW 27.0.0.4120, MCU TR8005-4120, CAN 1.19, OS 27.0.0.4108, ID) gets its versions from the MCU; here the
// shell's version is known by the size of G3NavHMI.exe (4120: 4 077 056 B). Diagnostics open from here, as the
// stock engineering menu opens from "Info".
static void ScrInfo(HDC dc)
{
    wchar_t shell[48], os[48], can[64], sd[48];
    WIN32_FIND_DATA fd;
    HANDLE f = FindFirstFile(L"\\ResidentFlash\\navigation\\app\\G3NavHMI.exe", &fd);
    if (f == INVALID_HANDLE_VALUE)
        wcscpy(shell, L"нет G3NavHMI.exe");
    else {
        FindClose(f);
        if (fd.nFileSizeLow == 4077056)   // the stock shell's build = the firmware's (the proxies need 4120)
            wcscpy(shell, L"4120 (27.0.0.4120)");
        else
            _snwprintf(shell, 48, L"неизвестна (%u Б)", (unsigned)fd.nFileSizeLow);
    }
    OSVERSIONINFO vi;
    memset(&vi, 0, sizeof(vi));
    vi.dwOSVersionInfoSize = sizeof(vi);
#ifdef UNDER_CE
    const wchar_t* osName = L"Windows CE";
#else
    const wchar_t* osName = L"Windows (ПК)";
#endif
    if (GetVersionEx(&vi))
        _snwprintf(os, 48, L"%s %u.%u · сборка %u", osName, (unsigned)vi.dwMajorVersion, (unsigned)vi.dwMinorVersion,
                   (unsigned)vi.dwBuildNumber);
    else
        wcscpy(os, L"?");
    CanStatusText(can, 64);
    const wchar_t* rows[5][2] = { { L"Модель", L"LADA Vesta · MMC" }, { L"Прошивка ГУ", shell }, { L"ОС", os },
                                  { L"CAN-модуль", can }, { L"VestaHome", VhVersion() } };
    Settings(dc, S_INFO);
    for (int i = 0; i < 5; i++) {
        int y = 64 + i * 44;
        T(dc, 264, y + 29, rows[i][0], 17, kMuted, 400);
        T(dc, 760, y + 29, rows[i][1], 17, kText, 700, AL_END, 360);
        Sep(dc, 256, y + 42, 512);
    }
#ifdef UNDER_CE
    const wchar_t* vol = L"\\SDMemory";
#else
    const wchar_t* vol = s_dir;
#endif
    ULARGE_INTEGER avail, size, freeAll;
    int pct = -1;
    if (GetDiskFreeSpaceEx(vol, &avail, &size, &freeAll) && size.QuadPart) {
        wchar_t used[24], total[24];
        Size(used, 24, size.QuadPart - freeAll.QuadPart);
        Size(total, 24, size.QuadPart);
        _snwprintf(sd, 48, L"%s из %s", used, total);
        pct = (int)((size.QuadPart - freeAll.QuadPart) * 100 / size.QuadPart);
    } else {
        wcscpy(sd, L"нет");
    }
    T(dc, 264, 304, L"SD-карта", 17, kMuted, 400);
    if (pct >= 0) {
        R(dc, 372, 294, 200, 10, 5, kLine);
        R(dc, 372, 294, pct * 2 > 10 ? pct * 2 : 10, 10, 5, pct > 90 ? kStop : kMuted);
    }
    T(dc, 760, 304, sd, 17, kText, 700, AL_END, 180);
    // what the stock "Info" and its engineering screens show; diagnostics; closing / minimizing VestaHome
    Button(dc, 256, 324, 200, 64, L"Подробнее", B_N, IC_INFO, 15, 26, A_SCREEN, S_INFO_MORE);
    Button(dc, 464, 324, 216, 64, L"Диагностика", B_N, IC_SEARCH, 15, 26, A_SCREEN, S_DIAG);
    Button(dc, 688, 324, 80, 64, NULL, B_N, IC_POWER, 18, 36, A_SCREEN, S_EXIT);
}
