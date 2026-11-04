// test_btowner.cpp - Bluetooth for one device at a time (btowner.cpp) on the PC (test_btowner.bat).
#include <windows.h>
#include <stdio.h>
#include "btowner.h"

static int s_failed, s_checks;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAIL: %s\n", what);
    }
}

// the adapter's owner, an adapter chosen, nothing done yet
static BtOwnerIn Obd()
{
    BtOwnerIn in;
    in.owner = BTO_OBD;
    in.phone = false;
    in.obdBusy = false;
    in.obdAddr = true;
    in.hold = false;
    in.lastAct = BTA_NONE;
    in.sinceAct = 0;
    in.tries = 0;
    return in;
}

static void PhoneOwner()
{
    BtOwnerIn in = Obd();
    in.owner = BTO_PHONE;
    Check(BtOwnerStep(in) == BTA_NONE, "phone: the adapter idle - nothing");
    in.obdBusy = true;
    Check(BtOwnerStep(in) == BTA_OBD_STOP, "phone: the adapter busy - stopped");
    in.phone = true;
    Check(BtOwnerStep(in) == BTA_OBD_STOP, "phone connected, the adapter busy - stopped");
}

static void ObdStart()
{
    BtOwnerIn in = Obd();
    Check(BtOwnerStep(in) == BTA_PHONE_OFF, "obd: the first step lets the phone go (the stack's connects off)");
    in.phone = true;
    Check(BtOwnerStep(in) == BTA_PHONE_OFF, "obd: the first step with the phone connected");
    in.hold = true;
    Check(BtOwnerStep(in) == BTA_PHONE_OFF, "obd: the phone let go during a search too");
}

static void ObdPhoneBack()
{
    BtOwnerIn in = Obd();
    in.phone = true;
    in.lastAct = BTA_PHONE_OFF;
    in.sinceAct = kBtoPhoneOffMs - 1;
    Check(BtOwnerStep(in) == BTA_NONE, "obd: the phone still there just after - waited");
    in.sinceAct = kBtoPhoneOffMs;
    Check(BtOwnerStep(in) == BTA_PHONE_OFF, "obd: the phone still there - let go again");
    in.lastAct = BTA_OBD_CONNECT;
    in.obdBusy = true;
    Check(BtOwnerStep(in) == BTA_PHONE_OFF, "obd: the phone came back while the adapter is on");
}

static void ObdConnect()
{
    BtOwnerIn in = Obd();
    in.lastAct = BTA_PHONE_OFF;
    in.sinceAct = kBtoObdWaitMs - 1;
    Check(BtOwnerStep(in) == BTA_NONE, "obd: the phone just let go - waited");
    in.sinceAct = kBtoObdWaitMs;
    Check(BtOwnerStep(in) == BTA_OBD_CONNECT, "obd: the phone gone - the adapter connected");
    in.hold = true;
    Check(BtOwnerStep(in) == BTA_NONE, "obd: a search on its way - no connect");
    in.hold = false;
    in.obdAddr = false;
    Check(BtOwnerStep(in) == BTA_PHONE_BACK, "obd: no adapter chosen - no connect, back to the phone");
    in.obdAddr = true;
    in.obdBusy = true;
    Check(BtOwnerStep(in) == BTA_NONE, "obd: the adapter busy - nothing");
}

static void ObdRetry()
{
    BtOwnerIn in = Obd();
    in.lastAct = BTA_OBD_CONNECT;
    in.tries = 1;
    in.sinceAct = kBtoObdRetryMs - 1;
    Check(BtOwnerStep(in) == BTA_NONE, "obd: a connect failed just now - waited");
    in.sinceAct = kBtoObdRetryMs;
    Check(BtOwnerStep(in) == BTA_OBD_CONNECT, "obd: a connect failed - again");
    in.tries = kBtoObdTries - 1;
    Check(BtOwnerStep(in) == BTA_OBD_CONNECT, "obd: the last try");
    in.tries = kBtoObdTries;
    Check(BtOwnerStep(in) == BTA_PHONE_BACK, "obd: tries used up - Bluetooth back to the phone");
    in.phone = true;
    Check(BtOwnerStep(in) == BTA_PHONE_BACK, "obd: tries used up, the phone there - back to it, not let go");
    in.obdBusy = true;
    Check(BtOwnerStep(in) == BTA_PHONE_OFF, "obd: tries used up but the adapter connecting - the phone let go");
    in.obdBusy = false;
    in.hold = true;
    Check(BtOwnerStep(in) == BTA_PHONE_OFF, "obd: tries used up, a search on its way - not given back");
}

// HU 2026-11-04: the adapter never connected and the phone was let go every 10 s until both were paired again
static void ObdNoAdapter()
{
    BtOwnerIn in = Obd();
    in.obdAddr = false;
    in.lastAct = BTA_PHONE_OFF;
    in.sinceAct = kBtoPhoneOffMs;
    Check(BtOwnerStep(in) == BTA_PHONE_BACK, "obd: no adapter chosen - back to the phone");
    in.hold = true;
    Check(BtOwnerStep(in) == BTA_NONE, "obd: no adapter yet, a search on its way - waited");
}

int main()
{
    PhoneOwner();
    ObdStart();
    ObdPhoneBack();
    ObdConnect();
    ObdRetry();
    ObdNoAdapter();
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
