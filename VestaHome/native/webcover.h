// webcover.cpp - a song's cover from the net (2026-11-17, PLAN.md: the internet radio's and Bluetooth music's covers):
// iTunes Search (itunes.apple.com, the Russian store: fast from Russia, no key, DigiCert Global Root G2 - a root
// VestaCore's vhttp has), its picture at 300 x 300 (is*-ssl.mzstatic.com). Only the text work here; vh_cover.inc asks,
// downloads and keeps the pictures. UTF-8 throughout.
#pragma once

// The search's path for artist and title (either may be empty), cut to cap.
void WebCoverSearch(const char* artist, const char* title, char* path, int cap);

// The first result's picture in the search's answer: its https host and path, the 300 x 300 one; false - none.
bool WebCoverArt(const char* json, char* host, int hostCap, char* path, int pathCap);

// Yandex Music (api.music.yandex.net, no sign-in; the user 17.11: faster from Russia, a smaller catalogue - asked
// first, iTunes after it): its search's path for one track, and its answer's cover (avatars.yandex.net, 300 x 300).
void YandexSearch(const char* artist, const char* title, char* path, int cap);
bool YandexArt(const char* json, char* host, int hostCap, char* path, int pathCap);

// The internet radio's line "Artist - Title" (split at the first " - "); false - not a song's (a station's name).
bool RadioSplit(const char* line, char* artist, char* title, int cap);

// The cache's name of a song: the same for any letter case (ASCII), the artist and the title kept apart.
unsigned long WebCoverKey(const char* artist, const char* title);
