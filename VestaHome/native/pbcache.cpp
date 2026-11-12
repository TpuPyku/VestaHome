// The phone book's cache beside VestaHome (see pbcache.h).
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include "pbcache.h"

enum { kHead = 4 + 4 + 18 + 4 + 4 + 4, kEntry = sizeof(BtPbEntry) };

static void PutInt(unsigned char* p, int v)
{
    memcpy(p, &v, 4);
}

static int GetInt(const unsigned char* p)
{
    int v;
    memcpy(&v, p, 4);
    return v;
}

int PbPack(const char* phone, int storage, const BtPbEntry* book, int nb, const BtPbEntry* calls, int nc,
           unsigned char* out, int cap)
{
    int b = nb > 0 ? nb : 0, c = nc > 0 ? nc : 0, len = kHead + (b + c) * kEntry;
    if (!out)
        return len;
    if (cap < len)
        return 0;
    memcpy(out, "VHPB", 4);
    PutInt(out + 4, 1);
    memset(out + 8, 0, 18);
    strncpy((char*)out + 8, phone, 17);
    PutInt(out + 26, storage);
    PutInt(out + 30, nb);
    PutInt(out + 34, nc);
    if (b)
        memcpy(out + kHead, book, b * kEntry);
    if (c)
        memcpy(out + kHead + b * kEntry, calls, c * kEntry);
    return len;
}

// A list's count read: -1 (not pulled) .. BT_PB_MAX; anything else - not ours
static bool CountOk(int n)
{
    return n >= -1 && n <= BT_PB_MAX;
}

static BtPbEntry* Copied(const unsigned char* from, int n)
{
    if (n <= 0)
        return NULL;
    BtPbEntry* e = (BtPbEntry*)malloc(n * kEntry);
    if (e)
        memcpy(e, from, n * kEntry);
    return e;
}

bool PbUnpack(const unsigned char* in, int len, char phone[18], int* storage, BtPbEntry** book, int* nb,
              BtPbEntry** calls, int* nc)
{
    if (len < kHead || memcmp(in, "VHPB", 4) || GetInt(in + 4) != 1)
        return false;
    int b = GetInt(in + 30), c = GetInt(in + 34);
    if (!CountOk(b) || !CountOk(c) || len != kHead + ((b > 0 ? b : 0) + (c > 0 ? c : 0)) * kEntry)
        return false;
    memcpy(phone, in + 8, 18);
    phone[17] = 0;
    *storage = GetInt(in + 26);
    *nb = b;
    *nc = c;
    *book = Copied(in + kHead, b);
    *calls = Copied(in + kHead + (b > 0 ? b : 0) * kEntry, c);
    return true;
}

// ---- the book's order
static wchar_t Lower(wchar_t c)   // Latin and Cyrillic; ё as е
{
    if ((c >= L'A' && c <= L'Z') || (c >= 0x410 && c <= 0x42F))
        return (wchar_t)(c + 0x20);
    return c == 0x401 || c == 0x451 ? (wchar_t)0x435 : c;
}

static void SortKey(const char* s, wchar_t* out, int cap)   // UTF-8 (up to 3 bytes a character) lowered
{
    const unsigned char* p = (const unsigned char*)s;
    int n = 0;
    while (*p && n < cap - 1) {
        int more = *p >= 0xE0 ? 2 : *p >= 0xC0 ? 1 : 0;
        wchar_t c = (wchar_t)(more == 2 ? *p & 0x0F : more ? *p & 0x1F : *p);
        for (p++; more && (*p & 0xC0) == 0x80; more--, p++)
            c = (wchar_t)(c << 6 | (*p & 0x3F));
        out[n++] = Lower(c);
    }
    out[n] = 0;
}

static int Rank(const wchar_t* k)   // Cyrillic, Latin, the rest, no name
{
    return !k[0] ? 3 : k[0] >= 0x430 && k[0] <= 0x44F ? 0 : k[0] >= L'a' && k[0] <= L'z' ? 1 : 2;
}

static int ByName(const void* a, const void* b)
{
    wchar_t ka[64], kb[64];
    SortKey(((const BtPbEntry*)a)->name, ka, 64);
    SortKey(((const BtPbEntry*)b)->name, kb, 64);
    int ra = Rank(ka), rb = Rank(kb);
    return ra != rb ? ra - rb : wcscmp(ka, kb);
}

void PbSortBook(BtPbEntry* book, int n)
{
    if (n > 1)
        qsort(book, n, sizeof(BtPbEntry), ByName);
}
