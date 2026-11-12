// The phone book and the call log kept beside VestaHome (the user 2026-11-12: "сохраняй зашифрованно рядом с VH"):
// the last phone's lists, shown at once when it connects again (a pull still refreshes them). The file is the plain
// form below sealed with this head unit's key (secret.h SecretSealBytes): unreadable on another unit or a PC.
// Plain form: "VHPB", version 1, the phone's address (18), the book's storage (0 phone, 1 SIM), the counts of the book
// and the calls, then the entries as they are (BtPbEntry: chars only - the same bytes on the unit and the PC).
#ifndef VESTA_PBCACHE_H
#define VESTA_PBCACHE_H

#include "../../SPIPower/bt/btshm.h"

// Its plain form's length (0: out too small); out NULL - only the length needed.
int PbPack(const char* phone, int storage, const BtPbEntry* book, int nb, const BtPbEntry* calls, int nc,
           unsigned char* out, int cap);

// The plain form read back: the lists malloc'ed (NULL when empty), counts -1 when that list was not there; false -
// not a cache of ours (the lists left alone).
bool PbUnpack(const unsigned char* in, int len, char phone[18], int* storage, BtPbEntry** book, int* nb,
              BtPbEntry** calls, int* nc);

#endif
