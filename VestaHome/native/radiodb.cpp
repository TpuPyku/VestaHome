// radiodb.cpp - the FM station by the city and the frequency, its logo's key by the station's name (radiodb.h).
#include <windows.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "radiodb.h"
#include "vjson.h"

double RdbKm(double lat1, double lon1, double lat2, double lon2)
{
    const double r = 3.14159265358979 / 180;
    double a = sin((lat2 - lat1) * r / 2), b = sin((lon2 - lon1) * r / 2);
    double h = a * a + cos(lat1 * r) * cos(lat2 * r) * b * b;
    return 2 * 6371 * asin(sqrt(h));
}

// A line "lat;lon;f10;name" (end: its end) -> st; false: not one.
static bool Line(const char* line, const char* end, RdbStation* st)
{
    char* e;
    double lat = strtod(line, &e);
    if (*e != ';')
        return false;
    double lon = strtod(e + 1, &e);
    if (*e != ';')
        return false;
    long f = strtol(e + 1, &e, 10);
    if (*e != ';')
        return false;   // (a frequency 0 harmless: never asked for)
    int n = MultiByteToWideChar(CP_UTF8, 0, e + 1, (int)(end - e - 1), st->name, RDB_NAME - 1);
    if (n <= 0)
        return false;   // (no name, or one too long for its room)
    st->name[n] = 0;
    st->f10 = (int)f;
    st->lat = (float)lat;
    st->lon = (float)lon;
    return true;
}

void RdbLoad(Rdb* d, const char* text, double lat, double lon)
{
    d->n = 0;
    d->lat = lat;
    d->lon = lon;
    while (*text) {
        const char* end = text + strcspn(text, "\r\n");
        RdbStation st;
        float km = Line(text, end, &st) ? (float)RdbKm(lat, lon, st.lat, st.lon) : RDB_KM + 1;
        text = end + strspn(end, "\r\n");
        if (km > RDB_KM)
            continue;
        int at = d->n;
        if (at == RDB_NEAR) {   // full: in place of the farthest, when that one is farther
            at = 0;
            for (int i = 1; i < d->n; i++)
                if (d->km[i] > d->km[at])
                    at = i;
            if (d->km[at] <= km)
                continue;
        } else {
            d->n++;
        }
        d->s[at] = st;
        d->km[at] = km;
    }
}

bool RdbStale(const Rdb* d, double lat, double lon)
{
    return RdbKm(d->lat, d->lon, lat, lon) >= RDB_KM / 4;
}

const wchar_t* RdbName(const Rdb* d, int f10, double lat, double lon)
{
    const wchar_t* best = NULL;
    double closest = RDB_KM;
    for (int i = 0; i < d->n; i++) {
        if (d->s[i].f10 != f10)
            continue;
        double km = RdbKm(lat, lon, d->s[i].lat, d->s[i].lon);
        if (km <= closest) {
            closest = km;
            best = d->s[i].name;
        }
    }
    return best;
}

// ---------------------------------------------------------------- the names' keys

static const char* const kLat[33] = {   // а..я, then ё (make_radio.py's LAT)
    "a", "b", "v", "g", "d", "e", "zh", "z", "i", "y", "k", "l", "m", "n", "o", "p", "r", "s", "t", "u", "f", "kh",
    "ts", "ch", "sh", "shch", "", "y", "", "e", "yu", "ya", "e"
};

// The character in Latin (lower case): its letters, "" for ъ ь and ' (no letter, the word goes on), NULL when not a
// letter or a digit (a word ends).
static const char* Latin(wchar_t c, char* one)
{
    if ((unsigned)(c - L'A') < 26)
        c = (wchar_t)(c + 32);
    if ((unsigned)(c - L'a') < 26 || (unsigned)(c - L'0') < 10) {
        one[0] = (char)c;
        one[1] = 0;
        return one;
    }
    if ((unsigned)(c - 0x410) < 32)
        c = (wchar_t)(c + 0x20);
    if (c == 0x401 || c == 0x451)
        return kLat[32];
    if ((unsigned)(c - 0x430) < 32)
        return kLat[c - 0x430];
    return c == L'\'' ? "" : NULL;
}

void RdbKey(const wchar_t* name, char* key)
{
    char buf[RDB_KEY * 2];
    int len = 0, words = 0, firstEnd = 0, lastStart = 0;
    bool in = false;
    for (const wchar_t* p = name;; p++) {
        char one[2];
        const char* l = *p ? Latin(*p, one) : NULL;
        if (l) {
            if (!in && *l) {   // (a word starts with its first letter: ъ alone is none)
                in = true;
                words++;
                lastStart = len;
            }
            for (; *l && len < (int)sizeof(buf) - 1; l++)
                buf[len++] = *l;
            continue;
        }
        if (in && words == 1)
            firstEnd = len;
        in = false;
        if (!*p)
            break;
    }
    buf[len] = 0;
    int from = 0, to = len;
    if (words > 1 && firstEnd == 5 && !memcmp(buf, "radio", 5)) {
        from = 5;
        words--;
    }
    if (words > 1 && (!strcmp(buf + lastStart, "radio") || !strcmp(buf + lastStart, "fm")))
        to = lastStart;
    else if (words == 1 && to - from > 5 && !memcmp(buf + to - 5, "radio", 5))
        to -= 5;   // (Авторадио: one word)
    int k = to - from < RDB_KEY - 1 ? to - from : RDB_KEY - 1;
    memcpy(key, buf + from, k);
    key[k] = 0;
}

// ---------------------------------------------------------------- the logos from the net

void RdbNetPath(const wchar_t* name, const char* country, char* out, int cap)
{
    out[0] = 0;
    char u8[RDB_NAME * 3], enc[RDB_NAME * 9];
    if (!name[0] || !WideCharToMultiByte(CP_UTF8, 0, name, -1, u8, sizeof(u8), NULL, NULL))
        return;
    UrlEncode(u8, enc, sizeof(enc));
    int n = _snprintf(out, cap, "/json/stations/search?name=%s&countrycode=%s&hidebroken=true&order=clickcount"
                      "&reverse=true&limit=20", enc, country);
    if (n < 0 || n >= cap)
        out[0] = 0;   // (cut: no search)
}

// "http(s)://host/path" -> host, path ("/" at least), tls; false: another scheme, no host, a port.
static bool SplitUrl(const char* url, char* host, int hostCap, char* path, int pathCap, bool* tls)
{
    const char* p;
    if (!strncmp(url, "https://", 8))
        p = url + 8;
    else if (!strncmp(url, "http://", 7))
        p = url + 7;
    else
        return false;
    *tls = p - url == 8;
    int h = (int)strcspn(p, "/?#");
    if (!h || h >= hostCap || memchr(p, ':', h))
        return false;
    memcpy(host, p, h);
    host[h] = 0;
    _snprintf(path, pathCap, "%s%s", p[h] == '/' ? "" : "/", p + h);
    path[pathCap - 1] = 0;
    return true;
}

// cp1251's upper half (0x80..0xFF) in Unicode (0: none there)
static const unsigned short kCp1251[128] = {
    0x0402, 0x0403, 0x201A, 0x0453, 0x201E, 0x2026, 0x2020, 0x2021, 0x20AC, 0x2030, 0x0409, 0x2039,
    0x040A, 0x040C, 0x040B, 0x040F, 0x0452, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
    0x0000, 0x2122, 0x0459, 0x203A, 0x045A, 0x045C, 0x045B, 0x045F, 0x00A0, 0x040E, 0x045E, 0x0408,
    0x00A4, 0x0490, 0x00A6, 0x00A7, 0x0401, 0x00A9, 0x0404, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x0407,
    0x00B0, 0x00B1, 0x0406, 0x0456, 0x0491, 0x00B5, 0x00B6, 0x00B7, 0x0451, 0x2116, 0x0454, 0x00BB,
    0x0458, 0x0405, 0x0455, 0x0457, 0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415, 0x0416, 0x0417,
    0x0418, 0x0419, 0x041A, 0x041B, 0x041C, 0x041D, 0x041E, 0x041F, 0x0420, 0x0421, 0x0422, 0x0423,
    0x0424, 0x0425, 0x0426, 0x0427, 0x0428, 0x0429, 0x042A, 0x042B, 0x042C, 0x042D, 0x042E, 0x042F,
    0x0430, 0x0431, 0x0432, 0x0433, 0x0434, 0x0435, 0x0436, 0x0437, 0x0438, 0x0439, 0x043A, 0x043B,
    0x043C, 0x043D, 0x043E, 0x043F, 0x0440, 0x0441, 0x0442, 0x0443, 0x0444, 0x0445, 0x0446, 0x0447,
    0x0448, 0x0449, 0x044A, 0x044B, 0x044C, 0x044D, 0x044E, 0x044F
};

// A name kept read wrong once - its UTF-8 bytes taken for cp1251 (radio-browser has such: "Р\xa0СѓСЃ..." for
// "Русский"): read right again into out; false - not one (a character outside cp1251, not UTF-8). A name without
// such characters comes out as it was (its key the same: harmless).
static bool Unmangle(const wchar_t* in, wchar_t* out, int cap)
{
    char bytes[RDB_NAME * 6];
    int n = 0;
    for (; *in && n < (int)sizeof(bytes) - 1; in++) {
        int b = *in < 0x80 ? *in : -1;
        for (int i = 0; i < 128 && b < 0; i++)
            if (kCp1251[i] == *in)
                b = 0x80 + i;
        if (b < 0)
            return false;
        bytes[n++] = (char)b;
    }
    bytes[n] = 0;
    return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes, -1, out, cap) > 0;
}

bool RdbNetLogo(const char* json, const wchar_t* name, char* host, int hostCap, char* path, int pathCap, bool* tls)
{
    char want[RDB_KEY], key[RDB_KEY];
    RdbKey(name, want);
    if (!want[0])
        return false;
    for (const char* it = JFirst(json); it; it = JNext(it)) {
        const char* n = JKey(it, "name");
        const char* f = JKey(it, "favicon");
        if (!n || !f)
            continue;
        wchar_t wn[RDB_NAME * 2], fixed[RDB_NAME * 2];
        JWide(n, wn, RDB_NAME * 2);
        RdbKey(wn, key);
        if (strcmp(key, want) && Unmangle(wn, fixed, RDB_NAME * 2))
            RdbKey(fixed, key);
        char url[400];
        JStr(f, url, sizeof(url));
        if (!strcmp(key, want) && SplitUrl(url, host, hostCap, path, pathCap, tls))
            return true;
    }
    return false;
}

bool RdbMissed(const char* text, const char* key, int day)
{
    size_t k = strlen(key);
    for (const char* p = text; *p; p += strcspn(p, "\n"), p += *p == '\n') {
        if (!strncmp(p, key, k) && p[k] == ';') {
            int when = atoi(p + k + 1);
            if (day - when < RDB_MISS_DAYS)
                return true;
        }
    }
    return false;
}
