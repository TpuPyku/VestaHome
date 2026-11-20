// pecheck.cpp - a PE's facts and export names from its bytes (pecheck.h). Every read is bounded by the reader itself
// (out of the file: 0), so a short or broken file reads as "not a PE" or "no exports", never past its end.
#include <string.h>
#include "pecheck.h"

struct Pe {
    const unsigned char* b;
    int len, sec, nsec;
};

static unsigned U16(const Pe& p, unsigned o)
{
    return o + 2 <= (unsigned)p.len ? p.b[o] | p.b[o + 1] << 8 : 0;
}

static unsigned U32(const Pe& p, unsigned o)
{
    return o + 4 <= (unsigned)p.len ? U16(p, o) | U16(p, o + 2) << 16 : 0;
}

// An RVA's file offset through the section table; -1 outside them or past the file.
static int Off(const Pe& p, unsigned rva)
{
    for (int i = 0; i < p.nsec; i++) {
        unsigned s = p.sec + 40 * i, va = U32(p, s + 12), raw = U32(p, s + 20);
        if (rva - va < U32(p, s + 16))
            return raw + (rva - va) < (unsigned)p.len ? (int)(raw + (rva - va)) : -1;
    }
    return -1;
}

// The export directory's name table: its offset and count (0: none, or it does not fit in the file).
static int Names(const Pe& p, unsigned opt, int* names)
{
    int exp = U32(p, opt + 92) ? Off(p, U32(p, opt + 96)) : -1;
    *names = exp < 0 ? -1 : Off(p, U32(p, exp + 32));
    unsigned n = U32(p, exp + 24);
    return *names >= 0 && n <= (unsigned)(p.len - *names) / 4 ? (int)n : 0;
}

static bool Open(const unsigned char* b, int len, Pe* p, PeFacts* f, int* names)
{
    memset(f, 0, sizeof(*f));
    p->b = b;
    p->len = len;
    p->nsec = 0;
    unsigned e = U32(*p, 0x3C);
    if (U16(*p, 0) != ('M' | 'Z' << 8) || U32(*p, e) != 0x4550)   // "MZ", "PE\0\0"
        return false;
    p->nsec = (int)U16(*p, e + 6);
    p->sec = (int)(e + 24 + U16(*p, e + 20));
    f->pe = true;
    f->machine = U16(*p, e + 4);
    f->stamp = U32(*p, e + 8);
    f->exports = Names(*p, e + 24, names);
    return true;
}

bool PeRead(const unsigned char* b, int len, PeFacts* f)
{
    Pe p;
    int names;
    return Open(b, len, &p, f, &names);
}

bool PeHasExport(const unsigned char* b, int len, const char* name)
{
    Pe p;
    PeFacts f;
    int names;
    Open(b, len, &p, &f, &names);
    size_t k = strlen(name);
    for (int i = 0; i < f.exports; i++) {
        int s = Off(p, U32(p, names + 4 * i));
        if (s >= 0 && (size_t)(len - s) > k && !memcmp(b + s, name, k + 1))
            return true;
    }
    return false;
}

int PeMissing(const unsigned char* b, int len, const char* const* want, int n, int* miss, int cap)
{
    int m = 0;
    for (int i = 0; i < n; i++)
        if (!PeHasExport(b, len, want[i])) {
            if (m < cap)
                miss[m] = i;
            m++;
        }
    return m;
}

int FwKnown(int part, unsigned stamp)
{
    // (part, the build's UTC day since 1970, the firmware) - from MAIN's 4120 and 4125 and the CAN panel's 4123
    static const int kKnown[][3] = { { FW_OS, 17634, 4120 }, { FW_OS, 17857, 4123 }, { FW_OS, 18240, 4125 },
                                     { FW_SHELL, 17647, 4120 }, { FW_SHELL, 18136, 4125 },
                                     { FW_SPM, 17486, 4120 }, { FW_SPM, 18136, 4125 } };
    int day = (int)(stamp / 86400);
    for (int i = 0; i < (int)(sizeof(kKnown) / sizeof(kKnown[0])); i++)
        if (kKnown[i][0] == part && kKnown[i][1] == day)
            return kKnown[i][2];
    return 0;
}
