// cardpic.h - the skin's card picture ([images] card / card_small, the user 2026-11-23): a PNG with transparency
// stretched over a card and cut by the card's own rounding. The card takes the picture's nearest pixel: the card's
// pictures are soft gradients, and the unit's ARM can't afford a bilinear sample for every pixel of every card in
// every frame (nor a scaled copy per card size: the icons' cache is 256 KB).
#pragma once
#include <windows.h>

// The card's pixel (dx, dy) is inside its w x h rounded rectangle of corner radius rx (as RSoft cuts it).
inline bool CardInside(int dx, int dy, int w, int h, int rx)
{
    int ex = dx < rx ? rx - dx : dx >= w - rx ? dx - (w - rx - 1) : 0;
    int ey = dy < rx ? rx - dy : dy >= h - rx ? dy - (h - rx - 1) : 0;
    return ex * ex + ey * ey <= rx * rx;   // (ex, ey <= rx: a straight edge is always in)
}

enum { kCardSmallH = 100 };   // lower cards (the radio's presets, the settings' sections) are the small tiles

// Which picture a card h high takes: 1 = card_small (when the skin gives one), 0 = card.
inline int CardPick(int h, bool haveSmall)
{
    return haveSmall && h < kCardSmallH ? 1 : 0;
}

// The picture's pixel (of c) for the card's pixel d (of n): the nearest.
inline int CardPicAt(int d, int n, int c)
{
    return d * c / n;
}

// The picture's premultiplied s[0..3] over the frame's pixel 0x00RRGGBB; at night the picture a third darker.
inline DWORD CardBlend(DWORD p, const int* s, bool night)
{
    int inv = 255 - s[3], out[3];
    for (int k = 0; k < 3; k++) {
        int c = night ? s[k] * 2 / 3 : s[k];
        int v = c + (int)((p >> (16 - 8 * k)) & 255) * inv / 255;
        out[k] = v > 255 ? 255 : v;
    }
    return ((DWORD)out[0] << 16) | ((DWORD)out[1] << 8) | (DWORD)out[2];
}
