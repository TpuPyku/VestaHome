// vh_weather.inc - part of home.cpp (included there, in order): HTTP(S) and the weather.

// ---- HTTP and HTTPS through https.cpp (BearSSL; the copy of OSMapsCE's client).
// Body of a 200 answer (malloc'ed, zero-terminated) or NULL with the reason in err.
static char* Fetch(const char* host, bool tls, const char* path, const char* headers, wchar_t* err, int errCap)
{
    unsigned char* body = NULL;
    int len = 0, status = 0;
    int r = HttpGet(host, tls, path, headers, &body, &len, &status);
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

static DWORD WINAPI WeatherThread(LPVOID)
{
    VLog(L"поток погоды: id %08X", (unsigned)GetCurrentThreadId());
    Weather w;
    memset(&w, 0, sizeof(w));
    int prov = s_wxProv;
    Gps g;
    GpsGet(&g);
    bool here = s_cfg.weatherGps && GpsFresh(g, 600000);
    bool home = s_cfg.homeLat != 0 || s_cfg.homeLon != 0;
    double lat = here ? g.lat : s_cfg.homeLat, lon = here ? g.lon : s_cfg.homeLon;
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
                      "weather_code,wind_speed_10m&daily=temperature_2m_max,temperature_2m_min,sunrise,sunset"
                      "&wind_speed_unit=ms&timezone=auto&forecast_days=1", lat, lon);
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
        _snprintf(path, sizeof(path), "/v2/forecast?lat=%.3f&lon=%.3f&limit=1&hours=false&extra=false", lat, lon);
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
            if (w.rise[0] == L'0')   // "07:05" -> "7:05", like the other sources
                memmove(w.rise, w.rise + 1, 7 * sizeof(wchar_t));
            if (w.set[0] == L'0')
                memmove(w.set, w.set + 1, 7 * sizeof(wchar_t));
            if (here)
                wcscpy(w.place, L"по GPS");   // no place name without the paid "extra" data
            w.ok = 1;
        }
    }
    free(body);
    if (w.ok && !here && home && s_cfg.homeName[0])
        wcscpy(w.place, s_cfg.homeName);
    EnterCriticalSection(&s_wxCs);
    if (w.ok) {
        GetLocalTime(&w.at);
        w.tick = GetTickCount();
        w.have = true;
        w.prov = prov;
        s_wx = w;
    } else {
        s_wx.ok = -1;
        wcscpy(s_wx.err, w.err);
    }
    LeaveCriticalSection(&s_wxCs);
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
