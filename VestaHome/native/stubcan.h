// stubcan.h - does the loader on the unit (VestaStub as \ResidentFlash\navigation\app\G3NavHMI.exe) know the update
// from "О системе" (its job \SDMMC\VestaUpdate.job, 2026-11-25)? The testers 2026-11-28: "Обновить" restarted, nothing
// installed - their loader was older (vhcan7: no "=== VestaStub" line, the job run one boot later, after a hand-run
// installer had put the new loader). The silent update replaces VestaHome only, so an old loader stays old: then the
// installer is opened as by hand (it puts the new loader). The new loader's file holds the job's name (UTF-16).
#ifndef STUBCAN_H
#define STUBCAN_H

#include <string.h>

inline bool StubKnowsUpdate(const unsigned char* buf, int n)
{
    static const char kName[] = "VestaUpdate.job";
    enum { kLen = sizeof(kName) - 1 };
    for (int at = 0; buf && at + 2 * kLen <= n; at++) {
        int i = 0;
        while (i < kLen && buf[at + 2 * i] == (unsigned char)kName[i] && buf[at + 2 * i + 1] == 0)
            i++;
        if (i == kLen)
            return true;
    }
    return false;
}

#endif
