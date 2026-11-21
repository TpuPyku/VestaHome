// musdir.cpp: a browser's folder - its subfolders all there, files in the room left (the user 2026-11-20: a card
// with 400+ files in its root showed none of its music folders).
#include "musdir.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int s_checks, s_failed;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAIL: %s\n", what);
    }
}

static bool Mp3(const wchar_t* n)
{
    const wchar_t* dot = wcsrchr(n, L'.');
    return dot && !_wcsicmp(dot, L".mp3");
}

static void Touch(const wchar_t* path)
{
    HANDLE h = CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    CloseHandle(h);
}

static wchar_t s_dir[MAX_PATH];

static void Make(const wchar_t* name, bool dir)
{
    wchar_t p[MAX_PATH];
    _snwprintf(p, MAX_PATH, L"%s%s", s_dir, name);
    if (dir)
        CreateDirectory(p, NULL);
    else
        Touch(p);
}

int main()
{
    wchar_t tmp[MAX_PATH];
    GetTempPath(MAX_PATH, tmp);
    _snwprintf(s_dir, MAX_PATH, L"%stest_musdir_%u\\", tmp, GetCurrentProcessId());
    CreateDirectory(s_dir, NULL);
    // the files first on the disk (the card: the old dump in the root), the folders after them
    Make(L"b.mp3", false);
    Make(L"a.mp3", false);
    Make(L"c.mp3", false);
    Make(L"d.mp3", false);
    Make(L"notes.txt", false);
    Make(L"Rock", true);
    Make(L"2001", true);
    Make(L".hidden", true);

    wchar_t* v[8];
    bool cut = true;
    int n = DirList(s_dir, v, 8, true, Mp3, &cut);
    Check(n == 6 && !cut, "all fit: 2 folders and 4 mp3, nothing cut");
    Check(n == 6 && !wcscmp(v[0], L"2001\\") && !wcscmp(v[1], L"Rock\\") && !wcscmp(v[2], L"a.mp3") &&
          !wcscmp(v[5], L"d.mp3"), "folders first with their '\\', then the files - each sorted");
    DirFree(v, n);

    n = DirList(s_dir, v, 4, true, Mp3, &cut);
    Check(n == 4 && cut, "room for 4: cut, said so");
    Check(n == 4 && !wcscmp(v[0], L"2001\\") && !wcscmp(v[1], L"Rock\\"), "... the folders kept, the files cut");
    DirFree(v, n);

    n = DirList(s_dir, v, 8, false, Mp3, &cut);
    Check(n == 4 && !cut && !wcscmp(v[0], L"a.mp3"), "no folders asked: the files only");
    DirFree(v, n);

    n = DirList(s_dir, v, 2, true, Mp3, &cut);
    Check(n == 2 && cut && !wcscmp(v[1], L"Rock\\"), "room only for the folders: the folders");
    DirFree(v, n);

    n = DirList(L"Z:\\no_such_dir\\", v, 8, true, Mp3, &cut);
    Check(n == 0 && !cut, "no folder: nothing");

    wchar_t p[MAX_PATH];
    // "Играть всё" (DirTree): the folder's own files, then each subfolder's, in the order of the paths - cut at cap in
    // that order, not the disk's (the user 2026-11-20: 400+ files - the first by the disk were taken)
    Make(L"Rock\\b2.mp3", false);
    Make(L"Rock\\a2.mp3", false);
    Make(L"Rock\\Live", true);
    Make(L"Rock\\Live\\s.mp3", false);
    Make(L"2001\\z.mp3", false);
    wchar_t* t[16];
    int m = DirTree(s_dir, t, 16, Mp3, 6, &cut);
    Check(m == 8 && !cut, "the whole tree: 8 mp3, nothing cut");
    Check(m == 8 && !wcscmp(t[0], L"a.mp3") && !wcscmp(t[3], L"d.mp3") && !wcscmp(t[4], L"2001\\z.mp3") &&
          !wcscmp(t[5], L"Rock\\a2.mp3") && !wcscmp(t[6], L"Rock\\b2.mp3") && !wcscmp(t[7], L"Rock\\Live\\s.mp3"),
          "own files first, then the subfolders by name - each the same way");
    DirFree(t, m);
    m = DirTree(s_dir, t, 6, Mp3, 6, &cut);
    Check(m == 6 && cut && !wcscmp(t[5], L"Rock\\a2.mp3"), "room for 6: the first 6 by the paths, cut said");
    DirFree(t, m);
    m = DirTree(s_dir, t, 16, Mp3, 1, &cut);
    Check(m == 7 && !cut, "one level deep: Rock\\Live left out");
    DirFree(t, m);
    m = DirTree(s_dir, t, 16, Mp3, 2, &cut);
    Check(m == 8, "two levels deep: Rock\\Live in");
    DirFree(t, m);
    m = DirTree(L"Z:\\no_such_dir\\", t, 16, Mp3, 6, &cut);
    Check(m == 0 && !cut, "no folder: nothing");
    const wchar_t* tree[] = { L"Rock\\b2.mp3", L"Rock\\a2.mp3", L"Rock\\Live\\s.mp3", L"2001\\z.mp3" };
    for (int i = 0; i < 4; i++) {
        _snwprintf(p, MAX_PATH, L"%s%s", s_dir, tree[i]);
        DeleteFile(p);
    }
    _snwprintf(p, MAX_PATH, L"%sRock\\Live", s_dir);
    RemoveDirectory(p);

    const wchar_t* names[] = { L"b.mp3", L"a.mp3", L"c.mp3", L"d.mp3", L"notes.txt" };
    for (int i = 0; i < 5; i++) {
        _snwprintf(p, MAX_PATH, L"%s%s", s_dir, names[i]);
        DeleteFile(p);
    }
    const wchar_t* dirs[] = { L"Rock", L"2001", L".hidden" };
    for (int i = 0; i < 3; i++) {
        _snwprintf(p, MAX_PATH, L"%s%s", s_dir, dirs[i]);
        RemoveDirectory(p);
    }
    RemoveDirectory(s_dir);
    printf("test_musdir: %d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
