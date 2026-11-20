// pefile.h: the vendor's version and the functions present, read from files.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "pefile.h"

static int s_checks, s_failed;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAIL: %s\n", what);
    }
}

// A file of zeros with the shell's string at off.
static void Make(const wchar_t* path, int size, int off, const char* text)
{
    static unsigned char b[300 * 1024];
    memset(b, 0, sizeof(b));
    memcpy(b + off, text, strlen(text));
    HANDLE f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    DWORD put = 0;
    WriteFile(f, b, size, &put, NULL);
    CloseHandle(f);
}

static void TestVersion()
{
    const wchar_t* p = L"obj\\fw_test.bin";
    Make(p, 200 * 1024, 100, "Itelma_TR8005_14_02_41.25_190828_R");
    Check(FwFileVersion(p, 2) == 4125, "the string in the first piece");
    Make(p, 200 * 1024, 64 * 1024 - 10, "Itelma_TR8005_14_02_41.20_180426_R");
    Check(FwFileVersion(p, 2) == 4120, "the string across the first two pieces (64 KB)");
    Make(p, 200 * 1024, 2 * 64 * 1024 - 5, "Itelma_TR8005_14_02_41.23_R");
    Check(FwFileVersion(p, 2) == 4123, "across the second and the third");
    Make(p, 200 * 1024, 199 * 1024, "Itelma_TR8005_14_02_41.18_R");
    Check(FwFileVersion(p, 2) == 4118, "in the last piece");
    Check(FwFileVersion(p, 1) == 0, "another part - none");
    Make(p, 10, 0, "");
    Check(FwFileVersion(p, 2) == 0, "a short file without it - 0");
    Check(FwFileVersion(L"obj\\no_such_file.bin", 2) == 0, "no file - 0");
    DeleteFileW(p);
}

static void TestMissing()
{
    static const char* const want[] = { "GetTickCount", "Sleep", "NoSuchFunctionHere" };
    wchar_t k32[MAX_PATH];
    GetSystemDirectoryW(k32, MAX_PATH);
    wcscat(k32, L"\\kernel32.dll");
    Check(PeMissingFile(k32, want, 2) == 0, "kernel32 has GetTickCount and Sleep");
    Check(PeMissingFile(k32, want, 3) == 1, "but not the third");
    Check(PeMissingFile(L"obj\\no_such.dll", want, 2) == -1, "no file - -1");
    // (the tester's 4120, 2026-11-20: \Windows\desay_sdk_api.dll is loaded by main.exe and the shell, but not there as
    // a file - the setup said "3 of 3 missing"): no such file, yet a module that loads - its functions
    Check(PeMissingFile(L"kernel32.dll", want, 3) == 1, "not a file here, but a module that loads: checked loaded");
    // (read, but no PE in it - a ROM module's file on the unit - then asked of the loaded module: the desktop loads a
    // path's own file, so not to be shown here; the unit only)
    const wchar_t* p = L"obj\\notpe.bin";
    Make(p, 100, 0, "not a PE");
    Check(PeMissingFile(p, want, 3) == 3, "a file but no PE - all missing");
    // there but not to be read (as a module of the unit's ROM) and not loading either - -1
    HANDLE lock = CreateFileW(p, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    Check(PeMissingFile(p, want, 3) == -1, "there, not readable, not loading - -1");
    CloseHandle(lock);
    DeleteFileW(p);
}

int main()
{
    TestVersion();
    TestMissing();
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
