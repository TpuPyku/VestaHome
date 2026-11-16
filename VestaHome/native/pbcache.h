// The phone book and the call log kept beside VestaHome (the user 2026-11-12: "сохраняй зашифрованно рядом с VH"):
// the last phone's lists, shown at once when it connects again (a pull still refreshes them). The file is the plain
// form below sealed with this head unit's key (VestaCore vsecret.h SecretSealBytes): unreadable on another unit or a PC.
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

// The book in the order of a phone in Russian (the user 2026-11-12: "не в алфавитном порядке" - PBAP gives it in the
// order the contacts were made): Cyrillic names by the alphabet (ё as е, any case), then Latin, then the rest, the
// entries without a name last.
void PbSortBook(BtPbEntry* book, int n);

// The book's entry with this number (any of its numbers), digits only compared - the last 10 of them (+7 / 8 and
// spaces, brackets, dashes do not matter; a number shorter than 10 digits must match whole): its index, -1 none.
int PbFindNumber(const BtPbEntry* book, int n, const char* number);

#endif
