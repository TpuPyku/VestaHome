// vh_sources.inc - part of home.cpp (included there, in order): one sound source at a time, the beep, the keypad tones.

// ---- one source at a time (HU 2026-10-20: the radio stayed on under our music, Bluetooth played with it). The
// radio and Bluetooth music are the MCU's own sources; ours (music, internet radio, video) sound through waveOut
// only while the MCU's source is "media" (0x4904 06/07) - turned on as the shell's "USB music" does it:
// SendMediaSourceSelect(5, 3) after its 0x30.
// ---- the MCU's main menu keys (0x7F radio, 0x84 BT): taken only in its main menu (0x1102 00 16) - the shell goes
// there by "back" 0x30, a level a press (2 from the SD player or Bluetooth's music; Ghidra + the shell's logs
// 2026-10-29: our 0x7F from the player was ignored - the radio silent, its presets dead). After each "back" up to
// 0.5 s for the MCU's answer: the main menu - the key; a level below (0x9101 / 0x8902 / 0x8905) - back again; no
// answer - it is there already (it answers none in the main menu). At most 4 backs.
enum { TIMER_MENUKEY = 24, kMenuBacks = 4 };
static int   s_mkKey;     // the key waiting (0: none)
static int   s_mkBacks;
static DWORD s_mkSent;    // the last back (0: none yet)

static void MenuKeyNow()
{
    KillTimer(s_wnd, TIMER_MENUKEY);
    int key = s_mkKey;
    s_mkKey = 0;
    DWORD now = GetTickCount();
    VLog(L"mcu: клавиша 0x%02X после «назад» x%d (%s)", key, s_mkBacks,
         s_menuMain ? L"главное меню" : s_mkBacks >= kMenuBacks ? L"меню не подтверждено" : L"без ответа - уже там");
    McuKey(key, false);
    s_menuMain = false;   // (the key leaves the main menu - the radio's or the phone's; the MCU says nothing of it: HU
                          // 2026-10-31, radio -> Bluetooth: 0x84 sent with no "back" from the radio's menu, ignored)
    if (key == 0x7F)
        s_srcByUs = s_radioSrcAsk = now;   // (the preset 1.5 s after the key itself)
    else if (key == 0x84)
        SetTimer(s_wnd, TIMER_BTSRC, 800, NULL);   // then Bluetooth's music key 0x54
}

static void MenuKeyStep()   // TIMER_MENUKEY, every 100 ms
{
    if (!s_mkKey)
        return;
    DWORD now = GetTickCount();
    bool answered = s_mkSent && (LONG)(s_menuTick - s_mkSent) >= 0;
    if ((!s_mkSent && s_menuMain) || (answered && (s_menuMain || s_mkBacks >= kMenuBacks)) ||
        (s_mkSent && !answered && now - s_mkSent >= 500)) {
        MenuKeyNow();
        return;
    }
    if (s_mkSent && !answered)
        return;   // waiting for the answer
    McuKey(0x30, false);
    s_mkSent = GetTickCount() | 1;
    s_mkBacks++;
}

static void MainMenuKey(int key)
{
    s_mkKey = key;
    s_mkBacks = 0;
    s_mkSent = 0;
    SetTimer(s_wnd, TIMER_MENUKEY, 100, NULL);
    MenuKeyStep();
}

// Our "media" asked (SendMediaSourceSelect(5, 3)): VestaStub answers the MCU's player request (0x9101) with the card
// only then - its answer to every 0x9101 put the MCU back into the player after each "back" (2026-10-29)
static void MediaAskSignal(bool on)
{
    static HANDLE s_ev;
    if (!s_ev)
        s_ev = CreateEvent(NULL, TRUE, FALSE, L"VestaMediaAsk");
    if (s_ev)
        on ? SetEvent(s_ev) : ResetEvent(s_ev);
}

static void OursQuiet()   // the radio or Bluetooth takes over: our players stop (pause)
{
    NetStop();
    if (PlayerPlaying())
        s_bass.pause(s_chan);
    VidPauseForAudio();
}

static void BtQuiet()   // the phone's music paused (inaudible under another source, it would still stream)
{
    BtState bt;
    if (BtGet(&bt, NULL) && bt.av == 2)
        BtCmd(BT_CMD_PAUSE);
}

// The screen after a source changed not by us - MODE, the phone (the user 2026-10-29): with VestaHome in front on the
// radio, the music or the Bluetooth music only (the home screen's widget shows it; another program in front, the
// settings, the phone - untouched): the media -> "Music", Bluetooth -> its tab, the radio -> the radio (its preset's page).
// The same change opens the volume window first (a screen over the one left: HU 2026-10-29 - the screen stayed), so
// then the screen under it follows, the window closing onto the new one.
static void BuildButtons();
static void RedrawFrom(const char* file, int line);

static void FollowSource(int src)
{
    if (!IsWindowVisible(s_wnd) || GetForegroundWindow() != s_wnd)
        return;
    bool over = s_screen == S_VOLUME && s_histLen > 0;
    Screen* at = over ? &s_hist[s_histLen - 1] : &s_screen;
    if (*at != S_RADIO && *at != S_AUDIO && *at != S_AUDIO_BT)
        return;
    Screen to = src == 6 || src == 7 ? S_AUDIO : src == 9 ? S_AUDIO_BT : src == 0 ? S_RADIO : *at;
    if (to == *at)
        return;
    VLog(L"источник %d: экран следом%s", src, over ? L" (под окном громкости)" : L"");
    if (over) {
        *at = to;
        s_media = to;   // (as Go does for the media screens)
    } else {
        Go(to, false);
    }
    BuildButtons();
    Redraw();
}

// The phone's play while another source sounds (the user 2026-10-29: the radio on, play on the phone - the phone's
// music): the source to Bluetooth (its key, as "Media -> Bluetooth"), once on its start (the playing state's edge),
// not right after our own switching (a pause sent then may come late)
static void PhonePlayTakes(int src, DWORD now)
{
    static int s_avWas = -1;
    BtState bt;
    if (!BtGet(&bt, NULL))
        return;
    if (bt.av != s_avWas)   // (HU 2026-10-29: it worked twice, then not - whether the phone's state came at all)
        VLog(L"телефон: музыка %d -> %d (источник %d, наше переключение %lu с назад)", s_avWas, bt.av, src,
             (now - s_srcByUs) / 1000);
    if (bt.av == 2 && s_avWas >= 0 && s_avWas != 2 && src != 9 && now - s_srcByUs > 5000 && McuHostOn()) {
        VLog(L"телефон: играет - источник Bluetooth");
        MainMenuKey(0x84);
        s_srcByUs = now;
    }
    s_avWas = bt.av;
}

static void MediaSource()   // one of our players starts
{
    BtQuiet();
    int vol, mute, src;
    VolGet(&vol, &mute, &src);
    if (src == 6 || src == 7 || src == 9)
        return;   // media, or Bluetooth's path: ours sound
    if (McuHostOn()) {   // no shell: its main menu's "Media" (FUN_005f7cdc, Ghidra 2026-10-27) - the MCU knows the
        VLog(L"источник: медиа для нашего звука (было %d)", src);   // cards from our host now, MODE turns it 07
        MediaAskSignal(true);
        McuSend2("SPApi_SendMediaSourceSelect", 5, 3);
        s_srcByUs = s_mediaAsk = GetTickCount();
        return;
    }
    // Straight to Bluetooth's path: SendMediaSourceSelect never made it "media" by itself, and it woke the shell's
    // media player - with it our video stayed black, sound only (HU 2026-10-20-3; the picture was there on the
    // 18th, before the call): the hardware overlay is one.
    VLog(L"источник: путь Bluetooth для нашего звука (было %d)", src);
    MainMenuKey(0x84);
    s_srcByUs = s_btPathTick = GetTickCount();
}


static void VidStep(int dir)
{
    if (s_vpCount > 0)
        VidPlay((s_vpCur + dir + s_vpCount) % s_vpCount);
}

static void VidSeek(int k)   // k of 20 along the bar
{
    double pos, len;
    if (VideoTimes(&pos, &len) && len > 0)
        VideoSeek(len * k / 20);
}

static int CarKmh()
{
    Gps g;
    GpsGet(&g);
    if (g.carTick && GetTickCount() - g.carTick < 3000)
        return g.carSpeed;
    return GpsFresh(g, 5000) ? (int)g.speedKmh : 0;
}

// Every 250 ms while a video is open: its bus, the end (the next file of the folder), the car's speed; the
// player's bar once a second.
static void VidTick()
{
    if (s_vidState == VID_NONE || s_vidState == VID_ERROR) {
        KillTimer(s_wnd, TIMER_VIDEO);
        return;
    }
    int was = s_vidState;
    if (!s_vidParked)   // parked: no pipeline, only the speed is watched (to come back when the car stops)
        s_vidState = VideoPoll(s_vidMsg, 160);
    if (s_vidSeekTo > 0 && s_vidState == VID_PLAYING) {   // back from parking: the position once it can seek
        double pos, len;
        if (VideoTimes(&pos, &len) && len > 0) {
            VideoSeek(s_vidSeekTo);
            s_vidSeekTo = 0;
        }
    }
    if (s_vidState == VID_ENDED && was != VID_ENDED && s_vpCur + 1 < s_vpCount) {
        VidPlay(s_vpCur + 1);
        return;
    }
    int kmh = CarKmh();
    bool moving = !s_cfg.videoMoving && (s_vidMoving ? kmh >= 3 : kmh >= 8);   // a margin against flicker
    bool redraw = moving != s_vidMoving || s_vidState != was;
    s_vidMoving = moving;
    static DWORD s_barTick;
    bool bar = false;
    if (s_screen == S_VIDEO_PLAY && GetTickCount() - s_barTick >= 1000) {
        s_barTick = GetTickCount();
        bar = true;
    }
    if (IsWindowVisible(s_wnd) && GetForegroundWindow() == s_wnd) {
        if (redraw)
            Redraw();
        else if (bar && !s_vidFull)
            RedrawArea(0, 398, 800, 480);   // the bar and the times
    }
}

static int VidPages()
{
    int per = kVidRows * kVidCols;
    return s_vidCount ? (s_vidCount + per - 1) / per : 1;
}

// Media -> video (design 0.6): the path line ("‹" up, a tap on the path - the volumes), then the volumes, folders
// and videos; a file plays its folder from it. Pages: a swipe, the dots.
static void ScrVideo(HDC dc)
{
    if (!s_vidRead)
        VidOpenDir(s_cfg.videoDir[0] && GetFileAttributes(s_cfg.videoDir) != 0xFFFFFFFF ? s_cfg.videoDir : L"");
    Frame(dc, L"Медиа · Видео");
    MediaTabs(dc, S_VIDEO, IC_NONE, A_NONE, 0);
    if (s_vidPage >= VidPages())
        s_vidPage = VidPages() - 1;
    DirPath(dc, s_vidDir, A_VUP, 0, 1, 784);
    DirEntries(dc, s_vidDir, s_vids, s_vidCount, s_vidPage, kVidRows, kVidCols, A_VFILE, IC_VIDEO,
               s_vpCur >= 0 && !_wcsicmp(s_vpDir, s_vidDir) ? s_vp[s_vpCur] : NULL, L"Здесь нет папок и видео");
    PageDots(dc, s_vidPage, VidPages(), 400, 396);
}

// The player: the picture (our child window) between the top bar and the dock: the bar (a tap seeks), the
// list, previous, play / pause, next, the whole screen, the settings.
static void ScrVideoPlay(HDC dc)
{
    const wchar_t* name = s_vpCur >= 0 ? s_vp[s_vpCur] : NULL;
    wchar_t s[MAX_PATH + 16];
    _snwprintf(s, MAX_PATH + 16, L"Видео · %s", name ? name : L"Нет файла");
    s[MAX_PATH + 15] = 0;
    Frame(dc, s);
    Fill(dc, 0, 56, 800, 398, RGB(0, 0, 0));
    if (s_vidState == VID_ERROR || s_vidState == VID_NONE) {
        T(dc, 400, 210, s_vidState == VID_ERROR ? L"Видео не играет" : L"Ничего не открыто", 22, kText, 700, AL_MID, 720);
        T(dc, 400, 244, s_vidState == VID_ERROR ? s_vidMsg : L"Выберите файл в «Медиа → Видео»", 16, kMuted, 400,
          AL_MID, 720);
    } else if (s_vidMoving) {
        T(dc, 400, 210, L"В движении — пауза", 22, kText, 700, AL_MID, 720);
        T(dc, 400, 244, L"Картинка вернётся на остановке (или включите её в настройках видео)", 16, kMuted, 400,
          AL_MID, 720);
    }
    Dock(dc);
    double pos = 0, len = 0;
    bool have = (s_vidState == VID_PLAYING || s_vidState == VID_PAUSED || s_vidState == VID_ENDED) &&
                VideoTimes(&pos, &len);
    int k = len > 0 ? (int)(pos * 640 / len) : 0;
    R(dc, 80, 404, 640, 6, 3, kLine);
    if (have) {
        R(dc, 80, 404, k > 6 ? k : 6, 6, 3, kOrange);
        Disc(dc, 80 + k, 407, 8, kText);
        for (int z = 0; z < 20; z++)
            Zone(80 + z * 32, 392, 33, 28, A_VSEEK, z);
    }
    Mmss(s, 16, pos);
    T(dc, 16, 414, s, 15, kMuted, 600);
    Mmss(s, 16, len);
    T(dc, 784, 414, s, 15, kMuted, 600, AL_END);
    IconBtn(dc, IC_LIST, 124, 430, 40, kText, A_BACK);
    IconBtn(dc, IC_PREV, 236, 430, 40, kText, A_VPREV);
    bool down = Zone(376, 426, 48, 48, A_VPLAY);
    Disc(dc, 400, 450, 24, down ? kFacet : kOrange);
    Ico(dc, s_vidState == VID_PLAYING ? IC_PAUSE : IC_PLAY, 384, 434, 32, kInk);
    IconBtn(dc, IC_FWD, 524, 430, 40, kText, A_VNEXT);
    IconBtn(dc, s_vidFull ? IC_UNFULL : IC_FULL, 632, 430, 40, s_vidFull ? kOrange : kText, A_VFULL);
    IconBtn(dc, IC_GEAR, 736, 430, 40, kText, A_SCREEN, S_SET_VIDEO);
}
