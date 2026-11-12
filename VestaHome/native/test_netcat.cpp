// test_netcat.cpp - netcat.cpp on the PC: the stations' lines, the catalogue's answers and paths, the Russian
// names. Exit code: the failed checks.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "netcat.h"
#include "country.h"

static int s_failed, s_checks;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAILED: %s\n", what);
    }
}

static void TestLines()
{
    NcStation s;
    Check(NcParseLine("DFM|https://dfm.hostingradio.ru/dfm96.aacp", &s) && !wcscmp(s.name, L"DFM") &&
          !strcmp(s.url, "https://dfm.hostingradio.ru/dfm96.aacp") && !s.uuid[0] && !s.sub[0], "radio.txt's line");
    Check(NcParseLine("  \xD0\xA0\xD0\xB0\xD0\xB4\xD0\xB8\xD0\xBE\x20\xD0\x92\xD0\xB0\xD0\xBD\xD1\x8F"  "|http://a.b/c\r", &s) && !wcscmp(s.name, L"Радио Ваня") && !strcmp(s.url, "http://a.b/c"),
          "the name trimmed, CR dropped");
    Check(NcParseLine("Jazz24|https://j.org/s|9f-1|\xD0\xA1\xD0\xA8\xD0\x90\x20\xC2\xB7" " MP3 128", &s) && !strcmp(s.uuid, "9f-1") &&
          !wcscmp(s.sub, L"США · MP3 128") && !strcmp(s.url, "https://j.org/s"), "a favourite's line");
    Check(!NcParseLine("# DFM|https://a.b", &s), "a comment");
    Check(!NcParseLine("DFM|ftp://a.b", &s) && !NcParseLine("DFM", &s) && !NcParseLine("|https://a.b", &s) &&
          !NcParseLine("", &s), "no address, no name, empty");
    char line[600];
    NcStation f;
    NcParseLine("Jazz24|https://j.org/s|9f-1|\xD0\xA1\xD0\xA8\xD0\x90\x20\xC2\xB7" " MP3 128", &f);
    int n = NcFormatLine(f, line, sizeof(line));
    Check(n == (int)strlen(line) && !strcmp(line, "Jazz24|https://j.org/s|9f-1|\xD0\xA1\xD0\xA8\xD0\x90 \xC2\xB7 MP3 128"),
          "a favourite's line back");
    NcParseLine("DFM|http://a.b", &f);
    NcFormatLine(f, line, sizeof(line));
    Check(!strcmp(line, "DFM|http://a.b||"), "ours: empty id and second line");
}

static const char* kStations =
    "[{\"stationuuid\":\"u1\",\"name\":\" Jazz24 \",\"url\":\"http://old\",\"url_resolved\":\"https://j.org/s\","
    "\"countrycode\":\"US\",\"country\":\"The United States Of America\",\"codec\":\"MP3\",\"bitrate\":128,\"hls\":0},"
    "{\"stationuuid\":\"u2\",\"name\":\"Flac FM\",\"url_resolved\":\"https://f.org\",\"codec\":\"FLAC\",\"bitrate\":900,\"hls\":0},"
    "{\"stationuuid\":\"u3\",\"name\":\"HLS\",\"url_resolved\":\"https://h.org/p.m3u8\",\"codec\":\"AAC\",\"hls\":1},"
    "{\"stationuuid\":\"u4\",\"name\":\"Swiss\",\"url\":\"http://s.ch\",\"url_resolved\":\"\",\"countrycode\":\"XX\","
    "\"country\":\"Narnia\",\"codec\":\"aac+\",\"bitrate\":0,\"hls\":0},"
    "{\"stationuuid\":\"u5\",\"name\":\"\",\"url_resolved\":\"https://n.org\",\"codec\":\"MP3\"},"
    "{\"stationuuid\":\"u6\",\"name\":\"Ogg\",\"url_resolved\":\"rtmp://o.org\",\"codec\":\"OGG\"},"
    "{\"stationuuid\":\"u7\",\"name\":\"Ogg2\",\"url_resolved\":\"https://o.org\",\"codec\":\"OGG\",\"countrycode\":\"RU\",\"bitrate\":64}]";

static void TestStations()
{
    NcStation s[8];
    int n = NcStations(kStations, s, 8);
    Check(n == 3, "the playable ones: MP3, AAC+, OGG over http(s), with a name");
    Check(!wcscmp(s[0].name, L"Jazz24") && !strcmp(s[0].url, "https://j.org/s") && !strcmp(s[0].uuid, "u1"),
          "the resolved address, the id, the name trimmed");
    Check(!wcscmp(s[0].sub, L"США · MP3 128"), "the second line: the country, the codec, the bitrate");
    Check(!strcmp(s[1].url, "http://s.ch") && !wcscmp(s[1].sub, L"Narnia · AAC+"),
          "no resolved address: the plain one; an unknown country by its name; no bitrate");
    Check(!wcscmp(s[2].sub, L"Россия · OGG 64"), "a known country in Russian");
    Check(NcStations(kStations, s, 2) == 2, "cap at most");
    Check(NcStations("[]", s, 8) == 0 && NcStations("{\"error\":1}", s, 8) == 0 && NcStations("", s, 8) == 0,
          "an empty or wrong answer: none");
}

static void TestCats()
{
    NcCat c[4];
    int n = NcCats("[{\"name\":\"jazz\",\"stationcount\":1760},{\"name\":\"zz top fans\",\"stationcount\":3},"
                   "{\"name\":\"\",\"stationcount\":9},{\"name\":\"Rock\",\"stationcount\":7}]", NC_TAGS, c, 4);
    Check(n == 2 && !strcmp(c[0].key, "jazz") && !wcscmp(c[0].name, L"Джаз") && c[0].count == 1760, "a known tag");
    Check(!strcmp(c[1].key, "Rock") && !wcscmp(c[1].name, L"Рок") && c[1].count == 7,
          "not a genre (a band, a name, a country, a word) left out; a genre in any case kept");
    n = NcCats("[{\"name\":\"oldies\",\"stationcount\":90},{\"name\":\"m\\u00e9xico\",\"stationcount\":80},"
               "{\"name\":\"radio\",\"stationcount\":70},{\"name\":\"retro\",\"stationcount\":60},"
               "{\"name\":\"hiphop\",\"stationcount\":50},{\"name\":\"hip-hop\",\"stationcount\":40}]", NC_TAGS, c, 4);
    Check(n == 2 && !strcmp(c[0].key, "oldies") && !strcmp(c[1].key, "hiphop") && !wcscmp(c[1].name, L"Хип-хоп"),
          "one row a genre: the tag with more stations, the same genre's others left out");
    NcCats("[{\"name\":\"jazz\"}]", NC_TAGS, c, 4);
    Check(NcCats("[{\"name\":\"jazz\"}]", NC_TAGS, c, 4) == 1, "a genre against the rows of this answer only");
    n = NcCats("[{\"name\":\"Nowhere\",\"iso_3166_1\":\"\"},{\"name\":\"Russia\",\"iso_3166_1\":\"RU\"}]",
               NC_COUNTRIES, c, 4);
    Check(n == 1 && !strcmp(c[0].key, "RU"), "a country without its code left out");
    n = NcCats("[{\"name\":\"Russia\",\"iso_3166_1\":\"RU\",\"stationcount\":900},"
               "{\"name\":\"Narnia\",\"iso_3166_1\":\"XX\",\"stationcount\":2}]", NC_COUNTRIES, c, 4);
    Check(n == 2 && !strcmp(c[0].key, "RU") && !wcscmp(c[0].name, L"Россия") && !wcscmp(c[1].name, L"Narnia"),
          "countries: the code to ask, the Russian name or its own");
    n = NcCats("[{\"name\":\"russian\",\"iso_639\":\"ru\",\"stationcount\":1200}]", NC_LANGS, c, 4);
    Check(n == 1 && !strcmp(c[0].key, "russian") && !wcscmp(c[0].name, L"Русский"), "languages: asked by the name");
    Check(NcCats("[{\"name\":\"pop\",\"stationcount\":1},{\"name\":\"rock\",\"stationcount\":1}]", NC_TAGS, c, 1) == 1,
          "cap at most");
}

static void TestPaths()
{
    char p[300];
    NcStationsPath(NC_TAG, "hip hop", 20, false, p, sizeof(p));
    Check(!strcmp(p, "/json/stations/search?tag=hip%20hop&tagExact=true&order=clickcount&reverse=true"
                     "&hidebroken=true&limit=20&offset=20"), "a tag's second page, the most listened first");
    NcStationsPath(NC_COUNTRY, "RU", 0, true, p, sizeof(p));
    Check(!strcmp(p, "/json/stations/search?countrycode=RU&order=name&reverse=false&hidebroken=true&limit=20&offset=0"),
          "a country by the name");
    NcStationsPath(NC_LANG, "russian", 0, false, p, sizeof(p));
    Check(strstr(p, "?language=russian&languageExact=true&order=clickcount") != 0, "a language");
    NcStationsPath(NC_SEARCH, "\xD0\xB4\xD0\xB6\xD0\xB0\xD0\xB7", 0, false, p, sizeof(p));
    Check(strstr(p, "?name=%D0%B4%D0%B6%D0%B0%D0%B7&order=clickcount") != 0, "a search, UTF-8 encoded");
    NcStationsPath(NC_TOP, NULL, 0, false, p, sizeof(p));
    Check(!strcmp(p, "/json/stations/search?order=clickcount&reverse=true&hidebroken=true&limit=20&offset=0"),
          "the most listened of all");
    NcCatsPath(NC_TAGS, p, sizeof(p));
    Check(!strcmp(p, "/json/tags?order=stationcount&reverse=true&hidebroken=true&limit=100"), "the tags");
    NcCatsPath(NC_COUNTRIES, p, sizeof(p));
    Check(!strcmp(p, "/json/countries?order=stationcount&reverse=true&hidebroken=true"), "the countries");
    NcCatsPath(NC_LANGS, p, sizeof(p));
    Check(!strcmp(p, "/json/languages?order=stationcount&reverse=true&hidebroken=true&limit=100"), "the languages");
    NcClickPath("9f-1", p, sizeof(p));
    Check(!strcmp(p, "/json/url/9f-1"), "the click");
}

static void TestNames()
{
    wchar_t w[48];
    NcName(NC_TAGS, "Jazz", NULL, w, 48);
    Check(!wcscmp(w, L"Джаз"), "a tag whatever its case");
    NcName(NC_COUNTRIES, "us", "USA", w, 48);
    Check(!wcscmp(w, L"США"), "a country whatever its case");
    NcName(NC_LANGS, "klingon", NULL, w, 48);
    Check(!wcscmp(w, L"Klingon"), "an unknown language: a capital first letter");
    NcName(NC_TAGS, "", NULL, w, 48);
    Check(!w[0], "nothing: empty");
}

// The edges: long and cut fields, spaces and tabs, missing parts.
static void TestEdges()
{
    NcStation s;
    char line[700];
    memset(line, 'n', 70);
    strcpy(line + 70, "|http://x");
    Check(NcParseLine(line, &s) && wcslen(s.name) == 63, "a name longer than its room: cut");
    Check(NcParseLine("\tA\t|http://x \t\r\n", &s) && !wcscmp(s.name, L"A") && !strcmp(s.url, "http://x"),
          "tabs around the name, white space after the address");
    Check(!NcParseLine("   |http://x", &s), "a name of spaces only");
    Check(NcParseLine("a|http://x|id", &s) && !strcmp(s.uuid, "id") && !s.sub[0], "three fields");
    Check(NcParseLine("a|http://x|id|sub|more", &s) && !wcscmp(s.sub, L"sub|more"), "the fourth field takes the rest");
    strcpy(line, "a|http://");
    memset(line + 9, 'u', 450);
    line[459] = 0;
    Check(NcParseLine(line, &s) && strlen(s.url) == 399, "a long address cut to its room");
    NcParseLine("Jazz24|https://j.org/s|9f-1|x", &s);
    Check(NcFormatLine(s, line, 10) == 9 && !strcmp(line, "Jazz24|ht"), "a line back cut to its room");

    wchar_t w[48];
    NcName(NC_TAGS, "jazz", NULL, w, 3);
    Check(!wcscmp(w, L"Дж"), "a Russian name cut to its room");
    NcName(NC_TAGS, "abc", NULL, w, 48);
    Check(!wcscmp(w, L"Abc"), "a capital from a");
    NcName(NC_TAGS, "zed", NULL, w, 48);
    Check(!wcscmp(w, L"Zed"), "a capital from z");
    NcName(NC_TAGS, "`x", NULL, w, 48);
    Check(!wcscmp(w, L"`x"), "not a letter: as it is");
    NcName(NC_TAGS, "{x", NULL, w, 48);
    Check(!wcscmp(w, L"{x"), "not a letter after z: as it is");

    NcStation st[2];
    int n = NcStations("[{\"name\":\"A\",\"url_resolved\":\"https://a\",\"codec\":\"MP3\",\"bitrate\":1}]", st, 2);
    Check(n == 1 && !st[0].uuid[0] && !wcscmp(st[0].sub, L"MP3 1"),
          "no id: empty; no country: the stream only; bitrate 1");
    n = NcStations("[{\"name\":\"   \",\"url_resolved\":\"https://a\",\"codec\":\"MP3\"},"
                   "{\"name\":\"B\",\"url_resolved\":\"x\",\"url\":\"https://b\",\"codec\":\"MP3\"}]", st, 2);
    Check(n == 0, "a name of spaces; a resolved address that is no address");
    char cc[71], lng[400];
    memset(cc, 'c', 70);
    cc[70] = 0;
    _snprintf(lng, sizeof(lng), "[{\"name\":\"C\",\"url_resolved\":\"https://c\",\"codec\":\"mp3\",\"country\":\"%s\"}]", cc);
    n = NcStations(lng, st, 2);
    Check(n == 1 && wcslen(st[0].sub) == 53 && !wcsncmp(st[0].sub, L"Ccccc", 5) && !wcscmp(st[0].sub + 47, L" · MP3"),
          "a long country cut to 47 characters, the codec after it");

    NcCat c[2];
    n = NcCats("[{\"name\":\"jazz\"}]", NC_TAGS, c, 2);
    Check(n == 1 && c[0].count == 0, "no station count: 0");
    char p[10];
    NcCatsPath(NC_TAGS, p, sizeof(p));
    Check(strlen(p) == 9, "a path cut to its room");
    NcClickPath("abcdefghij", p, sizeof(p));
    Check(!strcmp(p, "/json/url"), "the click's path cut to its room");
    char big[300];
    NcStationsPath(NC_TAG, "x", 0, false, p, sizeof(p));
    Check(strlen(p) == 9, "a page's path cut to its room");
    NcStationsPath(NC_SEARCH, "a b", 0, true, big, sizeof(big));
    Check(strstr(big, "name=a%20b&order=name&reverse=false") != 0, "a search by the name");
}

static void TestCis()
{
    NcCat c[4];
    int n = NcCats("[{\"name\":\"Germany\",\"iso_3166_1\":\"DE\",\"stationcount\":3000},"
                   "{\"name\":\"Russia\",\"iso_3166_1\":\"RU\",\"stationcount\":900},"
                   "{\"name\":\"Latvia\",\"iso_3166_1\":\"LV\",\"stationcount\":90},"
                   "{\"name\":\"Kazakhstan\",\"iso_3166_1\":\"KZ\",\"stationcount\":80},"
                   "{\"name\":\"Tajikistan\",\"iso_3166_1\":\"TJ\",\"stationcount\":5}]", NC_CIS, c, 4);
    Check(n == 3 && !strcmp(c[0].key, "RU") && !wcscmp(c[0].name, L"Россия") && c[0].count == 900 &&
          !wcscmp(c[1].name, L"Казахстан") && !wcscmp(c[2].name, L"Таджикистан"), "the CIS: only its countries");
    char p[300];
    NcCatsPath(NC_CIS, p, sizeof(p));
    Check(!strcmp(p, "/json/countries?order=stationcount&reverse=true&hidebroken=true"), "the CIS: the countries");
}

static NcStation St(const char* url)
{
    NcStation s;
    memset(&s, 0, sizeof(s));
    strcpy(s.url, url);
    s.name[0] = (wchar_t)url[0];
    return s;
}

static void TestRecent()
{
    NcStation l[3];
    int n = NcRecentPut(l, 0, 3, St("a"));
    n = NcRecentPut(l, n, 3, St("b"));
    Check(n == 2 && !strcmp(l[0].url, "b") && !strcmp(l[1].url, "a"), "the latest first");
    n = NcRecentPut(l, n, 3, St("a"));
    Check(n == 2 && !strcmp(l[0].url, "a") && !strcmp(l[1].url, "b"), "played again: to the top, once");
    n = NcRecentPut(l, n, 3, St("c"));
    n = NcRecentPut(l, n, 3, St("d"));
    Check(n == 3 && !strcmp(l[0].url, "d") && !strcmp(l[1].url, "c") && !strcmp(l[2].url, "a"), "full: the oldest out");
    n = NcRecentPut(l, n, 3, St("a"));
    Check(n == 3 && !strcmp(l[0].url, "a") && !strcmp(l[1].url, "d") && !strcmp(l[2].url, "c"), "full, the last again");
}

static bool At(double lat, double lon, const char* want)
{
    char c[3];
    bool in = CountryAt(lat, lon, c);
    return want ? in && !strcmp(c, want) : !in;
}

static void TestCountry()
{
    Check(At(55.75, 37.62, "RU") && At(53.20, 50.15, "RU") && At(43.12, 131.90, "RU"), "Moscow, Samara, Vladivostok");
    Check(At(54.71, 20.51, "RU"), "Kaliningrad: Russia's other polygon");
    Check(At(53.90, 27.57, "BY") && At(43.24, 76.89, "KZ") && At(41.30, 69.24, "UZ") && At(40.18, 44.51, "AM"),
          "Minsk, Almaty, Tashkent, Yerevan");
    Check(At(60.17, 24.94, "FI") && At(59.91, 10.75, "NO") && At(48.86, 2.35, "FR"), "Helsinki; Oslo, Paris (no ISO_A2)");
    Check(At(-29.31, 27.48, "LS"), "Maseru: Lesotho, inside South Africa's hole");
    Check(At(45.0, -30.0, NULL) && At(-40.0, 90.0, NULL), "the sea: none");
    Check(At(23.8, 54.5, "AE") && At(-17.83, 31.05, "ZW"), "the table's first country and its last");
    static const short tri[3][2] = { { 0, 0 }, { 1000, 0 }, { 0, 1000 } };   // its long side: x + y = 1000
    Check(RingHas(tri, 3, 100, 100) && RingHas(tri, 3, 510, 480) && RingHas(tri, 3, 1, 998), "a ring: inside, by its sides");
    Check(!RingHas(tri, 3, 510, 500) && !RingHas(tri, 3, 990, 20) && !RingHas(tri, 3, -1, 500) &&
          !RingHas(tri, 3, 500, -1) && !RingHas(tri, 3, 2000, 10), "a ring: just outside each side, far east");
    static const short tri2[3][2] = { { 100, 200 }, { 1100, 200 }, { 100, 1200 } };   // the same, moved: x + y = 1300
    Check(RingHas(tri2, 3, 610, 680) && !RingHas(tri2, 3, 610, 700) && RingHas(tri2, 3, 1080, 210) &&
          !RingHas(tri2, 3, 1090, 220), "a ring off the origin: by its long side");
    char c[3] = "XX";
    Check(!CountryAt(45.0, -30.0, c) && !c[0], "the sea: no code");
}

int main()
{
    TestCountry();
    TestCis();
    TestRecent();
    TestEdges();
    TestLines();
    TestStations();
    TestCats();
    TestPaths();
    TestNames();
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
