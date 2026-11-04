// btowner.h - Bluetooth for one device at a time (the user 2026-11-03: the stack keeps either the phone or the OBD
// adapter - with both it drops the phone, and each had to be paired again): who has it now, and the step to take,
// asked once a second. The owner is remembered (ini bt.owner): the OBD adapter last - it is connected again after
// a restart and the phone is let go.
#pragma once
#include <windows.h>

enum { BTO_PHONE, BTO_OBD };
// The OBD adapter at all (the user 2026-11-04, sheet 22: the phone's pairings broke again, the adapter never
// connected - "disable OBD, we do it after the release"): 0 - Bluetooth always the phone's, a stored "obd" owner
// handed back to it, no OBD rows anywhere.
enum { kObdOn = 0 };
// BTA_PHONE_BACK: the adapter not connected after its tries (or none chosen) - Bluetooth back to the phone, so the
// phone is not let go for ever (HU 2026-11-04)
enum { BTA_NONE, BTA_PHONE_OFF, BTA_OBD_CONNECT, BTA_OBD_STOP, BTA_PHONE_BACK };
enum { kBtoPhoneOffMs = 10000,   // the phone back while the adapter has Bluetooth: let go again after that long
       kBtoObdWaitMs = 5000,     // the phone let go: the adapter connected after that long
       kBtoObdRetryMs = 30000,   // a connect failed: the next one after that long (since the last one began)
       kBtoObdTries = 3 };       // connects in a row without success, then none until asked again

struct BtOwnerIn {
    int   owner;     // BTO_*
    bool  phone;     // the phone connected (handsfree, music or internet)
    bool  obdBusy;   // the adapter connecting or connected
    bool  obdAddr;   // an adapter chosen
    bool  hold;      // a search or a pairing on its way: no connect meanwhile
    int   lastAct;   // the last step taken for this owner (BTA_*), BTA_NONE none yet
    DWORD sinceAct;  // ms since it
    int   tries;     // the adapter's connects since it last connected, or since the owner was set
};

int BtOwnerStep(const BtOwnerIn& in);   // BTA_*
