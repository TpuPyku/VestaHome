// test_webcover.cpp - a song's cover from the net (webcover.cpp) on the PC (test_webcover.bat): the search's request,
// its answer read, the radio's "Artist - Title" split, the cache's name.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "webcover.h"

static int s_failed, s_checks;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAIL: %s\n", what);
    }
}

int main()
{
    char p[512];
    WebCoverSearch("Queen", "Bohemian Rhapsody", p, sizeof(p));
    Check(!strcmp(p, "/search?term=Queen+Bohemian+Rhapsody&entity=song&limit=1&country=ru"),
          "the search: the artist and the title, spaces as +, the Russian store");
    WebCoverSearch("\xD0\x9A\xD0\xB8\xD0\xBD\xD0\xBE", "A&B?=#%", p, sizeof(p));   // "Кино"
    Check(!strcmp(p, "/search?term=%D0%9A%D0%B8%D0%BD%D0%BE+A%26B%3F%3D%23%25&entity=song&limit=1&country=ru"),
          "the search: UTF-8 and the URL's own signs encoded");
    WebCoverSearch("", "Song", p, sizeof(p));
    Check(!strcmp(p, "/search?term=Song&entity=song&limit=1&country=ru"), "no artist: the title alone");
    WebCoverSearch("A very long artist name", "and a very long title too", p, 40);
    Check(strlen(p) < 40, "a small room: cut, never past it");

    const char* json = "{\n \"resultCount\":1,\n \"results\": [\n{\"wrapperType\":\"track\", \"artistName\":\"Kino\", "
                       "\"artworkUrl60\":\"https://is1-ssl.mzstatic.com/a/60x60bb.jpg\", "
                       "\"artworkUrl100\":\"https://is1-ssl.mzstatic.com/image/thumb/Music/c7/cover.jpg/100x100bb.jpg\"}]}";
    char host[64], path[256];
    Check(WebCoverArt(json, host, sizeof(host), path, sizeof(path)) && !strcmp(host, "is1-ssl.mzstatic.com") &&
          !strcmp(path, "/image/thumb/Music/c7/cover.jpg/300x300bb.jpg"),
          "the answer: the picture's host and path, 300 x 300 asked instead of 100");
    Check(!WebCoverArt("{\"resultCount\":0,\"results\": []}", host, sizeof(host), path, sizeof(path)),
          "nothing found: none");
    Check(!WebCoverArt("{\"artworkUrl100\":\"http://x/1.jpg\"}", host, sizeof(host), path, sizeof(path)),
          "not https: none");
    Check(!WebCoverArt("{\"artworkUrl100\":\"https://x", host, sizeof(host), path, sizeof(path)), "cut short: none");
    Check(!WebCoverArt(NULL, host, sizeof(host), path, sizeof(path)), "no answer: none");
    Check(WebCoverArt("{\"artworkUrl100\":\"https://h\\/a\\/b.jpg\"}", host, sizeof(host), path, sizeof(path)) &&
          !strcmp(host, "h") && !strcmp(path, "/a/b.jpg"), "JSON's escaped slashes");

    // Yandex Music first (its servers in Russia, the user 17.11), iTunes when it has nothing
    YandexSearch("Queen", "Bohemian Rhapsody", p, sizeof(p));
    Check(!strcmp(p, "/search?text=Queen+Bohemian+Rhapsody&type=track&page=0&pageSize=1"),
          "Yandex's search: one track (4 KB, not 70)");
    const char* yj = "{\"result\":{\"tracks\":{\"results\":[{\"title\":\"Bohemian Rhapsody\",\"coverUri\":"
                     "\"avatars.yandex.net/get-music-content/4304260/c35255ac.a.227551-3/%%\"}]}}}";
    Check(YandexArt(yj, host, sizeof(host), path, sizeof(path)) && !strcmp(host, "avatars.yandex.net") &&
          !strcmp(path, "/get-music-content/4304260/c35255ac.a.227551-3/300x300"),
          "Yandex's answer: the cover's host and path, %% as 300x300");
    Check(!YandexArt("{\"result\":{\"tracks\":{\"results\":[]}}}", host, sizeof(host), path, sizeof(path)),
          "Yandex found nothing: none");
    Check(!YandexArt("{\"coverUri\":\"avatars.yandex.net/a/b\"}", host, sizeof(host), path, sizeof(path)),
          "no size mark in it: none");
    Check(!YandexArt(NULL, host, sizeof(host), path, sizeof(path)), "Yandex, no answer: none");

    char a[64], t[64];
    Check(RadioSplit("Fleetwood Mac - Coming Your Way", a, t, 64) && !strcmp(a, "Fleetwood Mac") &&
          !strcmp(t, "Coming Your Way"), "the radio's line: artist - title");
    Check(RadioSplit("AC-DC - Back In Black - Live", a, t, 64) && !strcmp(a, "AC-DC") && !strcmp(t, "Back In Black - Live"),
          "split at the first spaced dash");
    Check(!RadioSplit("Radio Paradise Main Mix", a, t, 64), "no dash: not a song (the station's name)");
    Check(!RadioSplit(" - Title", a, t, 64) && !RadioSplit("Artist - ", a, t, 64), "an empty half: none");

    Check(WebCoverKey("Kino", "Group") == WebCoverKey("KINO", "group"), "the cache's name: any letter case");
    Check(WebCoverKey("Kino", "Group") != WebCoverKey("Kin", "oGroup"), "the cache's name: the halves kept apart");
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
