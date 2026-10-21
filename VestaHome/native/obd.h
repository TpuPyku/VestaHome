// OBD-II through a Bluetooth ELM327 adapter (a test, 2026-10-20). The unit's Bluetooth stack (iAnywhere, in the
// firmware) has a serial port over SPP: BTCELegacy.dll as \Drivers\BT\XCBTSER2 (COM12), loaded by the shell's
// Flc_Bluetooth_PowerOn. Asked to connect (its IOCTL 0x1B0324), the stack takes the device's address from
// HKLM\Drivers\BT\XCBTSER1 "BtAddress" (where the SDK's Flc_Bluetooth_Spp_SetAddr writes it). The adapter must be
// paired first (the proxy's BT_CMD_PAIR). The work is done in a thread of its own; the window learns of changes by
// a posted message.
#ifndef VESTA_OBD_H
#define VESTA_OBD_H

enum { OBD_IDLE, OBD_CONNECTING, OBD_ON, OBD_FAILED };
enum { kObdLines = 5 };

struct ObdInfo {
    int     state;                    // OBD_*
    wchar_t lines[kObdLines][96];     // the last steps, oldest first
    int     rpm, speed, coolant;      // -1 = no answer (yet)
    wchar_t volt[12];                 // ATRV: the adapter's supply voltage, "" none
};

typedef void (*ObdLogFn)(const wchar_t* fmt, ...);

void ObdInit(HWND notify, UINT msg, ObdLogFn log);   // msg is posted when ObdInfo changed
// The address also given the way the shell's SDK does it (Flc_Bluetooth_Spp_SetAddr, through the Bluetooth proxy):
// the stack first asks its device chooser for 5 s, only then reads XCBTSER1. Returns the SDK's result, -1 none.
typedef int (*ObdSppAddrFn)(const char* addr);
void ObdSetSppAddrFn(ObdSppAddrFn fn);
void ObdConnect(const char* addr);                   // "A4:E2:87:98:D6:9F": connect and poll until ObdStop
void ObdStop();
void ObdGet(ObdInfo* out);

#endif
