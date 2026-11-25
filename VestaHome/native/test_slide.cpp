// test_slide.cpp - a row of a page sliding over a still background (slide.h), on the PC.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "slide.h"

static int s_failed, s_checks;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAIL: %s\n", what);
    }
}

// Rows of 8 pixels as letters: the background "abcdefgh", a page's own pixels capitals / digits.
static void Row(DWORD* r, const char* s)
{
    for (int i = 0; i < 8; i++)
        r[i] = (DWORD)(unsigned char)s[i];
}

static bool Is(const DWORD* r, const char* s)
{
    for (int i = 0; i < 8; i++)
        if (r[i] != (DWORD)(unsigned char)s[i]) {
            printf("  at %d: %c, want %c\n", i, (char)r[i], s[i]);
            return false;
        }
    return true;
}

int main()
{
    DWORD bg[8], a[8], prev[8], next[8], out[8];
    Row(bg, "abcdefgh");
    Row(a, "aXYdefgh");      // the page: X, Y at 1, 2 over the background
    Row(prev, "abcdefQ9");   // the page before it: its right end Q, 9 (comes in at the left)
    Row(next, "P2cdefgh");   // the page after it: its left end P, 2 (comes in at the right)
    SlideRow(out, a, prev, bg, 0, 8, 0, NULL, NULL);
    Check(Is(out, "aXYdefgh"), "still: the page as it is");
    SlideRow(out, a, prev, bg, 0, 8, 2, NULL, NULL);
    Check(Is(out, "Q9cXYfgh"), "right by 2: the page's own pixels move, the background stays; the one before comes in");
    SlideRow(out, a, next, bg, 0, 8, -3, NULL, NULL);
    Check(Is(out, "abcdeP2h"), "left by 3: X, Y gone off the left, the next one's P, 2 in from the right");
    SlideRow(out, a, NULL, bg, 0, 8, 2, NULL, NULL);
    Check(Is(out, "abcXYfgh"), "no neighbour (an end): the background where the page has gone");
    // a band from column 3: what stays at its left is not touched
    Row(out, "........");
    Row(a, "abcdQfgh");
    SlideRow(out, a, NULL, bg, 3, 8, 1, NULL, NULL);
    Check(Is(out, "...deQgh"), "a band [3, 8): only it written, the page moved inside it");
    Row(out, "........");
    SlideRow(out, a, NULL, bg, 3, 8, -1, NULL, NULL);
    Check(Is(out, "...Qefgh"), "a band [3, 8), to the left");
    // the page's own pixel at its first column; a band with a neighbour; nothing written past the row's end
    DWORD a9[9], o9[9];
    Row(a9, "ZXYdefgh");
    Row(o9, "........");
    o9[8] = '#';
    SlideRow(o9, a9, prev, bg, 0, 8, 0, NULL, NULL);
    Check(Is(o9, "ZXYdefgh") && o9[8] == '#', "still: its first column is the page's, nothing past the end");
    SlideRow(o9, a9, prev, bg, 0, 8, 1, NULL, NULL);
    Check(Is(o9, "9ZXYefgh"), "right by 1: the first column's Z moves to 1, the one before's last in at 0");
    Row(a, "abcdQfgh");
    Row(next, "abcPefgh");   // its own P at 3: the band's first column
    Row(out, "........");
    SlideRow(out, a, next, bg, 3, 8, -1, NULL, NULL);
    Check(Is(out, "...QefgP"), "a band [3, 8) to the left: the next one's band start comes in at its end");
    // a page's pixel equal to the background where it stands is background
    Row(a, "abcdefgh");
    SlideRow(out, a, prev, bg, 0, 8, 4, NULL, NULL);
    Check(Is(out, "abQ9efgh"), "a page of background only: the background still, the neighbour's own pixels in");
    // glass (the user 2026-11-24: the cards' transparency real while sliding, not their old place's background):
    // a card's pixel over bg 0x101010 at alpha 128 (premultiplied colour 0x404040 + 0x10 * 127 / 255 = 0x47) moved
    // over bg 0x303030 - its colour plus the background's difference times (255 - 128) / 255
    Check(GlassMove(0x00474747, 0x00101010, 0x00303030, 128) == 0x00565656, "glass: the background seen anew");
    Check(GlassMove(0x00474747, 0x00303030, 0x00101010, 128) == 0x00383838, "glass: onto a darker one");
    Check(GlassMove(0x00474747, 0x00101010, 0x00F0F0F0, 0) == 0x00FFFFFF, "glass: alpha 0 - the new background, clipped");
    Check(GlassMove(0x00102030, 0x00000000, 0x00FFFFFF, 255) == 0x00102030, "glass: opaque - unchanged");
    Check(GlassMove(0x00000080, 0, 0x00000080, 0) == 0x000000FF, "glass: 256 - clipped, nothing into the next channel");
    Check(GlassMove(0x00050505, 0x00FF0000, 0x00000000, 0) == 0x00000505, "glass: below 0 - clipped at 0");
    Check(GlassMove(0x00102030, 0x00010203, 0x00040608, 0) == 0x00132435, "glass: each channel on its own");
    // in a row: the page's glass pixels (mask) re-blended where they land, its own opaque ones moved as they are
    DWORD g[8], gbg[8], gout[8];
    unsigned char gm[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    for (int i = 0; i < 8; i++) {
        gbg[i] = 0x00101010 * (i + 1);   // a background that changes along the row
        g[i] = gbg[i];
    }
    g[2] = GlassMove(0x00404040, 0, gbg[2], 128);   // (a glass card's pixel at 2: 0x40 premultiplied over its bg)
    gm[2] = 128;
    g[3] = 0x00ABCDEF;   // an opaque one at 3 (no mask)
    SlideRow(gout, g, NULL, gbg, 0, 8, 2, gm, NULL);
    Check(gout[4] == GlassMove(g[2], gbg[2], gbg[4], 128), "glass row: the glass pixel over the background it lands on");
    Check(gout[5] == 0x00ABCDEF, "glass row: an opaque one as it is");
    Check(gout[2] == gbg[2] && gout[0] == gbg[0], "glass row: the background still elsewhere");
    unsigned char nm[8] = { 0, 0, 0, 0, 0, 0, 0, 200 };
    DWORD nb[8];
    for (int i = 0; i < 8; i++)
        nb[i] = gbg[i];
    nb[7] = 0x00777777;   // the neighbour's glass pixel at 7 comes in at 1 (x = 2: 7 - 8 + 2)
    SlideRow(gout, g, nb, gbg, 0, 8, 2, gm, nm);
    Check(gout[1] == GlassMove(0x00777777, gbg[7], gbg[1], 200), "glass row: the neighbour's glass by its own mask");
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
