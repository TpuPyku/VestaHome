// inikey.cpp - one ini key set. See inikey.h.
#include "inikey.h"
#include <string.h>

// The line at p (up to end) is the key's: the key, then spaces, then '=' or the line's end
static bool KeyLine(const char* p, const char* end, const char* key, int kl)
{
    if (end - p < kl || memcmp(p, key, kl))
        return false;
    p += kl;
    while (p < end && *p == ' ')
        p++;
    return p == end || *p == '=' || *p == '\r' || *p == '\n';
}

static bool Append(char* out, int cap, int* n, const char* s, int len)
{
    if (len > cap - *n)
        return false;
    memcpy(out + *n, s, len);
    *n += len;
    return true;
}

// The key's line (the first), end if none
static const char* FindKey(const char* p, const char* end, const char* key, int kl)
{
    while (p < end && !KeyLine(p, end, key, kl)) {
        const char* nl = (const char*)memchr(p, '\n', end - p);
        p = nl ? nl + 1 : end;
    }
    return p;
}

static const char* LineEnd(const char* p, const char* end)   // (its CR LF / LF not in it)
{
    while (p < end && *p != '\r' && *p != '\n')
        p++;
    return p;
}

// A line added after the last one: that one's end first, when it has none
static bool NeedsBreak(const char* in, const char* p, const char* end)
{
    return p == end && p != in && end[-1] != '\n';
}

static bool PutLine(char* out, int cap, int* n, const char* key, int kl, const char* sep, const char* val)
{
    return Append(out, cap, n, key, kl) && Append(out, cap, n, sep, (int)strlen(sep)) &&
           Append(out, cap, n, val, (int)strlen(val));
}

int IniPut(const char* in, int len, const char* key, const char* val, char* out, int cap, bool* changed)
{
    return IniPutSep(in, len, key, " = ", val, out, cap, changed);
}

int IniPutSep(const char* in, int len, const char* key, const char* sep, const char* val, char* out, int cap,
              bool* changed)
{
    int kl = (int)strlen(key), n = 0;
    const char *end = in + len, *p = FindKey(in, end, key, kl), *le = LineEnd(p, end);
    bool add = p == end;
    bool ok = Append(out, cap, &n, in, (int)(p - in)) &&
              (!NeedsBreak(in, p, end) || Append(out, cap, &n, "\r\n", 2)) && PutLine(out, cap, &n, key, kl, sep, val) &&
              (add ? Append(out, cap, &n, "\r\n", 2) : Append(out, cap, &n, le, (int)(end - le)));
    if (!ok)
        return -1;
    *changed = n != len || memcmp(out, in, n);
    return n;
}
