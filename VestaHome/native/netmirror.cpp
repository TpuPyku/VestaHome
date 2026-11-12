// netmirror.cpp - the radio catalogue's mirror read (see netmirror.h).
#include <string.h>
#include <stdlib.h>
#include "netmirror.h"

// The line from at up to "\n" (its length; -1: no "\n" before the end)
static int Line(const unsigned char* at, const unsigned char* end)
{
    const unsigned char* nl = (const unsigned char*)memchr(at, '\n', end - at);
    return nl ? (int)(nl - at) : -1;
}

bool NcMirrorFind(const unsigned char* data, int len, const char* path, const unsigned char** body, int* blen)
{
    const unsigned char* at = data, *end = data + len;
    if (len < 6 || memcmp(data, "VHRM1\n", 6))
        return false;
    int want = (int)strlen(path);
    for (at += 6; at < end;) {
        int pl = Line(at, end);
        if (pl < 0)
            return false;
        const unsigned char* num = at + pl + 1;
        int nl = Line(num, end), n = 0;
        if (nl <= 0 || nl > 9)
            return false;
        for (int i = 0; i < nl; i++) {
            if (num[i] < '0' || num[i] > '9')
                return false;
            n = n * 10 + (num[i] - '0');
        }
        const unsigned char* b = num + nl + 1;
        if (n > end - b)
            return false;
        if (pl == want && !memcmp(at, path, want)) {
            *body = b;
            *blen = n;
            return true;
        }
        at = b + n;
    }
    return false;
}
