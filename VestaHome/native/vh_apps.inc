// vh_apps.inc - part of home.cpp (included there, in order): programs: the list, the editor, the keyboard, their screens.

// ---------------------------------------------------------------- programs (VestaLaunch's, built in)
// The "Apps" screen lists programs from apps.txt next to the exe; shortcuts are added, changed and removed
// right there (own on-screen keyboard and file browser: the unit has neither). The same code as VestaLaunch
// (native/launch.cpp), without its part for the stock shell: no Nav_* events, VestaHome doesn't hide.

static const wchar_t* kKeysRu[4] = { L"1234567890-_", L"йцукенгшщзхъ", L"фывапролджэё", L"ячсмитьбю.,!" };
static const wchar_t* kKeysEn[4] = { L"1234567890-_", L"qwertyuiop()", L"asdfghjkl:;\"", L"zxcvbnm.,/\\=" };
static const wchar_t  kKeysSym[] = L"!@#$%&*+?|~'";   // the digits' row with Shift (the configs: # @ & ? % + |)
enum { F_NAME, F_PATH, F_PARAMS, F_ICON, F_CAMS, F_TEXT };   // fields of the shortcut editor; F_CAMS: the camera file;
                                                             // F_TEXT: a text file for the editor

// Programs of the "Apps" screen, from apps.txt: "Name | \path\to\program.exe | class!title | parameters | icon".
struct App {
    wchar_t name[48];
    wchar_t path[MAX_PATH];
    wchar_t wclass[64];    // optional window to raise, "class!title" like AppLauncher; empty = any window of the process
    wchar_t wtitle[96];
    wchar_t params[200];   // command line passed to the program, may be empty
    wchar_t iconPath[MAX_PATH];   // own icon: .png, .ico or an exe/dll to take it from; relative = from our folder
    HICON   icon;          // the icon from an exe/dll/ico (the program's own if no iconPath), NULL if none
    Img     img;           // the icon from a .png, at most 64 x 64
    bool    exists;
};

// VestaHome is topmost (above the stock shell), so a started program must be raised above it explicitly.
// After the start its window is looked for every 300 ms for up to kWaitMs. The guard: while a raised program
// runs, every window of its process is kept topmost (a dialog it opens is a normal window and would land under
// VestaHome, and the program with it); when it closes, VestaHome is in front again.
enum { TIMER_WAIT = 3, TIMER_GUARD = 4, kWaitMs = 20000, kGuardMs = 500, kMaxRaised = 16 };
enum { TIMER_LONG = 6, kLongMs = 600 };   // a long press (the keyboard's text field)
static DWORD s_waitPid;
static int   s_waitApp = -1;   // -2: a program not in the list (a widget's tap=), found by s_waitPid
static DWORD s_waitTick;
static HWND  s_raised[kMaxRaised];   // windows we made topmost; dropped again when the program closes
static int   s_raisedCount;
static HANDLE s_guard;        // process of the program in front; NULL = no guard
static DWORD  s_guardPid;
static bool   s_guardOwned;   // s_guard was opened here (otherwise it is s_running's handle, not ours to close)

// Programs started from here: their processes, so that a second tap brings the running copy to the front
// instead of starting another one.
enum { kMaxRunning = 16 };
struct Running { wchar_t path[MAX_PATH]; HANDLE proc; DWORD pid; };
static Running s_running[kMaxRunning];

enum { kMaxApps = 64, kAppsPerPage = 8 };
static App   s_apps[kMaxApps];
static int   s_appCount;
static int   s_page;

// Shortcut editor: works on a copy, written to apps.txt on "Save".
static bool  s_editMode;          // apps screen: a tile opens its editor instead of starting the program
static int   s_editIdx;           // index in s_apps, -1 = new shortcut
static App   s_edit;
static bool  s_delArmed;          // "Delete" was pressed once; the second press deletes

static wchar_t*       s_keysTarget;   // the text being typed: a shortcut's field or a setting
static int            s_keysCap;
static const wchar_t* s_keysTitle;
static Screen         s_keysBack;     // where "Done" and "Cancel" return to
static wchar_t s_keysText[400];
// The cursor (2026-10-22, the user: long lines are edited in the middle): typing and backspace work at it; the finger
// puts and drags it anywhere on the field (WndProc: KeysCursorAt), its handle shows below the line. The field shows
// the text from s_keysView, as far as the cursor needs.
enum { kKeyFieldX = 32, kKeyFieldW = 560 };
static int s_keysCur, s_keysView, s_keysVisN;   // the cursor, the first character shown, how many are shown
static int s_keysDx[400];                      // the shown characters' right edges from kKeyFieldX (the finger's map)   // (the text editor's lines: up to 399)
static bool    s_keysEn, s_keysShift;
static bool    s_clipMenu;     // "Copy" / "Paste" shown in place of the text field

// File browser for the program path: folders and .exe files of s_fdir.
struct FileEnt { wchar_t name[100]; bool dir; };
enum { kMaxFiles = 256, kFilesPerPage = 10 };
static FileEnt s_files[kMaxFiles];
static int     s_fileCount, s_filePage;
static wchar_t s_fdir[MAX_PATH];  // ends with a backslash
static int     s_filesFor;        // F_PATH: .exe files; F_ICON: pictures (.png, .ico) and .exe/.dll

static bool LoadImg(const wchar_t* path, Img* im);

static int AppsPages()
{
    int tiles = s_appCount + (s_appCount < kMaxApps ? 1 : 0);   // + the "Add" tile
    return (tiles + kAppsPerPage - 1) / kAppsPerPage;
}

static int FilePages()
{
    int pages = (s_fileCount + kFilesPerPage - 1) / kFilesPerPage;
    return pages < 1 ? 1 : pages;
}

// A line too long for maxw shows its end after "…" (paths, typed text). Returns its right edge.
static int TTail(HDC dc, int x, int y, const wchar_t* s, int size, COLORREF c, int weight, int maxw)
{
    if (TextWidth(dc, s, size, weight) <= maxw)
        return T(dc, x, y, s, size, c, weight);
    wchar_t buf[MAX_PATH + 2];
    const wchar_t* p = s;
    do {
        p++;
        _snwprintf(buf, MAX_PATH + 2, L"…%s", p);
        buf[MAX_PATH + 1] = 0;
    } while (*p && TextWidth(dc, buf, size, weight) > maxw);
    return T(dc, x, y, buf, size, c, weight);
}

static bool Exists(const wchar_t* path)
{
    return GetFileAttributes(path) != 0xFFFFFFFF;
}

#define APPS_HEADER \
    "# Programs of the \"Apps\" screen: one per line, \"Name | full path to the exe\".\r\n" \
    "# Lines starting with # are ignored. Save the file as UTF-8.\r\n" \
    "# The started program is raised above the stock shell. If it starts another program (a launcher,\r\n" \
    "# a MortScript), name the window to raise as the third field: \"Name | path | class!title\".\r\n" \
    "# The fourth field is the program's command line: \"Name | path | | parameters\" (third field may be empty).\r\n" \
    "# The fifth field is an own icon: \"Name | path | | | Icons\\name.png\" - a .png (up to 256 x 256), .ico or\r\n" \
    "# an exe/dll to take the icon from; a path without a drive/root is from the launcher's folder.\r\n" \
    "# Shortcuts changed on the \"Apps\" screen rewrite this file; comments other than these are not kept.\r\n"

static const char kDefaultApps[] =
    APPS_HEADER
    "\xD0\xAF\xD0\xBD\xD0\xB4\xD0\xB5\xD0\xBA\xD1\x81 \xD0\xBA\xD0\xB0\xD1\x80\xD1\x82\xD1\x8B | \\SDMMC\\YaMapsCE\\YaMapsCE.exe\r\n"
    "OSM \xD0\xBA\xD0\xB0\xD1\x80\xD1\x82\xD1\x8B | \\SDMMC\\OSMapsCE\\OSMapsCE.exe\r\n"
    "CityGuide | \\SDMMC\\Navi\\CityGuide\\CityGuideCE.exe\r\n";

static void FreeAppIcon(App& a)
{
    ImgCopiesDrop();   // (its scaled copies)

    if (a.icon)
        DestroyIcon(a.icon);
    if (a.img.px)
        free(a.img.px);
    a.icon = NULL;
    a.img.px = NULL;
}

// A shortcut's own icon is kept at most 64 x 64 (the tile's size): averaged down from a bigger picture.
static void ShrinkImg(Img* im)
{
    if (im->w <= 64 && im->h <= 64)
        return;
    int w = im->w > 64 ? 64 : im->w, h = im->h > 64 ? 64 : im->h;
    unsigned char* out = (unsigned char*)malloc(w * h * 4);
    if (!out)
        return;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int x0 = x * im->w / w, x1 = (x + 1) * im->w / w, y0 = y * im->h / h, y1 = (y + 1) * im->h / h;
            int s[4] = { 0, 0, 0, 0 }, n = (x1 - x0) * (y1 - y0);
            for (int sy = y0; sy < y1; sy++)
                for (int sx = x0; sx < x1; sx++)
                    for (int k = 0; k < 4; k++)
                        s[k] += im->px[(sy * im->w + sx) * 4 + k];
            for (int k = 0; k < 4; k++)
                out[(y * w + x) * 4 + k] = (unsigned char)(s[k] / n);
        }
    free(im->px);
    im->px = out;
    im->w = w;
    im->h = h;
}

// The tile's icon: the one chosen for the shortcut (if it loads), otherwise the program's own.
static void LoadAppIcon(App& a)
{
    a.icon = NULL;
    a.img.px = NULL;
    if (a.iconPath[0]) {
        wchar_t file[MAX_PATH];
        if (a.iconPath[0] == L'\\' || wcschr(a.iconPath, L':'))
            wcscpy(file, a.iconPath);
        else
            _snwprintf(file, MAX_PATH, L"%s%s", s_dir, a.iconPath);
        file[MAX_PATH - 1] = 0;
        int n = (int)wcslen(file);
        if (n > 4 && !_wcsicmp(file + n - 4, L".png")) {
            if (LoadImg(file, &a.img)) {
                ShrinkImg(&a.img);
                return;
            }
        } else if (ExtractIconEx(file, 0, &a.icon, NULL, 1) && a.icon) {
            return;
        }
        a.icon = NULL;
    }
    if (a.exists)
        ExtractIconEx(a.path, 0, &a.icon, NULL, 1);
}

// Reads apps.txt (UTF-8, optional BOM); writes an example list on the first start.
static void LoadApps()
{
    for (int i = 0; i < s_appCount; i++)
        FreeAppIcon(s_apps[i]);
    s_appCount = 0;

    wchar_t path[MAX_PATH];
    _snwprintf(path, MAX_PATH, L"%sapps.txt", s_dir);
    DWORD got;
    char* buf = ReadAll(path, &got);
    if (!buf) {
        HANDLE h = CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        if (h != INVALID_HANDLE_VALUE) {
            DWORD w;
            WriteFile(h, kDefaultApps, sizeof(kDefaultApps) - 1, &w, NULL);
            CloseHandle(h);
        }
        buf = ReadAll(path, &got);
        if (!buf)
            return;
    }
    char* p = Utf8Text(buf);
    wchar_t line[400];
    while (s_appCount < kMaxApps && NextUtf8Line(&p, line, 400)) {
        Trim(line);
        wchar_t* bar = wcschr(line, L'|');
        if (!line[0] || line[0] == L'#' || line[0] == L';' || !bar)
            continue;
        *bar = 0;
        App& a = s_apps[s_appCount];
        wcsncpy(a.name, line, 47);
        a.name[47] = 0;
        Trim(a.name);
        // optional third field: window "class!title" (a leading '!' as in AppLauncher profiles is allowed),
        // optional fourth: the program's command line; optional fifth: its own icon
        a.wclass[0] = a.wtitle[0] = a.params[0] = a.iconPath[0] = 0;
        wchar_t* bar2 = wcschr(bar + 1, L'|');
        if (bar2) {
            *bar2 = 0;
            wchar_t* w = bar2 + 1;
            wchar_t* bar3 = wcschr(w, L'|');
            if (bar3) {
                *bar3 = 0;
                wchar_t* bar4 = wcschr(bar3 + 1, L'|');
                if (bar4) {
                    *bar4 = 0;
                    wcsncpy(a.iconPath, bar4 + 1, MAX_PATH - 1);
                    a.iconPath[MAX_PATH - 1] = 0;
                    Trim(a.iconPath);
                }
                wcsncpy(a.params, bar3 + 1, 199);
                a.params[199] = 0;
                Trim(a.params);
            }
            Trim(w);
            if (*w == L'!')
                w++;
            wchar_t* ex = wcschr(w, L'!');
            if (ex) {
                *ex = 0;
                wcsncpy(a.wtitle, ex + 1, 95);
                a.wtitle[95] = 0;
            }
            wcsncpy(a.wclass, w, 63);
            a.wclass[63] = 0;
            Trim(a.wclass);
            Trim(a.wtitle);
        }
        wcsncpy(a.path, bar + 1, MAX_PATH - 1);
        a.path[MAX_PATH - 1] = 0;
        Trim(a.path);
        if (!a.name[0] || !a.path[0])
            continue;
        a.exists = Exists(a.path);
        LoadAppIcon(a);
        s_appCount++;
    }
    free(buf);
}

// Writes s_apps back to apps.txt, through a temporary file so a power cut can't leave half a list.
static bool SaveApps()
{
    wchar_t path[MAX_PATH], tmp[MAX_PATH];
    _snwprintf(path, MAX_PATH, L"%sapps.txt", s_dir);
    _snwprintf(tmp, MAX_PATH, L"%sapps.tmp", s_dir);
    HANDLE h = CreateFile(tmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return false;
    DWORD w;
    bool ok = WriteFile(h, APPS_HEADER, sizeof(APPS_HEADER) - 1, &w, NULL) != 0;
    for (int i = 0; i < s_appCount && ok; i++) {
        const App& a = s_apps[i];
        wchar_t win[170], line[1000];
        if (a.wtitle[0])
            _snwprintf(win, 170, L"!%s!%s", a.wclass, a.wtitle);
        else
            wcscpy(win, a.wclass);
        if (a.iconPath[0])
            _snwprintf(line, 1000, L"%s | %s | %s | %s | %s\r\n", a.name, a.path, win, a.params, a.iconPath);
        else if (a.params[0])
            _snwprintf(line, 1000, L"%s | %s | %s | %s\r\n", a.name, a.path, win, a.params);
        else if (win[0])
            _snwprintf(line, 1000, L"%s | %s | %s\r\n", a.name, a.path, win);
        else
            _snwprintf(line, 1000, L"%s | %s\r\n", a.name, a.path);
        line[999] = 0;
        char out[3000];
        int n = WideCharToMultiByte(CP_UTF8, 0, line, -1, out, sizeof(out), NULL, NULL);
        ok = n > 1 && WriteFile(h, out, n - 1, &w, NULL);
    }
    CloseHandle(h);
    if (!ok) {
        DeleteFile(tmp);
        return false;
    }
    DeleteFile(path);
    return MoveFile(tmp, path) != 0;
}

static bool Alive(HANDLE proc)
{
    return proc && WaitForSingleObject(proc, 0) == WAIT_TIMEOUT;
}

// The process of a program started from here that still runs, 0 if none. Finished ones are forgotten.
static DWORD RunningPid(const wchar_t* path)
{
    DWORD pid = 0;
    for (int k = 0; k < kMaxRunning; k++) {
        Running& r = s_running[k];
        if (r.proc && !Alive(r.proc)) {
            CloseHandle(r.proc);
            r.proc = NULL;
        }
        if (r.proc && !_wcsicmp(r.path, path))
            pid = r.pid;
    }
    return pid;
}

static void Remember(const wchar_t* path, HANDLE proc, DWORD pid)
{
    int k = 0;
    while (k < kMaxRunning - 1 && s_running[k].proc)
        k++;
    if (s_running[k].proc)   // all slots busy: the last one is forgotten
        CloseHandle(s_running[k].proc);
    wcscpy(s_running[k].path, path);
    s_running[k].proc = proc;
    s_running[k].pid = pid;
}

struct FindApp { HWND found; };
static BOOL CALLBACK EnumApp(HWND h, LPARAM lp);

static void RunApp(int i)
{
    const App& a = s_apps[i];
    // Already running (started from here, or its window "class!title" is open): bring it to the front.
    s_waitPid = RunningPid(a.path);
    s_waitApp = i;
    FindApp f = { NULL };
    if (!s_waitPid && (a.wclass[0] || a.wtitle[0]))
        EnumWindows(EnumApp, (LPARAM)&f);
    if (s_waitPid || f.found) {
        s_waitTick = GetTickCount();
        SetTimer(s_wnd, TIMER_WAIT, 300, NULL);
        _snwprintf(s_status, 96, L"уже запущена: %s", a.name);
        return;
    }
    s_waitApp = -1;
    PROCESS_INFORMATION pi;
#ifdef UNDER_CE
    LPSTARTUPINFO si = NULL;          // must be NULL on Windows CE
    wchar_t cmd[200];                 // CE passes just the arguments
    wcscpy(cmd, a.params);
    wchar_t* args = cmd[0] ? cmd : NULL;
#else
    STARTUPINFO sib;
    memset(&sib, 0, sizeof(sib));
    sib.cb = sizeof(sib);
    LPSTARTUPINFO si = &sib;          // desktop Windows requires it
    wchar_t cmd[MAX_PATH + 204];      // desktop: the program name comes first
    _snwprintf(cmd, MAX_PATH + 204, L"\"%s\" %s", a.path, a.params);
    wchar_t* args = cmd;
#endif
    if (a.exists && CreateProcess(a.path, args, NULL, NULL, FALSE, 0, NULL, NULL, si, &pi)) {
        CloseHandle(pi.hThread);
        Remember(a.path, pi.hProcess, pi.dwProcessId);
        s_waitPid = pi.dwProcessId;
        s_waitApp = i;
        s_waitTick = GetTickCount();
        SetTimer(s_wnd, TIMER_WAIT, 300, NULL);
        _snwprintf(s_status, 96, L"запускается: %s", a.name);
    } else {
        _snwprintf(s_status, 96, L"не запускается: %s", a.name);
    }
}

// Window of the program being started: by class/title from apps.txt, otherwise any visible
// top-level window of its process.

static BOOL CALLBACK EnumApp(HWND h, LPARAM lp)
{
    if (h == s_wnd || !IsWindowVisible(h) || GetParent(h))
        return TRUE;
    static const App none = App();
    const App& a = s_waitApp >= 0 ? s_apps[s_waitApp] : none;
    bool match;
    if (a.wclass[0] || a.wtitle[0]) {
        wchar_t cls[64] = L"", title[96] = L"";
        GetClassName(h, cls, 64);
        GetWindowText(h, title, 96);
        match = (!a.wclass[0] || !wcscmp(cls, a.wclass)) && (!a.wtitle[0] || !wcscmp(title, a.wtitle));
    } else {
        DWORD pid = 0;
        GetWindowThreadProcessId(h, &pid);
        match = pid == s_waitPid;
    }
    if (match) {
        ((FindApp*)lp)->found = h;
        return FALSE;
    }
    return TRUE;
}

static void StopGuard()
{
    KillTimer(s_wnd, TIMER_GUARD);
    if (s_guard && s_guardOwned)
        CloseHandle(s_guard);
    s_guard = NULL;
}

static void StartGuard(DWORD pid)
{
    StopGuard();
#ifdef UNDER_CE
    s_guard = OpenProcess(0, FALSE, pid);
#else
    s_guard = OpenProcess(SYNCHRONIZE, FALSE, pid);
#endif
    s_guardOwned = s_guard != NULL;
    for (int k = 0; !s_guard && k < kMaxRunning; k++)   // no OpenProcess: the handle we started it with
        if (s_running[k].proc && s_running[k].pid == pid)
            s_guard = s_running[k].proc;
    s_guardPid = pid;
    if (s_guard)
        SetTimer(s_wnd, TIMER_GUARD, kGuardMs, NULL);
}

struct Guarded { HWND h[16]; int n; };

static BOOL CALLBACK EnumGuard(HWND h, LPARAM lp)
{
    Guarded* g = (Guarded*)lp;
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    if (pid == s_guardPid && IsWindowVisible(h) && !(GetWindowLong(h, GWL_EXSTYLE) & WS_EX_TOPMOST) && g->n < 16)
        g->h[g->n++] = h;
    return TRUE;
}

// Programs we raised stay above everything: put them back into the normal z-order.
static void DropRaised()
{
    for (int i = 0; i < s_raisedCount; i++)
        if (IsWindow(s_raised[i]))
            SetWindowPos(s_raised[i], HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    s_raisedCount = 0;
}

// Every 0.5 s while a raised program runs: its windows that are not topmost (a new dialog) are raised,
// bottom one first so that their order stays; not while VestaHome is in front (the program stepped aside).
// When it has closed, VestaHome (under it all the time) gets the focus back.
static void OnGuardTimer()
{
    if (!Alive(s_guard)) {
        StopGuard();
        DropRaised();
        SetForegroundWindow(s_wnd);
        InvalidateRect(s_wnd, NULL, FALSE);
        return;
    }
    if (GetForegroundWindow() == s_wnd)
        return;
    Guarded g;
    g.n = 0;
    EnumWindows(EnumGuard, (LPARAM)&g);   // top to bottom
    for (int k = g.n - 1; k >= 0; k--) {
        SetWindowPos(g.h[k], HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        if (s_raisedCount < kMaxRaised)
            s_raised[s_raisedCount++] = g.h[k];
    }
}

// A program's window on top of everything, guarded while its program runs.
static void RaiseWindow(HWND h)
{
    SetWindowPos(h, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    SetForegroundWindow(h);
    if (s_raisedCount < kMaxRaised)
        s_raised[s_raisedCount++] = h;
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    StartGuard(pid);
}

static void OnWaitTimer()
{
    if (s_waitApp == -1) {
        KillTimer(s_wnd, TIMER_WAIT);
        return;
    }
    FindApp f = { NULL };
    EnumWindows(EnumApp, (LPARAM)&f);
    if (f.found) {
        KillTimer(s_wnd, TIMER_WAIT);
        RaiseWindow(f.found);
        s_status[0] = 0;
        s_waitApp = -1;
    } else if (GetTickCount() - s_waitTick > kWaitMs) {
        KillTimer(s_wnd, TIMER_WAIT);
        _snwprintf(s_status, 96, L"окно не найдено: %s", s_waitApp >= 0 ? s_apps[s_waitApp].name : L"программа");
        s_waitApp = -1;
    }
}

// ---- shortcut editor

static void OpenEditor(int i)
{
    s_editIdx = i;
    if (i >= 0)
        s_edit = s_apps[i];
    else
        memset(&s_edit, 0, sizeof(s_edit));
    s_edit.icon = NULL;   // the icons stay owned by s_apps
    s_edit.img.px = NULL;
    s_delArmed = false;
    s_status[0] = 0;
    s_screen = S_EDIT;
}

// Writes the list, reads it back (icons, "no file" marks) and returns to the apps screen.
static void FinishEdit()
{
    bool ok = SaveApps();
    LoadApps();
    s_screen = S_APPS;
    if (s_page >= AppsPages())
        s_page = AppsPages() - 1;
    if (ok)
        s_status[0] = 0;
    else
        wcscpy(s_status, L"не удалось записать apps.txt");
}

static void SaveEdit()
{
    if (!s_edit.name[0] || !s_edit.path[0]) {
        wcscpy(s_status, L"нужны название и программа");
        return;
    }
    if (s_editIdx < 0) {
        s_apps[s_appCount++] = s_edit;
    } else {
        HICON icon = s_apps[s_editIdx].icon;   // freed by the reload in FinishEdit
        Img img = s_apps[s_editIdx].img;
        s_apps[s_editIdx] = s_edit;
        s_apps[s_editIdx].icon = icon;
        s_apps[s_editIdx].img = img;
    }
    FinishEdit();
}

static void DeleteEdit()
{
    FreeAppIcon(s_apps[s_editIdx]);
    memmove(&s_apps[s_editIdx], &s_apps[s_editIdx + 1], (s_appCount - s_editIdx - 1) * sizeof(App));
    s_appCount--;
    s_waitApp = -1;   // indices moved: stop waiting for a window of a started program
    s_delArmed = false;
    FinishEdit();
}

static void OpenKeysFor(wchar_t* target, int cap, const wchar_t* title, Screen back)
{
    s_keysTarget = target;
    s_keysCap = cap;
    s_keysTitle = title;
    s_keysBack = back;
    wcsncpy(s_keysText, target, 399);
    s_keysText[399] = 0;
    s_keysShift = false;
    s_clipMenu = false;
    s_keysCur = (int)wcslen(s_keysText);
    s_keysView = 0;
    s_screen = S_KEYS;
}

static void OpenKeys(int field)
{
    if (field == F_NAME)
        OpenKeysFor(s_edit.name, 47, L"Название", S_EDIT);
    else
        OpenKeysFor(s_edit.params, 199, L"Параметры запуска", S_EDIT);
}

static void KeyType(wchar_t ch)
{
    int n = (int)wcslen(s_keysText);
    if (n >= s_keysCap)
        return;
    if (s_keysShift) {   // one-shot, like a phone keyboard
        CharUpperBuff(&ch, 1);
        s_keysShift = false;
    }
    int c = s_keysCur < 0 ? 0 : s_keysCur > n ? n : s_keysCur;
    memmove(s_keysText + c + 1, s_keysText + c, (n - c + 1) * sizeof(wchar_t));
    s_keysText[c] = ch;
    s_keysCur = c + 1;
}

static void KeyBack()   // the character before the cursor
{
    int n = (int)wcslen(s_keysText);
    int c = s_keysCur < 0 ? 0 : s_keysCur > n ? n : s_keysCur;
    if (c > 0) {
        memmove(s_keysText + c - 1, s_keysText + c, (n - c + 1) * sizeof(wchar_t));
        s_keysCur = c - 1;
    }
}

// The finger at x on the field: the cursor to the nearest gap between the shown characters; at the field's ends one
// further (the view follows it: the field's drawing keeps the cursor in sight).
static void KeysCursorAt(int x)
{
    int n = (int)wcslen(s_keysText), rel = x - kKeyFieldX;
    if (rel < 8 && s_keysView > 0) {
        s_keysCur = s_keysView - 1;
        return;
    }
    if (rel > kKeyFieldW - 8 && s_keysView + s_keysVisN < n) {
        s_keysCur = s_keysView + s_keysVisN + 1;
        return;
    }
    int best = 0, bd = rel < 0 ? -rel : rel;
    for (int k = 1; k <= s_keysVisN; k++) {
        int d = rel - s_keysDx[k - 1];
        d = d < 0 ? -d : d;
        if (d < bd) {
            bd = d;
            best = k;
        }
    }
    s_keysCur = s_keysView + best;
}

// The field's text from s_keysView with the cursor in sight: s_keysVisN, s_keysDx; returns the cursor's x.
static int KeysLayout(HDC dc)
{
    int n = (int)wcslen(s_keysText);
    s_keysCur = s_keysCur < 0 ? 0 : s_keysCur > n ? n : s_keysCur;
    if (s_keysView > s_keysCur)
        s_keysView = s_keysCur;
    HGDIOBJ of = SelectObject(dc, Font(26, 700));
    for (;;) {
        SIZE sz;
        int fit = 0;
        if (!GetTextExtentExPoint(dc, s_keysText + s_keysView, n - s_keysView, kKeyFieldW, &fit, s_keysDx, &sz))
            fit = 0;
        s_keysVisN = fit < 0 ? 0 : fit;
        if (s_keysCur - s_keysView <= s_keysVisN || s_keysView >= s_keysCur)
            break;
        s_keysView++;   // the cursor beyond the field: one character on
    }
    SelectObject(dc, of);
    int k = s_keysCur - s_keysView;
    return kKeyFieldX + (k > 0 ? s_keysDx[k - 1] : 0);
}

// ---- the clipboard on the keyboard screen: a long press on the text field (TIMER_LONG) shows "Copy",
// "Paste" and "Clear" in its place. The text is copied whole; pasting goes in at the cursor, without line breaks and
// tabs, up to the field's length.

static void ClipCopy()
{
    int n = (int)wcslen(s_keysText) + 1;
    bool ok = false;
    if (OpenClipboard(s_wnd)) {
        EmptyClipboard();
#ifdef UNDER_CE
        HANDLE h = LocalAlloc(LMEM_FIXED, n * sizeof(wchar_t));   // CE: the clipboard takes local memory as is
        if (h)
            memcpy(h, s_keysText, n * sizeof(wchar_t));
#else
        HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, n * sizeof(wchar_t));
        if (h) {
            memcpy(GlobalLock(h), s_keysText, n * sizeof(wchar_t));
            GlobalUnlock(h);
        }
#endif
        ok = h && SetClipboardData(CF_UNICODETEXT, h);
        CloseClipboard();
    }
    wcscpy(s_status, ok ? L"скопировано" : L"не скопировано");
}

static void ClipPaste()
{
    int n = (int)wcslen(s_keysText), cap = s_keysCap;
    bool got = false;
    if (OpenClipboard(s_wnd)) {
        HANDLE h = GetClipboardData(CF_UNICODETEXT);
#ifdef UNDER_CE
        const wchar_t* p = (const wchar_t*)h;
#else
        const wchar_t* p = h ? (const wchar_t*)GlobalLock(h) : NULL;
#endif
        for (; p && *p && n < cap; p++)
            if (*p >= L' ') {
                KeyType(*p);   // at the cursor
                n++;
                got = true;
            }
#ifndef UNDER_CE
        if (h)
            GlobalUnlock(h);
#endif
        CloseClipboard();
    }
    wcscpy(s_status, got ? L"вставлено" : L"в буфере нет текста");
}

static void CfgKeysDone();

static void KeysDone()
{
    wcscpy(s_keysTarget, s_keysText);
    Trim(s_keysTarget);
    s_screen = s_keysBack;
    if (s_keysBack != S_EDIT)
        CfgKeysDone();
}

static int CmpFiles(const void* pa, const void* pb)
{
    const FileEnt* a = (const FileEnt*)pa;
    const FileEnt* b = (const FileEnt*)pb;
    if (a->dir != b->dir)
        return a->dir ? -1 : 1;
    return _wcsicmp(a->name, b->name);
}

static bool HasExt(const wchar_t* name, const wchar_t* ext)
{
    int n = (int)wcslen(name);
    return n > 4 && !_wcsicmp(name + n - 4, ext);
}

// Folders and .exe files of s_fdir (for an icon: .png, .ico, .exe, .dll; for the cameras: .txt), folders first.
static void ListDir()
{
    s_fileCount = 0;
    s_filePage = 0;
    wchar_t mask[MAX_PATH];
    _snwprintf(mask, MAX_PATH, L"%s*", s_fdir);
    WIN32_FIND_DATA fd;
    HANDLE h = FindFirstFile(mask, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return;
    do {
        bool dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        int n = (int)wcslen(fd.cFileName);
        if (n >= 100 || !wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L".."))
            continue;
        bool want = s_filesFor == F_TEXT ? HasExt(fd.cFileName, L".ini") || HasExt(fd.cFileName, L".txt") ||
                                           HasExt(fd.cFileName, L".cfg")
                  : s_filesFor == F_CAMS ? HasExt(fd.cFileName, L".txt") : HasExt(fd.cFileName, L".exe") ||
                    (s_filesFor == F_ICON && (HasExt(fd.cFileName, L".png") || HasExt(fd.cFileName, L".ico") ||
                                              HasExt(fd.cFileName, L".dll")));
        if (!dir && !want)
            continue;
        s_files[s_fileCount].dir = dir;
        wcscpy(s_files[s_fileCount].name, fd.cFileName);
        s_fileCount++;
    } while (s_fileCount < kMaxFiles && FindNextFile(h, &fd));
    FindClose(h);
    qsort(s_files, s_fileCount, sizeof(FileEnt), CmpFiles);
}

// Points the browser at the folder of file, if it has one that exists.
static bool BrowseFolderOf(const wchar_t* file)
{
    const wchar_t* slash = wcsrchr(file, L'\\');
    if (!slash || slash == file)
        return false;
    wchar_t dir[MAX_PATH];
    int n = (int)(slash - file);
    wcsncpy(dir, file, n);
    dir[n] = 0;
    DWORD attr = GetFileAttributes(dir);
    if (attr == 0xFFFFFFFF || !(attr & FILE_ATTRIBUTE_DIRECTORY))
        return false;
    wcscpy(s_fdir, dir);
    wcscat(s_fdir, L"\\");
    return true;
}

// For the program the browser opens in its folder, otherwise at the root. For the icon: in the current
// icon's folder, otherwise our Icons\ folder, otherwise the program's folder.
static void UserWgDir(wchar_t* dir);                               // (vh_userwidgets.inc)
static bool TeOpen(const wchar_t* path, Screen back, bool widget);   // (vh_textedit.inc)

static void OpenFiles(int field)
{
    s_filesFor = field;
    wcscpy(s_fdir, L"\\");
    wchar_t icons[MAX_PATH];
    _snwprintf(icons, MAX_PATH, L"%sIcons\\", s_dir);
    if (field == F_ICON) {
        wchar_t cur[MAX_PATH];
        if (s_edit.iconPath[0] == L'\\' || wcschr(s_edit.iconPath, L':') || !s_edit.iconPath[0])
            wcscpy(cur, s_edit.iconPath);
        else
            _snwprintf(cur, MAX_PATH, L"%s%s", s_dir, s_edit.iconPath);
        if (!(cur[0] && BrowseFolderOf(cur)) && !BrowseFolderOf(icons))
            BrowseFolderOf(s_edit.path);
    } else if (field == F_TEXT) {   // the user widgets' folder, else ours
        wchar_t uw[MAX_PATH];
        UserWgDir(uw);
        if (!BrowseFolderOf(uw))
            BrowseFolderOf(s_dir);
    } else if (field == F_CAMS) {
        wchar_t cur[MAX_PATH];
        if (s_cfg.camFile[0] == L'\\' || wcschr(s_cfg.camFile, L':'))
            wcscpy(cur, s_cfg.camFile);
        else
            _snwprintf(cur, MAX_PATH, L"%s%s", s_dir, s_cfg.camFile);
        if (!BrowseFolderOf(cur))
            BrowseFolderOf(s_dir);
    } else {
        BrowseFolderOf(s_edit.path);
    }
    ListDir();
    s_screen = S_FILES;
}

static void UpDir()
{
    int n = (int)wcslen(s_fdir);
    if (n <= 1)
        return;   // already at the root
    s_fdir[n - 1] = 0;
    wchar_t* slash = wcsrchr(s_fdir, L'\\');
    if (slash)
        slash[1] = 0;
    else
        wcscpy(s_fdir, L"\\");
    ListDir();
}

// A folder opens; an exe becomes the program, and names the shortcut if it has no name yet.
static void PickFile(int i)
{
    const FileEnt& f = s_files[i];
    if (wcslen(s_fdir) + wcslen(f.name) + 2 >= MAX_PATH)
        return;
    if (f.dir) {
        wcscat(s_fdir, f.name);
        wcscat(s_fdir, L"\\");
        ListDir();
        return;
    }
    if (s_filesFor == F_TEXT) {   // into the editor; VestaHome.ini is its settings' own (it writes it itself)
        wchar_t path[MAX_PATH];
        _snwprintf(path, MAX_PATH, L"%s%s", s_fdir, f.name);
        path[MAX_PATH - 1] = 0;
        if (!_wcsicmp(f.name, L"VestaHome.ini"))
            wcscpy(s_status, L"VestaHome.ini меняется в Настройках");
        else
            TeOpen(path, S_FILES, false);
        return;
    }
    if (s_filesFor == F_CAMS) {   // like the icon: relative inside our folder
        int n = (int)wcslen(s_dir);
        _snwprintf(s_cfg.camFile, MAX_PATH, L"%s%s", _wcsnicmp(s_fdir, s_dir, n) ? s_fdir : s_fdir + n, f.name);
        s_cfg.camFile[MAX_PATH - 1] = 0;
        SaveCfg();
        s_camLoaded = false;   // read again at the next fix
        s_screen = S_WSET_SPEED;
        return;
    }
    if (s_filesFor == F_ICON) {   // inside our folder kept relative: survives a move of the card's folders
        int n = (int)wcslen(s_dir);
        if (!_wcsnicmp(s_fdir, s_dir, n))
            _snwprintf(s_edit.iconPath, MAX_PATH, L"%s%s", s_fdir + n, f.name);
        else
            _snwprintf(s_edit.iconPath, MAX_PATH, L"%s%s", s_fdir, f.name);
        s_edit.iconPath[MAX_PATH - 1] = 0;
        s_screen = S_EDIT;
        return;
    }
    wcscpy(s_edit.path, s_fdir);
    wcscat(s_edit.path, f.name);
    if (!s_edit.name[0]) {
        wcsncpy(s_edit.name, f.name, 47);
        s_edit.name[47] = 0;
        int n = (int)wcslen(s_edit.name);
        if (n > 4 && !_wcsicmp(s_edit.name + n - 4, L".exe"))
            s_edit.name[n - 4] = 0;
    }
    s_screen = S_EDIT;
}

// ---- programs: screens

// A program's icon: its own picture or icon, otherwise its first letter in a ring.
static void AppIcon(HDC dc, const App& a, int x, int y, int size, bool d)
{
    if (a.img.px) {
        DrawImg(a.img, x, y, size, size);
    } else if (a.icon) {
        DrawIconEx(dc, x, y, a.icon, size, size, 0, NULL, DI_NORMAL);
    } else {
        wchar_t letter[2] = { a.name[0], 0 };
        Pen ring(dc, x, y, size, d ? kInk : kText);
        ring.Circle(24, 24, 20);
        T(dc, x + size / 2, y + size * 42 / 64, letter, size * 26 / 64, d ? kInk : kText, 700, AL_MID);
    }
}

// Apps, a main screen: program tiles 184 x 150, 4 x 2 per page, the last one "Add" as a dashed empty slot; in edit
// mode (the pencil in the top bar) the tiles are framed orange and open the editor. Page dots under the tiles.
static void DrawApps(HDC dc)
{
    Frame(dc, s_editMode ? L"Приложения · Изменение" : L"Приложения", A_BACK, IC_EDIT, A_EDITMODE, s_editMode,
          IC_TASKS, A_SCREEN, S_TASKS);
    int first = s_page * kAppsPerPage;
    int tiles = s_appCount + (s_appCount < kMaxApps ? 1 : 0);
    for (int k = 0; k < kAppsPerPage && first + k < tiles; k++) {
        int i = first + k, x = 16 + (k % 4) * 194, y = 64 + (k / 4) * 162;
        if (i == s_appCount) {   // "Add"
            bool d = Zone(x, y, 184, 150, A_ADD);
            if (d)
                R(dc, x, y, 184, 150, 14, kOrange);
            else
                DashRect(dc, x, y, 184, 150, 14, kLine);
            Ico(dc, IC_ADD, x + 68, y + 30, 48, d ? kInk : kDim);
            T(dc, x + 92, y + 124, L"Добавить", 15, d ? kInk : kDim, 600, AL_MID);
            continue;
        }
        const App& a = s_apps[i];
        bool d = Zone(x, y, 184, 150, A_RUN, i);
        R(dc, x, y, 184, 150, 18, d ? kOrange : kPanel, d || !s_editMode ? CLR_INVALID : kOrange);   // editing: outlined
        if (!d) {   // orange facet in the top right corner, following the rounded corner (radius 14)
            POINT f[12];
            int n = 0;
            f[n].x = x + 184 - 38; f[n].y = y + 1; n++;
            for (int j = 0; j <= 8; j++, n++) {
                double an = (-90 + j * 90 / 8.0) * 3.14159265 / 180;
                f[n].x = x + 184 - 14 + (int)(13 * cos(an) + 0.5);
                f[n].y = y + 14 + (int)(13 * sin(an) + 0.5);
            }
            f[n].x = x + 184 - 1; f[n].y = y + 38; n++;
            Poly(dc, f, n, kFacet);
        }
        AppIcon(dc, a, x + 60, y + 24, 64, d);
        T(dc, x + 92, y + 124, a.exists ? a.name : L"нет файла", 15, d ? kInk : a.exists ? kMuted : kDim,
          d ? 700 : 600, AL_MID, 168);
    }
    if (s_appCount == 0) {
        T(dc, 400, 270, L"Список программ пуст", 22, kText, 700, AL_MID);
        T(dc, 400, 302, L"Нажмите «Добавить» или впишите программы в apps.txt", 17, kMuted, 400, AL_MID, 760);
    }
    PageDots(dc, s_page, AppsPages());
}

static const wchar_t* kFieldCaption[4] = { L"Название", L"Программа", L"Параметры запуска", L"Иконка" };
static const wchar_t* kFieldEmpty[4] = { L"нажмите, чтобы ввести", L"нажмите, чтобы выбрать exe", L"нет",
    L"своя у программы; нажмите, чтобы выбрать png, ico или exe" };

// One shortcut: four fields (keyboard or file browser behind each), cancel / delete / save.
// A chosen icon has a "reset" button at the end of its field.
static void DrawEdit(HDC dc)
{
    Frame(dc, s_editIdx < 0 ? L"Новый ярлык" : L"Изменить ярлык", A_CANCEL);
    for (int k = 0; k < 4; k++) {
        int y = 72 + k * 80;
        const wchar_t* v = k == F_NAME ? s_edit.name : k == F_PATH ? s_edit.path :
                           k == F_PARAMS ? s_edit.params : s_edit.iconPath;
        bool reset = k == F_ICON && v[0];
        int w = reset ? 640 : 768;
        bool d = Zone(16, y, w, 70, A_FIELD, k);
        R(dc, 16, y, w, 70, 16, d ? kOrange : kPanel);
        T(dc, 36, y + 26, kFieldCaption[k], 13, d ? kInk : kMuted, 700);
        if (v[0])
            TTail(dc, 36, y + 55, v, 22, d ? kInk : kText, 700, w - 88);
        else
            T(dc, 36, y + 55, kFieldEmpty[k], 18, d ? kInk : kDim, 400, AL_START, w - 88);
        Ico(dc, IC_NEXT, 16 + w - 48, y + 19, 32, d ? kInk : kMuted);
        if (reset)
            Button(dc, 668, y, 116, 70, L"Сброс", B_N, IC_NONE, 17, 0, A_NOICON);
    }
    Dock(dc);
    Button(dc, 16, 410, 220, 58, L"Отмена", B_N, IC_NONE, 18, 0, A_CANCEL);
    if (s_editIdx >= 0)   // the second press deletes
        Button(dc, 290, 410, 220, 58, s_delArmed ? L"Точно удалить?" : L"Удалить", s_delArmed ? B_FILL : B_N,
               IC_NONE, 18, 0, A_DELETE);
    Button(dc, 564, 410, 220, 58, L"Сохранить", B_PRI, IC_NONE, 18, 0, A_SAVE);
}

// On-screen keyboard (like the mockup's Wi-Fi keyboard): the field and "Done" on top, 4 rows of 12 keys,
// then Shift, language, space, backspace.
static void DrawKeys(HDC dc)
{
    Frame(dc, s_keysTitle, A_CANCEL);
    if (s_clipMenu) {   // after a long press on the field, in its place; any other tap closes it
        Button(dc, 16, 68, 194, 60, L"Копировать", s_keysText[0] ? B_FILL : B_DIS, IC_NONE, 17, 0,
               s_keysText[0] ? A_CLIPCOPY : A_NONE);
        Button(dc, 219, 68, 194, 60, L"Вставить", B_FILL, IC_NONE, 17, 0, A_CLIPPASTE);
        Button(dc, 422, 68, 194, 60, L"Очистить", s_keysText[0] ? B_FILL : B_DIS, IC_NONE, 17, 0,
               s_keysText[0] ? A_CLIPCLEAR : A_NONE);
    } else {
        Zone(16, 68, 600, 70, A_KEYFIELD);   // the finger: the cursor (WndProc); held still: copy / paste
        R(dc, 16, 68, 600, 60, 16, kPanel);
        int cx = KeysLayout(dc);
        wchar_t shown[400];
        wcsncpy(shown, s_keysText + s_keysView, s_keysVisN);
        shown[s_keysVisN] = 0;
        T(dc, kKeyFieldX, 104, shown, 26, kText, 700);
        if (s_keysView > 0)   // more on the left / the right
            T(dc, 20, 104, L"‹", 22, kOrange, 700);
        if (s_keysView + s_keysVisN < (int)wcslen(s_keysText))
            T(dc, 604, 104, L"›", 22, kOrange, 700);
        R(dc, cx - 1, 76, 3, 34, 0, kOrange);   // the cursor and its handle under the line, for the finger
        Disc(dc, cx, 124, 13, kOrange);
        R(dc, cx - 1, 108, 3, 6, 0, kOrange);
    }
    Button(dc, 628, 68, 156, 60, L"Готово", B_PRI, IC_NONE, 18, 0, A_OK);
    const wchar_t** rows = s_keysEn ? kKeysEn : kKeysRu;
    for (int r = 0; r < 4; r++)
        for (int c = 0; rows[r][c]; c++) {
            int x = 13 + c * 65, y = 140 + r * 66;
            wchar_t ch[2] = { r == 0 && s_keysShift ? kKeysSym[c] : rows[r][c], 0 };
            if (s_keysShift)
                CharUpperBuff(ch, 1);
            bool d = Zone(x, y, 60, 60, A_CHAR, ch[0]);
            R(dc, x, y, 60, 60, 14, d ? kOrange : kPanel);
            T(dc, x + 30, y + 40, ch, 24, d ? kInk : kText, 700, AL_MID);
        }
    Button(dc, 13, 404, 125, 60, NULL, s_keysShift ? B_SEL : B_FILL, IC_SHIFT, 18, 32, A_SHIFT);
    Button(dc, 143, 404, 125, 60, s_keysEn ? L"ENG" : L"Рус", B_FILL, IC_NONE, 20, 0, A_LANG);
    bool d = Zone(273, 404, 385, 60, A_CHAR, L' ');
    R(dc, 273, 404, 385, 60, 14, d ? kOrange : kPanel);
    R(dc, 397, 438, 136, 4, 2, d ? kInk : kMuted);
    Button(dc, 663, 404, 125, 60, NULL, B_FILL, IC_BKSP, 18, 36, A_BKSP);
}

// Choosing the program: the folder's path, folders and .exe files 2 x 5 per page.
static void DrawFiles(HDC dc)
{
    Frame(dc, s_filesFor == F_ICON ? L"Выбор иконки" : s_filesFor == F_CAMS ? L"Файл камер"
              : s_filesFor == F_TEXT ? L"Текстовые файлы (.ini, .txt, .cfg)" : L"Выбор программы",
          A_CANCEL);
    TTail(dc, 20, 84, s_fdir, 15, kMuted, 400, 760);
    int first = s_filePage * kFilesPerPage;
    for (int k = 0; k < kFilesPerPage && first + k < s_fileCount; k++) {
        const FileEnt& f = s_files[first + k];   // folders stand out
        ListRow(dc, 16 + (k / 5) * 388, 96 + (k % 5) * 56, 380, 50, f.name, NULL, L_N, f.dir ? IC_FOLDER : IC_FILE, NULL,
                NULL, A_FILE, first + k, f.dir ? kOrange : CLR_INVALID);
    }
    if (s_fileCount == 0)
        T(dc, 400, 240, s_filesFor == F_ICON ? L"Здесь нет папок и иконок" : s_filesFor == F_TEXT ?
          L"Здесь нет папок и текстовых файлов" : s_filesFor == F_CAMS ?
          L"Здесь нет папок и .txt" : L"Здесь нет папок и программ", 22, kText, 700, AL_MID);
    PageDots(dc, s_filePage, FilePages(), 400, 382);
    Dock(dc);
    Button(dc, 16, 410, 120, 58, NULL, B_N, IC_UP, 18, 0, A_UP);
    Button(dc, 664, 410, 120, 58, L"Отмена", B_N, IC_NONE, 18, 0, A_CANCEL);
}
