// slide.h - a page sliding over a still background (the user 2026-11-24: the background must not move with the
// pages; the frame used to be shifted whole, the background with it).
#pragma once
#include <windows.h>

// A glass card's pixel p (the skin's card picture blended at alpha over bgFrom, cardpic.h CardBlend) seen over bgTo
// instead (the user 2026-11-24: the cards' transparency real while sliding - the hills under a card were its old
// place's): p + (bgTo - bgFrom) * (255 - alpha) / 255 for each channel, clipped to 0..255.
inline DWORD GlassMove(DWORD p, DWORD bgFrom, DWORD bgTo, int alpha)
{
    DWORD out = 0;
    for (int k = 0; k < 24; k += 8) {
        int v = (int)(p >> k & 255) + ((int)(bgTo >> k & 255) - (int)(bgFrom >> k & 255)) * (255 - alpha) / 255;
        out |= (DWORD)(v < 0 ? 0 : v > 255 ? 255 : v) << k;
    }
    return out;
}

// One row of the band [l, r) with the page a shifted by x (right > 0) and its neighbour b beside it (the one before
// when x > 0, after when x < 0; NULL - none, an end): out[l..r). A page's pixel that is the background where it
// stands (bg, the same row of the still background) is not the page's - the background where it lands shows instead;
// a glass one (its page's card mask ma / mb above 0; NULL: none) is seen over the background where it lands.
inline void SlideRow(DWORD* out, const DWORD* a, const DWORD* b, const DWORD* bg, int l, int r, int x,
                     const unsigned char* ma, const unsigned char* mb)
{
    int w = r - l;
    for (int px = l; px < r; px++) {
        int p = px - l - x;   // the column in the band of the page that lands here
        const DWORD* src = a;
        const unsigned char* m = ma;
        if (p < 0) {   // the one before
            p += w;
            src = b;
            m = mb;
        } else if (p >= w) {   // the one after
            p -= w;
            src = b;
            m = mb;
        }
        int col = l + p;
        if (src && m && m[col])
            out[px] = GlassMove(src[col], bg[col], bg[px], m[col]);
        else
            out[px] = src && src[col] != bg[col] ? src[col] : bg[px];
    }
}
