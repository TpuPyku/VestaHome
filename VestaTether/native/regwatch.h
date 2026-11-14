// regwatch.h - TetherProbe: what came and went among the loaded drivers (HKLM\Drivers\Active, a line each) while the
// phone is plugged in (sheet 34 p. 24-25: the phone only charged, our driver never called - did USBD see it at all,
// did another driver take it?). Snapshots are '\n'-ended lines.
#ifndef REGWATCH_H
#define REGWATCH_H
#include <wchar.h>

// The lines of a that b has not, into out (cap characters, cut short there); their count
static int LinesNotIn(const wchar_t* a, const wchar_t* b, wchar_t* out, int cap)
{
    int n = 0, k = 0;
    out[0] = 0;
    for (const wchar_t* l = a; *l;) {
        const wchar_t* e = wcschr(l, L'\n');
        int len = e ? (int)(e - l) : (int)wcslen(l);
        bool found = false;
        for (const wchar_t* m = b; *m && !found;) {
            const wchar_t* f = wcschr(m, L'\n');
            int ml = f ? (int)(f - m) : (int)wcslen(m);
            found = ml == len && !wcsncmp(l, m, len);
            m += ml + (f ? 1 : 0);
        }
        if (!found) {
            n++;
            for (int i = 0; i <= len && k < cap - 1; i++)
                out[k++] = i < len ? l[i] : L'\n';
            out[k] = 0;
        }
        l += len + (e ? 1 : 0);
    }
    return n;
}

#endif
