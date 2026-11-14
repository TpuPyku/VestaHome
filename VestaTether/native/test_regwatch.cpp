// regwatch.h: what changed between two snapshots of the loaded drivers (lines).
#include "regwatch.h"
#include <stdio.h>

static int s_checks, s_failed;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAIL: %s\n", what);
    }
}

int main()
{
    wchar_t out[256];
    Check(LinesNotIn(L"a\nb\n", L"a\nb\n", out, 256) == 0 && !out[0], "the same - nothing");
    Check(LinesNotIn(L"a\nb\nc\n", L"a\nc\n", out, 256) == 1 && !wcscmp(out, L"b\n"), "one gone");
    Check(LinesNotIn(L"x\ny\n", L"", out, 256) == 2 && !wcscmp(out, L"x\ny\n"), "all gone");
    Check(LinesNotIn(L"", L"a\n", out, 256) == 0, "nothing before");
    Check(LinesNotIn(L"ab\n", L"abc\n", out, 256) == 1, "a line is whole, not a prefix");
    Check(LinesNotIn(L"abc\n", L"ab\n", out, 256) == 1, "nor longer");
    Check(LinesNotIn(L"long line\nb\n", L"", out, 6) == 2 && wcslen(out) < 6, "out: cut to its room");
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
