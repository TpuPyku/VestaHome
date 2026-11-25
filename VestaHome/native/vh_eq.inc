// vh_eq.inc - part of home.cpp (included there, in order: after vh_plugins.inc): the equalizer's two kinds (the user
// 2026-11-25): "Грубый" - the stock one, 3 bands in the MCU, for every source (radio and Bluetooth too); "Точный" -
// ours, 10 bands in 0.5 dB steps (VestaCore veq), on the sound the processor plays: the music, the internet radio
// (a BASS DSP on their channels) and the add-ons that play themselves (VestaAuto: "eq" of plugin-api.md). With the fine
// one on, the MCU's bands go to 0 (kept in eqCoarse, put back when the coarse one is chosen): never both at once.

static Eq s_eqChan[2];   // the music player's channel, the internet radio's

static void EqFineGains(int* g)   // what ours does now: the gains, or flat when it is off
{
    for (int i = 0; i < EQ_BANDS; i++)
        g[i] = s_cfg.eqMode ? s_cfg.eqFine[i] : 0;
}

static void CALLBACK EqDsp(DWORD, DWORD, void* buf, DWORD len, void* user)   // BASS's thread: 16-bit frames
{
    Eq* e = (Eq*)user;
    EqRun(e, (short*)buf, (int)(len / 2 / e->chans));
}

// A new channel of the player (which 0) or the radio (1): ours filters it (always attached - off is flat, untouched).
static void EqAttach(DWORD chan, int which)
{
    typedef DWORD (WINAPI *DspFn)(DWORD, void (CALLBACK*)(DWORD, DWORD, void*, DWORD, void*), void*, int);
    struct Info { DWORD freq, chans, flags, ctype, origres; void* plugin; DWORD sample; const char* filename; } info;
    typedef BOOL (WINAPI *InfoFn)(DWORD, Info*);
    static DspFn s_dsp;
    static InfoFn s_info;
    if (!s_dsp) {
        HMODULE lib = GetModuleHandle(L"bass.dll");
        s_dsp = lib ? (DspFn)BASS_PROC("BASS_ChannelSetDSP") : NULL;
        s_info = lib ? (InfoFn)BASS_PROC("BASS_ChannelGetInfo") : NULL;
    }
    if (!s_dsp || !s_info || !s_info(chan, &info) || (info.flags & 0x101) || info.chans < 1 || info.chans > 2) {
        VLog(L"эквалайзер: канал не подходит (16 бит, 1-2 канала)");   // (8-bit 0x1, float 0x100: not ours)
        return;
    }
    int g[EQ_BANDS];
    EqFineGains(g);
    EqInit(&s_eqChan[which], g, (int)info.freq, (int)info.chans);
    s_dsp(chan, EqDsp, &s_eqChan[which], 0);
}

static void PlugEqTell();

static void EqFineApply()   // the gains changed or the kind: the channels, the add-ons
{
    int g[EQ_BANDS];
    EqFineGains(g);
    for (int i = 0; i < 2; i++)
        if (s_eqChan[i].rate)
            EqChange(&s_eqChan[i], g);
    PlugEqTell();
}

// The kind: fine - the MCU's bands to 0 (the coarse ones kept); coarse - put back.
static void EqModeSet(int mode)
{
    if (mode == s_cfg.eqMode)
        return;
    s_cfg.eqMode = mode;
    for (int i = 0; i < 3; i++) {
        if (mode)
            s_cfg.eqCoarse[i] = s_cfg.eq[i];
        int v = mode ? 0 : s_cfg.eqCoarse[i];
        if (s_cfg.eq[i] != v) {
            s_cfg.eq[i] = v;
            EqSend(i);
        }
    }
    SaveCfg();
    EqFineApply();
    VLog(L"эквалайзер: %s", mode ? L"точный (VestaHome)" : L"грубый (штатный)");
}

// The fine sliders: x 268 + band * 50, from y kFineY (+12 dB) down kFineH (-12 dB); a tap or a drag, 0.5 dB a step.
enum { kFineX = 290, kFineDX = 52, kFineY = 172, kFineH = 158 };

static void EqFineAt(int band, int y, bool done)
{
    int v = (kFineY + kFineH / 2 - y) * 2 * EQ_MAX / kFineH;   // tenths of a dB
    v = (v >= 0 ? v + 2 : v - 2) / 5 * 5;                       // 0.5 dB steps
    v = v < -EQ_MAX ? -EQ_MAX : v > EQ_MAX ? EQ_MAX : v;
    if (band >= 0 && band < EQ_BANDS && s_cfg.eqFine[band] != v) {
        s_cfg.eqFine[band] = v;
        EqFineApply();
        RedrawArea(256, 120, 784, 400);
    }
    if (done)
        SaveCfg();
}

struct EqFinePreset { const wchar_t* name; int g[EQ_BANDS]; };
static const EqFinePreset kEqFinePresets[5] = {
    { L"Ровно", { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { L"Бас", { 60, 50, 35, 15, 0, 0, 0, 0, 0, 0 } },
    { L"Голос", { -30, -20, -10, 0, 15, 30, 30, 15, 0, -10 } },
    { L"Ярче", { 0, 0, 0, 0, 0, 0, 10, 25, 40, 45 } },
    { L"Тише ночью", { 30, 20, 10, 0, -10, -10, 0, 10, 20, 20 } } };

static void EqFinePreset(int p)
{
    memcpy(s_cfg.eqFine, kEqFinePresets[p].g, sizeof(s_cfg.eqFine));
    EqFineApply();
    SaveCfg();
}

static void EqFineDraw(HDC dc)
{
    static const wchar_t* kHz[EQ_BANDS] = { L"31", L"62", L"125", L"250", L"500", L"1к", L"2к", L"4к", L"8к", L"16к" };
    for (int p = 0; p < 5; p++) {
        bool cur = !memcmp(s_cfg.eqFine, kEqFinePresets[p].g, sizeof(s_cfg.eqFine));
        Button(dc, 256 + p * 104, 112, 98, 38, kEqFinePresets[p].name, cur ? B_SEL : B_N, IC_NONE, 13, 0,
               A_EQFPRESET, p);
    }
    for (int i = 0; i < EQ_BANDS; i++) {
        int x = kFineX + i * kFineDX;
        wchar_t v[12];
        int g = s_cfg.eqFine[i];
        if (g % 10)
            _snwprintf(v, 12, L"%+d,%d", g / 10, abs(g % 10));
        else
            _snwprintf(v, 12, g ? L"%+d" : L"0", g / 10);
        if (g < 0 && g > -10)
            wcscpy(v, L"-0,5");
        VSlider(dc, x, kFineY, kFineH, (g + EQ_MAX) * 100 / (2 * EQ_MAX), kHz[i], v);
        Zone(x - kFineDX / 2, kFineY - 14, kFineDX, kFineH + 28, A_EQFINE, i);   // (a tap or a drag: EqFineAt)
    }
    T(dc, 264, 392, L"Для музыки, интернет-радио и Android Auto; радио и Bluetooth — без эквалайзера", 12, kDim, 400,
      AL_START, 512);
}

// The add-ons that play sound themselves (VestaAuto) filter it with the same gains: "eq\tmode=<0|1>\tg=<10 gains>"
// (plugin-api.md), at their hello and at each change.
static void PlugEqTell()
{
    char g[120];
    int f[EQ_BANDS];
    EqFineGains(f);
    EqGainsText(f, g, sizeof(g));
    for (int i = 0; i < s_plugN; i++)
        if (s_plug[i].hwnd)
            PlugSend(s_plug[i], L"eq\tmode=%d\tg=%S", s_cfg.eqMode, g);
}
