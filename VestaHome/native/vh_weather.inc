// vh_weather.inc - part of home.cpp (included there, in order): HTTP(S) and the weather.

// ---- HTTP and HTTPS through VestaCore's vhttp.cpp (BearSSL; the client shared with OSMapsCE).
// Body of a 200 answer (malloc'ed, zero-terminated) or NULL with the reason in err.
static char* Fetch(const char* host, bool tls, const char* path, const char* headers, wchar_t* err, int errCap)
{
    unsigned char* body = NULL;
    int len = 0, status = 0;
    HttpExtra extra = { headers, NULL, 0 };
    int r = HttpGet(host, tls, path, &body, &len, &status, NULL, NULL, 0, &extra);
    if (r != NET_OK) {
        _snwprintf(err, errCap, L"%S: %s", host, NetErrorText(r));
        return NULL;
    }
    if (status != 200) {
        _snwprintf(err, errCap, L"%S: ответ %d", host, status);
        free(body);
        return NULL;
    }
    return (char*)body;
}

// Minimal JSON lookups: the value after "key": (from p on), as a number or a string.
static const char* JsonAt(const char* p, const char* key)
{
    char k[48];
    _snprintf(k, sizeof(k), "\"%s\"", key);
    p = p ? strstr(p, k) : NULL;
    if (!p)
        return NULL;
    p += strlen(k);
    while (*p == ' ' || *p == ':')
        p++;
    return p;
}

static double JsonNum(const char* p, const char* key, double def)
{
    p = JsonAt(p, key);
    if (!p)
        return def;
    if (*p == '"')
        p++;
    return atof(p);
}

static bool JsonStr(const char* p, const char* key, wchar_t* out, int cap)
{
    p = JsonAt(p, key);
    if (!p || *p != '"')
        return false;
    p++;
    char tmp[200];
    int n = 0;
    while (*p && *p != '"' && n < (int)sizeof(tmp) - 1) {
        if (*p == '\\' && p[1])
            p++;
        tmp[n++] = *p++;
    }
    tmp[n] = 0;
    return MultiByteToWideChar(CP_UTF8, 0, tmp, -1, out, cap) > 0;
}

// ---- weather: Open-Meteo (open, no key) or OpenWeatherMap (free key), both over HTTP; Yandex over HTTPS with the
// user's own key of "Yandex Weather for smart home" (free, personal use only; its key opens REST /v2/forecast, not
// GraphQL). wttr.in and Gismeteo dropped by the user's decision 2026-10-15. Needs a place: GPS or the home point.
// Refreshed every 30 minutes, on a tap on the widget and after a change of the settings.
enum { WX_SUN, WX_PART, WX_CLOUD, WX_RAIN, WX_SNOW, WX_STORM, WX_FOG };
struct Weather {
    int     ok;                    // 1 data, 0 none yet, -1 the last try failed (old data may stay)
    bool    have;                  // the values below are from some successful fetch
    int     temp, tmax, tmin, wind, kind;
    wchar_t desc[48], place[48], err[80], rise[8], set[8];
    SYSTEMTIME at;                 // local time of the fetch
    DWORD   tick;
    int     prov;                  // W_*: where the data came from (Open-Meteo: its credit, CC BY 4.0)
};
static CRITICAL_SECTION s_wxCs;
static Weather          s_wx;
static volatile LONG    s_wxBusy;
static DWORD            s_wxTry;      // tick of the last attempt
static int              s_wxProv;     // the request's source: the chosen one, Open-Meteo when Yandex's are used up
// Yandex "weather for smart home": 30 requests a day, 1000 a month (the user's tariff, 2026-10-15); 30 x 31 = 930,
// so the daily limit keeps the monthly one. Used up: Open-Meteo until the next day.
enum { kYaPerDay = 30 };

// WMO weather code (Open-Meteo): its picture, and its text into desc (48).
static int OmKind(int code, wchar_t* desc)
{
    static const struct { int upTo, kind; const wchar_t* text; } k[] = {
        { 0, WX_SUN, L"ясно" }, { 1, WX_PART, L"преимущественно ясно" }, { 2, WX_PART, L"переменная облачность" },
        { 3, WX_CLOUD, L"пасмурно" }, { 48, WX_FOG, L"туман" }, { 57, WX_RAIN, L"морось" }, { 67, WX_RAIN, L"дождь" },
        { 77, WX_SNOW, L"снег" }, { 82, WX_RAIN, L"ливень" }, { 86, WX_SNOW, L"снегопад" }, { 99, WX_STORM, L"гроза" } };
    for (int i = 0; i < (int)(sizeof(k) / sizeof(k[0])); i++)
        if (code <= k[i].upTo) {
            wcscpy(desc, k[i].text);
            return k[i].kind;
        }
    desc[0] = 0;
    return WX_CLOUD;
}

// Yandex's condition ("partly-cloudy"): its picture, and its text into desc (48).
static int YaKind(const wchar_t* cond, wchar_t* desc)
{
    static const struct { const wchar_t* key; int kind; const wchar_t* text; } k[] = {
        { L"clear", WX_SUN, L"ясно" }, { L"partly-cloudy", WX_PART, L"малооблачно" },
        { L"cloudy", WX_CLOUD, L"облачно с прояснениями" }, { L"overcast", WX_CLOUD, L"пасмурно" },
        { L"fog", WX_FOG, L"туман" }, { L"drizzle", WX_RAIN, L"морось" }, { L"light-rain", WX_RAIN, L"небольшой дождь" },
        { L"rain", WX_RAIN, L"дождь" }, { L"moderate-rain", WX_RAIN, L"умеренный дождь" },
        { L"heavy-rain", WX_RAIN, L"сильный дождь" }, { L"continuous-heavy-rain", WX_RAIN, L"затяжной дождь" },
        { L"showers", WX_RAIN, L"ливень" }, { L"wet-snow", WX_SNOW, L"дождь со снегом" },
        { L"light-snow", WX_SNOW, L"небольшой снег" }, { L"snow", WX_SNOW, L"снег" },
        { L"snow-showers", WX_SNOW, L"снегопад" }, { L"hail", WX_STORM, L"град" },
        { L"thunderstorm", WX_STORM, L"гроза" }, { L"thunderstorm-with-rain", WX_STORM, L"дождь с грозой" },
        { L"thunderstorm-with-hail", WX_STORM, L"гроза с градом" } };
    for (int i = 0; i < (int)(sizeof(k) / sizeof(k[0])); i++)
        if (!wcscmp(cond, k[i].key)) {
            wcscpy(desc, k[i].text);
            return k[i].kind;
        }
    desc[0] = 0;
    return WX_CLOUD;
}

// The first number of an array value ("key":[21.7]).
static double JsonFirst(const char* p, const char* key, double def)
{
    p = JsonAt(p, key);
    return p && *p == '[' ? atof(p + 1) : def;
}

// The first time of an array value ("key":["2026-10-15T07:05"]) -> "7:05".
static void IsoClock(const char* p, const char* key, wchar_t* out)
{
    p = JsonAt(p, key);
    const char* t = p && *p == '[' ? strchr(p, 'T') : NULL;
    if (t && t - p < 16)
        _snwprintf(out, 8, L"%d:%02d", atoi(t + 1), atoi(t + 4));
}

static int OwmKind(int id)
{
    if (id >= 200 && id < 300) return WX_STORM;
    if (id >= 600 && id < 700) return WX_SNOW;
    if (id >= 700 && id < 800) return WX_FOG;
    if (id == 800) return WX_SUN;
    if (id == 801 || id == 802) return WX_PART;
    if (id > 802) return WX_CLOUD;
    return WX_RAIN;
}

static void UnixClock(double t, double tz, wchar_t* out)
{
    long s = (long)fmod(t + tz, 86400.0);
    _snwprintf(out, 8, L"%ld:%02ld", s / 3600, s / 60 % 60);
}

// ---- the week kept (the user 2026-10-31: no internet - the day's weather still shown, not lost on a restart): Open-Meteo
// gives 7 days; they and the last weather go to weather.dat beside the program - written when a day changes or every 3
// hours at most (the flash), read at the start. Shown as before: today's only - the last fetched if it is today's,
// else today's from the week.
enum { kWxDays = 7, kWxSaveMs = 3 * 3600000 };
struct WxDay { int date, kind, tmax, tmin; wchar_t rise[8], set[8], desc[48]; };   // date: YYYYMMDD; kind: WX_*
static WxDay s_wxWeek[kWxDays];
static int   s_wxDays;
static DWORD s_wxSaved;   // tick of the last write (0: none yet)

static int TodayDate()
{
    SYSTEMTIME t;
    GetLocalTime(&t);
    return t.wYear * 10000 + t.wMonth * 100 + t.wDay;
}

static const char* JsonItem(const char* p, const char* key, int i)   // the i-th item of an array value
{
    p = JsonAt(p, key);
    if (!p || *p != '[')
        return NULL;
    p++;
    for (int k = 0; k < i && p; k++) {
        p = strchr(p, ',');
        p = p ? p + 1 : NULL;
    }
    while (p && (*p == ' ' || *p == '"'))
        p++;
    return p && *p != ']' ? p : NULL;
}

static void ClockOf(const char* iso, wchar_t* out)   // "2026-10-15T07:05" -> "7:05"
{
    const char* t = iso ? strchr(iso, 'T') : NULL;
    out[0] = 0;
    if (t && t - iso < 12)
        _snwprintf(out, 8, L"%d:%02d", atoi(t + 1), atoi(t + 4));
}

static int WxWeekRead(const char* day, WxDay* out)   // Open-Meteo's "daily" arrays -> the days; their count
{
    int n = 0;
    for (int i = 0; i < kWxDays; i++) {
        const char* d = JsonItem(day, "time", i);
        if (!d || strlen(d) < 10)
            break;
        WxDay& x = out[n++];
        x.date = atoi(d) * 10000 + atoi(d + 5) * 100 + atoi(d + 8);
        const char* v = JsonItem(day, "weather_code", i);
        x.kind = OmKind(v ? atoi(v) : 0, x.desc);
        v = JsonItem(day, "temperature_2m_max", i);
        x.tmax = v ? (int)floor(atof(v) + 0.5) : 0;
        v = JsonItem(day, "temperature_2m_min", i);
        x.tmin = v ? (int)floor(atof(v) + 0.5) : 0;
        ClockOf(JsonItem(day, "sunrise", i), x.rise);
        ClockOf(JsonItem(day, "sunset", i), x.set);
    }
    return n;
}

static void Unzero(wchar_t* t)   // "07:05" -> "7:05", like the other sources
{
    if (t[0] == L'0')
        memmove(t, t + 1, 7 * sizeof(wchar_t));
}

static int YaKind(const wchar_t* cond, wchar_t* desc);

static int WxWeekYa(const char* fc, WxDay* out)   // Yandex's "forecasts" (limit=7): a day each, in their order
{
    int n = 0;
    for (const char* p = JsonAt(fc, "date"); p && n < kWxDays; p = JsonAt(p, "date")) {
        if (*p != '"' || strlen(p) < 11)
            break;
        WxDay& x = out[n++];
        x.date = atoi(p + 1) * 10000 + atoi(p + 6) * 100 + atoi(p + 9);
        x.rise[0] = x.set[0] = 0;
        JsonStr(p, "sunrise", x.rise, 8);
        JsonStr(p, "sunset", x.set, 8);
        Unzero(x.rise);
        Unzero(x.set);
        const char* ds = JsonAt(p, "day_short");
        x.tmax = (int)floor(JsonNum(ds, "temp", 0) + 0.5);
        x.tmin = (int)floor(JsonNum(ds, "temp_min", x.tmax) + 0.5);
        wchar_t cond[32] = L"";
        JsonStr(ds, "condition", cond, 32);
        x.kind = YaKind(cond, x.desc);
        p = ds ? ds : p + 1;
    }
    return n;
}

static void WxWeekSet(const WxDay* week, int n)   // the week fetched
{
    if (!n)
        return;
    EnterCriticalSection(&s_wxCs);
    memcpy(s_wxWeek, week, n * sizeof(WxDay));
    s_wxDays = n;
    LeaveCriticalSection(&s_wxCs);
}

static void WxPath(wchar_t* path)
{
    _snwprintf(path, MAX_PATH, L"%sweather.dat", s_dir);
    path[MAX_PATH - 1] = 0;
}

// The file: a line of the last weather (date, temp, max, min, wind, kind, the provider, HH:MM, rise, set, desc,
// place), then a line a day (date, code, max, min, rise, set); tab-separated UTF-8.
static void WxSave(const Weather& w)
{
    wchar_t path[MAX_PATH], line[256];
    WxPath(path);
    HANDLE f = CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return;
    char u[512];
    DWORD put;
    _snwprintf(line, 256, L"%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d:%02d\t%s\t%s\t%s\t%s\r\n",
               w.at.wYear * 10000 + w.at.wMonth * 100 + w.at.wDay, w.temp, w.tmax, w.tmin, w.wind, w.kind, w.prov,
               w.at.wHour, w.at.wMinute, w.rise, w.set, w.desc, w.place);
    line[255] = 0;
    WriteFile(f, u, WideCharToMultiByte(CP_UTF8, 0, line, -1, u, sizeof(u), NULL, NULL) - 1, &put, NULL);
    for (int i = 0; i < s_wxDays; i++) {
        const WxDay& x = s_wxWeek[i];
        _snwprintf(line, 256, L"%d\t%d\t%d\t%d\t%s\t%s\t%s\r\n", x.date, x.kind, x.tmax, x.tmin, x.rise, x.set,
                   x.desc);
        line[255] = 0;
        WriteFile(f, u, WideCharToMultiByte(CP_UTF8, 0, line, -1, u, sizeof(u), NULL, NULL) - 1, &put, NULL);
    }
    CloseHandle(f);
    s_wxSaved = GetTickCount() | 1;
}

static void WxField(wchar_t*& p, wchar_t* out, int cap)   // the next tab-separated field
{
    int k = 0;
    while (*p && *p != L'\t' && *p != L'\r' && *p != L'\n') {
        if (k < cap - 1)
            out[k++] = *p;
        p++;
    }
    out[k] = 0;
    if (*p == L'\t')
        p++;
}

static void WxLastRead(wchar_t* p, Weather* w, int* date)   // the file's first line
{
    wchar_t v[48];
    int f[7];
    for (int i = 0; i < 7; i++) {
        WxField(p, v, 48);
        f[i] = _wtoi(v);
    }
    *date = f[0];
    w->temp = f[1];
    w->tmax = f[2];
    w->tmin = f[3];
    w->wind = f[4];
    w->kind = f[5];
    w->prov = f[6];
    WxField(p, v, 48);
    w->at.wYear = (WORD)(f[0] / 10000);
    w->at.wMonth = (WORD)(f[0] / 100 % 100);
    w->at.wDay = (WORD)(f[0] % 100);
    w->at.wHour = (WORD)_wtoi(v);
    w->at.wMinute = (WORD)(wcschr(v, L':') ? _wtoi(wcschr(v, L':') + 1) : 0);
    WxField(p, w->rise, 8);
    WxField(p, w->set, 8);
    WxField(p, w->desc, 48);
    WxField(p, w->place, 48);
}

static void WxDayRead(wchar_t* p, WxDay* x)
{
    wchar_t v[16];
    int f[4];
    for (int i = 0; i < 4; i++) {
        WxField(p, v, 16);
        f[i] = _wtoi(v);
    }
    x->date = f[0];
    x->kind = f[1];
    x->tmax = f[2];
    x->tmin = f[3];
    WxField(p, x->rise, 8);
    WxField(p, x->set, 8);
    WxField(p, x->desc, 48);
}

// Today's from the week (none: false): the day's picture, max, min, sun; its time - midnight
static bool WxFromWeek(Weather* w)
{
    int today = TodayDate();
    for (int i = 0; i < s_wxDays; i++)
        if (s_wxWeek[i].date == today) {
            const WxDay& x = s_wxWeek[i];
            w->kind = x.kind;
            wcscpy(w->desc, x.desc);
            w->temp = x.tmax;
            w->tmax = x.tmax;
            w->tmin = x.tmin;
            w->wind = 0;
            wcscpy(w->rise, x.rise);
            wcscpy(w->set, x.set);
            GetLocalTime(&w->at);
            w->at.wHour = w->at.wMinute = 0;
            w->have = true;
            w->ok = 1;
            w->tick = 0;
            return true;
        }
    return false;
}

// At the start: the file read; the last weather shown if it is today's, else today's from the week
static void WxLoad()
{
    wchar_t path[MAX_PATH];
    WxPath(path);
    HANDLE f = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return;
    char u[2048];
    DWORD got = 0;
    ReadFile(f, u, sizeof(u) - 1, &got, NULL);
    CloseHandle(f);
    u[got] = 0;
    static wchar_t text[2048];
    MultiByteToWideChar(CP_UTF8, 0, u, -1, text, 2048);
    wchar_t* p = text;
    Weather w;
    memset(&w, 0, sizeof(w));
    int date = 0;
    WxLastRead(p, &w, &date);
    s_wxDays = 0;
    for (p = wcschr(p, L'\n'); p && *++p && s_wxDays < kWxDays; p = wcschr(p, L'\n'))
        WxDayRead(p, &s_wxWeek[s_wxDays++]);
    if (date == TodayDate()) {
        w.have = true;
        w.ok = 1;
        w.tick = 0;   // (old: asked again soon)
    } else if (!WxFromWeek(&w)) {
        return;
    }
    EnterCriticalSection(&s_wxCs);
    s_wx = w;
    LeaveCriticalSection(&s_wxCs);
    VLog(L"погода: из файла (%s), дней %d", date == TodayDate() ? L"сегодняшняя" : L"прогноз на сегодня", s_wxDays);
}

// No new weather: the day changed since the one shown - today's from the week
static void WxDayChanged()
{
    Weather w;
    EnterCriticalSection(&s_wxCs);
    w = s_wx;
    bool old = w.have && w.at.wYear * 10000 + w.at.wMonth * 100 + w.at.wDay != TodayDate();
    if (old && WxFromWeek(&w)) {
        wcscpy(w.place, s_wx.place);
        s_wx = w;
    }
    LeaveCriticalSection(&s_wxCs);
}

static DWORD WINAPI WeatherThread(LPVOID)
{
    VLog(L"поток погоды: id %08X", (unsigned)GetCurrentThreadId());
    Weather w;
    memset(&w, 0, sizeof(w));
    int prov = s_wxProv;
    double lat = s_cfg.homeLat, lon = s_cfg.homeLon;
    // by GPS: the fix of the last 10 min, else the last place it told (placepick.h); else the home point
    int src = s_cfg.weatherGps ? HerePlace(600000, &lat, &lon) : PLACE_NONE;
    bool here = src == PLACE_GPS || src == PLACE_LAST;
    bool home = s_cfg.homeLat != 0 || s_cfg.homeLon != 0;
    if (!here)
        lat = s_cfg.homeLat, lon = s_cfg.homeLon;
    VLog(L"погода: запрос, провайдер %d, %s", prov, here ? L"по GPS" : home ? L"дом" : L"по адресу");
    char key[64];
    WideCharToMultiByte(CP_UTF8, 0, s_cfg.weatherKey[prov], -1, key, sizeof(key), NULL, NULL);
    char path[300];
    char* body = NULL;
    if (prov == W_OMETEO) {
        // api.open-meteo.com does not answer from some Russian networks (2026-10-15: a timeout by HTTP and HTTPS,
        // its site works); the same service under historical-forecast-api.open-meteo.com does. The host that
        // answered last is tried first.
        static const char* hosts[2] = { "api.open-meteo.com", "historical-forecast-api.open-meteo.com" };
        static int s_omHost;
        if (!here && !home) {
            wcscpy(w.err, L"нет места: GPS или домашняя точка");
        } else {
            _snprintf(path, sizeof(path), "/v1/forecast?latitude=%.3f&longitude=%.3f&current=temperature_2m,"
                      "weather_code,wind_speed_10m&daily=weather_code,temperature_2m_max,temperature_2m_min,sunrise,"
                      "sunset&wind_speed_unit=ms&timezone=auto&forecast_days=7", lat, lon);
            for (int i = 0; i < 2 && !body; i++) {
                int h = (s_omHost + i) % 2;
                body = Fetch(hosts[h], false, path, NULL, w.err, 80);
                if (body)
                    s_omHost = h;
            }
            if (body) {
                const char* cur = JsonAt(body, "current");   // after "current_units", which has the same keys
                const char* day = JsonAt(body, "daily");
                w.temp = (int)floor(JsonNum(cur, "temperature_2m", 0) + 0.5);
                w.wind = (int)(JsonNum(cur, "wind_speed_10m", 0) + 0.5);
                w.kind = OmKind((int)JsonNum(cur, "weather_code", 0), w.desc);
                w.tmax = (int)floor(JsonFirst(day, "temperature_2m_max", w.temp) + 0.5);
                w.tmin = (int)floor(JsonFirst(day, "temperature_2m_min", w.temp) + 0.5);
                IsoClock(day, "sunrise", w.rise);
                IsoClock(day, "sunset", w.set);
                if (here)
                    wcscpy(w.place, L"по GPS");   // Open-Meteo gives no place name
                WxDay week[kWxDays];
                WxWeekSet(week, WxWeekRead(day, week));
                w.ok = 1;
            }
        }
    } else if (prov == W_OWM) {
        if (!key[0]) {
            wcscpy(w.err, L"нужен ключ API OpenWeatherMap");
        } else if (!here && !home) {
            wcscpy(w.err, L"нет места: GPS или домашняя точка");
        } else {
            _snprintf(path, sizeof(path), "/data/2.5/weather?lat=%.3f&lon=%.3f&units=metric&lang=ru&appid=%s",
                      lat, lon, key);
            body = Fetch("api.openweathermap.org", false, path, NULL, w.err, 80);
            if (body) {
                w.temp = (int)floor(JsonNum(JsonAt(body, "main"), "temp", 0) + 0.5);
                w.tmax = (int)floor(JsonNum(JsonAt(body, "main"), "temp_max", w.temp) + 0.5);
                w.tmin = (int)floor(JsonNum(JsonAt(body, "main"), "temp_min", w.temp) + 0.5);
                w.wind = (int)(JsonNum(JsonAt(body, "wind"), "speed", 0) + 0.5);
                w.kind = OwmKind((int)JsonNum(JsonAt(body, "weather"), "id", 800));
                JsonStr(JsonAt(body, "weather"), "description", w.desc, 48);
                JsonStr(body, "name", w.place, 48);
                double tz = JsonNum(body, "timezone", 0);
                UnixClock(JsonNum(JsonAt(body, "sys"), "sunrise", 0), tz, w.rise);
                UnixClock(JsonNum(JsonAt(body, "sys"), "sunset", 0), tz, w.set);
                w.ok = 1;
            }
        }
    } else if (!key[0]) {
        wcscpy(w.err, L"нужен ключ Яндекс Погоды");
    } else if (!here && !home) {
        wcscpy(w.err, L"нет места: GPS или домашняя точка");
    } else {
        char hdr[120];
        _snprintf(hdr, sizeof(hdr), "X-Yandex-Weather-Key: %s\r\n", key);
        _snprintf(path, sizeof(path), "/v2/forecast?lat=%.3f&lon=%.3f&limit=7&hours=false&extra=false", lat, lon);
        body = Fetch("api.weather.yandex.ru", true, path, hdr, w.err, 80);
        if (body) {
            const char* fact = JsonAt(body, "fact");
            const char* fc = JsonAt(body, "forecasts");
            const char* ds = JsonAt(fc, "day_short");
            w.temp = (int)floor(JsonNum(fact, "temp", 0) + 0.5);
            w.wind = (int)(JsonNum(fact, "wind_speed", 0) + 0.5);
            wchar_t cond[32] = L"";
            JsonStr(fact, "condition", cond, 32);
            w.kind = YaKind(cond, w.desc);
            w.tmax = (int)floor(JsonNum(ds, "temp", w.temp) + 0.5);
            w.tmin = (int)floor(JsonNum(ds, "temp_min", w.temp) + 0.5);
            JsonStr(fc, "sunrise", w.rise, 8);
            JsonStr(fc, "sunset", w.set, 8);
            Unzero(w.rise);
            Unzero(w.set);
            WxDay week[kWxDays];
            WxWeekSet(week, WxWeekYa(fc, week));
            if (here)
                wcscpy(w.place, L"по GPS");   // no place name without the paid "extra" data
            w.ok = 1;
        }
    }
    free(body);
    if (w.ok && !here && home && s_cfg.homeName[0])
        wcscpy(w.place, s_cfg.homeName);
    EnterCriticalSection(&s_wxCs);
    bool save = false;
    if (w.ok) {
        GetLocalTime(&w.at);
        w.tick = GetTickCount();
        w.have = true;
        w.prov = prov;
        save = !s_wxSaved || GetTickCount() - s_wxSaved >= kWxSaveMs ||
               s_wx.at.wDay != w.at.wDay || s_wx.at.wMonth != w.at.wMonth;
        s_wx = w;
        if (save)
            WxSave(w);
    } else {
        s_wx.ok = -1;
        wcscpy(s_wx.err, w.err);
    }
    LeaveCriticalSection(&s_wxCs);
    if (!w.ok)
        WxDayChanged();
    VLog(L"погода: %s %s", w.ok ? L"ok" : L"ошибка", w.ok ? w.place : w.err);
    s_wxBusy = 0;
    PostMessage(s_wnd, WM_APP_WX, 0, 0);
    return 0;
}

// One of today's Yandex requests, counted in the ini (a restart doesn't reset it); false = today's are used up,
// or the clock is not set yet (it resets on boot until GPS sets it: the count's day is unknown).
static bool YaTake()
{
    SYSTEMTIME t;
    GetLocalTime(&t);
    if (t.wYear < 2025)
        return false;
    int day = t.wYear * 10000 + t.wMonth * 100 + t.wDay;
    if (s_cfg.yaDay != day) {
        s_cfg.yaDay = day;
        s_cfg.yaCount = 0;
    }
    if (s_cfg.yaCount >= kYaPerDay)
        return false;
    s_cfg.yaCount++;
    SaveCfg();
    return true;
}

static void WeatherRefresh()
{
    if (InterlockedExchange((LONG*)&s_wxBusy, 1))
        return;
    s_wxTry = GetTickCount();
    s_wxProv = s_cfg.weatherProv;
    if (s_wxProv == W_YANDEX && s_cfg.weatherKey[W_YANDEX][0] && !YaTake()) {
        VLog(L"погода: запросы Яндекса на сегодня кончились (%d из %d) или часы не выставлены — Open-Meteo",
             s_cfg.yaCount, kYaPerDay);
        s_wxProv = W_OMETEO;
    }
    HANDLE h = CreateThread(NULL, 0, WeatherThread, NULL, 0, NULL);
    if (h)
        CloseHandle(h);
    else
        s_wxBusy = 0;
}

static void WxGet(Weather* w)
{
    EnterCriticalSection(&s_wxCs);
    *w = s_wx;
    LeaveCriticalSection(&s_wxCs);
}
