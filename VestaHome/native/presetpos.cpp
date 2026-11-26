// presetpos.cpp - which preset plays and the next one (presetpos.h).
#include <stdio.h>
#include <wchar.h>
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

int PresetChanges(const int* was, const int* now, int from, int n, wchar_t* out, int cap)
{
    int count = 0, k = 0;
    out[0] = 0;
    for (int i = 0; i < n; i++) {
        if (!was[i] || now[i] == was[i])
            continue;
        wchar_t one[48];
        int len = _snwprintf(one, 48, L"%s%d: %d.%02d -> %d.%02d", count ? L", " : L"", from + i + 1, was[i] / 100,
                             was[i] % 100, now[i] / 100, now[i] % 100);
        count++;
        for (int j = 0; j < len && k < cap - 1; j++)
            out[k++] = one[j];
        out[k] = 0;
    }
    return count;
}
