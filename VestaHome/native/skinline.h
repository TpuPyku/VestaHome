// skinline.h - a line of Skin\skin.ini or VHContent\Skin\user_skin.ini (the user 2026-11-19: the user's own colours,
// icons and fonts, kept over updates): [colors] name = RRGGBB, [images] name = file.png, [fonts] name = value. The
// keys are VestaLaunch's where they mean the same (one colours file fits both). Pure, header-only (test_skinline).
#ifndef SKINLINE_H
#define SKINLINE_H

#include <wchar.h>
#include <stdlib.h>

enum { SKS_COLORS, SKS_IMAGES, SKS_FONTS, SKS_OTHER };   // the section a line is in
enum { SKL_NONE, SKL_SECTION, SKL_COLOR, SKL_FILE };    // what the line is

static void SkinTrim(wchar_t* s)   // the spaces and tabs at both ends
{
    wchar_t* b = s;
    while (*b == L' ' || *b == L'\t')
        b++;
    int n = (int)wcslen(b);
    while (n > 0 && (b[n - 1] == L' ' || b[n - 1] == L'\t' || b[n - 1] == L'\r'))
        n--;
    memmove(s, b, n * sizeof(wchar_t));
    s[n] = 0;
}

static int SkinSection(const wchar_t* s)
{
    return !_wcsicmp(s, L"[colors]") ? SKS_COLORS : !_wcsicmp(s, L"[images]") ? SKS_IMAGES
                                                  : !_wcsicmp(s, L"[fonts]") ? SKS_FONTS : SKS_OTHER;
}

static bool SkinColor(const wchar_t* v, COLORREF* c)   // "RRGGBB" or "#RRGGBB", nothing after
{
    if (*v == L'#')
        v++;
    wchar_t* e;
    unsigned long x = wcstoul(v, &e, 16);
    if (e - v != 6 || *e)
        return false;
    *c = RGB(x >> 16, (x >> 8) & 255, x & 255);
    return true;
}

// "name = value": the name into key, the value's start (in line, trimmed); NULL - no '=', no name or no value, the
// name too long.
static wchar_t* SkinPair(wchar_t* line, wchar_t* key, int keyCap)
{
    wchar_t* eq = wcschr(line, L'=');
    if (!eq)
        return NULL;
    *eq = 0;
    wchar_t* v = eq + 1;
    SkinTrim(line);
    SkinTrim(v);
    if (!line[0] || !v[0] || (int)wcslen(line) >= keyCap)
        return NULL;
    wcscpy(key, line);
    return v;
}

// line: changed in place (the comment after ';' cut). *section: the one it is in, a "[...]" line sets it. A colour -
// key and *c; a file or a font's value - key and val.
static int SkinLine(wchar_t* line, int* section, wchar_t* key, int keyCap, wchar_t* val, int valCap, COLORREF* c)
{
    wchar_t* semi = wcschr(line, L';');
    if (semi)
        *semi = 0;
    SkinTrim(line);
    if (line[0] == L'[') {
        *section = SkinSection(line);
        return SKL_SECTION;
    }
    wchar_t* v = *section == SKS_OTHER ? NULL : SkinPair(line, key, keyCap);
    if (!v)
        return SKL_NONE;
    if (*section == SKS_COLORS)
        return SkinColor(v, c) ? SKL_COLOR : SKL_NONE;
    if ((int)wcslen(v) >= valCap)
        return SKL_NONE;
    wcscpy(val, v);
    return SKL_FILE;
}

#endif
