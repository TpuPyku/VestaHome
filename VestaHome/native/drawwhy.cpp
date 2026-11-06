// drawwhy.cpp - see drawwhy.h. On the window's thread only; a frame or two a second: a plain search is enough.
#include "drawwhy.h"
#include <stdio.h>
#include <string.h>

enum { kCallers = 32, kShown = 3, kNameCap = 24 };
struct Caller { char file[kNameCap]; int line, count; };
static Caller s_caller[kCallers];
static int s_callers;

static const char* BaseName(const char* path)
{
    const char* base = path;
    for (const char* p = path; *p; p++)
        if (*p == '\\' || *p == '/')
            base = p + 1;
    return base;
}

void WhyCount(const char* file, int line)
{
    const char* name = BaseName(file);
    for (int i = 0; i < s_callers; i++)
        if (s_caller[i].line == line && !strncmp(s_caller[i].file, name, kNameCap - 1)) {
            s_caller[i].count++;
            return;
        }
    if (s_callers == kCallers)
        return;   // (a count for the log: the frequent ones came long ago)
    Caller& c = s_caller[s_callers++];
    strncpy(c.file, name, kNameCap - 1);
    c.file[kNameCap - 1] = 0;
    c.line = line;
    c.count = 1;
}

void WhyTake(wchar_t* out, int cap)
{
    out[0] = 0;
    int used = 0;
    for (int k = 0; k < kShown; k++) {
        int best = -1;
        for (int i = 0; i < s_callers; i++)
            if (s_caller[i].count > 0 && (best < 0 || s_caller[i].count > s_caller[best].count))
                best = i;
        if (best < 0)
            break;
        int got = _snwprintf(out + used, cap - used, L"%s%S:%d x%d", k ? L", " : L"", s_caller[best].file,
                             s_caller[best].line, s_caller[best].count);
        s_caller[best].count = 0;
        if (got < 0 || got >= cap - used) {   // (cut: -1, and no zero at its end)
            out[cap - 1] = 0;
            break;
        }
        used += got;
    }
    s_callers = 0;
}
