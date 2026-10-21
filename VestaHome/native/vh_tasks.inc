// vh_tasks.inc - part of home.cpp (included there, in order): the task manager.

// ---------------------------------------------------------------- the main screen: widgets
// Two pages of five widgets (3 x 2 grid of 248 x 158 cells, the first of each page two cells wide), swiped or
// turned with the dots. A widget opens its screen; small buttons inside it act on their own; a caption with a gear
// opens the widget's settings. Real: the date, the programs, the weather, the speed (GPS) and the cameras; the
// rest are demo values until the car's bus through the MCU is known.

// ---- task manager (Apps -> the button in the top bar): processes and windows (VestaCore vtask), the CPU
// and memory graphs over the last 2 minutes. Sampled every 2 s while the screen is in front, never in the
// background. A process ends on the second press of its button; system ones and VestaHome itself can't.

enum { kTaskRows = 5 };
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
    R(dc, x, y, w, 104, 14, kPanel, kLine);
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
    Frame(dc, L"ДИСПЕТЧЕР ЗАДАЧ");
    if (s_task.cpu10 >= 0)
        _snwprintf(s, 96, L"%d %%", (s_task.cpu10 + 5) / 10);
    else
        wcscpy(s, L"...");
    TaskGraph(dc, 16, 64, 380, L"ПРОЦЕССОР", s, s_task.cpuHist, kOrange);
    DWORD total = s_task.memTotalKB, used = total - s_task.memAvailKB;
    MemText(a, 24, used);
    MemText(b, 24, total);
    _snwprintf(s, 96, L"%s из %s · %u %%", a, b, total ? (unsigned)(used * 100 / total) : 0);
    TaskGraph(dc, 404, 64, 380, L"ПАМЯТЬ", s, s_task.memHist, kText);

    _snwprintf(s, 96, L"ПРОЦЕССЫ %d", s_task.procCount);
    Button(dc, 16, 176, 172, 48, s, s_taskTab == 0 ? B_SEL : B_N, IC_NONE, 16, 0, A_TASKTAB, 0);
    _snwprintf(s, 96, L"ОКНА %d", s_task.winCount);
    Button(dc, 196, 176, 128, 48, s, s_taskTab == 1 ? B_SEL : B_N, IC_NONE, 16, 0, A_TASKTAB, 1);
    if (s_taskTab == 0) {
        TaskSortBtn(dc, 336, L"ИМЯ", TS_NAME);
        TaskSortBtn(dc, 446, L"ЦП", TS_CPU);
        TaskSortBtn(dc, 556, L"ПАМЯТЬ", TS_MEM);
    }
    int pages = TaskPages();
    if (s_taskPage >= pages)
        s_taskPage = pages - 1;
    Button(dc, 668, 176, 40, 48, NULL, B_N, IC_BACK, 18, 24, A_TASKPAGE, -1);
    _snwprintf(s, 96, L"%d/%d", s_taskPage + 1, pages);
    T(dc, 726, 207, s, 15, kMuted, 600, AL_MID);
    Button(dc, 744, 176, 40, 48, NULL, B_N, IC_NEXT, 18, 24, A_TASKPAGE, 1);

    if (s_taskTab == 0 && !s_task.procOk) {
        T(dc, 400, 350, L"Список процессов недоступен: нет toolhelp.dll", 18, kMuted, 600, AL_MID, 760);
        return;
    }
    for (int k = 0; k < kTaskRows; k++) {
        int i = s_taskPage * kTaskRows + k, y = 232 + k * 48;
        if (s_taskTab == 0) {
            if (i >= s_task.procCount)
                break;
            const TaskProc& p = s_task.proc[i];
            bool self = p.pid == GetCurrentProcessId(), sys = TaskIsSystem(p.name);
            R(dc, 16, y, 768, 44, 10, kPanel);
            T(dc, 32, y + 29, p.name, 18, self ? kOrange : kText, 700, AL_START, 250);
            _snwprintf(s, 96, L"%d пот.", p.threads);
            T(dc, 296, y + 28, s, 15, kMuted, 400);
            if (p.cpu10 >= 0)
                _snwprintf(s, 96, L"%d,%d %%", p.cpu10 / 10, p.cpu10 % 10);
            else
                wcscpy(s, L"...");
            T(dc, 470, y + 29, s, 17, p.cpu10 >= 200 ? kOrange : kText, 700, AL_END);
            if (p.memKB)
                MemText(s, 96, p.memKB);
            else
                wcscpy(s, L"-");
            T(dc, 604, y + 29, s, 17, kText, 700, AL_END);
            if (sys || self)
                T(dc, 700, y + 28, sys ? L"система" : L"эта программа", 14, kDim, 600, AL_MID);
            else
                Button(dc, 624, y + 4, 152, 36, s_taskArm == p.pid ? L"ТОЧНО?" : L"ЗАВЕРШИТЬ",
                       s_taskArm == p.pid ? B_PRI : B_N, IC_NONE, 15, 0, A_TASKKILL, (int)p.pid);
        } else {
            if (i >= s_task.winCount)
                break;
            const TaskWin& w = s_task.win[i];
            R(dc, 16, y, 768, 44, 10, kPanel);
            T(dc, 32, y + 29, w.title, 18, kText, 700, AL_START, 300);
            T(dc, 346, y + 28, w.proc, 15, kMuted, 400, AL_START, 190);
            Button(dc, 544, y + 4, 112, 36, L"ПЕРЕЙТИ", B_N, IC_NONE, 15, 0, A_TASKSWITCH, (int)(INT_PTR)w.h);
            Button(dc, 664, y + 4, 112, 36, L"ЗАКРЫТЬ", B_N, IC_NONE, 15, 0, A_TASKCLOSE, (int)(INT_PTR)w.h);
        }
    }
    if (s_taskTab == 1 && !s_task.winCount)
        T(dc, 400, 350, L"Других окон нет", 18, kMuted, 600, AL_MID);
}

static int s_homePage;
// The widgets' grid: 3 x 2 on a page, a single 248 x 158, a double two singles and the gap.
enum { kWgW = 248, kWgH = 158, kWgGap = 12, kWgW2 = kWgW * 2 + kWgGap, kWgPages = 6 };

// Widget panel with its caption; pressed = orange like a tile (returns it: the content follows in ink).
// With a settings screen the caption strip is a button of its own (a gear at its end) leading there.
static bool Widget(HDC dc, int x, int y, int w, int h, const wchar_t* caption, Icon ic, Action a, int arg = 0,
                   Screen settings = S_NONE)
{
    bool d = a != A_NONE && Zone(x, y, w, h, a, arg);
    bool hd = settings != S_NONE && Zone(x, y, w, 42, A_SCREEN, settings);
    R(dc, x, y, w, h, 14, d ? kOrange : kPanel, d ? CLR_INVALID : kLine);
    if (hd)
        R(dc, x + 2, y + 2, w - 4, 40, 12, kOrange);
    Ico(dc, ic, x + 16, y + 12, 22, d || hd ? kInk : kOrange);
    T(dc, x + 46, y + 29, caption, 13, d || hd ? kInk : kMuted, 700, AL_START, w - (settings != S_NONE ? 90 : 62));
    if (settings != S_NONE)
        Ico(dc, IC_GEAR, x + w - 34, y + 12, 20, d || hd ? kInk : kDim);
    return d;
}

// The player's track (or "nothing plays" with play = the last track / the list); the progress updates with the
// minute redraw, the player screen itself updates every second.
static void WMedia(HDC dc, int x, int y)
{
    if (s_netOn && s_netCur >= 0) {   // the internet radio: the station, the song or its state
        bool d = Widget(dc, x, y, kWgW2, kWgH, L"СЕЙЧАС ИГРАЕТ · ИНТЕРНЕТ-РАДИО", IC_RADIO, A_NAV, S_NETRADIO);
        TriFill(x + 16, y + 42, 100, 100, 12);
        Ico(dc, IC_RADIO, x + 40, y + 66, 52, Mix(kInk, kTriOr[1], 140));
        wchar_t s[160];
        EnterCriticalSection(&s_netCs);
        wcscpy(s, s_netTitle);
        LeaveCriticalSection(&s_netCs);
        if (!s[0] || s_netState != NR_PLAY)
            NetStateText(s, 160);
        T(dc, x + 136, y + 70, s_stations[s_netCur].name, 26, d ? kInk : kText, 700, AL_START, 352);
        TWrap(dc, x + 136, y + 94, s, 16, d ? kInk : kMuted, 400, 352, 1, 0);
        IconBtn(dc, IC_PREV, x + 196, y + 122, 28, d ? kInk : kText, A_NRSTEP, -1);
        NetPlayBtn(dc, x + 312, y + 136, 20, 22);
        IconBtn(dc, IC_FWD, x + 400, y + 122, 28, d ? kInk : kText, A_NRSTEP, 1);
        return;
    }
    bool d = Widget(dc, x, y, kWgW2, kWgH, L"СЕЙЧАС ИГРАЕТ · МУЗЫКА", IC_MUSIC, A_NAV, S_AUDIO);
    COLORREF fg = d ? kInk : kText, mg = d ? kInk : kMuted;
    TriFill(x + 16, y + 42, 100, 100, 12);
    Ico(dc, IC_MUSIC, x + 40, y + 66, 52, Mix(kInk, kTriOr[1], 140));
    int tx = x + 136;
    wchar_t title[96], sub[200], s[48], a[16], b[16];
    double pos = 0, len = 0;
    if (PlayerNow(title, sub, 96, &pos, &len)) {
        T(dc, tx, y + 70, title, 26, fg, 700, AL_START, 352);
        _snwprintf(s, 48, L"трек %d из %d", s_plCur + 1, s_plCount);
        TWrap(dc, tx, y + 94, sub[0] ? sub : s, 16, mg, 400, 260, 1, 0);
        Mmss(a, 16, pos);
        Mmss(b, 16, len);
        _snwprintf(s, 48, L"%s / %s", a, b);
        T(dc, x + kWgW2 - 16, y + 94, s, 15, mg, 600, AL_END);
        int k = len > 0 ? (int)(pos * 352 / len) : 0;
        R(dc, tx, y + 106, 352, 6, 3, d ? kInk : kLine);
        R(dc, tx, y + 106, k > 6 ? k : 6, 6, 3, d ? kInk : kOrange);
    } else {   // nothing loaded: the last track (dim) to go on with, or a dash
        wchar_t last[MAX_PATH];
        const wchar_t* fn = wcsrchr(s_cfg.musicFile, L'\\');
        wcscpy(last, fn ? fn + 1 : s_cfg.musicFile);
        wchar_t* dot = wcsrchr(last, L'.');
        if (dot)
            *dot = 0;
        T(dc, tx, y + 70, last[0] ? last : L"—", 26, last[0] ? mg : fg, 700, AL_START, 352);
        TWrap(dc, tx, y + 94, last[0] ? L"последний трек · ▶ — продолжить" : L"выберите музыку в «Медиа»", 16, mg,
              400, 352, 1, 0);
        R(dc, tx, y + 106, 352, 6, 3, d ? kInk : kLine);
    }
    IconBtn(dc, IC_PREV, tx + 60, y + 122, 28, fg, A_PREVTRK);
    PlayBtn(dc, tx + 176, y + 136, 20, 22);
    IconBtn(dc, IC_FWD, tx + 264, y + 122, 28, fg, A_NEXTTRK);
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
    Widget(dc, x, y, 248, 158, car ? L"СКОРОСТЬ · CAN" : L"СКОРОСТЬ · GPS", IC_NAV, A_NONE, 0, S_WSET_SPEED);
    wchar_t v[64];
    if (car)
        _snwprintf(v, 8, L"%d", g.carSpeed);
    else if (fix)
        _snwprintf(v, 8, L"%d", (int)(g.speedKmh + 0.5));
    else
        wcscpy(v, L"—");
    int end = T(dc, x + 16, y + 112, v, 72, fix || car ? kText : kDim, 700);
    if (s_camNear >= 0) {
        const Cam& cm = s_cams[s_camNear];
        Disc(dc, x + 204, y + 80, 28, kStop);
        Disc(dc, x + 204, y + 80, 21, kText);
        if (cm.speed) {
            _snwprintf(v, 8, L"%d", cm.speed);
            T(dc, x + 204, y + 88, v, 20, kInk, 700, AL_MID);
        } else {
            T(dc, x + 204, y + 85, L"КАМ", 12, kInk, 700, AL_MID);
        }
        _snwprintf(v, 16, L"%d м", s_camNearDist);
        T(dc, x + 204, y + 128, v, 14, kOrange, 700, AL_MID);
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
    bool d = Widget(dc, x, y, 248, 158, t.band == 2 || (t.f10 > 0 && t.f10 < 3000) ? L"РАДИО · СВ"
                    : t.band == 1 || (t.f10 >= 6500 && t.f10 < 8750) ? L"РАДИО · УКВ" : L"РАДИО · FM", IC_RADIO, A_NAV,
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
    if (w.have && w.place[0])
        _snwprintf(cap, 64, L"ПОГОДА · %s", w.place);
    else
        wcscpy(cap, L"ПОГОДА");
    CharUpperBuff(cap, (DWORD)wcslen(cap));
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

static void WCar(HDC dc, int x, int y)
{
    static const wchar_t* rows[3][2] = { { L"Двигатель", L"90 °C" }, { L"Аккумулятор", L"14,2 В" },
                                         { L"ТО через", L"3 200 км" } };
    bool d = Widget(dc, x, y, 248, 158, L"АВТОМОБИЛЬ", IC_CAR, A_SCREEN, S_CAR, S_WSET_CAR);
    Disc(dc, x + 24, y + 58, 7, d ? kInk : kOk);
    T(dc, x + 40, y + 65, L"Ошибок нет", 19, d ? kInk : kText, 700);
    for (int i = 0; i < 3; i++) {
        T(dc, x + 16, y + 94 + i * 24, rows[i][0], 15, d ? kInk : kMuted, 400, AL_START, 140);
        T(dc, x + 232, y + 94 + i * 24, rows[i][1], 15, d ? kInk : kText, 700, AL_END);
    }
}

// The trip (2026-10-21, out of the demo): since VestaHome started (the ignition), counted once a second from the
// car's speed (CAN, the MCU's 0x412A) or else GPS: the distance, the time moving (3 km/h and more), the mean of it
// and the top speed. Fuel and range come with OBD.
struct Trip { double km; DWORD movingS; int maxKmh; SYSTEMTIME start; bool started; int src; };   // src 1 CAN 2 GPS
static Trip s_trip;

static void TripTick()   // once a second
{
    if (!s_trip.started) {
        GetLocalTime(&s_trip.start);
        s_trip.started = true;
    }
    Gps g;
    GpsGet(&g);
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
    s_trip.movingS++;
    if (kmh > s_trip.maxKmh)
        s_trip.maxKmh = kmh;
}

static void WTrip(HDC dc, int x, int y)
{
    bool d = Widget(dc, x, y, kWgW2, kWgH, s_trip.src == 1 ? L"БОРТОВОЙ КОМПЬЮТЕР · ПОЕЗДКА · CAN"
                    : L"БОРТОВОЙ КОМПЬЮТЕР · ПОЕЗДКА · GPS", IC_CLOCK, A_SCREEN, S_CAR);
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
    const wchar_t* head[4] = { L"ПРОБЕГ", L"В ДВИЖЕНИИ", L"СРЕДНЯЯ", L"МАКС." };
    const wchar_t* unit[4] = { L"км", L"ч:мин", L"км/ч", L"км/ч" };
    for (int i = 0; i < 4; i++) {
        int cx = x + 16 + i * 120;
        if (i)
            Fill(dc, cx - 8, y + 50, cx - 6, y + 126, d ? kInk : kLine);
        T(dc, cx, y + 62, head[i], 12, d ? kInk : kMuted, 700, AL_START, 108);
        T(dc, cx, y + 102, v[i], 32, d ? kInk : kText, 700, AL_START, 108);
        T(dc, cx, y + 122, unit[i], 14, d ? kInk : kMuted, 400);
    }
    wchar_t s[64];
    if (s_trip.started)
        _snwprintf(s, 64, L"Поездка с %02d:%02d · расход и запас хода — с OBD", s_trip.start.wHour,
                   s_trip.start.wMinute);
    else
        wcscpy(s, L"Расход и запас хода — с OBD");
    s[63] = 0;
    T(dc, x + 16, y + 146, s, 14, d ? kInk : kDim, 400, AL_START, kWgW2 - 32);
}

static bool SunNight(int* rise, int* set);

static void WDate(HDC dc, int x, int y)
{
    SYSTEMTIME t;
    GetLocalTime(&t);
    wchar_t day[4];
    _snwprintf(day, 4, L"%d", t.wDay);
    bool d = Widget(dc, x, y, 248, 158, L"КАЛЕНДАРЬ", IC_CLOCK, A_NAV, S_SETTINGS);
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
        Widget(dc, x, y, 248, 158, L"КЛИМАТ", IC_CAR, A_SCREEN, S_TEST_MCU, S_WSET_CAR);
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
        R(dc, x + 16, y + 120, 56, 28, 8, h.ac == 1 ? kOrange : CLR_INVALID, h.ac == 1 ? CLR_INVALID : kLine);
        T(dc, x + 44, y + 140, L"A/C", 14, h.ac == 1 ? kInk : kMuted, 700, AL_MID);
        R(dc, x + 80, y + 120, 64, 28, 8, h.autoMode == 2 ? kOrange : CLR_INVALID, h.autoMode == 2 ? CLR_INVALID : kLine);
        T(dc, x + 112, y + 140, L"АВТО", 13, h.autoMode == 2 ? kInk : kMuted, 700, AL_MID);
        R(dc, x + 152, y + 120, 80, 28, 8, h.rear == 1 ? kOrange : CLR_INVALID, h.rear == 1 ? CLR_INVALID : kLine);
        T(dc, x + 192, y + 140, L"ОБОГРЕВ", 12, h.rear == 1 ? kInk : kMuted, 700, AL_MID);
        if (h.outer > -100) {
            _snwprintf(s, 24, L"за бортом %+d°", h.outer);
            T(dc, x + 232, y + 54, s, 13, kMuted, 600, AL_END);
        }
        return;
    }
    Widget(dc, x, y, 248, 158, L"КЛИМАТ · ДЕМО", IC_CAR, A_NONE, 0, S_WSET_CAR);
    T(dc, x + 16, y + 100, L"22,0°", 46, kText, 700);
    T(dc, x + 156, y + 72, L"АВТО", 15, kOrange, 700);
    for (int i = 0; i < 5; i++)   // fan speed 3 of 5
        R(dc, x + 156 + i * 15, y + 100 - (8 + i * 4), 10, 8 + i * 4, 2, i < 3 ? kText : kLine);
    R(dc, x + 16, y + 116, 64, 30, 8, kOrange);
    T(dc, x + 48, y + 137, L"A/C", 15, kInk, 700, AL_MID);
    R(dc, x + 88, y + 116, 104, 30, 8, CLR_INVALID, kLine);
    T(dc, x + 140, y + 137, L"РЕЦИРК.", 13, kMuted, 700, AL_MID);
}

// The phone (2026-10-21, out of the demo): the shell's Bluetooth through our proxy - the phone connected, its
// operator, battery and signal, a call going on (answer / hang up here). Not connected: the last phone's name.
// The last calls come with the phone book (PBAP).
static void WPhone(HDC dc, int x, int y)
{
    BtState bt;
    bool proxy = BtGet(&bt, NULL), on = proxy && bt.hf == 2;
    wchar_t title[96], name[BT_TEXT], s[96];
    wcscpy(title, L"ТЕЛЕФОН");
    if (on && bt.devName[0]) {
        BtText(bt.devName, name, BT_TEXT);
        _snwprintf(title, 96, L"ТЕЛЕФОН · %s", name);
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
        const wchar_t* st = call == BT_CALL_INCOMING || call == BT_CALL_WAITING ? L"ВХОДЯЩИЙ"
                          : call == BT_CALL_DIALING || call == BT_CALL_ALERTING ? L"ВЫЗОВ"
                          : call == BT_CALL_HELD ? L"НА УДЕРЖАНИИ" : L"РАЗГОВОР";
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
    bool d = Widget(dc, x, y, 248, 158, L"ПРИЛОЖЕНИЯ", IC_APPS, A_NAV, S_APPS);
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
    bool d = Widget(dc, x, y, 248, 158, L"НАВИГАЦИЯ", IC_NAV, app >= 0 ? A_RUN : A_NAV, app >= 0 ? app : S_APPS);
    Ico(dc, IC_MAP, x + 16, y + 52, 48, d ? kInk : kText);
    T(dc, x + 78, y + 74, name, 19, d ? kInk : kText, 700, AL_START, 154);
    TWrap(dc, x + 78, y + 96, L"маршрут не задан", 15, d ? kInk : kMuted, 400, 154, 1, 0);
    TWrap(dc, x + 16, y + 144, app >= 0 ? L"нажмите, чтобы открыть карты" : L"добавьте его в «Приложениях»", 14,
          d ? kInk : kDim, 400, 216, 1, 0);
}
