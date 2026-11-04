// soundmix.cpp - the programs whose sound goes with the radio (soundmix.h)
#include <string.h>
#include "soundmix.h"

const wchar_t* MixExeOf(const wchar_t* path)
{
    const wchar_t* name = path;
    for (const wchar_t* p = path; *p; p++)
        if (*p == L'\\' || *p == L'/')
            name = p + 1;
    return name;
}

// The list's entry equal to exe (any case): where it starts, NULL none
static const wchar_t* Find(const wchar_t* list, const wchar_t* exe)
{
    size_t n = wcslen(exe);
    if (!n)
        return NULL;
    for (const wchar_t* p = list; *p; ) {
        const wchar_t* end = wcschr(p, L';');
        size_t len = end ? (size_t)(end - p) : wcslen(p);
        if (len == n && !_wcsnicmp(p, exe, n))
            return p;
        p += end ? len + 1 : len;
    }
    return NULL;
}

bool MixListHas(const wchar_t* list, const wchar_t* exe)
{
    return Find(list, exe) != NULL;
}

static bool Add(wchar_t* list, int cap, const wchar_t* exe)
{
    size_t have = wcslen(list), add = wcslen(exe) + (have ? 1 : 0);
    if (!exe[0] || have + add >= (size_t)cap)
        return false;
    if (have)
        wcscat(list, L";");
    wcscat(list, exe);
    return true;
}

static void Remove(wchar_t* list, wchar_t* at, const wchar_t* exe)
{
    wchar_t* next = at + wcslen(exe);
    if (*next == L';')
        next++;
    else if (at > list)
        at--;   // the last one: its ';' before it goes too
    memmove(at, next, (wcslen(next) + 1) * sizeof(wchar_t));
}

bool MixListSet(wchar_t* list, int cap, const wchar_t* exe, bool on)
{
    wchar_t* at = (wchar_t*)Find(list, exe);
    if (on)
        return at || Add(list, cap, exe);
    if (at)
        Remove(list, at, exe);
    return true;
}

int MixRunning(const wchar_t* list, const wchar_t (*procs)[kMixProcName], int n)
{
    for (int i = 0; i < n; i++)
        if (MixListHas(list, procs[i]))
            return i;
    return -1;
}
