// musdir.h - a media browser's folder (music, video, photos): its subfolders and the files it wants, sorted - folders
// first ("Name\"), then the files. The folders are read first and kept whole; the files take the room left (the user
// 2026-11-20: a card with 400+ files in its root lost its music folders - the list stopped at 400 in the disk's order).
#ifndef MUSDIR_H
#define MUSDIR_H

#include <windows.h>

typedef bool (*DirWant)(const wchar_t* name);

// `dir` in the "X\" form; v gets malloc'ed names (DirFree). Hidden ones and ".x" are left out. *cut: not all fitted.
int  DirList(const wchar_t* dir, wchar_t** v, int cap, bool folders, DirWant want, bool* cut);
void DirFree(wchar_t** v, int n);
int  DirNameCmp(const void* a, const void* b);   // by name, case aside (qsort)

#endif
