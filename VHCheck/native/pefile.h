// pefile.h - pecheck.h's facts read from the unit's files (VHCheck, VestaHome's "Details", the installer's gate):
// the reading in one place.
#pragma once

// The vendor's version (pecheck.h FwVersion) in a file, read in 64 KB pieces (a string across two kept): aabb, 0 none.
int FwFileVersion(const wchar_t* path, int part);

// The functions of want[] the module at path lacks: read as a file (pecheck.h PeMissing), or - a module of the ROM,
// there but no file to read - asked of the loaded module. -1: not there, or a ROM module that does not load.
int PeMissingFile(const wchar_t* path, const char* const* want, int n);
