// cover.cpp - where a track's cover is: the picture inside its ID3v2 tag (mp3), or the picture of its folder that
// stands for the album. Only finding: the caller reads the files and decodes (photo.cpp). No UI, no files here.
#pragma once
#include <windows.h>
#include "tagtext.h"

// The whole ID3v2 tag's size (its 10-byte header, the body, a v2.4 footer) from the file's first n bytes; 0 when
// they are not an ID3v2 tag.
int  Id3TagSize(const unsigned char* head, int n);

// The picture inside a whole tag (n bytes from its header): APIC (v2.3, v2.4) or PIC (v2.2), the front cover
// (type 3) before the others, only a JPEG or a PNG. off / len: the picture's bytes in tag. The tag may be changed
// in place (unsynchronisation undone).
bool Id3Picture(unsigned char* tag, int n, int* off, int* len);

// The texts in a tag (n bytes from its header; the head of a big tag will do - a frame cut off is not read): TIT2,
// TPE1, TALB, TYER / TDRC (its year) - v2.2 TT2, TP1, TAL, TYE; each one's first frame, only the fields of t still
// empty; its one-byte text 1251 or 866 (tagtext.h). The tag may be changed in place. false: none found.
bool Id3Texts(unsigned char* tag, int n, TagTexts* t);

// The folder's picture for its album among the file names (no paths): cover, folder, front, Windows' AlbumArt
// (large first), a name with "cover" or "front"; otherwise the folder's only picture. -1: none.
int  CoverPick(const wchar_t* const* names, int n);
