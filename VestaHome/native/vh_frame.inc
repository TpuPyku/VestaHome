// vh_frame.inc - part of home.cpp (included there, in order): the frame: drawing a screen, redraws, the load line, page turns and dragging, buttons.

static void DrawScreen(HDC dc, Screen s)
{
    switch (s) {
    case S_HOME:        DrawHome(dc); break;
    case S_RADIO:       ScrRadio(dc); break;
    case S_RADIO_LIST:  ScrRadioList(dc); break;
    case S_RADIO_PTY:   ScrRadioPty(dc); break;
    case S_AUDIO:       ScrAudio(dc); break;
    case S_AUDIO_LIST:  ScrAudioList(dc); break;
    case S_AUDIO_BT:    ScrAudioBt(dc); break;
    case S_AUX:         ScrAux(dc); break;
    case S_VIDEO:       ScrVideo(dc); break;
    case S_VIDEO_PLAY:  ScrVideoPlay(dc); break;
    case S_PHOTO:       ScrPhoto(dc); break;
    case S_SAVER_PICK:  ScrPhoto(dc); break;
    case S_PHOTO_VIEW:  ScrPhotoView(dc); break;
    case S_PHONE:       ScrPhone(dc); break;
    case S_CONTACTS:    ScrContacts(dc); break;
    case S_CONTACT:     ScrContact(dc); break;
    case S_CALLS:       ScrCalls(dc); break;
    case S_FAVS:        ScrFavs(dc); break;
    case S_CALL:        ScrCall(dc); break;
    case S_INCOMING:    ScrIncoming(dc); break;
    case S_SETTINGS:    ScrSetGeneral(dc); break;
    case S_SET_SOUND:   ScrSetSound(dc); break;
    case S_SET_BALANCE: ScrSetBalance(dc); break;
    case S_SET_SCREEN:  ScrSetScreen(dc); break;
    case S_SET_RADIO:   ScrSetRadio(dc); break;
    case S_SET_BT:      ScrSetBt(dc); break;
    case S_SET_WIFI:    ScrSetWifi(dc); break;
    case S_SET_OBD:     ScrSetObd(dc); break;
    case S_BT_PAIRNEW:  ScrBtPairNew(dc); break;
    case S_BT_PAIRASK:  ScrBtPairAsk(dc); break;
    case S_BT_PAIRED:   ScrBtPaired(dc); break;
    case S_WIFI_SAVED:  ScrWifiSaved(dc); break;
    case S_PLUGIN:      ScrPlugin(dc); break;
    case S_SET_PLUGINS: ScrSetPlugins(dc); break;
    case S_SET_VIDEO:   ScrSetVideo(dc); break;
    case S_SET_PHOTO:   ScrSetVideo(dc); break;   // (one "Media" section now)
    case S_SET_NET:     ScrSetNet(dc); break;
    case S_SET_NET3G:   ScrSetNet3G(dc); break;
    case S_INFO:        ScrInfo(dc); break;
    case S_WARNING:     ScrWarning(dc); break;
    case S_VOLUME:      ScrVolume(dc); break;
    case S_SAVER:       ScrSaver(dc); break;
    case S_EXIT:        ScrExit(dc); break;
    case S_REBOOT:      ScrReboot(dc); break;
    case S_CAR:         ScrCar(dc); break;
    case S_OBD_DTC:     ScrObdDtc(dc); break;
    case S_TEXTEDIT:    ScrTextEdit(dc); break;
    case S_APPS:        DrawApps(dc); break;
    case S_EDIT:        DrawEdit(dc); break;
    case S_KEYS:        DrawKeys(dc); break;
    case S_FILES:       DrawFiles(dc); break;
    case S_DIAG:        ScrDiag(dc); break;
    case S_TEST_SCREEN: ScrTestScreen(dc); break;
    case S_TEST_TOUCH:  ScrTestTouch(dc); break;
    case S_TEST_MEM:    ScrTestMem(dc); break;
    case S_WSET_WEATHER: ScrWSetWeather(dc); break;
    case S_WSET_SPEED:  ScrWSetSpeed(dc); break;
    case S_WSET_CAR:    ScrWSetCar(dc); break;
    case S_WSET_TRIP:   ScrWSetTrip(dc); break;
    case S_SET_HOME:    ScrSetHome(dc); break;
    case S_SET_THEME:   ScrSetTheme(dc); break;
    case S_SET_CAR:     ScrSetCar(dc); break;
    case S_TEST_CAN:    ScrTestCan(dc); break;
    case S_TEST_MCU:    ScrTestMcu(dc); break;
    case S_TEST_AV:     ScrTestAv(dc); break;
    case S_INFO_MORE:   ScrInfoMore(dc); break;
    case S_LICENSES:    ScrLicenses(dc); break;
    case S_THANKS:      ScrThanks(dc); break;
    case S_SET_VOLUME:  ScrSetVolume(dc); break;
    case S_CAMDL:       ScrCamDl(dc); break;
    case S_CAMREG:      ScrCamReg(dc); break;
    case S_TASKS:       DrawTasks(dc); break;
    case S_NETRADIO:    ScrNetRadio(dc); break;
    case S_NETRADIO_LIST: ScrNetRadioList(dc); break;
    case S_NETCAT:      ScrNetCat(dc); break;
    case S_NETCATS:     ScrNetCats(dc); break;
    default:            break;
    }
}

// The full frames' parts since the last load line, ms: the background, the screen, the bars, out to the display.
static DWORD s_partMs[4];

// A screen and, for the main ones, the bottom bar of sections.
static void DrawFrame(HDC dc)
{
    DWORD t0 = GetTickCount();
    CardMaskClear();   // (the glass of the cards drawn below: their own)
    if (s_screen == S_HOME)
        MainBackground(dc);
    else
        SubBackground(dc);
    DWORD t1 = GetTickCount();
    if (s_partial) {
        s_zoneIdx = 0;
    } else {
        kBtnCount = 0;
        s_scroll = NULL;
    }
    s_frameBack = A_NONE;
    DrawScreen(dc, s_screen);
    DWORD t2 = GetTickCount();
    int sec = NavSection(s_screen);
    if (sec >= 0)
        NavBar(dc, sec);
    else
        BackButton(dc);
    if (!s_partial) {
        s_partMs[0] += t1 - t0;
        s_partMs[1] += t2 - t1;
        s_partMs[2] += GetTickCount() - t2;
    }
}

static int s_drawnMinute = -1;   // the minute on the clock of the last frame
static DWORD s_drawCount, s_drawMs, s_drawMax;   // the frames since the last load line, their time
static int   s_drawMaxScreen = -1;               // the slowest one's screen and its text measurements
static DWORD s_drawMaxText;
static DWORD s_areaCount, s_areaMs, s_areaShown;  // the partial ones: drawn, their time, how many went out

static void RedrawFrom(const char* file, int line)
{
    if (s_drag)
        return;
    WhyCount(file, line);
    SYSTEMTIME t;
    GetLocalTime(&t);
    s_drawnMinute = t.wMinute;
    DWORD t0 = GetTickCount(), m0 = s_txtMeasures;
#ifndef UNDER_CE
    LARGE_INTEGER q0, q1, qf;   // the PC: a frame's time to the microsecond, every frame into the log
    QueryPerformanceCounter(&q0);
#endif
    HDC hdc = GetDC(s_wnd);
    HDC dc = CreateCompatibleDC(hdc);
    HGDIOBJ ob = SelectObject(dc, s_back);
    DrawFrame(dc);
#ifndef UNDER_CE
    QueryPerformanceCounter(&q1);
    QueryPerformanceFrequency(&qf);
    VLog(L"кадр: экран %d, %u мкс, измерений текста %u", (int)s_screen,
         (unsigned)((q1.QuadPart - q0.QuadPart) * 1000000 / qf.QuadPart), (unsigned)(s_txtMeasures - m0));
#endif
    DWORD tb = GetTickCount();
    BitBlt(hdc, 0, 0, 800, 480, dc, 0, 0, SRCCOPY);   // the video's window is left out (WS_CLIPCHILDREN)
    s_partMs[3] += GetTickCount() - tb;
    SelectObject(dc, ob);
    DeleteDC(dc);
    ReleaseDC(s_wnd, hdc);
    VidSync();
    DWORD ms = GetTickCount() - t0;   // for the load line of the log (once a minute)
    s_drawCount++;
    s_drawMs += ms;
    if (ms > s_drawMax) {
        s_drawMax = ms;
        s_drawMaxScreen = s_screen;
        s_drawMaxText = s_txtMeasures - m0;
    }
}

// Only what changed: the frame drawn again clipped to [l, t, r, b) (the layout and the buttons stay those of the
// last full frame, so it is for values that change in place: the speed, the track's position, a song title),
// and only if its pixels are not the same as before does the area go to the screen.
static void RedrawArea(int l, int t, int r, int b)
{
    if (!s_back || l >= r || t >= b || s_drag)
        return;
    DWORD t0 = GetTickCount();
    int w = r - l, h = b - t;
    Pix* was = (Pix*)malloc(w * h * sizeof(Pix));
    if (!was) {
        Redraw();
        return;
    }
    for (int y = t; y < b; y++)
        memcpy(was + (y - t) * w, s_backBits + (479 - y) * 800 + l, w * sizeof(Pix));
    s_partial = true;
    s_zoneIdx = 0;
    s_zoneMoved = false;
    s_clip.left = l;
    s_clip.top = t;
    s_clip.right = r;
    s_clip.bottom = b;
    HDC hdc = GetDC(s_wnd);
    HDC dc = CreateCompatibleDC(hdc);
    HGDIOBJ ob = SelectObject(dc, s_back);
    IntersectClipRect(dc, l, t, r, b);
    DrawFrame(dc);
#ifndef UNDER_CE
    GdiFlush();
#endif
    bool changed = false;
    for (int y = t; y < b && !changed; y++)
        changed = memcmp(was + (y - t) * w, s_backBits + (479 - y) * 800 + l, w * sizeof(Pix)) != 0;
    if (changed)
        BitBlt(hdc, l, t, w, h, dc, l, t, SRCCOPY);
    SelectObject(dc, ob);
    DeleteDC(dc);
    ReleaseDC(s_wnd, hdc);
    free(was);
    s_partial = false;
    s_clip.left = s_clip.top = 0;
    s_clip.right = 800;
    s_clip.bottom = 480;
    DWORD ms = GetTickCount() - t0;
    s_areaCount++;
    s_areaMs += ms;
    if (changed)
        s_areaShown++;
    if (s_zoneMoved || s_zoneIdx != kBtnCount)
        Redraw();   // the layout changed under the area (the media widget: the music became the radio): its buttons
}

// One button's look (pressed or let go) drawn again alone: the touches made most of the full frames, three a tap
// at ~45 ms each on the unit (sheet 29's load lines), the area 6-12 ms. The margin: its glow and frame.
static void RedrawBtn(int i)
{
    if (i < 0 || i >= kBtnCount)
        return;
    const RECT& r = s_btns[i].rc;
    RedrawArea(r.left > 4 ? r.left - 4 : 0, r.top > 4 ? r.top - 4 : 0, r.right < 796 ? r.right + 4 : 800,
               r.bottom < 476 ? r.bottom + 4 : 480);
}

// Our own threads' share of the CPU since the last call, the 4 busiest: "<id> NN%" (2026-10-18: VestaHome showed
// 82-92 % with no frames drawn and minimized - which thread; the threads log their ids as they start).
static void OwnThreadsLoad(wchar_t* out, int n)
{
    out[0] = 0;
    struct Th { DWORD id; unsigned __int64 t; };
    static Th s_prev[48];
    static int s_prevCount;
    static DWORD s_prevTick;
    Th cur[48];
    int count = 0;
#ifdef UNDER_CE
    typedef HANDLE (WINAPI *SnapFn)(DWORD, DWORD);
    typedef BOOL (WINAPI *ThFn)(HANDLE, THREADENTRY32*);
    typedef BOOL (WINAPI *CloseFn)(HANDLE);
    typedef HANDLE (WINAPI *OpenFn)(DWORD, BOOL, DWORD);
    static HMODULE s_th;
    if (!s_th)
        s_th = LoadLibrary(L"toolhelp.dll");
    SnapFn snap = s_th ? (SnapFn)GetProcAddress(s_th, L"CreateToolhelp32Snapshot") : NULL;
    ThFn first = s_th ? (ThFn)GetProcAddress(s_th, L"Thread32First") : NULL;
    ThFn next = s_th ? (ThFn)GetProcAddress(s_th, L"Thread32Next") : NULL;
    CloseFn close = s_th ? (CloseFn)GetProcAddress(s_th, L"CloseToolhelp32Snapshot") : NULL;
    OpenFn open = (OpenFn)GetProcAddress(GetModuleHandle(L"coredll.dll"), L"OpenThread");
    if (!snap || !first || !next || !close) {
        wcscpy(out, L"нет toolhelp");
        return;
    }
    HANDLE h = snap(TH32CS_SNAPTHREAD, 0);
    if (h == INVALID_HANDLE_VALUE)
        return;
    THREADENTRY32 te;
    te.dwSize = sizeof(te);
    DWORD pid = GetCurrentProcessId();
    for (BOOL ok = first(h, &te); ok && count < 48; ok = next(h, &te)) {
        if (te.th32OwnerProcessID == pid) {
            HANDLE th = open ? open(THREAD_QUERY_INFORMATION, FALSE, te.th32ThreadID) : NULL;
            FILETIME c, e, k, u;
            if (GetThreadTimes(th ? th : (HANDLE)te.th32ThreadID, &c, &e, &k, &u)) {
                cur[count].id = te.th32ThreadID;
                cur[count].t = ((unsigned __int64)k.dwHighDateTime << 32 | k.dwLowDateTime) +
                               ((unsigned __int64)u.dwHighDateTime << 32 | u.dwLowDateTime);
                count++;
            }
            if (th)
                CloseHandle(th);
        }
        te.dwSize = sizeof(te);
    }
    close(h);
#else
    wcscpy(out, L"только на ГУ");
    return;
#endif
    DWORD now = GetTickCount(), wall = now - s_prevTick;
    int pct[48];
    for (int i = 0; i < count; i++) {
        pct[i] = -1;
        for (int j = 0; j < s_prevCount; j++)
            if (s_prev[j].id == cur[i].id && s_prevTick && wall)
                pct[i] = (int)((cur[i].t - s_prev[j].t) / 10000 * 100 / wall);   // 100 ns -> ms -> %
    }
    for (int k = 0; k < 4; k++) {   // the 4 busiest
        int best = -1;
        for (int i = 0; i < count; i++)
            if (pct[i] >= 0 && (best < 0 || pct[i] > pct[best]))
                best = i;
        if (best < 0)
            break;
        wchar_t s[32];
        _snwprintf(s, 32, L"%s%08X %d%%", k ? L", " : L"", (unsigned)cur[best].id, pct[best]);
        s[31] = 0;
        if ((int)(wcslen(out) + wcslen(s)) < n - 1)
            wcscat(out, s);
        pct[best] = -1;
    }
    if (!out[0])
        _snwprintf(out, n, L"%d, замер с этой минуты", count);
    memcpy(s_prev, cur, count * sizeof(Th));
    s_prevCount = count;
    s_prevTick = now;
}

// Once a minute into the log: the CPU over that minute (the whole and the busiest processes, VestaCore vtask)
// and what our frames cost - to find what loads the unit.
static void LogLoad(HWND hwnd)
{
    if (s_screen != S_TASKS)   // (its screen samples every 2 s itself)
        TaskSample(&s_task, hwnd);
    int idx[4] = { -1, -1, -1, -1 };
    for (int i = 0; i < s_task.procCount; i++) {
        int c = s_task.proc[i].cpu10;
        for (int k = 0; k < 4; k++)
            if (idx[k] < 0 || c > s_task.proc[idx[k]].cpu10) {
                for (int m = 3; m > k; m--)
                    idx[m] = idx[m - 1];
                idx[k] = i;
                break;
            }
    }
    wchar_t top[200] = L"";
    for (int k = 0; k < 4 && idx[k] >= 0 && s_task.proc[idx[k]].cpu10 > 0; k++) {
        wchar_t s[64];
        _snwprintf(s, 64, L"%s%s %d%%", k ? L", " : L"", s_task.proc[idx[k]].name, (s_task.proc[idx[k]].cpu10 + 5) / 10);
        s[63] = 0;
        if (wcslen(top) + wcslen(s) < 199)
            wcscat(top, s);
    }
    wchar_t thr[160];
    OwnThreadsLoad(thr, 160);
    wchar_t why[96];
    WhyTake(why, 96);
    DWORD n = s_drawCount ? s_drawCount : 1;
    VLog(L"нагрузка: ЦП %d%%; %s; кадров %u, в среднем %u мс (фон %u, экран %u, панели %u, вывод %u), макс %u мс "
         L"(экран %d, измерений текста %u); от: %s; частичных %u (%u на экран), в среднем %u мс; потоки VH: %s",
         s_task.cpu10 >= 0 ? (s_task.cpu10 + 5) / 10 : -1, top[0] ? top : L"процессы не видны", (unsigned)s_drawCount,
         (unsigned)(s_drawMs / n), (unsigned)(s_partMs[0] / n), (unsigned)(s_partMs[1] / n),
         (unsigned)(s_partMs[2] / n), (unsigned)(s_partMs[3] / n), (unsigned)s_drawMax, s_drawMaxScreen,
         (unsigned)s_drawMaxText, why, (unsigned)s_areaCount, (unsigned)s_areaShown,
         (unsigned)(s_areaCount ? s_areaMs / s_areaCount : 0), thr);
    s_drawCount = s_drawMs = s_drawMax = s_areaCount = s_areaMs = s_areaShown = 0;
    memset(s_partMs, 0, sizeof(s_partMs));
}

// The band that slides on a page turn: under the top bar, above the bottom bar and above the page dots where
// they are (they stay in place, the new page's show when it has turned); on the radio only the presets.
static bool PhotoFull();   // the photo viewer on the whole screen (vh_photo.inc)

static int SlideTop()   // under what stays: the radio's frequency panel, the tabs, the path and head lines
{
    switch (s_screen) {
    case S_PHOTO_VIEW:
        return PhotoFull() ? 0 : 56;
    case S_RADIO:   // (the presets from 256: 258 left their top 2 rows behind, the user 2026-11-24)
        return 252;
    case S_PHOTO: case S_VIDEO: case S_AUDIO_LIST:   // the tabs and the path line (with the pages' arrows)
        return 156;
    case S_CONTACTS: case S_CALLS:                   // the phone's pills and the list's head line
        return 160;
    case S_NETRADIO_LIST: case S_FAVS:               // the tabs (pills)
        return 112;
    case S_SAVER_PICK:
        return 116;
    case S_FILES:
        return 104;   // (the files: their path line stays)
    case S_TASKS:
        return 228;   // (the graphs, the tabs and the arrows stay: HU sheet 30)
    default:
        return 56;
    }
}

static int SlideLeft()   // right of what stays: the settings' tiles
{
    return s_screen == S_BT_PAIRED || s_screen == S_WIFI_SAVED ? 244 : 0;
}

static int SlideBottom()   // above the page dots (396), the dock and the bottom bar (408): they stay (the user
{                          // 2026-11-10: the bottom bar does not move with a swipe; HU 2026-11-11 sheet 29 p. 13)
    if (s_screen == S_PHOTO_VIEW)
        return PhotoFull() ? 480 : 398;   // over its dock
    if (s_screen == S_TASKS)
        return 406;   // (its 4th row ends at 404; no dots)
    return 392;
}

// A page turn: the new page slides in from the right (dir 1) or the left (dir -1) and pushes the old one out,
// in the band between the top bar and the bottom bar (they stay). The old frame is what s_back holds now; the
// copy of it lives only for the animation (1.5 MB).
// The page's band onto the screen: the page a shifted by x, its neighbour b beside (NULL: the background), over the
// still background (the user 2026-11-24: it must not move with the pages - the frame was shifted whole). Made a
// strip of kStripH rows at a time (51 KB, kept) and put on the screen.
enum { kStripH = 16 };
static HBITMAP s_strip;
static Pix*    s_stripBits;

static void SlideBand(HDC hdc, const Pix* a, const Pix* b, int x, const unsigned char* ma, const unsigned char* mb)
{
    if (!s_strip)
        s_strip = MakeDib(800, kStripH, &s_stripBits);
    if (!s_strip || !s_stripBits)
        return;   // (no memory: no slide, the frame comes at its end)
#ifndef UNDER_CE
    GdiFlush();
#endif
    static Pix bg[800];
    int top = SlideTop(), bottom = SlideBottom(), l = SlideLeft();
    HDC sdc = CreateCompatibleDC(hdc);
    HGDIOBJ os = SelectObject(sdc, s_strip);
    for (int y0 = top; y0 < bottom; y0 += kStripH) {
        int n = bottom - y0 < kStripH ? bottom - y0 : kStripH;
        for (int i = 0; i < n; i++) {
            int y = y0 + i;
            BgRow(bg, y);
            SlideRow(s_stripBits + (kStripH - 1 - i) * 800, a + (479 - y) * 800, b ? b + (479 - y) * 800 : NULL, bg, l,
                     800, x, ma ? ma + y * 800 : NULL, mb ? mb + y * 800 : NULL);
        }
        BitBlt(hdc, l, y0, 800 - l, n, sdc, l, 0, SRCCOPY);
    }
    SelectObject(sdc, os);
    DeleteDC(sdc);
}

static void RedrawSlide(int dir)
{
    Pix* oldBits = NULL;
    HBITMAP old = MakeDib(800, 480, &oldBits);
    if (!old || !oldBits) {
        if (old)
            DeleteObject(old);
        Redraw();
        return;
    }
    memcpy(oldBits, s_backBits, 800 * 480 * sizeof(Pix));
    unsigned char* oldA = s_cardA ? (unsigned char*)malloc(800 * 480) : NULL;   // (its glass: the new page's comes)
    if (oldA)
        memcpy(oldA, s_cardA, 800 * 480);
    SYSTEMTIME t;
    GetLocalTime(&t);
    s_drawnMinute = t.wMinute;
    HDC hdc = GetDC(s_wnd);
    HDC dc = CreateCompatibleDC(hdc);
    HGDIOBJ ob = SelectObject(dc, s_back);
    DrawFrame(dc);
    int top = SlideTop(), bottom = SlideBottom(), h = bottom - top, l = SlideLeft(), sw = 800 - l;
    BitBlt(hdc, 0, 0, 800, top, dc, 0, 0, SRCCOPY);
    BitBlt(hdc, 0, bottom, 800, 480 - bottom, dc, 0, bottom, SRCCOPY);
    BitBlt(hdc, 0, top, l, h, dc, 0, top, SRCCOPY);
    const DWORD kMs = 220;
    DWORD t0 = GetTickCount();
    for (;;) {
        DWORD el = GetTickCount() - t0;
        double p = el >= kMs ? 1.0 : (double)el / kMs;
        p = 1 - (1 - p) * (1 - p) * (1 - p);   // ease-out
        int off = (int)(sw * p + 0.5);
        SlideBand(hdc, oldBits, s_backBits, dir > 0 ? -off : off, oldA, s_cardA);   // (the new page: the neighbour)
        if (el >= kMs)
            break;
        Sleep(10);
    }
    SelectObject(dc, ob);
    DeleteDC(dc);
    ReleaseDC(s_wnd, hdc);
    DeleteObject(old);
    free(oldA);
    VidSync();
}

static int s_slideDir;   // set by an action that turned a page: the next frame slides in

// ---- a page dragged with the finger: once the finger has moved sideways, the page goes with it and its
// neighbour comes in beside it; let go far enough it turns (the rest slides on), otherwise it slides back. The
// current frame is copied aside (1.5 MB, only while dragging), the neighbour is drawn into s_back.
static bool Swipe(int dir);
static void BuildButtons();
static int  s_phPage;   // the photos' page (the photos' part is below)
static int  PhotoPages();
static void PhotoStep(int dir);
static int     s_dragNb, s_dragSide, s_dragX;   // the neighbour in s_back (1 next, -1 previous, 0 none: an end)
static HBITMAP s_dragOld;
static Pix* s_dragOldBits;
static unsigned char* s_dragOldA;   // its cards' glass (s_cardA then)

// The screens that turn pages - by a swipe (the neighbour follows the finger) and by their dots (A_PAGE): each one's
// page and how many; round = past the last page the first (a swipe; the dots always go round).
static int RadioPages()
{
    return kRadioPages;
}

struct Pager {
    Screen s;
    int*   page;
    int    (*pages)();
    bool   round;
};
static const Pager kPagers[] = { { S_HOME, &s_homePage, HomePages, true },
                                 { S_RADIO, &s_radioPage, RadioPages, false },   // (no round: СВ -> FM1 left СВ on, HU 2026-10-24-2)
                                 { S_APPS, &s_page, AppsPages, false }, { S_FILES, &s_filePage, FilePages, false },
                                 { S_PHOTO, &s_phPage, PhotoPages, false },
                                 { S_SAVER_PICK, &s_phPage, PhotoPages, false },
                                 { S_AUDIO_LIST, &s_musPage, MusPages, false },
                                 { S_VIDEO, &s_vidPage, VidPages, false },
                                 { S_TASKS, &s_taskPage, TaskPages, false },
                                 { S_SET_HOME, &s_setHomePage, SetHomePages, false },
                                 { S_NETRADIO_LIST, &s_netListPage, NetListPages, false },
                                 { S_NETCATS, &s_ncCatPage, NcCatPages, false },
                                 { S_CONTACTS, &s_bookPage, BookPages, false },
                                 { S_CALLS, &s_callPage, CallPages, false },
                                 { S_FAVS, &s_favPage, FavPages, false } };

static const Pager* PagerOf(Screen s)
{
    for (int i = 0; i < (int)(sizeof(kPagers) / sizeof(kPagers[0])); i++)
        if (kPagers[i].s == s)
            return &kPagers[i];
    return NULL;
}

// The page dir away: false at an end that does not go round (or a single page that does)
static bool PageStep(const Pager& p, int dir, int* to)
{
    int n = p.pages();
    *to = *p.page + dir;
    if (n < 2)
        return false;
    if (p.round) {
        *to = (*to + n) % n;
        return n > 1;
    }
    return *to >= 0 && *to < n;
}

// A finger's drag turns the page (the settings' lists scroll instead: the user 2026-11-08, "no turning of pages")
static bool CanSwipe()
{
    return PagerOf(s_screen) != NULL;
}

// The page beside (dir) drawn into s_back without turning to it (no side effects: the radio's band is not
// switched); false at an end that does not go round.
static bool DrawNeighbour(int dir)
{
    const Pager* p = PagerOf(s_screen);
    int was = *p->page, to;
    bool ok = PageStep(*p, dir, &to);
    if (ok) {
        *p->page = to;
        HDC hdc = GetDC(s_wnd);
        HDC dc = CreateCompatibleDC(hdc);
        HGDIOBJ ob = SelectObject(dc, s_back);
        DrawFrame(dc);
        SelectObject(dc, ob);
        DeleteDC(dc);
        ReleaseDC(s_wnd, hdc);
    }
    *p->page = was;
    return ok;
}

// The current page at x in the band between the bars, its neighbour beside it (or the background at an end).
static void DragShow(int x)
{
    HDC hdc = GetDC(s_wnd);
    // (the cards' glass of each page with it; at an end the background beside it)
    SlideBand(hdc, s_dragOldBits, s_dragNb ? s_backBits : NULL, x, s_dragOldA, s_dragNb ? s_cardA : NULL);
    ReleaseDC(s_wnd, hdc);
}

static void DragStart()
{
    KillTimer(s_wnd, TIMER_LONG);
    s_pressed = -1;
    Redraw();   // the page without the pressed look
    s_dragOld = MakeDib(800, 480, &s_dragOldBits);
    if (!s_dragOld || !s_dragOldBits) {   // no memory: the swipe as before (on letting go)
        if (s_dragOld)
            DeleteObject(s_dragOld);
        s_dragOld = NULL;
        return;
    }
    memcpy(s_dragOldBits, s_backBits, 800 * 480 * sizeof(Pix));
    s_dragOldA = s_cardA ? (unsigned char*)malloc(800 * 480) : NULL;
    if (s_dragOldA)
        memcpy(s_dragOldA, s_cardA, 800 * 480);
    s_drag = true;
    s_dragNb = s_dragSide = s_dragX = 0;
}

static void DragMove(int dx)
{
    int side = dx < 0 ? 1 : -1;
    if (side != s_dragSide) {
        s_dragSide = side;
        s_dragNb = DrawNeighbour(side) ? side : 0;
    }
    s_dragX = s_dragNb ? dx : dx / 3;   // at an end: held back
    DragShow(s_dragX);
}

// Let go: the page turns to the neighbour (turn: s_dragNb, touchdrag.h PageTurn - dragged past 60 px or flicked) or
// slides back (0).
static void DragEnd(int turn)
{
    int w = 800 - SlideLeft(), from = s_dragX, to = turn ? (s_dragNb > 0 ? -w : w) : 0;
    const DWORD kMs = 160;
    DWORD t0 = GetTickCount();
    for (;;) {
        DWORD el = GetTickCount() - t0;
        double p = el >= kMs ? 1.0 : (double)el / kMs;
        p = 1 - (1 - p) * (1 - p) * (1 - p);   // ease-out
        DragShow(from + (int)((to - from) * p));
        if (el >= kMs)
            break;
        Sleep(10);
    }
    s_drag = false;
    DeleteObject(s_dragOld);
    s_dragOld = NULL;
    free(s_dragOldA);
    s_dragOldA = NULL;
    if (turn)
        Swipe(s_dragNb);   // the page for real (the radio: its band too)
    BuildButtons();
    Redraw();
}

static void Trim(wchar_t* s)
{
    wchar_t* p = s;
    while (*p == L' ' || *p == L'\t')
        p++;
    memmove(s, p, (wcslen(p) + 1) * sizeof(wchar_t));
    int n = (int)wcslen(s);
    while (n > 0 && (s[n - 1] == L' ' || s[n - 1] == L'\t' || s[n - 1] == L'\r'))
        s[--n] = 0;
}

// ---------------------------------------------------------------- buttons

// The buttons are registered while the screen is drawn; a changed screen starts with none.
static void BuildButtons()
{
    kBtnCount = 0;
}

// Opens screen s over the current one (push) or instead of it. A screen that is already under the current
// one is returned to, so going around in circles doesn't pile up the history.
static void Go(Screen s, bool push)
{
    s_status[0] = 0;
    s_plugStackN = 0;   // (the add-on screens' own stack: only while one opens over another)
    if (s == S_TASKS)
        TasksOpen();
    for (int k = 0; k < s_histLen; k++)
        if (s_hist[k] == s) {
            s_histLen = k;
            s_screen = s;
            return;
        }
    if (push) {
        if (s_histLen == 16) {
            memmove(s_hist, s_hist + 1, 15 * sizeof(Screen));
            s_histLen--;
        }
        s_hist[s_histLen++] = s_screen;
    }
    s_screen = s;
    s_screenTick = GetTickCount();
    if (Section(s) == 2)
        s_media = s;
    if (s == S_CALL)
        s_callTick = s_screenTick;
    s_testStep = 0;
    s_touchCount = 0;
}

static void Back()
{
    if (PlugBack())
        return;   // the add-on screen under this one
    s_screen = s_histLen ? s_hist[--s_histLen] : S_HOME;
}

// A main screen from the bottom bar (or a widget): the history starts over.
static void RadioOn();
