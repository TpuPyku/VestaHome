// netcat.cpp - the internet radio's stations: lines and the radio-browser.info catalogue. See netcat.h.
#include "netcat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vjson.h"

static void Utf8ToWide(const char* s, wchar_t* out, int cap)   // cut to its room (cap - 1 characters)
{
    wchar_t w[512];
    if (!MultiByteToWideChar(CP_UTF8, 0, s, -1, w, 512))
        w[0] = 0;
    wcsncpy(out, w, cap - 1);
    out[cap - 1] = 0;
}

static void TrimW(wchar_t* s)
{
    int n = (int)wcslen(s), a = 0;
    while (n > 0 && (s[n - 1] == L' ' || s[n - 1] == L'\t'))
        s[--n] = 0;
    while (s[a] == L' ' || s[a] == L'\t')
        a++;
    memmove(s, s + a, (n - a + 1) * sizeof(wchar_t));
}

static bool IsHttp(const char* url)
{
    return !_strnicmp(url, "http://", 7) || !_strnicmp(url, "https://", 8);
}

// The field [from, to) of a line (no further than its end), into out.
static void Part(const char* from, const char* to, char* out, int cap)
{
    int n = (int)(to - from);
    if (n > cap - 1)
        n = cap - 1;
    memcpy(out, from, n);
    out[n] = 0;
    while (n > 0 && (out[n - 1] == '\r' || out[n - 1] == '\n' || out[n - 1] == ' ' || out[n - 1] == '\t'))
        out[--n] = 0;
}

bool NcParseLine(const char* line, NcStation* out)
{
    memset(out, 0, sizeof(*out));
    if (line[0] == '#')
        return false;
    const char* f[4];
    int n = 0;
    f[n++] = line;
    for (const char* p = line; *p && n < 4; p++)
        if (*p == '|')
            f[n++] = p + 1;
    if (n < 2)
        return false;
    const char* end = line + strlen(line);
    char name[200], sub[200];
    Part(f[0], f[1] - 1, name, sizeof(name));
    Part(f[1], n > 2 ? f[2] - 1 : end, out->url, sizeof(out->url));
    if (n > 2)
        Part(f[2], n > 3 ? f[3] - 1 : end, out->uuid, sizeof(out->uuid));
    Part(n > 3 ? f[3] : end, end, sub, sizeof(sub));
    Utf8ToWide(name, out->name, 64);
    TrimW(out->name);
    Utf8ToWide(sub, out->sub, 64);
    return out->name[0] && IsHttp(out->url);
}

int NcFormatLine(const NcStation& s, char* out, int cap)
{
    char name[200], sub[200];
    WideCharToMultiByte(CP_UTF8, 0, s.name, -1, name, sizeof(name), NULL, NULL);
    WideCharToMultiByte(CP_UTF8, 0, s.sub, -1, sub, sizeof(sub), NULL, NULL);
    int n = _snprintf(out, cap, "%s|%s|%s|%s", name, s.url, s.uuid, sub);
    out[cap - 1] = 0;
    return n < 0 ? (int)strlen(out) : n;
}

// ---- Russian names
struct NcWord { const char* key; const wchar_t* ru; };

static const NcWord kTags[] = {
    { "pop", L"Поп" }, { "rock", L"Рок" }, { "news", L"Новости" }, { "music", L"Музыка" },
    { "dance", L"Танцевальная" }, { "electronic", L"Электроника" }, { "classical", L"Классика" },
    { "jazz", L"Джаз" }, { "talk", L"Разговорное" }, { "oldies", L"Ретро" }, { "retro", L"Ретро" },
    { "80s", L"Хиты 80-х" }, { "90s", L"Хиты 90-х" }, { "70s", L"Хиты 70-х" }, { "hits", L"Хиты" },
    { "top 40", L"Топ-40" }, { "country", L"Кантри" }, { "chillout", L"Чилаут" }, { "ambient", L"Эмбиент" },
    { "hip-hop", L"Хип-хоп" }, { "hip hop", L"Хип-хоп" }, { "rap", L"Рэп" }, { "house", L"Хаус" },
    { "techno", L"Техно" }, { "trance", L"Транс" }, { "metal", L"Метал" }, { "blues", L"Блюз" },
    { "reggae", L"Регги" }, { "soul", L"Соул" }, { "christian", L"Христианское" }, { "religion", L"Религия" },
    { "sports", L"Спорт" }, { "kids", L"Детское" }, { "children", L"Детское" }, { "chanson", L"Шансон" },
    { "lounge", L"Лаунж" }, { "folk", L"Фолк" }, { "latin", L"Латино" }, { "disco", L"Диско" },
    { "easy listening", L"Лёгкая музыка" }, { "alternative", L"Альтернатива" }, { "indie", L"Инди" },
    { "rnb", L"R&B" }, { "world music", L"Музыка мира" }, { "relax", L"Релакс" }, { "comedy", L"Юмор" },
    { NULL, NULL }
};

static const NcWord kCountries[] = {
    { "RU", L"Россия" }, { "US", L"США" }, { "DE", L"Германия" }, { "FR", L"Франция" },
    { "GB", L"Великобритания" }, { "IT", L"Италия" }, { "ES", L"Испания" }, { "UA", L"Украина" },
    { "BY", L"Беларусь" }, { "KZ", L"Казахстан" }, { "PL", L"Польша" }, { "NL", L"Нидерланды" },
    { "CA", L"Канада" }, { "BR", L"Бразилия" }, { "MX", L"Мексика" }, { "AR", L"Аргентина" },
    { "CN", L"Китай" }, { "JP", L"Япония" }, { "IN", L"Индия" }, { "TR", L"Турция" }, { "GR", L"Греция" },
    { "AT", L"Австрия" }, { "CH", L"Швейцария" }, { "CZ", L"Чехия" }, { "RO", L"Румыния" },
    { "HU", L"Венгрия" }, { "AU", L"Австралия" }, { "SE", L"Швеция" }, { "NO", L"Норвегия" },
    { "FI", L"Финляндия" }, { "DK", L"Дания" }, { "BE", L"Бельгия" }, { "PT", L"Португалия" },
    { "IL", L"Израиль" }, { "LV", L"Латвия" }, { "LT", L"Литва" }, { "EE", L"Эстония" }, { "GE", L"Грузия" },
    { "AM", L"Армения" }, { "AZ", L"Азербайджан" }, { "UZ", L"Узбекистан" }, { "KG", L"Киргизия" },
    { "MD", L"Молдова" }, { "RS", L"Сербия" }, { "BG", L"Болгария" }, { "HR", L"Хорватия" },
    { "ID", L"Индонезия" }, { "KR", L"Южная Корея" }, { "CO", L"Колумбия" }, { "CL", L"Чили" },
    { "PE", L"Перу" }, { NULL, NULL }
};

static const NcWord kLangs[] = {
    { "russian", L"Русский" }, { "english", L"Английский" }, { "german", L"Немецкий" },
    { "spanish", L"Испанский" }, { "french", L"Французский" }, { "italian", L"Итальянский" },
    { "ukrainian", L"Украинский" }, { "polish", L"Польский" }, { "portuguese", L"Португальский" },
    { "chinese", L"Китайский" }, { "japanese", L"Японский" }, { "arabic", L"Арабский" },
    { "turkish", L"Турецкий" }, { "dutch", L"Нидерландский" }, { "greek", L"Греческий" },
    { "kazakh", L"Казахский" }, { "belarusian", L"Белорусский" }, { "hindi", L"Хинди" }, { NULL, NULL }
};

void NcName(int kind, const char* key, const char* name, wchar_t* out, int cap)
{
    const NcWord* t = kind == NC_COUNTRIES ? kCountries : kind == NC_LANGS ? kLangs : kTags;
    for (; t->key; t++)
        if (!_stricmp(t->key, key)) {
            wcsncpy(out, t->ru, cap - 1);
            out[cap - 1] = 0;
            return;
        }
    Utf8ToWide(name ? name : key, out, cap);
    if (out[0] >= L'a' && out[0] <= L'z')
        out[0] = (wchar_t)(out[0] - L'a' + L'A');
}

// ---- the catalogue's answers
static bool Playable(const char* codec)
{
    return !_stricmp(codec, "MP3") || !_stricmp(codec, "AAC") || !_stricmp(codec, "AAC+") || !_stricmp(codec, "OGG");
}

static bool OneStation(const char* it, NcStation* s)
{
    char name[200], codec[16], code[8], country[64];
    memset(s, 0, sizeof(*s));
    JStr(JKey(it, "url_resolved"), s->url, sizeof(s->url));
    if (!s->url[0])
        JStr(JKey(it, "url"), s->url, sizeof(s->url));
    JStr(JKey(it, "codec"), codec, sizeof(codec));
    JStr(JKey(it, "name"), name, sizeof(name));
    if (!IsHttp(s->url) || !Playable(codec) || JNum(JKey(it, "hls"), 0) != 0)
        return false;
    Utf8ToWide(name, s->name, 64);
    TrimW(s->name);
    if (!s->name[0])
        return false;
    JStr(JKey(it, "stationuuid"), s->uuid, sizeof(s->uuid));
    JStr(JKey(it, "countrycode"), code, sizeof(code));
    JStr(JKey(it, "country"), country, sizeof(country));
    wchar_t where[48];
    NcName(NC_COUNTRIES, code, country, where, 48);
    int rate = (int)JNum(JKey(it, "bitrate"), 0);
    wchar_t c[16];
    int k = 0;
    for (; codec[k] && k < 15; k++)   // (ASCII: "aac+" -> "AAC+")
        c[k] = (wchar_t)(codec[k] >= 'a' && codec[k] <= 'z' ? codec[k] - 'a' + 'A' : codec[k]);
    c[k] = 0;
    wchar_t rt[16] = L"";
    if (rate > 0)
        _snwprintf(rt, 16, L" %d", rate);
    _snwprintf(s->sub, 64, L"%s%s%s%s", where, where[0] ? L" · " : L"", c, rt);
    s->sub[63] = 0;
    return true;
}

int NcStations(const char* json, NcStation* out, int cap)
{
    int n = 0;
    for (const char* it = JFirst(json); it && n < cap; it = JNext(it))
        if (OneStation(it, &out[n]))
            n++;
    return n;
}

int NcCats(const char* json, int kind, NcCat* out, int cap)
{
    int n = 0;
    for (const char* it = JFirst(json); it && n < cap; it = JNext(it)) {
        NcCat& c = out[n];
        char name[64];
        JStr(JKey(it, "name"), name, sizeof(name));
        if (kind == NC_COUNTRIES)
            JStr(JKey(it, "iso_3166_1"), c.key, sizeof(c.key));
        else
            strcpy(c.key, name);
        if (!c.key[0])
            continue;
        NcName(kind, c.key, name, c.name, 48);
        c.count = (int)JNum(JKey(it, "stationcount"), 0);
        n++;
    }
    return n;
}

// ---- the requests' paths
void NcCatsPath(int kind, char* out, int cap)
{
    const char* what = kind == NC_COUNTRIES ? "countries" : kind == NC_LANGS ? "languages" : "tags";
    _snprintf(out, cap, "/json/%s?order=stationcount&reverse=true&hidebroken=true%s", what,
              kind == NC_COUNTRIES ? "" : "&limit=100");
    out[cap - 1] = 0;
}

void NcStationsPath(int kind, const char* arg, int offset, bool byName, char* out, int cap)
{
    char enc[200] = "", what[260] = "";
    if (arg)
        UrlEncode(arg, enc, sizeof(enc));
    if (kind == NC_TAG)
        _snprintf(what, sizeof(what), "tag=%s&tagExact=true&", enc);
    else if (kind == NC_COUNTRY)
        _snprintf(what, sizeof(what), "countrycode=%s&", enc);
    else if (kind == NC_LANG)
        _snprintf(what, sizeof(what), "language=%s&languageExact=true&", enc);
    else if (kind == NC_SEARCH)
        _snprintf(what, sizeof(what), "name=%s&", enc);
    what[sizeof(what) - 1] = 0;
    _snprintf(out, cap, "/json/stations/search?%sorder=%s&reverse=%s&hidebroken=true&limit=%d&offset=%d", what,
              byName ? "name" : "clickcount", byName ? "false" : "true", kNcPage, offset);
    out[cap - 1] = 0;
}

void NcClickPath(const char* uuid, char* out, int cap)
{
    _snprintf(out, cap, "/json/url/%s", uuid);
    out[cap - 1] = 0;
}
