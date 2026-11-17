// webcover.cpp - see webcover.h.
#include "webcover.h"
#include <stdio.h>
#include <string.h>

static int Encode(const char* s, char* out, int at, int cap)   // URL-encoded, spaces as +; returns the new end
{
    for (const unsigned char* p = (const unsigned char*)s; *p && at < cap - 4; p++) {
        unsigned c = *p;
        bool plain = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ||
                     c == '_' || c == '.' || c == '~';
        if (plain)
            out[at++] = (char)c;
        else if (c == ' ')
            out[at++] = '+';
        else
            at += _snprintf(out + at, cap - at, "%%%02X", c);
    }
    out[at] = 0;
    return at;
}

// head, "artist title" encoded, tail - cut to cap (the tail kept whole)
static void SearchPath(const char* head, const char* artist, const char* title, const char* kTail, char* path, int cap)
{
    int tail = (int)strlen(kTail);
    int at = _snprintf(path, cap, "%s", head);
    if (at < 0 || at >= cap - tail - 1) {
        path[cap - 1] = 0;
        return;
    }
    int room = cap - tail - 1;
    at = Encode(artist, path, at, room);
    if (artist[0] && title[0] && at < room - 1)
        path[at++] = '+';
    at = Encode(title, path, at, room);
    strcpy(path + at, kTail);
}

void WebCoverSearch(const char* artist, const char* title, char* path, int cap)
{
    SearchPath("/search?term=", artist, title, "&entity=song&limit=1&country=ru", path, cap);
}

void YandexSearch(const char* artist, const char* title, char* path, int cap)
{
    SearchPath("/search?text=", artist, title, "&type=track&page=0&pageSize=1", path, cap);
}

bool YandexArt(const char* json, char* host, int hostCap, char* path, int pathCap)
{
    static const char* kKey = "\"coverUri\":\"";
    const char* p = json ? strstr(json, kKey) : NULL;
    if (!p)
        return false;
    p += strlen(kKey);
    const char* end = strchr(p, '"');
    const char* slash = end ? strchr(p, '/') : NULL;
    if (!slash || slash > end || slash - p >= hostCap)
        return false;
    memcpy(host, p, slash - p);
    host[slash - p] = 0;
    int n = 0;
    bool sized = false;
    for (const char* q = slash; q < end && n < pathCap - 8; q++) {
        if (q[0] == '%' && q[1] == '%') {   // (the size asked for goes in place of %%)
            memcpy(path + n, "300x300", 7);
            n += 7;
            q++;
            sized = true;
        } else {
            path[n++] = *q;
        }
    }
    path[n] = 0;
    return sized;
}

bool WebCoverArt(const char* json, char* host, int hostCap, char* path, int pathCap)
{
    static const char* kKey = "\"artworkUrl100\":\"";
    const char* p = json ? strstr(json, kKey) : NULL;
    if (!p)
        return false;
    p += strlen(kKey);
    char url[512];
    int n = 0;
    for (; *p && *p != '"' && n < (int)sizeof(url) - 1; p++)
        if (!(p[0] == '\\' && p[1] == '/'))   // (JSON may write / as \/)
            url[n++] = *p;
    url[n] = 0;
    if (*p != '"' || strncmp(url, "https://", 8))
        return false;
    const char* h = url + 8;
    const char* slash = strchr(h, '/');
    if (!slash || slash - h >= hostCap)
        return false;
    memcpy(host, h, slash - h);
    host[slash - h] = 0;
    char* size = strstr(url, "100x100bb");   // (the same picture, larger)
    if (size)
        memcpy(size, "300x300bb", 9);
    if ((int)strlen(slash) >= pathCap)
        return false;
    strcpy(path, slash);
    return true;
}

bool RadioSplit(const char* line, char* artist, char* title, int cap)
{
    const char* d = strstr(line, " - ");
    if (!d || d == line || !d[3])
        return false;
    int a = (int)(d - line) < cap - 1 ? (int)(d - line) : cap - 1;
    memcpy(artist, line, a);
    artist[a] = 0;
    strncpy(title, d + 3, cap - 1);
    title[cap - 1] = 0;
    return true;
}

unsigned long WebCoverKey(const char* artist, const char* title)
{
    unsigned long c = 0xFFFFFFFFUL;
    const char* parts[2] = { artist, title };
    for (int k = 0; k < 2; k++) {
        for (const unsigned char* p = (const unsigned char*)parts[k]; ; p++) {
            unsigned b = *p >= 'A' && *p <= 'Z' ? *p + 32 : *p;   // (the zero ends each half: kept apart)
            c ^= b;
            for (int i = 0; i < 8; i++)
                c = c & 1 ? (c >> 1) ^ 0xEDB88320UL : c >> 1;
            if (!*p)
                break;
        }
    }
    return ~c & 0xFFFFFFFFUL;
}
