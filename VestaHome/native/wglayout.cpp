// wglayout.cpp - see wglayout.h.
#include "wglayout.h"
#include <string.h>

enum { kCells = 6, kMaxPages = 16 };

// The cells a shape takes from cell c, or 0 when it does not fit there (the page's cells used[]).
static int Takes(int shape, int c, const bool* used, int* cells)
{
    int n = 0;
    cells[n++] = c;
    if (shape == WS_WIDE) {
        if (c % 3 == 2)
            return 0;
        cells[n++] = c + 1;
    } else if (shape == WS_BIG) {
        if (c)
            return 0;
        cells[n++] = 1;
        cells[n++] = 3;
        cells[n++] = 4;
    } else if (shape == WS_TALL) {
        if (c >= 3)
            return 0;
        cells[n++] = c + 3;
    }
    for (int i = 0; i < n; i++)
        if (used[cells[i]])
            return 0;
    return n;
}

int WgPlaceAll(const int* shapes, int n, int maxPages, WgCell* at)
{
    bool used[kMaxPages][kCells];
    memset(used, 0, sizeof(used));
    maxPages = maxPages > kMaxPages ? kMaxPages : maxPages;
    int pages = 1;
    for (int i = 0; i < n; i++) {
        at[i].page = at[i].cell = -1;
        if (shapes[i] == WS_HIDDEN)
            continue;
        for (int k = 0; k < maxPages * kCells && at[i].page < 0; k++) {
            int pg = k / kCells, c = k % kCells, cells[4];
            int m = Takes(shapes[i], c, used[pg], cells);
            if (!m)
                continue;
            for (int j = 0; j < m; j++)
                used[pg][cells[j]] = true;
            at[i].page = pg;
            at[i].cell = c;
            pages = pg + 1 > pages ? pg + 1 : pages;
        }
    }
    return pages;
}
