// test_netcache.cpp - netcache.cpp on the PC, in a folder of its own: what is kept comes back, its age, its limits.
// Exit code: the failed checks.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "netcache.h"

static int s_failed, s_checks;
static const wchar_t* const kDir = L"obj\\nctest";

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAILED: %s\n", what);
    }
}

static void Wipe()
{
    WIN32_FIND_DATAW fd;
    wchar_t mask[MAX_PATH];
    _snwprintf(mask, MAX_PATH, L"%s\\*", kDir);
    HANDLE h = FindFirstFileW(mask, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            wchar_t f[MAX_PATH];
            _snwprintf(f, MAX_PATH, L"%s\\%s", kDir, fd.cFileName);
            DeleteFileW(f);
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    RemoveDirectoryW(kDir);
}

static int Files()
{
    WIN32_FIND_DATAW fd;
    wchar_t mask[MAX_PATH];
    _snwprintf(mask, MAX_PATH, L"%s\\*", kDir);
    int n = 0;
    HANDLE h = FindFirstFileW(mask, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                n++;
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    return n;
}

static bool Put(const char* path, const char* text)
{
    return NcCacheWrite(kDir, path, (const unsigned char*)text, (int)strlen(text));
}

static bool Is(const char* path, DWORD maxAge, const char* want)
{
    unsigned char* b = NULL;
    int n = -1;
    bool ok = NcCacheRead(kDir, path, maxAge, &b, &n);
    if (!want)
        return !ok;
    ok = ok && n == (int)strlen(want) && !strcmp((const char*)b, want);
    free(b);
    return ok;
}

// Every file of the folder made this much older
static void Age(DWORD seconds)
{
    WIN32_FIND_DATAW fd;
    wchar_t mask[MAX_PATH];
    _snwprintf(mask, MAX_PATH, L"%s\\*", kDir);
    HANDLE h = FindFirstFileW(mask, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;
        wchar_t f[MAX_PATH];
        _snwprintf(f, MAX_PATH, L"%s\\%s", kDir, fd.cFileName);
        HANDLE w = CreateFileW(f, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        ULARGE_INTEGER t;
        t.LowPart = fd.ftLastWriteTime.dwLowDateTime;
        t.HighPart = fd.ftLastWriteTime.dwHighDateTime;
        t.QuadPart -= (unsigned __int64)seconds * 10000000;
        FILETIME ft;
        ft.dwLowDateTime = t.LowPart;
        ft.dwHighDateTime = t.HighPart;
        SetFileTime(w, NULL, NULL, &ft);
        CloseHandle(w);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

static void TestKeep()
{
    Wipe();
    Check(Is("/a", 0, NULL), "nothing kept: no answer (the folder not even there)");
    Check(Put("/a", "[1,2]"), "kept");
    Check(Is("/a", 0, "[1,2]"), "what was kept comes back, closed by a zero");
    Check(Is("/b", 0, NULL), "another path: no answer");
    Check(Put("/a", "[3]"), "kept again");
    Check(Is("/a", 0, "[3]"), "the newer one replaces it");
    Check(Files() == 1, "one file per path, no temporary left");
}

static void TestBinaryAndNewlines()
{
    Wipe();
    Put("/line", "first\nsecond\n");
    Check(Is("/line", 0, "first\nsecond\n"), "line breaks in the answer stay");
}

static void TestAge()
{
    Wipe();
    Put("/a", "x");
    Check(Is("/a", 3600, "x"), "new: fresh");
    Age(7200);
    Check(Is("/a", 3600, NULL), "older than asked: no answer");
    Check(Is("/a", 10000, "x"), "younger than asked");
    Check(Is("/a", 0, "x"), "age 0: any (no link: better an old list)");
    Age((DWORD)-1 / 2 + 0);   // (far back)
    Check(Is("/a", 0, "x"), "age 0: however old");
}

static void TestFuture()
{
    Wipe();
    Put("/a", "x");
    // the unit's clock can be wrong before GPS: a file "from the future" is not fresh
    wchar_t mask[MAX_PATH];
    _snwprintf(mask, MAX_PATH, L"%s\\*", kDir);
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(mask, &fd);
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;
        wchar_t f[MAX_PATH];
        _snwprintf(f, MAX_PATH, L"%s\\%s", kDir, fd.cFileName);
        HANDLE w = CreateFileW(f, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        ULARGE_INTEGER t;
        t.LowPart = fd.ftLastWriteTime.dwLowDateTime;
        t.HighPart = fd.ftLastWriteTime.dwHighDateTime;
        t.QuadPart += (unsigned __int64)86400 * 10000000;
        FILETIME ft;
        ft.dwLowDateTime = t.LowPart;
        ft.dwHighDateTime = t.HighPart;
        SetFileTime(w, NULL, NULL, &ft);
        CloseHandle(w);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    Check(Is("/a", 3600, NULL), "from the future: not fresh");
    Check(Is("/a", 0, "x"), "from the future: still there with no link");
}

static void TestLimits()
{
    Wipe();
    char* big = (char*)malloc(kNcCacheMaxBytes + 2);
    memset(big, 'x', kNcCacheMaxBytes + 1);
    big[kNcCacheMaxBytes + 1] = 0;
    Check(!Put("/big", big), "too big: not kept");
    big[kNcCacheMaxBytes] = 0;
    Check(Put("/max", big), "the largest allowed: kept");
    Check(Is("/max", 0, big), "the largest allowed: whole");
    free(big);
    Check(!NcCacheWrite(kDir, "/empty", (const unsigned char*)"", 0), "empty: not kept (a server's 200 with 0 bytes)");
    Check(Is("/empty", 0, NULL), "empty: no answer");
}

static void TestPrune()
{
    Wipe();
    char path[32];
    for (int i = 0; i < kNcCacheFiles; i++) {
        _snprintf(path, 32, "/p%d", i);
        Put(path, "x");
        if (i == 0)
            Age(1000);   // the first the oldest
    }
    Check(Files() == kNcCacheFiles, "up to the limit: all kept");
    Put("/new", "y");
    Check(Files() == kNcCacheFiles, "over the limit: the count stays");
    Check(Is("/p0", 0, NULL), "the oldest went");
    Check(Is("/new", 0, "y") && Is("/p1", 0, "x"), "the new one and the others stayed");
}

static void TestForeign()
{
    Wipe();
    Put("/a", "x");
    // a file cut by a power loss: no first line
    WIN32_FIND_DATAW fd;
    wchar_t mask[MAX_PATH], f[MAX_PATH];
    _snwprintf(mask, MAX_PATH, L"%s\\*", kDir);
    HANDLE h = FindFirstFileW(mask, &fd);
    while ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && FindNextFileW(h, &fd)) {
    }
    FindClose(h);
    _snwprintf(f, MAX_PATH, L"%s\\%s", kDir, fd.cFileName);
    HANDLE w = CreateFileW(f, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    DWORD done;
    WriteFile(w, "/a", 2, &done, NULL);
    CloseHandle(w);
    Check(Is("/a", 0, NULL), "no line's end: no answer");
    w = CreateFileW(f, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(w, "/other\nz", 8, &done, NULL);
    CloseHandle(w);
    Check(Is("/a", 0, NULL), "another path in the file (the same hash): no answer");
    w = CreateFileW(f, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(w, "/a\n", 3, &done, NULL);
    CloseHandle(w);
    Check(Is("/a", 0, NULL), "a path with nothing after it: no answer");
}

int main()
{
    CreateDirectoryW(L"obj", NULL);
    TestKeep();
    TestBinaryAndNewlines();
    TestAge();
    TestFuture();
    TestLimits();
    TestPrune();
    TestForeign();
    Wipe();
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
