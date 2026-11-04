// vh_tasks.inc - part of home.cpp (included there, in order): the task manager.

// ---------------------------------------------------------------- the main screen: widgets
// Two pages of five widgets (3 x 2 grid of 248 x 158 cells, the first of each page two cells wide), swiped or
// turned with the dots. A widget opens its screen; small buttons inside it act on their own; a caption with a gear
// opens the widget's settings. Real: the date, the programs, the weather, the speed (GPS) and the cameras; the
// rest are demo values until the car's bus through the MCU is known.

// ---- task manager (Apps -> the button in the top bar): processes and windows (VestaCore vtask), the CPU
// and memory graphs over the last 2 minutes. Sampled every 2 s while the screen is in front, never in the
// background. A process ends on the second press of its button; system ones and VestaHome itself can't.

enum { kTaskRows = 4 };   // (above y 408: "back" at the bottom left)
static TaskInfo s_task;
static int      s_taskTab;       // 0 processes, 1 windows
static int      s_taskPage;
static DWORD    s_taskArm;       // the process whose "End" was pressed once
static int      s_taskTicks;

static void TasksOpen()
{
    if (!s_task.procCount && !s_task.histCount) {   // the first time: the busiest first
        s_task.sortKey = TS_CPU;
        s_task.sortDesc = true;
    }
    s_taskPage = 0;
    s_taskArm = 0;
    TaskSample(&s_task, s_wnd);
}

static int TaskPages()
{
    int n = s_taskTab ? s_task.winCount : s_task.procCount;
    return n ? (n + kTaskRows - 1) / kTaskRows : 1;
}

static void MemText(wchar_t* s, int n, DWORD kb)
{
    if (kb < 1024)
        _snwprintf(s, n, L"%u КБ", (unsigned)kb);
    else
        _snwprintf(s, n, L"%u,%u МБ", (unsigned)(kb / 1024), (unsigned)(kb % 1024 * 10 / 1024));
    s[n - 1] = 0;
}

static void TaskEnd(DWORD pid)
{
    int i = 0;
    while (i < s_task.procCount && s_task.proc[i].pid != pid)
        i++;
    if (i == s_task.procCount)
        return;
    wchar_t name[40];
    wcscpy(name, s_task.proc[i].name);
    if (s_taskArm != pid) {
        s_taskArm = pid;
        _snwprintf(s_status, 96, L"ещё раз, чтобы завершить %s", name);
        return;
    }
    s_taskArm = 0;
    if (TaskKill(pid, name)) {
        _snwprintf(s_status, 96, L"завершён: %s", name);
        memmove(&s_task.proc[i], &s_task.proc[i + 1], (s_task.procCount - i - 1) * sizeof(TaskProc));
        s_task.procCount--;
    } else {
        _snwprintf(s_status, 96, L"не завершается: %s", name);
    }
}

static int TaskWinIndex(HWND h)
{
    for (int i = 0; i < s_task.winCount; i++)
        if (s_task.win[i].h == h)
            return i;
    return -1;
}

static void TaskSwitch(HWND h)
{
    int i = TaskWinIndex(h);
    if (i < 0 || !IsWindow(h)) {
        wcscpy(s_status, L"окна уже нет");
        return;
    }
    s_status[0] = 0;
    RaiseWindow(h);
}

static void TaskClose(HWND h)
{
    int i = TaskWinIndex(h);
    if (i < 0 || !IsWindow(h)) {
        wcscpy(s_status, L"окна уже нет");
        return;
    }
    PostMessage(h, WM_CLOSE, 0, 0);
    _snwprintf(s_status, 96, L"закрывается: %s", s_task.win[i].title);
    memmove(&s_task.win[i], &s_task.win[i + 1], (s_task.winCount - i - 1) * sizeof(TaskWin));
    s_task.winCount--;
}

// A graph panel: caption, value, the history (percent, newest at the right edge), a line at 50 %.
static void TaskGraph(HDC dc, int x, int y, int w, const wchar_t* caption, const wchar_t* value,
                      const unsigned char* hist, COLORREF c)
{
    R(dc, x, y, w, 104, 18, kPanel);
    T(dc, x + 14, y + 26, caption, 15, kMuted, 700);
    T(dc, x + w - 14, y + 26, value, 15, kText, 700, AL_END, w - 120);
    int gx = x + 14, gy = y + 36, gw = w - 28, gh = 56;
    Fill(dc, gx, gy + gh / 2, gx + gw, gy + gh / 2 + 1, kLine);
    Fill(dc, gx, gy + gh, gx + gw, gy + gh + 2, kLine);
    int n = s_task.histCount;
    if (n < 2)
        return;
    POINT p[TASK_HIST];
    for (int i = 0; i < n; i++) {
        p[i].x = gx + (TASK_HIST - n + i) * gw / (TASK_HIST - 1);
        p[i].y = gy + gh - hist[i] * gh / 100;
    }
    HGDIOBJ old = SelectObject(dc, SolidPen(2, C(c)));
    Polyline(dc, p, n);
    SelectObject(dc, old);
}

// A sort button; the current one is orange with a small triangle: down = the biggest first.
static void TaskSortBtn(HDC dc, int x, const wchar_t* label, int key)
{
    bool cur = s_task.sortKey == key;
    Button(dc, x, 176, 104, 48, label, cur ? B_SEL : B_N, IC_NONE, 16, 0, A_TASKSORT, key);
    if (!cur)
        return;
    int cx = x + 92, cy = 200, d = s_task.sortDesc ? 1 : -1;
    POINT t[3] = { { cx - 5, cy - 3 * d }, { cx + 5, cy - 3 * d }, { cx, cy + 3 * d } };
    Poly(dc, t, 3, kInk);
}

static void DrawTasks(HDC dc)
{
    wchar_t s[96], a[24], b[24];
    Frame(dc, L"Диспетчер задач");
    if (s_task.cpu10 >= 0)
        _snwprintf(s, 96, L"%d %%", (s_task.cpu10 + 5) / 10);
    else
        wcscpy(s, L"...");
    TaskGraph(dc, 16, 64, 380, L"Процессор", s, s_task.cpuHist, kOrange);
    DWORD total = s_task.memTotalKB, used = total - s_task.memAvailKB;
    MemText(a, 24, used);
    MemText(b, 24, total);
    _snwprintf(s, 96, L"%s из %s · %u %%", a, b, total ? (unsigned)(used * 100 / total) : 0);
    TaskGraph(dc, 404, 64, 380, L"Память", s, s_task.memHist, kText);

    _snwprintf(s, 96, L"ПРОЦЕССЫ %d", s_task.procCount);
    Button(dc, 16, 176, 172, 48, s, s_taskTab == 0 ? B_SEL : B_N, IC_NONE, 16, 0, A_TASKTAB, 0);
    _snwprintf(s, 96, L"ОКНА %d", s_task.winCount);
    Button(dc, 196, 176, 128, 48, s, s_taskTab == 1 ? B_SEL : B_N, IC_NONE, 16, 0, A_TASKTAB, 1);
    if (s_taskTab == 0) {
        TaskSortBtn(dc, 336, L"Имя", TS_NAME);
        TaskSortBtn(dc, 446, L"ЦП", TS_CPU);
        TaskSortBtn(dc, 556, L"Память", TS_MEM);
    }
    int pages = TaskPages();
    if (s_taskPage >= pages)
        s_taskPage = pages - 1;
    Button(dc, 668, 176, 40, 48, NULL, B_N, IC_BACK, 18, 24, A_PAGESTEP, -1);
    _snwprintf(s, 96, L"%d/%d", s_taskPage + 1, pages);
    T(dc, 726, 207, s, 15, kMuted, 600, AL_MID);
    Button(dc, 744, 176, 40, 48, NULL, B_N, IC_NEXT, 18, 24, A_PAGESTEP, 1);

    if (s_taskTab == 0 && !s_task.procOk) {
        T(dc, 400, 350, L"Список процессов недоступен: нет toolhelp.dll", 18, kMuted, 600, AL_MID, 760);
        return;
    }
    for (int k = 0; k < kTaskRows; k++) {
        int i = s_taskPage * kTaskRows + k, y = 232 + k * 44;
        if (s_taskTab == 0) {
            if (i >= s_task.procCount)
                break;
            const TaskProc& p = s_task.proc[i];
            bool self = p.pid == GetCurrentProcessId(), sys = TaskIsSystem(p.name);
            R(dc, 16, y, 768, 40, 10, kPanel);
            T(dc, 32, y + 27, p.name, 18, self ? kOrange : kText, 700, AL_START, 250);
            _snwprintf(s, 96, L"%d пот.", p.threads);
            T(dc, 296, y + 26, s, 15, kMuted, 400);
            if (p.cpu10 >= 0)
                _snwprintf(s, 96, L"%d,%d %%", p.cpu10 / 10, p.cpu10 % 10);
            else
                wcscpy(s, L"...");
            T(dc, 470, y + 27, s, 17, p.cpu10 >= 200 ? kOrange : kText, 700, AL_END);
            if (p.memKB)
                MemText(s, 96, p.memKB);
            else
                wcscpy(s, L"-");
            T(dc, 604, y + 27, s, 17, kText, 700, AL_END);
            if (sys || self)
                T(dc, 700, y + 26, sys ? L"система" : L"эта программа", 14, kDim, 600, AL_MID);
            else
                Button(dc, 624, y + 2, 152, 36, s_taskArm == p.pid ? L"Точно?" : L"Завершить",
                       s_taskArm == p.pid ? B_PRI : B_N, IC_NONE, 15, 0, A_TASKKILL, (int)p.pid);
        } else {
            if (i >= s_task.winCount)
                break;
            const TaskWin& w = s_task.win[i];
            R(dc, 16, y, 768, 40, 10, kPanel);
            T(dc, 32, y + 27, w.title, 18, kText, 700, AL_START, 300);
            T(dc, 346, y + 26, w.proc, 15, kMuted, 400, AL_START, 190);
            Button(dc, 544, y + 2, 112, 36, L"Перейти", B_N, IC_NONE, 15, 0, A_TASKSWITCH, (int)(INT_PTR)w.h);
            Button(dc, 664, y + 2, 112, 36, L"Закрыть", B_N, IC_NONE, 15, 0, A_TASKCLOSE, (int)(INT_PTR)w.h);
        }
    }
    if (s_taskTab == 1 && !s_task.winCount)
        T(dc, 400, 350, L"Других окон нет", 18, kMuted, 600, AL_MID);
}

static int s_homePage;
// The widgets' grid: 3 x 2 on a page, a single 248 x 158, a double two singles and the gap; a big one (design 0.6:
// the playing one, the trip) 2 x 2 at the page's left.
enum { kWgW = 248, kWgH = 158, kWgGap = 12, kWgW2 = kWgW * 2 + kWgGap, kWgH2 = kWgH * 2 + kWgGap, kWgPages = 6 };

// Widget panel with its caption; pressed = orange like a tile (returns it: the content follows in ink).
// With a settings screen the caption strip is a button of its own (a gear at its end) leading there.
static bool Widget(HDC dc, int x, int y, int w, int h, const wchar_t* caption, Icon ic, Action a, int arg = 0,
                   Screen settings = S_NONE)
{
    bool d = a != A_NONE && Zone(x, y, w, h, a, arg);
    bool hd = settings != S_NONE && Zone(x, y, w, 42, A_SCREEN, settings);
    R(dc, x, y, w, h, 20, d ? kOrange : kPanel2);   // design 0.6: a card without a border (HU 2026-10-24: lighter)
    if (hd)
        R(dc, x + 2, y + 2, w - 4, 40, 18, kOrange);
    Ico(dc, ic, x + 16, y + 12, 22, d || hd ? kInk : kOrange);
    T(dc, x + 46, y + 29, caption, 13, d || hd ? kInk : kMuted, 700, AL_START, w - (settings != S_NONE ? 90 : 62));
    if (settings != S_NONE)
        Ico(dc, IC_GEAR, x + w - 34, y + 12, 20, d || hd ? kInk : kDim);
    return d;
}

// The player's track (or "nothing plays" with play = the last track / the list); the progress updates with the
// minute redraw, the player screen itself updates every second.
// The screen saver's line of what plays (its timer redraws it when the line changes): the internet radio's station
// and song, our music's track, Bluetooth's track, the radio's station; "" nothing.
// What plays, for the top bar's mark (2026-10-22): our internet radio, our music, our video, the phone's music, the
// radio; *to: its screen (a tap opens it). Each its own icon.
static Icon PlayingIcon(Screen* to)
{
    double pos, len;
    if (s_netOn) {
        *to = S_NETRADIO;
        return IC_NETRADIO;
    }
    if (PlayerPlaying()) {
        *to = S_AUDIO;
        return IC_MUSIC;
    }
    if (VideoTimes(&pos, &len)) {
        *to = S_VIDEO;
        return IC_VIDEO;
    }
    int vol, mute, src;
    if (!VolGet(&vol, &mute, &src))
        return IC_NONE;
    BtState bt;
    if (src == 9 && BtGet(&bt, NULL) && bt.av == 2) {   // the phone streams (our players are off: above)
        *to = S_AUDIO_BT;
        return IC_BTMUSIC;
    }
    if (src == 0) {
        *to = S_RADIO;
        return IC_RADIO;
    }
    return IC_NONE;
}

static Icon SaverNowPlaying(wchar_t* s, int n)
{
    s[0] = 0;
    if (s_netOn && NetCur()) {
        wchar_t t[160];
        EnterCriticalSection(&s_netCs);
        wcscpy(t, s_netTitle);
        LeaveCriticalSection(&s_netCs);
        if (t[0] && s_netState == NR_PLAY)
            _snwprintf(s, n, L"%s · %s", s_netNow.name, t);
        else
            _snwprintf(s, n, L"%s", s_netNow.name);
        s[n - 1] = 0;
        return IC_NETRADIO;
    }
    wchar_t title[96], sub[200];
    double pos, len;
    if (PlayerPlaying() && PlayerNow(title, sub, 96, &pos, &len)) {
        _snwprintf(s, n, L"%s", title);
        s[n - 1] = 0;
        return IC_MUSIC;
    }
    int vol, mute, src;
    VolGet(&vol, &mute, &src);
    BtState bt;
    if (src == 9 && BtGet(&bt, NULL) && bt.title[0]) {
        BtText(bt.title, s, n);
        return IC_BTMUSIC;
    }
    if (src == 0) {
        Tuner tu;
        TunerGet(&tu);
        wchar_t f[16];
        FreqText(tu.f10, f, 16);
        const wchar_t* nm = tu.ps[0] ? tu.ps : RadioName(tu, tu.f10);
        if (tu.f10)
            _snwprintf(s, n, L"%s  %s", f, nm);
        s[n - 1] = 0;
    }
    return IC_RADIO;
}

// The "now playing" widgets' parts (design 0.6): big 508 x 328 or compact 508 x 158 (1 x 2). The cover (the orange
// facets with the source's icon), up to three lines of text (the third only big), the bar (k of BarW, -1 none) with
// its times; the transport row below (MediaRow).
struct MediaGeo {
    bool big;
    int  x, y, h, cx, cy, isz, step, r, play, barW;
};

static MediaGeo MediaGeoOf(int x, int y, bool big)
{
    MediaGeo g;
    g.big = big;
    g.x = x;
    g.y = y;
    g.h = big ? kWgH2 : kWgH;
    g.cx = big ? x + kWgW2 / 2 : x + 312;   // the compact one: right of the cover
    g.cy = big ? y + 282 : y + 136;
    g.isz = big ? 36 : 28;
    g.step = big ? 114 : 102;               // the arrows' centres from the middle
    g.r = big ? 36 : 20;
    g.play = big ? 32 : 22;
    g.barW = big ? 468 : 352;
    return g;
}

static void WMediaBody(HDC dc, const MediaGeo& g, bool d, Icon ic, const wchar_t* title, const wchar_t* sub,
                       const wchar_t* third, int k, const wchar_t* t0, const wchar_t* t1)
{
    COLORREF fg = d ? kInk : kText, mg = d ? kInk : kMuted;
    int x = g.x, y = g.y;
    if (g.big) {
        TriFill(x + 20, y + 52, 132, 132, 18);
        Ico(dc, ic, x + 54, y + 86, 64, Mix(kInk, kTriOr[1], 140));
        T(dc, x + 172, y + 98, title, 28, fg, 700, AL_START, 316);
        TWrap(dc, x + 172, y + 126, sub, 17, mg, 400, 316, 1, 0);
        if (third)
            TWrap(dc, x + 172, y + 152, third, 14, d ? kInk : kDim, 400, 316, 1, 0);
        if (k >= 0) {
            R(dc, x + 20, y + 206, g.barW, 6, 3, d ? kInk : kLine);
            R(dc, x + 20, y + 206, k > 6 ? k : 6, 6, 3, d ? kInk : kOrange);
            T(dc, x + 20, y + 234, t0, 15, mg, 600);
            T(dc, x + 488, y + 234, t1, 15, mg, 600, AL_END);
        }
        return;
    }
    TriFill(x + 16, y + 42, 100, 100, 14);
    Ico(dc, ic, x + 40, y + 66, 52, Mix(kInk, kTriOr[1], 140));
    T(dc, x + 136, y + 70, title, 26, fg, 700, AL_START, 352);
    wchar_t times[40] = L"";
    if (k >= 0 && t0 && t1 && t0[0])
        _snwprintf(times, 40, L"%s / %s", t0, t1);
    int tw = times[0] ? TextWidth(dc, times, 15, 600) + 12 : 0;
    TWrap(dc, x + 136, y + 94, sub, 16, mg, 400, 352 - tw, 1, 0);
    if (times[0])
        T(dc, x + kWgW2 - 20, y + 94, times, 15, mg, 600, AL_END);
    if (k >= 0) {
        R(dc, x + 136, y + 106, g.barW, 6, 3, d ? kInk : kLine);
        R(dc, x + 136, y + 106, k > 6 ? k : 6, 6, 3, d ? kInk : kOrange);
    }
}

// The transport row: previous, the middle (a play button of its own, drawn by the caller), next.
static void MediaArrows(HDC dc, const MediaGeo& g, bool d, Action pa, int prevArg, Action na, int nextArg)
{
    IconBtn(dc, IC_PREV, g.cx - g.step - g.isz / 2, g.cy - g.isz / 2, g.isz, d ? kInk : kText, pa, prevArg);
    IconBtn(dc, IC_FWD, g.cx + g.step - g.isz / 2, g.cy - g.isz / 2, g.isz, d ? kInk : kText, na, nextArg);
}

// The radio's page name of the tuner ("FM1", "УКВ", "СВ").
static void TunerPage(const Tuner& t, wchar_t* s, int n)
{
    if (t.band == 1 || t.band == 2)
        wcscpy(s, t.band == 1 ? L"УКВ" : L"СВ");
    else
        _snwprintf(s, n, L"FM%d", t.cur >= 0 && t.cur < 24 ? t.cur / 8 + 1 : 1);
}

// What plays now (design 0.6; the user 2026-10-23: the radio too): our internet radio, our music, the radio (the
// MCU's source), the phone's music; nothing - the last of ours to go on with. Big (2 x 2) or compact (1 x 2).
static void WMediaAt(HDC dc, int x, int y, bool big)
{
    MediaGeo g = MediaGeoOf(x, y, big);
    int vol, mute, src;
    bool srcKnown = VolGet(&vol, &mute, &src);
    bool ours = s_netOn || PlayerPlaying();
    if (!ours && srcKnown && src == 0) {   // the radio: its name, frequency and page, RDS; the presets step
        Tuner t;
        TunerGet(&t);
        bool d = Widget(dc, x, y, kWgW2, g.h, L"Сейчас играет · Радио", IC_RADIO, A_NAV, S_RADIO);
        wchar_t f[16], pg[8], sub[48], rt[68];
        FreqText(t.f10, f, 16);
        TunerPage(t, pg, 8);
        _snwprintf(sub, 48, L"%s %s · %s", f, t.band == 2 ? L"кГц" : L"МГц", pg);
        _snwprintf(rt, 68, L"%s%s", t.rt[0], t.rt[1]);
        rt[67] = 0;
        const wchar_t* nm = t.ps[0] ? t.ps : RadioName(t, t.f10);
        WMediaBody(dc, g, d, IC_RADIO, nm[0] ? nm : f, t.f10 ? sub : L"нет данных от радио", rt, -1, NULL, NULL);
        if (t.presets > 0) {   // previous / next preset of the page playing (none playing: from the first)
            int cur = t.cur >= 0 && t.cur < 24 ? t.cur : 0, page = cur / 8 * 8, per = t.presets < 8 ? t.presets : 8;
            MediaArrows(dc, g, d, A_PRESET, page + (cur - page + per - 1) % per, A_PRESET,
                        t.cur >= 0 ? page + (cur - page + 1) % per : cur);
        }
        T(dc, g.cx, g.cy + (big ? 12 : 8), f, big ? 34 : 22, d ? kInk : t.f10 ? kOrange : kDim, 700, AL_MID);
        return;
    }
    BtState bt;
    if (!ours && srcKnown && src == 9 && BtGet(&bt, NULL) && bt.title[0]) {   // the phone's music (AVRCP)
        bool d = Widget(dc, x, y, kWgW2, g.h, L"Сейчас играет · Bluetooth", IC_BTMUSIC, A_NAV, S_AUDIO_BT);
        wchar_t title[BT_TEXT], artist[BT_TEXT], t0[16] = L"", t1[16] = L"";
        BtText(bt.title, title, BT_TEXT);
        BtText(bt.artist, artist, BT_TEXT);
        int len = bt.lengthMs > 0 ? bt.lengthMs : bt.durationMs, k = -1;
        if (len > 0) {
            int pos = bt.posMs;
            if (bt.playing == 1 && bt.playTick)
                pos += (int)(GetTickCount() - bt.playTick);
            pos = pos < 0 ? 0 : pos > len ? len : pos;
            k = g.barW * (pos / 1000) / (len / 1000 > 0 ? len / 1000 : 1);
            _snwprintf(t0, 16, L"%d:%02d", pos / 60000, pos / 1000 % 60);
            _snwprintf(t1, 16, L"%d:%02d", len / 60000, len / 1000 % 60);
        }
        WMediaBody(dc, g, d, IC_BTMUSIC, title, artist, NULL, k, t0, t1);
        bool playing = BtPlayingNow(bt);
        MediaArrows(dc, g, d, A_BTCMD, BT_CMD_PREV, A_BTCMD, BT_CMD_NEXT);
        bool down = Zone(g.cx - g.r, g.cy - g.r, 2 * g.r, 2 * g.r, A_BTCMD, BT_CMD_PLAYPAUSE);
        Disc(dc, g.cx, g.cy, g.r, down ? kFacet : kOrange);
        Ico(dc, playing ? IC_PAUSE : IC_PLAY, g.cx - g.play / 2, g.cy - g.play / 2, g.play, kInk);
        return;
    }
    if (s_mediaLast == 2 && !PlayerPlaying())   // the internet radio last (it may be off): its station
        NetLoadList();
    if (NetCur() && (s_netOn || (s_mediaLast == 2 && !PlayerPlaying()))) {   // the station, the song, its state
        bool d = Widget(dc, x, y, kWgW2, g.h, L"Сейчас играет · Интернет-радио", IC_NETRADIO, A_NAV, S_NETRADIO);
        wchar_t s[160];
        EnterCriticalSection(&s_netCs);
        wcscpy(s, s_netTitle);
        LeaveCriticalSection(&s_netCs);
        if (!s[0] || s_netState != NR_PLAY)
            NetStateText(s, 160);
        WMediaBody(dc, g, d, IC_NETRADIO, s_netNow.name, s, NULL, -1, NULL, NULL);
        MediaArrows(dc, g, d, A_NRSTEP, -1, A_NRSTEP, 1);
        NetPlayBtn(dc, g.cx, g.cy, g.r, g.play);
        return;
    }
    bool d = Widget(dc, x, y, kWgW2, g.h, L"Сейчас играет · Музыка", IC_MUSIC, A_NAV, S_AUDIO);
    wchar_t title[96], sub[200], s[48], a[16], b[16];
    double pos = 0, len = 0;
    if (PlayerNow(title, sub, 96, &pos, &len)) {
        _snwprintf(s, 48, L"трек %d из %d", s_plCur + 1, s_plCount);
        Mmss(a, 16, pos);
        Mmss(b, 16, len);
        WMediaBody(dc, g, d, IC_MUSIC, title, sub[0] ? sub : s, sub[0] ? s : NULL,
                   len > 0 ? (int)(pos * g.barW / len) : 0, a, b);
    } else {   // nothing loaded: the last track (dim) to go on with, or a dash
        wchar_t last[MAX_PATH];
        const wchar_t* fn = wcsrchr(s_cfg.musicFile, L'\\');
        wcscpy(last, fn ? fn + 1 : s_cfg.musicFile);
        wchar_t* dot = wcsrchr(last, L'.');
        if (dot)
            *dot = 0;
        WMediaBody(dc, g, d, IC_MUSIC, last[0] ? last : L"—",
                   last[0] ? L"последний трек · ▶ — продолжить" : L"выберите музыку в «Медиа»", NULL, -1, NULL, NULL);
    }
    MediaArrows(dc, g, d, A_PREVTRK, 0, A_NEXTTRK, 0);
    PlayBtn(dc, g.cx, g.cy, g.r, g.play);
}

static void WMedia(HDC dc, int x, int y)   // big, 2 x 2
{
    WMediaAt(dc, x, y, true);
}

static void WMediaSmall(HDC dc, int x, int y)   // compact, 1 x 2
{
    WMediaAt(dc, x, y, false);
}

static const wchar_t* kHeadings[8] = { L"С", L"СВ", L"В", L"ЮВ", L"Ю", L"ЮЗ", L"З", L"СЗ" };

// Speed: the car's own from the CAN module ("@,<km/h>") when it comes, otherwise GPS; a camera ahead shows as its
// speed sign ("КАМ" without a limit) with the distance to it.
static void WSpeed(HDC dc, int x, int y)
{
    Gps g;
    GpsGet(&g);
    bool fix = GpsFresh(g, 5000);
    bool car = g.carTick && GetTickCount() - g.carTick < 3000;
    Widget(dc, x, y, 248, 158, car ? L"Скорость · CAN" : L"Скорость · GPS", IC_NAV, A_NONE, 0, S_WSET_SPEED);
    wchar_t v[64];
    if (car)
        _snwprintf(v, 8, L"%d", g.carSpeed);
    else if (fix)
        _snwprintf(v, 8, L"%d", (int)(g.speedKmh + 0.5));
    else
        wcscpy(v, L"—");
    int end = T(dc, x + 16, y + 112, v, 72, fix || car ? kText : kDim, 700);
    if (s_camNear >= 0) {
        SpeedSign(dc, x + 204, y + 80, 30, s_cams[s_camNear].speed);   // (the real sign's colours, the user 2026-10-26)
        _snwprintf(v, 16, L"%d м", s_camNearDist);
        T(dc, x + 204, y + 130, v, 14, CamOver() ? kStop : kOrange, 700, AL_MID);
    } else {
        T(dc, end + 8, y + 112, L"км/ч", 18, kMuted, 600);
    }
    if (fix)
        _snwprintf(v, 64, L"%s · %d м · спутников %d", kHeadings[((int)(g.course + 22.5) / 45) % 8], (int)g.alt, g.sats);
    else
        GpsStatusText(v, 64);
    TWrap(dc, x + 16, y + 144, v, 14, fix ? kMuted : kDim, 400, 216, 1, 0);
}

// The radio widget: the MCU's frequency and RDS name; the arrows step the presets of the page playing.
static void WRadio(HDC dc, int x, int y)
{
    Tuner t;
    TunerGet(&t);
    wchar_t f[16];
    FreqText(t.f10, f, 16);
    bool d = Widget(dc, x, y, 248, 158, t.band == 2 || (t.f10 > 0 && t.f10 < 3000) ? L"Радио · СВ"
                    : t.band == 1 || (t.f10 >= 6500 && t.f10 < 8750) ? L"Радио · УКВ" : L"Радио · FM", IC_RADIO, A_NAV,
                    S_RADIO);
    T(dc, x + 16, y + 88, f, 44, d ? kInk : t.f10 ? kText : kDim, 700);
    // RDS's name, else the one known for the frequency
    TWrap(dc, x + 16, y + 114, t.ps[0] ? t.ps : RadioName(t, t.f10), 17, d ? kInk : kMuted, 600, 216, 1, 0);
    if (t.presets > 0) {   // previous / next preset (none playing: from the first)
        int cur = t.cur >= 0 && t.cur < 24 ? t.cur : 0, page = cur / 8 * 8, per = t.presets < 8 ? t.presets : 8;
        IconBtn(dc, IC_PREV, x + 24, y + 124, 26, d ? kInk : kText, A_PRESET, page + (cur - page + per - 1) % per);
        IconBtn(dc, IC_FWD, x + 82, y + 124, 26, d ? kInk : kText, A_PRESET,
                t.cur >= 0 ? page + (cur - page + 1) % per : cur);
    }
}

// The weather's sign in a 64 x 64 box: sun, sun and cloud, cloud, and the cloud with rain, snow or lightning; fog.
static void WxIcon(HDC dc, int kind, int x, int y, COLORREF fg)
{
    if (kind == WX_SUN) {
        Ico(dc, IC_SUN, x + 4, y + 4, 56, kOrange);
        return;
    }
    if (kind == WX_FOG) {
        for (int i = 0; i < 3; i++)
            R(dc, x + 6 + i * 4, y + 20 + i * 12, 52 - i * 8, 4, 2, fg);
        return;
    }
    if (kind == WX_PART)
        Ico(dc, IC_SUN, x - 2, y - 2, 42, kOrange);
    Ico(dc, IC_CLOUD, x + 12, y + 6, 50, fg);
    for (int i = 0; i < 3; i++) {
        if (kind == WX_RAIN || kind == WX_STORM)
            Line(dc, x + 26 + i * 11, y + 46, x + 22 + i * 11, y + 58, 3, kind == WX_STORM && i == 1 ? kOrange : kMuted);
        else if (kind == WX_SNOW)
            Disc(dc, x + 26 + i * 11, y + 52, 3, fg);
    }
}

// Weather: a tap on it refreshes, the caption leads to its settings (provider, place).
static void WWeather(HDC dc, int x, int y)
{
    Weather w;
    WxGet(&w);
    wchar_t cap[64], s[100];
    if (w.have && w.place[0]) {
        _snwprintf(cap, 64, L"Погода · %s", w.place);
        CharUpperBuff(cap + 9, 1);   // the place as a name
    } else {
        wcscpy(cap, L"Погода");
    }
    bool d = Widget(dc, x, y, 248, 158, cap, IC_CLOUD, A_WXNOW, 0, S_WSET_WEATHER);
    COLORREF fg = d ? kInk : kText, mg = d ? kInk : kMuted;
    if (!w.have) {
        T(dc, x + 16, y + 84, s_wxBusy ? L"Загрузка…" : L"Нет данных", 22, fg, 700, AL_START, 216);
        TWrap(dc, x + 16, y + 108, w.ok < 0 ? w.err : L"нажмите, чтобы обновить", 14, mg, 400, 216, 3, 18);
        return;
    }
    WxIcon(dc, w.kind, x + 16, y + 44, fg);
    _snwprintf(s, 16, L"%+d°", w.temp);
    T(dc, x + 96, y + 96, s, 44, fg, 700);
    _snwprintf(s, 100, L"%s, ветер %d м/с", w.desc, w.wind);
    CharUpperBuff(s, 1);
    TWrap(dc, x + 16, y + 126, s, 15, mg, 400, 216, 1, 0);
    if (w.ok < 0)
        _snwprintf(s, 100, L"не обновилась: %s", w.err);
    else
        _snwprintf(s, 100, L"днём %+d°, ночью %+d° · %02d:%02d", w.tmax, w.tmin, w.at.wHour, w.at.wMinute);
    TWrap(dc, x + 16, y + 146, s, 14, d ? kInk : kDim, 400, 216, 1, 0);
}

// The screen saver's weather, centred on cx: the picture and the temperature (the car's sensor first), below them
// the sky's text.
static void SaverWeather(HDC dc, int cx, int y)
{
    Weather w;
    WxGet(&w);
    int temp;
    if (!OutsideTemp(&temp))
        return;
    wchar_t ts[16];
    _snwprintf(ts, 16, L"%+d°", temp);
    int tw = TextWidth(dc, ts, 40, 700), iw = w.have ? 72 : 0, x = cx - (iw + tw) / 2;
    if (w.have)
        WxIcon(dc, w.kind, x, y, kText);
    T(dc, x + iw, y + 48, ts, 40, kText, 700);
    if (w.have && w.desc[0])
        T(dc, cx, y + 88, w.desc, 18, kMuted, 400, AL_MID, 400);
}

static bool CarSensor(int id, wchar_t* v, int n);   // (below: the trip's)
static const wchar_t* CarSensorName(int id);

// The car (2026-10-22, out of the demo; the user: three lines, chosen in its settings): the sensors chosen, "—"
// while one has no data.
static void WCar(HDC dc, int x, int y)
{
    bool d = Widget(dc, x, y, 248, 158, L"Автомобиль", IC_CAR, A_SCREEN, S_CAR, S_WSET_CAR);
    for (int i = 0; i < 3; i++) {
        if (s_cfg.carSens[i] < 0)
            continue;
        wchar_t v[32];
        bool have = CarSensor(s_cfg.carSens[i], v, 32);
        int ly = y + 74 + i * 30;
        T(dc, x + 16, ly, CarSensorName(s_cfg.carSens[i]), 15, d ? kInk : kMuted, 400, AL_START, 132);
        T(dc, x + 232, ly, have ? v : L"—", 19, d ? kInk : have ? kText : kDim, 700, AL_END, 96);
    }
}

// The trip (2026-10-21, out of the demo): since VestaHome started (the ignition), counted once a second from the
// car's speed (CAN, the MCU's 0x412A) or else GPS: the distance, the time moving (3 km/h and more), the mean of it
// and the top speed. With OBD (2026-10-22): the fuel burnt from the air flow (0110, petrol 14.7:1, 745 g/L) while it
// answers, its own km beside it - the mean consumption; the range = the tank's level (012F) x car.tank / that mean.
struct Trip {
    double km; DWORD movingS; int maxKmh; SYSTEMTIME start; bool started; int src;   // src 1 CAN 2 GPS
    double fuelL, obdKm;
};
static Trip s_trip;

static double TripMean()   // L/100 km of the trip, -1 not yet (under 1 km with OBD)
{
    return s_trip.obdKm >= 1 ? s_trip.fuelL * 100 / s_trip.obdKm : -1;
}

static int TripRange()   // km on what is left in the tank, -1 not known
{
    ObdInfo o;
    ObdGet(&o);
    double m = TripMean();
    if (o.state != OBD_ON || o.fuel < 0 || m < 1)
        return -1;
    return (int)(o.fuel * s_cfg.tank / 100.0 * 100 / m + 0.5);
}

static void TripTick()   // once a second
{
    if (!s_trip.started) {
        GetLocalTime(&s_trip.start);
        s_trip.started = true;
    }
    Gps g;
    GpsGet(&g);
    ObdInfo o;
    ObdGet(&o);
    bool obd = o.state == OBD_ON && o.maf100 >= 0;
    if (obd)   // a second of it, L
        s_trip.fuelL += o.maf100 / 100.0 / (14.7 * 745);
    int kmh = -1;
    if (g.carTick && GetTickCount() - g.carTick < 3000) {
        kmh = g.carSpeed;
        s_trip.src = 1;
    } else if (GpsFresh(g, 3000)) {
        kmh = (int)(g.speedKmh + 0.5);
        s_trip.src = 2;
    }
    if (kmh < 3)
        return;
    s_trip.km += kmh / 3600.0;
    if (obd)
        s_trip.obdKm += kmh / 3600.0;
    s_trip.movingS++;
    if (kmh > s_trip.maxKmh)
        s_trip.maxKmh = kmh;
}

static void WTrip(HDC dc, int x, int y)
{
    bool d = Widget(dc, x, y, kWgW2, kWgH2, s_trip.src == 1 ? L"Бортовой компьютер · Поездка · CAN"
                    : L"Бортовой компьютер · Поездка · GPS", IC_CLOCK, A_SCREEN, S_CAR);
    wchar_t v[4][16];
    _snwprintf(v[0], 16, s_trip.km < 100 ? L"%.1f" : L"%.0f", s_trip.km);
    for (wchar_t* c = v[0]; *c; c++)
        if (*c == L'.')
            *c = L',';
    _snwprintf(v[1], 16, L"%u:%02u", (unsigned)(s_trip.movingS / 3600), (unsigned)(s_trip.movingS / 60 % 60));
    if (s_trip.movingS >= 60)
        _snwprintf(v[2], 16, L"%d", (int)(s_trip.km * 3600 / s_trip.movingS + 0.5));
    else
        wcscpy(v[2], L"—");
    if (s_trip.maxKmh > 0)
        _snwprintf(v[3], 16, L"%d", s_trip.maxKmh);
    else
        wcscpy(v[3], L"—");
    const wchar_t* head[4] = { L"Пробег", L"В движении", L"Средняя", L"Максимальная" };
    const wchar_t* unit[4] = { L"км", L"ч:мин", L"км/ч", L"км/ч" };
    for (int i = 0; i < 4; i++) {   // big (design 0.6): 2 x 2, the number large with its unit after it
        int cx = x + 20 + i % 2 * 244, cy = y + 50 + i / 2 * 98;
        T(dc, cx, cy + 18, head[i], 14, d ? kInk : kMuted, 400, AL_START, 228);
        int end = T(dc, cx, cy + 70, v[i], 46, d ? kInk : kText, 700, AL_START, 160);
        T(dc, end + 8, cy + 70, unit[i], 16, d ? kInk : kMuted, 400);
    }
    wchar_t s[96], f[48] = L"расход и запас хода — с OBD";
    double m = TripMean();
    int range = TripRange();
    if (m >= 0 && range >= 0)
        _snwprintf(f, 48, L"расход %d,%d л/100 км · запас %d км", (int)m, (int)(m * 10) % 10, range);
    else if (m >= 0)
        _snwprintf(f, 48, L"расход %d,%d л/100 км", (int)m, (int)(m * 10) % 10);
    else if (s_trip.fuelL > 0)
        _snwprintf(f, 48, L"сожжено %d,%d л", (int)s_trip.fuelL, (int)(s_trip.fuelL * 10) % 10);
    f[47] = 0;
    CharUpperBuff(f, 1);
    Sep(dc, x + 20, y + 246, kWgW2 - 40);
    T(dc, x + 20, y + 280, f, 18, d ? kInk : kText, 700, AL_START, kWgW2 - 40);
    if (s_trip.started) {
        _snwprintf(s, 96, L"поездка с %02d:%02d", s_trip.start.wHour, s_trip.start.wMinute);
        s[95] = 0;
        T(dc, x + 20, y + 306, s, 14, d ? kInk : kDim, 400, AL_START, kWgW2 - 40);
    }
}

static bool SunNight(int* rise, int* set);

static void WDate(HDC dc, int x, int y)
{
    SYSTEMTIME t;
    GetLocalTime(&t);
    wchar_t day[4];
    _snwprintf(day, 4, L"%d", t.wDay);
    bool d = Widget(dc, x, y, 248, 158, L"Календарь", IC_CLOCK, A_NAV, S_SETTINGS);
    int end = T(dc, x + 16, y + 112, day, 64, d ? kInk : kText, 700);
    T(dc, end + 12, y + 84, kMonths[(t.wMonth + 11) % 12], 19, d ? kInk : kText, 700, AL_START, x + 232 - end - 12);
    T(dc, end + 12, y + 108, kDays[t.wDayOfWeek % 7], 15, d ? kInk : kMuted, 400, AL_START, x + 232 - end - 12);
    int rise, set;   // our own sun (the theme's AUTO: GPS, else home, else 7:00-20:00) - no weather needed
    SunNight(&rise, &set);
    wchar_t s[48];
    Gps g;
    GpsGet(&g);
    if (!(g.fixTick && (g.lat || g.lon)) && !s_cfg.homeLat && !s_cfg.homeLon)   // (SunNight's 7:00-20:00 then)
        wcscpy(s, L"Восход и закат — по GPS или «дому»");
    else if (rise >= 0 && set >= 0)
        _snwprintf(s, 48, L"Восход %d:%02d · закат %d:%02d", rise / 60, rise % 60, set / 60, set % 60);
    else
        wcscpy(s, rise == -1 ? L"Полярная ночь" : L"Полярный день");
    TWrap(dc, x + 16, y + 144, s, 14, d ? kInk : kDim, 400, 216, 1, 0);
}

// Climate control, only when the car has it (the car widget's settings); demo until the MCU protocol is known.
// The climate: live from the MCU (through the proxy) when it has sent anything since start - the knob's
// position (0..16: the Vesta's climate reports a knob, not degrees), the blower 1..7, A/C, auto, the rear window
// heating and the outside temperature; otherwise the demo.
static void WClimate(HDC dc, int x, int y)
{
    Hvac h;
    if (HvacGet(&h)) {
        Widget(dc, x, y, 248, 158, L"Климат", IC_CAR, A_SCREEN, S_TEST_MCU, S_WSET_CAR);
        wchar_t s[24];
        if (h.temp >= 0) {
            for (int i = 0; i < 16; i++) {   // the knob: blue to red, the set part lit
                COLORREF c = i < h.temp ? (i < 8 ? RGB(0x4a, 0x9e, 0xff) : kOrange) : kLine;
                R(dc, x + 16 + i * 9, y + 70, 7, 18, 2, c);
            }
            _snwprintf(s, 24, L"%d/16", h.temp);
            T(dc, x + 168, y + 86, s, 18, kText, 700);
        }
        if (h.blower >= 0) {
            for (int i = 0; i < 7; i++)
                R(dc, x + 16 + i * 13, y + 112 - (6 + i * 3), 9, 6 + i * 3, 2, i < h.blower ? kText : kLine);
            _snwprintf(s, 24, L"вент %d", h.blower);
            T(dc, x + 112, y + 112, s, 14, kMuted, 600);
        }
        R(dc, x + 16, y + 120, 56, 28, 14, h.ac == 1 ? kOrange : kPanel2);
        T(dc, x + 44, y + 140, L"A/C", 14, h.ac == 1 ? kInk : kMuted, 700, AL_MID);
        R(dc, x + 80, y + 120, 64, 28, 14, h.autoMode == 2 ? kOrange : kPanel2);
        T(dc, x + 112, y + 140, L"Авто", 13, h.autoMode == 2 ? kInk : kMuted, 700, AL_MID);
        R(dc, x + 152, y + 120, 80, 28, 14, h.rear == 1 ? kOrange : kPanel2);
        T(dc, x + 192, y + 140, L"Обогрев", 12, h.rear == 1 ? kInk : kMuted, 700, AL_MID);
        if (h.outer > -100) {
            _snwprintf(s, 24, L"за бортом %+d°", h.outer);
            T(dc, x + 232, y + 54, s, 13, kMuted, 600, AL_END);
        }
        return;
    }
    Widget(dc, x, y, 248, 158, L"Климат · Демо", IC_CAR, A_NONE, 0, S_WSET_CAR);
    T(dc, x + 16, y + 100, L"22,0°", 46, kText, 700);
    T(dc, x + 156, y + 72, L"Авто", 15, kOrange, 700);
    for (int i = 0; i < 5; i++)   // fan speed 3 of 5
        R(dc, x + 156 + i * 15, y + 100 - (8 + i * 4), 10, 8 + i * 4, 2, i < 3 ? kText : kLine);
    R(dc, x + 16, y + 116, 64, 30, 8, kOrange);
    T(dc, x + 48, y + 137, L"A/C", 15, kInk, 700, AL_MID);
    R(dc, x + 88, y + 116, 104, 30, 15, kPanel2);
    T(dc, x + 140, y + 137, L"Рецирк.", 13, kMuted, 700, AL_MID);
}

// The phone (2026-10-21, out of the demo): the shell's Bluetooth through our proxy - the phone connected, its
// operator, battery and signal, a call going on (answer / hang up here). Not connected: the last phone's name.
// The last calls come with the phone book (PBAP).
static void WPhone(HDC dc, int x, int y)
{
    BtState bt;
    bool proxy = BtGet(&bt, NULL), on = proxy && bt.hf == 2;
    wchar_t title[96], name[BT_TEXT], s[96];
    wcscpy(title, L"Телефон");
    if (on && bt.devName[0]) {
        BtText(bt.devName, name, BT_TEXT);
        _snwprintf(title, 96, L"Телефон · %s", name);
        title[95] = 0;
    }
    bool d = Widget(dc, x, y, 248, 158, title, IC_PHONE, A_NAV, S_PHONE);
    COLORREF big = d ? kInk : kText, mut = d ? kInk : kMuted;
    if (!proxy) {
        TWrap(dc, x + 16, y + 92, L"нужен proxy Bluetooth (VestaHomeInstall, кнопка 2)", 15, mut, 400, 216, 2, 20);
        return;
    }
    if (!on) {
        T(dc, x + 16, y + 94, L"не подключён", 22, d ? kInk : kDim, 700, AL_START, 216);
        name[0] = 0;   // the last phone, if it is among the paired ones
        for (int i = 0; i < bt.paired && i < BT_PAIRED; i++) {
            wchar_t a[18];
            BtText(bt.pairedAddr[i], a, 18);
            if (!wcscmp(a, s_cfg.btLast))
                BtText(bt.pairedName[i], name, 48);
        }
        if (name[0]) {
            _snwprintf(s, 96, L"последний: %s", name);
            s[95] = 0;
            T(dc, x + 16, y + 122, s, 15, mut, 400, AL_START, 216);
        }
        return;
    }
    int call = bt.call[0] ? bt.call[0] : bt.call[1];
    if (call) {   // a call: its state, the number, answer / hang up
        const wchar_t* st = call == BT_CALL_INCOMING || call == BT_CALL_WAITING ? L"Входящий"
                          : call == BT_CALL_DIALING || call == BT_CALL_ALERTING ? L"Вызов"
                          : call == BT_CALL_HELD ? L"На удержании" : L"Разговор";
        T(dc, x + 16, y + 66, st, 13, d ? kInk : kOrange, 700);
        wchar_t num[40];
        MultiByteToWideChar(CP_ACP, 0, bt.callNumber, -1, num, 40);
        num[39] = 0;
        T(dc, x + 16, y + 100, num[0] ? num : L"номер скрыт", 22, big, 700, AL_START, 150);
        bool in = call == BT_CALL_INCOMING || call == BT_CALL_WAITING;
        if (in) {
            bool a = Zone(x + 100, y + 104, 64, 44, A_BTCMD, BT_CMD_ANSWER);
            R(dc, x + 100, y + 104, 64, 44, 10, a ? Mix(kOk, kInk, 160) : kOk);
            Ico(dc, IC_CALL, x + 116, y + 110, 32, kInk);
        }
        bool e = Zone(x + 172, y + 104, 64, 44, A_BTCMD, BT_CMD_END);
        R(dc, x + 172, y + 104, 64, 44, 10, e ? Mix(kStop, kInk, 160) : kStop);
        Ico(dc, IC_HANGUP, x + 188, y + 110, 32, kInk);
        return;
    }
    MultiByteToWideChar(CP_UTF8, 0, bt.operatorName, -1, s, 96);
    s[95] = 0;
    T(dc, x + 16, y + 94, s[0] ? s : L"подключён", 22, big, 700, AL_START, 150);
    wchar_t bs[16] = L"—", ss[16] = L"—";
    if (bt.battery >= 0)
        _snwprintf(bs, 16, L"%d/5", bt.battery);
    if (bt.signal >= 0)
        _snwprintf(ss, 16, L"%d/5", bt.signal);
    _snwprintf(s, 96, L"батарея %s · сеть %s", bs, ss);
    s[95] = 0;
    T(dc, x + 16, y + 122, s, 15, mut, 400, AL_START, 150);
    bool c = Zone(x + 172, y + 98, 64, 48, A_SCREEN, S_PHONE);   // the keypad
    R(dc, x + 172, y + 98, 64, 48, 10, c ? Mix(kOk, kInk, 160) : kOk);
    Ico(dc, IC_CALL, x + 188, y + 106, 32, kInk);
}

// The first three programs of the list; the widget itself leads to all of them.
static void WFavorites(HDC dc, int x, int y)
{
    bool d = Widget(dc, x, y, 248, 158, L"Приложения", IC_APPS, A_NAV, S_APPS);
    if (s_appCount == 0) {
        TWrap(dc, x + 16, y + 92, L"Добавьте программы в «Приложениях»", 16, d ? kInk : kMuted, 400, 216, 2, 22);
        return;
    }
    for (int i = 0; i < 3 && i < s_appCount; i++) {
        int cx = x + 8 + i * 78;
        bool p = Zone(cx, y + 44, 76, 106, A_RUN, i);
        if (p)
            R(dc, cx, y + 44, 76, 106, 10, kOrange);
        AppIcon(dc, s_apps[i], cx + 16, y + 54, 44, p || d);
        T(dc, cx + 38, y + 132, s_apps[i].name, 12, p || d ? kInk : kMuted, 700, AL_MID, 72);
    }
}

// Navigation: starts the first program of the list (the example list begins with the maps).
// OSMapsCE among the programs: the first whose path has "OSMaps" (OSMapsCE.exe; OSMapsPC.exe on the PC); -1 none.
static int NaviApp()
{
    for (int i = 0; i < s_appCount; i++) {
        wchar_t lp[MAX_PATH];
        wcscpy(lp, s_apps[i].path);
        CharLowerBuff(lp, (DWORD)wcslen(lp));
        if (wcsstr(lp, L"osmaps"))
            return i;
    }
    return -1;
}

static void WNavi(HDC dc, int x, int y)
{
    int app = NaviApp();
    const wchar_t* name = app >= 0 ? s_apps[app].name : L"OSMapsCE не найден";
    bool d = Widget(dc, x, y, 248, 158, L"Навигация", IC_NAV, app >= 0 ? A_RUN : A_NAV, app >= 0 ? app : S_APPS);
    Ico(dc, IC_MAP, x + 16, y + 52, 48, d ? kInk : kText);
    T(dc, x + 78, y + 74, name, 19, d ? kInk : kText, 700, AL_START, 154);
    TWrap(dc, x + 78, y + 96, L"маршрут не задан", 15, d ? kInk : kMuted, 400, 154, 1, 0);
    TWrap(dc, x + 16, y + 144, app >= 0 ? L"нажмите, чтобы открыть карты" : L"добавьте его в «Приложениях»", 14,
          d ? kInk : kDim, 400, 216, 1, 0);
}

// "ШТАТНАЯ" (the user 2026-10-22): the stock shell instead of VestaHome. Under VestaStub (G3NavHMI_orig.exe beside
// it) VestaHome only closes: on a clean exit the stub starts the shell itself (and stops its MCU host first);
// otherwise the shell is started when it does not run (it normally runs beside VestaHome).
static void ShellStart()
{
#ifdef UNDER_CE
    if (GetFileAttributes(L"\\ResidentFlash\\navigation\\app\\G3NavHMI_orig.exe") != 0xFFFFFFFF) {
        VLog(L"штатная оболочка: её запустит загрузчик VestaHome");
        return;
    }
    TaskInfo* t = (TaskInfo*)calloc(1, sizeof(TaskInfo));
    bool running = false;
    if (t) {
        TaskSample(t, s_wnd);
        for (int i = 0; i < t->procCount; i++)
            running = running || !_wcsicmp(t->proc[i].name, L"G3NavHMI.exe");
        free(t);
    }
    if (!running) {
        PROCESS_INFORMATION pi;
        memset(&pi, 0, sizeof(pi));
        if (CreateProcess(L"\\ResidentFlash\\navigation\\app\\G3NavHMI.exe", NULL, NULL, NULL, FALSE, 0, NULL, NULL,
                          NULL, &pi)) {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
        }
    }
    VLog(L"штатная оболочка: %s", running ? L"уже работает" : L"запущена");
#endif
}

// ---- the car's sensors (2026-10-22): what VestaHome knows of the car and the unit, for the car widget's lines and
// the "Car" screen. The CAN module gives only the speed and the steering angle (its firmware, PROJECT.md), the
// MCU the unit's temperatures (System status 0x7905), the OBD adapter the engine once it connects.
enum { kCarSensors = 27 };
static const wchar_t* const kCarSensor[kCarSensors][2] = {
    { L"Скорость (CAN)", L"км/ч" }, { L"Угол руля", L"°" }, { L"Пробег поездки", L"км" },
    { L"В движении", L"ч:мин" }, { L"Средняя скорость", L"км/ч" }, { L"Макс. скорость", L"км/ч" },
    { L"Обороты (OBD)", L"об/мин" }, { L"Охл. жидкость (OBD)", L"°C" }, { L"Напряжение (OBD)", L"В" },
    { L"Скорость (OBD)", L"км/ч" }, { L"Плата ГУ", L"°C" }, { L"Усилитель", L"°C" }, { L"Экран", L"°C" },
    { L"Поле тюнера", L"дБ" }, { L"CAN-модуль", L"" }, { L"Спутники GPS", L"" }, { L"Высота GPS", L"м" },
    { L"Нагрузка (OBD)", L"%" }, { L"Дроссель (OBD)", L"%" }, { L"Впуск (OBD)", L"°C" }, { L"За бортом (OBD)", L"°C" },
    { L"Топливо (OBD)", L"%" }, { L"Расход (OBD)", L"л/100 км" }, { L"Расход стоя (OBD)", L"л/ч" },
    { L"Ошибки (OBD)", L"" }, { L"Расход поездки (OBD)", L"л/100 км" }, { L"Запас хода (OBD)", L"км" } };

static const wchar_t* CarSensorName(int id)
{
    return id >= 0 && id < kCarSensors ? kCarSensor[id][0] : L"";
}

static bool CarSensor(int id, wchar_t* v, int n)   // its value with the unit; false = no data
{
    v[0] = 0;
    Gps g;
    GpsGet(&g);
    DWORD now = GetTickCount();
    int x = -1;
    switch (id) {
    case 0: x = g.carTick && now - g.carTick < 3000 ? g.carSpeed : -1; break;
    case 1:
        if (!g.angTick || now - g.angTick > 3000)
            return false;
        _snwprintf(v, n, L"%+d,%d°", g.angle10 / 10, abs(g.angle10) % 10);
        return true;
    case 2:
        if (!s_trip.started)
            return false;
        _snwprintf(v, n, L"%d,%d км", (int)s_trip.km, (int)(s_trip.km * 10) % 10);
        return true;
    case 3:
        if (!s_trip.started)
            return false;
        _snwprintf(v, n, L"%u:%02u", (unsigned)(s_trip.movingS / 3600), (unsigned)(s_trip.movingS / 60 % 60));
        return true;
    case 4: x = s_trip.movingS >= 60 ? (int)(s_trip.km * 3600 / s_trip.movingS + 0.5) : -1; break;
    case 5: x = s_trip.maxKmh > 0 ? s_trip.maxKmh : -1; break;
    case 6: case 7: case 8: case 9: case 17: case 18: case 19: case 20: case 21: case 22: case 23: case 24: {
        ObdInfo o;
        ObdGet(&o);
        if (o.state != OBD_ON)
            return false;
        if (id == 8) {
            wcsncpy(v, o.volt, n - 1);
            v[n - 1] = 0;
            return v[0] != 0;
        }
        if (id == 19 || id == 20) {   // the temperatures may be below 0
            int t = id == 19 ? o.intake : o.ambient;
            if (t <= -1000)
                return false;
            _snwprintf(v, n, L"%d °C", t);
            return true;
        }
        if (id == 22 || id == 23) {   // the fuel from the air flow: L/h = g/s x 3600 / (14.7 x 745 g/L)
            if (o.maf100 < 0)
                return false;
            double lh = o.maf100 / 100.0 * 3600 / (14.7 * 745);
            if (id == 22 && o.speed < 5)
                return false;   // standing: per 100 km means nothing
            double val = id == 22 ? lh * 100 / o.speed : lh;
            _snwprintf(v, n, L"%d,%d %s", (int)val, (int)(val * 10) % 10, kCarSensor[id][1]);
            return true;
        }
        if (id == 24) {
            if (o.dtcs < 0)
                return false;
            if (!o.dtcs && !o.mil)
                wcscpy(v, L"нет");
            else
                _snwprintf(v, n, L"%d%s", o.dtcs, o.mil ? L" · лампа" : L"");
            return true;
        }
        x = id == 6 ? o.rpm : id == 7 ? o.coolant : id == 9 ? o.speed : id == 17 ? o.load : id == 18 ? o.throttle
          : o.fuel;
        break;
    }
    case 10: case 11: case 12: case 13: {   // the MCU's System status: its subs 0A, 06, 0D, 07
        static const int sub[4] = { 0x0A, 0x06, 0x0D, 0x07 };
        if (!s_mcuCsReady)
            return false;
        EnterCriticalSection(&s_mcuCs);
        x = s_info.sys[sub[id - 10]];
        LeaveCriticalSection(&s_mcuCs);
        break;
    }
    case 14:
        if (!g.canVer[0])
            return false;
        _snwprintf(v, n, L"v%S", g.canVer);
        return true;
    case 15: x = GpsFresh(g, 5000) || g.sats ? g.sats : -1; break;
    case 25: {
        double m = TripMean();
        if (m < 0)
            return false;
        _snwprintf(v, n, L"%d,%d %s", (int)m, (int)(m * 10) % 10, kCarSensor[id][1]);
        return true;
    }
    case 26: x = TripRange(); break;
    case 16: x = GpsFresh(g, 5000) ? (int)(g.alt + 0.5) : -1; break;
    }
    if (x < 0 && id != 1)
        return false;
    _snwprintf(v, n, L"%d %s", x, kCarSensor[id][1]);
    v[n - 1] = 0;
    return true;
}

// Car -> "Ошибки (OBD)": the stored trouble codes (03) and their clearing (04, asked twice: it also puts out the
// "Check engine" lamp and resets the engine's learnt values).
static bool s_dtcArm;   // "СБРОСИТЬ" pressed once: the next press clears
enum { kDtcInfoMax = 3 };
static char    s_dtcCode[6];                // the code whose meaning is shown ("" none)
static wchar_t s_dtcInfo[kDtcInfoMax][400]; // its lines of obd_codes.txt past the code: ECU;system;meaning[;more]
static int     s_dtcInfoN;                  // -1 no file

// A code's meaning (a tap on it): its lines of obd_codes.txt next to the exe (tools/make_obd_codes.py), read on
// each tap, nothing kept but them.
static void DtcInfo(const char* code)
{
    strncpy(s_dtcCode, code, 5);
    s_dtcCode[5] = 0;
    s_dtcInfoN = -1;
    wchar_t path[MAX_PATH], wc[8];
    _snwprintf(path, MAX_PATH, L"%sobd_codes.txt", s_dir);
    MultiByteToWideChar(CP_ACP, 0, s_dtcCode, -1, wc, 8);
    DWORD size;
    char* buf = ReadAll(path, &size);
    if (!buf)
        return;
    s_dtcInfoN = 0;
    char* p = Utf8Text(buf);
    wchar_t line[400];
    while (s_dtcInfoN < kDtcInfoMax && NextUtf8Line(&p, line, 400))
        if (!wcsncmp(line, wc, 5) && line[5] == L';')
            wcscpy(s_dtcInfo[s_dtcInfoN++], line + 6);
    free(buf);
}

// The meaning's window over the codes: per line the system (bold), the meaning and more, the ECU (small)
static void DtcInfoWindow(HDC dc)
{
    Zone(0, 0, 800, 480, A_DTCINFO, -1);   // a tap beside it closes it (and the codes under it take none)
    Panel(dc, 64, 64, 672, 352);
    wchar_t s[64], code[8];
    MultiByteToWideChar(CP_ACP, 0, s_dtcCode, -1, code, 8);
    T(dc, 88, 104, code, 26, kOrange, 700);
    int y = 132;
    if (s_dtcInfoN <= 0)
        T(dc, 88, y + 24, s_dtcInfoN < 0 ? L"Нет файла расшифровки obd_codes.txt (переустановите VestaHome)"
                                        : L"Этого кода нет в таблице расшифровки", 17, kMuted, 400, AL_START, 624);
    for (int i = 0; i < s_dtcInfoN && y < 340; i++) {
        wchar_t f[5][400];   // ECU, system, meaning, more
        int nf = 0;
        const wchar_t* q = s_dtcInfo[i];
        while (nf < 5) {
            const wchar_t* e = wcschr(q, L';');
            int len = e ? (int)(e - q) : (int)wcslen(q);
            wcsncpy(f[nf], q, len);
            f[nf++][len] = 0;
            if (!e)
                break;
            q = e + 1;
        }
        wchar_t more[800] = L"";
        for (int k = 2; k < nf; k++)
            if (f[k][0]) {
                if (more[0])
                    wcscat(more, L". ");
                wcsncat(more, f[k], 799 - wcslen(more));
            }
        if (nf > 1 && f[1][0])
            y += 22 * TWrap(dc, 88, y + 22, f[1], 18, kText, 700, 624, 2, 22);
        if (more[0])
            y += 20 * TWrap(dc, 88, y + 22, more, 16, kText, 400, 624, 3, 20);
        _snwprintf(s, 64, L"блок: %s", f[0]);
        s[63] = 0;
        T(dc, 88, y + 20, s, 13, kDim, 400, AL_START, 624);
        y += 34;
    }
    Button(dc, 300, 348, 200, 48, L"Закрыть", B_N, IC_NONE, 17, 0, A_DTCINFO, -1);
}

static void ScrObdDtc(HDC dc)
{
    Frame(dc, L"Автомобиль · Ошибки двигателя (OBD)");
    ObdInfo o;
    ObdGet(&o);
    bool on = o.state == OBD_ON, busy = o.dtcState == OBD_DTC_READING || o.dtcState == OBD_DTC_CLEARING;
    Panel(dc, 16, 68, 768, 324);
    wchar_t s[96];
    if (!on)
        wcscpy(s, L"OBD-адаптер не подключён: Настройки → Bluetooth → OBD-адаптер");
    else if (o.dtcState == OBD_DTC_READING)
        wcscpy(s, L"Читаю коды…");
    else if (o.dtcState == OBD_DTC_CLEARING)
        wcscpy(s, L"Сбрасываю…");
    else if (o.dtcState == OBD_DTC_FAILED)
        wcscpy(s, L"Блок не ответил — повторите при включённом зажигании");
    else if (o.dtcState == OBD_DTC_READ)
        _snwprintf(s, 96, o.dtcN ? L"Сохранённых ошибок: %d" : L"Сохранённых ошибок нет", o.dtcN);
    else
        wcscpy(s, L"Нажмите ПРОЧИТАТЬ");
    s[95] = 0;
    T(dc, 32, 104, s, 20, on ? kText : kOrange, 700, AL_START, 736);
    if (on && o.mil >= 0)
        T(dc, 32, 134, o.mil ? L"Лампа «Check engine» горит" : L"Лампа «Check engine» не горит", 16,
          o.mil ? kOrange : kMuted, 400, AL_START, 736);
    if (o.dtcState == OBD_DTC_READ)
        for (int i = 0; i < o.dtcN; i++) {   // four columns
            wchar_t c[8];
            MultiByteToWideChar(CP_ACP, 0, o.dtc[i], -1, c, 8);
            int x = 32 + i % 4 * 184, y = 160 + i / 4 * 52;
            bool d = Zone(x, y, 172, 44, A_DTCINFO, i);   // a tap: its meaning
            R(dc, x, y, 172, 44, 22, d ? kOrange : kPanel2);
            T(dc, x + 86, y + 30, c, 22, d ? kInk : kText, 700, AL_MID, 160);
        }
    T(dc, 32, 380, L"P — двигатель и коробка, C — шасси, B — кузов, U — сеть. Нажмите на код — расшифровка.", 14,
      kDim, 400, AL_START, 736);
    Dock(dc);
    Button(dc, 200, 412, 190, 52, L"Прочитать", on && !busy ? B_N : B_DIS, IC_NONE, 17, 0,
           on && !busy ? A_DTCREAD : A_NONE);
    Button(dc, 410, 412, 190, 52, s_dtcArm ? L"Точно сбросить?" : L"Сбросить",
           !on || busy ? B_DIS : s_dtcArm ? B_FILL : B_N, IC_NONE, 17, 0, on && !busy ? A_DTCCLEAR : A_NONE);
    if (s_dtcCode[0])
        DtcInfoWindow(dc);
}

// Car -> its settings: the widget's three lines, from the sensors (a tap: in / out, in the order chosen)
static void ScrWSetCar(HDC dc)
{
    Frame(dc, L"Автомобиль · Строки виджета");
    int n = 0;
    for (int i = 0; i < 3; i++)
        n += s_cfg.carSens[i] >= 0;
    wchar_t s[64];
    _snwprintf(s, 64, L"Выберите до трёх показаний: выбрано %d. «—» — данных пока нет.", n);
    T(dc, 24, 92, s, 16, kMuted, 400, AL_START, 752);
    for (int id = 0; id < kCarSensors; id++) {
        int at = -1;
        for (int i = 0; i < 3; i++)
            if (s_cfg.carSens[i] == id)
                at = i;
        wchar_t v[32], label[64];
        bool have = CarSensor(id, v, 32);
        _snwprintf(label, 64, at >= 0 ? L"%d. %s" : L"%s", at + 1, kCarSensor[id][0]);
        if (at < 0)
            wcscpy(label, kCarSensor[id][0]);
        int bx = 16 + id / 9 * 258, by = 104 + id % 9 * 34;   // (above y 408: "back" at the bottom left)
        Button(dc, bx, by, 250, 30, NULL, at >= 0 ? B_SEL : B_N, IC_NONE, 14, 0, A_CARSENS, id);
        T(dc, bx + 12, by + 21, label, 13, at >= 0 ? kOrange : kText, 700, AL_START, 150);
        T(dc, bx + 238, by + 21, have ? v : L"—", 13, at >= 0 ? kOrange : kMuted, 600, AL_END, 90);
    }
}
