// vh_photo.inc - part of home.cpp (included there, in order): photos.

// ---- photos (Media -> Photo): the volumes, folders and pictures as tiles with thumbnails (4 x 2 a page, a swipe
// turns it), the viewer between the top bar and the dock: previous / next (and a swipe), a quarter turn either
// way, the slide show. The pictures are decoded by photo.cpp in a thread of their own (a photo takes a second or
// two on the unit): the viewed one first, then the thumbnails of the page shown; a result posts WM_APP_PHOTO.
#define WM_APP_PHOTO (WM_APP + 25)
#define WM_APP_OBD (WM_APP + 27)
enum { TIMER_PHOTO = 13, kPhMax = 1000, kPhTiles = 8, kThW = 172, kThH = 92, kThRad = 10, kPhBoxW = 800, kPhBoxH = 342 };
static wchar_t  s_phDir[MAX_PATH];   // "" = the volumes
static wchar_t* s_phs[kPhMax];       // folders ("\" at the end), then pictures
static int      s_phCount, s_phCur = -1;   // s_phCur: the picture viewed (index in s_phs)
static bool     s_phRead, s_phShow;  // s_phShow: the slide show runs
static int      s_phRot;             // the viewer's own quarter turns
struct PhThumb { wchar_t path[MAX_PATH]; Pic pic; int state; };   // state: 0 wanted, 1 working, 2 done, 3 failed
static PhThumb  s_th[kPhTiles];
static wchar_t  s_pvPath[MAX_PATH];  // the viewer's request
static int      s_pvRot, s_pvState, s_pvErr, s_pvW, s_pvH;   // state: 0 none, 1 wanted, 2 working, 3 done
static bool     s_pvFill, s_pvAuto;
static int      s_pvBoxH = kPhBoxH;   // the box asked for: 342 under the bars, 480 the whole screen (s_phFull)
static bool     s_phFull;             // the viewer on the whole screen (the user 2026-10-24); a tap - back
static bool PhotoFull() { return s_phFull; }
static Pic      s_pv;
static CRITICAL_SECTION s_phCs;
static HANDLE   s_phEv, s_phThread;

static DWORD WINAPI PhotoThread(LPVOID)
{
    for (;;) {
        WaitForSingleObject(s_phEv, INFINITE);
        for (;;) {
            wchar_t path[MAX_PATH];
            int slot = -1, rot = 0, boxH = kPhBoxH;
            bool view = false, fill = false, exif = false;
            EnterCriticalSection(&s_phCs);
            if (s_pvState == 1) {   // the viewed picture first
                view = true;
                wcscpy(path, s_pvPath);
                rot = s_pvRot;
                fill = s_pvFill;
                boxH = s_pvBoxH;
                exif = s_pvAuto;
                s_pvState = 2;
            } else {
                for (int i = 0; i < kPhTiles && slot < 0; i++)
                    if (s_th[i].path[0] && s_th[i].state == 0) {
                        slot = i;
                        wcscpy(path, s_th[i].path);
                        s_th[i].state = 1;
                        exif = s_cfg.photoAutoRot;
                    }
            }
            LeaveCriticalSection(&s_phCs);
            if (!view && slot < 0)
                break;
            Pic pic = { 0, 0, NULL, NULL };
            int w = 0, h = 0;
            int res = view ? PicLoad(path, kPhBoxW, boxH, fill, rot, exif, &pic, &w, &h, true)   // full colour
                           : PicThumb(path, kThW, kThH, exif, &pic);
            EnterCriticalSection(&s_phCs);
            if (view && s_pvState == 2 && !wcscmp(path, s_pvPath) && rot == s_pvRot && fill == s_pvFill &&
                boxH == s_pvBoxH) {
                PicFree(&s_pv);
                s_pv = pic;
                s_pvErr = res;
                s_pvW = w;
                s_pvH = h;
                s_pvState = 3;
                pic.px = NULL;
                pic.px32 = NULL;
            } else if (!view && s_th[slot].state == 1 && !wcscmp(path, s_th[slot].path)) {
                s_th[slot].pic = pic;
                s_th[slot].state = res == PIC_OK ? 2 : 3;
                pic.px = NULL;
                pic.px32 = NULL;
            }
            LeaveCriticalSection(&s_phCs);
            PicFree(&pic);   // (a request changed meanwhile: not wanted any more)
            PostMessage(s_wnd, WM_APP_PHOTO, 0, 0);
        }
    }
    return 0;
}

static void PhotoWake()
{
    if (!s_phThread) {
        InitializeCriticalSection(&s_phCs);
        s_phEv = CreateEvent(NULL, FALSE, FALSE, NULL);
        s_phThread = CreateThread(NULL, 0, PhotoThread, NULL, 0, NULL);
        VLog(L"поток фото: %s", s_phThread ? L"запущен" : L"не запустился");
    }
    if (s_phEv)
        SetEvent(s_phEv);
}

static bool PhIsDir(int i)
{
    return i >= 0 && i < s_phCount && s_phs[i][wcslen(s_phs[i]) - 1] == L'\\';
}

static void PhotoOpenDir(const wchar_t* dir)
{
    FreeNames(s_phs, &s_phCount);
    wcsncpy(s_phDir, dir, MAX_PATH - 1);
    s_phDir[MAX_PATH - 1] = 0;
    s_phPage = 0;
    s_phCur = -1;
    s_phRead = true;
    s_phCount = s_phDir[0] ? ReadMusicDir(s_phDir, s_phs, kPhMax, true, 2) : ReadVolumes(s_phs);
    if (s_phPickMode && s_phDir[0]) {   // the saver's pick: not the skin's icons (pictures under 16 KB left out)
        int n = 0;
        for (int i = 0; i < s_phCount; i++) {
            wchar_t p[MAX_PATH];
            _snwprintf(p, MAX_PATH, L"%s%s", s_phDir, s_phs[i]);
            p[MAX_PATH - 1] = 0;
            WIN32_FILE_ATTRIBUTE_DATA fa;
            bool dir = s_phs[i][0] && s_phs[i][wcslen(s_phs[i]) - 1] == L'\\';
            if (dir || !GetFileAttributesEx(p, GetFileExInfoStandard, &fa) || fa.nFileSizeHigh || fa.nFileSizeLow >= 16384)
                s_phs[n++] = s_phs[i];
            else
                free(s_phs[i]);
        }
        s_phCount = n;
        return;   // (nor the photos' own folder remembered)
    }
    if (s_phDir[0] && wcscmp(s_cfg.photoDir, s_phDir)) {
        wcscpy(s_cfg.photoDir, s_phDir);
        CfgSoon();
    }
}

static int PhotoPer()   // the folder's entries a page ("up" is the path line's arrow)
{
    return kPhTiles;
}

static int PhotoPages()
{
    return s_phCount > 0 ? (s_phCount + PhotoPer() - 1) / PhotoPer() : 1;
}

static int PhotoTileEntry(int t)   // tile t of the page: its entry, -2 none
{
    int k = s_phPage * PhotoPer() + t;
    return k < s_phCount ? k : -2;
}

// The picture into the frame at x, y (cut at the frame and at the partial redraw's area): the viewer's 32-bit, the
// thumbnails RGB565 widened. rad: its corners rounded by that radius (the thumbnails, HU 2026-10-25: the square
// corners stuck out of the rounded outline) - what is under them stays.
static void BlitPic(const Pic& p, int x, int y, int rad = 0)
{
    if (!p.px && !p.px32)
        return;
#ifndef UNDER_CE
    GdiFlush();
#endif
    for (int r = 0; r < p.h; r++) {
        int fy = y + r;
        if (fy < s_clip.top || fy >= s_clip.bottom)
            continue;
        int cut = 0;   // the row's corner: the pixels left out at each end
        int e = r < rad ? rad - r : r >= p.h - rad ? r - (p.h - rad) + 1 : 0;
        if (e > 0) {
            double dy = e - 0.5, in = rad - sqrt((double)rad * rad - dy * dy);
            cut = (int)(in + 0.5);
        }
        int x0 = x + cut < s_clip.left ? s_clip.left : x + cut;
        int x1 = x + p.w - cut > s_clip.right ? s_clip.right : x + p.w - cut;
        Pix* dst = s_backBits + (479 - fy) * 800;
        if (p.px32) {
            if (x1 > x0)
                memcpy(dst + x0, p.px32 + r * p.w + (x0 - x), (x1 - x0) * sizeof(Pix));
            continue;
        }
        const unsigned short* src = p.px + r * p.w - x;
        for (int i = x0; i < x1; i++) {
            unsigned q = src[i];
            dst[i] = PIX((q >> 8 & 0xF8) | q >> 13, (q >> 3 & 0xFC) | (q >> 9 & 3), (q << 3 & 0xF8) | (q >> 2 & 7));
        }
    }
}

// The page's thumbnails asked for (the worker makes them); a tile no longer shown lets go of its picture.
static void PhotoWantThumbs()
{
    if (!s_phThread)
        PhotoWake();
    bool any = false;
    EnterCriticalSection(&s_phCs);
    for (int t = 0; t < kPhTiles; t++) {
        int k = PhotoTileEntry(t);
        wchar_t path[MAX_PATH] = L"";
        if (k >= 0 && k < s_phCount && !PhIsDir(k))
            _snwprintf(path, MAX_PATH, L"%s%s", s_phDir, s_phs[k]);
        if (wcscmp(path, s_th[t].path)) {
            PicFree(&s_th[t].pic);
            wcscpy(s_th[t].path, path);
            s_th[t].state = 0;
            any = any || path[0];
        }
    }
    LeaveCriticalSection(&s_phCs);
    if (any)
        PhotoWake();
}

static void ScrPhoto(HDC dc)
{
    if (!s_phRead)
        PhotoOpenDir(s_cfg.photoDir[0] && GetFileAttributes(s_cfg.photoDir) != 0xFFFFFFFF ? s_cfg.photoDir : L"");
    if (s_phPage >= PhotoPages())
        s_phPage = PhotoPages() - 1;
    bool pick = s_screen == S_SAVER_PICK;   // Settings -> Theme: the saver's picture, a tap picks it
    if (pick) {
        Frame(dc, L"Картинка заставки");
        T(dc, 24, 92, L"Выберите картинку — она станет фоном заставки", 15, kMuted, 400, AL_START, 752);
    } else {
        Frame(dc, L"Медиа · Фото");
        MediaTabs(dc, S_PHOTO, IC_NONE, A_NONE, 0);
    }
    DirPath(dc, s_phDir, A_PHFILE, -1, -3, PhotoPages() > 1 ? 608 : 784);
    PhotoWantThumbs();
    for (int t = 0; t < kPhTiles; t++) {   // design 0.6: 4 x 2, the picture without a frame, its name under it
        int k = PhotoTileEntry(t), x = 22 + (t % 4) * 194, y = 160 + (t / 4) * 118;
        if (k == -2)
            break;
        bool down = Zone(x - 6, y - 4, 184, 116, A_PHFILE, k);
        bool dir = PhIsDir(k);
        bool cur = !dir && k == s_phCur;   // the last one viewed: an orange outline
        wchar_t name[MAX_PATH];
        const wchar_t* rest;
        const wchar_t* vol = s_phDir[0] ? NULL : VolName(s_phs[k], &rest);   // the volumes by their names
        wcscpy(name, vol ? vol : s_phs[k]);
        if (dir && name[0] && name[wcslen(name) - 1] == L'\\')
            name[wcslen(name) - 1] = 0;
        if (down || cur)   // (around the thumbnail's rounded corners: kThRad + the gap)
            R(dc, x - 4, y - 4, kThW + 8, kThH + 8, kThRad + 4, down ? kOrange : CLR_INVALID, kOrange);
        if (dir) {
            R(dc, x, y, kThW, kThH, 14, down ? kOrange : kPanel);
            Ico(dc, IC_FOLDER, x + kThW / 2 - 24, y + kThH / 2 - 24, 48, down ? kInk : kOrange);
        } else {
            R(dc, x, y, kThW, kThH, kThRad, kMargin);
            EnterCriticalSection(&s_phCs);
            if (s_th[t].state == 2)
                BlitPic(s_th[t].pic, x + (kThW - s_th[t].pic.w) / 2, y + (kThH - s_th[t].pic.h) / 2, kThRad);
            else
                Ico(dc, IC_PHOTO, x + kThW / 2 - 22, y + kThH / 2 - 22, 44, s_th[t].state == 3 ? kOff : kDim);
            LeaveCriticalSection(&s_phCs);
        }
        T(dc, x, y + kThH + 18, name, 13, cur || down ? kOrange : dir ? kText : kMuted, dir ? 700 : 400, AL_START,
          kThW);
    }
    if (!s_phCount)
        EmptyList(dc, 40, 190, 720, s_phDir[0] ? L"Здесь нет папок и фото (JPG, PNG, BMP)" : L"Нет носителей");
    ListPages(dc, s_phPage, PhotoPages(), 112, 40);   // (a main screen: the arrows on the path line, as the video's)
}

// The viewer: picture i of the folder asked for (its own turn reset), the slide show's timer as set.
static void PhotoView(int i)
{
    if (i < 0 || i >= s_phCount || PhIsDir(i))
        return;
    if (i != s_phCur)
        s_phRot = 0;
    s_phCur = i;
    s_phPage = i / PhotoPer();
    if (wcscmp(s_cfg.photoFile, s_phs[i])) {   // the start comes back to it (ResumeScreen)
        wcsncpy(s_cfg.photoFile, s_phs[i], MAX_PATH - 1);
        s_cfg.photoFile[MAX_PATH - 1] = 0;
        CfgSoon();
    }
    wchar_t path[MAX_PATH];
    _snwprintf(path, MAX_PATH, L"%s%s", s_phDir, s_phs[i]);
    if (!s_phThread)
        PhotoWake();
    EnterCriticalSection(&s_phCs);
    wcscpy(s_pvPath, path);
    s_pvRot = s_phRot;
    s_pvFill = s_cfg.photoFill;
    s_pvAuto = s_cfg.photoAutoRot;
    s_pvBoxH = s_phFull ? 480 : kPhBoxH;
    s_pvState = 1;
    LeaveCriticalSection(&s_phCs);
    PhotoWake();
    if (s_phShow)
        SetTimer(s_wnd, TIMER_PHOTO, s_cfg.photoInterval * 1000, NULL);
}

static void PhotoStep(int dir)   // the next / previous picture of the folder, round
{
    if (s_phCount <= 0)
        return;
    int i = s_phCur < 0 ? (dir > 0 ? -1 : s_phCount) : s_phCur;
    for (int n = 0; n < s_phCount; n++) {
        i = (i + dir + s_phCount) % s_phCount;
        if (!PhIsDir(i)) {
            PhotoView(i);
            return;
        }
    }
}

static void PhotoShow(bool on)   // the slide show
{
    s_phShow = on;
    if (on)
        SetTimer(s_wnd, TIMER_PHOTO, s_cfg.photoInterval * 1000, NULL);
    else
        KillTimer(s_wnd, TIMER_PHOTO);
}

static void ScrPhotoView(HDC dc)
{
    int n = 0, at = 0;
    for (int i = 0; i < s_phCount; i++)
        if (!PhIsDir(i)) {
            n++;
            if (i == s_phCur)
                at = n;
        }
    if (s_phFull) {   // the whole screen, no bars: a tap goes back, a swipe the next / previous
        Zone(0, 0, 800, 480, A_PHFULL);
        R(dc, 0, 0, 800, 480, 0, RGB(0, 0, 0));
        EnterCriticalSection(&s_phCs);
        if (s_pvState == 3 && s_pvErr == PIC_OK)
            BlitPic(s_pv, (800 - s_pv.w) / 2, (480 - s_pv.h) / 2);
        int st = s_pvState;
        LeaveCriticalSection(&s_phCs);
        if (st == 1 || st == 2)
            T(dc, 400, 246, L"Загрузка…", 20, kMuted, 600, AL_MID);
        return;
    }
    wchar_t s[MAX_PATH + 32];
    _snwprintf(s, MAX_PATH + 32, L"Фото · %d / %d · %s", at, n, s_phCur >= 0 ? s_phs[s_phCur] : L"");
    Frame(dc, s);
    R(dc, 0, 56, 800, 342, 0, kMargin);
    EnterCriticalSection(&s_phCs);
    int state = s_pvState, err = s_pvErr;
    if (state == 3 && err == PIC_OK)
        BlitPic(s_pv, (800 - s_pv.w) / 2, 56 + (342 - s_pv.h) / 2);
    LeaveCriticalSection(&s_phCs);
    if (state == 1 || state == 2) {
        T(dc, 400, 236, L"Загрузка…", 20, kMuted, 600, AL_MID);
    } else if (state == 3 && err != PIC_OK) {
        static const wchar_t* why[5] = { L"", L"Формат не поддерживается", L"Слишком большое изображение",
                                         L"Файл не читается", L"Не хватает памяти" };
        Panel(dc, 220, 110, 360, 230);
        Ico(dc, IC_PHOTO, 360, 140, 80, kOff);
        Line(dc, 352, 140, 448, 220, 4, kMuted);
        T(dc, 400, 262, why[err % 5], 18, kText, 700, AL_MID, 336);
        T(dc, 400, 292, s_phCur >= 0 ? s_phs[s_phCur] : L"", 17, kMuted, 400, AL_MID, 336);
        T(dc, 400, 318, err == PIC_FORMAT ? L"JPG (не прогрессивный), PNG, BMP" : L"Пропустить можно стрелками внизу",
          15, kDim, 400, AL_MID, 336);
    }
    Dock(dc);
    IconBtn(dc, IC_PREV, 132, 417, 44, kText, A_PHSTEP, -1);
    IconBtn(dc, IC_ROT_L, 244, 417, 44, kText, A_PHROT, -1);
    bool d = Zone(330, 410, 140, 58, A_PHSHOW);   // the slide show: orange while it runs
    R(dc, 330, 410, 140, 58, 10, d || s_phShow ? kOrange : kPanel2);
    Ico(dc, s_phShow ? IC_PAUSE : IC_PLAY, 378, 417, 44, d || s_phShow ? kInk : kOrange);
    IconBtn(dc, IC_ROT_R, 512, 417, 44, kText, A_PHROT, 1);
    IconBtn(dc, IC_FWD, 624, 417, 44, kText, A_PHSTEP, 1);
    IconBtn(dc, IC_FULL, 732, 417, 44, kText, A_PHFULL);   // the whole screen (the settings: Настройки -> Фото)
}

// "Media" (the user 2026-10-30: the video's and the photos' sections as one - room for "Network")
static void ScrSetVideo(HDC dc)
{
    static const wchar_t* scale[2] = { L"Вписать", L"Растянуть" };
    static const wchar_t* fit[2] = { L"Вписать", L"Заполн." };
    Settings(dc, S_SET_VIDEO);
    SetRow(dc, 64, L"Видео: масштаб");
    Seg(dc, 512, 70, 248, 44, scale, 2, G_VIDEO_SCALE, 15);
    SetRow(dc, 120, L"Видео в движении", L"выключено: в движении видео на паузе");
    Toggle(dc, 688, 130, T_VIDEO_MOVING);
    SetRow(dc, 176, L"Фото: интервал слайд-шоу");
    wchar_t iv[16];
    _snwprintf(iv, 16, L"%d С", s_cfg.photoInterval);
    Stepper(dc, 560, 180, 200, iv, 48, 18, A_PHINT);
    SetRow(dc, 232, L"Фото: масштаб");
    Seg(dc, 512, 238, 248, 44, fit, 2, G_PHOTO_SCALE, 15);
    SetRow(dc, 288, L"Слайд-шоу при открытии");
    Toggle(dc, 688, 298, T_SLIDES_OPEN);
    SetRow(dc, 344, L"Автоповорот фото", NULL, true);
    Toggle(dc, 688, 354, T_AUTOROTATE);
}

static void DoActionOn(const Btn& b);

// A button's action; "Music" opened by it while our music waits under the radio: it goes on (sheet 28 p. 1-2);
// AUX opened: the MCU's AUX input (AuxOn)
static void DoAction(const Btn& b)
{
    Screen was = s_screen;
    DoActionOn(b);
    if (TabOpened(was, s_screen, S_AUX))
        AuxOn();
    if (TabOpened(was, s_screen, S_AUDIO) && MusicBackOnTab(s_musicHeld, PlayerPlaying())) {
        VLog(L"плеер: «Музыка» открыта - продолжаю после паузы источником");
        PlayerToggle();
        BuildButtons();
    }
}

static void DoActionOn(const Btn& b)
{
    if (b.action != A_DELETE)
        s_delArmed = false;
    if (b.action != A_TASKKILL)
        s_taskArm = 0;
    s_clipMenu = false;
    switch (b.action) {
    case A_TASKTAB:    s_taskTab = b.arg; s_taskPage = 0; s_status[0] = 0; break;
    case A_TASKSORT:   TaskSort(&s_task, b.arg); s_taskPage = 0; break;
    case A_TASKKILL:   TaskEnd((DWORD)b.arg); break;
    case A_TASKSWITCH: TaskSwitch((HWND)(INT_PTR)b.arg); break;
    case A_NAVCHIP: NavChipTap(); break;
    case A_TASKCLOSE:  TaskClose((HWND)(INT_PTR)b.arg); break;
    case A_CLIPCOPY:   ClipCopy(); break;
    case A_CLIPPASTE:  ClipPaste(); break;
    case A_CLIPCLEAR:  s_keys.text[0] = 0; s_keys.cur = 0; break;
    case A_SAVERBG:   // Settings -> General -> Theme: the saver's background - the mosaic, or a picture (the photo
        if (b.arg == 1) {   // browser picks it, with thumbnails; in the current picture's folder)
            wchar_t cur[MAX_PATH];
            SaverFilePath(cur);
            wcscpy(s_phPrevDir, s_phDir);
            s_phPrevSet = true;
            s_phPickMode = true;
            wchar_t* sl = wcsrchr(cur, L'\\');
            if (sl)
                sl[1] = 0;
            PhotoOpenDir(sl && GetFileAttributes(cur) != 0xFFFFFFFF ? cur : L"");
            Go(S_SAVER_PICK, true);
            break;
        }
        s_cfg.saverBg = 0;
        ApplyAccent();
        SaverFree();
        SaveCfg();
        break;
    case A_ACCENT:   // Settings -> General -> Theme: the accent colour
        s_cfg.accent = b.arg >= 0 && b.arg < kAccentCount ? b.arg : 0;
        ApplyAccent();
        SaveCfg();
        break;
    case A_NAV: {   // a section of the bottom bar (or a widget's screen): the bar's pill slides there
        int from = NavSection(s_screen);
        Nav((Screen)b.arg);
        NavAnimStart(from, NavSection(s_screen));
        break;
    }
    case A_NIGHT:    SetTheme(!s_night); break;   // the bar: the theme (the backlight: in the settings)
    case A_PHFILE:   // a tile: a folder, a picture (the viewer); the path line: -1 up, -3 the volumes
        if (b.arg == -3) {
            PhotoOpenDir(L"");
        } else if (b.arg < 0) {
            wchar_t d[MAX_PATH];
            wcscpy(d, s_phDir);
            ParentDir(d);
            PhotoOpenDir(d);
        } else if (PhIsDir(b.arg)) {
            wchar_t d[MAX_PATH];
            _snwprintf(d, MAX_PATH, L"%s%s", s_phDir, s_phs[b.arg]);
            PhotoOpenDir(d);
        } else if (s_screen == S_SAVER_PICK) {   // the saver's picture chosen: back to the theme
            int n = (int)wcslen(s_dir);
            _snwprintf(s_cfg.saverFile, MAX_PATH, L"%s%s", _wcsnicmp(s_phDir, s_dir, n) ? s_phDir : s_phDir + n,
                       s_phs[b.arg]);
            s_cfg.saverFile[MAX_PATH - 1] = 0;
            s_cfg.saverBg = 1;
            ApplyAccent();
            SaverFree();
            SaveCfg();
            Back();
        } else {
            s_phFull = false;
            PhotoView(b.arg);
            Go(S_PHOTO_VIEW, true);
            PhotoShow(s_cfg.photoSlides);
        }
        break;
    case A_PHSTEP:   PhotoStep(b.arg); break;
    case A_SCROLL:   if (s_scroll) ScrollTo(*s_scroll, s_scroll->pos + b.arg * s_scroll->step); break;
    case A_PHROT:    s_phRot = (s_phRot + b.arg + 4) % 4; PhotoView(s_phCur); break;
    case A_PHSHOW:   PhotoShow(!s_phShow); break;
    case A_PHFULL:   // the viewer on the whole screen and back: the picture decoded again for its box
        s_phFull = !s_phFull;
        PhotoView(s_phCur);
        break;
    case A_PHINT: {   // the slide show's interval: 2, 3, 5, 10, 15, 30, 60 s
        static const int iv[7] = { 2, 3, 5, 10, 15, 30, 60 };
        int k = 0;
        while (k < 6 && iv[k] < s_cfg.photoInterval)
            k++;
        k = k + b.arg < 0 ? 0 : k + b.arg > 6 ? 6 : k + b.arg;
        s_cfg.photoInterval = iv[k];
        SaveCfg();
        break;
    }
    case A_PLUGEV:   PlugEvent(b.arg); break;
    case A_PLUGNAV:  PlugNavTap(b.arg); break;
    case A_CAMOVERSTEP: {   // the speeding threshold: 0..99 km/h over the camera's limit
        int n = s_cfg.camOverBy + b.arg;
        s_cfg.camOverBy = n < 0 ? 0 : n > 99 ? 99 : n;
        if (s_cfg.camOver >= 0)
            s_cfg.camOver = s_cfg.camOverBy;
        SaveCfg();
        break;
    }
    case A_PLUGON:   PlugSwitch(b.arg); break;
    case A_PLUGPAGE: s_plugPage += b.arg; if (s_plugPage < 0) s_plugPage = 0; break;
    case A_PLUGOPEN:
        if (b.arg >= 0 && b.arg < s_plugN && s_plug[b.arg].ns)
            PlugOpen(s_plug[b.arg], 0);
        break;
    case A_USERWG:   // a user widget's tap (an add-on's: to the add-on)
        if (b.arg >= 0 && b.arg < s_uwCount && PlugWidgetTap(s_uw[b.arg]))
            break;
        if (b.arg >= 0 && b.arg < s_uwCount && s_uw[b.arg].tap[0])
            UserWgRun(s_uw[b.arg], s_uw[b.arg].tap, L"нажатие");
        break;
    case A_WGVIS:
        s_cfg.homeHidden ^= 1 << b.arg;
        SaveCfg();
        if (WgShown(b.arg)) {   // shown again: to work now (a hidden one does nothing)
            if (b.arg >= WG_COUNT)
                UserWgWake(b.arg - WG_COUNT);
            else if (b.arg == WG_WEATHER)
                WeatherRefresh();
        }
        break;
    case A_WGMOVE: {   // arg: id * 2 + (1 later, 0 earlier)
        int i = 0;
        while (i < WgCount() && s_cfg.homeOrder[i] != b.arg / 2)
            i++;
        int dir = b.arg % 2 ? 1 : -1, j = i + dir;
        while (j >= 0 && j < WgCount() && !WgListed(s_cfg.homeOrder[j]))   // over the ones not in the list
            j += dir;
        if (i < WgCount() && j >= 0 && j < WgCount()) {
            int t = s_cfg.homeOrder[i];
            s_cfg.homeOrder[i] = s_cfg.homeOrder[j];
            s_cfg.homeOrder[j] = t;
            SaveCfg();
        }
        break;
    }
    case A_THEME:
    case A_LIGHT:
        (b.action == A_THEME ? s_cfg.themeMode : s_cfg.lightMode) = b.arg;
        SaveCfg();
        NightAuto(true);
        break;
    case A_SCREEN:   Go((Screen)b.arg, true); break;
    case A_TAB:      Go((Screen)b.arg, false); break;
    case A_BACK:     Back(); break;
    case A_TOGGLE:
        s_on[b.arg] = !s_on[b.arg];
        if (RsetOfToggle(b.arg) >= 0)   // a radio setting: the MCU's key, its reply sets the switch
            McuKey(kRsetKey[RsetOfToggle(b.arg)][s_on[b.arg] ? 1 : 0], true);
        if (IsCfgToggle(b.arg))
            ControlsToCfg();
        if (b.arg == T_LOGS)
            LogsSet(s_on[T_LOGS]);
        if (b.arg == T_BT)
            BtPowerSet(s_on[T_BT]);
        if (b.arg == T_PB_AUTO) {   // the phone book at each connect (the user 2026-11-12: off by default)
            s_cfg.pbAuto = s_on[T_PB_AUTO] ? 1 : 0;
            SaveCfg();
        }
        if (b.arg == T_AUTOCONNECT || b.arg == T_AUTOANSWER) {   // the stack's options (the proxy), kept for the next boot
            BtCmdText(BT_CMD_SETOPT, b.arg == T_AUTOCONNECT ? (s_on[b.arg] ? L"c 1" : L"c 0")
                                                            : (s_on[b.arg] ? L"a 1" : L"a 0"));
            (b.arg == T_AUTOCONNECT ? s_cfg.btAutoConnect : s_cfg.btAutoAnswer) = s_on[b.arg] ? 1 : 0;
            SaveCfg();
        }
        break;
    case A_SEG:
        s_seg[b.arg / 16] = b.arg % 16;
        if (b.arg / 16 == G_PHOTO_SCALE) {   // fit / fill: the picture viewed again
            s_cfg.photoFill = b.arg % 16 == 1;
            SaveCfg();
        }
        if (b.arg / 16 == G_TIME_FMT) {   // 12 / 24 h: the clock
            s_cfg.time12 = b.arg % 16 == 0;
            SaveCfg();
        }
        if (b.arg / 16 == G_SAVER) {
            s_cfg.saverMin = kSaverMins[b.arg % 16 % 5];
            SaveCfg();
        }
        if (b.arg / 16 == G_WPROV || b.arg / 16 == G_CAMDIST || b.arg / 16 == G_VIDEO_SCALE || b.arg / 16 == G_CAMSND)
            ControlsToCfg();
        if (b.arg / 16 == G_CAMSND && b.arg % 16 > 0)
            Beep2();   // a sample: how it sounds over the radio
        break;
    case A_KEYS:     OpenCfgKeys(b.arg); break;
    case A_HOMEGPS:  HomeFromGps(); break;
    case A_SVCDONE:  SvcDone(); break;
    case A_TZ:       TzStep(b.arg); break;
    case A_GPSPORT:  GpsPortStep(b.arg); break;
    case A_NMEACOM: {   // the navigators' port: its number; loaded again under the new one when on
        int n = s_cfg.nmeaCom + b.arg;
        s_cfg.nmeaCom = n < kNmeaComMin ? kNmeaComMax : n > kNmeaComMax ? kNmeaComMin : n;
        SaveCfg();
        if (s_cfg.nmeaPort) {
            NmeaPortStart(true, true);
        }
        break;
    }
    case A_TANK:   // the fuel tank, L: 20..120
        s_cfg.tank = s_cfg.tank + b.arg < 20 ? 20 : s_cfg.tank + b.arg > 120 ? 120 : s_cfg.tank + b.arg;
        SaveCfg();
        break;
    case A_DTCINFO: {   // a code's meaning: open (its index) / close (-1)
        ObdInfo o;
        ObdGet(&o);
        if (b.arg >= 0 && b.arg < o.dtcN)
            DtcInfo(o.dtc[b.arg]);
        else
            s_dtcCode[0] = 0;
        break;
    }
    case A_TELINE: case A_TEADD: case A_TEDEL: case A_TESAVE: case A_TEBACK:
        TeAction(b.action, b.arg);
        break;
    case A_TEXTFILES:
        OpenFiles(F_TEXT);
        break;
    case A_UWEDIT:   // a user widget's .ini (Home -> widgets)
        if (b.arg >= 0 && b.arg < s_uwCount) {
            wchar_t path[MAX_PATH];
            _snwprintf(path, MAX_PATH, L"%s%s", s_uw[b.arg].dir, s_uw[b.arg].file);   // its folder (SD or SDMMC)
            path[MAX_PATH - 1] = 0;
            TeOpen(path, S_SET_HOME, true);
        }
        break;
    case A_DTCREAD:
        s_dtcArm = false;
        ObdReadDtc();
        break;
    case A_DTCCLEAR:   // asked twice
        if (!s_dtcArm) {
            s_dtcArm = true;
        } else {
            s_dtcArm = false;
            VLog(L"OBD: сброс кодов ошибок");
            ObdClearDtc();
        }
        break;
    case A_GPSBAUD:  GpsBaudStep(b.arg); break;
    case A_HOUR:     ClockStep(true, b.arg); break;
    case A_MINUTE:   ClockStep(false, b.arg); break;
    case A_CANLOG:   CanLogToggle(); break;
    case A_CANMARK:  MarkLogs(); break;
    case A_MCULOG:   McuLogToggle(); break;
    case A_PLAY:     PlayerToggle(); break;
    case A_PREVTRK:  PlayerPrev(); break;
    case A_NEXTTRK:  PlayerStep(1, false); break;
    case A_SEEK:     PlayerSeek(b.arg); break;
    case A_SHUFFLE:  s_cfg.shuffle = !s_cfg.shuffle; SaveCfg(); break;
    case A_REPEAT:   s_cfg.repeat = !s_cfg.repeat; SaveCfg(); break;
    case A_AVTEST:   Beep2(); break;
    case A_AVDEV:    AvDevNext(); break;
    case A_PRESET:   RadioOn(); RadioPreset(b.arg); break;
    case A_RKEY:     RadioKey(b.arg); break;
    case A_RPAGE:    RadioPageTo(s_radioPage + 1); break;   // the band button: the next page, round
    case A_RLIST:    McuSend3("SPApi_SendListItemControl", 0, b.arg, 0); break;
    case A_RSCAN:    RadioScan(); break;
    case A_RFILL:    RadioFill(); break;
    case A_MODEMTEST: ModemNet(); break;
    case A_NETOP:     NetOpStep(b.arg); break;
    case A_NETPAN:    NetPanTap(); break;
    case A_OSMFAV:    OsmFavGo(b.arg); break;
    case A_BTMUSIC:  BtMusicOn(); break;
    case A_BTCMD: {
        int want = -1;   // (play / pause: shown at once)
        if (b.arg == BT_CMD_PLAYPAUSE || b.arg == BT_CMD_PLAY) {   // the phone's music: the source too, ours stop
            int vol, mute, src;
            VolGet(&vol, &mute, &src);
            OursQuiet();
            if (src != 9)
                BtMusicOn();
            BtState bt;
            if (BtGet(&bt, NULL))
                want = b.arg == BT_CMD_PLAY || !BtPlayingNow(bt) ? 1 : 0;
        }
        BtCmd(b.arg);
        BtMusicSoon(want);
        break;
    }
    case A_REBOOTNO: McuKey(0x5A, false); Back(); break;
    case A_MFILE:    MusicPick(b.arg); break;
    case A_PHONECALL:
        wcscpy(s_callOut, s_dial);
        BtCmdText(BT_CMD_DIAL, s_dial);
        Go(S_CALL, true);
        break;
    case A_PBCONTACT:
        s_pbContact = b.arg;
        Go(S_CONTACT, true);
        break;
    case A_PBDIAL:   // a contact's number (contact * 4 + number) or a call's (4000 + call)
        if (b.arg >= 4000 && b.arg - 4000 < s_callsN)
            PbDial(s_calls[b.arg - 4000].num[0]);
        else if (b.arg < 4000 && b.arg / 4 < s_bookN)
            PbDial(s_book[b.arg / 4].num[b.arg % 4]);
        break;
    case A_PBRELOAD: s_pbNoAuto = false; PbAsk(b.arg); break;
    case A_PBDEL:    // asked twice (4 s): this phone's book and log, its file
        if (s_pbDelArm && GetTickCount() - s_pbDelArm < 4000) {
            s_pbDelArm = 0;
            PbDelete();
        } else {
            s_pbDelArm = GetTickCount();
        }
        break;
    case A_PBFIND:   OpenKeysFor(s_pbFind, 23, L"Поиск контакта", S_CONTACTS); s_bookPage = 0; break;
    case A_PBSIM:    // the other book: pulled anew
        s_pbStorage ^= 1;
        free(s_book);
        s_book = NULL;
        s_bookN = -1;
        s_bookPage = 0;
        PbAsk(BT_PB_PB);
        break;
    case A_PBFAV:    // the contact's first number in / out of the favourites
        if (b.arg >= 0 && b.arg < s_bookN && s_book[b.arg].num[0][0]) {
            wchar_t nm[64], num[24];
            BtText(s_book[b.arg].name, nm, 64);
            BtText(s_book[b.arg].num[0], num, 24);
            FavToggle(nm, num);
        }
        break;
    case A_FAVDIAL: {
        wchar_t names[kFavMax][48], nums[kFavMax][24];
        if (b.arg < FavCount(names, nums)) {
            wcscpy(s_callOut, nums[b.arg]);
            BtCmdText(BT_CMD_DIAL, nums[b.arg]);
            Go(S_CALL, true);
        }
        break;
    }
    case A_BTCONNECT:
        BtOwnerSet(BTO_PHONE);
        BtCmdText(BT_CMD_CONNECT, s_cfg.btLast);
        wcscpy(s_status, L"подключаю телефон…");
        VLog(L"телефон: подключить %s", s_cfg.btLast);
        break;
    case A_PTYSEL:   s_ptySel = b.arg; break;
    case A_CARSENS: {   // a sensor in / out of the car widget's lines
        int at = -1, n = 0;
        for (int i = 0; i < 3; i++) {
            if (s_cfg.carSens[i] == b.arg)
                at = i;
            n += s_cfg.carSens[i] >= 0;
        }
        if (at >= 0) {   // out: the later ones move up
            for (int i = at; i < 2; i++)
                s_cfg.carSens[i] = s_cfg.carSens[i + 1];
            s_cfg.carSens[2] = -1;
        } else if (n < 3) {
            s_cfg.carSens[n] = b.arg;
        } else {
            wcscpy(s_status, L"уже три: сначала уберите одно");
        }
        SaveCfg();
        break;
    }
    case A_PTYGO:    PtySearch(); Back(); break;
    case A_SHELL:
        if (ShellStart())
            DestroyWindow(s_wnd);
        else
            Redraw();   // (the restart did not take: what to do, in the top bar)
        return;
    case A_PHONEANS:
        BtCmd(BT_CMD_ANSWER);
        Back();
        Go(S_CALL, true);
        break;
    case A_PHONEEND:
        BtCmd(BT_CMD_END);
        if (s_screen == S_INCOMING || s_screen == S_CALL)
            Back();
        break;
    case A_MALL:
        if (!BassLoad())
            wcscpy(s_status, L"звук не запускается (BASS)");
        else if (PlayerLoadDir(s_musDir, NULL, true))
            Back();
        else
            wcscpy(s_status, L"здесь и глубже нет музыки");
        break;
    case A_MUP:      MusicUp(b.arg != 0); break;
    case A_VFILE:    VidPick(b.arg); break;
    case A_VUP:      VidUp(b.arg != 0); break;
    case A_VPLAY:    VidToggle(); break;
    case A_VPREV:    VidStep(-1); break;
    case A_VNEXT:    VidStep(1); break;
    case A_VSEEK:    VidSeek(b.arg); break;
    case A_VFULL:    s_vidFull = !s_vidFull; break;
    case A_NRPLAY:   NetToggle(); break;
    case A_NRSTEP:   NetStep(b.arg); break;
    case A_NRPICK:   // ours / the favourites: back to the player; a catalogue page stays (more to try there)
        NetPlay(s_netView, b.arg);
        if (s_netView != NL_CAT)
            Back();
        break;
    case A_NRLIST:   NetListAction(b.arg); break;
    case A_NRFAV:    NetFavAction(b.arg); break;
    case A_NCTILE:   NcTile(b.arg); break;
    case A_NCPICK:   NcPick(b.arg); break;
    case A_NCMORE:   NcStepPage(b.arg); break;
    case A_NCSORT:   NcToggleSort(); break;
    case A_NCSEARCH: NcSearchOpen(); break;
    case A_WIFINET:  WifiTap(b.arg); break;
    case A_WIFIFORGET: WifiForgetTap(b.arg); break;
    case A_WIFIDEL:   // saved networks: the second press forgets
        if (s_wsAsk != b.arg) {
            s_wsAsk = b.arg;
        } else if (b.arg >= 0 && b.arg < s_cfg.wifiCount) {
            wchar_t ssid[33];
            wcscpy(ssid, s_cfg.wifiSsid[b.arg]);
            WifiKnownSet(ssid, NULL);
            SaveCfg();
            s_wsAsk = -1;
            _snwprintf(s_status, 96, L"сеть %s забыта", ssid);
            s_status[95] = 0;
        }
        break;
    case A_WIFISCAN: WifiScan(); break;
    case A_OBDSCAN:   // the stack's search; its devices are read from the proxy's events from now on
        s_obdFoundN = 0;
        s_obdEvSeen = BtShmGet() ? BtShmGet()->evSeq : -1;
        s_obdSearchTick = GetTickCount();
        if (s_screen == S_SET_OBD) {   // the adapter's: the phone let go first (the search broke its link, HU
            BtOwnerSet(BTO_OBD);       // 2026-11-03); a new phone's search no longer gives Bluetooth to the adapter
            BtOwnerTick();             // (sheet 22: it let the phone go and turned the stack's connects off)
        }
        VLog(L"bt: поиск устройств");
        BtCmd(BT_CMD_SEARCH);
        Redraw();
        break;
    case A_OBDPICK:
        if (b.arg >= 0 && b.arg < s_obdFoundN) {
            MultiByteToWideChar(CP_ACP, 0, s_obdFound[b.arg].addr, -1, s_obdAddr, 18);
            s_obdAddr[17] = 0;
            SaveCfg();
        }
        Redraw();
        break;
    case A_OBDPIN: s_obdPin = b.arg; Redraw(); break;
    case A_PNPICK:   // a new phone picked (Bluetooth -> Paired -> + new)
        if (b.arg >= 0 && b.arg < s_obdFoundN) {
            strncpy(s_pnAddr, s_obdFound[b.arg].addr, 17);
            s_pnAddr[17] = 0;
        }
        break;
    case A_BTVISIBLE:
        BtVisible(!s_visUntil);
        break;
    case A_PAIRANSWER:   // a phone's request: accept (1) / refuse (0)
        BtCmdText(BT_CMD_PAIRCONFIRM, b.arg ? L"1" : L"0");
        VLog(L"bt: запрос сопряжения %S - %s", s_pairAsk.addr, b.arg ? L"принят" : L"отклонён");
        if (b.arg)
            BtOwnerSet(BTO_PHONE);   // a phone paired: Bluetooth for it (HU 2026-11-04: else let go every 10 s)
        Back();
        break;
    case A_PNPAIR: {
        s_pnPairTick = GetTickCount();
        wchar_t t[40];
        _snwprintf(t, 40, L"%S %S", s_pnAddr, kObdPins[s_obdPin]);
        t[39] = 0;
        VLog(L"bt: сопряжение телефона %s", t);
        BtOwnerSet(BTO_PHONE);
        BtCmdText(BT_CMD_PAIR, t);
        break;
    }
    case A_PNCONN: {
        wchar_t t[20];
        _snwprintf(t, 20, L"%S", s_pnAddr);
        t[19] = 0;
        VLog(L"bt: подключить %s", t);
        BtOwnerSet(BTO_PHONE);
        BtCmdText(BT_CMD_CONNECT, t);
        break;
    }
    case A_RVCVIEW:   // the camera's picture now, whatever the gear (powered only in reverse? - to be seen)
        s_rvcManual = true;
        s_rvcManualTick = GetTickCount();
        VLog(L"камера: показать вручную");
        RvcTick();
        break;
    case A_BTUNPAIR: {   // the first press asks, the second removes
        BtState bt;
        if (b.arg != s_pairedAsk) {
            s_pairedAsk = b.arg;
        } else if (BtGet(&bt, NULL) && b.arg < bt.paired && b.arg < BT_PAIRED) {
            wchar_t a[18];
            BtText(bt.pairedAddr[b.arg], a, 18);
            VLog(L"bt: удалить сопряжение %s", a);
            BtCmdText(BT_CMD_UNPAIR, a);
            s_pairedAsk = -1;
        }
        Redraw();
        break;
    }
    case A_OBDPAIR: {
        wchar_t t[40];
        _snwprintf(t, 40, L"%s %S", s_obdAddr, kObdPins[s_obdPin]);
        t[39] = 0;
        VLog(L"obd: сопряжение %s", t);
        s_obdPairTick = GetTickCount();   // (first: no connect from the tick meanwhile)
        BtOwnerSet(BTO_OBD);
        BtOwnerTick();
        BtCmdText(BT_CMD_PAIR, t);
        break;
    }
    case A_OBDCONN: {
        ObdInfo o;
        ObdGet(&o);
        BtState bt;
        if (o.state == OBD_CONNECTING || o.state == OBD_ON) {
            BtPhoneBack(L"OBD отключён");
        } else if (BtGet(&bt, NULL) && BtPhoneOn(bt)) {   // the phone let go, the adapter 5 s later (BtOwnerTick)
            BtOwnerSet(BTO_OBD);
            BtOwnerTick();
        } else {
            BtOwnerSet(BTO_OBD);
            if (s_btoLastAct == BTA_NONE)   // the stack's own connects of the phone off all the same
                BtCmdText(BT_CMD_PHONE, L"0");
            BtObdConnectNow();
        }
        Redraw();
        break;
    }
    case A_WIFIAP: {   // the access point by hand: on until pressed again
        WifiInfo w;
        WifiGet(&w);
        WifiApManual(!(w.state == WIFI_AP && !w.apAuto));
        break;
    }
    case A_PAGESTEP: {   // a paged list's arrows (kPagers: the tasks)
        const Pager* p = PagerOf(s_screen);
        int to;
        if (p && PageStep(*p, b.arg, &to)) {
            *p->page = to;
            s_slideDir = b.arg;
        }
        break;
    }
    case A_MCUREQ:   McuRequest(); break;
    case A_EQ:       EqSet(b.arg); break;
    case A_EQPRESET: EqPresetSet(b.arg); break;
    case A_BAL:      BalanceStep(false, b.arg); break;
    case A_FADE:     BalanceStep(true, b.arg); break;
    case A_BALCENTER: BalanceStep(false, 0); break;
    case A_SNDVOL: {   // the volumes (ScrSetVolume): row * 4 + 1 (down) / + 3 (up)
        int row = b.arg / 4;
        bool up = b.arg % 4 == 3;
        if (row == 0) {   // the navigation / the keys: its value
            int v = (s_cfg.keyVol < 0 ? 10 : s_cfg.keyVol) + (up ? 1 : -1);
            s_cfg.keyVol = v < 0 ? 0 : v > 30 ? 30 : v;
            McuSend3("SPApi_SendRadioKeyVolData", s_cfg.keyVol, 0, 0);
            McuKey(0x9E, true);
            SaveCfg();
        } else {   // the start's cap: 0 (off), 5..30
            int v = s_cfg.startMax + (up ? 1 : -1);
            s_cfg.startMax = v < 5 ? (up ? 5 : 0) : v > 30 ? 30 : v;
            SaveCfg();
        }
        break;
    }
    case A_WXNOW:    WeatherRefresh(); break;
    case A_CAMFILE:  OpenFiles(F_CAMS); break;
    case A_KIND:     ToggleKind(b.arg); break;
    case A_KINDS:    KindsPreset(b.arg); break;
    case A_REGION:   ToggleRegion(b.arg); break;
    case A_CAMDL:    CamDownload(); break;
    case A_KEY:
    case A_DEL: {    // typing into the dialled number or the Wi-Fi password
        int field = b.action == A_KEY ? b.arg / 65536 : b.arg;
        wchar_t* text = field == K_DIAL ? s_dial : s_pass;
        int cap = field == K_DIAL ? 19 : 23, n = (int)wcslen(text);
        if (b.action == A_DEL && n > 0) {
            text[n - 1] = 0;
        } else if (b.action == A_KEY && n < cap) {
            text[n] = (wchar_t)(b.arg % 65536);
            text[n + 1] = 0;
            if (field == K_DIAL)
                DtmfPlay(text[n]);
        }
        break;
    }
    case A_PAGE: {   // the dots go round
        const Pager* p = PagerOf(s_screen);
        if (!p || p->pages() < 1)
            break;
        int to = (*p->page + 1) % p->pages();
        if (s_screen == S_RADIO)
            RadioPageTo(to);
        else
            *p->page = to;
        s_slideDir = 1;
        break;
    }
    case A_RUN:      if (s_editMode) OpenEditor(b.arg); else RunApp(b.arg); break;
    case A_ADD:      OpenEditor(-1); break;
    case A_EDITMODE: s_editMode = !s_editMode; break;
    case A_FIELD:    if (b.arg == F_PATH || b.arg == F_ICON) OpenFiles(b.arg); else OpenKeys(b.arg); break;
    case A_NOICON:   s_edit.iconPath[0] = 0; break;
    case A_SAVE:     SaveEdit(); break;
    case A_DELETE:   if (s_delArmed) DeleteEdit(); else s_delArmed = true; break;
    case A_CANCEL:   // editor -> apps, keyboard -> where it was opened from, browser -> editor / cameras
        if (s_screen == S_EDIT) {
            s_screen = S_APPS;
            s_status[0] = 0;
        } else if (s_screen == S_KEYS) {
            s_screen = s_keysBack;
        } else {
            s_screen = s_filesFor == F_CAMS ? S_WSET_SPEED : s_filesFor == F_TEXT ? S_DIAG : S_EDIT;
            s_status[0] = 0;
        }
        break;
    case A_OK:       KeysDone(); break;
    case A_CHAR:     KeyChar((wchar_t)b.arg); break;
    case A_SHIFT:    s_keys.shift = !s_keys.shift; break;
    case A_LANG:     s_keys.en = !s_keys.en; break;
    case A_BKSP:     KeyBack(); break;
    case A_FILE:     PickFile(b.arg); break;
    case A_UP:       UpDir(); break;
    case A_MINIMIZE: MinimizeHome(); break;
    case A_CALIBRATE: Calibrate(); break;
    case A_TEST:     if (++s_testStep >= kTestSteps) Back(); break;
    case A_EXIT:     s_exitCode = 3; StartExplorer(); DestroyWindow(s_wnd); return;   // (3: not the shell after)
    default:         break;
    }
    BuildButtons();
}

// The last registered button wins: one drawn over another (a widget's own button) is the one pressed.
static int HitTest(int x, int y)
{
    for (int i = kBtnCount - 1; i >= 0; i--) {
        const RECT& r = s_btns[i].rc;
        if (x >= r.left && x < r.right && y >= r.top && y < r.bottom)
            return i;
    }
    return -1;
}

// (the start's screen, vh_resume.inc: here, after the photos it opens)
static void ResumeMedia()   // the file of the screen the start went back to (before the first frame)
{
    int what = s_resumeMedia;
    s_resumeMedia = 0;
    if (what == 1) {   // the video: its folder as the playlist, the file parked from its start
        FreeNames(s_vp, &s_vpCount);
        wcsncpy(s_vpDir, s_cfg.videoDir, MAX_PATH - 1);
        s_vpDir[MAX_PATH - 1] = 0;
        s_vpCount = ReadMusicDir(s_vpDir, s_vp, kVidMax, false, true);
        for (int i = 0; i < s_vpCount; i++)
            if (!_wcsicmp(s_vp[i], s_cfg.videoFile)) {
                s_vpCur = i;
                s_vidParked = true;
                s_vidParkPos = 0;
                s_vidState = VID_PAUSED;
                VLog(L"запуск: видео «%s»", s_cfg.videoFile);
            }
    } else if (what == 2) {   // the photo in the viewer
        PhotoOpenDir(s_cfg.photoDir);
        for (int i = 0; i < s_phCount; i++)
            if (!_wcsicmp(s_phs[i], s_cfg.photoFile)) {
                PhotoView(i);
                VLog(L"запуск: фото «%s»", s_cfg.photoFile);
            }
    }
}

// ---- the panel's and the wheel's buttons without the stock shell (the user 2026-10-22): MCU 0x4C01 as G3NavHMI's
// CInputTrigger handles it (Ghidra 2026-10-21, docs/MCU.md) - only under VestaStub (its MCU host holds the event
// VestaMcuHost; while the stock shell runs, it does them itself and VestaHome must not do them twice).
// key 7 long (the power button on the volume knob) - the screen off / on; the wheel's 0x12 "phone" - a call coming
// answered, otherwise up: the radio's seek up, the next track / station (held - the phone's screen); 0x14 "hang up" -
// a call ended, otherwise down (the user 2026-10-22: as the stock shell, by the mode); the wheel's up 9 / down 8 -
// next / previous, also while OSMapsCE's map is in front (the user 2026-11-02; held - its zoom, PlugKey).
static bool McuHostOn()
{
    HANDLE h = CreateEvent(NULL, TRUE, FALSE, L"VestaMcuHost");
    bool on = h && GetLastError() == ERROR_ALREADY_EXISTS;
    if (h)
        CloseHandle(h);
    return on;
}

static void Backlight(bool on)   // the stock shell's SV_BacklightOn / Off(0) (desay_sdk_api.dll, the OS's)
{
#ifdef UNDER_CE
    static HMODULE s_sv;
    if (!s_sv)
        s_sv = LoadLibrary(L"desay_sdk_api.dll");
    typedef int (*SvFn)(int);
    SvFn f = s_sv ? (SvFn)GetProcAddress(s_sv, on ? L"SV_BacklightOn" : L"SV_BacklightOff") : NULL;
    if (f)
        f(0);
#endif
}

// Play / pause of the source playing (an add-on's "media cmd=toggle"): our player, the internet radio, the phone.
static void MediaToggle()
{
    int vol, mute, src;
    VolGet(&vol, &mute, &src);
    if (PlayerPlaying() || (s_chan && s_screen == S_AUDIO))
        PlayerToggle();
    else if (s_netState != NR_OFF && s_netState != NR_ERROR)
        NetToggle();
    else if (src == 9)
        BtCmd(BT_CMD_PLAYPAUSE);
}

static void MediaStep(int dir)   // the source playing: the next / previous track, station
{
    int vol, mute, src;
    VolGet(&vol, &mute, &src);
    if (s_netOn)
        NetStep(dir);
    else if (PlayerPlaying())
        dir > 0 ? PlayerStep(1, false) : PlayerPrev();
    else if (s_vidState == VID_PLAYING)
        VidStep(dir);
    else if (src == 9)
        BtCmd(dir > 0 ? BT_CMD_NEXT : BT_CMD_PREV);
    else if (src == 0)
        RadioPresetStep(dir);
}

static void HwKey(int key, int st, int kind)
{
    if (!McuHostOn())
        return;
    VLog(L"кнопка 0x%02X %s %s", key, st ? L"отпущена" : L"нажата", kind == 2 ? L"долго" : kind == 3 ? L"повтор" : L"");
    s_lastTouch = GetTickCount();   // (a key is use too: the saver counts from it)
    BtState call;
    bool calling = BtGet(&call, NULL) && CallKeysOurs(call.call[0]);   // (a call: the buttons ours, HU 17.11)
    if (!calling && PlugKey(key, st, kind))   // the add-on in front took it (OSMapsCE: the zoom on a long up / down)
        return;
    static bool s_dark;
    if (key == 7 && st == 0 && kind == 2) {   // the screen
        s_dark = !s_dark;
        McuSend3("SPApi_SendRadioNaviTFTOpenClose", s_dark ? 0 : 1, 0, 0);
        Backlight(!s_dark);
    } else if (key == 0x12 && st == 0 && kind == 2) {   // "phone" held: its screen
        Nav(S_PHONE);
    } else if (key == 0x12 && st == 1 && kind == 1) {   // "phone": a call coming answered, otherwise up
        BtState bt;
        if (BtGet(&bt, NULL) && bt.call[0] == BT_CALL_INCOMING) {
            BtCmd(BT_CMD_ANSWER);
            Go(S_CALL, true);
        } else {
            MediaStep(1);
        }
    } else if (key == 0x14 && st == 1 && kind == 1) {   // "hang up": a call ended, otherwise down
        BtState bt;
        if (BtGet(&bt, NULL) && bt.call[0] != BT_CALL_NONE) {
            BtCmd(BT_CMD_END);
            if (s_screen == S_INCOMING || s_screen == S_CALL)
                Back();
        } else {
            MediaStep(-1);
        }
    } else if ((key == 8 || key == 9) && st == 1 && kind == 1) {   // up 9 / down 8 (HU 2026-10-31: they were swapped)
        MediaStep(key == 9 ? 1 : -1);
    } else {
        return;
    }
    BuildButtons();
    Redraw();
}
