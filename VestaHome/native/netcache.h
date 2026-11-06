// netcache.h - the radio catalogue's answers kept in files (the user 2026-11-06): a list seen once opens at once and
// without a link. One file per request's path, its first line the path (another path with the same hash is no answer).
#ifndef NETCACHE_H
#define NETCACHE_H

#include <windows.h>

enum { kNcCacheFiles = 40, kNcCacheMaxBytes = 256 * 1024 };

// The answer kept for path, not older than maxAgeS (0: any age), as a new zero-closed buffer for free(); false: none.
bool NcCacheRead(const wchar_t* dir, const char* path, DWORD maxAgeS, unsigned char** body, int* len);
// Keeps it (replacing the older one); the oldest files beyond kNcCacheFiles go. An answer over kNcCacheMaxBytes is not
// kept. A failed write leaves what was there.
bool NcCacheWrite(const wchar_t* dir, const char* path, const unsigned char* body, int len);

#endif
