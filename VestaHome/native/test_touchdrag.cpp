// test_touchdrag.cpp - what a finger's drag moves and whether the page turns at the release (touchdrag.h), on the PC.
#include <windows.h>
#include <stdio.h>
#include "touchdrag.h"

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
    // what the drag moves (its way from the press's point)
    Check(DragWhat(30, 10, true, false) == DRAG_PAGE, "sideways on a paged screen: the page");
    Check(DragWhat(-30, 14, true, true) == DRAG_PAGE, "clearly sideways over a list on a paged screen: the page");
    Check(DragWhat(30, 15, true, false) == DRAG_NONE, "not clearly sideways (2 x |dy| = |dx|): nothing yet");
    Check(DragWhat(30, 15, true, true) == DRAG_LIST, "not clearly sideways over a list: the list");
    Check(DragWhat(2, -25, false, true) == DRAG_LIST, "up over a list: the list");
    Check(DragWhat(30, 0, false, true) == DRAG_LIST, "sideways over a list on a screen without pages: the list");
    Check(DragWhat(30, 0, false, false) == DRAG_NONE, "sideways, no pages, no list: nothing");
    Check(DragWhat(0, 30, true, false) == DRAG_NONE, "up on a paged screen without a list: nothing");
    // the page's way at the release: far enough sideways
    Check(PageDir(-61, 0, 0, 0) == 1, "dragged left past 60 px: the next page");
    Check(PageDir(61, 30, 0, 0) == -1, "dragged right past 60 px, sideways: the previous page");
    Check(PageDir(-60, 0, 0, 0) == 0, "60 px left: not yet");
    Check(PageDir(60, 0, 0, 0) == 0, "60 px right: not yet");
    Check(PageDir(-80, 40, 0, 0) == 0, "past 60 px but not clearly sideways: back");
    // ... or flicked: quick enough and sideways
    Check(PageDir(-10, 0, -0.6, 0) == 1, "a short quick flick left: the next page");
    Check(PageDir(10, 0, 0.6, 0.29) == -1, "a short quick flick right, sideways: the previous page");
    Check(PageDir(-10, 0, -0.5, 0) == 0, "0.5 px/ms: not a flick");
    Check(PageDir(-10, 0, -0.6, 0.3) == 0, "quick but not clearly sideways: back");
    Check(PageDir(0, -10, 0.1, -2.0) == 0, "a vertical flick: no page");
    Check(PageDir(-100, 0, 0.8, 0) == -1, "dragged left, flicked back right: the flick decides");
    Check(PageDir(-100, 0, 0.3, 0) == 1, "dragged left, a slow lift back: the way decides");
    // only towards the neighbour drawn beside the page (0: an end)
    Check(PageTurn(-80, 0, 0, 0, 1) == 1, "towards the next page shown: turns");
    Check(PageTurn(80, 0, 0.7, 0, -1) == -1, "towards the previous page shown: turns");
    Check(PageTurn(-80, 0, -0.7, 0, 0) == 0, "at the end (no neighbour): back");
    Check(PageTurn(-80, 0, 0.7, 0, 1) == 0, "flicked away from the neighbour shown: back");
    Check(PageTurn(-10, 0, 0, 0, 1) == 0, "neither far nor quick: back");
    // a still finger's wander: the farthest raw point from the settled one
    POINT p[4] = { { 100, 100 }, { 104, 97 }, { 98, 106 }, { 101, 100 } };
    Check(TouchWander(p, 4, 100, 100) == 6, "the farthest point 6 px away (rounded)");
    Check(TouchWander(p, 2, 100, 100) == 5, "only the first n points");
    Check(TouchWander(p, 0, 100, 100) == 0, "no points: 0");
    POINT q[1] = { { 103, 102 } };
    Check(TouchWander(q, 1, 100, 100) == 4, "3, 2 px away: 3.6 -> 4");
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
