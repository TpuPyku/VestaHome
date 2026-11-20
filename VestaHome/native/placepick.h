// placepick.h - where the unit is (the radio's station table, the weather, the sun, the speed cameras): the fix,
// else the last place the GPS told - kept over restarts, as OSMapsCE does (the user 2026-11-20: the tester's radio
// had no logos - no fix, no home point) - else the home point. Pure, header-only (test_placepick).
#ifndef PLACEPICK_H
#define PLACEPICK_H

#include <math.h>

enum { PLACE_NONE, PLACE_GPS, PLACE_LAST, PLACE_HOME };

// 0, 0 - a place not set (the last, the home). lat / lon untouched with PLACE_NONE.
static int PlacePick(bool fix, double fixLat, double fixLon, double lastLat, double lastLon, double homeLat,
                     double homeLon, double* lat, double* lon)
{
    int src = fix ? PLACE_GPS : lastLat != 0 || lastLon != 0 ? PLACE_LAST : homeLat != 0 || homeLon != 0 ? PLACE_HOME
                                                                                                         : PLACE_NONE;
    if (src == PLACE_GPS)
        *lat = fixLat, *lon = fixLon;
    else if (src == PLACE_LAST)
        *lat = lastLat, *lon = lastLon;
    else if (src == PLACE_HOME)
        *lat = homeLat, *lon = homeLon;
    return src;
}

// The last place to be written again (the ini on the flash): none kept yet, or the car 2 km and more from it.
static bool PlaceMoved(double keptLat, double keptLon, double lat, double lon)
{
    if (keptLat == 0 && keptLon == 0)
        return true;
    double dy = (lat - keptLat) * 111.2, dx = (lon - keptLon) * 111.2 * cos(keptLat * 3.14159265 / 180);
    return dx * dx + dy * dy >= 4.0;
}

#endif
