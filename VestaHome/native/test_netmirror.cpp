// The radio catalogue's mirror (netmirror.h) on the PC.
#include <stdio.h>
#include <string.h>
#include "netmirror.h"

static int s_checks, s_failed;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAIL: %s\n", what);
    }
}

static bool Found(const char* file, const char* path, const char* want)
{
    const unsigned char* b = 0;
    int n = -1;
    if (!NcMirrorFind((const unsigned char*)file, (int)strlen(file), path, &b, &n))
        return want == 0;
    return want && n == (int)strlen(want) && !memcmp(b, want, n);
}

int main()
{
    const char* f = "VHRM1\n/json/a?x=1\n3\n[1]/json/b\n7\n[\"b\",2]";
    Check(Found(f, "/json/a?x=1", "[1]"), "the first answer");
    Check(Found(f, "/json/b", "[\"b\",2]"), "the last answer, its length to the end");
    Check(Found(f, "/json/c", 0), "a path not there - none");
    Check(Found(f, "/json/a", 0), "a path's beginning only - not that answer");
    Check(Found("VHRM2\n/json/b\n2\n[]", "/json/b", 0), "another form - none");
    Check(Found("VHRM1\n/json/b\n9\n[1]", "/json/b", 0), "a length past the end - none");
    Check(Found("VHRM1\n/json/a\nxx\n[1]/json/b\n2\n[]", "/json/b", 0), "a length not a number - stop, none");
    Check(Found("VHRM1\n/json/b", "/json/b", 0), "cut after its path - none");
    Check(Found("", "/json/b", 0), "empty - none");
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
