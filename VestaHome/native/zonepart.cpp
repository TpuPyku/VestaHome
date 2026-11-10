// zonepart.cpp - see zonepart.h.
#include "zonepart.h"

bool ZoneSame(const RECT* was, int x, int y, int w, int h)
{
    return was && was->left == x && was->top == y && was->right == x + w && was->bottom == y + h;
}
