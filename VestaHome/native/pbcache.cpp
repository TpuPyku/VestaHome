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
