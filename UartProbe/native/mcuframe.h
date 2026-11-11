// mcuframe.h - the radio MCU's own UART protocol (the firmware RdDnl.upg, Ghidra 2026-11-11; PROJECT.md): a frame is
// [L][payload][X], L = the payload's length + 1, X = L xor every payload byte; the payload is FF idHi idLo FF data.
// The MCU answers 06 FF idHi idLo FF status X (00 done, 01 busy, B1 bad value, A2/A3 unknown id, FF locked).
#pragma once

// The frame of the command id with n data bytes into out (n + 6 bytes); returns its length.
int McuFrame(int id, const unsigned char* data, int n, unsigned char* out);

// The first whole reply in the n bytes received: its offset (id and status set), -1 none.
int McuReply(const unsigned char* b, int n, int* id, int* status);
