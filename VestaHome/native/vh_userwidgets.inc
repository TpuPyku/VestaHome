// vh_userwidgets.inc - part of home.cpp (included there, in order): the user's widgets and their HTTPS.

// ---- the user's widgets: VestaWidgets\*.ini on the SD card (the PC: next to the exe), one a widget:
// (and the HTTPS that VestaHome lends them: url= every= save=, value.from= ... - see UserWgFetch)
//     title=FUEL  icon=car  wide=0  data=fuel.txt  refresh=10  start=fuel.mscr  tap=fuel.mscr
// data (default <name>.txt beside the ini) is written by a script (MortScript); VestaHome reads it every refresh
// seconds when its time changed:  value=7,4  unit=л/100 км  text=...  text2=...  state=ok|warn|alert
// start= runs once when VestaHome starts (a script that keeps the data fresh), tap= on a tap; .mscr through
// MortScript.exe, .exe as it is; a relative path is from the ini's folder. UTF-8 or cp1251 (MortScript writes
// ANSI). A broken file can't harm VestaHome: unknown lines are skipped, no file - "no data".
static const struct { const wchar_t* name; Icon ic; } kIconNames[] = {
    { L"music", IC_MUSIC }, { L"radio", IC_RADIO }, { L"phone", IC_PHONE }, { L"gear", IC_GEAR },
    { L"home", IC_HOME }, { L"car", IC_CAR }, { L"cloud", IC_CLOUD }, { L"nav", IC_NAV }, { L"map", IC_MAP },
    { L"sd", IC_SD }, { L"usb", IC_USB }, { L"wifi", IC_WIFI }, { L"bt", IC_BT }, { L"info", IC_INFO },
    { L"clock", IC_CLOCK }, { L"photo", IC_PHOTO }, { L"video", IC_VIDEO }, { L"apps", IC_APPS },
    { L"warn", IC_WARN }, { L"eye", IC_EYE }, { L"power", IC_POWER }, { L"folder", IC_FOLDER },
    { L"tasks", IC_TASKS }, { L"star", IC_STAR }, { L"lock", IC_LOCK }, { L"file", IC_FILE } };

// The widgets' folders (2026-10-22, the user: widgets without the card too): the SD card's, then the built-in
// card's (a name in both - the SD card's). UserWgDir: the first there is (the text editor's start).
static int UserWgDirs(wchar_t (*dirs)[MAX_PATH])
{
#ifdef UNDER_CE
    wcscpy(dirs[0], L"\\SDMemory\\VestaWidgets\\");
    wcscpy(dirs[1], L"\\SDMMC\\VestaWidgets\\");
    return 2;
#else
    _snwprintf(dirs[0], MAX_PATH, L"%sVestaWidgets\\", s_dir);
    return 1;
#endif
}

static void UserWgDir(wchar_t* dir)
{
    wchar_t d[2][MAX_PATH];
    int n = UserWgDirs(d);
    wcscpy(dir, d[0]);
    for (int i = 0; i < n; i++)
        if (GetFileAttributes(d[i]) != 0xFFFFFFFF) {
            wcscpy(dir, d[i]);
            return;
        }
}

// What VestaHome writes for a widget (a url's answer, the fields taken out of it) when the ini names no place: in
// RAM, \Temp\VestaWidgets\ (no wear of either card; gone at power-off - fetched again). The PC: beside the exe.
static void UserWgTemp(const wchar_t* name, wchar_t* out)
{
#ifdef UNDER_CE
    CreateDirectory(L"\\Temp", NULL);
    CreateDirectory(L"\\Temp\\VestaWidgets", NULL);
    _snwprintf(out, MAX_PATH, L"\\Temp\\VestaWidgets\\%s", name);
#else
    wchar_t d[MAX_PATH];
    _snwprintf(d, MAX_PATH, L"%sTemp", s_dir);
    CreateDirectory(d, NULL);
    _snwprintf(d, MAX_PATH, L"%sTemp\\VestaWidgets", s_dir);
    CreateDirectory(d, NULL);
    _snwprintf(out, MAX_PATH, L"%sTemp\\VestaWidgets\\%s", s_dir, name);
#endif
    out[MAX_PATH - 1] = 0;
}

static bool IsUtf8(const unsigned char* p, int n)
{
    for (int i = 0; i < n;) {
        int k = p[i] < 0x80 ? 0 : (p[i] & 0xE0) == 0xC0 ? 1 : (p[i] & 0xF0) == 0xE0 ? 2 : (p[i] & 0xF8) == 0xF0 ? 3 : -1;
        if (k < 0 || i + k >= n + (k ? 0 : 1))
            return false;
        for (int j = 1; j <= k; j++)
            if ((p[i + j] & 0xC0) != 0x80)
                return false;
        i += k + 1;
    }
    return true;
}

// A key=value text file (UTF-8 with or without BOM, else cp1251) into wide text; NULL when missing. free() it.
static wchar_t* ReadText(const wchar_t* path)
{
    DWORD size;
    char* buf = ReadAll(path, &size);
    if (!buf)
        return NULL;
    const unsigned char* b = (const unsigned char*)buf;
    int n = (int)size, skip = 0;
    if (n >= 3 && b[0] == 0xEF && b[1] == 0xBB && b[2] == 0xBF)
        skip = 3;
    UINT cp = IsUtf8(b + skip, n - skip) ? CP_UTF8 : 1251;
    wchar_t* w = (wchar_t*)malloc((n + 1) * sizeof(wchar_t));
    if (w) {
        int k = MultiByteToWideChar(cp, 0, buf + skip, n - skip, w, n);
        if (k <= 0 && cp == 1251)
            k = MultiByteToWideChar(CP_ACP, 0, buf + skip, n - skip, w, n);
        w[k > 0 ? k : 0] = 0;
    }
    free(buf);
    return w;
}

// The next "key=value" line of text (comments '#' / ';' and blank lines skipped); false at the end.
static bool NextKeyVal(wchar_t** pos, wchar_t** k, wchar_t** v)
{
    while (**pos) {
        wchar_t* line = *pos;
        wchar_t* end = line;
        while (*end && *end != L'\n' && *end != L'\r')
            end++;
        *pos = *end ? end + 1 : end;
        *end = 0;
        Trim(line);
        if (!line[0] || line[0] == L'#' || line[0] == L';')
            continue;
        wchar_t* eq = wcschr(line, L'=');
        if (!eq)
            continue;
        *eq = 0;
        *k = line;
        *v = eq + 1;
        Trim(*k);
        Trim(*v);
        return true;
    }
    return false;
}

// A path of the widget's ini: as it is when absolute, else from the ini's folder.
static void UserWgPath(const UserWg& w, const wchar_t* v, wchar_t* out)
{
    if (v[0] == L'\\' || (v[0] && v[1] == L':'))
        wcsncpy(out, v, MAX_PATH - 1);
    else
        _snwprintf(out, MAX_PATH, L"%s%s", w.dir, v);
    out[MAX_PATH - 1] = 0;
}

static bool UserWgLoad(const wchar_t* dir, const wchar_t* file, UserWg* w)
{
    wchar_t path[MAX_PATH];
    _snwprintf(path, MAX_PATH, L"%s%s", dir, file);
    wchar_t* text = ReadText(path);
    if (!text)
        return false;
    memset(w, 0, sizeof(*w));
    wcsncpy(w->dir, dir, MAX_PATH - 1);
    wcsncpy(w->file, file, MAX_PATH - 1);
    wchar_t name[40];
    wcsncpy(name, file, 39);
    name[39] = 0;
    wchar_t* dot = wcsrchr(name, L'.');
    if (dot)
        *dot = 0;
    _snwprintf(w->key, 40, L"u.%s", name);
    for (wchar_t* c = w->key; *c; c++)   // the key goes into a comma list
        if (*c == L',' || *c == L' ')
            *c = L'_';
    wcsncpy(w->title, name, 39);
    w->ic = IC_INFO;
    w->refresh = 10;
    wchar_t dataName[MAX_PATH];
    _snwprintf(dataName, MAX_PATH, L"%s.txt", name);
    UserWgPath(*w, dataName, w->data);
    bool dataSet = false;   // data= given (else for a url widget its data goes to RAM)
    wchar_t* pos = text;
    wchar_t *k, *v;
    while (NextKeyVal(&pos, &k, &v)) {
        if (!_wcsicmp(k, L"title")) {
            wcsncpy(w->title, v, 39);
            w->title[39] = 0;
        } else if (!_wcsicmp(k, L"icon")) {
            for (int i = 0; i < (int)(sizeof(kIconNames) / sizeof(kIconNames[0])); i++)
                if (!_wcsicmp(v, kIconNames[i].name))
                    w->ic = kIconNames[i].ic;
        } else if (!_wcsicmp(k, L"wide")) {
            w->wide = CfgBool(v);
        } else if (!_wcsicmp(k, L"data")) {
            UserWgPath(*w, v, w->data);
            dataSet = true;
        } else if (!_wcsicmp(k, L"refresh")) {
            w->refresh = _wtoi(v) < 1 ? 1 : _wtoi(v) > 3600 ? 3600 : _wtoi(v);
        } else if (!_wcsicmp(k, L"tap")) {
            if (v[0])
                UserWgPath(*w, v, w->tap);
        } else if (!_wcsicmp(k, L"start")) {
            if (v[0])
                UserWgPath(*w, v, w->start);
        } else if (!_wcsicmp(k, L"url")) {
            WideCharToMultiByte(CP_UTF8, 0, v, -1, w->url, sizeof(w->url), NULL, NULL);
        } else if (!_wcsicmp(k, L"every")) {
            w->every = _wtoi(v) < 60 ? 60 : _wtoi(v);   // not more often than once a minute
        } else if (!_wcsicmp(k, L"save")) {
            if (v[0])
                UserWgPath(*w, v, w->resp);
        } else {
            static const wchar_t* fields[5] = { L"value", L"unit", L"text", L"text2", L"state" };
            for (int i = 0; i < 5; i++) {
                size_t n = wcslen(fields[i]);
                if (!_wcsnicmp(k, fields[i], n) && !_wcsicmp(k + n, L".from")) {
                    wcsncpy(w->from[i], v, 63);
                    w->from[i][63] = 0;
                } else if (!_wcsicmp(k, fields[i])) {
                    wcsncpy(w->fixed[i], v, 95);
                    w->fixed[i][95] = 0;
                }
            }
        }
    }
    if (w->url[0]) {
        if (!w->every)
            w->every = 600;
        if (!w->resp[0]) {   // VestaHome's writes: in RAM unless the ini names a place
            wchar_t respName[MAX_PATH];
            _snwprintf(respName, MAX_PATH, L"%s.resp", name);
            UserWgTemp(respName, w->resp);
        }
        if (!dataSet)
            UserWgTemp(dataName, w->data);
    }
    free(text);
    return true;
}

// The data file, when its write time changed; true: something to draw anew.
static bool UserWgRead(UserWg& w)
{
    WIN32_FIND_DATA fd;
    HANDLE f = FindFirstFile(w.data, &fd);
    if (f == INVALID_HANDLE_VALUE) {
        bool was = w.have;
        w.have = false;
        return was;
    }
    FindClose(f);
    if (w.have && !CompareFileTime(&fd.ftLastWriteTime, &w.dataTime))
        return false;
    wchar_t* text = ReadText(w.data);
    if (!text)
        return false;
    w.dataTime = fd.ftLastWriteTime;
    w.have = true;
    w.value[0] = w.unit[0] = w.text[0][0] = w.text[1][0] = 0;
    w.state = 0;
    wchar_t* pos = text;
    wchar_t *k, *v;
    while (NextKeyVal(&pos, &k, &v)) {
        if (!_wcsicmp(k, L"value"))
            wcsncpy(w.value, v, 23);
        else if (!_wcsicmp(k, L"unit"))
            wcsncpy(w.unit, v, 23);
        else if (!_wcsicmp(k, L"text"))
            wcsncpy(w.text[0], v, 95);
        else if (!_wcsicmp(k, L"text2"))
            wcsncpy(w.text[1], v, 95);
        else if (!_wcsicmp(k, L"state"))
            w.state = !_wcsicmp(v, L"ok") ? 1 : !_wcsicmp(v, L"warn") ? 2 : !_wcsicmp(v, L"alert") ? 3 : 0;
    }
    w.value[23] = w.unit[23] = w.text[0][95] = w.text[1][95] = 0;
    free(text);
    return true;
}

// Runs a widget's command: a .mscr through MortScript.exe (looked for beside the widget, on the card, next to
// VestaHome, in the usual folders), anything else as a program.
static void UserWgRun(const UserWg& w, const wchar_t* cmd, const wchar_t* why)
{
    wchar_t exe[MAX_PATH], args[MAX_PATH + 4];
    const wchar_t* dot = wcsrchr(cmd, L'.');
    if (dot && !_wcsicmp(dot, L".mscr")) {
        wchar_t cand[7][MAX_PATH];
        _snwprintf(cand[0], MAX_PATH, L"%sMortScript.exe", w.dir);
        wcscpy(cand[1], L"\\SDMemory\\MortScript\\MortScript.exe");
        wcscpy(cand[2], L"\\SDMemory\\MortScript.exe");
        _snwprintf(cand[3], MAX_PATH, L"%sMortScript.exe", s_dir);
        wcscpy(cand[4], L"\\SDMMC\\MortScript\\MortScript.exe");
        wcscpy(cand[5], L"\\StorageFlash\\MortScript\\MortScript.exe");
        wcscpy(cand[6], L"\\ResidentFlash\\MortScript\\MortScript.exe");
        exe[0] = 0;
        for (int i = 0; i < 7 && !exe[0]; i++)
            if (GetFileAttributes(cand[i]) != 0xFFFFFFFF)
                wcscpy(exe, cand[i]);
        if (!exe[0]) {
            VLog(L"виджет %s: %s - нет MortScript.exe (рядом с виджетом, \\SDMemory\\MortScript\\)", w.key, why);
            return;
        }
        _snwprintf(args, MAX_PATH + 4, L"\"%s\"", cmd);
    } else {
        wcsncpy(exe, cmd, MAX_PATH - 1);
        exe[MAX_PATH - 1] = 0;
        args[0] = 0;
        // A program: as from "Приложения" (VestaHome waits for its window and brings it up, otherwise VestaHome
        // stayed over it - HU 2026-10-20). Its path wrong (an example's ini): the one of that file name there.
        const wchar_t* fn = wcsrchr(exe, L'\\');
        fn = fn ? fn + 1 : exe;
        bool there = GetFileAttributes(exe) != 0xFFFFFFFF;
        int app = -1;
        for (int i = 0; i < s_appCount && app < 0; i++) {
            const wchar_t* an = wcsrchr(s_apps[i].path, L'\\');
            an = an ? an + 1 : s_apps[i].path;
            if (!_wcsicmp(s_apps[i].path, exe) || (!there && !_wcsicmp(an, fn)))
                app = i;
        }
        if (app >= 0) {
            VLog(L"виджет %s: %s - %s (из «Приложений»)", w.key, why, s_apps[app].path);
            RunApp(app);
            return;
        }
        if (!there) {
            VLog(L"виджет %s: %s - нет %s (и в «Приложениях» нет %s)", w.key, why, exe, fn);
            _snwprintf(s_status, 96, L"нет %s: добавьте в «Приложения»", fn);
            return;
        }
    }
    PROCESS_INFORMATION pi;
#ifdef UNDER_CE
    BOOL ok = CreateProcess(exe, args[0] ? args : NULL, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi);
#else
    STARTUPINFO si;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    wchar_t line[2 * MAX_PATH + 8];
    _snwprintf(line, 2 * MAX_PATH + 8, L"\"%s\" %s", exe, args);
    BOOL ok = CreateProcess(exe, line, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
#endif
    VLog(L"виджет %s: %s - %s %s", w.key, why, cmd, ok ? L"запущен" : L"не запустился");
    if (ok) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        if (!args[0]) {   // a program (not a script): its window brought up when it comes
            s_waitApp = -2;
            s_waitPid = pi.dwProcessId;
            s_waitTick = GetTickCount();
            SetTimer(s_wnd, TIMER_WAIT, 300, NULL);
        }
    }
}

// ---- HTTPS for the widgets: a widget with url= is fetched by VestaHome every every= seconds (one at a time, in a
// thread of its own; at most 256 KB, 60 s at least between two of the same widget). The answer goes to resp
// (for a script), and when the widget asks for fields (value.from= "start|end" ...) VestaHome takes them out and
// writes the data file itself - no script needed. A failure keeps the last data and goes to the log.
#define WM_APP_UWG (WM_APP + 26)
enum { kUwgMaxBody = 256 * 1024 };
static CRITICAL_SECTION s_uwCs;   // s_uw between the scan (the window) and the fetch thread
static HANDLE s_uwThread;

static void WriteUtf8(const wchar_t* path, const wchar_t* text)
{
    int n = WideCharToMultiByte(CP_UTF8, 0, text, -1, NULL, 0, NULL, NULL);
    char* b = (char*)malloc(n > 0 ? n : 1);
    if (!b)
        return;
    WideCharToMultiByte(CP_UTF8, 0, text, -1, b, n, NULL, NULL);
    HANDLE f = CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (f != INVALID_HANDLE_VALUE) {
        DWORD w;
        WriteFile(f, b, n > 0 ? n - 1 : 0, &w, NULL);
        CloseHandle(f);
    }
    free(b);
}

// The text between the markers "start|end" of rule in body (the first match), entities &amp; &lt; &gt; &quot;
// &apos; decoded, trimmed; false: not found.
static bool TakeBetween(const wchar_t* body, const wchar_t* rule, wchar_t* out, int cap)
{
    const wchar_t* bar = wcschr(rule, L'|');
    if (!bar)
        return false;
    wchar_t a[64], b[64];
    int la = (int)(bar - rule);
    wcsncpy(a, rule, la);
    a[la] = 0;
    wcsncpy(b, bar + 1, 63);
    b[63] = 0;
    const wchar_t* p = a[0] ? wcsstr(body, a) : body;
    if (!p)
        return false;
    p += la;
    const wchar_t* e = b[0] ? wcsstr(p, b) : p + wcslen(p);
    if (!e)
        return false;
    int n = 0;
    for (const wchar_t* q = p; q < e && n < cap - 1; q++) {
        static const struct { const wchar_t* ent; wchar_t c; } ents[5] = {
            { L"&amp;", L'&' }, { L"&lt;", L'<' }, { L"&gt;", L'>' }, { L"&quot;", L'"' }, { L"&apos;", L'\'' } };
        bool done = false;
        for (int i = 0; i < 5 && !done; i++)
            if (!wcsncmp(q, ents[i].ent, wcslen(ents[i].ent))) {
                out[n++] = ents[i].c;
                q += wcslen(ents[i].ent) - 1;
                done = true;
            }
        if (!done)
            out[n++] = *q == L'\r' || *q == L'\n' || *q == L'\t' ? L' ' : *q;
    }
    out[n] = 0;
    Trim(out);
    return true;
}

struct UwgJob { wchar_t key[40], resp[MAX_PATH], data[MAX_PATH], from[5][64], fixed[5][96]; char url[512]; };

static void UserWgFetchOne(const UwgJob& j)
{
    const char* u = j.url;
    bool tls = !_strnicmp(u, "https://", 8);
    if (!tls && _strnicmp(u, "http://", 7)) {
        VLog(L"виджет %s: url не http:// и не https://", j.key);
        return;
    }
    u += tls ? 8 : 7;
    char host[128];
    const char* slash = strchr(u, '/');
    int hl = slash ? (int)(slash - u) : (int)strlen(u);
    if (hl <= 0 || hl >= (int)sizeof(host))
        return;
    memcpy(host, u, hl);
    host[hl] = 0;
    const char* path = slash ? slash : "/";
    DWORD t0 = GetTickCount();
    unsigned char* body = NULL;
    int len = 0, status = 0;
    int r = HttpGet(host, tls, path, "User-Agent: Mozilla/5.0 (VestaHome)\r\n", &body, &len, &status);
    if (r != NET_OK || status != 200 || !body) {
        VLog(L"виджет %s: загрузка %S - %s", j.key, host, r != NET_OK ? NetErrorText(r) : L"ответ не 200");
        if (r == NET_OK)
            VLog(L"виджет %s: HTTP %d", j.key, status);
        free(body);
        return;
    }
    if (len > kUwgMaxBody) {
        VLog(L"виджет %s: ответ %d байт - больше 256 КБ, отброшен", j.key, len);
        free(body);
        return;
    }
    HANDLE f = CreateFile(j.resp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);   // the raw answer for a script
    if (f != INVALID_HANDLE_VALUE) {
        DWORD w;
        WriteFile(f, body, len, &w, NULL);
        CloseHandle(f);
    }
    bool rules = false;
    for (int i = 0; i < 5; i++)
        rules = rules || j.from[i][0];
    if (rules) {   // the fields taken out: the data file
        UINT cp = IsUtf8(body, len) ? CP_UTF8 : 1251;
        wchar_t* w = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
        if (w) {
            int k = MultiByteToWideChar(cp, 0, (const char*)body, len, w, len);
            w[k > 0 ? k : 0] = 0;
            static const wchar_t* keys[5] = { L"value", L"unit", L"text", L"text2", L"state" };
            wchar_t data[1024] = L"# written by VestaHome from the widget's url\r\n", got[96], line[256];
            for (int i = 0; i < 5; i++) {
                bool have = j.from[i][0] && TakeBetween(w, j.from[i], got, 96);
                if (i == 4 && have)   // Yandex's colours
                    wcscpy(got, !_wcsicmp(got, L"green") ? L"ok" : !_wcsicmp(got, L"yellow") ? L"warn"
                                : !_wcsicmp(got, L"red") ? L"alert" : got);
                if (j.fixed[i][0] && wcsstr(j.fixed[i], L"%s"))
                    _snwprintf(line, 256, j.fixed[i], have ? got : L"—");
                else if (have)
                    wcscpy(line, got);
                else
                    wcsncpy(line, j.fixed[i], 255);
                line[255] = 0;
                if (line[0] && wcslen(data) + wcslen(keys[i]) + wcslen(line) + 4 < 1024) {
                    wcscat(data, keys[i]);
                    wcscat(data, L"=");
                    wcscat(data, line);
                    wcscat(data, L"\r\n");
                }
            }
            WriteUtf8(j.data, data);
            free(w);
        }
    }
    VLog(L"виджет %s: загрузка %S - %d байт за %u мс", j.key, host, len, (unsigned)(GetTickCount() - t0));
    free(body);
}

static bool WgShown(int id);   // (below)

// (2026-10-22, the user: a hidden widget must not work) A hidden user widget fetches nothing, starts no script and
// reads no data; shown again (A_WGVIS: UserWgWake) it starts, reads and fetches at once.
static DWORD WINAPI UserWgThread(LPVOID)
{
    for (;;) {
        Sleep(1000);
        UwgJob j;
        bool due = false;
        EnterCriticalSection(&s_uwCs);
        for (int i = 0; i < s_uwCount && !due; i++) {
            UserWg& w = s_uw[i];
            if (!w.url[0] || !WgShown(WG_COUNT + i) ||
                (w.fetchTick && GetTickCount() - w.fetchTick < (DWORD)w.every * 1000))
                continue;
            w.fetchTick = GetTickCount();
            if (!w.fetchTick)
                w.fetchTick = 1;
            wcscpy(j.key, w.key);
            wcscpy(j.resp, w.resp);
            wcscpy(j.data, w.data);
            memcpy(j.from, w.from, sizeof(j.from));
            memcpy(j.fixed, w.fixed, sizeof(j.fixed));
            strcpy(j.url, w.url);
            due = true;
        }
        LeaveCriticalSection(&s_uwCs);
        if (due) {
            UserWgFetchOne(j);
            PostMessage(s_wnd, WM_APP_UWG, 0, 0);
        }
    }
    return 0;
}

static int CmpUserWg(const void* a, const void* b) { return wcscmp(((const UserWg*)a)->key, ((const UserWg*)b)->key); }

// The widgets' folder read (at the start and when a card comes or goes); a widget seen the first time since
// VestaHome started runs its start= script. The main screen's order is applied again with them.
static wchar_t s_uwStarted[32][40];   // the widgets whose start= ran since VestaHome started
static int     s_uwStartedCount;

static void UserWgStart(int u)   // its start= once a run, when it is shown
{
    UserWg& w = s_uw[u];
    if (!w.start[0] || !WgShown(WG_COUNT + u))
        return;
    for (int k = 0; k < s_uwStartedCount; k++)
        if (!wcscmp(s_uwStarted[k], w.key))
            return;
    UserWgRun(w, w.start, L"start");
    if (s_uwStartedCount < 32)
        wcscpy(s_uwStarted[s_uwStartedCount++], w.key);
}

static void UserWgWake(int u)   // shown again: its script, its data, its url now
{
    if (u < 0 || u >= s_uwCount)
        return;
    UserWgStart(u);
    UserWgRead(s_uw[u]);
    EnterCriticalSection(&s_uwCs);
    s_uw[u].fetchTick = 0;
    LeaveCriticalSection(&s_uwCs);
}

static void UserWidgetsScan()
{
    wchar_t dirs[2][MAX_PATH], pat[MAX_PATH];
    int nd = UserWgDirs(dirs);
    if (!s_uwThread)
        InitializeCriticalSection(&s_uwCs);
    EnterCriticalSection(&s_uwCs);
    s_uwCount = 0;
    for (int d = 0; d < nd && s_uwCount < kWgUser; d++) {   // the SD card's first: a name in both - its
        _snwprintf(pat, MAX_PATH, L"%s*.ini", dirs[d]);
        WIN32_FIND_DATA fd;
        HANDLE f = FindFirstFile(pat, &fd);
        if (f == INVALID_HANDLE_VALUE)
            continue;
        do {
            if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || !UserWgLoad(dirs[d], fd.cFileName, &s_uw[s_uwCount]))
                continue;
            bool dup = false;
            for (int i = 0; i < s_uwCount && !dup; i++)
                dup = !wcscmp(s_uw[i].key, s_uw[s_uwCount].key);
            if (!dup)
                s_uwCount++;
        } while (s_uwCount < kWgUser && FindNextFile(f, &fd));
        FindClose(f);
    }
    qsort(s_uw, s_uwCount, sizeof(UserWg), CmpUserWg);
    bool net = false;
    for (int i = 0; i < s_uwCount; i++)
        net = net || s_uw[i].url[0];
    LeaveCriticalSection(&s_uwCs);
    if (net && !s_uwThread)
        s_uwThread = CreateThread(NULL, 0, UserWgThread, NULL, 0, NULL);
    if (s_uwCount)
        VLog(L"виджеты пользователя: %d (%s, %s)", s_uwCount, dirs[0], nd > 1 ? dirs[1] : L"-");
    HomeWidgetsLoad(s_cfg.homeRaw);   // the hidden ones known from here
    for (int i = 0; i < s_uwCount; i++)
        if (WgShown(WG_COUNT + i)) {
            UserWgRead(s_uw[i]);
            UserWgStart(i);
        }
}

static void WUser(HDC dc, int x, int y, int u)
{
    const UserWg& w = s_uw[u];
    int ww = w.wide ? kWgW2 : kWgW;
    bool d = Widget(dc, x, y, ww, kWgH, w.title, w.ic, w.tap[0] ? A_USERWG : A_NONE, u);
    COLORREF fg = d ? kInk : kText, mg = d ? kInk : kMuted;
    if (w.state) {
        static const COLORREF dot[4] = { 0, kOk, kOrange, kStop };
        Disc(dc, x + ww - 24, y + 21, 7, d ? kInk : dot[w.state]);
    }
    if (!w.have) {
        T(dc, x + 16, y + 84, L"Нет данных", 22, fg, 700, AL_START, ww - 32);
        const wchar_t* fn = wcsrchr(w.data, L'\\');
        wchar_t s[MAX_PATH + 32];
        _snwprintf(s, MAX_PATH + 32, L"скрипт ещё не записал %s", fn ? fn + 1 : w.data);
        TWrap(dc, x + 16, y + 108, s, 14, mg, 400, ww - 32, 3, 18);
        return;
    }
    if (w.value[0]) {
        int end = T(dc, x + 16, y + 96, w.value, 44, fg, 700, AL_START, ww - 32);
        if (w.unit[0] && end + 8 < x + ww - 16)
            T(dc, end + 8, y + 96, w.unit, 18, mg, 600, AL_START, x + ww - 16 - end - 8);
        TWrap(dc, x + 16, y + 124, w.text[0], 15, mg, 400, ww - 32, 1, 0);
        TWrap(dc, x + 16, y + 146, w.text[1], 14, d ? kInk : kDim, 400, ww - 32, 1, 0);
    } else {
        int n = TWrap(dc, x + 16, y + 80, w.text[0], 19, fg, 700, ww - 32, 2, 24);
        TWrap(dc, x + 16, y + 80 + n * 24 + 8, w.text[1], 15, mg, 400, ww - 32, 2, 19);
    }
}

// cells: 1 a single, 2 a double (1 x 2), 4 a big one (2 x 2, design 0.6).
struct WidgetDef { const wchar_t* name; int cells; Icon ic; void (*draw)(HDC dc, int x, int y); };
static const WidgetDef kWidgets[WG_COUNT] = {
    { L"Сейчас играет", 4, IC_MUSIC, WMedia }, { L"Скорость", 1, IC_NAV, WSpeed },
    { L"Радио", 1, IC_RADIO, WRadio }, { L"Погода", 1, IC_CLOUD, WWeather },
    { L"Приложения", 1, IC_APPS, WFavorites }, { L"Поездка", 4, IC_CLOCK, WTrip },
    { L"Климат", 1, IC_CAR, WClimate }, { L"Календарь", 1, IC_CLOCK, WDate },
    { L"Телефон", 1, IC_PHONE, WPhone }, { L"Автомобиль", 1, IC_CAR, WCar },
    { L"Навигация", 1, IC_NAV, WNavi }, { L"Сейчас играет · компактный", 2, IC_MUSIC, WMediaSmall } };

// A widget by its id (built-in or the user's): the only way to it for the layout, the settings and the screen.
static const wchar_t* WgName(int id) { return id < WG_COUNT ? kWidgets[id].name : s_uw[id - WG_COUNT].title; }
static bool WgWide(int id) { return id < WG_COUNT ? kWidgets[id].cells >= 2 : s_uw[id - WG_COUNT].wide; }
static bool WgBig(int id) { return id < WG_COUNT && kWidgets[id].cells == 4; }
static Icon WgIcon(int id) { return id < WG_COUNT ? kWidgets[id].ic : s_uw[id - WG_COUNT].ic; }
static void WgDraw(int id, HDC dc, int x, int y)
{
    if (id < WG_COUNT)
        kWidgets[id].draw(dc, x, y);
    else
        WUser(dc, x, y, id - WG_COUNT);
}

// Shown: not hidden (the climate too: a car without it hides it here, "General -> Widgets of the main screen";
// the user 2026-10-21 - it no longer stands in for the calendar).
static bool WgShown(int id);

static bool WeatherWanted()   // its widget shown, or the screen saver on (it shows the weather)
{
    return WgShown(WG_WEATHER) || s_cfg.saverMin > 0;
}

static bool WgShown(int id)
{
    return !(s_cfg.homeHidden >> id & 1) && id != WG_CLIMATE;   // the climate: off for now (no data from the car)
}

// Where each shown widget goes: in their order, each into the first free place (a double needs two free cells
// side by side in a row, a big one the page's left 2 x 2), so the later ones close up the gaps. page < 0: not
// placed. Returns the pages.
struct WgPlace { int page, x, y; };
static int HomeLayout(WgPlace* at)
{
    bool used[kWgPages][6];
    memset(used, 0, sizeof(used));
    int pages = 1;
    for (int i = 0; i < WgCount(); i++) {
        int id = s_cfg.homeOrder[i];
        at[id].page = -1;
        if (!WgShown(id))
            continue;
        bool wide = WgWide(id), big = WgBig(id);
        for (int k = 0; k < kWgPages * 6 && at[id].page < 0; k++) {
            int pg = k / 6, c = k % 6;
            if (used[pg][c] || (wide && (c % 3 == 2 || used[pg][c + 1])) ||
                (big && (c || used[pg][3] || used[pg][4])))
                continue;
            used[pg][c] = true;
            if (wide)
                used[pg][c + 1] = true;
            if (big)
                used[pg][3] = used[pg][4] = true;
            at[id].page = pg;
            at[id].x = 16 + c % 3 * (kWgW + kWgGap);
            at[id].y = 64 + c / 3 * (kWgH + kWgGap);
            if (pg + 1 > pages)
                pages = pg + 1;
        }
    }
    return pages;
}

static int HomePages()
{
    WgPlace at[kWgMax];
    return HomeLayout(at);
}

// A widget's rectangle when it is on the page shown (for its own redraws).
static bool HomeRect(int id, RECT* rc)
{
    WgPlace at[kWgMax];
    HomeLayout(at);
    if (at[id].page != s_homePage)
        return false;
    rc->left = at[id].x;
    rc->top = at[id].y;
    rc->right = at[id].x + (WgWide(id) ? kWgW2 : kWgW);
    rc->bottom = at[id].y + (WgBig(id) ? kWgH2 : kWgH);
    return true;
}

// The "now playing" widgets on the page shown (the big one, the compact one, both): their area.
static bool HomeMediaRect(RECT* rc)
{
    RECT a, b;
    bool ha = HomeRect(WG_MEDIA, &a), hb = HomeRect(WG_MEDIA2, &b);
    if (ha && hb)
        UnionRect(rc, &a, &b);
    else if (ha || hb)
        *rc = ha ? a : b;
    return ha || hb;
}

// The main screen: the date as the title, the widgets of the page, the page dots.
static void DrawHome(HDC dc)
{
    SYSTEMTIME t;
    GetLocalTime(&t);
    wchar_t s[48];
    _snwprintf(s, 48, L"%s, %d %s", kDays[t.wDayOfWeek % 7], t.wDay, kMonths[(t.wMonth + 11) % 12]);
    CharUpperBuff(s, 1);
    Frame(dc, s);
    WgPlace at[kWgMax];
    int pages = HomeLayout(at);
    if (s_homePage >= pages)
        s_homePage = pages - 1;
    for (int id = 0; id < WgCount(); id++)
        if (at[id].page == s_homePage)
            WgDraw(id, dc, at[id].x, at[id].y);
    bool any = false;
    for (int id = 0; id < WgCount(); id++)
        any = any || at[id].page >= 0;
    if (!any)
        T(dc, 400, 230, L"Все виджеты скрыты: «Настройки → Общие → Виджеты главной»", 16, kMuted, 400, AL_MID, 740);
    PageDots(dc, s_homePage, pages);
}

// Settings -> General -> the main screen's widgets: in their order (down the left column, then the right, 12 a
// page), each with "show" (the eye) and up / down (earlier / later); the user's widgets with their file.
static int s_setHomePage;

static void ScrSetHome(HDC dc)
{
    int pages = (WgCount() + 11) / 12;
    s_setHomePage = s_setHomePage >= pages ? pages - 1 : s_setHomePage;
    Frame(dc, L"Главная · Виджеты");
    Panel(dc, 16, 64, 768, 336);
    for (int i = s_setHomePage * 12; i < WgCount() && i < s_setHomePage * 12 + 12; i++) {
        int id = s_cfg.homeOrder[i], j = i % 12, x = 24 + j / 6 * 380, y = 70 + j % 6 * 54;
        bool shown = !(s_cfg.homeHidden >> id & 1);
        if (j % 6)
            Sep(dc, x, y - 2, 368);
        Ico(dc, WgIcon(id), x + 8, y + 12, 26, shown ? kOrange : kDim);
        const wchar_t* sub = WgBig(id) ? L"большой" : WgWide(id) ? L"двойной" : NULL;
        wchar_t usub[48];
        if (id >= WG_COUNT) {
            _snwprintf(usub, 48, L"%s.ini · изменить%s", s_uw[id - WG_COUNT].key + 2, WgWide(id) ? L" · двойной" : L"");
            sub = usub;
            if (Zone(x, y, 218, 54, A_UWEDIT, id - WG_COUNT))   // its file into the editor
                R(dc, x, y + 2, 218, 50, 8, kPanel2);
        }
        if (id == WG_CLIMATE)
            sub = L"выключен: нет данных от машины";
        T(dc, x + 46, y + (sub ? 24 : 33), WgName(id), 18, shown ? kText : kDim, 700, AL_START, 172);
        if (sub)
            T(dc, x + 46, y + 43, sub, 13, kMuted, 400, AL_START, 172);
        Button(dc, x + 222, y + 7, 44, 40, NULL, i ? B_N : B_DIS, IC_UP, 15, 24, A_WGMOVE, id * 2);
        Button(dc, x + 270, y + 7, 44, 40, NULL, i < WgCount() - 1 ? B_N : B_DIS, IC_DOWN, 15, 24, A_WGMOVE,
               id * 2 + 1);
        Button(dc, x + 318, y + 7, 48, 40, NULL, shown ? B_SEL : B_N, shown ? IC_EYE : IC_EYE_OFF, 15, 24, A_WGVIS,
               id);   // hidden: the eye crossed out (the user 2026-10-24)
    }
    T(dc, 96, 440, L"вверх / вниз — раньше / позже; следующие занимают место скрытых. Свои виджеты — "
                   L"VestaWidgets\\*.ini на SD или \\SDMMC", 13, kDim, 400, AL_START, 490);   // (to the page buttons)
    if (pages > 1) {
        wchar_t pg[16];
        _snwprintf(pg, 16, L"%d / %d", s_setHomePage + 1, pages);
        Button(dc, 600, 412, 56, 52, NULL, s_setHomePage ? B_N : B_DIS, IC_BACK, 15, 26, A_SHPAGE, -1);
        T(dc, 692, 444, pg, 15, kMuted, 600, AL_MID);
        Button(dc, 728, 412, 56, 52, NULL, s_setHomePage + 1 < pages ? B_N : B_DIS, IC_NEXT, 15, 26, A_SHPAGE, 1);
    }
}

// The car's systems and the trip, opened from the widgets.
static void ScrCar(HDC dc)   // all the car's sensors (2026-10-22, out of the demo): "—" without data
{
    Frame(dc, L"Автомобиль");
    Panel(dc, 16, 68, 768, 340);   // (above y 408: "back" at the bottom left)
    for (int id = 0; id < kCarSensors; id++) {   // three columns of nine
        int x = 28 + id / 9 * 252, y = 72 + id % 9 * 37;
        wchar_t v[32];
        bool have = CarSensor(id, v, 32);
        bool d = id == 24 && Zone(x - 8, y + 2, 252, 34, A_SCREEN, S_OBD_DTC);   // the trouble codes: their screen
        if (d)
            R(dc, x - 8, y + 2, 252, 34, 8, kOrange);
        T(dc, x, y + 24, kCarSensor[id][0], 14, d ? kInk : kMuted, 400, AL_START, 140);
        T(dc, x + 236, y + 24, have ? v : id == 24 ? L"›" : L"—", 15, d ? kInk : have ? kText : kDim, 700, AL_END,
          100);
        if (id % 9 < 8 && id + 1 < kCarSensors)
            Sep(dc, x, y + 36, 236);
    }
}
