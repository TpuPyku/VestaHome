// slide.h - a page sliding over a still background (the user 2026-11-24: the background must not move with the
// pages; the frame used to be shifted whole, the background with it).
#pragma once
#include <windows.h>

// One row of the band [l, r) with the page a shifted by x (right > 0) and its neighbour b beside it (the one before
// when x > 0, after when x < 0; NULL - none, an end): out[l..r). A page's pixel that is the background where it
// stands (bg, the same row of the still background) is not the page's - the background where it lands shows instead.
inline void SlideRow(DWORD* out, const DWORD* a, const DWORD* b, const DWORD* bg, int l, int r, int x)
{
    int w = r - l;
    for (int px = l; px < r; px++) {
        int p = px - l - x;   // the column in the band of the page that lands here
        const DWORD* src = a;
        if (p < 0) {   // the one before
            p += w;
            src = b;
        } else if (p >= w) {   // the one after
            p -= w;
            src = b;
        }
        int col = l + p;
        out[px] = src && src[col] != bg[col] ? src[col] : bg[px];
    }
}
