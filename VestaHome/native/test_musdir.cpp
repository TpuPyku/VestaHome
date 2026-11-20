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
