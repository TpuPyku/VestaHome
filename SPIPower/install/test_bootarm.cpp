// test_bootarm.cpp - the boot job's decisions (bootarm.h), on the PC: arming over a flag already set, the boot run
// without a job, our installer's mark found in a file read in pieces.
#include <stdio.h>
#include <string.h>
#include "bootarm.h"

static int s_failed, s_checks;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAIL: %s\n", what);
    }
}

static void ArmChecks()
{
    Check(ArmVerdict(-1, false) == ARM_NEW, "no flag: armed anew");
    Check(ArmVerdict(0x02, false) == ARM_OVER, "0x02 (Main.exe after 3 failed shells, or ours left): ours over it");
    Check(ArmVerdict(0x02, true) == ARM_REFUSE, "0x02 with the recovery test staged: refused");
    Check(ArmVerdict(0x03, false) == ARM_REFUSE, "another value: not ours to touch");
    Check(ArmVerdict(0x00, false) == ARM_REFUSE, "0x00: not ours either");
}

static void NoJobChecks()
{
    Check(NoJobAction(true, false, false) == NOJOB_STUB_OFF, "the stub in place: taken off");
    Check(NoJobAction(true, true, false) == NOJOB_STUB_OFF, "stub and proxy: the stub first");
    Check(NoJobAction(false, true, false) == NOJOB_ROLLBACK, "our proxy in place: rolled back");
    Check(NoJobAction(false, false, true) == NOJOB_HANDOVER, "a factory package on the card: the factory updater");
    Check(NoJobAction(false, false, false) == NOJOB_CLEAR, "nothing of ours, no package: the flag off, the shell "
          "(was: the factory updater, \"Setup not found\" at every boot)");
}

static void StubOnChecks()
{
    Check(StubOnVerdict(true, true, false) == STUBON_UPDATE, "the stub there: updated");
    Check(StubOnVerdict(false, false, false) == STUBON_MOVE, "the shell alone: renamed _orig, the stub in");
    Check(StubOnVerdict(false, true, true) == STUBON_DROP_COPY, "the shell and the same _orig: the copy goes, then as "
          "usual (the user 2026-11-26: the shell put back by hand, its _orig left)");
    Check(StubOnVerdict(false, true, false) == STUBON_REFUSE, "two different shells: not guessed");
}

static void MarkChecks()
{
    BootMarkScan s;
    BootMarkInit(&s, "VHINSTVER:");
    Check(!BootMarkFeed(&s, "abc VHINST", 10), "a part of the mark: not yet");
    Check(BootMarkFeed(&s, "VER:1.2.261126", 14), "the rest in the next piece: found");
    BootMarkInit(&s, "VHINSTVER:");
    Check(BootMarkFeed(&s, "xxVHINSTVER:1", 13), "in one piece");
    BootMarkInit(&s, "VHINSTVER:");
    Check(!BootMarkFeed(&s, "VHINSTVEX:VHINST", 16) && !BootMarkFeed(&s, "VE", 2) && BootMarkFeed(&s, "R:", 2),
          "a near miss, then the mark over three pieces");
    BootMarkInit(&s, "VHINSTVER:");
    Check(BootMarkFeed(&s, "VVHINSTVER:", 11), "a repeated first letter before it");
    BootMarkInit(&s, "VHINSTVER:");
    Check(BootMarkFeed(&s, "VHINSTVHINSTVER:", 16), "a part of the mark that turns out to start it again");
    BootMarkInit(&s, "VHINSTVER:");
    Check(!BootMarkFeed(&s, "aVHINSTV", 8) && BootMarkFeed(&s, "HINSTVER:", 9), "the same over two pieces");
    BootMarkInit(&s, "VHINSTVER:");
    const char* bits[] = { "zzzzV", "HI", "N", "ST", "V", "E", "R", ":" };
    bool got = false, early = false;
    for (int i = 0; i < 8; i++) {
        bool f = BootMarkFeed(&s, bits[i], (int)strlen(bits[i]));
        early = early || (f && i < 7);
        got = got || f;
    }
    Check(got && !early, "a mark over many short pieces: found at its last letter, not before");
    BootMarkInit(&s, "VHINSTVER:");
    Check(!BootMarkFeed(&s, "VHINSTVER", 9) && !BootMarkFeed(&s, "x", 1) && !BootMarkFeed(&s, ":", 1),
          "a letter between: no mark");
    BootMarkInit(&s, "VHINSTVER:");
    Check(!BootMarkFeed(&s, "123456789", 9) && !BootMarkFeed(&s, "VHINSTVE", 8) && BootMarkFeed(&s, "R:", 2),
          "a piece of exactly len - 1, then the mark over the next two");
    BootMarkInit(&s, "VHINSTVER:");
    BootMarkFeed(&s, "VHINSTVER", 9);   // (its tail starts with the mark's first letter)
    BootMarkInit(&s, "VHINSTVER:");
    Check(!BootMarkFeed(&s, "HINSTVER:", 9), "started again: the old tail is gone");
    BootMarkInit(&s, "VHINSTVER:");
    char z[4] = { 'V', 0, 'H', 0 };
    Check(!BootMarkFeed(&s, z, 4) && !BootMarkFeed(&s, "INSTVER:", 8), "zeros break the mark");
}

int main()
{
    ArmChecks();
    NoJobChecks();
    StubOnChecks();
    MarkChecks();
    printf("bootarm: %d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
