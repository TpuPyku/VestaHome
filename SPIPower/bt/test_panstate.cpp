// test_panstate.cpp - panstate.h on the PC: when the stack's PAN state counts as stuck, and that the code it checks is
// the stock DLL's (argument: its path; without the file only the made-up image). Exit code: the failed checks.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "panstate.h"

static int s_failed, s_checks;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAILED: %s\n", what);
    }
}

static void Put(unsigned char* image, unsigned long rva, unsigned long v)
{
    memcpy(image + rva, &v, 4);
}

static void TestStates()
{
    Check(!PanWaiting(PAN_ST_DEINIT) && !PanWaiting(PAN_ST_CONNECTED) && !PanWaiting(PAN_ST_READY), "settled states");
    Check(PanWaiting(PAN_ST_CONNECTING) && PanWaiting(PAN_ST_DISCONNECTING), "waiting for the stack");
    Check(!PanDeadRequest(PAN_ST_CONNECTING, kPanStuckMs - 1), "a young request is left alone");
    Check(PanDeadRequest(PAN_ST_CONNECTING, kPanStuckMs), "an old connect is dead");
    Check(PanDeadRequest(PAN_ST_DISCONNECTING, kPanStuckMs + 1), "an old disconnect is dead");
    Check(!PanDeadRequest(PAN_ST_READY, kPanStuckMs * 10), "ready is never dead");
    Check(!PanDeadRequest(PAN_ST_CONNECTED, kPanStuckMs * 10), "connected is never dead");
}

static void TestMadeUp()
{
    enum { kSize = 0x10700, kBase = 0x20000000 };
    unsigned char* im = (unsigned char*)calloc(1, kSize);
    Check(!PanLayoutOk(im, kBase), "empty: not the known build");
    Put(im, 0x105A8, 0xE3510002);
    Put(im, 0x10610, 0xE3510004);
    Put(im, 0x1065C, 0xE585300C);
    Put(im, 0x1068C, kBase + kPanStateRva - 0xC);
    Check(PanLayoutOk(im, kBase), "the known code at its base");
    Check(!PanLayoutOk(im, kBase + 0x1000), "another base: refused");
    Put(im, 0x105A8, 0xE3510003);
    Check(!PanLayoutOk(im, kBase), "the first check differs");
    Put(im, 0x105A8, 0xE3510002);
    Put(im, 0x10610, 0);
    Check(!PanLayoutOk(im, kBase), "the second check differs");
    Put(im, 0x10610, 0xE3510004);
    Put(im, 0x1065C, 0);
    Check(!PanLayoutOk(im, kBase), "the store differs");
    free(im);
}

// The file as the loader lays it out (no relocation: its preferred base stays)
static unsigned char* LoadImage(const char* path, unsigned long* base, unsigned long* connectAt)
{
    FILE* f = fopen(path, "rb");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char* d = (unsigned char*)malloc(n);
    fread(d, 1, n, f);
    fclose(f);
    IMAGE_NT_HEADERS32* nt = (IMAGE_NT_HEADERS32*)(d + ((IMAGE_DOS_HEADER*)d)->e_lfanew);
    unsigned char* im = (unsigned char*)calloc(1, nt->OptionalHeader.SizeOfImage);
    IMAGE_SECTION_HEADER* s = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; i++)
        memcpy(im + s[i].VirtualAddress, d + s[i].PointerToRawData, s[i].SizeOfRawData);
    *base = nt->OptionalHeader.ImageBase;
    // the export the proxy starts from
    IMAGE_DATA_DIRECTORY dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    IMAGE_EXPORT_DIRECTORY* e = (IMAGE_EXPORT_DIRECTORY*)(im + dir.VirtualAddress);
    DWORD* names = (DWORD*)(im + e->AddressOfNames);
    WORD* ords = (WORD*)(im + e->AddressOfNameOrdinals);
    DWORD* funcs = (DWORD*)(im + e->AddressOfFunctions);
    *connectAt = 0;
    for (DWORD i = 0; i < e->NumberOfNames; i++)
        if (!strcmp((char*)im + names[i], "Flc_Bluetooth_PanServer_Connect"))
            *connectAt = funcs[ords[i]];
    free(d);
    return im;
}

static void TestRealDll(const char* path)
{
    unsigned long base = 0, connect = 0;
    unsigned char* im = LoadImage(path, &base, &connect);
    if (!im) {
        printf("the stock DLL not here: checked on the made-up image only\n");
        return;
    }
    Check(connect == kPanConnectRva, "the export the base is counted from");
    Check(PanLayoutOk(im, base), "the stock DLL is the known build");
    Check(!PanLayoutOk(im, base + 0x10000), "the stock DLL at a wrong base: refused");
    free(im);
}

static void TestAddr()
{
    unsigned char a[6];
    Check(PanAddr("A4:E2:01:02:03:fe", a) && a[0] == 0xFE && a[1] == 3 && a[2] == 2 && a[3] == 1 && a[4] == 0xE2 &&
          a[5] == 0xA4, "address: the last pair first (as the SDK's)");
    Check(!PanAddr("A4:E2:01:02:03", a), "address: five pairs - refused");
    Check(!PanAddr("A4:E2:01:02:03:04:05", a), "address: seven pairs - refused");
    Check(!PanAddr("A4:E2:01:02:03:0G", a), "address: not hex - refused");
    Check(!PanAddr("", a) && !PanAddr(0, a), "address: none - refused");
    Check(PanAddr("00:00:00:00:00:09", a) && a[0] == 9 && a[5] == 0, "address: zeros kept");
}

int main(int argc, char** argv)
{
    TestAddr();
    TestStates();
    TestMadeUp();
    if (argc > 1)
        TestRealDll(argv[1]);
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
