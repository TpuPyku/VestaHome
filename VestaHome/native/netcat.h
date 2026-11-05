// netcat.h - the internet radio's stations: the lines of radio.txt / radio_fav.txt and the radio-browser.info
// catalogue (its requests' paths, its answers read into stations and categories, Russian names for its English
// ones). Pure: no network, no files, no screen (vh_netradio.inc does those).
#pragma once
#include <windows.h>

// stations a catalogue page asks for (HU 2026-11-05: 50 = 57 KB - radio-browser's server sends answers over
// ~32 KB chunked and stalls on them over BT and 3G; 20 = ~23 KB, sent with its length)
enum { kNcPage = 20 };

struct NcStation {
    wchar_t name[64];
    char    url[400];
    char    uuid[40];      // the catalogue's id ("" ours): its click is counted when it plays
    wchar_t sub[64];       // the second line: "США · MP3 128", "" none
};

// One line "name|address[|catalogue id|second line]" (UTF-8, as radio.txt and radio_fav.txt hold them); false: a
// comment (#), no name, or the address is not http:// / https://.
bool NcParseLine(const char* line, NcStation* out);
int  NcFormatLine(const NcStation& s, char* out, int cap);   // the line back (for radio_fav.txt), its length

// A catalogue answer (a JSON array of stations): the ones VestaHome can play (MP3, AAC, AAC+, OGG; not HLS),
// in its order, cap at most.
int NcStations(const char* json, NcStation* out, int cap);

// The station played to the top of the recent ones (the latest first; once each, by its address; full: the oldest
// goes); the list's new length.
int NcRecentPut(NcStation* list, int n, int cap, const NcStation& s);

// The category lists; NC_CIS: the countries, only the CIS ones (the user 2026-11-04: "at least the CIS").
enum { NC_TAGS, NC_COUNTRIES, NC_LANGS, NC_CIS };
struct NcCat {
    char    key[64];       // what a request takes: the tag, the country's code, the language
    wchar_t name[48];      // shown: Russian if known
    int     count;         // its stations
};
int  NcCats(const char* json, int kind, NcCat* out, int cap);
void NcCatsPath(int kind, char* out, int cap);

enum { NC_TOP, NC_TAG, NC_COUNTRY, NC_LANG, NC_SEARCH };   // what a page of stations is
// A page's path: kNcPage stations from `offset`, the most listened first (byName: by the name).
void NcStationsPath(int kind, const char* arg, int offset, bool byName, char* out, int cap);
void NcClickPath(const char* uuid, char* out, int cap);   // the click (radio-browser counts it, gives the address)

// Russian for the catalogue's English: a tag ("jazz" -> "Джаз"), a country by its code ("RU" -> "Россия"; unknown -
// `name` as it is), a language ("russian" -> "Русский"); others with a capital first letter.
void NcName(int kind, const char* key, const char* name, wchar_t* out, int cap);
