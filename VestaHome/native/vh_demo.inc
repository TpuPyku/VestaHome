// vh_demo.inc - part of home.cpp (included there, in order: after vh_mcu.inc): the PC build's demo head unit (the
// user 2026-11-22: "the fake data into the PC build, otherwise I cannot test some functions myself"). PC only
// (#ifndef UNDER_CE): the ARM build has none of it. What the unit's proxies give VestaHome, made here when nobody else
// gives it (a mapping already written - make_shots.py's stand-ins, a proxy - is left alone):
//  - the MCU's ring VestaMCU2 (vh_mcu.inc): the climate, the sound, the versions, the tuner with RDS, its presets and
//    station list; VestaHome's MCU calls (McuSend*) answered as the MCU answers them (DemoMcuCall);
//  - Bluetooth VestaBT5 / VestaBTPB1 (btshm.h): a phone connected, its music, its book and calls; the commands carried
//    out by a thread, as the proxy's; the PC's key C - an incoming call (again - its end, DemoRing);
//  - GPS: GpsSim's Moscow drive (route.inc) unless gps.port plays a .nmea recording (DemoGpsSource);
//  - OSMapsCE's favourites: favorites.pc.txt beside VestaHome, seeded once (DemoFavsPath; OSMapsCE's own file is
//    never read or written by the PC build).
#ifndef UNDER_CE
#include "../../GpsSim/native/simnmea.h"
#include "../../GpsSim/native/route.inc"   // kRoute: GpsSim's drive, 27 km of Moscow

// A named mapping that nobody has written yet (its first DWORD, the magic, still 0); NULL - someone else's.
static void* DmMap(const wchar_t* name, DWORD size)
{
    HANDLE m = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, size, name);
    void* v = m ? MapViewOfFile(m, FILE_MAP_ALL_ACCESS, 0, 0, size) : NULL;
    return v && !*(DWORD*)v ? v : NULL;   // (the handle stays open: the mapping lives while VestaHome does)
}

static void DmU8(const wchar_t* w, char* out, int cap)   // the stack's texts are UTF-8 (a narrow literal here: cp1251)
{
    WideCharToMultiByte(CP_UTF8, 0, w, -1, out, cap, NULL, NULL);
    out[cap - 1] = 0;
}

// ---- the MCU (the window's thread only, but the ring itself: as the proxy writes it, any number of writers)
static McuShm* s_dmMcu;   // NULL: the demo is not the MCU

static void DmPost(unsigned short id, const unsigned char* d, int len)
{
    if (!s_dmMcu)
        return;
    LONG n = InterlockedIncrement((LONG*)&s_dmMcu->seq);
    McuMsg* m = &s_dmMcu->msg[(n - 1) & (MCU_SHM_SLOTS - 1)];
    m->seq = 0;
    m->tick = GetTickCount();
    m->id = id;
    m->len = (unsigned char)len;
    m->skel = 0;
    memset(m->data, 0, MCU_SHM_DATA);
    memcpy(m->data, d, len);
    m->seq = n;
}

static void DmPost4(unsigned short id, int len, int a, int b, int c = 0, int d = 0)
{
    unsigned char p[4] = { (unsigned char)a, (unsigned char)b, (unsigned char)c, (unsigned char)d };
    DmPost(id, p, len);
}

static void DmPad(unsigned char* out, const char* s, int n)   // ASCII, spaces to n
{
    int k = (int)strlen(s);
    memset(out, ' ', n);
    memcpy(out, s, k < n ? k : n);
}

struct DmMsg { unsigned short id; unsigned char len, d[8]; };

static void DmPostAll(const DmMsg* t, int n)
{
    for (int i = 0; i < n; i++)
        DmPost(t[i].id, t[i].d, t[i].len);
}

// As the unit sends them at its start (vh_mcu.inc McuTake's formats): the climate (-3 °C outside, the knob 9/16, the
// blower 3, A/C, auto, the rear window), the volume 14, the radio as the source, the equalizer and the balance, the
// System status (the board, the amplifier, the screen, the tuner's field, the wheel buttons' voltages).
static const DmMsg kDmBoot[] = {
    { 0x6109, 4, { 0, 1, 0, 40 - 3 } }, { 0x6109, 4, { 0, 3, 0, 9 } }, { 0x610C, 2, { 0, 3 } },
    { 0x610D, 2, { 0, 2 } }, { 0x610B, 2, { 0, 1 } }, { 0x610A, 2, { 0, 2 } }, { 0x6107, 2, { 0, 1 } },
    { 0x610F, 2, { 0, 0 } }, { 0x6106, 2, { 0, 1 } },
    { 0x3101, 3, { 0, 0, 14 } }, { 0x3109, 2, { 0, 0 } }, { 0x4904, 2, { 0, 0 } },
    { 0x3102, 2, { 0, 6 + 2 } }, { 0x3103, 2, { 0, 6 } }, { 0x3104, 2, { 0, 6 + 1 } }, { 0x3105, 3, { 0, 8, 8 } },
    { 0x7905, 5, { 0, 0x0A, 41 } }, { 0x7905, 5, { 0, 0x06, 47 } }, { 0x7905, 5, { 0, 0x0D, 39 } },
    { 0x7905, 5, { 0, 0x07, 28 } }, { 0x7905, 4, { 0, 0x03, 0x0C, 0xE4 } }, { 0x7905, 4, { 0, 0x0E, 0x0C, 0xDA } } };
// The radio's settings (the answers to the extended key 0x90): RDS on, AF on, CT on.
static const DmMsg kDmRadioSets[] = {
    { 0x0920, 3, { 0, 1, 0 } }, { 0x0921, 3, { 0, 0, 1 } }, { 0x0924, 3, { 0, 0, 1 } }, { 0x092D, 3, { 0, 0, 0 } },
    { 0x092E, 3, { 0, 0, 0 } } };
// The unit's details ("О системе", after the product info's request): its BT address, the machine ID.
static const DmMsg kDmInfo[] = {
    { 0x7922, 8, { 0, 6, 0x00, 0x1D, 0xDE, 0x00, 0x00, 0x01 } }, { 0x7902, 5, { 0, 0, 1, 0x4B, 0x2D } },
    { 0x4118, 5, { 0, 0x20, 0x26, 0xDE, 0x01 } }, { 0x4111, 5, { 0, 0x20, 0x26, 0x11, 0x22 } } };

static void DmPostText(unsigned short id, const unsigned char* head, int hn, const char* text)
{
    unsigned char d[MCU_SHM_DATA];
    int n = (int)strlen(text);
    n = hn + n < MCU_SHM_DATA ? n : MCU_SHM_DATA - hn;
    memcpy(d, head, hn);
    memcpy(d + hn, text, n);
    DmPost(id, d, hn + n);
}

static void DmInfo()
{
    static const unsigned char sw[5] = { 0, 0, 0, 0, 16 }, hw[5] = { 0, 3, 0, 0, 6 }, num[1] = { 0 };
    DmPostText(0x7902, sw, 5, "VESTA.4120.DEMO.");
    DmPostText(0x7902, hw, 5, "HW 1.0");
    DmPostText(0x4112, num, 1, "DEMO0000001");
    DmPostAll(kDmInfo, sizeof(kDmInfo) / sizeof(kDmInfo[0]));
}

// The tuner: Moscow's stations (radiodb.cpp knows them by the city - their logos), FM only.
struct DmStation { int f10; const char* ps; const char* rt; };
static const DmStation kDmFm[] = {
    { 8750, "BUSINESS", "Business FM: the news every half an hour" },
    { 8830, "RETRO FM", "Retro FM: the best of the 70s, 80s and 90s" },
    { 8910, "JAZZ", "Radio Jazz: Ella Fitzgerald - Summertime" },
    { 9030, "AVTORAD.", "Avtoradio: the traffic on the MKAD now" },
    { 9520, "ROCK FM", "Rock FM: Queen - Don't Stop Me Now" },
    { 10120, "DFM", "DFM: dance hits non-stop" },
    { 10170, "NASHE", "Nashe Radio: Kino - Gruppa krovi" },
    { 10340, "MAYAK", "Radio Mayak: the news at the top of the hour" },
    { 10370, "MAXIMUM", "Maximum: Linkin Park - Numb" },
    { 10620, "EUROPA+", "Europa Plus: the Top 40 of the week" } };
enum { kDmFmN = sizeof(kDmFm) / sizeof(kDmFm[0]), kDmFmPad = (kDmFmN + 7) / 8 * 8 };
static const DmStation kDmNone = { 0, "", "" };   // (a frequency not in the table: no RDS)
static const int kDmPresets[10] = { 8750, 8830, 8910, 9030, 9520, 10120, 10170, 10620, 10340, 10370 };
static int s_dmF10 = 10120;     // the tuner here is only ever on one of kDmFm
static int s_dmFmF[kDmFmPad];   // kDmFm's frequencies, 0 after them to a whole message of 8
static int s_dmPreset[24];

static int DmFind(const int* a, int n, int v)   // its index, -1 none
{
    for (int i = 0; i < n; i++)
        if (a[i] == v)
            return i;
    return -1;
}

static void DmPostFreqs(int list, const int* f, int n)   // 0x0908: the presets (list 1) or the stations (0), 8 a message
{                                                         // (f: whole messages of 8, n how many count)
    for (int from = 0; from < n; from += 8) {
        unsigned char d[21];
        memset(d, 0, sizeof(d));
        d[1] = (unsigned char)list;
        d[2] = (unsigned char)from;
        d[3] = (unsigned char)(from + 7);
        d[4] = (unsigned char)n;
        for (int k = 0; k < 8; k++) {
            d[5 + 2 * k] = (unsigned char)(f[from + k] >> 8);
            d[6 + 2 * k] = (unsigned char)f[from + k];
        }
        DmPost(0x0908, d, 21);
    }
}

static void DmPostNames(int from)   // 0x0930: 4 names of 8 characters from 'from' (past the last: blank)
{
    unsigned char d[36];
    memset(d, ' ', sizeof(d));
    d[0] = 0;
    d[1] = (unsigned char)from;
    d[2] = (unsigned char)(from + 3);
    d[3] = (unsigned char)kDmFmN;
    for (int k = 0; k < 4 && from + k < kDmFmN; k++)
        DmPad(d + 4 + k * 8, kDmFm[from + k].ps, 8);
    DmPost(0x0930, d, 36);
}

static void DmList()   // the list key 0x3C: the stations, then their names
{
    DmPostFreqs(0, s_dmFmF, kDmFmN);
    for (int from = 0; from < kDmFmN; from += 4)
        DmPostNames(from);
}

static void DmRds(const DmStation& st)   // the station's RDS: its name (0x090B), its RadioText in two parts of 32 (0x0931)
{
    unsigned char d[36], rt[64];
    DmPad(rt, st.rt, 64);
    memset(d, 0, sizeof(d));
    DmPad(d + 3, st.ps, 8);
    DmPost(0x090B, d, 11);
    for (int part = 0; part < 2; part++) {
        d[3] = (unsigned char)part;
        memcpy(d + 4, rt + part * 32, 32);
        DmPost(0x0931, d, 36);
    }
}

static void DmTune(int f10)   // the frequency (FM), its RDS, the preset's place on its page (0xFF none)
{
    s_dmF10 = f10;
    DmPost4(0x0901, 4, 0, 0, f10 >> 8, f10 & 0xFF);
    int i = DmFind(s_dmFmF, kDmFmN, f10);
    DmRds(i < 0 ? kDmNone : kDmFm[i]);
    int p = DmFind(s_dmPreset, 24, f10);
    DmPost4(0x0907, 3, 0, 1, p < 0 ? 0xFF : p % 8);
}

static void DmSeek(int dir)   // the next station up / down the band (round)
{
    int i = DmFind(s_dmFmF, kDmFmN, s_dmF10);
    DmTune(s_dmFmF[(i + dir + kDmFmN) % kDmFmN]);
}

static void DmPresetSave(int i)   // the station playing into preset i: 0x0909 00 01 <preset> <frequency BE>
{
    s_dmPreset[i] = s_dmF10;
    unsigned char d[5] = { 0, 1, (unsigned char)i, (unsigned char)(s_dmF10 >> 8), (unsigned char)s_dmF10 };
    DmPost(0x0909, d, 5);
    DmPost4(0x0907, 3, 0, 1, i % 8);
}

// The keys (as the MCU answers them, vh_mcu.inc / vh_sources.inc / vh_sound.inc): "back" - its main menu; settings,
// the equalizer's and the balance's pages; the sources (radio 0x7F, Bluetooth music 0x54); the radio's list, band
// (FM only here: the same station again), seek (extended 0x3B / 0x3C), its settings (extended 0x90); the details 0x9B.
static void DmMenu() { DmPost4(0x1102, 2, 0, 0x16); }
static void DmSettings() { DmPost4(0x3903, 2, 0, 0); }
static void DmEqPage() { DmPost4(0x4102, 2, 0, 0x10); }
static void DmBalPage() { DmPost4(0x4102, 2, 0, 0x0C); }
static void DmRetune() { DmTune(s_dmF10); }
static void DmSrcRadio() { DmPost4(0x4904, 2, 0, 0); DmRetune(); }
static void DmSrcBt() { DmPost4(0x4904, 2, 0, 9); }
static void DmSeekUp() { DmSeek(1); }
static void DmSeekDown() { DmSeek(-1); }
static void DmRadioSets() { DmPostAll(kDmRadioSets, sizeof(kDmRadioSets) / sizeof(kDmRadioSets[0])); }

struct DmKey { int code; bool ext; void (*fn)(); };
static const DmKey kDmKeys[] = {
    { 0x30, false, DmMenu }, { 0x13, false, DmSettings }, { 0xE6, false, DmEqPage }, { 0xE4, false, DmBalPage },
    { 0x7F, false, DmSrcRadio }, { 0x54, false, DmSrcBt }, { 0x3C, false, DmList }, { 0x3B, false, DmRetune },
    { 0x3A, false, DmRetune }, { 0x9B, false, DmInfo }, { 0x3B, true, DmSeekDown }, { 0x3C, true, DmSeekUp },
    { 0x90, true, DmRadioSets } };

static void DmKeyRun(int code, bool ext)
{
    for (int i = 0; i < (int)(sizeof(kDmKeys) / sizeof(kDmKeys[0])); i++)
        if (kDmKeys[i].code == code && kDmKeys[i].ext == ext) {
            kDmKeys[i].fn();
            return;
        }
}

// The calls (McuSend*'s name and arguments): a key acts when pressed (b 0; its release is (code, 1, 1)).
static void DmKeyCall(int a, int b, int) { if (!b) DmKeyRun(a, false); }
static void DmExtKeyCall(int a, int b, int) { if (!b) DmKeyRun(a, true); }
static void DmEq(int band, int v, int) { DmPost4((unsigned short)(0x3104 - band), 2, 0, v); }   // band 0 = high
static void DmBalance(int a, int b, int) { DmPost4(0x3105, 3, 0, a, b); }
static void DmVolume(int type, int v, int) { if (!type) DmPost4(0x3101, 3, 0, 0, v); }
static void DmMediaSrc(int, int, int) { DmPost4(0x4904, 2, 0, 6); }   // (5, 3): the media player
static void DmInfoCall(int, int, int) { DmInfo(); }

static void DmListTune(int row)
{
    if ((unsigned)row < (unsigned)kDmFmN)
        DmTune(s_dmFmF[row]);
}

static void DmPresetTune(int i)   // (an empty one: nothing)
{
    if (s_dmPreset[i])
        DmTune(s_dmPreset[i]);
}

static void DmPresetItem(int i, int save)   // tuned, or the station playing saved there
{
    if ((unsigned)i < 24)
        save ? DmPresetSave(i) : DmPresetTune(i);
}

static void DmListItem(int a, int b, int c)   // (0, row) the list's station; (1, n) preset n, (1, n, 1) saved there
{
    a ? DmPresetItem(b, c == 1) : DmListTune(b);
}

struct DmCallDef { const char* name; void (*fn)(int a, int b, int c); };
static const DmCallDef kDmCalls[] = {
    { "SPApi_SendKeyEvent", DmKeyCall }, { "SPApi_SendExtendKeyEvent", DmExtKeyCall },
    { "SPApi_SendListItemControl", DmListItem }, { "SPApi_SendRadioAudioHMLData", DmEq },
    { "SPApi_SendRadioAudioBalanceFaderData", DmBalance }, { "SPApi_SendRadioAudioVolData", DmVolume },
    { "SPApi_SendMediaSourceSelect", DmMediaSrc }, { "SPApi_SendRadioRequestProductInfo", DmInfoCall },
    { "SPApi_SendRadioRequestBTAddress", DmInfoCall } };

// McuSend*'s hook: false - the demo is not the MCU (the call goes on as on the unit); true - taken (the other calls,
// the clock's or the navigator's voice, are taken and do nothing).
static bool DemoMcuCall(const char* name, int a, int b, int c)
{
    if (!s_dmMcu)
        return false;
    for (int i = 0; i < (int)(sizeof(kDmCalls) / sizeof(kDmCalls[0])); i++)
        if (!strcmp(kDmCalls[i].name, name)) {
            kDmCalls[i].fn(a, b, c);
            break;
        }
    return true;
}

static void DmTunerStart()   // the presets, the station list, the station playing
{
    memcpy(s_dmPreset, kDmPresets, sizeof(kDmPresets));
    for (int i = 0; i < kDmFmN; i++)
        s_dmFmF[i] = kDmFm[i].f10;
    DmPostFreqs(1, s_dmPreset, 24);
    DmList();
    DmTune(s_dmF10);
}

static void DmMcuStart()
{
    s_dmMcu = (McuShm*)DmMap(MCU_SHM_NAME, sizeof(McuShm));
    if (!s_dmMcu)
        return;
    s_dmMcu->version = 2;
    s_dmMcu->attach = 1;
    s_dmMcu->magic = MCU_SHM_MAGIC;
    DmPostAll(kDmBoot, sizeof(kDmBoot) / sizeof(kDmBoot[0]));
    DmRadioSets();
    DmInfo();
    DmTunerStart();
}

// ---- Bluetooth (the demo's thread writes the state: one writer, under the state's sequence lock)
static BtShm* s_dmBt;
static BtPb*  s_dmPb;
static volatile LONG s_dmRing;   // key C: a call to come, or the call going on to end (the thread takes it)
static int    s_dmOpts = 1;      // GETOPTS: bit 0 connect at power-on, bit 1 auto answer

struct DmPhone { const char* addr; const wchar_t* name; };
static const DmPhone kDmPaired[] = { { "D0:E0:00:00:00:01", L"Demo Phone" }, { "D0:E0:00:00:00:02", L"Pixel Маши" },
                                     { "D0:E0:00:00:00:03", L"iPhone Сергея" } };
struct DmContact { const wchar_t* name; const char* num[2]; };
static const DmContact kDmBook[] = {
    { L"Мама", { "+79161234501", NULL } }, { L"Папа", { "+79161234502", NULL } },
    { L"Алексей Смирнов", { "+79035550101", "+74951234567" } }, { L"Анна", { "+79267770202", NULL } },
    { L"Борис Иванович", { "+79858880303", NULL } }, { L"Вера (соседка)", { "+79169990404", NULL } },
    { L"Дмитрий Петров", { "+79031110505", NULL } }, { L"Екатерина", { "+79262220606", NULL } },
    { L"Игорь Шиномонтаж", { "+79853330707", NULL } }, { L"Лада Сервис", { "+74957770808", NULL } },
    { L"Ольга Николаевна", { "+79164440909", NULL } }, { L"Такси", { "+74959991010", NULL } } };
enum { kDmBookN = sizeof(kDmBook) / sizeof(kDmBook[0]) };
struct DmCall { int who, type, minAgo; };   // the call log, the latest first
static const DmCall kDmLog[] = {
    { 0, BT_PBC_RECEIVED, 25 }, { 3, BT_PBC_MISSED, 70 }, { 9, BT_PBC_DIALLED, 180 }, { 2, BT_PBC_RECEIVED, 300 },
    { 6, BT_PBC_DIALLED, 1440 }, { 11, BT_PBC_MISSED, 1500 }, { 1, BT_PBC_DIALLED, 2900 },
    { 7, BT_PBC_RECEIVED, 3000 } };
struct DmTrack { const wchar_t* title; const wchar_t* artist; const wchar_t* album; int sec; };
static const DmTrack kDmTracks[] = {
    { L"Группа крови", L"Кино", L"Группа крови", 286 }, { L"Город золотой", L"Аквариум", L"Десять стрел", 230 },
    { L"Что такое осень", L"ДДТ", L"Актриса Весна", 301 }, { L"Звезда по имени Солнце", L"Кино", L"Звезда по имени Солнце", 225 } };
enum { kDmTrackN = sizeof(kDmTracks) / sizeof(kDmTracks[0]) };
static int   s_dmTrack;
static DWORD s_dmBase;   // playing: the tick at the track's 0:00

static void DmBtLock() { InterlockedIncrement((LONG*)&s_dmBt->seq); }   // odd: being written
static void DmBtUnlock() { InterlockedIncrement((LONG*)&s_dmBt->seq); }

static void DmTrackSet(int i)   // (playing on, from its start)
{
    BtState& s = s_dmBt->st;
    s_dmTrack = (i + kDmTrackN) % kDmTrackN;
    const DmTrack& t = kDmTracks[s_dmTrack];
    DmU8(t.title, s.title, BT_TEXT);
    DmU8(t.artist, s.artist, BT_TEXT);
    DmU8(t.album, s.album, BT_TEXT);
    s.lengthMs = s.durationMs = t.sec * 1000;
    s.trackNo = s_dmTrack + 1;
    s.trackCount = kDmTrackN;
    s.posMs = 0;
    s_dmBase = GetTickCount();
    s.playTick = s.mediaTick = s_dmBase;
}

static void DmPlayState(int playing)   // 1 playing (A2DP streaming), 2 paused
{
    BtState& s = s_dmBt->st;
    s.playing = playing;
    s.av = 3 - playing;
    s.playTick = GetTickCount();
}

static void DmPlayOn()
{
    if (s_dmBt->st.playing != 1)
        s_dmBase = GetTickCount() - s_dmBt->st.posMs;
    DmPlayState(1);
}

static void DmPause()
{
    if (s_dmBt->st.playing == 1)
        s_dmBt->st.posMs = (int)(GetTickCount() - s_dmBase);
    DmPlayState(2);
}

static void DmPlayPos()   // playing: the position as the stack reports it, every 2 s; the end - the next one
{
    BtState& s = s_dmBt->st;
    DWORD now = GetTickCount();
    int pos = (int)(now - s_dmBase);
    if (pos >= s.lengthMs) {
        DmTrackSet(s_dmTrack + 1);
        return;
    }
    s.posMs = pos;
    s.playTick = now;
}

static void DmCallSet(int state, const char* num)   // num NULL: the number kept
{
    BtState& s = s_dmBt->st;
    s.call[0] = state;
    if (num) {
        strncpy(s.callNumber, num, sizeof(s.callNumber) - 1);
        s.callNumber[sizeof(s.callNumber) - 1] = 0;
    }
    s.hfAudio = state == BT_CALL_ACTIVE;
    s.callTick = GetTickCount();
}

static void DmCallTime(int minAgo, char* out)   // "20261021T153000", minAgo before now
{
    SYSTEMTIME t;
    GetLocalTime(&t);
    AddMinutes(&t, -minAgo);
    _snprintf(out, 20, "%04u%02u%02uT%02u%02u%02u", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    out[19] = 0;
}

static void DmPbEntry(BtPbEntry* e, int who)
{
    memset(e, 0, sizeof(*e));
    DmU8(kDmBook[who].name, e->name, sizeof(e->name));
    for (int k = 0; k < 2 && kDmBook[who].num[k]; k++)
        strcpy(e->num[k], kDmBook[who].num[k]);
}

static int DmPbBook(BtPbEntry* e)
{
    for (int i = 0; i < kDmBookN; i++)
        DmPbEntry(e + i, i);
    return kDmBookN;
}

static bool DmLogIn(int i, int type)   // type: BT_PBC_*, -1 all
{
    return type < 0 || kDmLog[i].type == type;
}

static int DmPbCalls(BtPbEntry* e, int type)
{
    int n = 0;
    for (int i = 0; i < (int)(sizeof(kDmLog) / sizeof(kDmLog[0])); i++) {
        if (!DmLogIn(i, type))
            continue;
        DmPbEntry(e + n, kDmLog[i].who);
        e[n].callType = (unsigned char)kDmLog[i].type;
        DmCallTime(kDmLog[i].minAgo, e[n].time);
        n++;
    }
    return n;
}

// The commands (BtShm::cmd), each its result (0 = ok)
static int DmCmdPlayPause(const char*) { s_dmBt->st.playing == 1 ? DmPause() : DmPlayOn(); return 0; }
static int DmCmdPlay(const char*) { DmPlayOn(); return 0; }
static int DmCmdPause(const char*) { DmPause(); return 0; }
static int DmCmdNext(const char*) { DmTrackSet(s_dmTrack + 1); return 0; }
static int DmCmdPrev(const char*) { DmTrackSet(s_dmTrack - 1); return 0; }
static int DmCmdDial(const char* num) { DmCallSet(BT_CALL_ALERTING, num); return 0; }   // (3 s on: answered)
static int DmCmdEnd(const char*) { DmCallSet(BT_CALL_NONE, ""); return 0; }
static int DmCmdGetOpts(const char*) { return s_dmOpts; }

static int DmCmdAnswer(const char*)
{
    if (s_dmBt->st.call[0] == BT_CALL_INCOMING)
        DmCallSet(BT_CALL_ACTIVE, NULL);
    return 0;
}

static int DmCmdSetOpt(const char* t)   // "c 1" / "a 0"
{
    int bit = t[0] == 'a' ? 2 : 1;
    s_dmOpts = t[2] == '1' ? s_dmOpts | bit : s_dmOpts & ~bit;
    return 0;
}

static int DmCmdPower(const char* t)   // "0" off, "1" / "2" on (at once: no "going" states here)
{
    int on = t[0] != '0';
    s_dmBt->power = on ? BT_POW_ON : BT_POW_OFF;
    s_dmBt->st.hf = 2 * on;        // handsfree 2 connected,
    s_dmBt->st.av = on;            // the music 1 connected,
    s_dmBt->st.playing = 2 * on;   // paused
    return 0;
}

static void DmPbFill(int storage, int list)   // the list into the book's mapping, done at once
{
    static const int kType[5] = { BT_PBC_DIALLED, BT_PBC_MISSED, -2, -1, BT_PBC_RECEIVED };   // by BT_PB_*; -2 the book
    BtPb* pb = s_dmPb;
    pb->state = BT_PBS_PULLING;
    pb->storage = storage;
    pb->list = list;
    pb->count = pb->total = kType[list] == -2 ? DmPbBook(pb->e) : DmPbCalls(pb->e, kType[list]);
    pb->tick = GetTickCount();
    pb->state = BT_PBS_DONE;
    InterlockedIncrement((LONG*)&pb->seq);
}

static int DmCmdPull(const char* t)   // "<storage> <list>" (BT_PB_*)
{
    int storage = 0, list = -1;
    sscanf(t, "%d %d", &storage, &list);
    if (!s_dmPb || (unsigned)list > BT_PB_ICH)
        return 1;
    DmPbFill(storage, list);
    return 0;
}

struct DmBtCmd { int cmd; int (*fn)(const char* text); };
static const DmBtCmd kDmBtCmds[] = {
    { BT_CMD_PLAYPAUSE, DmCmdPlayPause }, { BT_CMD_PLAY, DmCmdPlay }, { BT_CMD_PLAYON, DmCmdPlay },
    { BT_CMD_PAUSE, DmCmdPause }, { BT_CMD_NEXT, DmCmdNext }, { BT_CMD_PREV, DmCmdPrev }, { BT_CMD_DIAL, DmCmdDial },
    { BT_CMD_ANSWER, DmCmdAnswer }, { BT_CMD_END, DmCmdEnd }, { BT_CMD_GETOPTS, DmCmdGetOpts },
    { BT_CMD_SETOPT, DmCmdSetOpt }, { BT_CMD_POWER, DmCmdPower }, { BT_CMD_PBAP_PULL, DmCmdPull } };

static int DmBtCmdRun(int cmd, const char* text)   // (the others - the search, pairing... - only "done")
{
    for (int i = 0; i < (int)(sizeof(kDmBtCmds) / sizeof(kDmBtCmds[0])); i++)
        if (kDmBtCmds[i].cmd == cmd)
            return kDmBtCmds[i].fn(text);
    return 0;
}

static void DmBtCmdTake()   // a command written: carried out
{
    BtShm* b = s_dmBt;
    LONG want = b->cmdSeq;
    if (b->cmdDone == want)
        return;
    DmBtLock();
    b->cmdResult = DmBtCmdRun(b->cmd, b->cmdText);
    DmBtUnlock();
    b->cmdDone = want;
}

static void DmBtDo(void (*fn)())   // a change of the state, under its lock
{
    DmBtLock();
    fn();
    DmBtUnlock();
}

static void DmRingToggle()   // key C: "Мама" calling, or the call going on ended
{
    BtState& s = s_dmBt->st;
    s.call[0] ? DmCallSet(BT_CALL_NONE, "") : DmCallSet(BT_CALL_INCOMING, kDmBook[0].num[0]);
}

static void DmAnswered() { DmCallSet(BT_CALL_ACTIVE, NULL); }

static bool DmDue(int state, int want, DWORD since, DWORD ms)   // in that state for ms
{
    return state == want && GetTickCount() - since >= ms;
}

static void DmBtTick()   // every 250 ms: the key C's call, a dialled call answered 3 s on, the music's position every 2 s
{
    const BtState& s = s_dmBt->st;
    if (InterlockedExchange((LONG*)&s_dmRing, 0))
        DmBtDo(DmRingToggle);
    if (DmDue(s.call[0], BT_CALL_ALERTING, s.callTick, 3000))
        DmBtDo(DmAnswered);
    if (DmDue(s.playing, 1, s.playTick, 2000))
        DmBtDo(DmPlayPos);
}

static DWORD WINAPI DmThread(LPVOID)   // the proxy's thread: the commands as VestaHome sets its event, the ticks
{
    HANDLE ev = CreateEvent(NULL, FALSE, FALSE, BT_CMD_EVENT);
    for (;;) {
        WaitForSingleObject(ev, 250);
        DmBtCmdTake();
        DmBtTick();
    }
}

static void DmBtPaired()
{
    BtState& s = s_dmBt->st;
    s.paired = sizeof(kDmPaired) / sizeof(kDmPaired[0]);
    for (int i = 0; i < s.paired; i++) {
        strcpy(s.pairedAddr[i], kDmPaired[i].addr);
        DmU8(kDmPaired[i].name, s.pairedName[i], 48);
    }
}

static void DmBtStart()
{
    s_dmBt = (BtShm*)DmMap(BT_SHM_NAME, sizeof(BtShm));
    if (!s_dmBt)
        return;
    BtState& s = s_dmBt->st;
    s.ready = s.avrcp = 1;
    s.hf = 2;
    s.battery = 4;
    s.signal = 3;
    s.pan = BT_PAN_IDLE;
    strcpy(s.devAddr, kDmPaired[0].addr);
    DmU8(kDmPaired[0].name, s.devName, BT_TEXT);
    DmBtPaired();
    DmU8(L"МТС", s.operatorName, sizeof(s.operatorName));
    DmTrackSet(0);
    DmPlayState(2);   // (paused: the phone's music plays once it is the source - BT_CMD_PLAYON - or by its key)
    s.devTick = s.avTick = GetTickCount();
    s_dmPb = (BtPb*)DmMap(BT_PB_NAME, sizeof(BtPb));
    if (s_dmPb) {
        s_dmPb->pbap = 2;
        s_dmPb->magic = BT_SHM_MAGIC;
    }
    s_dmBt->version = 5;
    s_dmBt->origOk = 1;
    s_dmBt->power = BT_POW_ON;
    s_dmBt->magic = BT_SHM_MAGIC;
    CreateThread(NULL, 0, DmThread, NULL, 0, NULL);
}

static void DemoRing()   // the PC's key C
{
    InterlockedExchange((LONG*)&s_dmRing, 1);
}

// ---- GPS: GpsSim's drive, a minute in (moving), once a second; its "UTC" = the PC's clock less the zone, so
// GpsTimeSync finds the clock right and never sets the PC's
static int DmGpsLines(char* buf, int cap)   // (vgps's thread, every ~100 ms)
{
    static DWORD s_t0, s_sent;
    DWORD now = GetTickCount();
    if (!s_t0)
        s_t0 = now - 60000;
    if (now - s_sent < 1000)
        return 0;
    s_sent = now;
    SYSTEMTIME t;
    GetLocalTime(&t);
    AddMinutes(&t, -s_cfg.tzMin);
    return SimLines(kRoute[SimAt(now - s_t0, sizeof(kRoute) / sizeof(kRoute[0]))], t, buf, cap);
}

static GpsSourceFn DemoGpsSource()   // NULL: gps.port plays a .nmea recording beside the exe
{
    wchar_t p[MAX_PATH];
    _snwprintf(p, MAX_PATH, L"%s%s", s_dir, s_cfg.gpsPort);
    p[MAX_PATH - 1] = 0;
    bool rec = wcsstr(s_cfg.gpsPort, L".nmea") && GetFileAttributes(p) != INVALID_FILE_ATTRIBUTES;
    return rec ? NULL : DmGpsLines;
}

// ---- OSMapsCE's favourites on the PC: favorites.pc.txt beside VestaHome (VFavsSave's format), five of Moscow's
// places written when there is no such file
struct DmFav { const wchar_t* name; double lat, lon; int icon; const wchar_t* addr; };
static const DmFav kDmFavs[] = {
    { L"Дом", 55.80310, 37.51540, VFAV_HOME, L"Ленинградский пр-т, 64" },
    { L"Работа", 55.75700, 37.61500, VFAV_WORK, L"Тверская ул., 7" },
    { L"Дача", 55.98200, 37.41800, VFAV_COTTAGE, L"Лунёво" },
    { L"АЗС", 55.82580, 37.49640, VFAV_FUEL, L"Ленинградское ш., 39" },
    { L"Сервис", 55.88350, 37.54470, VFAV_PARKING, L"Дмитровское ш., 98" } };

static void DemoFavsPath(wchar_t* path)
{
    _snwprintf(path, MAX_PATH, L"%sfavorites.pc.txt", s_dir);
    path[MAX_PATH - 1] = 0;
    if (GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES)
        return;
    static VFavs f;
    VFavsClear(&f);
    for (int i = 0; i < (int)(sizeof(kDmFavs) / sizeof(kDmFavs[0])); i++) {
        int k = VFavsAdd(&f, kDmFavs[i].name, kDmFavs[i].lat, kDmFavs[i].lon);
        f.f[k].icon = kDmFavs[i].icon;
        VFavsSetAddr(&f, k, kDmFavs[i].addr);
    }
    VFavsSave(&f, path);
}

static void DemoStart()   // WM_CREATE, before the MCU's and GPS's threads
{
    DmMcuStart();
    DmBtStart();
    VLog(L"демо ГУ (ПК): MCU %s, Bluetooth %s", s_dmMcu ? L"демо" : L"чужой", s_dmBt ? L"демо" : L"чужой");
}
#endif
