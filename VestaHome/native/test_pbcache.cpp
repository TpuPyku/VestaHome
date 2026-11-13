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

static void TestSort()   // (the names in UTF-8 escapes: this file has no BOM)
{
    BtPbEntry e[9] = { Entry("\xD0\xAF\xD0\xBD\xD0\xB0", "1"),                         // Яна
                       Entry("anna", "2"),
                       Entry("\xD0\x81\xD0\xBB\xD0\xBA\xD0\xB0", "3"),                 // Ёлка
                       Entry("", "4"),
                       Entry("\xD0\xB1\xD0\xBE\xD1\x80\xD0\xB8\xD1\x81", "5"),         // борис
                       Entry("8 800 taxi", "6"),
                       Entry("\xD0\x95\xD0\xB2\xD0\xB3\xD0\xB5\xD0\xBD\xD0\xB8\xD0\xB9", "7"),   // Евгений
                       Entry("Boris", "8"),
                       Entry("\xD0\x9E\xD0\xBB\xD1\x8C\xD0\xB3\xD0\xB0", "9") };       // Ольга
    PbSortBook(e, 9);
    const char* want = "573912864";   // борис Евгений Ёлка Ольга Яна anna Boris "8 800" (no name)
    bool ok = true;
    for (int i = 0; i < 9; i++)
        ok = ok && e[i].num[0][0] == want[i];
    Check(ok, "sort: Cyrillic by the alphabet (ё as е, any case), then Latin, then the rest, no name last");
    BtPbEntry f[6] = { Entry("#1", "1"),
                       Entry("\xD0\xAE", "2"),                          // Ю (one letter)
                       Entry("X\xD1\x8F", "3"),                         // Xя
                       Entry("\xD0\xAF-\xD1\x8F", "4"),                 // Я-я
                       Entry("J.Lo", "5"),
                       Entry("", "6") };
    PbSortBook(f, 6);
    want = "245316";   // by the first letter only: Ю Я-я J.Lo Xя #1 (no name)
    ok = true;
    for (int i = 0; i < 6; i++)
        ok = ok && f[i].num[0][0] == want[i];
    Check(ok, "sort: the group by the first letter alone");
    BtPbEntry g[2] = { Entry("b", "1"), Entry("a", "2") };
    PbSortBook(g, 2);
    Check(g[0].num[0][0] == '2', "sort: two entries");
    BtPbEntry h[4] = { Entry("zed", "1"), Entry("\xD0\xB0\xD0\xBD\xD1\x82", "2"), Entry("#", "3"),
                       Entry("\xD0\x90\xD2\x9B", "4") };   // zed, ант, Ақ (a Kazakh second letter)
    PbSortBook(h, 4);
    Check(h[0].num[0][0] == '2' && h[1].num[0][0] == '4' && h[2].num[0][0] == '1' && h[3].num[0][0] == '3',
          "sort: the alphabets' ends (а, z), a group by the first letter only");
    PbSortBook(e, 0);
    PbSortBook(NULL, 0);
}

static void TestFind()   // the caller's name for an incoming number (the user 2026-11-13: only the number shown)
{
    BtPbEntry book[4] = { Entry("Mama", "+7 (900) 123-45-67"), Entry("Work", "84951234567"), Entry("Short", "112"),
                          Entry("Ten", "9031112233") };   // (kept with 10 digits)
    Check(PbFindNumber(book, 4, "+7 903 111 22 33") == 3, "find: an 11-digit call finds a 10-digit entry");
    strcpy(book[1].num[1], "8-916-000-11-22");   // a second number of the same contact
    Check(PbFindNumber(book, 3, "89001234567") == 0, "find: 8... finds +7 (...) - the last 10 digits");
    Check(PbFindNumber(book, 3, "+79161234567") == -1, "find: another number - none");
    Check(PbFindNumber(book, 3, "9001234567") == 0, "find: 10 digits (no +7, no 8) find the 11-digit one");
    Check(PbFindNumber(book, 3, "+79160001122") == 1, "find: a contact's second number");
    Check(PbFindNumber(book, 3, "112") == 2, "find: a short number - whole");
    Check(PbFindNumber(book, 3, "4567") == -1, "find: a short number does not match a long one's tail");
    Check(PbFindNumber(book, 3, "") == -1 && PbFindNumber(book, 0, "112") == -1 && PbFindNumber(NULL, 0, "1") == -1,
          "find: nothing to find - none");
}

int main()
{
    TestFind();
    TestSort();
    TestPack();
    TestSeal();
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
