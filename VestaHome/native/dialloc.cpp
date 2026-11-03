// dialloc.cpp - the dialing location's area code and country (dialloc.h).
#include "dialloc.h"

enum { kAreaField = 4, kCountryField = 6 };

// The n-th field: its start and length (up to its terminator or the value's end); a field not there: length 0
static void Field(const wchar_t* loc, int chars, int n, const wchar_t** p, int* len)
{
    int i = 0;
    for (; n > 0 && i < chars; i++)
        if (!loc[i])
            n--;
    int e = i;
    while (e < chars && loc[e])
        e++;
    *p = loc + i;
    *len = e - i;
}

static bool Number(const wchar_t* p, int len, DWORD* v)
{
    *v = 0;
    for (int i = 0; i < len; i++) {
        if (p[i] < L'0' || p[i] > L'9')
            return false;
        *v = *v * 10 + (p[i] - L'0');
    }
    return len > 0;
}

bool DialLocation(const wchar_t* loc, int chars, wchar_t* area, int areaCap, DWORD* country)
{
    const wchar_t *a, *c;
    int na, nc;
    DWORD v;
    Field(loc, chars, kAreaField, &a, &na);
    Field(loc, chars, kCountryField, &c, &nc);
    if (na >= areaCap || !Number(c, nc, &v))   // (no country field: none of the fields before it either)
        return false;
    memcpy(area, a, na * sizeof(wchar_t));
    area[na] = 0;
    *country = v;
    return true;
}
