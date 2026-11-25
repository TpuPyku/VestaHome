// test_cardpic.cpp - the skin's card picture (cardpic.h): the rounded mask, the picture's pixel taken, the blend
// into the frame (day, night), on the PC.
#include <windows.h>
#include <stdio.h>
#include "cardpic.h"

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
    // the mask: a 100 x 60 card, corners of radius 20
    Check(CardInside(50, 30, 100, 60, 20), "the middle");
    Check(CardInside(0, 30, 100, 60, 20) && CardInside(99, 30, 100, 60, 20), "the left and right edge's middle");
    Check(CardInside(50, 0, 100, 60, 20) && CardInside(50, 59, 100, 60, 20), "the top and bottom edge's middle");
    Check(!CardInside(0, 0, 100, 60, 20) && !CardInside(99, 0, 100, 60, 20), "the top corners cut");
    Check(!CardInside(0, 59, 100, 60, 20) && !CardInside(99, 59, 100, 60, 20), "the bottom corners cut");
    Check(CardInside(20, 0, 100, 60, 20) && CardInside(0, 20, 100, 60, 20), "where the round ends");
    Check(CardInside(6, 6, 100, 60, 20) && !CardInside(5, 5, 100, 60, 20), "on the arc (as RSoft)");
    Check(CardInside(79, 0, 100, 60, 20) && CardInside(99, 39, 100, 60, 20), "the right side's arc ends");
    Check(CardInside(0, 0, 100, 60, 0), "no rounding: the corner is in");
    // a whole 13 x 11 card of radius 5 against the arcs' centres (5, 5) .. (7, 5): (2, 1) is 3-4-5, on the arc
    static const char* map[11] = { ".....###.....", "..#########..", ".###########.", ".###########.",
                                   ".###########.", "#############", ".###########.", ".###########.",
                                   ".###########.", "..#########..", ".....###....." };
    bool same = true;
    for (int y = 0; y < 11; y++)
        for (int x = 0; x < 13; x++)
            if (CardInside(x, y, 13, 11, 5) != (map[y][x] == '#')) {
                same = false;
                printf("  mask differs at %d, %d\n", x, y);
            }
    Check(same, "the 13 x 11 card's mask");
    // which picture: card_small under 100 px high when the skin gives one, else card
    Check(CardPick(64, true) == 1 && CardPick(99, true) == 1, "small tiles: card_small");
    Check(CardPick(100, true) == 0 && CardPick(158, true) == 0, "cards: card");
    Check(CardPick(64, false) == 0, "no card_small: card");
    // the picture's pixel for the card's pixel: nearest, spread over the whole picture
    Check(CardPicAt(0, 768, 128) == 0 && CardPicAt(767, 768, 128) == 127, "the ends");
    Check(CardPicAt(384, 768, 128) == 64 && CardPicAt(5, 768, 128) == 0 && CardPicAt(6, 768, 128) == 1, "between");
    Check(CardPicAt(17, 64, 64) == 17, "the same size: the same pixel");
    // the blend: premultiplied s[0..3] over the frame's 0x00RRGGBB
    int half[4] = { 100, 50, 0, 128 };   // a colour at alpha 128, premultiplied
    DWORD p = CardBlend(0x00C8C8C8, half, false);
    Check(((p >> 16) & 255) == 100 + 200 * 127 / 255 && ((p >> 8) & 255) == 50 + 200 * 127 / 255 &&
          (p & 255) == 200 * 127 / 255, "half over grey");
    int solid[4] = { 30, 60, 90, 255 };
    Check(CardBlend(0x00FFFFFF, solid, false) == 0x001E3C5A, "opaque: the picture");
    Check(CardBlend(0x00FFFFFF, solid, true) == 0x0014283C, "opaque at night: a third darker");
    int none[4] = { 0, 0, 0, 0 };
    Check(CardBlend(0x00123456, none, false) == 0x00123456, "transparent: the frame");
    int over[4] = { 255, 255, 255, 128 };   // (not premultiplied right: clipped)
    Check(CardBlend(0x00FFFFFF, over, false) == 0x00FFFFFF, "clipped at 255");
    int edge[4] = { 255, 255, 255, 254 };   // 255 + 255 * 1 / 255 = 256: just past the top
    Check(CardBlend(0x00FFFFFF, edge, false) == 0x00FFFFFF, "256 clipped too");
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
