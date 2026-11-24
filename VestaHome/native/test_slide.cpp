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
    SlideRow(out, a, prev, bg, 0, 8, 0);
    Check(Is(out, "aXYdefgh"), "still: the page as it is");
    SlideRow(out, a, prev, bg, 0, 8, 2);
    Check(Is(out, "Q9cXYfgh"), "right by 2: the page's own pixels move, the background stays; the one before comes in");
    SlideRow(out, a, next, bg, 0, 8, -3);
    Check(Is(out, "abcdeP2h"), "left by 3: X, Y gone off the left, the next one's P, 2 in from the right");
    SlideRow(out, a, NULL, bg, 0, 8, 2);
    Check(Is(out, "abcXYfgh"), "no neighbour (an end): the background where the page has gone");
    // a band from column 3: what stays at its left is not touched
    Row(out, "........");
    Row(a, "abcdQfgh");
    SlideRow(out, a, NULL, bg, 3, 8, 1);
    Check(Is(out, "...deQgh"), "a band [3, 8): only it written, the page moved inside it");
    Row(out, "........");
    SlideRow(out, a, NULL, bg, 3, 8, -1);
    Check(Is(out, "...Qefgh"), "a band [3, 8), to the left");
    // the page's own pixel at its first column; a band with a neighbour; nothing written past the row's end
    DWORD a9[9], o9[9];
    Row(a9, "ZXYdefgh");
    Row(o9, "........");
    o9[8] = '#';
    SlideRow(o9, a9, prev, bg, 0, 8, 0);
    Check(Is(o9, "ZXYdefgh") && o9[8] == '#', "still: its first column is the page's, nothing past the end");
    SlideRow(o9, a9, prev, bg, 0, 8, 1);
    Check(Is(o9, "9ZXYefgh"), "right by 1: the first column's Z moves to 1, the one before's last in at 0");
    Row(a, "abcdQfgh");
    Row(next, "abcPefgh");   // its own P at 3: the band's first column
    Row(out, "........");
    SlideRow(out, a, next, bg, 3, 8, -1);
    Check(Is(out, "...QefgP"), "a band [3, 8) to the left: the next one's band start comes in at its end");
    // a page's pixel equal to the background where it stands is background
    Row(a, "abcdefgh");
    SlideRow(out, a, prev, bg, 0, 8, 4);
    Check(Is(out, "abQ9efgh"), "a page of background only: the background still, the neighbour's own pixels in");
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
