// photo.cpp - pictures for VestaHome's photo screens. JPEG through TJpgDec (third_party/tjpgd): decoded straight
// to RGB565 and scaled down by 1/2..1/8 while decoding, with a few KB of working memory, the file read as a
// stream - a 12 MP photo never needs its full size in RAM. PNG, BMP and the JPEGs TJpgDec can't read
// (progressive) through stb_image, up to kStbMaxPx. The EXIF thumbnail and orientation. Pixels RGB565, rows
// top-down. No UI, no threads: the caller decides where it runs (a photo takes a second or two on the unit).
#pragma once
#include <windows.h>

struct Pic {
    int             w, h;
    unsigned short* px;   // w * h, malloc'ed (PicFree)
};

enum { PIC_OK, PIC_FORMAT, PIC_BIG, PIC_READ, PIC_MEMORY };

// Whether the file name is a picture these read: .jpg .jpeg .png .bmp
bool PicIsName(const wchar_t* name);

// The picture fitted into box w x h (fill: covering the box, the overflow cut off; otherwise all of it inside),
// turned rot quarter turns clockwise, plus the EXIF orientation when exifRot. The result's size: fill - the box;
// fit - the picture's own proportions, at most the box. srcW / srcH (may be NULL): the file's own size.
int  PicLoad(const wchar_t* path, int boxW, int boxH, bool fill, int rot, bool exifRot, Pic* out, int* srcW,
             int* srcH);

// A thumbnail filling w x h: the EXIF thumbnail when the JPEG has one (fast), else the picture at 1/8.
int  PicThumb(const wchar_t* path, int w, int h, bool exifRot, Pic* out);

void PicFree(Pic* p);
