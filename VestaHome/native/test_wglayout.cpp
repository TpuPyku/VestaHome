// test_wglayout.cpp - the main screen's widgets in their places (wglayout.cpp) on the PC (test_wglayout.bat).
#include <stdio.h>
#include "wglayout.h"

static int s_failed, s_checks;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAIL: %s\n", what);
    }
}

static bool At(const WgCell& c, int page, int cell)
{
    return c.page == page && c.cell == cell;
}

int main()
{
    WgCell at[8];
    {   // the map (big) and the tall "now playing": one page, the column on the right
        int s[2] = { WS_BIG, WS_TALL };
        Check(WgPlaceAll(s, 2, 6, at) == 1, "map + tall: one page");
        Check(At(at[0], 0, 0) && At(at[1], 0, 2), "map + tall: the tall one in the right column");
    }
    {   // the tall one first: the first column; then a big one does not fit there - the next page
        int s[3] = { WS_TALL, WS_BIG, WS_ONE };
        Check(WgPlaceAll(s, 3, 6, at) == 2, "tall, big: two pages");
        Check(At(at[0], 0, 0), "tall first: the left column");
        Check(At(at[1], 1, 0), "a big one after a tall one: the next page's left");
        Check(At(at[2], 0, 1), "a single closes the gap on the first page");
    }
    {   // two talls and a single fill a page
        int s[3] = { WS_TALL, WS_TALL, WS_TALL };
        Check(WgPlaceAll(s, 3, 6, at) == 1 && At(at[0], 0, 0) && At(at[1], 0, 1) && At(at[2], 0, 2),
              "three talls: the three columns");
    }
    {   // a tall one needs the cell under it free
        int s[4] = { WS_ONE, WS_ONE, WS_ONE, WS_TALL };
        WgPlaceAll(s, 4, 6, at);
        Check(At(at[3], 1, 0), "the top row taken: a tall one on the next page");
        int t[5] = { WS_ONE, WS_ONE, WS_ONE, WS_ONE, WS_TALL };
        WgPlaceAll(t, 5, 6, at);
        Check(At(at[4], 1, 0), "the bottom row started: no column free on the page - the next one");
    }
    {   // as before: singles, a wide one, a big one, hidden ones
        int s[5] = { WS_ONE, WS_WIDE, WS_HIDDEN, WS_ONE, WS_BIG };
        Check(WgPlaceAll(s, 5, 6, at) == 2, "the old shapes: two pages");
        Check(At(at[0], 0, 0) && At(at[1], 0, 1) && At(at[3], 0, 3), "a single, a wide one beside it, a single below");
        Check(at[2].page == -1, "hidden: not placed");
        Check(At(at[4], 1, 0), "a big one: the next page's left");
        int w[2] = { WS_ONE, WS_ONE };
        WgPlaceAll(w, 2, 6, at);
        int x[3] = { WS_ONE, WS_ONE, WS_WIDE };
        WgPlaceAll(x, 3, 6, at);
        Check(At(at[2], 0, 3), "a wide one does not go over the row's end: the next row");
    }
    {   // the cells each shape takes, seen by the singles after it
        int s[4] = { WS_TALL, WS_ONE, WS_ONE, WS_ONE };
        WgPlaceAll(s, 4, 6, at);
        Check(At(at[1], 0, 1) && At(at[2], 0, 2) && At(at[3], 0, 4), "a tall one takes the cell under it");
        int b[4] = { WS_BIG, WS_ONE, WS_ONE, WS_ONE };
        WgPlaceAll(b, 4, 6, at);
        Check(At(at[1], 0, 2) && At(at[2], 0, 5) && At(at[3], 1, 0), "a big one takes four cells");
        int w[6] = { WS_ONE, WS_ONE, WS_ONE, WS_ONE, WS_ONE, WS_WIDE };
        WgPlaceAll(w, 6, 6, at);
        Check(At(at[5], 1, 0), "a wide one not at the second row's end: the next page");
        int o[7] = { WS_ONE, WS_ONE, WS_ONE, WS_ONE, WS_ONE, WS_ONE, WS_ONE };
        Check(WgPlaceAll(o, 7, 6, at) == 2 && At(at[6], 1, 0), "six cells a page: the seventh on the next");
    }
    {   // the pages run out
        int s[3] = { WS_BIG, WS_BIG, WS_BIG };
        Check(WgPlaceAll(s, 3, 2, at) == 2 && at[2].page == -1, "no page left: not placed");
        Check(WgPlaceAll(s, 0, 2, at) == 1, "none: one page");
    }
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
