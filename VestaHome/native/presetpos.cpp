// presetpos.cpp - which preset plays and the next one (presetpos.h).
#include "presetpos.h"

int PresetPlaying(int pos, int sent)
{
    if (pos < 0)
        return -1;
    return sent / 8 * 8 + pos;   // (-1 / 8 is 0 in C++: nothing asked - the first page)
}

int PresetNext(int cur, int dir, int per)
{
    if (cur < 0)
        return 0;
    int page = cur / 8 * 8;
    return page + (cur - page + dir + per) % per;
}
