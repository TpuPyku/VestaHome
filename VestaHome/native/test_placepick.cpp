// placepick.h: where the unit is for the radio's table, the weather, the sun and the cameras - the fix, else the last
// place the GPS told (kept over restarts, as OSMapsCE does), else the home point; the last place written only when
// it moved (the flash).
#include <windows.h>
#include "placepick.h"
#include <stdio.h>

static int s_checks, s_failed;

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
    double lat = -1, lon = -1;
    Check(PlacePick(true, 55.75, 37.62, 59.9, 30.3, 43.2, 76.9, &lat, &lon) == PLACE_GPS && lat == 55.75 && lon == 37.62,
          "a fix: the fix");
    Check(PlacePick(false, 0, 0, 59.9, 30.3, 43.2, 76.9, &lat, &lon) == PLACE_LAST && lat == 59.9 && lon == 30.3,
          "no fix: the last place before the home point");
    Check(PlacePick(false, 0, 0, 0, 30.3, 43.2, 76.9, &lat, &lon) == PLACE_LAST && lat == 0 && lon == 30.3,
          "the last place on the equator: still a place");
    Check(PlacePick(false, 0, 0, 0, 0, 43.2, 76.9, &lat, &lon) == PLACE_HOME && lat == 43.2 && lon == 76.9,
          "no fix, no last place: the home point");
    Check(PlacePick(false, 0, 0, 0, 0, 0, 76.9, &lat, &lon) == PLACE_HOME && lon == 76.9, "the home on the equator");
    lat = lon = -1;
    Check(PlacePick(false, 0, 0, 0, 0, 0, 0, &lat, &lon) == PLACE_NONE && lat == -1 && lon == -1, "nothing: none");
    Check(PLACE_NONE == 0 && PLACE_GPS != PLACE_LAST && PLACE_LAST != PLACE_HOME && PLACE_GPS != PLACE_HOME,
          "the sources apart, none 0");

    // the last place written: none yet, or 2 km and more away
    Check(PlaceMoved(0, 0, 55.75, 37.62), "none kept yet: written");
    Check(!PlaceMoved(0, 30.3, 0, 30.31) && !PlaceMoved(30.3, 0, 30.31, 0), "kept on the equator / the meridian: a "
          "place, 1.1 km - not written");
    Check(!PlaceMoved(55.75, 37.62, 55.76, 37.62), "1.1 km north: not written");
    Check(PlaceMoved(55.75, 37.62, 55.77, 37.62), "2.2 km north: written");
    Check(!PlaceMoved(55.75, 37.62, 55.75, 37.65), "1.9 km east at 55.75: not written (the degree shorter there)");
    Check(PlaceMoved(55.75, 37.62, 55.75, 37.66), "2.5 km east: written");
    Check(PlaceMoved(55.75, 37.62, 55.73, 37.62) && PlaceMoved(55.75, 37.62, 55.75, 37.58), "south and west: the same");
    printf("test_placepick: %d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
