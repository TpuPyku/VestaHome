// vh_photo.inc - part of home.cpp (included there, in order): photos.

// ---- photos (Media -> Photo): the volumes, folders and pictures as tiles with thumbnails (4 x 2 a page, a swipe
// turns it), the viewer between the top bar and the dock: previous / next (and a swipe), a quarter turn either
// way, the slide show. The pictures are decoded by photo.cpp in a thread of their own (a photo takes a second or
// two on the unit): the viewed one first, then the thumbnails of the page shown; a result posts WM_APP_PHOTO.
#define WM_APP_PHOTO (WM_APP + 25)
#define WM_APP_OBD (WM_APP + 27)
enum { TIMER_PHOTO = 13, kPhMax = 1000, kPhTiles = 8, kThW = 172, kThH = 92, kPhBoxW = 800, kPhBoxH = 342 };
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
static Pic      s_pv;
static CRITICAL_SECTION s_phCs;
static HANDLE   s_phEv, s_phThread;

static DWORD WINAPI PhotoThread(LPVOID)
{
    for (;;) {
        WaitForSingleObject(s_phEv, INFINITE);
        for (;;) {
            wchar_t path[MAX_PATH];
            int slot = -1, rot = 0;
            bool view = false, fill = false, exif = false;
            EnterCriticalSection(&s_phCs);
            if (s_pvState == 1) {   // the viewed picture first
                view = true;
                wcscpy(path, s_pvPath);
                rot = s_pvRot;
                fill = s_pvFill;
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
            Pic pic = { 0, 0, NULL };
            int w = 0, h = 0;
            int res = view ? PicLoad(path, kPhBoxW, kPhBoxH, fill, rot, exif, &pic, &w, &h)
                           : PicThumb(path, kThW, kThH, exif, &pic);
            EnterCriticalSection(&s_phCs);
            if (view && s_pvState == 2 && !wcscmp(path, s_pvPath) && rot == s_pvRot && fill == s_pvFill) {
                PicFree(&s_pv);
                s_pv = pic;
                s_pvErr = res;
                s_pvW = w;
                s_pvH = h;
                s_pvState = 3;
                pic.px = NULL;
            } else if (!view && s_th[slot].state == 1 && !wcscmp(path, s_th[slot].path)) {
                s_th[slot].pic = pic;
                s_th[slot].state = res == PIC_OK ? 2 : 3;
                pic.px = NULL;
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
    if (s_phDir[0] && wcscmp(s_cfg.photoDir, s_phDir)) {
        wcscpy(s_cfg.photoDir, s_phDir);
        CfgSoon();
    }
}

static int PhotoTiles()   // the tiles of the folder: "up" first in a folder
{
    return s_phCount + (s_phDir[0] ? 1 : 0);
}

static int PhotoPages()
{
    int n = PhotoTiles();
    return n > 0 ? (n + kPhTiles - 1) / kPhTiles : 1;
}

// The picture into the frame at x, y (cut at the frame and at the partial redraw's area).
static void BlitPic(const Pic& p, int x, int y)
{
    if (!p.px)
        return;
#ifndef UNDER_CE
    GdiFlush();
#endif
    for (int r = 0; r < p.h; r++) {
        int fy = y + r;
        if (fy < s_clip.top || fy >= s_clip.bottom)
            continue;
        int x0 = x < s_clip.left ? s_clip.left : x, x1 = x + p.w > s_clip.right ? s_clip.right : x + p.w;
        if (x1 > x0)
            memcpy(s_backBits + (479 - fy) * 800 + x0, p.px + r * p.w + (x0 - x), (x1 - x0) * 2);
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
        int k = s_phPage * kPhTiles + t - (s_phDir[0] ? 1 : 0);
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
    DirTitle(dc, L"МЕДИА · ФОТО", s_phDir);
    MediaTabs(dc, S_PHOTO, IC_PLAY, A_PHPLAY, 0, S_SET_PHOTO);
    PhotoWantThumbs();
    int up = s_phDir[0] ? 1 : 0;
    for (int t = 0; t < kPhTiles; t++) {
        int k = s_phPage * kPhTiles + t - up, x = 16 + (t % 4) * 194, y = 124 + (t / 4) * 138;
        if (k >= s_phCount)
            break;
        bool down = Zone(x, y, 184, 128, A_PHFILE, k);   // k -1: up
        bool dir = k < 0 || PhIsDir(k);
        bool cur = !dir && k == s_phCur;
        R(dc, x, y, 184, 128, 12, down || cur ? kOrange : kPanel, down || cur ? CLR_INVALID : kLine);
        wchar_t name[MAX_PATH];
        if (k < 0)
            wcscpy(name, L"НАВЕРХ");
        else {
            wcscpy(name, s_phs[k]);
            if (dir && name[0] && name[wcslen(name) - 1] == L'\\')
                name[wcslen(name) - 1] = 0;
        }
        if (dir) {
            Ico(dc, k < 0 ? IC_UP : IC_FOLDER, x + 64, y + 22, 56, down ? kInk : k < 0 ? kMuted : kOrange);
        } else {
            R(dc, x + 6, y + 6, kThW, kThH, 0, kMargin);
            EnterCriticalSection(&s_phCs);
            if (s_th[t].state == 2)
                BlitPic(s_th[t].pic, x + 6, y + 6);
            else
                Ico(dc, IC_PHOTO, x + 6 + kThW / 2 - 22, y + 6 + kThH / 2 - 22, 44, s_th[t].state == 3 ? kOff : kDim);
            LeaveCriticalSection(&s_phCs);
        }
        T(dc, x + 92, y + 118, name, 14, down || cur ? kInk : kMuted, 700, AL_MID, 172);
    }
    if (!s_phCount)
        T(dc, 400, 250, s_phDir[0] ? L"Здесь нет папок и фото (JPG, PNG, BMP)" : L"Нет носителей", 18, kMuted, 400,
          AL_MID);
    PageDots(dc, s_phPage, PhotoPages());
}

// The viewer: picture i of the folder asked for (its own turn reset), the slide show's timer as set.
static void PhotoView(int i)
{
    if (i < 0 || i >= s_phCount || PhIsDir(i))
        return;
    if (i != s_phCur)
        s_phRot = 0;
    s_phCur = i;
    s_phPage = (i + (s_phDir[0] ? 1 : 0)) / kPhTiles;
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
    wchar_t s[MAX_PATH + 32];
    _snwprintf(s, MAX_PATH + 32, L"ФОТО · %d / %d · %s", at, n, s_phCur >= 0 ? s_phs[s_phCur] : L"");
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
        static const wchar_t* why[5] = { L"", L"ФОРМАТ НЕ ПОДДЕРЖИВАЕТСЯ", L"СЛИШКОМ БОЛЬШОЕ ИЗОБРАЖЕНИЕ",
                                         L"ФАЙЛ НЕ ЧИТАЕТСЯ", L"НЕ ХВАТАЕТ ПАМЯТИ" };
        Panel(dc, 220, 110, 360, 230);
        Ico(dc, IC_PHOTO, 360, 140, 80, kOff);
        Line(dc, 352, 140, 448, 220, 4, kMuted);
        T(dc, 400, 262, why[err % 5], 18, kText, 700, AL_MID, 336);
        T(dc, 400, 292, s_phCur >= 0 ? s_phs[s_phCur] : L"", 17, kMuted, 400, AL_MID, 336);
        T(dc, 400, 318, err == PIC_FORMAT ? L"JPG (не прогрессивный), PNG, BMP" : L"Пропустить можно стрелками внизу",
          15, kDim, 400, AL_MID, 336);
    }
    Dock(dc);
    IconBtn(dc, IC_GALLERY, 24, 417, 44, kText, A_BACK);
    IconBtn(dc, IC_PREV, 132, 417, 44, kText, A_PHSTEP, -1);
    IconBtn(dc, IC_ROT_L, 244, 417, 44, kText, A_PHROT, -1);
    bool d = Zone(330, 410, 140, 58, A_PHSHOW);   // the slide show: orange while it runs
    R(dc, 330, 410, 140, 58, 10, d || s_phShow ? kOrange : kPanel2);
    Ico(dc, s_phShow ? IC_PAUSE : IC_PLAY, 378, 417, 44, d || s_phShow ? kInk : kOrange);
    IconBtn(dc, IC_ROT_R, 512, 417, 44, kText, A_PHROT, 1);
    IconBtn(dc, IC_FWD, 624, 417, 44, kText, A_PHSTEP, 1);
    IconBtn(dc, IC_GEAR, 732, 417, 44, kText, A_SCREEN, S_SET_PHOTO);
}

static void ScrSetPhoto(HDC dc)
{
    static const wchar_t* scale[2] = { L"ВПИСАТЬ", L"ЗАПОЛН." };
    Settings(dc, S_SET_PHOTO);
    SetRow(dc, 64, L"Интервал слайд-шоу");
    wchar_t iv[16];
    _snwprintf(iv, 16, L"%d С", s_cfg.photoInterval);
    Stepper(dc, 560, 68, 200, iv, 48, 18, A_PHINT);
    SetRow(dc, 120, L"Масштаб");
    Seg(dc, 512, 126, 248, 44, scale, 2, G_PHOTO_SCALE, 15);
    SetRow(dc, 176, L"Слайд-шоу при открытии");
    Toggle(dc, 688, 186, T_SLIDES_OPEN);
    SetRow(dc, 232, L"Автоповорот", NULL, true);
    Toggle(dc, 688, 242, T_AUTOROTATE);
}

static void DoAction(const Btn& b)
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
    case A_TASKCLOSE:  TaskClose((HWND)(INT_PTR)b.arg); break;
    case A_CLIPCOPY:   ClipCopy(); break;
    case A_CLIPPASTE:  ClipPaste(); break;
    case A_CLIPCLEAR:  s_keysText[0] = 0; break;
    case A_NAV:      Nav((Screen)b.arg); break;
    case A_NIGHT:    SetTheme(!s_night); break;   // the bar: the theme (the backlight: in the settings)
    case A_PHFILE:   // a tile: up, a folder, a picture (the viewer)
        if (b.arg < 0) {
            wchar_t d[MAX_PATH];
            wcscpy(d, s_phDir);
            ParentDir(d);
            PhotoOpenDir(d);
        } else if (PhIsDir(b.arg)) {
            wchar_t d[MAX_PATH];
            _snwprintf(d, MAX_PATH, L"%s%s", s_phDir, s_phs[b.arg]);
            PhotoOpenDir(d);
        } else {
            PhotoView(b.arg);
            Go(S_PHOTO_VIEW, true);
            PhotoShow(s_cfg.photoSlides);
        }
        break;
    case A_PHPLAY:   // the tab's play: the slide show from the picture viewed (or the first)
        if (s_phCur < 0)
            PhotoStep(1);
        else
            PhotoView(s_phCur);
        if (s_phCur >= 0) {
            Go(S_PHOTO_VIEW, true);
            PhotoShow(true);
        }
        break;
    case A_PHSTEP:   PhotoStep(b.arg); break;
    case A_SCROLL:   if (s_scroll) ScrollTo(*s_scroll, s_scroll->pos + b.arg * s_scroll->step); break;
    case A_PHROT:    s_phRot = (s_phRot + b.arg + 4) % 4; PhotoView(s_phCur); break;
    case A_PHSHOW:   PhotoShow(!s_phShow); break;
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
    case A_USERWG:   // a user widget's tap
        if (b.arg >= 0 && b.arg < s_uwCount && s_uw[b.arg].tap[0])
            UserWgRun(s_uw[b.arg], s_uw[b.arg].tap, L"нажатие");
        break;
    case A_SHPAGE:   s_setHomePage += b.arg; break;
    case A_WGVIS:
        s_cfg.homeHidden ^= 1 << b.arg;
        SaveCfg();
        break;
    case A_WGMOVE: {   // arg: id * 2 + (1 later, 0 earlier)
        int i = 0;
        while (i < WgCount() && s_cfg.homeOrder[i] != b.arg / 2)
            i++;
        int j = b.arg % 2 ? i + 1 : i - 1;
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
        if (b.arg == T_AUTOCONNECT || b.arg == T_AUTOANSWER)   // the stack's options (the proxy)
            BtCmdText(BT_CMD_SETOPT, b.arg == T_AUTOCONNECT ? (s_on[b.arg] ? L"c 1" : L"c 0")
                                                            : (s_on[b.arg] ? L"a 1" : L"a 0"));
        if (b.arg == T_LOUDNESS)   // the shell's loudness keys (CAudioEffectDlg, Ghidra 2026-10-22; to verify)
            McuKey(s_on[T_LOUDNESS] ? 0x93 : 0x94, false);
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
        if (b.arg / 16 == G_SPEED_VOL) {   // (SpeedVolTick follows it)
            s_cfg.speedVol = b.arg % 16;
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
    case A_TZ:       TzStep(b.arg); break;
    case A_GPSPORT:  GpsPortStep(b.arg); break;
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
    case A_AVTEST:   AvTestRun(b.arg); break;
    case A_AVDEV:    AvDevNext(); break;
    case A_PRESET:   RadioOn(); RadioPreset(b.arg); break;
    case A_RKEY:     RadioKey(b.arg); break;
    case A_RPAGE:    RadioPageTo(s_radioPage + 1); break;   // the band button: the next page, round
    case A_RLIST:    McuSend3("SPApi_SendListItemControl", 0, b.arg, 0); break;
    case A_RLISTPAGE: s_rlistPage += b.arg; if (s_rlistPage < 0) s_rlistPage = 0; break;
    case A_RSCAN:    RadioScan(); break;
    case A_BTMUSIC:  BtMusicOn(); break;
    case A_BTCMD:
        if (b.arg == BT_CMD_PLAYPAUSE || b.arg == BT_CMD_PLAY) {   // the phone's music: the source too, ours stop
            int vol, mute, src;
            VolGet(&vol, &mute, &src);
            OursQuiet();
            if (src != 9)
                BtMusicOn();
        }
        BtCmd(b.arg);
        break;
    case A_REBOOTNO: McuKey(0x5A, false); Back(); break;
    case A_MFILE:    MusicPick(b.arg); break;
    case A_PHONECALL:
        wcscpy(s_callOut, s_dial);
        BtCmdText(BT_CMD_DIAL, s_dial);
        Go(S_CALL, true);
        break;
    case A_PHONECLR: s_dial[0] = 0; break;
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
    case A_PBRELOAD: PbAsk(b.arg); break;
    case A_PBFIND:   OpenKeysFor(s_pbFind, 23, L"ПОИСК КОНТАКТА", S_CONTACTS); s_bookList.pos = 0; break;
    case A_PBSIM:    // the other book: pulled anew
        s_pbStorage ^= 1;
        free(s_book);
        s_book = NULL;
        s_bookN = -1;
        s_bookList.pos = 0;
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
    case A_SHELL:    ShellStart(); DestroyWindow(s_wnd); return;
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
    case A_NRPICK:   NetPlay(b.arg); Back(); break;
    case A_WIFINET:  WifiTap(b.arg); break;
    case A_WIFIFORGET: WifiForgetTap(b.arg); break;
    case A_WIFISCAN: WifiScan(); break;
    case A_OBDSCAN:   // the stack's search; its devices are read from the proxy's events from now on
        s_obdFoundN = 0;
        s_obdEvSeen = BtShmGet() ? BtShmGet()->evSeq : -1;
        s_obdSearchTick = GetTickCount();
        VLog(L"obd: поиск устройств");
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
    case A_RVCVIEW:   // the camera's picture now, whatever the gear (powered only in reverse? - to be seen)
        s_rvcManual = true;
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
        BtCmdText(BT_CMD_PAIR, t);
        break;
    }
    case A_OBDCONN: {
        ObdInfo o;
        ObdGet(&o);
        if (o.state == OBD_CONNECTING || o.state == OBD_ON) {
            ObdStop();
        } else {
            char a[18];
            WideCharToMultiByte(CP_ACP, 0, s_obdAddr, -1, a, 18, NULL, NULL);
            a[17] = 0;
            ObdConnect(a);
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
    case A_PAGESTEP: {   // a paged list's arrows (kPagers: Wi-Fi, tasks, the cameras' regions)
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
        } else if (row == 1) {
            McuKey(up ? 0x52 : 0x53, true);
        } else if (row == 2) {
            McuKey(up ? 0x95 : 0x96, false);
        } else if (row == 3) {
            McuKey(up ? 0x2D : 0x2B, false);
        } else if (row == 4) {   // the start's cap: 0 (off), 5..30
            int v = s_cfg.startMax + (up ? 1 : -1);
            s_cfg.startMax = v < 5 ? (up ? 5 : 0) : v > 30 ? 30 : v;
            SaveCfg();
        }
        if (row >= 1 && row <= 3)
            VLog(L"громкости: строка %d %s", row, up ? L"+" : L"-");
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
            s_screen = s_filesFor == F_CAMS ? S_WSET_SPEED : S_EDIT;
        }
        break;
    case A_OK:       KeysDone(); break;
    case A_CHAR:     KeyType((wchar_t)b.arg); break;
    case A_SHIFT:    s_keysShift = !s_keysShift; break;
    case A_LANG:     s_keysEn = !s_keysEn; break;
    case A_BKSP:     KeyBack(); break;
    case A_FILE:     PickFile(b.arg); break;
    case A_UP:       UpDir(); break;
    case A_MINIMIZE: MinimizeHome(); break;
    case A_CALIBRATE: Calibrate(); break;
    case A_SOON:     wcscpy(s_status, b.arg ? L"нужно чтение GPS (NMEA)" : L"нужен протокол MCU"); break;
    case A_TEST:     if (++s_testStep >= kTestSteps) Back(); break;
    case A_EXIT:     StartExplorer(); DestroyWindow(s_wnd); return;
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
// a call ended, otherwise down (the user 2026-10-22: as the stock shell, by the mode); 8 / 9 short - next / previous
// too (which is which: to verify on the unit).
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
        RadioKey(dir > 0 ? 0x13C : 0x13B);   // the seek up / down
}

static void HwKey(int key, int st, int kind)
{
    if (!McuHostOn())
        return;
    VLog(L"кнопка 0x%02X %s %s", key, st ? L"отпущена" : L"нажата", kind == 2 ? L"долго" : kind == 3 ? L"повтор" : L"");
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
    } else if ((key == 8 || key == 9) && st == 1 && kind == 1) {
        MediaStep(key == 8 ? 1 : -1);
    } else {
        return;
    }
    BuildButtons();
    Redraw();
}
