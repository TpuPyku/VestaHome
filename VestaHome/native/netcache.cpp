// netcache.cpp - see netcache.h. A file: the path, a line's end, the answer. Written aside and moved in, so a power cut
// leaves the older one or nothing. One decision of age here: the file's write time against the clock.
#include "netcache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void FileOf(const wchar_t* dir, const char* path, wchar_t* out)
{
    DWORD h = 2166136261u;   // FNV-1a
    for (const char* p = path; *p; p++)
        h = (h ^ (unsigned char)*p) * 16777619u;
    _snwprintf(out, MAX_PATH, L"%s\\%08X.txt", dir, (unsigned)h);
    out[MAX_PATH - 1] = 0;
}

static unsigned __int64 Ticks(const FILETIME& f)
{
    return (unsigned __int64)f.dwHighDateTime << 32 | f.dwLowDateTime;
}

static bool Fresh(HANDLE file, DWORD maxAgeS)
{
    SYSTEMTIME st;
    FILETIME now, wrote;
    GetSystemTime(&st);
    if (!SystemTimeToFileTime(&st, &now) || !GetFileTime(file, NULL, NULL, &wrote))
        return false;
    // A file just written can look a little newer than the clock (the clock in ms, FAT's times round up to 2 s): that is
    // no clock gone wrong.
    enum { kSlipS = 60 };
    unsigned __int64 n = Ticks(now), w = Ticks(wrote), s = 10000000;
    return w <= n + kSlipS * s && (n > w ? (n - w) / s : 0) <= maxAgeS;
}

bool NcCacheRead(const wchar_t* dir, const char* path, DWORD maxAgeS, unsigned char** body, int* len)
{
    wchar_t name[MAX_PATH];
    FileOf(dir, path, name);
    HANDLE f = CreateFile(name, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return false;
    int head = (int)strlen(path) + 1;
    DWORD size = GetFileSize(f, NULL), got = 0;
    unsigned char* b = NULL;
    if ((!maxAgeS || Fresh(f, maxAgeS)) && size != INVALID_FILE_SIZE && size > (DWORD)head &&
        size <= (DWORD)(head + kNcCacheMaxBytes))
        b = (unsigned char*)malloc(size + 1);
    bool ok = b && ReadFile(f, b, size, &got, NULL) && got == size && !memcmp(b, path, head - 1) && b[head - 1] == '\n';
    CloseHandle(f);
    if (!ok) {
        free(b);
        return false;
    }
    memmove(b, b + head, size - head);
    b[size - head] = 0;
    *body = b;
    *len = (int)size - head;
    return true;
}

// Beyond the limit: the oldest ones go
static void Prune(const wchar_t* dir)
{
    wchar_t mask[MAX_PATH], name[MAX_PATH];
    _snwprintf(mask, MAX_PATH, L"%s\\*.txt", dir);
    mask[MAX_PATH - 1] = 0;
    for (;;) {
        WIN32_FIND_DATA fd;
        HANDLE h = FindFirstFile(mask, &fd);
        if (h == INVALID_HANDLE_VALUE)
            return;
        int n = 0;
        unsigned __int64 oldest = 0;
        name[0] = 0;
        do {
            unsigned __int64 t = Ticks(fd.ftLastWriteTime);
            if (!n || t < oldest) {
                oldest = t;
                wcsncpy(name, fd.cFileName, MAX_PATH - 1);
                name[MAX_PATH - 1] = 0;
            }
            n++;
        } while (FindNextFile(h, &fd));
        FindClose(h);
        if (n <= kNcCacheFiles)
            return;
        wchar_t full[MAX_PATH];
        _snwprintf(full, MAX_PATH, L"%s\\%s", dir, name);
        full[MAX_PATH - 1] = 0;
        if (!DeleteFile(full))
            return;
    }
}

bool NcCacheWrite(const wchar_t* dir, const char* path, const unsigned char* body, int len)
{
    if (len <= 0 || len > kNcCacheMaxBytes)
        return false;
    CreateDirectory(dir, NULL);
    wchar_t name[MAX_PATH], tmp[MAX_PATH];
    FileOf(dir, path, name);
    _snwprintf(tmp, MAX_PATH, L"%s.tmp", name);
    tmp[MAX_PATH - 1] = 0;
    HANDLE f = CreateFile(tmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return false;
    DWORD head = (DWORD)strlen(path), done = 0, done2 = 0, done3 = 0;
    bool ok = WriteFile(f, path, head, &done, NULL) && done == head && WriteFile(f, "\n", 1, &done2, NULL) && done2 == 1 &&
              WriteFile(f, body, (DWORD)len, &done3, NULL) && done3 == (DWORD)len;
    CloseHandle(f);
    if (ok) {
        DeleteFile(name);
        ok = MoveFile(tmp, name) != 0;
    }
    if (!ok) {
        DeleteFile(tmp);
        return false;
    }
    Prune(dir);
    return true;
}
