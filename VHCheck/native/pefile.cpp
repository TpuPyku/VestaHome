// pefile.cpp - pefile.h.
#include <windows.h>
#include <stdlib.h>
#include "pecheck.h"
#include "pefile.h"

int FwFileVersion(const wchar_t* path, int part)
{
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return 0;
    static unsigned char buf[64 * 1024 + 32];
    DWORD got = 0, keep = 0;
    int v = 0;
    while (!v && ReadFile(f, buf + keep, 64 * 1024, &got, NULL) && got) {
        DWORD total = keep + got;
        v = FwVersion(buf, (int)total, part);
        keep = total < 32 ? total : 32;   // (its last 32 bytes on: a string across two pieces)
        memmove(buf, buf + total - keep, keep);
    }
    CloseHandle(f);
    return v;
}

static FARPROC NamedProc(HMODULE m, const char* name)
{
#ifdef UNDER_CE
    wchar_t w[160];
    MultiByteToWideChar(CP_ACP, 0, name, -1, w, 160);
    return GetProcAddressW(m, w);
#else
    return GetProcAddress(m, name);
#endif
}

static int MissingLoaded(const wchar_t* path, const char* const* want, int n)   // a module of the ROM
{
    HMODULE h = LoadLibraryW(path);
    if (!h)
        return -1;
    int m = 0;
    for (int i = 0; i < n; i++)
        m += NamedProc(h, want[i]) ? 0 : 1;
    FreeLibrary(h);
    return m;
}

int PeMissingFile(const wchar_t* path, const char* const* want, int n)
{
    if (GetFileAttributesW(path) == 0xFFFFFFFF)
        return -1;
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return MissingLoaded(path, want, n);
    DWORD size = GetFileSize(f, NULL), got = 0;
    unsigned char* b = size < 16 * 1024 * 1024 ? (unsigned char*)malloc(size + 1) : NULL;
    bool ok = b && ReadFile(f, b, size, &got, NULL) && got == size;
    CloseHandle(f);
    int m = ok ? PeMissing(b, (int)size, want, n, NULL, 0) : n;
    free(b);
    return m;
}
