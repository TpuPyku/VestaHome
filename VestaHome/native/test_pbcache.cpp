// The phone book's cache beside VestaHome (pbcache.h) and the byte seal (secret.h) on the PC.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pbcache.h"
#include "secret.h"

static int s_checks, s_failed;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAIL: %s\n", what);
    }
}

static BtPbEntry Entry(const char* name, const char* num)
{
    BtPbEntry e;
    memset(&e, 0, sizeof(e));
    strcpy(e.name, name);
    strcpy(e.num[0], num);
    return e;
}

static void TestPack()
{
    BtPbEntry book[2] = { Entry("Mama", "+79001234567"), Entry("Work", "84951234567") };
    BtPbEntry calls[1] = { Entry("Mama", "+79001234567") };
    calls[0].callType = 2;
    int n = PbPack("A4:E2:87:98:D6:9F", 1, book, 2, calls, 1, NULL, 0);
    Check(n == 4 + 4 + 18 + 4 + 4 + 4 + 3 * (int)sizeof(BtPbEntry), "pack: its length asked");
    unsigned char* buf = (unsigned char*)malloc(n);
    Check(PbPack("A4:E2:87:98:D6:9F", 1, book, 2, calls, 1, buf, n - 1) == 0, "pack: too small - 0");
    Check(PbPack("A4:E2:87:98:D6:9F", 1, book, 2, calls, 1, buf, n) == n, "pack: written");
    char phone[18];
    int storage = -5, nb = -5, nc = -5;
    BtPbEntry *b = NULL, *c = NULL;
    Check(PbUnpack(buf, n, phone, &storage, &b, &nb, &c, &nc), "unpack: ours");
    Check(!strcmp(phone, "A4:E2:87:98:D6:9F") && storage == 1 && nb == 2 && nc == 1, "unpack: the phone, the counts");
    Check(b && c && !strcmp(b[1].name, "Work") && !strcmp(c[0].num[0], "+79001234567") && c[0].callType == 2,
          "unpack: the entries");
    free(b);
    free(c);
    Check(!PbUnpack(buf, n - 1, phone, &storage, &b, &nb, &c, &nc), "unpack: cut short - not ours");
    buf[0] = 'X';
    Check(!PbUnpack(buf, n, phone, &storage, &b, &nb, &c, &nc), "unpack: another magic - not ours");
    free(buf);
    int m = PbPack("A4:E2:87:98:D6:9F", 0, NULL, -1, NULL, 0, NULL, 0);
    unsigned char little[64];
    Check(m > 0 && m <= 64 && PbPack("A4:E2:87:98:D6:9F", 0, NULL, -1, NULL, 0, little, 64) == m, "pack: no book");
    Check(PbUnpack(little, m, phone, &storage, &b, &nb, &c, &nc) && nb == -1 && nc == 0 && !b && !c,
          "unpack: the book not pulled (-1), no calls");
    Check(m == 4 + 4 + 18 + 4 + 4 + 4, "pack: no entries - the head alone");
    unsigned char one[4 + 4 + 18 + 12 + 2 * sizeof(BtPbEntry)];
    int k = PbPack("A4:E2:87:98:D6:9F", 0, book, 1, calls, 1, one, sizeof(one));
    Check(k == (int)sizeof(one) && PbUnpack(one, k, phone, &storage, &b, &nb, &c, &nc) && nb == 1 && nc == 1 &&
          !strcmp(b[0].name, "Mama") && c[0].callType == 2, "pack: one in each list");
    free(b);
    free(c);
    int bad = -2;
    memcpy(little + 30, &bad, 4);
    Check(!PbUnpack(little, m, phone, &storage, &b, &nb, &c, &nc), "unpack: a count below -1 - not ours");
    BtPbEntry* many = (BtPbEntry*)calloc(BT_PB_MAX + 1, sizeof(BtPbEntry));
    int big = PbPack("A4:E2:87:98:D6:9F", 0, many, BT_PB_MAX, NULL, 0, NULL, 0);
    unsigned char* bb = (unsigned char*)malloc(big + sizeof(BtPbEntry));
    PbPack("A4:E2:87:98:D6:9F", 0, many, BT_PB_MAX, NULL, 0, bb, big);
    Check(PbUnpack(bb, big, phone, &storage, &b, &nb, &c, &nc) && nb == BT_PB_MAX, "unpack: the most entries (1000)");
    free(b);
    int over = PbPack("A4:E2:87:98:D6:9F", 0, many, BT_PB_MAX + 1, NULL, 0, bb, big + sizeof(BtPbEntry));
    Check(over == big + (int)sizeof(BtPbEntry) && !PbUnpack(bb, over, phone, &storage, &b, &nb, &c, &nc),
          "unpack: past the most - not ours");
    free(bb);
    free(many);
}

static void TestSeal()
{
    Check(SecretInit() != 0, "seal: a key");
    unsigned char plain[300], sealed[400], back[400];
    for (int i = 0; i < 300; i++)
        plain[i] = (unsigned char)(i * 7);
    int n = SecretSealBytes(plain, 300, sealed, sizeof(sealed));
    Check(n > 300 && n < 400, "seal: nonce and tag added");
    Check(memcmp(sealed + 12, plain, 300) != 0, "seal: not plain");
    Check(SecretSealBytes(plain, 300, sealed, n - 1) < 0, "seal: too small - -1");
    n = SecretSealBytes(plain, 300, sealed, sizeof(sealed));
    Check(SecretOpenBytes(sealed, n, back, sizeof(back)) == 300 && !memcmp(back, plain, 300), "open: the same back");
    Check(SecretOpenBytes(sealed, n, back, 299) < 0, "open: too small - -1");
    Check(SecretOpenBytes(sealed, n, back, 300) == 300, "open: just big enough");
    Check(SecretSealBytes(plain, 300, sealed, n) == n, "seal: just big enough");
    Check(SecretSealBytes(plain, -1, sealed, sizeof(sealed)) < 0, "seal: a negative length - -1");
    int e = SecretSealBytes(plain, 0, sealed, sizeof(sealed));
    Check(e > 0 && SecretOpenBytes(sealed, e, back, 0) == 0, "seal: nothing - nonce and tag, opened back to nothing");
    sealed[20] ^= 1;
    Check(SecretOpenBytes(sealed, n, back, sizeof(back)) < 0, "open: damaged (or another unit's) - -1");
    Check(SecretOpenBytes(sealed, 10, back, sizeof(back)) < 0, "open: shorter than nonce and tag - -1");
}

int main()
{
    TestPack();
    TestSeal();
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
