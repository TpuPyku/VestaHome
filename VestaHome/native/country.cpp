// country.cpp - the country of a point. See country.h.
#include "country.h"
#include <string.h>

struct CountryRing {
    char  code[3];
    int   first, count;            // its points in kCountryPt
    short minLon, maxLon, minLat, maxLat;   // its box, hundredths of a degree
};
#include "countries.inc"

enum { kRings = sizeof(kCountryRing) / sizeof(kCountryRing[0]) };

bool RingHas(const short (*p)[2], int n, int x, int y)   // even-odd: a ray to the east, the edges it crosses
{
    bool in = false;
    for (int i = 0, j = n - 1; i < n; j = i++)
        if ((p[i][1] > y) != (p[j][1] > y) &&
            x < p[j][0] + (double)(p[i][0] - p[j][0]) * (y - p[j][1]) / (p[i][1] - p[j][1]))
            in = !in;
    return in;
}

static bool InRing(const CountryRing& r, int x, int y)   // (its box first: most rings are far)
{
    if (x < r.minLon || x > r.maxLon || y < r.minLat || y > r.maxLat)
        return false;
    return RingHas(kCountryPt + r.first, r.count, x, y);
}

bool CountryAt(double lat, double lon, char code[3])
{
    int x = (int)(lon * 100), y = (int)(lat * 100);   // (0.01 degree: finer than the borders)
    for (int i = 0; i < kRings;) {   // a country's rings together: inside an odd number of them (a hole: even)
        int k = i;
        bool in = false;
        for (; k < kRings && !strcmp(kCountryRing[k].code, kCountryRing[i].code); k++)
            if (InRing(kCountryRing[k], x, y))
                in = !in;
        if (in) {
            strcpy(code, kCountryRing[i].code);
            return true;
        }
        i = k;
    }
    code[0] = 0;
    return false;
}
