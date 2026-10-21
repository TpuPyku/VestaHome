// vh_camdl.inc - part of home.cpp (included there, in order): the speed camera base download.

// ---- speed camera base from speedcamonline.ru (HTTPS only): the site's own export link for iGO,
// /igo/Rus&filter_id=&cam_name=&cam_adress=&cam_N=&cam_E=&region=RR&kind_list=1000,k1,k2,... (kind_list must start
// with 1000; the kinds are the site's filter checkboxes e<k>). The site's "verified only" switch doesn't change
// the export (checked 2026-10-12). Regions are the car number codes, Moscow is in 50 with its oblast, 00 is abroad;
// -1 = the whole of Russia (some 50 000 cameras, 2 MB). Each region is one request; they are joined into
// speedcam.txt through speedcam.tmp. A finished download is told to the window (WM_APP_CAM), which takes the file.
struct CamKind { int id; const wchar_t* name; int group; };   // group: 0 speed, 1 other fixed, 2 mobile/useless
static const CamKind kCamKinds[18] = {
    { 0, L"СТАЦ. РАДАР", 0 }, { 14, L"МАЛОМОЩНЫЙ", 0 }, { 11, L"СР. СКОР.: НАЧАЛО", 0 }, { 110, L"СР. СКОР.: КОНЕЦ", 0 },
    { 2, L"ПОТОК", 0 }, { 3, L"СТРЕЛКА", 0 }, { 12, L"ВИДЕОБЛОК", 1 }, { 5, L"СВЕТОФОРЫ", 1 },
    { 16, L"КОНТР. ОСТАНОВКИ", 1 }, { 1, L"КАМЕРА ПОСТА ДПС", 1 }, { 10, L"ПОСТ ДПС", 1 }, { 6, L"Ж/Д ПЕРЕЕЗД", 1 },
    { 9, L"МОБИЛЬНЫЙ ПОСТ", 2 }, { 7, L"МОБ. ЗАСАДА", 2 }, { 8, L"ТРЕНОГА", 2 }, { 4, L"НАБЛЮДЕНИЕ", 2 },
    { 300, L"МУЛЯЖ", 2 }, { 999, L"НЕ ОПРЕДЕЛЁН", 2 } };

struct CamRegion { const char* code; const wchar_t* name; };
static const CamRegion kCamRegions[] = {
    { "-1", L"Вся Россия" }, { "50", L"Москва и обл." }, { "78", L"Санкт-Петербург" }, { "47", L"Ленинградская" },
    { "01", L"Адыгея" }, { "02", L"Башкортостан" }, { "03", L"Бурятия" }, { "04", L"Респ. Алтай" },
    { "05", L"Дагестан" }, { "06", L"Ингушетия" }, { "07", L"Кабардино-Балк." }, { "08", L"Калмыкия" },
    { "09", L"Карачаево-Черк." }, { "10", L"Карелия" }, { "11", L"Коми" }, { "12", L"Марий Эл" },
    { "13", L"Мордовия" }, { "14", L"Якутия" }, { "15", L"Сев. Осетия" }, { "16", L"Татарстан" },
    { "17", L"Тыва" }, { "18", L"Удмуртия" }, { "19", L"Хакасия" }, { "21", L"Чувашия" },
    { "22", L"Алтайский кр." }, { "23", L"Краснодарский кр." }, { "24", L"Красноярский кр." }, { "25", L"Приморский кр." },
    { "26", L"Ставропольский кр." }, { "27", L"Хабаровский кр." }, { "28", L"Амурская" }, { "29", L"Архангельская" },
    { "30", L"Астраханская" }, { "31", L"Белгородская" }, { "32", L"Брянская" }, { "33", L"Владимирская" },
    { "34", L"Волгоградская" }, { "35", L"Вологодская" }, { "36", L"Воронежская" }, { "37", L"Ивановская" },
    { "38", L"Иркутская" }, { "39", L"Калининградская" }, { "40", L"Калужская" }, { "41", L"Камчатский кр." },
    { "42", L"Кемеровская" }, { "43", L"Кировская" }, { "44", L"Костромская" }, { "45", L"Курганская" },
    { "46", L"Курская" }, { "48", L"Липецкая" }, { "49", L"Магаданская" }, { "51", L"Мурманская" },
    { "52", L"Нижегородская" }, { "53", L"Новгородская" }, { "54", L"Новосибирская" }, { "55", L"Омская" },
    { "56", L"Оренбургская" }, { "57", L"Орловская" }, { "58", L"Пензенская" }, { "59", L"Пермский кр." },
    { "60", L"Псковская" }, { "61", L"Ростовская" }, { "62", L"Рязанская" }, { "63", L"Самарская" },
    { "64", L"Саратовская" }, { "65", L"Сахалинская" }, { "66", L"Свердловская" }, { "67", L"Смоленская" },
    { "68", L"Тамбовская" }, { "69", L"Тверская" }, { "70", L"Томская" }, { "71", L"Тульская" },
    { "72", L"Тюменская" }, { "73", L"Ульяновская" }, { "74", L"Челябинская" }, { "75", L"Забайкальский кр." },
    { "76", L"Ярославская" }, { "79", L"Еврейская АО" }, { "82", L"Крым" }, { "83", L"Ненецкий АО" },
    { "86", L"ХМАО — Югра" }, { "89", L"ЯНАО" }, { "92", L"Севастополь" }, { "95", L"Чечня" } };
enum { kCamRegionCount = sizeof(kCamRegions) / sizeof(kCamRegions[0]), kRegionsPerPage = 24 };

#define WM_APP_CAM (WM_APP + 12)
static volatile LONG s_dlBusy;
static wchar_t       s_dlStatus[100];   // the download's progress or result
static DWORD         s_dlTry;           // tick of the last automatic attempt
static int           s_regPage;

// Comma lists of the settings ("63,73"): membership and toggling of one item.
static bool InList(const wchar_t* list, const wchar_t* item)
{
    int n = (int)wcslen(item);
    for (const wchar_t* p = list; *p;) {
        const wchar_t* e = wcschr(p, L',');
        int len = e ? (int)(e - p) : (int)wcslen(p);
        if (len == n && !wcsncmp(p, item, n))
            return true;
        if (!e)
            break;
        p = e + 1;
    }
    return false;
}

static void ToggleInList(wchar_t* list, int cap, const wchar_t* item)
{
    wchar_t out[256] = L"";
    bool had = false;
    int n = (int)wcslen(item);
    for (const wchar_t* p = list; *p;) {
        const wchar_t* e = wcschr(p, L',');
        int len = e ? (int)(e - p) : (int)wcslen(p);
        if (len == n && !wcsncmp(p, item, n)) {
            had = true;
        } else if (len) {
            if (out[0])
                wcscat(out, L",");
            wcsncat(out, p, len);
        }
        if (!e)
            break;
        p = e + 1;
    }
    if (!had && (int)(wcslen(out) + n + 2) < cap) {
        if (out[0])
            wcscat(out, L",");
        wcscat(out, item);
    }
    wcsncpy(list, out, cap - 1);
    list[cap - 1] = 0;
}

static bool KindOn(int k)
{
    wchar_t s[8];
    _snwprintf(s, 8, L"%d", kCamKinds[k].id);
    return InList(s_cfg.camKinds, s);
}

static void ToggleKind(int k)
{
    wchar_t s[8];
    _snwprintf(s, 8, L"%d", kCamKinds[k].id);
    ToggleInList(s_cfg.camKinds, 100, s);
    SaveCfg();
}

// Presets: 0 speed only, 1 every fixed camera (no mobile posts, tripods, dummies), 2 all.
static void KindsPreset(int preset)
{
    s_cfg.camKinds[0] = 0;
    for (int k = 0; k < 18; k++)
        if (kCamKinds[k].group <= preset) {
            wchar_t s[8];
            _snwprintf(s, 8, L"%d", kCamKinds[k].id);
            ToggleInList(s_cfg.camKinds, 100, s);
        }
    SaveCfg();
}

static bool RegionOn(int i)
{
    wchar_t s[8];
    _snwprintf(s, 8, L"%S", kCamRegions[i].code);
    return InList(s_cfg.camRegions, s);
}

// "The whole of Russia" excludes the regions and the other way round.
static void ToggleRegion(int i)
{
    if (i == 0)
        s_cfg.camRegions[0] = 0;
    else if (InList(s_cfg.camRegions, L"-1"))
        s_cfg.camRegions[0] = 0;
    wchar_t s[8];
    _snwprintf(s, 8, L"%S", kCamRegions[i].code);
    ToggleInList(s_cfg.camRegions, 200, s);
    SaveCfg();
}

static void RegionsText(wchar_t* s, int n)
{
    s[0] = 0;
    for (int i = 0; i < kCamRegionCount; i++)
        if (RegionOn(i) && (int)wcslen(s) + 40 < n) {
            if (s[0])
                wcscat(s, L", ");
            wcscat(s, kCamRegions[i].name);
        }
    if (!s[0])
        wcscpy(s, L"не выбраны — нажмите, чтобы выбрать");
}

static long YmdDays(int ymd)   // days since 2000-01-01, enough to count a week
{
    int y = ymd / 10000, m = ymd / 100 % 100, d = ymd % 100;
    if (m < 3) {
        y--;
        m += 12;
    }
    return 365L * y + y / 4 - y / 100 + y / 400 + (153 * (m - 3) + 2) / 5 + d;
}

static int Today()
{
    SYSTEMTIME t;
    GetLocalTime(&t);
    return t.wYear * 10000 + t.wMonth * 100 + t.wDay;
}

static DWORD WINAPI CamDownloadThread(LPVOID)
{
    VLog(L"поток базы камер: id %08X", (unsigned)GetCurrentThreadId());
    char regions[200], kinds[120];
    WideCharToMultiByte(CP_UTF8, 0, s_cfg.camRegions, -1, regions, sizeof(regions), NULL, NULL);
    strcpy(kinds, "1000");
    if (s_cfg.camKinds[0]) {
        strcat(kinds, ",");
        WideCharToMultiByte(CP_UTF8, 0, s_cfg.camKinds, -1, kinds + 5, sizeof(kinds) - 5, NULL, NULL);
    }
    int count = 1;
    for (const char* p = regions; *p; p++)
        count += *p == ',';
    wchar_t tmp[MAX_PATH], dir[MAX_PATH];
    _snwprintf(dir, MAX_PATH, L"%s%s", s_dir, kCamFolder);
    CreateDirectory(dir, NULL);   // not in the release: made by the first download
    _snwprintf(tmp, MAX_PATH, L"%sspeedcam.tmp", dir);
    HANDLE h = CreateFile(tmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    bool ok = h != INVALID_HANDLE_VALUE;
    int cams = 0, idx = 0;
    wchar_t err[80] = L"не записать speedcam.tmp";
    for (char* r = regions; ok && *r; idx++) {
        char* e = strchr(r, ',');
        if (e)
            *e = 0;
        _snwprintf(s_dlStatus, 100, L"скачивается %S (%d из %d)…", r, idx + 1, count);
        PostMessage(s_wnd, WM_APP_CAM, 0, 0);
        char path[300];
        _snprintf(path, sizeof(path), "/igo/Rus&filter_id=&cam_name=&cam_adress=&cam_N=&cam_E=&region=%s&kind_list=%s",
                  r, kinds);
        char* body = Fetch("speedcamonline.ru", true, path, NULL, err, 80);
        if (!body) {
            ok = false;
            break;
        }
        char* data = body;
        if (idx > 0) {   // the header only once
            char* nl = strchr(data, '\n');
            data = nl ? nl + 1 : data + strlen(data);
        }
        for (const char* p = data; *p; p++)
            cams += *p == '\n';
        DWORD w;
        ok = WriteFile(h, data, (DWORD)strlen(data), &w, NULL) != 0;
        free(body);
        r = e ? e + 1 : r + strlen(r);
    }
    if (h != INVALID_HANDLE_VALUE)
        CloseHandle(h);
    if (ok) {
        wchar_t dst[MAX_PATH];
        _snwprintf(dst, MAX_PATH, L"%s%s", s_dir, kCamDefault);
        DeleteFile(dst);
        ok = MoveFile(tmp, dst) != 0;
        _snwprintf(s_dlStatus, 100, ok ? L"загружено камер: %d" : L"не заменить speedcam.txt", cams - 1);
    } else {
        DeleteFile(tmp);
        _snwprintf(s_dlStatus, 100, L"не скачалось: %s", err);
    }
    s_dlBusy = 0;
    PostMessage(s_wnd, WM_APP_CAM, ok ? 1 : 0, 0);
    return 0;
}

static void CamDownload()
{
    if (!s_cfg.camRegions[0]) {
        wcscpy(s_dlStatus, L"выберите регионы");
        return;
    }
    if (InterlockedExchange((LONG*)&s_dlBusy, 1))
        return;
    wcscpy(s_dlStatus, L"соединение…");
    HANDLE h = CreateThread(NULL, 0, CamDownloadThread, NULL, 0, NULL);
    if (h)
        CloseHandle(h);
    else
        s_dlBusy = 0;
}

// The download has finished well: speedcam\speedcam.txt becomes the camera file, read again.
static void CamDownloaded()
{
    wcscpy(s_cfg.camFile, kCamDefault);
    s_cfg.camUpdated = Today();
    SaveCfg();
    s_camLoaded = false;
    Gps g;
    GpsGet(&g);
    if (GpsFresh(g, 10000))
        LoadCams(g.lat, g.lon);
    else if (s_cfg.homeLat || s_cfg.homeLon)
        LoadCams(s_cfg.homeLat, s_cfg.homeLon);
}

// Once a week by itself when the setting is on (tried again in an hour if the network isn't there).
static void CamAutoUpdate()
{
    if (!s_cfg.camWeekly || !s_cfg.camRegions[0] || s_dlBusy || GetTickCount() - s_dlTry < 3600000)
        return;
    if (s_cfg.camUpdated && YmdDays(Today()) - YmdDays(s_cfg.camUpdated) < 7)
        return;
    s_dlTry = GetTickCount();
    CamDownload();
}

static void ScrCamDl(HDC dc)
{
    Frame(dc, L"БАЗА КАМЕР · SPEEDCAMONLINE.RU");
    Panel(dc, 16, 64, 768, 330);
    wchar_t s[200];
    RegionsText(s, 200);
    WRow(dc, 68, L"Регионы", s, A_SCREEN, S_CAMREG, 776);
    T(dc, 36, 150, L"Типы камер", 19, kText, 700);
    Button(dc, 372, 126, 136, 36, L"СКОРОСТЬ", B_N, IC_NONE, 13, 0, A_KINDS, 0);
    Button(dc, 516, 126, 136, 36, L"БЕЗ МОБИЛЬНЫХ", B_N, IC_NONE, 13, 0, A_KINDS, 1);
    Button(dc, 660, 126, 108, 36, L"ВСЕ", B_N, IC_NONE, 13, 0, A_KINDS, 2);
    for (int k = 0; k < 18; k++)
        Button(dc, 28 + (k % 4) * 188, 174 + (k / 4) * 43, 180, 37, kCamKinds[k].name, KindOn(k) ? B_SEL : B_N,
               IC_NONE, 12, 0, A_KIND, k);
    Dock(dc);
    if (!s_dlStatus[0] && s_cfg.camUpdated)
        _snwprintf(s_dlStatus, 100, L"обновлено %02d.%02d.%04d", s_cfg.camUpdated % 100, s_cfg.camUpdated / 100 % 100,
                   s_cfg.camUpdated / 10000);
    T(dc, 20, 446, s_dlStatus[0] ? s_dlStatus : L"файл speedcam\\speedcam.txt (iGO) в папке VestaHome", 15, kMuted, 400,
      AL_START, 360);
    Button(dc, 392, 410, 176, 58, L"РАЗ В НЕДЕЛЮ", s_on[T_CAM_WEEKLY] ? B_SEL : B_N, IC_REFRESH, 13, 24, A_TOGGLE,
           T_CAM_WEEKLY);
    Button(dc, 576, 410, 208, 58, s_dlBusy ? L"ЗАГРУЗКА…" : L"СКАЧАТЬ", s_dlBusy ? B_DIS : B_PRI, IC_NONE, 18, 0,
           s_dlBusy ? A_NONE : A_CAMDL);
}

static void ScrCamReg(HDC dc)
{
    int pages = (kCamRegionCount + kRegionsPerPage - 1) / kRegionsPerPage;
    if (s_regPage >= pages)
        s_regPage = 0;
    Frame(dc, L"РЕГИОНЫ КАМЕР");
    for (int k = 0; k < kRegionsPerPage; k++) {
        int i = s_regPage * kRegionsPerPage + k;
        if (i >= kCamRegionCount)
            break;
        wchar_t s[48];
        if (i == 0)
            wcscpy(s, kCamRegions[0].name);
        else
            _snwprintf(s, 48, L"%S  %s", kCamRegions[i].code, kCamRegions[i].name);
        Button(dc, 16 + (k % 4) * 194, 64 + (k / 4) * 55, 184, 48, s, RegionOn(i) ? B_SEL : B_FILL, IC_NONE, 14, 0,
               A_REGION, i);
    }
    Dock(dc);
    Button(dc, 16, 410, 120, 58, NULL, B_N, IC_BACK, 18, 0, A_REGPAGE, -1);
    wchar_t s[16];
    _snwprintf(s, 16, L"%d / %d", s_regPage + 1, pages);
    T(dc, 210, 448, s, 22, kText, 700, AL_MID);
    Button(dc, 284, 410, 120, 58, NULL, B_N, IC_NEXT, 18, 0, A_REGPAGE, 1);
    Button(dc, 584, 410, 200, 58, L"ГОТОВО", B_PRI, IC_NONE, 18, 0, A_BACK);
}
