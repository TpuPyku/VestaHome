// wglayout.cpp - where the main screen's widgets go: a page is 3 x 2 cells (cell = row * 3 + column); each shown
// widget, in its order, into the first free place of its shape, so the later ones close up the gaps.
#pragma once

enum WgShape {
    WS_HIDDEN = -1,
    WS_ONE,    // a single cell
    WS_WIDE,   // two cells side by side in a row
    WS_BIG,    // the page's left 2 x 2
    WS_TALL    // a cell and the one under it (a column)
};

struct WgCell { int page, cell; };   // page -1: not placed (hidden, or no room)

// shapes[i] of widget i in their order; at[i] its place. Returns the pages used (at least 1).
int WgPlaceAll(const int* shapes, int n, int maxPages, WgCell* at);
