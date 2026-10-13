// drilltag.h - "is this really the harmless drill package?" guard used by
// SPIPowerInstall.exe before it arms the factory boot updater SwDnl.exe (see ../SWDNL.md),
// plus the tag it looks for. Used by install/spipower_install.cpp and proven on the PC by
// tools/drilltag_hosttest.bat (that is why the very same source lives in a header).
//
// The package is BINARY (.upg = file blob + TLV TOC + S1 trailer), so the TOC contains NUL
// bytes: the "S2" record header is 'S','2',0x00,... - for the 357-byte drill package the
// first 0x00 sits at offset 257 while the "VestaDrill" file name sits at 314 in the S4
// record. A plain strstr() stops at that first NUL, so it silently rejected a
// byte-identical, md5-matched drill package on the HU (2026-10-11): the search below is
// byte-wise (memcmp) over the length actually read. The authoritative protection stays the
// md5 check of the package in fingerprints.h - this is only a cheap second guard.
//
// The drill package built by tools/make_drill.py is 357 bytes, so it always arrives in the
// first 4096-byte read; maxLen still bounds how much of an unknown file is scanned.
//
// Needs <windows.h> (CreateFile/ReadFile/CloseHandle/DWORD) and <string.h> (strlen/memcmp)
// included before it. Plain C89, so tools/drilltag_hosttest.c can compile it as C as well
// as the C++ ARM installer does. `static` on purpose: exactly one translation unit.
#ifndef SPIPOWER_DRILLTAG_H
#define SPIPOWER_DRILLTAG_H

static const char* kDrillTag = "VestaDrill";   // file name inside the drill package

static int FileHasDrillTag(const wchar_t* path, DWORD maxLen)
{
    HANDLE h;
    char buf[4096];
    DWORD total = 0, got = 0, i;
    int found = 0;
    int tagLen = (int)strlen(kDrillTag);

    h = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    while (ReadFile(h, buf, sizeof(buf), &got, NULL) && got) {
        for (i = 0; i + (DWORD)tagLen <= got; i++) {
            if (memcmp(buf + i, kDrillTag, (size_t)tagLen) == 0) { found = 1; break; }
        }
        if (found) break;
        total += got;
        if (total >= maxLen) break;
    }
    CloseHandle(h);
    return found;
}

#endif
