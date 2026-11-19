// radiodb.cpp - the FM station by the city and the frequency, its logo's key by the station's name (radiodb.h).
#include <windows.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "radiodb.h"

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
