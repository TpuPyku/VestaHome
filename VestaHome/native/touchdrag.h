// touchdrag.h - a finger's drag in VestaHome (VestaCore vtouch.h filters the panel's points; the user 2026-11-23:
// "the touch smooth, as in the maps"): what it moves - a paged screen's page or a list - and whether the page turns
// when the finger lifts: dragged far enough sideways (60 px, HU 2026-10-24) or flicked sideways, towards the
// neighbour drawn beside it. Pure: the caller passes vtouch's way and speed.
#ifndef TOUCHDRAG_H
#define TOUCHDRAG_H

#include <math.h>

enum { DRAG_NONE, DRAG_PAGE, DRAG_LIST };
enum { kTurnPx = 60 };
static const double kFlickPxMs = 0.5;   // a flick: quicker than this (px/ms) - a page turns by it, however short

static bool Sideways(double x, double y)   // clearly sideways, not slanting (|x| > 2 |y|)
{
    return fabs(x) > 2 * fabs(y);
}

// dx, dy - the drag's way from the press's point; page - the screen turns pages, list - the press began on a list.
// DRAG_NONE: not yet (asked again at the next point)
static int DragWhat(int dx, int dy, bool page, bool list)
{
    if (page && Sideways(dx, dy))
        return DRAG_PAGE;
    return list ? DRAG_LIST : DRAG_NONE;
}

// The page the finger sent it to at the release: dx, dy - the page's way, vx, vy - the finger's speed then (px/ms;
// a flick decides before the way). 1 - the next (the finger went left), -1 - the previous, 0 - back
static int PageDir(int dx, int dy, double vx, double vy)
{
    if (fabs(vx) > kFlickPxMs && Sideways(vx, vy))
        return vx < 0 ? 1 : -1;
    if ((dx > kTurnPx || dx < -kTurnPx) && Sideways(dx, dy))
        return dx < 0 ? 1 : -1;
    return 0;
}

// The same, turning only towards nb - the neighbour drawn beside the page (0: none, an end)
static int PageTurn(int dx, int dy, double vx, double vy, int nb)
{
    int d = PageDir(dx, dy, vx, vy);
    return d == nb ? d : 0;
}

// A still finger's wander: the farthest of its n raw points from the settled point px, py (px, rounded)
static int TouchWander(const POINT* p, int n, int px, int py)
{
    double most = 0;
    for (int i = 0; i < n; i++) {
        double dx = p[i].x - px, dy = p[i].y - py, d = sqrt(dx * dx + dy * dy);
        if (d > most)
            most = d;
    }
    return (int)floor(most + 0.5);
}

#endif
