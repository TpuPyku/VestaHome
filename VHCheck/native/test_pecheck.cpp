// test_pecheck.cpp - pecheck.cpp on the PC: a PE made here (its export names), broken ones, and - on the author's PC,
// when MAIN is beside the repos - the real 4120 SPIPowerMgr.dll (139 exports). Exit code: the failed checks.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "pecheck.h"

static int s_failed, s_checks;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAILED: %s\n", what);
    }
}

static void Put16(unsigned char* b, int o, unsigned v)
{
    b[o] = (unsigned char)v;
    b[o + 1] = (unsigned char)(v >> 8);
}

static void Put32(unsigned char* b, int o, unsigned v)
{
    Put16(b, o, v & 0xFFFF);
    Put16(b, o + 2, v >> 16);
}

// A PE32 of one section (.edata at RVA 0x1000, file 0x200) exporting names[]; its size.
static int MakePe(unsigned char* b, const char* const* names, int n)
{
    memset(b, 0, 4096);
    b[0] = 'M';
    b[1] = 'Z';
    Put32(b, 0x3C, 0x80);
    memcpy(b + 0x80, "PE\0\0", 4);
    Put16(b, 0x84, 0x1C0);            // ARM
    Put16(b, 0x86, 1);                // one section
    Put32(b, 0x88, 0x5D66113B);       // the build time
    Put16(b, 0x94, 0xE0);             // the optional header's size
    int opt = 0x98;
    Put16(b, opt, 0x10B);             // PE32
    Put32(b, opt + 92, 16);           // data directories
    Put32(b, opt + 96, 0x1000);       // export directory: RVA, size
    Put32(b, opt + 100, 0x200);
    int sec = opt + 0xE0;
    memcpy(b + sec, ".edata", 6);
    Put32(b, sec + 8, 0x400);         // virtual size
    Put32(b, sec + 12, 0x1000);       // RVA
    Put32(b, sec + 16, 0x400);        // raw size
    Put32(b, sec + 20, 0x200);        // raw pointer
    int dir = 0x200, ptrs = dir + 40, str = dir + 40 + 4 * n;
    Put32(b, dir + 24, n);
    Put32(b, dir + 32, 0x1000 + (ptrs - dir));
    for (int i = 0; i < n; i++) {
        Put32(b, ptrs + 4 * i, 0x1000 + (str - dir));
        strcpy((char*)b + str, names[i]);
        str += (int)strlen(names[i]) + 1;
    }
    return 0x600;
}

static void TestMade()
{
    static unsigned char b[4096];
    const char* names[3] = { "SPApi_LoadRadioEngine", "SPApi_SetSkelonton", "Flc_Bluetooth_Init" };
    int len = MakePe(b, names, 3);
    PeFacts f;
    Check(PeRead(b, len, &f) && f.pe && f.machine == 0x1C0 && f.stamp == 0x5D66113B && f.exports == 3,
          "made: a PE, ARM, its build time, 3 exports");
    Check(PeHasExport(b, len, "SPApi_SetSkelonton") && !PeHasExport(b, len, "SPApi_Set") &&
          !PeHasExport(b, len, "SPApi_SetSkelontonX"), "made: an export by its whole name only");
    const char* want[4] = { "Flc_Bluetooth_Init", "SV_AudioSetRoute", "SPApi_LoadRadioEngine", "Nope" };
    int miss[4];
    Check(PeMissing(b, len, want, 4, miss, 4) == 2 && miss[0] == 1 && miss[1] == 3, "made: the missing ones");
    Check(PeMissing(b, len, want, 4, miss, 1) == 2 && miss[0] == 1, "made: more missing than room - counted all");
    // broken ones
    Check(!PeRead(b, 2, &f) && !f.pe, "a file too short - not a PE");
    b[0] = 'X';
    Check(!PeRead(b, len, &f) && PeMissing(b, len, want, 4, miss, 4) == 4, "no MZ - not a PE, all missing");
    b[0] = 'M';
    Put32(b, 0x3C, 0x7FFFFFF0);
    Check(!PeRead(b, len, &f), "the PE header out of the file - not a PE");
    MakePe(b, names, 3);
    Put32(b, 0x98 + 96, 0x9000);        // the export directory outside every section
    Check(PeRead(b, len, &f) && f.exports == 0 && !PeHasExport(b, len, "SPApi_SetSkelonton"),
          "exports outside the sections - none (the rest read)");
    MakePe(b, names, 3);
    Put32(b, 0x200 + 24, 100000);       // a count past the file
    Check(PeRead(b, len, &f) && f.exports == 0, "an export count past the file - none");
    MakePe(b, names, 3);   // the name table just fitting the file (its count at the limit) / one entry over
    Put32(b, 0x200 + 32, 0x1000 + 0x400 - 12);
    Check(PeRead(b, 0x600, &f) && f.exports == 3, "a name table ending at the file's end - read");
    Check(PeRead(b, 0x600 - 1, &f) && f.exports == 0, "a name table one byte past the file - none");
    MakePe(b, names, 3);
    Check(PeRead(b, 0x80 + 4, &f) && f.pe && f.machine == 0 && f.exports == 0,
          "the PE signature at the file's end - a PE of nothing (no read past it)");
    Check(!PeRead(b, 0x80 + 3, &f), "the PE signature cut - not a PE");
    MakePe(b, names, 3);
    Put32(b, 0x98 + 92, 0);                 // no data directories
    Check(PeRead(b, len, &f) && f.exports == 0, "no data directories - no exports");
    MakePe(b, names, 3);
    Put32(b, 0x200 + 40, 0x1000 + 0x3FF);   // the first name's string at the section's very end, unended
    b[0x5FF] = 'A';
    Check(!PeHasExport(b, len, "A"), "a name not ended in the file - not taken");
}

static void TestReal()
{
    const char* path = "..\\..\\..\\MAIN\\ResidentFlash\\navigation\\app\\SPIPowerMgr.dll";
    FILE* f = fopen(path, "rb");
    if (!f) {
        printf("(no MAIN: the real DLL skipped)\n");
        return;
    }
    static unsigned char b[200000];
    int len = (int)fread(b, 1, sizeof(b), f);
    fclose(f);
    PeFacts pf;
    Check(PeRead(b, len, &pf) && pf.machine == 0x1C2 && pf.stamp == 0x5A0D34C7 && pf.exports == 139,
          "real: 4120's SPIPowerMgr.dll - ARM (Thumb, 0x1C2), 16.11.2017, 139 exports");
    Check(PeHasExport(b, len, "SPApi_SetSkelonton") && PeHasExport(b, len, "SPApi_SendResetEvent"),
          "real: its functions");
}

// The firmware by a part's build day: the stamps of MAIN's 4120 and 4125 and the CAN panel's (4123's OS).
static void TestFirmware()
{
    Check(FwKnown(FW_OS, 0x5AD0A21E) == 4120 && FwKnown(FW_OS, 0x5DEF339A) == 4125, "firmware: the OS of 4120, 4125");
    Check(FwKnown(FW_OS, 17857u * 86400) == 4123 && FwKnown(FW_OS, 17857u * 86400 + 86399) == 4123 &&
          !FwKnown(FW_OS, 17857u * 86400 - 1) && !FwKnown(FW_OS, 17858u * 86400), "firmware: 4123's OS by its day");
    Check(FwKnown(FW_SHELL, 0x5AE1448B) == 4120 && FwKnown(FW_SHELL, 0x5D6612D7) == 4125, "firmware: the shell");
    Check(FwKnown(FW_SPM, 0x5A0D34C7) == 4120 && FwKnown(FW_SPM, 0x5D66113B) == 4125, "firmware: SPIPowerMgr.dll");
    Check(!FwKnown(FW_SHELL, 0x5AD0A21E) && !FwKnown(FW_OS, 0x5A0D34C7) && !FwKnown(FW_SPM, 0) && !FwKnown(7, 0x5D66113B),
          "firmware: another part's day, none, a part not known - not known");
}

// The version numbers as the stock "Info" shows them, from the vendor's version strings: "Itelma_TR8005_14_02_41.25"
// in the shell (SW 27.0.0.4125), "_14_01_" from the MCU (TR8005-4125), "_14_03_" the OS's (27.0.0.4108).
static void TestVersion()
{
    const char* shell = "x\0yItelma_TR8005_14_02_41.25_190828_R\0z";
    int n = FwVersion((const unsigned char*)shell, 40, 2);
    Check(n == 4125, "version: the shell's SW 41.25 -> 4125");
    Check(FwVersion((const unsigned char*)"ITELMA_TR8005_14_03_41.08_170413_R", 34, 3) == 4108, "version: the OS's, upper case");
    Check(FwVersion((const unsigned char*)"Itelma_TR8005_14_01_41.", 23, 1) == 0, "version: cut before its number - none");
    Check(FwVersion((const unsigned char*)"Itelma_TR8005_14_01_41.2", 24, 1) == 0, "version: one digit - none");
    Check(FwVersion((const unsigned char*)"Itelma_TR8005_14_02_41.20", 25, 1) == 0, "version: another part's - none");
    Check(FwVersion((const unsigned char*)"aaItelma_TR8005_14_01_4x.20Itelma_TR8005_14_01_41.23", 52, 1) == 4123,
          "version: a broken one skipped, the next found");
    Check(FwVersion((const unsigned char*)"Ktelma_TR8005_14_01_41.23", 25, 1) == 0, "version: its first letter checked");
    Check(FwVersion((const unsigned char*)"Itelma_TR8005_14_01_90.09", 25, 1) == 9009, "version: the digits 0 and 9");
    Check(FwVersion((const unsigned char*)"Itelma_TR8005_14_01_x1.23", 25, 1) == 0 &&
          FwVersion((const unsigned char*)"Itelma_TR8005_14_01_4x.23", 25, 1) == 0 &&
          FwVersion((const unsigned char*)"Itelma_TR8005_14_01_41.x3", 25, 1) == 0 &&
          FwVersion((const unsigned char*)"Itelma_TR8005_14_01_41.2x", 25, 1) == 0, "version: each of its four digits checked");
}

int main()
{
    TestVersion();
    TestFirmware();
    TestMade();
    TestReal();
    printf("test_pecheck: %d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
