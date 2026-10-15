// mcushm.h - the proxy's shared ring of MCU messages, read by VestaHome (keep both copies identical:
// VestaHome/VestaHome/native/home.cpp has the same layout).
//
// The proxy lives inside every process that loads SPIPowerMgr.dll (G3NavHMI, Main.exe, AppLauncher...).
// Each inbound message the MCU hands to a skeleton (rmtAdapter::interpretCommand -> vtable[1]) is copied
// here: a named file mapping "VestaMCU", 256 slots. Writer: n = InterlockedIncrement(&seq), slot (n-1)%256,
// the slot's seq is set to n last. Reader: walks its last seen n up to seq and takes a slot only when its
// seq equals the n it expects (a slot being rewritten is skipped).
#ifndef MCUSHM_H
#define MCUSHM_H

#define MCU_SHM_NAME   L"VestaMCU"
#define MCU_SHM_MAGIC  0x55434D56   // "VMCU"
#define MCU_SHM_SLOTS  256
#define MCU_SHM_DATA   28

struct McuMsg {
    volatile LONG seq;              // = the writer's n when the slot is complete
    DWORD         tick;             // GetTickCount() when it came
    unsigned short id;              // MCU message id (big-endian in the buffer)
    unsigned char len;              // payload length as the MCU sent it
    unsigned char skel;             // skeleton id the handler was registered with
    unsigned char data[MCU_SHM_DATA];   // payload, first MCU_SHM_DATA bytes
};

struct McuShm {
    DWORD         magic;
    DWORD         version;          // 1
    volatile LONG seq;              // messages written so far
    volatile LONG attach;           // proxy loads into processes since boot
    volatile LONG wraps;            // skeleton handlers wrapped
    DWORD         reserved[3];
    McuMsg        msg[MCU_SHM_SLOTS];
};

#endif
