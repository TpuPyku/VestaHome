// vh_netcat.inc - part of home.cpp (included there, in order): the internet radio's catalogue, radio-browser.info
// (the user 2026-11-04, design netradio-v0.7: categories -> a list of them -> a page of kNcPage stations, as a playlist).
// Its requests run in a thread of their own (VestaCore vhttp, the API's two servers in turn); the answers are read
// there (netcat.h) and handed over by WM_APP_NETCAT. In memory: the open page (s_nl[NL_CAT]) and the category list.
#define WM_APP_NETCAT (WM_APP + 30)

enum { NCJ_CATS, NCJ_PAGE, NCJ_CLICK };
struct NcJob {
    LONG      gen;
    int       what, kind;     // NCJ_*, NC_TAGS.. / NC_TOP..
    char      path[400];
    int       n;              // the answer's items read (the playable ones)
    int       raw;            // the answer's items (a full page: there may be more)
    NcCat*    cats;
    NcStation* st;
    int       err;            // NET_* or -1 for an HTTP status other than 200
};

static const char* const kNcHosts[2] = { "de1.api.radio-browser.info", "de2.api.radio-browser.info" };
static int     s_ncHost;               // the server that answered last
static LONG    s_ncGen;                // the latest request: an older answer is thrown away
static bool    s_ncBusy;
static wchar_t s_ncMsg[96];            // the error of the last request, "" none
static NcCat*  s_ncCats;               // the category list shown (S_NETCATS)
static int     s_ncCatN, s_ncCatKind = -1;
struct NcPage { int kind; char arg[200]; wchar_t title[48]; int offset, total, raw; bool byName; };
static NcPage  s_ncPage;               // what s_nl[NL_CAT] holds
static wchar_t s_ncQuery[64];          // the search's text (the keyboard's: VestaCore vkeys)

static DWORD WINAPI NcThread(LPVOID p)
{
    NcJob* j = (NcJob*)p;
    unsigned char* body = NULL;
    int len = 0, status = 0;
    j->err = NET_CONNECT;
    // this server, then the other; HTTPS, then plain HTTP (2026-11-04: a provider resets TLS to radio-browser now
    // and then, both servers at once - its plain HTTP answered; the catalogue is public, no secret goes there)
    for (int k = 0; k < 4; k++) {
        if (j->what != NCJ_CLICK && j->gen != s_ncGen)   // another asked meanwhile: no more tries on the link
            break;                                        // (05.11: two at once over BT, each up to 4 x 16 s)
        int h = (s_ncHost + k) % 2;
        DWORD t0 = GetTickCount();
        j->err = HttpGet(kNcHosts[h], k < 2, j->path, &body, &len, &status);
        // each try into the log (05.11: one line after 63 s, the link ~150 bytes/s - which try failed how)
        VLog(L"каталог радио: %S %s - ошибка %d, ответ %d, %d байт за %u мс", kNcHosts[h], k < 2 ? L"HTTPS" : L"HTTP",
             j->err, status, body ? len : 0, (unsigned)(GetTickCount() - t0));
        if (j->err == NET_OK && status == 200) {
            s_ncHost = h;
            break;
        }
        free(body);
        body = NULL;
        if (j->err == NET_OK)
            j->err = -1;
    }
    if (j->what == NCJ_CLICK) {   // (only counted: the address we have)
        free(body);
        free(j);
        return 0;
    }
    if (body) {
        if (j->what == NCJ_CATS) {
            j->cats = (NcCat*)malloc(300 * sizeof(NcCat));
            j->n = j->cats ? NcCats((const char*)body, j->kind, j->cats, 300) : 0;
        } else {
            j->st = (NcStation*)malloc(kNcPage * sizeof(NcStation));
            j->n = j->st ? NcStations((const char*)body, j->st, kNcPage) : 0;
            for (const char* it = JFirst((const char*)body); it; it = JNext(it))
                j->raw++;
        }
        free(body);
    }
    if (!PostMessage(s_wnd, WM_APP_NETCAT, 0, (LPARAM)j)) {
        free(j->cats);
        free(j->st);
        free(j);
    }
    return 0;
}

static void NcRequest(int what, int kind, const char* path)
{
    NcJob* j = (NcJob*)calloc(1, sizeof(NcJob));
    if (!j)
        return;
    j->what = what;
    j->kind = kind;
    strncpy(j->path, path, sizeof(j->path) - 1);
    if (what != NCJ_CLICK) {
        j->gen = InterlockedIncrement(&s_ncGen);
        s_ncBusy = true;
        s_ncMsg[0] = 0;
    }
    VLog(L"каталог радио: %S", path);
    HANDLE t = CreateThread(NULL, 0, NcThread, j, 0, NULL);
    if (t)
        CloseHandle(t);
    else
        free(j);
}

static void NetCatClick(const char* uuid)   // radio-browser counts a station played (its rules ask for it)
{
    char path[80];
    NcClickPath(uuid, path, sizeof(path));
    NcRequest(NCJ_CLICK, 0, path);
}

static void NcOpenCats(int kind)   // Genres / Countries / Languages: the list, asked again when it is not this one
{
    if (s_ncCatKind != kind) {
        free(s_ncCats);
        s_ncCats = NULL;
        s_ncCatN = 0;
        s_ncCatKind = kind;
        char path[200];
        NcCatsPath(kind, path, sizeof(path));
        NcRequest(NCJ_CATS, kind, path);
    }
    Go(S_NETCATS, true);
}

static void NcAskPage()
{
    char path[400];
    NcStationsPath(s_ncPage.kind, s_ncPage.arg, s_ncPage.offset, s_ncPage.byName, path, sizeof(path));
    s_nl[NL_CAT].n = 0;
    s_ncPage.raw = 0;
    NcRequest(NCJ_PAGE, s_ncPage.kind, path);
}

// A page of stations: what (NC_TOP.., its key), its title for the screen, its station count (-1 unknown).
static void NcOpenPage(int kind, const char* arg, const wchar_t* title, int total)
{
    NetLoadList();
    memset(&s_ncPage, 0, sizeof(s_ncPage));
    s_ncPage.kind = kind;
    if (arg)
        strncpy(s_ncPage.arg, arg, sizeof(s_ncPage.arg) - 1);
    wcsncpy(s_ncPage.title, title, 47);
    s_ncPage.total = total;
    NcAskPage();
    s_netView = NL_CAT;
    if (s_screen == S_NETRADIO_LIST)   // (a search from the list: in place, not twice in the history)
        s_screenTick = GetTickCount();
    else
        Go(S_NETRADIO_LIST, true);
}

static void NcStepPage(int dir)   // "Ещё 20" / back by kNcPage
{
    int off = s_ncPage.offset + dir * kNcPage;
    if (off < 0 || (dir > 0 && s_ncPage.raw < kNcPage))
        return;
    s_ncPage.offset = off;
    NcAskPage();
}

static void OpenKeysFor(wchar_t* target, int cap, const wchar_t* title, Screen back);   // (vh_apps.inc)

static void NcSearchOpen()   // "Поиск": the keyboard, then the stations whose names have the text
{
    OpenKeysFor(s_ncQuery, 63, L"Поиск станции по названию", s_screen);
}

static void NcSearchDone()   // (CfgKeysDone: the keyboard closed on the search's text)
{
    if (!s_ncQuery[0])
        return;
    char q[200];
    WideCharToMultiByte(CP_UTF8, 0, s_ncQuery, -1, q, sizeof(q), NULL, NULL);
    wchar_t t[72];
    _snwprintf(t, 72, L"«%s»", s_ncQuery);
    t[71] = 0;
    NcOpenPage(NC_SEARCH, q, t, -1);
}

static void NcToggleSort()
{
    s_ncPage.byName = !s_ncPage.byName;
    s_ncPage.offset = 0;
    NcAskPage();
}

// An answer in the window's thread.
static void OnNetCat(NcJob* j)
{
    if (j->gen == s_ncGen) {
        s_ncBusy = false;
        if (j->err != NET_OK)
            _snwprintf(s_ncMsg, 96, L"Каталог недоступен: %s", j->err > 0 ? NetErrorText(j->err) : L"ошибка сервера");
        if (j->what == NCJ_CATS && j->kind == s_ncCatKind) {
            free(s_ncCats);
            s_ncCats = j->cats;
            s_ncCatN = j->n;
            j->cats = NULL;
        } else if (j->what == NCJ_PAGE && s_nl[NL_CAT].st) {
            memcpy(s_nl[NL_CAT].st, j->st, j->n * sizeof(NcStation));
            s_nl[NL_CAT].n = j->n;
            s_ncPage.raw = j->raw;
            if (s_netWhich == NL_CAT && s_netNow.url[0])
                s_netCur = NetFind(NL_CAT, s_netNow.url);
        }
        VLog(L"каталог радио: %d (%s)", j->n, j->err == NET_OK ? L"ok" : s_ncMsg);
    }
    free(j->cats);
    free(j->st);
    free(j);
}

// ---- the screens
static void NetListTabs(HDC dc, int active)   // Мои станции · Избранное · Недавние · Каталог, as playlists
{
    wchar_t a[32], b[32];
    _snwprintf(a, 32, L"Мои станции %d", s_nl[NL_MINE].n);
    _snwprintf(b, 32, L"Избранное %d", s_nl[NL_FAV].n);
    const wchar_t* label[4] = { a, b, L"Недавние", L"Каталог" };
    static const Icon ic[4] = { IC_RADIO, IC_STAR_F, IC_CLOCK, IC_NETRADIO };
    static const int which[4] = { NL_MINE, NL_FAV, NL_RECENT, NL_CAT };
    for (int i = 0, x = 16; i < 4; i++) {
        int w = TextWidth(dc, label[i], 16, 700) + 56;
        Button(dc, x, 62, w, 44, label[i], which[i] == active ? B_SEL : B_N, ic[i], 16, 20, A_NRLIST, which[i]);
        x += w + 8;
    }
    Button(dc, 652, 62, 132, 44, L"Поиск", B_N, IC_SEARCH, 16, 20, A_NCSEARCH);
}

static const wchar_t* const kNcTile[5][2] = {
    { L"Популярные", L"чаще всего слушают" }, { L"СНГ", L"Россия, Беларусь…" },
    { L"Жанры", L"поп, рок, джаз, новости…" }, { L"Страны", L"по числу станций" },
    { L"Языки", L"русский, английский…" }
};
static const Icon kNcTileIc[5] = { IC_STAR, IC_HOME, IC_MUSIC, IC_NETRADIO, IC_USERS };

static char s_ncNear[3];   // the country by GPS ("" none - no fix, the sea): the second tile's

static void NcNearNow()   // (the user 2026-11-04: "Россия" was always Russia - the country where the car is)
{
    Gps g;
    GpsGet(&g);
    if (!GpsFresh(g, 120000) || !CountryAt(g.lat, g.lon, s_ncNear))
        s_ncNear[0] = 0;
}

// The catalogue's ways in. The second: the stations of the country by GPS straight away (the user 2026-11-05: not
// through a list of one country), without a fix the CIS countries.
static void ScrNetCat(HDC dc)
{
    static DWORD s_seen;
    if (s_seen != s_screenTick) {
        s_seen = s_screenTick;
        NcNearNow();
    }
    wchar_t here[48];
    here[0] = 0;
    if (s_ncNear[0])
        NcName(NC_COUNTRIES, s_ncNear, s_ncNear, here, 48);
    NetLoadList();
    Frame(dc, L"Интернет-радио · Каталог");
    NetListTabs(dc, NL_CAT);
    for (int i = 0; i < 5; i++) {
        int x = 16 + (i % 3) * 260, y = 120 + (i / 3) * 132, w = 248, h = 120;
        bool down = Zone(x, y, w, h, A_NCTILE, i);
        R(dc, x, y, w, h, 18, down ? kOrange : kPanel);
        bool gps = i == 1 && here[0];
        Ico(dc, gps ? IC_NAV : kNcTileIc[i], x + 18, y + 16, 28, down ? kInk : kOrange);
        T(dc, x + 18, y + 72, gps ? here : kNcTile[i][0], 19, down ? kInk : kText, 700, AL_START, w - 36);
        T(dc, x + 18, y + 98, gps ? L"рядом, по GPS" : kNcTile[i][1], 14, down ? kInk : kMuted, 400, AL_START, w - 36);
    }
    T(dc, 24, 404, s_ncMsg[0] ? s_ncMsg : L"radio-browser.info · свободный каталог, ~50 000 станций", 13,
      s_ncMsg[0] ? kOrange : kDim, 400);
    BackButton(dc);
}

static void NcTile(int i)
{
    wchar_t nm[48];
    if (i == 0)
        NcOpenPage(NC_TOP, NULL, kNcTile[0][0], -1);
    else if (i == 1 && s_ncNear[0]) {
        NcName(NC_COUNTRIES, s_ncNear, s_ncNear, nm, 48);
        NcOpenPage(NC_COUNTRY, s_ncNear, nm, -1);
    } else
        NcOpenCats(i == 1 ? NC_CIS : i == 2 ? NC_TAGS : i == 3 ? NC_COUNTRIES : NC_LANGS);
}

static Scroll s_ncList = { 0, 0, 5 * 62 };

static void ScrNetCats(HDC dc)   // the genres, the countries or the languages, the most stations first
{
    static const wchar_t* kTitle[4] = { L"Жанры", L"Страны", L"Языки", L"СНГ" };   // (by NC_TAGS..)
    wchar_t s[80];
    _snwprintf(s, 80, L"Каталог › %s", kTitle[s_ncCatKind >= 0 ? s_ncCatKind : 0]);
    Frame(dc, s);
    static DWORD s_seen;
    if (s_seen != s_screenTick) {
        s_seen = s_screenTick;
        s_ncList.pos = 0;
    }
    Panel(dc, 16, 68, 704, 322);
    s_ncList.content = s_ncCatN > 0 ? s_ncCatN * 62 + 2 : 0;
    ScrollBegin(dc, s_ncList, 20, 72, 716, 386);
    for (int i = 0; i < s_ncCatN; i++) {
        int y = 76 + i * 62 - s_ncList.pos;
        if (y + 56 <= 72 || y >= 386)
            continue;
        wchar_t n[16];
        _snwprintf(n, 16, L"%d", s_ncCats[i].count);
        ListRow(dc, 24, y, 680, 56, s_ncCats[i].name, n, L_N, IC_NONE, NULL, NULL, A_NCPICK, i);
    }
    ScrollEnd(dc, s_ncList, 728, 68, 56, 322, 96);
    if (!s_ncCatN)
        T(dc, 40, 114, s_ncBusy ? L"Загрузка…" : s_ncMsg[0] ? s_ncMsg : L"Пусто", 18, s_ncMsg[0] ? kOrange : kMuted, 400);
    BackButton(dc);
}

static void NcPick(int i)
{
    if (i < 0 || i >= s_ncCatN)
        return;
    const NcCat& c = s_ncCats[i];
    NcOpenPage(s_ncCatKind == NC_COUNTRIES || s_ncCatKind == NC_CIS ? NC_COUNTRY : s_ncCatKind == NC_LANGS ? NC_LANG : NC_TAG, c.key, c.name,
               c.count);
}

// Media -> internet radio (design netradio-v0.7): the list it plays from (a tap - that list), the station with
// its star, the song, the country and the stream; previous / on-off / next in that list.
static const wchar_t* const kNetListName[kNetLists] = { L"Мои станции", L"Избранное", L"Каталог", L"Недавние" };

static void ScrNetRadio(HDC dc)
{
    NetLoadList();
    Frame(dc, L"Медиа · Интернет-радио");
    MediaTabs(dc, S_NETRADIO, IC_LIST, A_NRLIST, s_netWhich);
    Cover(dc, IC_NETRADIO, 16, 124, 232);
    wchar_t s[160], title[160];
    const NcStation* st = NetCur();
    if (!st) {
        bool none = !s_nl[NL_MINE].n && !s_nl[NL_FAV].n;
        T(dc, 272, 170, none ? L"Нет станций" : L"Выберите станцию", 22, kText, 700, AL_START, 512);
        if (none) {
            _snwprintf(s, 160, L"нет %sradio.txt — или «Каталог» в списке", s_dir);
            s[159] = 0;
            T(dc, 272, 200, s, 15, kMuted, 400, AL_START, 512);
        } else {
            T(dc, 272, 200, L"Кнопка списка вверху или ▶ — первая станция", 16, kMuted, 400, AL_START, 512);
        }
    } else {
        const wchar_t* list = kNetListName[s_netWhich];
        if (s_netWhich == NL_CAT && s_ncPage.title[0])
            list = s_ncPage.title;
        if (s_netCur >= 0)
            _snwprintf(s, 160, L"%s · %d из %d", list, s_netCur + 1, s_nl[s_netWhich].n);
        else
            _snwprintf(s, 160, L"%s", list);
        s[159] = 0;
        int cw = TextWidth(dc, s, 13, 700) + 44;
        if (cw > 500)
            cw = 500;
        Button(dc, 272, 128, cw, 34, s, B_SEL, IC_NONE, 13, 0, A_NRLIST, s_netWhich);
        int nw = TextWidth(dc, st->name, 34, 700);
        if (nw > 440)
            nw = 440;
        T(dc, 272, 206, st->name, 34, kText, 700, AL_START, 440);
        bool fav = NetIsFav(*st);
        IconBtn(dc, fav ? IC_STAR_F : IC_STAR, 272 + nw + 14, 178, 30, fav ? kOrange : kMuted, A_NRFAV, -1);
        EnterCriticalSection(&s_netCs);
        wcscpy(title, s_netTitle);
        LeaveCriticalSection(&s_netCs);
        if (s_netOn && title[0])
            T(dc, 272, 234, title, 18, kMuted, 400, AL_START, 512);
        if (st->sub[0])
            T(dc, 272, 256, st->sub, 14, kDim, 400, AL_START, 512);
        NetStateText(s, 160);
        T(dc, 272, 284, s, 15, s_netState == NR_PLAY ? kOrange : kMuted, 600, AL_START, 512);
    }
    IconBtn(dc, IC_PREV, 392, 312, 48, kText, A_NRSTEP, -1);
    NetPlayBtn(dc, 528, 336, 42, 48);
    IconBtn(dc, IC_FWD, 616, 312, 48, kText, A_NRSTEP, 1);
}

// A list of stations (ours, the favourites, a catalogue page) that scrolls; a star on each row; the station
// playing orange. The catalogue's page: its place instead of the tabs, "Ещё 20" and the sort below.
enum { kNetPitch = 62 };
static Scroll s_netList = { 0, 0, kNetRows * kNetPitch };

static void NetListOpenAt(int top, int bottom)   // opened: the station playing in view
{
    static DWORD s_seen;
    if (s_seen == s_screenTick)
        return;
    s_seen = s_screenTick;
    int at = s_netWhich == s_netView ? s_netCur : -1;
    s_netList.pos = 0;
    if (at >= 0 && at * kNetPitch + kNetPitch > bottom - top)
        s_netList.pos = (at - 2) * kNetPitch;
}

static void ScrNetRadioList(HDC dc)
{
    NetLoadList();
    const NetList& l = s_nl[s_netView];
    bool cat = s_netView == NL_CAT;
    int top = 116, bottom = 392;
    NetListOpenAt(top, bottom);
    wchar_t s[120];
    _snwprintf(s, 120, L"Интернет-радио · %s", cat ? L"Каталог" : kNetListName[s_netView]);
    Frame(dc, s);
    if (cat) {   // "Каталог" - its ways in, with the lists' tabs (or a station of the catalogue playing hides them)
        int bw = TextWidth(dc, L"Каталог", 16, 700) + 56;
        Button(dc, 16, 62, bw, 44, L"Каталог", B_N, IC_NETRADIO, 16, 20, A_NRLIST, NL_CAT);
        _snwprintf(s, 120, L"› %s", s_ncPage.title);
        T(dc, 16 + bw + 12, 94, s, 16, kText, 700, AL_START, 600 - bw);
        if (l.n) {
            if (s_ncPage.total > 0)
                _snwprintf(s, 120, L"%d–%d из %d", s_ncPage.offset + 1, s_ncPage.offset + s_ncPage.raw, s_ncPage.total);
            else
                _snwprintf(s, 120, L"%d–%d", s_ncPage.offset + 1, s_ncPage.offset + s_ncPage.raw);
            T(dc, 776, 94, s, 14, kMuted, 600, AL_END);
        }
    } else {
        NetListTabs(dc, s_netView);
    }
    Panel(dc, 16, top - 4, 704, bottom - top + 8);
    s_netList.content = l.n > 0 ? l.n * kNetPitch + 2 : 0;
    ScrollBegin(dc, s_netList, 20, top, 716, bottom);
    for (int i = 0; i < l.n; i++) {
        int y = top + 4 + i * kNetPitch - s_netList.pos;
        if (y + 56 <= top || y >= bottom)
            continue;
        bool cur = s_netView == s_netWhich && i == s_netCur;
        ListRow(dc, 24, y, 620, 56, l.st[i].name, NULL, cur ? L_CUR : L_N, IC_RADIO, l.st[i].sub[0] ? l.st[i].sub : NULL,
                NULL, A_NRPICK, i);
        bool fav = s_netView == NL_FAV || NetIsFav(l.st[i]);
        IconBtn(dc, fav ? IC_STAR_F : IC_STAR, 660, y + 14, 28, fav ? kOrange : kMuted, A_NRFAV, i);
    }
    ScrollEnd(dc, s_netList, 728, top - 4, 56, bottom - top + 8, 96);
    if (!l.n) {
        const wchar_t* why = cat ? (s_ncBusy ? L"Загрузка…" : s_ncMsg[0] ? s_ncMsg : L"Пусто")
                           : s_netView == NL_FAV    ? L"Пусто: звёздочка у станции добавляет её сюда"
                           : s_netView == NL_RECENT ? L"Пусто: здесь будут станции, которые вы включали"
                                                    : L"Нет станций: radio.txt рядом с VestaHome";
        T(dc, 40, top + 44, why, 17, cat && s_ncMsg[0] ? kOrange : kMuted, 400, AL_START, 660);
    }
    Dock(dc);
    if (cat) {
        if (s_ncPage.offset > 0)
            Button(dc, 330, 412, 120, 52, L"Назад 50", B_N, IC_NONE, 15, 0, A_NCMORE, -1);
        if (s_ncPage.raw >= kNcPage)
            Button(dc, 460, 412, 120, 52, L"Ещё 20", B_N, IC_NONE, 15, 0, A_NCMORE, 1);
        Button(dc, 590, 412, 194, 52, s_ncPage.byName ? L"Сорт.: название" : L"Сорт.: популярные", B_N, IC_NONE, 15,
               0, A_NCSORT);
    } else {
        IconBtn(dc, IC_RADIO, 600, 417, 44, kText, A_BACK);
        IconBtn(dc, IC_GEAR, 712, 417, 44, kText, A_SCREEN, S_SET_SOUND);
    }
}

// The list action: a list's tab (the catalogue: its ways in), or the player's line of its list.
static void NetListAction(int which)
{
    NetLoadList();
    if (which == NL_CAT && (s_screen != S_NETRADIO || !s_nl[NL_CAT].n)) {
        Go(S_NETCAT, s_screen == S_NETRADIO);
        return;
    }
    s_netView = which;
    if (s_screen == S_NETRADIO)
        Go(S_NETRADIO_LIST, true);
    else if (s_screen != S_NETRADIO_LIST)
        s_screen = S_NETRADIO_LIST;   // (from the catalogue's screen: a tab in place)
    s_screenTick = GetTickCount();
}

static void NetFavAction(int i)   // a star: the row's station (-1: the one chosen)
{
    NetLoadList();
    if (i < 0) {
        if (NetCur())
            NetFavToggle(s_netNow);
    } else if (i < s_nl[s_netView].n) {
        NcStation st = s_nl[s_netView].st[i];   // (a copy: the favourites' list moves under it)
        NetFavToggle(st);
    }
}
