// bootarm.h - the boot job's decisions (the installer, BOOT MODE in spipower_install.cpp), kept apart to be tested
// on the PC (test_bootarm.bat).
#pragma once
#include <string.h>

// Arming a boot job while \ResidentFlash\SwUpdate.flag is there already (user 2026-11-26, AppLauncher removed): 0x02 is
// what Main.exe writes after three failed shell starts - and what our own job leaves when its boot never came. The
// installer refused ("сначала ОТМЕНИТЬ ЗАДАНИЕ"), staged nothing, and the boot ran the factory updater: "Update Navi
// ... Setup not found!". Ours goes over a 0x02 (our SwDnl.exe takes the boot, clears the flag first); not over the
// recovery test staged on the card, nor over another value.
enum { ARM_NEW, ARM_OVER, ARM_REFUSE };

inline int ArmVerdict(int flag, bool drillStaged)
{
    if (flag < 0)
        return ARM_NEW;
    return flag == 0x02 && !drillStaged ? ARM_OVER : ARM_REFUSE;
}

// Our SwDnl.exe at the boot without a job (somebody else armed the flag): the stub in place - off; our proxy - rolled
// back; a factory package on the card (SwDnl.cfg, *.upg, update.pkg ...) - the factory updater; nothing of ours and no
// package - the flag off and the shell started (the factory updater would only say "Setup not found" every boot).
enum { NOJOB_STUB_OFF, NOJOB_ROLLBACK, NOJOB_HANDOVER, NOJOB_CLEAR };

inline int NoJobAction(bool stub, bool proxy, bool factoryPkg)
{
    if (stub)
        return NOJOB_STUB_OFF;
    if (proxy)
        return NOJOB_ROLLBACK;
    return factoryPkg ? NOJOB_HANDOVER : NOJOB_CLEAR;
}

// Putting the stub in place of G3NavHMI.exe (the boot run's "stub on"): the stub there - updated; the shell alone -
// it becomes G3NavHMI_orig.exe; the shell and an _orig the same - the copy goes first (the user 2026-11-26 put the
// shell back by hand after AppLauncher, its _orig stayed: the boot refused); two different shells - not guessed (the
// installer says so before arming).
enum { STUBON_UPDATE, STUBON_MOVE, STUBON_DROP_COPY, STUBON_REFUSE };

inline int StubOnVerdict(bool shellIsStub, bool origExists, bool same)
{
    if (shellIsStub)
        return STUBON_UPDATE;
    if (!origExists)
        return STUBON_MOVE;
    return same ? STUBON_DROP_COPY : STUBON_REFUSE;
}

// A mark looked for in a file read in pieces (an SwDnl.exe already on the card: our installer, an older one, is
// replaced - another file is not): the last len - 1 bytes are kept for a mark across two pieces.
struct BootMarkScan {
    const char* mark;
    int len, tailN;
    char tail[32];
};

inline void BootMarkInit(BootMarkScan* s, const char* mark)
{
    s->mark = mark;
    s->len = (int)strlen(mark);
    s->tailN = 0;
}

inline bool BootMarkIn(const char* p, int n, const char* mark, int len)
{
    for (int i = 0; i + len <= n; i++)
        if (!memcmp(p + i, mark, len))
            return true;
    return false;
}

// True once the mark has been seen (in this piece or across the last one and this).
inline bool BootMarkFeed(BootMarkScan* s, const char* p, int n)
{
    char edge[64];   // the tail and this piece's first len - 1 bytes
    int head = n < s->len - 1 ? n : s->len - 1;
    memcpy(edge, s->tail, s->tailN);
    memcpy(edge + s->tailN, p, head);
    if (BootMarkIn(edge, s->tailN + head, s->mark, s->len) || BootMarkIn(p, n, s->mark, s->len))
        return true;
    int keep = s->len - 1, m = s->tailN + head;   // the new tail: the last len - 1 bytes of tail + piece
    if (n >= keep) {
        memcpy(s->tail, p + n - keep, keep);
    } else {   // a short piece: all of it is in edge, after the old tail
        keep = m < keep ? m : keep;
        memcpy(s->tail, edge + m - keep, keep);
    }
    s->tailN = keep;
    return false;
}

// What starts the session after the boot run (the user 2026-11-26): the CAN panel's firmware has \Windows\Init.exe
// (its Main.exe starts it, not the shell): Init.exe starts the shell, then the panel unless SwUpdate.flag is there -
// the shell alone left the panel off till the next boot after every job of ours. The canrec job wants exactly that:
// the panel off for one boot, its COM3 free for VestaHome to record.
enum { START_INIT, START_SHELL };

inline int SessionStartWhat(bool initExists, bool canrecJob)
{
    return initExists && !canrecJob ? START_INIT : START_SHELL;
}

// Init.exe would not start (the tester 2026-11-27: err 87): we do its work - the shell, then the panel as it would:
// its exe there and no SwUpdate.flag.
inline bool InitFallbackPanel(bool panelExists, bool updateFlag)
{
    return panelExists && !updateFlag;
}
