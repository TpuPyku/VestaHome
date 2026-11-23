// ramlog.cpp - VestaHome's log kept in RAM while no card is there (see ramlog.h).
#include "ramlog.h"

static DWORD FileSize(const wchar_t* p)   // 0: none
{
    WIN32_FILE_ATTRIBUTE_DATA a;
    return GetFileAttributesEx(p, GetFileExInfoStandard, &a) ? a.nFileSizeLow : 0;
}

void RamLogPut(const wchar_t* ram, const wchar_t* old, const char* line, DWORD n)
{
    if (RamLogTurn(FileSize(ram), n)) {
        DeleteFile(old);
        MoveFile(ram, old);
    }
    HANDLE h = CreateFile(ram, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, 0, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return;
    SetFilePointer(h, 0, NULL, FILE_END);
    DWORD w;
    WriteFile(h, line, n, &w, NULL);
    CloseHandle(h);
}

static DWORD CopyOut(const wchar_t* p, HANDLE to)   // the file p appended to `to`, then removed
{
    HANDLE h = CreateFile(p, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return 0;
    static char buf[4096];   // (under the caller's lock)
    DWORD got, w, all = 0;
    while (ReadFile(h, buf, sizeof(buf), &got, NULL) && got) {
        WriteFile(to, buf, got, &w, NULL);
        all += got;
    }
    CloseHandle(h);
    DeleteFile(p);
    return all;
}

DWORD RamLogFlush(const wchar_t* ram, const wchar_t* old, HANDLE h)
{
    DWORD n = CopyOut(old, h);
    return n + CopyOut(ram, h);
}
