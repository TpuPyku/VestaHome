// md5_hosttest.c - PC-side proof that install/md5.h is byte-correct: it compiles the
// very same header the ARM installer uses and checks it against the RFC 1321 vectors
// and against file hashes produced by Python's hashlib.
//
//   tools\md5_hosttest.bat            -> vector self-test (no arguments)
//   tools\md5_hosttest.bat <file>...  -> "md5  path" lines for hashlib comparison
#include <stdio.h>
#include "../install/md5.h"

static int RunVectors(void)
{
    struct { const char* text; const char* want; } v[] = {
        { "",                                                     "d41d8cd98f00b204e9800998ecf8427e" },
        { "a",                                                    "0cc175b9c0f1b6a831c399e269772661" },
        { "abc",                                                  "900150983cd24fb0d6963f7d28e17f72" },
        { "message digest",                                       "f96b697d7cb7938d525a2f31aaf161d0" },
        { "abcdefghijklmnopqrstuvwxyz",                           "c3fcd3d76192e4007dfb496cca67e13b" },
        { "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
                                                                  "d174ab98d277d9f5a5611c2c9f419d9f" },
        { "12345678901234567890123456789012345678901234567890123456789012345678901234567890",
                                                                  "57edf4a22be3c955ac49da2e2107b67a" }
    };
    int i, bad = 0;
    char hex[33];

    for (i = 0; i < (int)(sizeof(v) / sizeof(v[0])); i++) {
        md5_ctx ctx;
        unsigned char d[16];
        md5_init(&ctx);
        md5_update(&ctx, v[i].text, (unsigned int)strlen(v[i].text));
        md5_final(&ctx, d);
        md5_hex(d, hex);
        printf("%-6s %s  %s\n", (strcmp(hex, v[i].want) == 0) ? "ok" : "FAIL", hex,
               strlen(v[i].text) ? v[i].text : "(empty)");
        if (strcmp(hex, v[i].want) != 0) bad++;
    }

    {   // one million 'a', hashed in chunks, to exercise the block/multi-update path
        md5_ctx ctx;
        unsigned char d[16], chunk[1000];
        const char* want = "7707d6ae4e027c70eea2a935c2296f21";
        int n;
        memset(chunk, 'a', sizeof(chunk));
        md5_init(&ctx);
        for (n = 0; n < 1000; n++) md5_update(&ctx, chunk, (unsigned int)sizeof(chunk));
        md5_final(&ctx, d);
        md5_hex(d, hex);
        printf("%-6s %s  1000000 x 'a' (chunked)\n", (strcmp(hex, want) == 0) ? "ok" : "FAIL", hex);
        if (strcmp(hex, want) != 0) bad++;
    }

    printf(bad ? "=== %d VECTOR FAILURES\n" : "=== vectors OK\n", bad);
    return bad ? 1 : 0;
}

static int HashFile(const char* path)
{
    FILE* f = fopen(path, "rb");
    md5_ctx ctx;
    unsigned char d[16], buf[65536];
    char hex[33];
    size_t got;

    if (!f) { printf("MISS   %s\n", path); return 1; }
    md5_init(&ctx);
    while ((got = fread(buf, 1, sizeof(buf), f)) > 0)
        md5_update(&ctx, buf, (unsigned int)got);
    fclose(f);
    md5_final(&ctx, d);
    md5_hex(d, hex);
    printf("%s  %s\n", hex, path);
    return 0;
}

int main(int argc, char** argv)
{
    int i, bad = 0;
    if (argc < 2) return RunVectors();
    for (i = 1; i < argc; i++) bad |= HashFile(argv[i]);
    return bad;
}
