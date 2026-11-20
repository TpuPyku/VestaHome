// pecheck.h - HuCheck: what a firmware file is, read from its bytes (no LoadLibrary: a vendor DLL loaded into the
// probe could start its own work) - a PE's machine, build time and export names. Pure: no files, no windows.
#pragma once

struct PeFacts {
    bool     pe;        // an MZ / PE file
    unsigned machine;   // IMAGE_FILE_HEADER.Machine (0x1C2 ARM with Thumb - the unit's, 0x1C0 ARM, 0x14C x86)
    unsigned stamp;     // its TimeDateStamp (the build time, seconds since 1970)
    int      exports;   // its export names, 0 none
};

bool PeRead(const unsigned char* b, int len, PeFacts* f);   // false: not a PE (f->pe false)
bool PeHasExport(const unsigned char* b, int len, const char* name);
// The names of want[] not exported: their indexes into miss (at most cap); the count missing (all of them when not
// a PE).
int  PeMissing(const unsigned char* b, int len, const char* const* want, int n, int* miss, int cap);
