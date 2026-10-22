// vh_skin.inc - part of home.cpp (included there, in order): the skin and its export.

// ---------------------------------------------------------------- skin

struct SkinColor { const wchar_t* key; COLORREF* c; const char* note; };
static const SkinColor kSkinColors[] = {
    { L"ink",     &kInk,     "background" },
    { L"panel",   &kPanel,   "tiles, buttons, widgets" },
    { L"line",    &kLine,    "borders" },
    { L"text",    &kText,    "main text, built-in icons" },
    { L"muted",   &kMuted,   "secondary text" },
    { L"dim",     &kDim,     "inactive text" },
    { L"accent",  &kOrange,  "pressed button, active mode, headings" },
    { L"facet",   &kFacet,   "tile corner" },
    { L"mosaic1", &kTri[0],  "built-in background triangles (unused with a background picture)" },
    { L"mosaic2", &kTri[1],  "" },
    { L"mosaic3", &kTri[2],  "" },
};

// PNG icon into premultiplied RGBA. Up to 256 x 256: icons only, keeps memory bounded.
static bool LoadImg(const wchar_t* path, Img* im)
{
    DWORD size;
    char* data = ReadAll(path, &size);
    if (!data)
        return false;
    int comp;
    im->px = stbi_load_from_memory((unsigned char*)data, size, &im->w, &im->h, &comp, 4);
    free(data);
    if (im->px && (im->w > 256 || im->h > 256)) {
        stbi_image_free(im->px);
        im->px = NULL;
    }
    if (!im->px)
        return false;
    for (int i = 0; i < im->w * im->h; i++) {
        unsigned char* p = im->px + i * 4;
        for (int k = 0; k < 3; k++)
            p[k] = (unsigned char)(p[k] * p[3] / 255);
    }
    return true;
}

// Skin\skin.ini: [colors] name=RRGGBB, [images] name=file.png (in the Skin folder). Read once at start.
static void LoadSkin()
{
    wchar_t path[MAX_PATH];
    _snwprintf(path, MAX_PATH, L"%sSkin\\skin.ini", s_dir);
    DWORD size;
    char* buf = ReadAll(path, &size);
    if (!buf)
        return;
    bool images = false;
    char* p = Utf8Text(buf);
    wchar_t line[300];
    while (NextUtf8Line(&p, line, 300)) {
        wchar_t* semi = wcschr(line, L';');
        if (semi)
            *semi = 0;
        Trim(line);
        if (line[0] == L'[') {
            images = !_wcsicmp(line, L"[images]");
            continue;
        }
        wchar_t* eq = wcschr(line, L'=');
        if (!eq)
            continue;
        *eq = 0;
        wchar_t* val = eq + 1;
        Trim(line);
        Trim(val);
        if (!images) {
            if (*val == L'#')
                val++;
            wchar_t* e;
            unsigned long v = wcstoul(val, &e, 16);
            if (e - val != 6)
                continue;
            for (int k = 0; k < (int)(sizeof(kSkinColors) / sizeof(kSkinColors[0])); k++)
                if (!_wcsicmp(line, kSkinColors[k].key))
                    *kSkinColors[k].c = RGB(v >> 16, (v >> 8) & 255, v & 255);
            continue;
        }
        if (!val[0])
            continue;
        wchar_t file[MAX_PATH];
        _snwprintf(file, MAX_PATH, L"%sSkin\\%s", s_dir, val);
        file[MAX_PATH - 1] = 0;
        if (!_wcsicmp(line, L"background")) {
            wcscpy(s_skinBg, file);
            continue;
        }
        if (!_wcsicmp(line, L"saver")) {
            wcscpy(s_skinSaver, file);
            continue;
        }
        if (!_wcsicmp(line, L"tile")) {
            wcscpy(s_skinTile, file);
            continue;
        }
        int len = (int)wcslen(line);
        int down = len > 5 && !_wcsicmp(line + len - 5, L"_down");
        if (down)
            line[len - 5] = 0;
        for (int ic = 0; ic < IC_COUNT; ic++)
            if (kIconKey[ic] && !_wcsicmp(line, kIconKey[ic]) && !s_icons[ic][down].px)
                LoadImg(file, &s_icons[ic][down]);
    }
    free(buf);
}

#ifndef UNDER_CE
// ---------------------------------------------------------------- skin export (PC: VestaHomePC.exe /export)

static unsigned long Crc32(unsigned long c, const unsigned char* p, unsigned long n)
{
    c = ~c;
    while (n--) {
        c ^= *p++;
        for (int k = 0; k < 8; k++)
            c = (c >> 1) ^ (0xEDB88320UL & (0 - (c & 1)));
    }
    return ~c;
}

static void PutBE(unsigned char* p, unsigned long v)
{
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

static void PngChunk(FILE* f, const char* type, const unsigned char* data, unsigned long n)
{
    unsigned char b[4];
    PutBE(b, n);
    fwrite(b, 1, 4, f);
    fwrite(type, 1, 4, f);
    fwrite(data, 1, n, f);
    PutBE(b, Crc32(Crc32(0, (const unsigned char*)type, 4), data, n));
    fwrite(b, 1, 4, f);
}

// Minimal PNG writer, 8-bit RGB (ch = 3) or RGBA (ch = 4), deflate "stored" blocks: no compression.
static void WritePng(const wchar_t* path, const unsigned char* px, int w, int h, int ch)
{
    unsigned long line = w * ch + 1, raw = line * h;
    unsigned char* rows = (unsigned char*)malloc(raw);
    unsigned char* z = (unsigned char*)malloc(raw + raw / 65535 * 5 + 16);
    for (int y = 0; y < h; y++) {
        rows[y * line] = 0;   // filter: none
        memcpy(rows + y * line + 1, px + y * w * ch, w * ch);
    }
    unsigned char* q = z;
    *q++ = 0x78;
    *q++ = 0x01;
    for (unsigned long i = 0; i < raw; i += 65535) {
        unsigned long n = raw - i < 65535 ? raw - i : 65535;
        *q++ = i + n == raw ? 1 : 0;
        *q++ = (unsigned char)n;
        *q++ = (unsigned char)(n >> 8);
        *q++ = (unsigned char)~n;
        *q++ = (unsigned char)(~n >> 8);
        memcpy(q, rows + i, n);
        q += n;
    }
    unsigned long a = 1, b = 0;
    for (unsigned long i = 0; i < raw; i++) {
        a = (a + rows[i]) % 65521;
        b = (b + a) % 65521;
    }
    PutBE(q, (b << 16) | a);
    q += 4;
    unsigned char hdr[13];
    PutBE(hdr, w);
    PutBE(hdr + 4, h);
    hdr[8] = 8;
    hdr[9] = ch == 4 ? 6 : 2;
    hdr[10] = hdr[11] = hdr[12] = 0;
    FILE* f = _wfopen(path, L"wb");
    if (f) {
        fwrite("\x89PNG\r\n\x1a\n", 1, 8, f);
        PngChunk(f, "IHDR", hdr, 13);
        PngChunk(f, "IDAT", z, (unsigned long)(q - z));
        PngChunk(f, "IEND", NULL, 0);
        fclose(f);
    }
    free(rows);
    free(z);
}

// Writes the built-in icons to Skin\ as <name>.png (64 x 64, drawn 4x larger white on black and averaged
// into the alpha channel: antialiased lines) and skin.ini with the built-in colours. No pressed variants and
// no background picture: they cost memory on the unit (the pressed state tints the icon dark, the background
// is the built-in mosaic). Doesn't read the existing skin.ini.
static void ExportSkin()
{
    wchar_t dir[MAX_PATH], path[MAX_PATH];
    _snwprintf(dir, MAX_PATH, L"%sSkin\\", s_dir);
    CreateDirectory(dir, NULL);

    BITMAPINFO bi;
    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = 800;
    bi.bmiHeader.biHeight = -480;   // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = NULL;
    HBITMAP bmp = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    HDC dc = CreateCompatibleDC(NULL);
    HGDIOBJ ob = SelectObject(dc, bmp);
    const unsigned* px = (const unsigned*)bits;   // 0x00RRGGBB

    static unsigned char out[64 * 64 * 4];
    for (int ic = 0; ic < IC_COUNT; ic++) {
        if (!kIconKey[ic])
            continue;
        Fill(dc, 0, 0, 256, 256, RGB(0, 0, 0));
        PaintIcon(dc, (Icon)ic, 0, 0, 256, RGB(255, 255, 255));
        GdiFlush();
        for (int y = 0; y < 64; y++)
            for (int x = 0; x < 64; x++) {
                int sum = 0;
                for (int j = 0; j < 4; j++)
                    for (int i = 0; i < 4; i++)
                        sum += (px[(y * 4 + j) * 800 + x * 4 + i] >> 16) & 255;
                unsigned char* o = out + (y * 64 + x) * 4;
                o[0] = GetRValue(kText);
                o[1] = GetGValue(kText);
                o[2] = GetBValue(kText);
                o[3] = (unsigned char)(sum / 16);
            }
        _snwprintf(path, MAX_PATH, L"%s%s.png", dir, kIconKey[ic]);
        WritePng(path, out, 64, 64, 4);
    }
    SelectObject(dc, ob);
    DeleteDC(dc);
    DeleteObject(bmp);

    _snwprintf(path, MAX_PATH, L"%sskin.ini", dir);
    FILE* f = _wfopen(path, L"w");
    if (!f)
        return;
    fprintf(f, "; VestaHome skin: the Skin folder next to VestaHome.exe. Read once at start.\n"
               "; [colors]: RRGGBB. [images]: PNG files in this folder, transparency supported.\n"
               "; Icons are square and drawn scaled: 60 px on tiles, 40 px on buttons, other sizes on the\n"
               "; other screens, where they are tinted with that screen's colour unless it is the text colour.\n"
               "; Optional: <name>_down = a separate pressed picture (otherwise the icon is tinted dark),\n"
               ";   background = an 800 x 480 picture for the main menu instead of the built-in mosaic,\n"
               ";   saver = the screen saver's 800 x 480 picture (kept only while it shows),\n"
               ";   tile = a picture (PNG or JPG, 8..400 px a side) repeated over the background of every\n"
               ";   screen instead of the built-in triangles (the main menu too, unless background is given).\n"
               "; Each picture costs memory: a 64 x 64 icon 16 KB, the background 750 KB.\n"
               "; A missing line or file falls back to the built-in drawing.\n\n[colors]\n");
    for (int k = 0; k < (int)(sizeof(kSkinColors) / sizeof(kSkinColors[0])); k++) {
        COLORREF c = *kSkinColors[k].c;
        fprintf(f, "%-8S= %02X%02X%02X%s%s\n", kSkinColors[k].key, GetRValue(c), GetGValue(c), GetBValue(c),
                kSkinColors[k].note[0] ? "   ; " : "", kSkinColors[k].note);
    }
    fprintf(f, "\n[images]\n");
    for (int ic = 0; ic < IC_COUNT; ic++)
        if (kIconKey[ic])
            fprintf(f, "%S = %S.png\n", kIconKey[ic], kIconKey[ic]);
    fclose(f);
}
#endif
