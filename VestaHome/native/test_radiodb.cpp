// test_radiodb.cpp - radiodb.cpp on the PC: the station by the place and the frequency, the names' keys (as
// make_radio.py's name_key). Exit code: the failed checks.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "radiodb.h"

static int s_failed, s_checks;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAILED: %s\n", what);
    }
}

static bool Key(const wchar_t* name, const char* want)
{
    char k[RDB_KEY];
    RdbKey(name, k);
    if (strcmp(k, want))
        printf("  key: '%s', not '%s'\n", k, want);
    return !strcmp(k, want);
}

static void TestKey()
{
    Check(Key(L"Радио DFM", "dfm") && Key(L"DFM", "dfm"), "key: a leading радио off, DFM kept whole");
    Check(Key(L"Ретро FM", "retro") && Key(L"RETRO", "retro"), "key: a trailing FM word off");
    Check(Key(L"Наше радио", "nashe") && Key(L"NASHE", "nashe"), "key: a trailing радио word off");
    Check(Key(L"Авторадио", "avto") && Key(L"AVTO", "avto"), "key: one word with радио at its end");
    Check(Key(L"Радио Monte Carlo", "montecarlo"), "key: words joined");
    Check(Key(L"Радио", "radio") && Key(L"FM", "fm"), "key: the word alone kept");
    Check(Key(L"Европа Плюс", "evropaplyus") && Key(L"ЁЖИК", "ezhik"), "key: Cyrillic in Latin, upper case, ё");
    Check(Key(L"Рок-н-ролл", "roknroll") && Key(L"Объект", "obekt"), "key: ъ ь no letter, a hyphen between words");
    Check(Key(L"Rock’n’Roll FM", "rocknroll") && Key(L"Mix ФМ", "mix"), "key: ’ between words, ФМ as FM");
    Check(Key(L"Радио №1", "1") && Key(L"", "") && Key(L"!!!", ""), "key: digits; nothing - empty");
    Check(Key(L"AZ az 09", "azaz09") && Key(L"a{b 1:2 [", "ab12"), "key: the Latin and digit ranges, to their ends");
    Check(Key(L"А Я а я Ё", "ayaayae") && Key(L"аѐб", "ab") && Key(L"Ъ", ""), "key: the Cyrillic range, to its ends");
    Check(Key(L"Радио Авторадио", "avto") && Key(L"Радио радио", "radio") && Key(L"Авто радио FM", "avtoradio"),
          "key: радио first, then the one word left; one trailing word only");
    Check(Key(L"radiox fm", "radiox") && Key(L"x radiofm", "xradiofm"), "key: only the words themselves");
    Check(Key(L"Xradio", "x"), "key: one word of six with радио at its end");
    wchar_t longName[100];
    for (int i = 0; i < 99; i++)
        longName[i] = L'a';
    longName[99] = 0;
    char k[RDB_KEY];
    RdbKey(longName, k);
    Check(strlen(k) == RDB_KEY - 1, "key: a long name cut to its room");
    longName[RDB_KEY] = 0;   // (a key of RDB_KEY: one too many)
    RdbKey(longName, k);
    Check(strlen(k) == RDB_KEY - 1, "key: one letter over its room - cut");
    longName[RDB_KEY - 1] = 0;
    RdbKey(longName, k);
    Check(strlen(k) == RDB_KEY - 1, "key: just its room");
}

static const char* kTable =   // (UTF-8 as the file: VS2005 would make a narrow literal cp1251)
    "53.208;50.136;8820;\xD0\xA0\xD0\xB0\xD0\xB4\xD0\xB8\xD0\xBE\x20\xD0\x93\xD0\xBE\xD1\x80\xD0\xB4\xD0\xBE\xD1\x81\xD1\x82\xD1\x8C" "\r\n"   // Самара
    "53.530;49.346;8820;\xD0\xA2\xD0\xBE\xD0\xBB\xD1\x8C\xD1\x8F\xD1\x82\xD1\x82\xD0\xB8\x20\x38\x38\x2E\x32" "\r\n"    // Тольятти, ~ 60 km
    "55.756;37.617;8820;\xD0\x9C\xD0\xBE\xD1\x81\xD0\xBA\xD0\xB2\xD0\xB0\x20\x38\x38\x2E\x32" "\r\n"      // far
    "53.208;50.136;10430;\xD0\xA1\xD0\xB0\xD0\xBC\xD0\xB0\xD1\x80\xD0\xB0\x2D\xD0\x9C\xD0\xB0\xD0\xBA\xD1\x81\xD0\xB8\xD0\xBC\xD1\x83\xD0\xBC" "\r\n"
    "bad line\r\n"
    "53.2;50.1;x;no frequency\r\n"
    "53.2;x;8820;no longitude\r\n"
    "53.2;50.1\r\n"
    "53.2;50.1;0;zero\r\n"
    "53.208;50.136;9100;A\r\n"                // a name of one letter
    "53.208;50.136;9200;0123456789012345678901234567890123456789\r\n"   // (40: too long for its room)
    "53.208;50.136;9300;012345678901234567890123456789012345678\r\n"    // (39: just its room)
    "53.208;50.136;9000;\r\n";                // no name

static void TestPlace()
{
    static Rdb d;
    memset(&d, 0, sizeof(d));
    RdbLoad(&d, kTable, 53.208, 50.136);   // in Samara
    Check(d.n == 6, "the stations within 100 km only, bad lines skipped");
    Check(RdbName(&d, 9100, 53.208, 50.136) && !RdbName(&d, 9200, 53.208, 50.136) &&
          RdbName(&d, 9300, 53.208, 50.136) && wcslen(RdbName(&d, 9300, 53.208, 50.136)) == 39,
          "a name of one letter; of 39 kept, of 40 not");
    Check(!RdbStale(&d, 53.208 + 0.19785, 50.136) && RdbStale(&d, 53.53, 49.346), "stale only 25 km away");
    const wchar_t* n = RdbName(&d, 8820, 53.208, 50.136);
    Check(n && !wcscmp(n, L"Радио Гордость"), "on 88.2 in Samara: Samara's");
    n = RdbName(&d, 8820, 53.50, 49.40);
    Check(n && !wcscmp(n, L"Тольятти 88.2"), "on 88.2 near Tolyatti: the nearer one");
    Check(RdbName(&d, 10430, 53.208, 50.136) && !RdbName(&d, 9999, 53.208, 50.136), "another frequency, an unknown");
    Check(!RdbName(&d, 10430, 55.756, 37.617), "too far from it: none");
    Check(fabs(RdbKm(53.208, 50.136, 53.53, 49.346) - 63.4746) < 0.002, "km: Samara - Tolyatti");
    // more near than room: the nearest kept
    static char many[RDB_NEAR * 2 * 40];
    int len = 0;
    for (int i = 0; i < RDB_NEAR + 50; i++)
        len += sprintf(many + len, "%.3f;50.000;%d;s%d\n", 53.0 + (RDB_NEAR + 50 - i) * 0.0001, 8800 + i, i);
    RdbLoad(&d, many, 53.0, 50.0);
    Check(d.n == RDB_NEAR && RdbName(&d, 8800 + RDB_NEAR + 49, 53.0, 50.0) && !RdbName(&d, 8800, 53.0, 50.0),
          "full: the farthest left out");
    RdbLoad(&d, "53.050;50.000;7000;farther\n", 53.0, 50.0);   // (alone: kept)
    Check(d.n == 1, "one far one alone: kept");
    len = 0;
    for (int i = 0; i < RDB_NEAR; i++)
        len += sprintf(many + len, "53.000;50.000;%d;s%d\n", 8800 + i, i);
    len += sprintf(many + len, "53.050;50.000;7000;farther\n");
    RdbLoad(&d, many, 53.0, 50.0);
    Check(d.n == RDB_NEAR && !RdbName(&d, 7000, 53.0, 50.0) && RdbName(&d, 8801, 53.0, 50.0),
          "full, a farther one after: left out, none of the near ones lost");
    len = 0;   // full, its farthest the second: a nearer one in its place
    for (int i = 0; i < RDB_NEAR; i++)
        len += sprintf(many + len, "%s;50.000;%d;s%d\n", i == 1 ? "53.040" : "53.000", 8800 + i, i);
    len += sprintf(many + len, "53.010;50.000;7000;nearer\n");
    RdbLoad(&d, many, 53.0, 50.0);
    Check(RdbName(&d, 7000, 53.0, 50.0) && !RdbName(&d, 8801, 53.0, 50.0) && RdbName(&d, 8800, 53.0, 50.0),
          "full: the farthest found wherever it is");
    RdbLoad(&d, "53.000;50.000;9100;one\n", 53.0, 50.0);
    Check(d.n == 1 && !RdbName(&d, 8801, 53.0, 50.0), "read again: the ones before gone");
    RdbLoad(&d, "55.756;37.617;8820;far\n", 53.0, 50.0);
    Check(d.n == 0 && !RdbStale(&d, 53.0, 50.0), "none near: not stale where it is (not read each frame)");
}

// The logos not in the set, from the net (the user 2026-11-19): radio-browser.info's search by the name and the
// country, the first station of the same key with a picture.
static void TestNet()
{
    char p[300];
    RdbNetPath(L"Ретро FM", "RU", p, sizeof(p));
    Check(!strcmp(p, "/json/stations/search?name=%D0%A0%D0%B5%D1%82%D1%80%D0%BE%20FM&countrycode=RU"
                     "&hidebroken=true&order=clickcount&reverse=true&limit=20"), "net: the search's path");
    RdbNetPath(L"", "RU", p, sizeof(p));
    Check(!p[0], "net: no name - no search");
    RdbNetPath(L"X", "RU", p, 20);
    Check(!p[0], "net: no room - no search");
    RdbNetPath(L"X", "RU", p, sizeof(p));
    int plen = (int)strlen(p);
    RdbNetPath(L"X", "RU", p, plen + 1);
    Check((int)strlen(p) == plen, "net: just its room - the path");
    RdbNetPath(L"X", "RU", p, plen);
    Check(!p[0], "net: a byte short - none");
    const char* json = "[{\"name\":\"Retro FM Moscow 80s\",\"favicon\":\"https://a.ru/x.png\"},"
                       "{\"name\":\"RETRO FM\",\"favicon\":\"\"},"
                       "{\"name\":\"" "\xD0\xA0\xD0\xB5\xD1\x82\xD1\x80\xD0\xBE\x20\x46\x4D" "\",\"favicon\":\"ftp://b.ru/y.png\"},"
                       "{\"name\":\"Retro\",\"favicon\":\"http://c.ru:8080/z.png\"},"
                       "{\"name\":\"" "\xD0\xA0\xD0\xB0\xD0\xB4\xD0\xB8\xD0\xBE\x20\xD0\xA0\xD0\xB5\xD1\x82\xD1\x80\xD0\xBE" "\",\"favicon\":\"https://retrofm.ru/img/logo.jpg?v=2\"},"
                       "{\"name\":\"Retro FM\",\"favicon\":\"http://other.ru/l.png\"}]";
    char host[64], path[200];
    bool tls = false;
    Check(RdbNetLogo(json, L"RETRO", host, sizeof(host), path, sizeof(path), &tls) && !strcmp(host, "retrofm.ru") &&
          !strcmp(path, "/img/logo.jpg?v=2") && tls,
          "net: the first of the same key with a picture by http(s), no port (others: another name, none, ftp)");
    Check(RdbNetLogo("[{\"name\":\"DFM\",\"favicon\":\"http://dfm.ru\"}]", L"Радио DFM", host, sizeof(host), path,
                     sizeof(path), &tls) && !strcmp(host, "dfm.ru") && !strcmp(path, "/") && !tls,
          "net: plain http, the root's path");
    Check(!RdbNetLogo(json, L"Europa Plus", host, sizeof(host), path, sizeof(path), &tls) &&
          !RdbNetLogo("{}", L"Retro", host, sizeof(host), path, sizeof(path), &tls) &&
          !RdbNetLogo("[{\"name\":\"Retro\",\"favicon\":\"https://\"}]", L"Retro", host, sizeof(host), path,
                      sizeof(path), &tls),
          "net: none of its name, not a list, no host");
    Check(!RdbNetLogo(json, L"", host, sizeof(host), path, sizeof(path), &tls), "net: no name");
    const char* one = "[{\"name\":\"DFM\",\"favicon\":\"https://abcd.ru/0123456789\"}]";
    char h8[8], p5[5];
    Check(RdbNetLogo(one, L"DFM", h8, 8, path, sizeof(path), &tls) && !strcmp(h8, "abcd.ru") &&
          !RdbNetLogo(one, L"DFM", h8, 7, path, sizeof(path), &tls), "net: a host just in its room, one over - none");
    Check(RdbNetLogo(one, L"DFM", host, sizeof(host), p5, 5, &tls) && !strcmp(p5, "/012"), "net: a long path cut");
    // a name kept read wrong once (its UTF-8 bytes taken for cp1251 - radio-browser has such): read right again
    const char* moj = "[{\"name\":\"" "\xD0\xA0\xC2\xA0\xD0\xA1\xD1\x93\xD0\xA1\xD0\x83\xD0\xA1\xD0\x83\xD0\xA0\xD1\x94\xD0\xA0\xD1\x91\xD0\xA0\xE2\x84\x96\x20\xD0\xA0\xD2\x90\xD0\xA0\xD1\x91\xD0\xA1\xE2\x80\x9A" "\",\"favicon\":\"https://fm-24.ru/ruhit.jpg\"}]";
    Check(RdbNetLogo(moj, L"Русский ХИТ", host, sizeof(host), path, sizeof(path), &tls) && !strcmp(host, "fm-24.ru"),
          "net: a name read wrong once - matched");
    Check(!RdbNetLogo("[{\"name\":\"\xC3\xA9t\xC3\xA9\",\"favicon\":\"https://x.fr/a.png\"}]", L"Ete", host,
                      sizeof(host), path, sizeof(path), &tls), "net: other letters not taken for it");
    // the misses: not asked again for 30 days
    const char* miss = "retro;100\r\ndfm;50\nbad\n";
    Check(RdbMissed(miss, "retro", 100) && RdbMissed(miss, "retro", 129) && !RdbMissed(miss, "retro", 130),
          "misses: within 30 days");
    Check(!RdbMissed(miss, "dfm", 100) && !RdbMissed(miss, "nashe", 100) && !RdbMissed(miss, "bad", 0) &&
          !RdbMissed(miss, "ret", 100), "misses: an old one, another, a bad line, a part of a key");
}

int main()
{
    TestKey();
    TestPlace();
    TestNet();
    printf("test_radiodb: %d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
