// musdir.cpp - a media browser's folder (see musdir.h).
#include "musdir.h"
#include <stdlib.h>
#include <stdio.h>

int DirNameCmp(const void* a, const void* b)   // (the folders and the files are sorted apart: by name only)
{
    return _wcsicmp(*(const wchar_t* const*)a, *(const wchar_t* const*)b);
}

void DirFree(wchar_t** v, int n)
{
    for (int i = 0; i < n; i++)
        free(v[i]);
}

static wchar_t* Name(const WIN32_FIND_DATA& fd, bool isDir)
{
    size_t len = wcslen(fd.cFileName);
    wchar_t* s = (wchar_t*)malloc((len + 2) * sizeof(wchar_t));
    if (s) {
        wcscpy(s, fd.cFileName);
        if (isDir)
            wcscat(s, L"\\");
    }
    return s;
}

// An entry this pass takes: not hidden, not ".x"; a folder in the folders' pass, a wanted file in the files'.
static bool Takes(const WIN32_FIND_DATA& fd, bool dirs, DirWant want)
{
    bool isDir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    if (fd.cFileName[0] == L'.' || (fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) || isDir != dirs)
        return false;
    return isDir || want(fd.cFileName);
}

// One pass over `dir`: its folders (dirs) or its wanted files; false - the room ran out (*n == cap).
static bool Pass(const wchar_t* dir, wchar_t** v, int* n, int cap, bool dirs, DirWant want)
{
    wchar_t pat[MAX_PATH];
    _snwprintf(pat, MAX_PATH, L"%s*", dir);
    pat[MAX_PATH - 1] = 0;
    WIN32_FIND_DATA fd;
    HANDLE h = FindFirstFile(pat, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return true;
    bool fits = true;
    do {
        if (!Takes(fd, dirs, want))
            continue;
        if (*n == cap) {
            fits = false;
            break;
        }
        wchar_t* s = Name(fd, dirs);
        if (!s)
            break;
        v[(*n)++] = s;
    } while (FindNextFile(h, &fd));
    FindClose(h);
    return fits;
}

int DirList(const wchar_t* dir, wchar_t** v, int cap, bool folders, DirWant want, bool* cut)
{
    int n = 0;
    bool fits = !folders || Pass(dir, v, &n, cap, true, want);
    int files = n;
    fits = Pass(dir, v, &n, cap, false, want) && fits;
    qsort(v, files, sizeof(wchar_t*), DirNameCmp);
    qsort(v + files, n - files, sizeof(wchar_t*), DirNameCmp);
    *cut = !fits;
    return n;
}

// One folder of the tree (sub: its path inside the top, "" or "A\B\"): its own files, then its subfolders.
static void TreeIn(const wchar_t* top, const wchar_t* sub, wchar_t** v, int* n, int cap, DirWant want, int depth,
                   bool* cut)
{
    enum { kLevel = 2000 };   // the entries of one folder (a card's root with 400+ files)
    wchar_t dir[MAX_PATH];
    _snwprintf(dir, MAX_PATH, L"%s%s", top, sub);
    dir[MAX_PATH - 1] = 0;
    wchar_t** e = (wchar_t**)malloc(kLevel * sizeof(wchar_t*));
    if (!e)
        return;
    bool more;
    int k = DirList(dir, e, kLevel, depth > 0, want, &more);
    int folders = 0;
    while (folders < k && e[folders][wcslen(e[folders]) - 1] == L'\\')
        folders++;
    for (int i = folders; i < k; i++) {   // the files
        if (*n == cap) {
            *cut = true;
            break;
        }
        wchar_t p[MAX_PATH];
        _snwprintf(p, MAX_PATH, L"%s%s", sub, e[i]);
        p[MAX_PATH - 1] = 0;
        v[*n] = _wcsdup(p);
        if (v[*n])
            (*n)++;
    }
    for (int i = 0; i < folders && !*cut; i++) {   // then each subfolder
        wchar_t s[MAX_PATH];
        _snwprintf(s, MAX_PATH, L"%s%s", sub, e[i]);
        s[MAX_PATH - 1] = 0;
        TreeIn(top, s, v, n, cap, want, depth - 1, cut);
    }
    DirFree(e, k);
    free(e);
}

int DirTree(const wchar_t* dir, wchar_t** v, int cap, DirWant want, int depth, bool* cut)
{
    int n = 0;
    *cut = false;
    TreeIn(dir, L"", v, &n, cap, want, depth, cut);
    return n;
}
