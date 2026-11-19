// radiodb.h - the FM station by the city and the frequency, and its logo by the station's name (the user 2026-11-18/19:
// RDS in the CIS often gives no name or a wrong one; the logos as a set of their own, each file named by its
// station - anyone can add their own). The table is RadioLogos\stations.txt from VestaHome/tools/make_radio.py: a
// station a line "lat;lon;f10;name" (UTF-8). Pure: no files, no UI.
#pragma once
#include <wchar.h>

enum { RDB_NEAR = 400, RDB_KM = 100, RDB_NAME = 40, RDB_KEY = 40 };

struct RdbStation { int f10; float lat, lon; wchar_t name[RDB_NAME]; };
struct Rdb {
    RdbStation s[RDB_NEAR];   // the table's stations within RDB_KM of (lat, lon), the nearest kept when more
    float km[RDB_NEAR];       // their distances from there
    int n;
    double lat, lon;          // where they were taken for
};

// The table's text (UTF-8, NUL-ended) -> the stations within RDB_KM of lat, lon. Bad lines skipped.
void RdbLoad(Rdb* d, const char* text, double lat, double lon);
// Taken again when the place is RDB_KM / 4 or more from where they were taken for.
bool RdbStale(const Rdb* d, double lat, double lon);
// The station on f10 nearest to lat, lon within RDB_KM; NULL none.
const wchar_t* RdbName(const Rdb* d, int f10, double lat, double lon);

// A station's name as matched - the table's, RDS's, a logo file's alike (make_radio.py's name_key, the same):
// Latin letters and digits, Cyrillic spelt in Latin, a leading "радио" word and a trailing "радио" / "FM" word left
// off (unless nothing else is left; "Авторадио" one word: its "радио" too). key: RDB_KEY; "" when none.
void RdbKey(const wchar_t* name, char* key);

double RdbKm(double lat1, double lon1, double lat2, double lon2);

// The logos not in the set, from the net (the user 2026-11-19): radio-browser.info's search of a station's name in
// a country (its code, "RU"): the path; "" no name or no room.
void RdbNetPath(const wchar_t* name, const char* country, char* out, int cap);
// The answer's first station whose name has name's key, with a picture by http(s) and no port: its host, path
// ("/" at least) and whether by https; false none.
bool RdbNetLogo(const char* json, const wchar_t* name, char* host, int hostCap, char* path, int pathCap, bool* tls);
// The names not found ("key;day" lines, day: days since 2000): key missed fewer than RDB_MISS_DAYS before day.
enum { RDB_MISS_DAYS = 30 };
bool RdbMissed(const char* text, const char* key, int day);
