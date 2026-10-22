// vh_video.inc - part of home.cpp (included there, in order): video.

// ---- video (Media -> Video): a browser of the volumes' video files, then the player (video.cpp: the shell's
// GStreamer) drawing into a child window of ours over the screen's picture area; a tap on the picture: the whole
// screen and back. The picture pauses when another screen or program comes up, and while the car moves it is
// hidden (the sound goes on) unless the setting allows it. The playlist = the videos of the file's folder.
#define WM_APP_VIDTAP (WM_APP + 23)
#define VIDEO_CLASS L"VestaHome_Video"
enum { TIMER_VIDEO = 7, kVidMax = 400, kVidRows = 4, kVidCols = 2 };
static HWND     s_vidWnd;             // the picture
static wchar_t  s_vidDir[MAX_PATH];   // the browser's folder, "" = the volumes
static wchar_t* s_vids[kVidMax];      // its entries: folders ("\" at the end), then videos
static int      s_vidCount, s_vidPage;
static bool     s_vidRead;
static wchar_t  s_vpDir[MAX_PATH];    // the playing file's folder and its videos
static wchar_t* s_vp[kVidMax];
static int      s_vpCount, s_vpCur = -1;
static int      s_vidState;           // VID_* (video.h)
static bool     s_vidFull;            // the picture over the whole screen
static bool     s_vidMoving;          // the car moves and the picture is not allowed
static bool     s_vidParked;          // stopped by VidSync (see there), s_vidParkPos = where
static double   s_vidParkPos, s_vidSeekTo;
static wchar_t  s_vidMsg[160];        // why it doesn't play
static bool     s_vidShown;

static LRESULT CALLBACK VideoProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_ERASEBKGND: {   // black, as the stock player's window
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect((HDC)wp, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
        return 1;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        VideoExpose();   // the sink draws its frame again
        return 0;
    }
    case WM_LBUTTONUP:   // a tap on the picture: the whole screen and back
        PostMessage(s_wnd, WM_APP_VIDTAP, 0, 0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

static bool VidEnsureWnd()
{
    if (s_vidWnd)
        return true;
    WNDCLASS wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = VideoProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = VIDEO_CLASS;
    RegisterClass(&wc);
    s_vidWnd = CreateWindowEx(0, VIDEO_CLASS, L"", WS_CHILD, 0, 56, 800, 342, s_wnd, NULL, wc.hInstance, NULL);
    return s_vidWnd != NULL;
}

static bool VidPlay(int i);

// After every frame: the picture is shown only on the player's screen, with VestaHome in front, a video open and
// the car standing (or allowed). The picture is a hardware overlay: hiding our window leaves it on top of
// everything (HU 2026-10-18), so leaving the player (or the car moving) "parks" the video - stopped, the overlay
// gone, the position kept - and coming back opens it again from there.
static void VidSync()
{
    if (!s_vidWnd)
        return;
    bool open = s_vidState == VID_PLAYING || s_vidState == VID_PAUSED || s_vidState == VID_ENDED;
    bool front = IsWindowVisible(s_wnd) && GetForegroundWindow() == s_wnd;
    bool show = open && front && s_screen == S_VIDEO_PLAY && !s_vidMoving;
    if (open && !show && !s_vidParked) {
        double pos, len;
        VideoTimes(&pos, &len);
        VideoStop();
        s_vidParked = true;
        s_vidParkPos = s_vidState == VID_ENDED ? 0 : pos;
        s_vidState = VID_PAUSED;
    } else if (show && s_vidParked) {
        double pos = s_vidParkPos;
        if (VidPlay(s_vpCur))
            s_vidSeekTo = pos;   // VidTick seeks once the file is open
        return;
    }
    if (show) {
        if (s_vidFull)
            SetWindowPos(s_vidWnd, HWND_TOP, 0, 0, 800, 480, SWP_SHOWWINDOW);
        else
            SetWindowPos(s_vidWnd, HWND_TOP, 0, 56, 800, 342, SWP_SHOWWINDOW);
        if (!s_vidShown)
            VideoExpose();
    } else if (s_vidShown) {
        ShowWindow(s_vidWnd, SW_HIDE);
    }
    s_vidShown = show;
}

static void VidPauseForAudio()
{
    if (s_vidState == VID_PLAYING) {
        VideoPause(true);
        s_vidState = VID_PAUSED;
    }
}

static void VidOpenDir(const wchar_t* dir)
{
    FreeNames(s_vids, &s_vidCount);
    wcsncpy(s_vidDir, dir, MAX_PATH - 1);
    s_vidDir[MAX_PATH - 1] = 0;
    s_vidPage = 0;
    s_vidRead = true;
    s_vidCount = s_vidDir[0] ? ReadMusicDir(s_vidDir, s_vids, kVidMax, true, true) : ReadVolumes(s_vids);
}

static bool VidPlay(int i)
{
    ResumeByUser();
    if (i < 0 || i >= s_vpCount)
        return false;
    s_vpCur = i;
    s_vidFull = false;
    if (!VideoLoad(VLog, s_vidMsg, 160) || !VidEnsureWnd()) {
        if (!s_vidMsg[0])
            wcscpy(s_vidMsg, L"нет окна для видео");
        s_vidState = VID_ERROR;
        return false;
    }
    NetStop();
    if (PlayerPlaying())
        s_bass.pause(s_chan);
    SetWindowPos(s_vidWnd, HWND_TOP, 0, 56, 800, 342, SWP_SHOWWINDOW);   // the sink takes the window as it is
    s_vidShown = true;
    wchar_t path[MAX_PATH];
    _snwprintf(path, MAX_PATH, L"%s%s", s_vpDir, s_vp[i]);
    path[MAX_PATH - 1] = 0;
    s_vidMsg[0] = 0;
    s_vidParked = false;
    s_vidSeekTo = 0;
    if (!VideoOpen(path, s_vidWnd, s_cfg.videoStretch, s_vidMsg, 160)) {
        VLog(L"видео: %s не открылось: %s", path, s_vidMsg);
        s_vidState = VID_ERROR;
        return false;
    }
    VLog(L"видео: %s", path);
    s_vidState = VID_PLAYING;
    MediaSource();
    SetTimer(s_wnd, TIMER_VIDEO, 250, NULL);
    wcscpy(s_cfg.videoDir, s_vpDir);
    wcsncpy(s_cfg.videoFile, s_vp[i], MAX_PATH - 1);
    s_cfg.videoFile[MAX_PATH - 1] = 0;
    SaveCfg();
    return true;
}

// The playlist = the videos of `dir`, starting with `file`.
static bool VidLoadDir(const wchar_t* dir, const wchar_t* file)
{
    FreeNames(s_vp, &s_vpCount);
    wcsncpy(s_vpDir, dir, MAX_PATH - 1);
    s_vpDir[MAX_PATH - 1] = 0;
    s_vpCount = ReadMusicDir(s_vpDir, s_vp, kVidMax, false, true);
    s_vpCur = -1;
    int start = 0;
    for (int i = 0; file && i < s_vpCount; i++)
        if (!_wcsicmp(s_vp[i], file))
            start = i;
    return s_vpCount > 0 && VidPlay(start);
}

static void VidPick(int i)
{
    if (i < 0 || i >= s_vidCount)
        return;
    const wchar_t* n = s_vids[i];
    wchar_t d[MAX_PATH];
    if (n[wcslen(n) - 1] == L'\\') {   // a folder (or a volume)
        _snwprintf(d, MAX_PATH, L"%s%s", s_vidDir, n);
        d[MAX_PATH - 1] = 0;
        VidOpenDir(d);
        return;
    }
    wchar_t file[MAX_PATH];
    wcscpy(file, n);
    wcscpy(d, s_vidDir);
    if (s_vidParked && s_vpCur >= 0 && s_vpCur < s_vpCount && !_wcsicmp(d, s_vpDir) && !_wcsicmp(s_vp[s_vpCur], file)) {
        Go(S_VIDEO_PLAY, true);   // the video left a moment ago: on from where it was (VidSync)
        return;
    }
    VidLoadDir(d, file);
    Go(S_VIDEO_PLAY, true);
}

static void VidUp(bool toVolumes)
{
    wchar_t d[MAX_PATH];
    wcscpy(d, s_vidDir);
    if (toVolumes)
        d[0] = 0;
    else
        ParentDir(d);
    VidOpenDir(d);
}

static void VidToggle()
{
    if (s_vidParked)
        return;   // parked on this screen = the car moves
    if (s_vidState == VID_PLAYING) {
        VideoPause(true);
        s_vidState = VID_PAUSED;
    } else if (s_vidState == VID_PAUSED || s_vidState == VID_ENDED) {
        PlayerStopStream();
        NetStop();
        VideoPause(false);
        s_vidState = VID_PLAYING;
        MediaSource();
    } else if (s_cfg.videoDir[0]) {   // nothing open: the last video
        VidLoadDir(s_cfg.videoDir, s_cfg.videoFile);
    }
}
