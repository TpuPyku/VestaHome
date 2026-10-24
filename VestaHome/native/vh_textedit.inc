// VestaHome, part of home.cpp (included by it, in order; one translation unit).
// The text files' editor (2026-10-22, the user: Notepad's CE keyboard keeps vanishing): our keyboard, a line at a
// time - the configs (the widgets' .ini, apps.txt, radio.txt) are lines; "key=value" opens its value only. Read as
// UTF-8 (its BOM kept) or, not being UTF-8, the ANSI page; saved the same way with CRLF, the old file as .bak.
// Opened from "Diagnostics -> Text files" (the file chooser, .ini / .txt / .cfg) and from the home's widgets list
// (a user widget's .ini; saved - the widgets read again).

enum { kTeMax = 500, kTeLen = 400 };
static wchar_t  s_teFile[MAX_PATH];
static wchar_t* s_teLine[kTeMax];
static int      s_teCount, s_teSel = -1, s_teAt = -1;   // s_teAt: the line on the keyboard (-1 none)
static bool     s_teDirty, s_teBom, s_teAnsi, s_teDelArm, s_teQuitArm, s_teNew, s_teWidget;
static bool     s_teRO;               // read only in part (too long): never written back
static wchar_t  s_teBuf[kTeLen];      // what the keyboard edits: the line, or its value
static wchar_t  s_teTitle[96];        // the keyboard's title: the key, "СТРОКА N", "НОВАЯ СТРОКА"
static Screen   s_teBack = S_HOME;

static Scroll s_teList = { 0, 0, 46 * 3 };   // the lines: the finger scrolls them, the dock stays

static void TeShow(int i)   // line i in sight
{
    int top = i * 46, h = s_teList.view.bottom - s_teList.view.top;
    if (h <= 0)
        h = 322;
    if (top < s_teList.pos)
        s_teList.pos = top;
    else if (top + 46 > s_teList.pos + h)
        s_teList.pos = top + 46 - h;
}

static void TeFree()
{
    for (int i = 0; i < s_teCount; i++)
        free(s_teLine[i]);
    s_teCount = 0;
}

static bool TeSet(int i, const wchar_t* s)
{
    wchar_t* p = (wchar_t*)malloc((wcslen(s) + 1) * sizeof(wchar_t));
    if (!p)
        return false;
    wcscpy(p, s);
    if (i < s_teCount)
        free(s_teLine[i]);
    s_teLine[i] = p;
    return true;
}

// key=value: the key's length (0: not one - a comment, a section, a plain line)
static int TeKeyLen(const wchar_t* s)
{
    if (s[0] == L'#' || s[0] == L';' || s[0] == L'[')
        return 0;
    const wchar_t* e = wcschr(s, L'=');
    return e && e > s ? (int)(e - s) : 0;
}

static bool TeOpen(const wchar_t* path, Screen back, bool widget)
{
    TeFree();
    wcsncpy(s_teFile, path, MAX_PATH - 1);
    s_teFile[MAX_PATH - 1] = 0;
    s_teBack = back;
    s_teWidget = widget;
    s_teSel = -1;
    s_teList.pos = 0;
    s_teDirty = s_teDelArm = s_teQuitArm = s_teRO = false;
    DWORD size = 0;
    char* buf = ReadAll(path, &size);
    s_teBom = buf && (unsigned char)buf[0] == 0xEF && (unsigned char)buf[1] == 0xBB && (unsigned char)buf[2] == 0xBF;
    s_teAnsi = buf && !s_teBom && !ValidUtf8((const unsigned char*)buf);
    s_screen = S_TEXTEDIT;
    if (!buf) {
        wcscpy(s_status, GetFileAttributes(path) == 0xFFFFFFFF ? L"новый файл" : L"файл не читается");
        return false;
    }
    s_status[0] = 0;
    char* p = s_teBom ? buf + 3 : buf;
    bool cut = false;
    while (*p && s_teCount < kTeMax) {
        char* end = strchr(p, '\n');
        if (end)
            *end = 0;
        int n = (int)strlen(p);
        if (n && p[n - 1] == '\r')
            p[--n] = 0;
        wchar_t line[kTeLen];
        if (MultiByteToWideChar(s_teAnsi ? CP_ACP : CP_UTF8, 0, p, -1, line, kTeLen) <= 0) {
            MultiByteToWideChar(s_teAnsi ? CP_ACP : CP_UTF8, 0, p, kTeLen / 2, line, kTeLen - 1);   // too long: cut
            line[kTeLen - 1] = 0;
            cut = true;
        }
        if (!TeSet(s_teCount, line))
            break;
        s_teCount++;
        p = end ? end + 1 : p + n;
    }
    s_teRO = *p || cut;
    if (s_teRO)
        wcscpy(s_status, *p ? L"файл длиннее 500 строк: только просмотр" : L"длинные строки обрезаны: только просмотр");
    free(buf);
    return true;
}

static bool TeReadOnly()   // a file read only in part must not be written back
{
    return s_teRO;
}

static void TeSave()
{
    if (TeReadOnly())
        return;
    wchar_t bak[MAX_PATH + 8];
    _snwprintf(bak, MAX_PATH + 8, L"%s.bak", s_teFile);
    bak[MAX_PATH + 7] = 0;
    DeleteFile(bak);
    MoveFile(s_teFile, bak);
    HANDLE h = CreateFile(s_teFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    bool ok = h != INVALID_HANDLE_VALUE;
    DWORD w;
    if (ok && s_teBom)
        ok = WriteFile(h, "\xEF\xBB\xBF", 3, &w, NULL) != 0;
    for (int i = 0; ok && i < s_teCount; i++) {
        char line[kTeLen * 3 + 2];
        int n = WideCharToMultiByte(s_teAnsi ? CP_ACP : CP_UTF8, 0, s_teLine[i], -1, line, kTeLen * 3, NULL, NULL);
        n = n > 0 ? n - 1 : 0;
        line[n++] = '\r';
        line[n++] = '\n';
        ok = WriteFile(h, line, n, &w, NULL) && (int)w == n;
    }
    if (h != INVALID_HANDLE_VALUE)
        CloseHandle(h);
    if (!ok) {   // the old one back
        DeleteFile(s_teFile);
        MoveFile(bak, s_teFile);
        wcscpy(s_status, L"не удалось записать");
        return;
    }
    s_teDirty = false;
    wcscpy(s_status, L"сохранено (прежний — .bak)");
    VLog(L"редактор: сохранён %s, строк %d", s_teFile, s_teCount);
    if (s_teWidget)
        UserWidgetsScan();
}

// A line onto the keyboard: its value when it is key=value, else all of it; new: an empty line after the selected
static void TeEdit(int i, bool isNew)
{
    s_teNew = isNew;
    s_teAt = i;
    s_teBuf[0] = 0;
    if (isNew) {
        wcscpy(s_teTitle, L"Новая строка");
    } else {
        const wchar_t* s = s_teLine[i];
        int k = TeKeyLen(s);
        if (k) {
            _snwprintf(s_teTitle, 96, L"%.*s =", k < 80 ? k : 80, s);
            wcsncpy(s_teBuf, s + k + 1, kTeLen - 1);
        } else {
            _snwprintf(s_teTitle, 96, L"СТРОКА %d", i + 1);
            wcsncpy(s_teBuf, s, kTeLen - 1);
        }
        s_teBuf[kTeLen - 1] = 0;
        s_teTitle[95] = 0;
    }
    OpenKeysFor(s_teBuf, kTeLen - 1, s_teTitle, S_TEXTEDIT);
}

static void TeKeysDone()   // (CfgKeysDone): the keyboard's text back into its line
{
    int i = s_teAt;
    s_teAt = -1;
    if (i < 0)
        return;
    wchar_t line[kTeLen * 2];
    if (s_teNew) {
        wchar_t* p = s_teCount < kTeMax && s_teBuf[0] ? (wchar_t*)malloc((wcslen(s_teBuf) + 1) * sizeof(wchar_t))
                                                     : NULL;
        if (!p)
            return;
        wcscpy(p, s_teBuf);
        for (int k = s_teCount; k > i; k--)   // in at i, the rest one down
            s_teLine[k] = s_teLine[k - 1];
        s_teLine[i] = p;
        s_teCount++;
    } else {
        int k = TeKeyLen(s_teLine[i]);
        if (k)
            _snwprintf(line, kTeLen * 2, L"%.*s=%s", k, s_teLine[i], s_teBuf);
        else
            wcscpy(line, s_teBuf);
        line[kTeLen - 1] = 0;
        if (!wcscmp(line, s_teLine[i]) || !TeSet(i, line))
            return;
    }
    s_teSel = i;
    TeShow(i);
    s_teDirty = true;
    s_status[0] = 0;
    s_teQuitArm = false;
}

static void TeDelete()
{
    if (s_teSel < 0 || s_teSel >= s_teCount)
        return;
    free(s_teLine[s_teSel]);
    for (int k = s_teSel; k < s_teCount - 1; k++)
        s_teLine[k] = s_teLine[k + 1];
    s_teCount--;
    if (s_teSel >= s_teCount)
        s_teSel = s_teCount - 1;
    s_teDirty = true;
    s_teDelArm = false;
}

static void TeClose()
{
    TeFree();
    s_teFile[0] = 0;
    s_status[0] = 0;
    s_screen = s_teBack;
}

// The file's lines in a list the finger scrolls (the dock stays): the number, key=value in two parts, comments
// dim; a tap selects, the selected one tapped again goes to the keyboard. The dock: back (asks when changed), add,
// delete (asks), save.
static void ScrTextEdit(HDC dc)
{
    const wchar_t* fn = wcsrchr(s_teFile, L'\\');
    wchar_t s[MAX_PATH + 32];
    _snwprintf(s, MAX_PATH + 32, L"Текст · %s%s", fn ? fn + 1 : s_teFile, s_teDirty ? L" *" : L"");
    s[MAX_PATH + 31] = 0;
    Frame(dc, s, A_TEBACK);
    Panel(dc, 16, 64, 768, 330);
    s_teList.content = s_teCount * 46;
    ScrollBegin(dc, s_teList, 16, 68, 736, 390);
    int first = s_teList.pos / 46;
    for (int i = first; i < s_teCount; i++) {   // only the lines in sight
        int y = 68 + i * 46 - s_teList.pos;
        if (y >= 390)
            break;
        bool sel = i == s_teSel, d = Zone(16, y, 720, 46, A_TELINE, i);
        if (sel || d)
            R(dc, 20, y + 2, 712, 42, 8, d ? kOrange : kPanel2, d ? CLR_INVALID : kOrange);
        else if (i)
            Sep(dc, 32, y, 692);
        COLORREF fg = d ? kInk : kText, mg = d ? kInk : kMuted;
        wchar_t no[8];
        _snwprintf(no, 8, L"%d", i + 1);
        T(dc, 64, y + 29, no, 13, d ? kInk : kDim, 400, AL_END);
        const wchar_t* line = s_teLine[i];
        int k = TeKeyLen(line);
        if (k) {
            wchar_t key[96];
            _snwprintf(key, 96, L"%.*s", k < 95 ? k : 95, line);
            key[95] = 0;
            T(dc, 80, y + 29, key, 16, mg, 400, AL_START, 230);
            T(dc, 320, y + 29, line + k + 1, 17, fg, 700, AL_START, 404);
        } else {
            bool note = line[0] == L'#' || line[0] == L';';
            T(dc, 80, y + 29, line[0] ? line : L"(пустая)", 16, d ? kInk : note || !line[0] ? kDim : kText,
              line[0] == L'[' ? 700 : 400, AL_START, 644);
        }
    }
    ScrollEnd(dc, s_teList, 740, 68, 40, 322, 60);
    if (!s_teCount)
        T(dc, 400, 220, L"Файл пуст — «+ СТРОКА» добавит первую", 18, kMuted, 400, AL_MID);
    Dock(dc);
    bool ro = TeReadOnly();
    Button(dc, 88, 410, 168, 58, L"+ Строка", ro || s_teCount >= kTeMax ? B_DIS : B_N, IC_NONE, 17, 0,   // (right of
           ro ? A_NONE : A_TEADD);                                                                         //  "back")
    bool can = s_teSel >= 0 && !ro;
    Button(dc, 264, 410, 168, 58, s_teDelArm ? L"Точно удалить?" : L"Удалить", !can ? B_DIS : s_teDelArm ? B_FILL : B_N,
           IC_NONE, 17, 0, can ? A_TEDEL : A_NONE);
    Button(dc, 440, 410, 168, 58, L"Изменить", can ? B_N : B_DIS, IC_NONE, 17, 0, can ? A_TELINE : A_NONE, s_teSel);
    Button(dc, 616, 410, 168, 58, L"Сохранить", s_teDirty && !ro ? B_PRI : B_DIS, IC_NONE, 17, 0,
           s_teDirty && !ro ? A_TESAVE : A_NONE);
}

// The actions (DoAction): a line tapped, add, delete, save, back
static void TeAction(Action a, int arg)
{
    if (a != A_TEDEL)
        s_teDelArm = false;
    if (a != A_TEBACK)
        s_teQuitArm = false;
    switch (a) {
    case A_TELINE:
        if (arg == s_teSel && !TeReadOnly())
            TeEdit(arg, false);
        else
            s_teSel = arg;
        break;
    case A_TEADD:
        TeEdit(s_teSel >= 0 ? s_teSel + 1 : s_teCount, true);
        break;
    case A_TEDEL:
        if (s_teDelArm)
            TeDelete();
        else
            s_teDelArm = true;
        break;
    case A_TESAVE:
        TeSave();
        break;
    case A_TEBACK:
        if (s_teDirty && !s_teQuitArm) {   // (the frame shows the status)
            s_teQuitArm = true;
            wcscpy(s_status, L"не сохранено: назад ещё раз — выйти");
        } else
            TeClose();
        break;
    default:
        break;
    }
}
