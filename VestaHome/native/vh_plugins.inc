// vh_plugins.inc - part of home.cpp (included there, in order): add-ons (docs/plugin-api.md, API 1). An add-on is
// a program of its own: VestaHome starts the ones in VHContent\VHPlugins\<Name>\<Name>.exe (not switched off - the file
// "disabled" in its folder), it says hello by WM_COPYDATA ("VHAP": one command a message, "verb\tkey=value..."
// in UTF-16) and declares widgets and screens; VestaHome draws them in its own style and sends the taps back.
// VestaHome stays free: whatever an add-on does lives in its own program (the user's decision, 2026-10-26).
// Done: hello / bye, widgets (through the user widgets' list; pictures in shared memory), described screens and
// screens of the add-on's own window, up to two buttons in the bottom bar, the data subscriptions (gps, car, media,
// night), notify, open, media; "Settings -> General -> Add-ons". Not yet: volume (no way to it but the knob).

// (the protocol's send and fields: VestaCore vhap.h)
#include "plugvol.h"   // the volumes add-ons start from
enum { kPlugMax = 4, kPlugScreens = 4, kPlugItems = 16, kPlugWidgets = 2, kPlugRows = 6, kPlugApi = 2 };   // (2: type=text)

struct PlugItem { wchar_t id[24], type[12], label[48], sub[64], value[48], options[64], link[24]; bool secret; };
struct PlugScreen { wchar_t id[24], title[48], icon[16]; bool nav, own; HWND win; int n; PlugItem it[kPlugItems]; };
struct PlugWidget { wchar_t id[24], title[40], icon[16], screen[24], value[24], unit[24], text[2][96]; int state;
                    bool wide, big, image, shown;
                    HANDLE map; BYTE* view; int iw, ih; };   // the picture (4.1): its shared memory and size
struct Plugin {
    wchar_t    id[24], name[48], ver[16], dir[MAX_PATH];   // dir: its folder ("" - started from elsewhere)
    HWND       hwnd;                                       // its window (0 - not connected)
    bool       off;                                        // switched off: not started with VestaHome
    int        sub;                                        // subscribed: 1 gps, 2 car, 4 media, 8 night, 16 keys
    int        ns, nw;
    PlugScreen sc[kPlugScreens];
    PlugWidget wg[kPlugWidgets];
};
static Plugin s_plug[kPlugMax];
static VNav   s_nav;             // the route's next manoeuvre, from add-on s_navPlug ("nav")
static int    s_navPlug = -1;
static int    s_plugN;
static int    s_plugCur = -1, s_plugScr = -1, s_plugPage;   // the add-on screen shown (S_PLUGIN)
static int    s_plugOpenP = -1, s_plugOpenS = -1;           // the one told "screen.open" (for its "screen.close")
static int    s_plugOwnP = -1, s_plugOwnS = -1;             // an add-on's own window in front (5.2)
// The add-on screens opened one over another (a row's link): the history keeps S_PLUGIN once, this stack which
// one (HU 2026-10-26: "back" on the second screen gave the second again, without its button).
static int    s_plugStack[8][2], s_plugStackN;

static bool PlugOwnsPid(DWORD pid)   // a connected add-on's process (GuardStuck: not taken for hung)
{
    for (int i = 0; i < s_plugN; i++) {
        DWORD p = 0;
        if (s_plug[i].hwnd && GetWindowThreadProcessId(s_plug[i].hwnd, &p) && p == pid)
            return true;
    }
    return false;
}

static bool PlugCanBack()   // an add-on screen under the one shown
{
    return s_screen == S_PLUGIN && s_plugStackN > 0;
}

static bool PlugBack()   // "back" on an add-on screen with another under it: that one
{
    if (!PlugCanBack())
        return false;
    s_plugStackN--;
    s_plugCur = s_plugStack[s_plugStackN][0];
    s_plugScr = s_plugStack[s_plugStackN][1];
    s_plugPage = 0;
    s_screenTick = GetTickCount();
    return true;
}

static void MediaStep(int dir);
static void MediaToggle();

static void PlugSend(Plugin& p, const wchar_t* fmt, ...)
{
    if (!p.hwnd || !IsWindow(p.hwnd)) {
        p.hwnd = NULL;
        return;
    }
    va_list a;
    va_start(a, fmt);
    VhapSendV(p.hwnd, s_wnd, fmt, a);
    va_end(a);
}

// A panel / wheel button to the add-on whose window is in front, if it subscribed to "keys" (7): its answer 2 -
// it took the button, VestaHome does nothing with it; otherwise VestaHome's own action (the user 2026-10-26: the
// map's buttons must not take the media's and the phone's).
static bool PlugFront(const Plugin& p, HWND fg)   // its window in front - or its screen's own one (Android Auto, 5.2)
{
    if (fg == p.hwnd)
        return true;
    for (int k = 0; k < p.ns; k++)
        if (p.sc[k].own && p.sc[k].win && p.sc[k].win == fg)
            return true;
    return false;
}

static bool PlugKey(int key, int st, int kind)
{
    HWND fg = GetForegroundWindow();
    for (int i = 0; i < s_plugN; i++) {
        Plugin& p = s_plug[i];
        if (!(p.sub & 16) || !p.hwnd || !IsWindow(p.hwnd) || !PlugFront(p, fg))
            continue;
        if (VhapSend(p.hwnd, s_wnd, L"key\tcode=%d\tst=%d\tkind=%d", key, st, kind) == 2) {
            VLog(L"кнопка 0x%02X — дополнению %s", key, p.id);
            return true;
        }
    }
    return false;
}

static Plugin* PlugById(const wchar_t* id)
{
    for (int i = 0; i < s_plugN; i++)
        if (!_wcsicmp(s_plug[i].id, id))
            return &s_plug[i];
    return NULL;
}

static bool PlugRunning(const wchar_t* id)
{
    Plugin* p = PlugById(id);
    return p && p->hwnd && IsWindow(p->hwnd);
}

static Plugin* PlugByWnd(HWND h)
{
    for (int i = 0; i < s_plugN; i++)
        if (s_plug[i].hwnd == h)
            return &s_plug[i];
    return NULL;
}

static PlugScreen* PlugScreenOf(Plugin& p, const wchar_t* id, int* idx = NULL)
{
    for (int i = 0; i < p.ns; i++)
        if (!wcscmp(p.sc[i].id, id)) {
            if (idx)
                *idx = i;
            return &p.sc[i];
        }
    return NULL;
}

static Icon PlugIcon(const wchar_t* name)
{
    for (int i = 0; i < (int)(sizeof(kIconNames) / sizeof(kIconNames[0])); i++)
        if (!_wcsicmp(name, kIconNames[i].name))
            return kIconNames[i].ic;
    return IC_APPS;
}

// The connected add-ons' widgets into the user widgets' list (UserWidgetsScan calls it, before their order is read):
// key "p:<add-on>.<widget>", drawn by WUser, a tap - widget.tap to the add-on.
// OSMapsCE's map widget before it runs (the user 2026-10-31: on the main screen always): the first two of its
// favourite places (VestaCore vfavs, favorites.txt beside its exe; read again every 10 s) - a tap: OSMapsCE with the
// route there ("route" as its command line, or the command to the copy running); elsewhere - OSMapsCE opens.
static const wchar_t kOsmKey[] = L"p:OSMapsCE.map";
static VFavs s_osmFavs;
static DWORD s_osmFavsTick;

static int NaviApp();   // (vh_tasks.inc: OSMapsCE on the apps' list)
static void RunApp(int i);

// An add-on that took the navigation ("navi on=1", plugin-api.md 7.4: Android Auto while its session runs): OSMapsCE's
// starts (its tile, the Navigation and the map widgets, a favourite's route) open the add-on's screen instead - two
// navigators at once would fight for the unit's memory and processor (the user 2026-11-17).
static int s_naviPlug = -1;
static void PlugOpen(Plugin& p, int s, bool push);   // (below)

static bool NaviToAddon()
{
    if (s_naviPlug < 0 || s_naviPlug >= s_plugN || !s_plug[s_naviPlug].hwnd || !IsWindow(s_plug[s_naviPlug].hwnd) ||
        !s_plug[s_naviPlug].ns)
        return false;
    VLog(L"навигация: идёт сеанс %s - вместо OSMapsCE его экран", s_plug[s_naviPlug].id);
    PlugOpen(s_plug[s_naviPlug], 0, true);
    return true;
}

static Plugin* OsmPlug()   // OSMapsCE running and connected
{
    Plugin* p = PlugById(L"OSMapsCE");
    return p && p->hwnd && IsWindow(p->hwnd) ? p : NULL;
}

static void OsmFavsRead()
{
    int app = NaviApp();
    if (app < 0 || (s_osmFavsTick && GetTickCount() - s_osmFavsTick < 10000))
        return;
    s_osmFavsTick = GetTickCount() | 1;
    wchar_t path[MAX_PATH];
    wcscpy(path, s_apps[app].path);
    wchar_t* slash = wcsrchr(path, L'\\');
    wcscpy(slash ? slash + 1 : path, L"favorites.txt");
#ifndef UNDER_CE
    DemoFavsPath(path);   // the PC: its own demo file (vh_demo.inc), never OSMapsCE's
#endif
    VFavsLoad(&s_osmFavs, path);
}

static void OsmFavGo(int i)   // a favourite tapped: OSMapsCE, the route there
{
    int app = NaviApp();
    if (app < 0 || i < 0 || i >= s_osmFavs.n)
        return;
    const VFav& f = s_osmFavs.f[i];
    wchar_t cmd[160];
    _snwprintf(cmd, 160, L"route\tlat=%.6f\tlon=%.6f\tname=%s", f.lat, f.lon, f.name);
    cmd[159] = 0;
    VLog(L"OSMapsCE: маршрут к «%s»", f.name);
    Plugin* p = OsmPlug();
    if (p) {   // running: the command, then to the front
        VhapSend(p->hwnd, s_wnd, L"%s", cmd);
        RunApp(app);
        return;
    }
    wchar_t keep[200];
    wcscpy(keep, s_apps[app].params);   // (its own parameters for this start only)
    wcsncpy(s_apps[app].params, cmd, 199);
    s_apps[app].params[199] = 0;
    RunApp(app);
    wcscpy(s_apps[app].params, keep);
}

static void OsmWidgetAppend()   // before it runs: the widget kept on the list, ours to draw
{
    if (OsmPlug() || NaviApp() < 0 || s_uwCount >= kWgUser)
        return;
    for (int i = 0; i < s_uwCount; i++)
        if (!wcscmp(s_uw[i].key, kOsmKey))
            return;
    UserWg& u = s_uw[s_uwCount++];
    memset(&u, 0, sizeof(u));
    wcscpy(u.key, kOsmKey);
    wcscpy(u.title, L"Карта OSM");
    u.ic = PlugIcon(L"map");
    u.wide = u.big = true;
    u.have = true;
    wcscpy(u.tap, L"plugin");
}

static void OsmWidgetDraw(HDC dc, int x, int y, int ww, int hh)
{
    OsmFavsRead();
    int n = s_osmFavs.n < 2 ? s_osmFavs.n : 2;
    for (int i = 0; i < n; i++)
        Button(dc, x + 16 + i * ((ww - 40) / 2 + 8), y + 60, (ww - 40) / 2, 64, s_osmFavs.f[i].name, B_N, IC_STAR, 18,
               0, A_OSMFAV, i);
    TWrap(dc, x + 16, y + (n ? 140 : 70), n ? L"маршрут туда — нажатие на адрес" :
          L"Избранное: долгое нажатие на карте → «В ИЗБРАННОЕ»", 15, kMuted, 400, ww - 32, 2, 0);
    T(dc, x + 16, y + hh - 22, L"нажмите, чтобы открыть карту", 14, kDim, 400, AL_START, ww - 32);
}

static void PlugWidgetsAppend()
{
    for (int i = 0; i < s_plugN; i++) {
        Plugin& p = s_plug[i];
        for (int k = 0; p.hwnd && k < p.nw && s_uwCount < kWgUser; k++) {
            const PlugWidget& g = p.wg[k];
            UserWg& u = s_uw[s_uwCount++];
            memset(&u, 0, sizeof(u));
            _snwprintf(u.key, 40, L"p:%s.%s", p.id, g.id);
            u.key[39] = 0;
            wcscpy(u.title, g.title);
            u.ic = PlugIcon(g.icon);
            u.wide = g.wide || g.big;
            u.big = g.big;
            u.have = true;
            wcscpy(u.tap, L"plugin");
            wcscpy(u.value, g.value);
            wcscpy(u.unit, g.unit);
            wcscpy(u.text[0], g.text[0]);
            wcscpy(u.text[1], g.text[1]);
            u.state = g.state;
        }
    }
    OsmWidgetAppend();
}

static void PlugWidgetCopy(Plugin& p, const PlugWidget& g)   // a widget's new values into its entry on the list
{
    wchar_t key[40];
    _snwprintf(key, 40, L"p:%s.%s", p.id, g.id);
    key[39] = 0;
    for (int i = 0; i < s_uwCount; i++)
        if (!wcscmp(s_uw[i].key, key)) {
            wcscpy(s_uw[i].value, g.value);
            wcscpy(s_uw[i].unit, g.unit);
            wcscpy(s_uw[i].text[0], g.text[0]);
            wcscpy(s_uw[i].text[1], g.text[1]);
            s_uw[i].state = g.state;
        }
    if (s_screen == S_HOME && IsWindowVisible(s_wnd))
        Redraw();
}

static bool PlugWidgetTap(const UserWg& u)   // an add-on's widget tapped: its screen, or the event
{
    if (wcsncmp(u.key, L"p:", 2))
        return false;
    wchar_t pid[24];
    const wchar_t* dot = wcschr(u.key + 2, L'.');
    if (!dot)
        return true;
    int n = (int)(dot - u.key - 2) < 23 ? (int)(dot - u.key - 2) : 23;
    wcsncpy(pid, u.key + 2, n);
    pid[n] = 0;
    Plugin* p = PlugById(pid);
    if (!wcscmp(u.key, kOsmKey) && NaviToAddon())   // (the navigation is Android Auto's now)
        return true;
    if (!wcscmp(u.key, kOsmKey) && !OsmPlug()) {   // OSMapsCE not running: it opens
        if (NaviApp() >= 0)
            RunApp(NaviApp());
        return true;
    }
    if (!p)
        return true;
    for (int k = 0; k < p->nw; k++)
        if (!wcscmp(p->wg[k].id, dot + 1)) {
            int s;
            if (p->wg[k].screen[0] && PlugScreenOf(*p, p->wg[k].screen, &s)) {
                s_plugCur = (int)(p - s_plug);
                s_plugScr = s;
                s_plugPage = 0;
                Go(S_PLUGIN, true);
            }
            PlugSend(*p, L"widget.tap\tid=%s", p->wg[k].id);
        }
    return true;
}

static void PlugOwnClose()   // the add-on's own window goes, VestaHome comes back
{
    if (s_plugOwnP >= 0 && s_plugOwnP < s_plugN && s_plugOwnS < s_plug[s_plugOwnP].ns) {
        Plugin& p = s_plug[s_plugOwnP];
        PlugSend(p, L"screen.close\tid=%s", p.sc[s_plugOwnS].id);
    }
    s_plugOwnP = s_plugOwnS = -1;
    ShowWindow(s_wnd, SW_SHOW);
    SetForegroundWindow(s_wnd);
    Redraw();
}

static void PlugOpen(Plugin& p, int s, bool push = true)
{
    PlugScreen& sc = p.sc[s];
    if (sc.own) {   // its own window over the whole screen (5.2)
        if (!sc.win || !IsWindow(sc.win))
            return;
        s_plugOwnP = (int)(&p - s_plug);
        s_plugOwnS = s;
        PlugSend(p, L"screen.open\tid=%s", sc.id);
        SetWindowPos(sc.win, HWND_TOPMOST, 0, 0, 800, 480, SWP_SHOWWINDOW);
        SetForegroundWindow(sc.win);
        return;
    }
    if (push && s_screen == S_PLUGIN) {   // over another add-on screen: remembered here, not in the history
        if (s_plugStackN < 8) {
            s_plugStack[s_plugStackN][0] = s_plugCur;
            s_plugStack[s_plugStackN][1] = s_plugScr;
            s_plugStackN++;
        }
        s_plugCur = (int)(&p - s_plug);
        s_plugScr = s;
        s_plugPage = 0;
        s_screenTick = GetTickCount();
        Redraw();
        return;
    }
    Go(S_PLUGIN, push);   // (Go empties the stack: a fresh start)
    s_plugCur = (int)(&p - s_plug);
    s_plugScr = s;
    s_plugPage = 0;
    Redraw();
}

// The bottom bar's add-on buttons: the screens with nav=1 of the connected add-ons, the first two.
static bool PlugNavAt(int k, int* pi, int* si)
{
    for (int i = 0; i < s_plugN; i++)
        for (int s = 0; s_plug[i].hwnd && !s_plug[i].off && s < s_plug[i].ns; s++)
            if (s_plug[i].sc[s].nav && k-- == 0) {
                *pi = i;
                *si = s;
                return true;
            }
    return false;
}

static int PlugNavCount()
{
    int n = 0, p, s;
    while (n < 2 && PlugNavAt(n, &p, &s))
        n++;
    return n;
}

static Icon PlugNavIcon(int k)
{
    int p, s;
    return PlugNavAt(k, &p, &s) ? PlugIcon(s_plug[p].sc[s].icon) : IC_APPS;
}

static const wchar_t* PlugNavLabel(int k)
{
    int p, s;
    return PlugNavAt(k, &p, &s) ? (s_plug[p].sc[s].title[0] ? s_plug[p].sc[s].title : s_plug[p].name) : L"";
}

static int PlugNavSection()   // the add-on screen shown: its button's section (6 / 7), or -1
{
    int p, s;
    for (int k = 0; k < PlugNavCount(); k++)
        if (PlugNavAt(k, &p, &s) && p == s_plugCur && s == s_plugScr)
            return 6 + k;
    return -1;
}

static void PlugNavTap(int k)
{
    int p, s;
    if (PlugNavAt(k, &p, &s))
        PlugOpen(s_plug[p], s, false);   // (a main screen: no history, as the other sections)
}

// An add-on's own sound (Android Auto, "sound"): the MCU's source watched; another source taken (MODE) - the add-on
// told "sound.lost" once (soundlost.h), until its sound starts again.
static int s_plugSound = -1, s_soundSrc = -1;   // the add-on, the MCU's source last seen

static int AddonSoundPlug()
{
    return s_plugSound;
}

// "playing" (plugin-api.md 7.3): what the add-on's music plays now
static void PlugPlaying(Plugin* p, const wchar_t* cmd)
{
    wchar_t v[16];
    s_ap.have = true;
    s_ap.plug = (int)(p - s_plug);
    s_ap.title[0] = s_ap.artist[0] = s_ap.album[0] = s_ap.app[0] = s_ap.art[0] = 0;
    VhapField(cmd, L"title", s_ap.title, 96);
    VhapField(cmd, L"artist", s_ap.artist, 96);
    VhapField(cmd, L"album", s_ap.album, 96);
    VhapField(cmd, L"app", s_ap.app, 32);
    VhapField(cmd, L"art", s_ap.art, MAX_PATH);
    s_ap.state = VhapField(cmd, L"state", v, 16) ? (!wcscmp(v, L"play") ? 2 : !wcscmp(v, L"pause") ? 3 : 1) : 1;
    s_ap.pos = VhapField(cmd, L"pos", v, 16) ? _wtoi(v) : 0;
    s_ap.len = VhapField(cmd, L"len", v, 16) ? _wtoi(v) : 0;
    s_ap.tick = GetTickCount();
    RECT r;
    if (s_screen == S_HOME && IsWindowVisible(s_wnd) && HomeMediaRect(&r))
        RedrawArea(r.left, r.top, r.right, r.bottom);
}

static void AddonKey(int dir)   // the "now playing" widget's buttons for the add-on's music
{
    if (s_ap.plug >= 0 && s_ap.plug < s_plugN)
        PlugSend(s_plug[s_ap.plug], L"media.key\tkey=%s", dir < 0 ? L"prev" : dir > 0 ? L"next" : L"toggle");
}

static void PlugSoundWatch()
{
    if (s_plugSound < 0 || s_plugSound >= s_plugN)
        return;
    int now = s_src;
    if (!SoundLost(s_soundSrc, now)) {
        s_soundSrc = now;
        return;
    }
    VLog(L"дополнение %s: источник MCU %d -> %d, свой звук потерян (sound.lost)", s_plug[s_plugSound].id, s_soundSrc, now);
    PlugSend(s_plug[s_plugSound], L"sound.lost\tsrc=%d", now);
    s_plugSound = -1;
}

// The data an add-on subscribed to (7): each second; gps while a fix is fresh, car - the sensors with data (their
// number as the key, VestaHome/docs/plugin-api.md), media and night when they change.
static void PlugData()
{
    static int s_night0 = -1;
    static wchar_t s_media0[160];
    wchar_t media[160];
    Icon ic = SaverNowPlaying(media, 160);
    (void)ic;
    bool mediaNew = wcscmp(media, s_media0) != 0, nightNew = s_night0 != (s_night ? 1 : 0);
    // Bluetooth (api 3, 2026-11-17: VestaAuto offers it to Android Auto - the phone takes the car's HFP for calls):
    // the unit's address, the phone connected and its HFP state
    static wchar_t s_bt0[80];
    wchar_t bt[80], unit[24] = L"", phone[18] = L"";
    BtState st;
    int hf = BtGet(&st, NULL) ? st.hf : -1;
    static DWORD s_btAsked;   // the unit's address: asked of the MCU until it answers, every 30 s (sheet 41, 2026-11-19:
    LONG attach, wraps;       // only "Details" asked for it - Android Auto never got it)
    if (!McuBtAddr(unit, 24) && McuState(&attach, &wraps) > 0 && (!s_btAsked || GetTickCount() - s_btAsked > 30000)) {
        s_btAsked = GetTickCount();
        McuSend0("SPApi_SendRadioRequestBTAddress");
    }
    if (hf >= 1 && st.devAddr[0])
        BtText(st.devAddr, phone, 18);
    _snwprintf(bt, 80, L"bt\tunit=%s\tphone=%s\thf=%d", unit, phone, hf);
    bt[79] = 0;
    bool btNew = wcscmp(bt, s_bt0) != 0;
    // the theme's accent: to every add-on when it changes (welcome has it at first; the user's rule 2026-10-26 -
    // the add-ons' accents and highlights are VestaHome's)
    static COLORREF s_accent0 = (COLORREF)-1;
    bool accentNew = s_accent0 != (COLORREF)-1 && s_accent0 != kOrange;
    s_accent0 = kOrange;
    for (int i = 0; i < s_plugN; i++) {
        Plugin& p = s_plug[i];
        if (accentNew && p.hwnd)
            PlugSend(p, L"theme\taccent=%06X", (unsigned)(GetRValue(kOrange) << 16 | GetGValue(kOrange) << 8 |
                                                           GetBValue(kOrange)));
        if (!p.hwnd || !(p.sub & 0xFF))
            continue;
        bool first = (p.sub & 0x100) != 0;
        p.sub &= 0xFF;
        if (p.sub & 1) {
            Gps g;
            GpsGet(&g);
            if (GpsFresh(g, 5000))
                PlugSend(p, L"gps\tfix=1\tlat=%.6f\tlon=%.6f\tspeed=%d\tcourse=%d", g.lat, g.lon, CarKmh(),
                         (int)g.course);
            else if (first)
                PlugSend(p, L"gps\tfix=0");
        }
        if (p.sub & 2) {
            wchar_t s[480], v[32];
            int n = _snwprintf(s, 480, L"car");
            for (int id = 0; id < kCarSensors && n < 440; id++)
                if (CarSensor(id, v, 32))
                    n += _snwprintf(s + n, 480 - n, L"\t%d=%s", id, v);
            s[479] = 0;
            PlugSend(p, L"%s", s);
        }
        if ((p.sub & 4) && (mediaNew || first))
            PlugSend(p, L"media\ttitle=%s\tplaying=%d", media, media[0] ? 1 : 0);
        if ((p.sub & 8) && (nightNew || first))
            PlugSend(p, L"night\tvalue=%d", s_night ? 1 : 0);
        if ((p.sub & 32) && (btNew || first))
            PlugSend(p, L"%s", bt);
    }
    wcscpy(s_bt0, bt);
    wcscpy(s_media0, media);
    s_night0 = s_night ? 1 : 0;
}

static void PlugItemSet(PlugItem& it, const wchar_t* cmd)   // the fields a command names
{
    wchar_t v[96];
    if (VhapField(cmd, L"type", v, 12)) wcscpy(it.type, v);
    if (VhapField(cmd, L"label", v, 48)) wcscpy(it.label, v);
    if (VhapField(cmd, L"sub", v, 64)) wcscpy(it.sub, v);
    if (VhapField(cmd, L"value", v, 48)) wcscpy(it.value, v);
    if (VhapField(cmd, L"options", v, 64)) wcscpy(it.options, v);
    if (VhapField(cmd, L"link", v, 24)) wcscpy(it.link, v);
    if (VhapField(cmd, L"secret", v, 4)) it.secret = v[0] == L'1';
}

static void PlugWidgetSet(PlugWidget& g, const wchar_t* cmd)
{
    wchar_t v[96];
    if (VhapField(cmd, L"title", v, 40)) wcscpy(g.title, v);
    if (VhapField(cmd, L"icon", v, 16)) wcscpy(g.icon, v);
    if (VhapField(cmd, L"screen", v, 24)) wcscpy(g.screen, v);
    if (VhapField(cmd, L"size", v, 8)) {
        g.wide = !wcscmp(v, L"2") || !wcscmp(v, L"2x1");
        g.big = !wcscmp(v, L"big") || !wcscmp(v, L"2x2");
    }
    if (VhapField(cmd, L"image", v, 8)) g.image = v[0] == L'1';
    if (VhapField(cmd, L"value", v, 24)) wcscpy(g.value, v);
    if (VhapField(cmd, L"unit", v, 24)) wcscpy(g.unit, v);
    if (VhapField(cmd, L"text", v, 96)) wcscpy(g.text[0], v);
    if (VhapField(cmd, L"text2", v, 96)) wcscpy(g.text[1], v);
    if (VhapField(cmd, L"state", v, 8))
        g.state = !wcscmp(v, L"ok") ? 1 : !wcscmp(v, L"warn") ? 2 : !wcscmp(v, L"alert") ? 3 : 0;
}

// The picture widget (4.1): VestaHome makes the shared memory - 16 bytes ('VHIM', width, height, frame number), then
// RGB565 rows top down - and tells the add-on its name and size; the field under the caption.
struct PlugImHead { DWORD magic, w, h, seq; };

static void PlugWidgetFree(PlugWidget& g)
{
    if (g.view)
        UnmapViewOfFile(g.view);
    if (g.map)
        CloseHandle(g.map);
    g.view = NULL;
    g.map = NULL;
}

static void PlugWidgetImage(Plugin& p, PlugWidget& g)
{
    PlugWidgetFree(g);
    g.iw = kWgW2 - 16;
    g.ih = (g.big ? kWgH2 : kWgH) - 52;
    wchar_t name[64];
    _snwprintf(name, 64, L"VHAP_%s_%s", p.id, g.id);
    name[63] = 0;
    DWORD size = sizeof(PlugImHead) + g.iw * g.ih * 2;
    g.map = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, size, name);
    g.view = g.map ? (BYTE*)MapViewOfFile(g.map, FILE_MAP_ALL_ACCESS, 0, 0, size) : NULL;
    if (!g.view) {
        VLog(L"дополнение: %s.%s - нет общей памяти для картинки (%u)", p.id, g.id, GetLastError());
        PlugWidgetFree(g);
        return;
    }
    PlugImHead* h = (PlugImHead*)g.view;
    h->magic = 0x4D494856;   // 'VHIM'
    h->w = g.iw;
    h->h = g.ih;
    h->seq = 0;
    g.shown = false;
    PlugSend(p, L"widget.size	id=%s	w=%d	h=%d	shm=%s", g.id, g.iw, g.ih, name);
}

static PlugWidget* PlugWidgetOf(const UserWg& u, Plugin** pp = NULL)   // an add-on widget's entry by its key
{
    if (wcsncmp(u.key, L"p:", 2))
        return NULL;
    const wchar_t* dot = wcschr(u.key + 2, L'.');
    if (!dot)
        return NULL;
    for (int i = 0; i < s_plugN; i++) {
        Plugin& p = s_plug[i];
        int n = (int)wcslen(p.id);
        if (n != dot - u.key - 2 || wcsncmp(p.id, u.key + 2, n))
            continue;
        for (int k = 0; k < p.nw; k++)
            if (!wcscmp(p.wg[k].id, dot + 1)) {
                if (pp)
                    *pp = &p;
                return &p.wg[k];
            }
    }
    return NULL;
}

static bool PlugWidgetDraw(HDC dc, const UserWg& w, int x, int y, int ww, int hh)
{
    if (!wcscmp(w.key, kOsmKey) && !OsmPlug()) {
        OsmWidgetDraw(dc, x, y, ww, hh);
        return true;
    }
    PlugWidget* g = PlugWidgetOf(w);
    if (!g || !g->image)
        return false;
    const PlugImHead* h = g->view ? (const PlugImHead*)g->view : NULL;
    if (!h || !h->seq) {
        T(dc, x + ww / 2, y + hh / 2 + 8, L"ждём картинку…", 16, kMuted, 600, AL_MID);
        return true;
    }
    Pic pic;
    pic.w = g->iw;
    pic.h = g->ih;
    pic.px = (unsigned short*)(g->view + sizeof(PlugImHead));
    pic.px32 = NULL;
    BlitPic(pic, x + 8, y + 44, 10);
    return true;
}

// The manoeuvre to show in the top bar: not when its navigator is gone, nor while its own picture is on the screen -
// the widget says the same.
static bool NavChipGet(VNav* n)
{
    if (!s_nav.on || s_navPlug < 0 || !s_plug[s_navPlug].hwnd || !IsWindow(s_plug[s_navPlug].hwnd))
        return false;
    const Plugin& p = s_plug[s_navPlug];
    for (int k = 0; k < p.nw; k++)
        if (p.wg[k].image && p.wg[k].shown)
            return false;
    *n = s_nav;
    return true;
}

static void NavChipTap()
{
    if (s_navPlug >= 0 && s_plug[s_navPlug].hwnd)
        PlugSend(s_plug[s_navPlug], L"nav.tap");
}

// A command from an add-on's window from (WM_COPYDATA). Unknown commands and fields are passed over.
static void PlugReceive(HWND from, const wchar_t* cmd)
{
    wchar_t verb[24], id[24], v[96];
    int n = 0;
    while (cmd[n] && cmd[n] != L'\t' && n < 23) {
        verb[n] = cmd[n];
        n++;
    }
    verb[n] = 0;
    if (!wcscmp(verb, L"hello")) {
        VhapField(cmd, L"id", id, 24);
        if (!id[0])
            return;
        Plugin* p = PlugById(id);
        if (!p && s_plugN < kPlugMax) {   // started from elsewhere: known from now
            p = &s_plug[s_plugN++];
            memset(p, 0, sizeof(*p));
            wcscpy(p->id, id);
        }
        if (!p)
            return;
        p->hwnd = VhapField(cmd, L"hwnd", v, 16) ? (HWND)wcstoul(v, NULL, 16) : from;
        if (!p->hwnd)
            p->hwnd = from;
        VhapField(cmd, L"name", p->name, 48);
        if (!p->name[0])
            wcscpy(p->name, id);
        VhapField(cmd, L"ver", p->ver, 16);
        for (int k = 0; k < p->nw; k++)
            PlugWidgetFree(p->wg[k]);
        p->ns = p->nw = 0;   // (declared again after each hello)
        VLog(L"дополнение: %s (%s %s) подключено", p->id, p->name, p->ver);
        PlugSend(*p, L"welcome\tapi=%d\tvh=%s\tnight=%d\taccent=%06X\tlang=ru\tsupport=%d\tunit=%s", kPlugApi,
                 VhVersion(), s_night ? 1 : 0,
                 (unsigned)(GetRValue(kOrange) << 16 | GetGValue(kOrange) << 8 | GetBValue(kOrange)), s_support,
                 s_unitId);
        PlugEqTell();   // (the fine equalizer's gains: an add-on that plays sound filters it the same)
        if (s_screen == S_SET_PLUGINS && IsWindowVisible(s_wnd))
            Redraw();
        return;
    }
    Plugin* p = PlugByWnd(from);
    if (!p && VhapField(cmd, L"from", id, 24))
        p = PlugById(id);
    if (!p)
        return;   // not said hello
    if (!wcscmp(verb, L"voice")) {   // an add-on's voice prompt: the radio down while it speaks (as the beep's)
        VhapField(cmd, L"on", v, 4);
        s_plugVoice = v[0] == L'1';
        if (s_plugVoice || !s_voice)
            NaviVoice(s_plugVoice);
    } else if (!wcscmp(verb, L"sound") && VhapField(cmd, L"on", v, 4) && v[0] == L'0') {
        // its music is over (the user 2026-11-21: AA's cable out, "Занято Android Auto" stayed till MODE)
        VLog(L"дополнение %s: своего звука больше нет", p->id);
        if (s_plugSound == (int)(p - s_plug))
            s_plugSound = -1;
        if (s_ap.plug == (int)(p - s_plug))
            s_ap.have = false;
        Redraw();
    } else if (!wcscmp(verb, L"sound")) {   // an add-on's own music starts (Android Auto): ours paused, the MCU's
        VLog(L"дополнение %s: свой звук - наши плееры на паузу, источник медиа", p->id);   // source to media
        OursQuiet();
        MediaSource();
        s_plugSound = (int)(p - s_plug);
        s_soundSrc = s_src;
    } else if (!wcscmp(verb, L"navi")) {   // the add-on takes the navigation / gives it back
        VhapField(cmd, L"on", v, 4);
        int me = (int)(p - s_plug);
        if (v[0] == L'1')
            s_naviPlug = me;
        else if (s_naviPlug == me)
            s_naviPlug = -1;
        VLog(L"навигация: %s %s", p->id, v[0] == L'1' ? L"взяло навигацию" : L"отдало навигацию");
    } else if (!wcscmp(verb, L"playing")) {
        PlugPlaying(p, cmd);
    } else if (!wcscmp(verb, L"nav")) {
        VNavParse(cmd, &s_nav);
        s_navPlug = (int)(p - s_plug);
        if (IsWindowVisible(s_wnd))
            RedrawArea(0, 0, 800, 56);
    } else if (!wcscmp(verb, L"bye")) {
        VLog(L"дополнение: %s отключилось", p->id);
        if (s_navPlug == (int)(p - s_plug))
            s_navPlug = -1;
        if (s_plugSound == (int)(p - s_plug))
            s_plugSound = -1;
        if (s_ap.plug == (int)(p - s_plug))
            s_ap.have = false;
        if (s_naviPlug == (int)(p - s_plug))
            s_naviPlug = -1;
        if (s_plugVoice && !s_voice)   // (gone mid-prompt: the radio back)
            NaviVoice(false);
        s_plugVoice = false;
        for (int k = 0; k < p->nw; k++)
            PlugWidgetFree(p->wg[k]);
        p->hwnd = NULL;
        p->ns = p->nw = 0;
        if (!s_quitting)   // (at the exit: no rescan - it would start the widgets' fetch)
            UserWidgetsScan();
        if (s_screen == S_PLUGIN && s_plugCur == (int)(p - s_plug))
            Back();
        Redraw();
    } else if (!wcscmp(verb, L"widget") || !wcscmp(verb, L"widget.set")) {
        VhapField(cmd, L"id", id, 24);
        int k = 0;
        while (k < p->nw && wcscmp(p->wg[k].id, id))
            k++;
        if (k == p->nw) {
            if (wcscmp(verb, L"widget") || k == kPlugWidgets)
                return;
            memset(&p->wg[k], 0, sizeof(PlugWidget));
            wcscpy(p->wg[k].id, id);
            p->nw++;
            PlugWidgetSet(p->wg[k], cmd);
            if (p->wg[k].image)
                PlugWidgetImage(*p, p->wg[k]);
            UserWidgetsScan();   // a new one: on the list (and in "Widgets of home"), hidden or not as set
            Redraw();
            return;
        }
        PlugWidgetSet(p->wg[k], cmd);
        PlugWidgetCopy(*p, p->wg[k]);
    } else if (!wcscmp(verb, L"widget.frame")) {   // a new picture: only this widget redrawn, when it is in sight
        VhapField(cmd, L"id", id, 24);
        wchar_t key[40];
        _snwprintf(key, 40, L"p:%s.%s", p->id, id);
        key[39] = 0;
        for (int i = 0; i < s_uwCount; i++) {
            RECT r;
            if (!wcscmp(s_uw[i].key, key) && s_screen == S_HOME && IsWindowVisible(s_wnd) &&
                HomeRect(WG_COUNT + i, &r))
                RedrawArea(r.left, r.top, r.right, r.bottom);
        }
    } else if (!wcscmp(verb, L"screen")) {
        VhapField(cmd, L"id", id, 24);
        PlugScreen* s = PlugScreenOf(*p, id);
        if (!s) {
            if (p->ns == kPlugScreens)
                return;
            s = &p->sc[p->ns++];
            memset(s, 0, sizeof(*s));
            wcscpy(s->id, id);
        }
        VhapField(cmd, L"title", s->title, 48);
        VhapField(cmd, L"icon", s->icon, 16);
        if (VhapField(cmd, L"nav", v, 8))
            s->nav = v[0] == L'1';
        if (VhapField(cmd, L"own", v, 8))
            s->own = v[0] == L'1';
        if (s->nav)
            Redraw();   // a new button in the bottom bar
    } else if (!wcscmp(verb, L"window")) {   // the own window of a screen (own=1)
        VhapField(cmd, L"screen", v, 24);
        PlugScreen* s = PlugScreenOf(*p, v);
        if (s && VhapField(cmd, L"hwnd", v, 16))
            s->win = (HWND)wcstoul(v, NULL, 16);
    } else if (!wcscmp(verb, L"back")) {   // its own window closed: VestaHome in front again
        PlugOwnClose();
    } else if (!wcscmp(verb, L"subscribe")) {
        VhapField(cmd, L"what", v, 96);
        p->sub = (wcsstr(v, L"gps") ? 1 : 0) | (wcsstr(v, L"car") ? 2 : 0) | (wcsstr(v, L"media") ? 4 : 0) |
                 (wcsstr(v, L"night") ? 8 : 0) | (wcsstr(v, L"keys") ? 16 : 0) | (wcsstr(v, L"bt") ? 32 : 0);
        p->sub |= 0x100;   // (the first round sends everything)
    } else if (!wcscmp(verb, L"media")) {   // the source playing: toggle / next / prev
        VhapField(cmd, L"cmd", v, 16);
        if (!wcscmp(v, L"next") || !wcscmp(v, L"prev"))
            MediaStep(!wcscmp(v, L"next") ? 1 : -1);
        else if (!wcscmp(v, L"toggle") || !wcscmp(v, L"play") || !wcscmp(v, L"pause"))
            MediaToggle();
    } else if (!wcscmp(verb, L"do")) {   // VestaHome's own action (vhaction.h; VestaSmartHome: the phone's assistant)
        VhapField(cmd, L"name", v, 24);
        VhDo(VhActionName(v), p->name);
        if (IsWindowVisible(s_wnd))
            Redraw();
    } else if (!wcscmp(verb, L"screen.clear")) {
        VhapField(cmd, L"id", id, 24);
        PlugScreen* s = PlugScreenOf(*p, id);
        if (s)
            s->n = 0;
    } else if (!wcscmp(verb, L"item") || !wcscmp(verb, L"item.set")) {
        VhapField(cmd, L"screen", v, 24);
        PlugScreen* s = PlugScreenOf(*p, v);
        if (!s)
            return;
        VhapField(cmd, L"id", id, 24);
        int k = 0;
        while (k < s->n && wcscmp(s->it[k].id, id))
            k++;
        if (k == s->n) {
            if (wcscmp(verb, L"item") || k == kPlugItems)
                return;
            memset(&s->it[k], 0, sizeof(PlugItem));
            wcscpy(s->it[k].id, id);
            s->n++;
        }
        PlugItemSet(s->it[k], cmd);
        if (s_screen == S_PLUGIN && s_plugCur == (int)(p - s_plug) && IsWindowVisible(s_wnd))
            Redraw();
    } else if (!wcscmp(verb, L"notify")) {
        VhapField(cmd, L"text", v, 96);
        _snwprintf(s_status, 96, L"%s", v);
        s_status[95] = 0;
        if (IsWindowVisible(s_wnd))
            Redraw();
    } else if (!wcscmp(verb, L"open")) {
        VhapField(cmd, L"screen", v, 24);
        int s;
        if (PlugScreenOf(*p, v, &s)) {
            ShowWindow(s_wnd, SW_SHOW);
            SetForegroundWindow(s_wnd);
            PlugOpen(*p, s);
        }
    }
}

// Each second (and on leaving the screen): "screen.open" / "screen.close" to the add-on whose screen came / went;
// "widget.show" to the picture widgets that came into sight or left it (no frames drawn for nothing).
static void PlugScreenWatch()
{
    PlugData();
    PlugSoundWatch();
    if (s_plugOwnP >= 0 && GetForegroundWindow() == s_wnd && IsWindowVisible(s_wnd))
        s_plugOwnP = s_plugOwnS = -1;   // VestaHome in front again without "back" (a key, the camera): just forgotten
    for (int i = 0; i < s_uwCount; i++) {
        Plugin* pp = NULL;
        PlugWidget* g = PlugWidgetOf(s_uw[i], &pp);
        if (!g || !g->image || !g->view)
            continue;
        RECT r;
        bool shown = s_screen == S_HOME && VhSeen() &&
                     WgShown(WG_COUNT + i) && HomeRect(WG_COUNT + i, &r);
        if (shown != g->shown) {
            g->shown = shown;
            PlugSend(*pp, L"widget.show	id=%s	value=%d", g->id, shown ? 1 : 0);
        }
    }
    int p = s_screen == S_PLUGIN ? s_plugCur : -1, s = s_screen == S_PLUGIN ? s_plugScr : -1;
    if (p == s_plugOpenP && s == s_plugOpenS)
        return;
    if (s_plugOpenP >= 0 && s_plugOpenP < s_plugN && s_plugOpenS < s_plug[s_plugOpenP].ns)
        PlugSend(s_plug[s_plugOpenP], L"screen.close\tid=%s", s_plug[s_plugOpenP].sc[s_plugOpenS].id);
    s_plugOpenP = p;
    s_plugOpenS = s;
    if (p >= 0 && s >= 0 && s < s_plug[p].ns)
        PlugSend(s_plug[p], L"screen.open\tid=%s", s_plug[p].sc[s].id);
}

// The add-ons of one folder: <Name>\<Name>.exe; the ones not switched off are started, the ones known already passed over.
static void PluginsIn(const wchar_t* dir)
{
    wchar_t pat[MAX_PATH];
    _snwprintf(pat, MAX_PATH, L"%s*", dir);
    WIN32_FIND_DATA fd;
    HANDLE f = FindFirstFile(pat, &fd);
    if (f == INVALID_HANDLE_VALUE)
        return;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || fd.cFileName[0] == L'.' || s_plugN == kPlugMax ||
            PlugById(fd.cFileName))
            continue;
        Plugin& p = s_plug[s_plugN];
        memset(&p, 0, sizeof(p));
        wcsncpy(p.id, fd.cFileName, 23);
        wcscpy(p.name, p.id);
        _snwprintf(p.dir, MAX_PATH, L"%s%s\\", dir, fd.cFileName);
        wchar_t exe[MAX_PATH], off[MAX_PATH];
        _snwprintf(exe, MAX_PATH, L"%s%s.exe", p.dir, fd.cFileName);
        if (GetFileAttributes(exe) == 0xFFFFFFFF)
            continue;
#ifdef UNDER_CE
        if (!PlugVolumeOk(exe)) {   // (plugvol.h: taken out, the add-on would die with its card)
            VLog(L"дополнение: %s на съёмном носителе - не запускаю (только \\ResidentFlash или \\SDMMC)", exe);
            _snwprintf(s_status, 96, L"%s на карте не запускается: перенесите в \\ResidentFlash", p.id);
            s_status[95] = 0;
            continue;
        }
#endif
        s_plugN++;
        _snwprintf(off, MAX_PATH, L"%sdisabled", p.dir);
        p.off = GetFileAttributes(off) != 0xFFFFFFFF;
        if (p.off)
            continue;
        PROCESS_INFORMATION pi;
#ifdef UNDER_CE
        bool ok = CreateProcess(exe, NULL, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi) != 0;
#else
        STARTUPINFO si;
        memset(&si, 0, sizeof(si));
        si.cb = sizeof(si);
        bool ok = CreateProcess(exe, NULL, NULL, NULL, FALSE, 0, NULL, p.dir, &si, &pi) != 0;
#endif
        if (ok) {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
        }
        VLog(L"дополнение: %s %s", exe, ok ? L"запущено" : L"не запустилось");
    } while (FindNextFile(f, &fd));
    FindClose(f);
}

// VHContent\VHPlugins on the partition, then on the built-in card (\SDMMC); the SD card's only to say they don't start
// from there (plugvol.h). Again when a card comes or goes.
static void PluginsStart()
{
    wchar_t dirs[kContentDirs + 1][MAX_PATH];
    int n = VhContentDirs(L"VHPlugins", dirs);
#ifdef UNDER_CE
    wcscpy(dirs[n++], dirs[1]);   // (the SD card's last)
    wcscpy(dirs[1], L"\\SDMMC\\VHContent\\VHPlugins\\");
#endif
    for (int i = 0; i < n; i++)
        PluginsIn(dirs[i]);
}

// VestaHome closes: the add-ons too - asked, waited for 3 s, the ones still there ended (the user 2026-11-18: an add-on
// hung stayed after the exit, its exe could not be replaced by an update until the unit was rebooted).
static void PluginsQuit()
{
    for (int i = 0; i < s_plugN; i++)
        PlugSend(s_plug[i], L"quit");
    DWORD t0 = GetTickCount();
    for (bool left = true; left && GetTickCount() - t0 < 3000;) {
        left = false;
        for (int i = 0; i < s_plugN; i++)
            left = left || (s_plug[i].hwnd && IsWindow(s_plug[i].hwnd));
        if (left)
            Sleep(100);
    }
    for (int i = 0; i < s_plugN; i++) {
        HWND h = s_plug[i].hwnd;
        DWORD pid = 0;
        if (!h || !IsWindow(h) || !GetWindowThreadProcessId(h, &pid) || !pid)
            continue;
#ifdef UNDER_CE
        HANDLE p = OpenProcess(0, FALSE, pid);   // (CE: no access rights - 0, as vh_apps.inc)
#else
        HANDLE p = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
#endif
        bool ok = p && TerminateProcess(p, 1);
        if (p)
            CloseHandle(p);
        VLog(L"выход: дополнение %s не закрылось за 3 с - %s", s_plug[i].id, ok ? L"завершено" : L"не завершается");
    }
}

static void PlugSwitch(int i)   // "Add-ons": on / off (the file "disabled" in its folder), started at once when on
{
    if (i < 0 || i >= s_plugN || !s_plug[i].dir[0])
        return;
    Plugin& p = s_plug[i];
    wchar_t off[MAX_PATH];
    _snwprintf(off, MAX_PATH, L"%sdisabled", p.dir);
    if (p.off) {
        DeleteFile(off);
        p.off = false;
        wchar_t exe[MAX_PATH];
        _snwprintf(exe, MAX_PATH, L"%s%s.exe", p.dir, p.id);
        PROCESS_INFORMATION pi;
        if (CreateProcess(exe, NULL, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi)) {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
        }
    } else {
        HANDLE h = CreateFile(off, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        if (h != INVALID_HANDLE_VALUE)
            CloseHandle(h);
        p.off = true;
        PlugSend(p, L"quit");
    }
    VLog(L"дополнение: %s %s", p.id, p.off ? L"выключено" : L"включено");
}

// Settings -> General -> Add-ons: each one found or connected, its state; on / off, "Открыть" its first screen.
static void ScrSetPlugins(HDC dc)
{
    Settings(dc, S_SET_PLUGINS);
    if (!s_plugN) {
        EmptyList(dc, 264, 100, 500, L"Дополнений нет", L"программы дополнений — в VHContent\\VHPlugins на \\ResidentFlash или \\SDMMC");
        return;
    }
    for (int i = 0; i < s_plugN && i < 6; i++) {
        Plugin& p = s_plug[i];
        int y = 64 + i * 56;
        wchar_t sub[96];
        _snwprintf(sub, 96, L"%s%s%s", p.ver[0] ? p.ver : L"", p.ver[0] ? L" · " : L"",
                   p.hwnd ? L"подключено" : p.off ? L"выключено" : L"не подключено");
        SetRow(dc, y, p.name, sub, i == s_plugN - 1);
        if (p.hwnd && p.ns)
            Button(dc, 556, y + 8, 116, 40, L"Открыть", B_N, IC_NONE, 14, 0, A_PLUGOPEN, i);
        if (p.dir[0]) {   // the switch (s_on is for the fixed ones: drawn here)
            bool on = !p.off != Zone(678, y + 2, 92, 52, A_PLUGON, i);
            R(dc, 688, y + 10, 72, 36, 18, on ? kOrange : kPanel2);
            Disc(dc, on ? 742 : 706, y + 28, on ? 14 : 13, on ? kInk : kDim);
        }
    }
}

// An add-on's described screen: its items from the top, 56 px each, kPlugRows a page; the taps go back as events.
// arg of A_PLUGEV: item * 64 + what (0 tap, 1 toggle, 2 step -1, 3 step +1, 8 + n the n-th option).
static void ScrPlugin(HDC dc)
{
    if (s_plugCur < 0 || s_plugCur >= s_plugN || s_plugScr < 0 || s_plugScr >= s_plug[s_plugCur].ns) {
        Frame(dc, L"Дополнение");
        T(dc, 400, 240, L"Дополнение отключилось", 20, kMuted, 600, AL_MID);
        return;
    }
    Plugin& p = s_plug[s_plugCur];
    PlugScreen& sc = p.sc[s_plugScr];
    Frame(dc, sc.title[0] ? sc.title : p.name);
    Panel(dc, 16, 64, 768, 344);
    int pages = sc.n ? (sc.n + kPlugRows - 1) / kPlugRows : 1;
    if (s_plugPage >= pages)
        s_plugPage = pages - 1;
    for (int r = 0; r < kPlugRows; r++) {
        int i = s_plugPage * kPlugRows + r, y = 68 + r * 56;
        if (i >= sc.n)
            break;
        PlugItem& it = sc.it[i];
        bool last = r == kPlugRows - 1 || i == sc.n - 1;
        const wchar_t* sub = it.sub[0] ? it.sub : NULL;
        if (!wcscmp(it.type, L"button")) {
            Button(dc, 32, y + 6, 736, 44, it.label, B_N, IC_NONE, 17, 0, A_PLUGEV, i * 64);
        } else if (!wcscmp(it.type, L"toggle")) {
            WRow(dc, y, it.label, sub, A_NONE, 0, 640, last);
            bool on = (it.value[0] == L'1') != Zone(678, y + 2, 92, 52, A_PLUGEV, i * 64 + 1);
            R(dc, 688, y + 10, 72, 36, 18, on ? kOrange : kPanel2);
            Disc(dc, on ? 742 : 706, y + 28, on ? 14 : 13, on ? kInk : kDim);
        } else if (!wcscmp(it.type, L"seg")) {
            WRow(dc, y, it.label, sub, A_NONE, 0, 340, last);
            const wchar_t* opt[6];
            wchar_t buf[64];
            wcscpy(buf, it.options);
            int n = 0;
            for (wchar_t* t = buf; *t && n < 6;) {
                opt[n++] = t;
                wchar_t* bar = wcschr(t, L'|');
                if (!bar)
                    break;
                *bar = 0;
                t = bar + 1;
            }
            int cur = _wtoi(it.value), w = 416, cw = n ? (w - 4) / n : w;
            R(dc, 352, y + 6, w, 44, 22, kPanel2);
            for (int k = 0; k < n; k++) {
                int cx = 354 + k * cw;
                bool on = Zone(cx, y + 6, cw, 44, A_PLUGEV, i * 64 + 8 + k) || k == cur;
                if (on)
                    R(dc, cx, y + 8, cw, 40, 20, kOrange);
                T(dc, cx + cw / 2, y + 34, opt[k], 14, on ? kInk : kMuted, 700, AL_MID, cw - 6);
            }
        } else if (!wcscmp(it.type, L"stepper")) {
            WRow(dc, y, it.label, sub, A_NONE, 0, 540, last);
            Button(dc, 556, y + 8, 48, 40, NULL, B_N, IC_BACK, 18, 28, A_PLUGEV, i * 64 + 2);
            T(dc, 662, y + 35, it.value, 19, kText, 700, AL_MID, 100);
            Button(dc, 720, y + 8, 48, 40, NULL, B_N, IC_NEXT, 18, 28, A_PLUGEV, i * 64 + 3);
        } else if (!wcscmp(it.type, L"row")) {
            WRow(dc, y, it.label, sub, A_PLUGEV, i * 64, 776, last);
        } else if (!wcscmp(it.type, L"text")) {   // a tap: the keyboard; a secret shows only whether it is set
            WRow(dc, y, it.label, sub, A_PLUGEV, i * 64 + 4, 776, last);
            const wchar_t* shown = it.secret ? (it.value[0] ? L"••••••" : L"не задан") : it.value[0] ? it.value : L"—";
            T(dc, 732, y + 35, shown, 19, it.value[0] ? kText : kMuted, 700, AL_END, 380);   // (left of the row's arrow)
        } else {   // "value" (and anything unknown: shown as a value)
            WRow(dc, y, it.label, sub, A_NONE, 0, 520, last);
            T(dc, 760, y + 35, it.value, 19, kText, 700, AL_END, 240);
        }
    }
    if (pages > 1) {
        wchar_t s[16];
        _snwprintf(s, 16, L"%d / %d", s_plugPage + 1, pages);
        Button(dc, 600, 414, 56, 52, NULL, s_plugPage ? B_N : B_DIS, IC_BACK, 15, 26, A_PLUGPAGE, -1);
        T(dc, 692, 446, s, 15, kMuted, 600, AL_MID);
        Button(dc, 728, 414, 56, 52, NULL, s_plugPage + 1 < pages ? B_N : B_DIS, IC_NEXT, 15, 26, A_PLUGPAGE, 1);
    }
}

// A text field (type=text): typed on VestaHome's keyboard, sent as "event act=text value=..." on "Done" (nothing on
// "Cancel"). A secret one starts empty (its value is never shown, only whether one is set) and is typed as dots.
static int     s_plugTextP;
static wchar_t s_plugTextScr[24], s_plugTextId[24];

static void PlugTextOpen(const PlugScreen& sc, const PlugItem& it)
{
    s_plugTextP = s_plugCur;
    wcscpy(s_plugTextScr, sc.id);
    wcscpy(s_plugTextId, it.id);
    wcscpy(s_plugText, it.secret ? L"" : it.value);
    OpenKeysFor(s_plugText, 63, it.label, S_PLUGIN);
    s_keysSecret = it.secret;
}

static void PlugTextDone()
{
    if (s_plugTextP < 0 || s_plugTextP >= s_plugN)
        return;
    Plugin& p = s_plug[s_plugTextP];
    PlugScreen* sc = PlugScreenOf(p, s_plugTextScr);
    for (int i = 0; sc && i < sc->n; i++)
        if (!wcscmp(sc->it[i].id, s_plugTextId)) {
            PlugItem& it = sc->it[i];
            if (it.secret)
                wcscpy(it.value, s_plugText[0] ? L"1" : L"");
            else
                wcsncpy(it.value, s_plugText, 47);   // (shown at once; the add-on may set it back)
        }
    PlugSend(p, L"event\tscreen=%s\tid=%s\tact=text\tvalue=%s", s_plugTextScr, s_plugTextId, s_plugText);
    memset(s_plugText, 0, sizeof(s_plugText));
    memset(s_keys.text, 0, sizeof(s_keys.text));   // (a password not left in memory)
}

static void PlugEvent(int arg)   // a tap on an item of the screen shown
{
    if (s_plugCur < 0 || s_plugCur >= s_plugN || s_plugScr < 0 || s_plugScr >= s_plug[s_plugCur].ns)
        return;
    Plugin& p = s_plug[s_plugCur];
    PlugScreen& sc = p.sc[s_plugScr];
    int i = arg / 64, what = arg % 64;
    if (i >= sc.n)
        return;
    PlugItem& it = sc.it[i];
    if (what == 1) {   // the switch flips at once; the add-on may set it back
        it.value[0] = it.value[0] == L'1' ? L'0' : L'1';
        it.value[1] = 0;
        PlugSend(p, L"event\tscreen=%s\tid=%s\tact=toggle\tvalue=%c", sc.id, it.id, it.value[0]);
    } else if (what == 4) {
        PlugTextOpen(sc, it);
    } else if (what == 2 || what == 3) {
        PlugSend(p, L"event\tscreen=%s\tid=%s\tact=step\tvalue=%s", sc.id, it.id, what == 2 ? L"-1" : L"+1");
    } else if (what >= 8) {
        _snwprintf(it.value, 48, L"%d", what - 8);
        PlugSend(p, L"event\tscreen=%s\tid=%s\tact=seg\tvalue=%d", sc.id, it.id, what - 8);
    } else if (it.link[0]) {
        int s;
        if (PlugScreenOf(p, it.link, &s))
            PlugOpen(p, s);
        PlugSend(p, L"event\tscreen=%s\tid=%s\tact=tap", sc.id, it.id);
    } else {
        PlugSend(p, L"event\tscreen=%s\tid=%s\tact=tap", sc.id, it.id);
    }
}
