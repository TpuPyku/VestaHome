// mcucmd.h - VestaHome's MCU calls carried out in the stock shell (keep both copies identical:
// VestaHome/VestaHome/native/home.cpp has the same layout).
//
// The audio calls (SendRadioAudioHMLData, ...BalanceFaderData, ...) reach the MCU only from the shell's process:
// sent by VestaHome's own copy of SPIPowerMgr.dll they were never answered (SPIPower.log 2026-10-17..20), the
// keys were. So the proxy, when it is loaded by G3NavHMI.exe, runs a thread that takes VestaHome's calls from a
// named mapping "VestaMCUCmd" (a queue of 16) and makes them there. Writer (VestaHome): call n = seq + 1 into
// slot n % 16, then seq = n, then SetEvent(MCU_CMD_EVENT); the thread makes every call from done + 1 up to seq.
#ifndef MCUCMD_H
#define MCUCMD_H

#define MCU_CMD_NAME   L"VestaMCUCmd"
#define MCU_CMD_EVENT  L"VestaMCUCmd_Ev"
#define MCU_CMD_MAGIC  0x44434D56   // "VMCD"
#define MCU_CMD_SLOTS  16

struct McuCall {
    char         name[48];          // "SPApi_SendRadioAudioHMLData"
    unsigned int arg[6];            // bytes, as the calls take them
};

struct McuCmd {
    DWORD         magic;
    volatile LONG ready;            // the shell's thread runs (its process id), 0 = none
    volatile LONG seq;              // calls queued so far
    volatile LONG done;             // calls made so far
    McuCall       call[MCU_CMD_SLOTS];
};

#endif
