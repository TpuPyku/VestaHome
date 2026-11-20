// gate.h: which unit the installer lets VestaHome's loader on.
#include "gate.h"
#include <stdio.h>

static int s_checks, s_failed;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAIL: %s\n", what);
    }
}

static GateFacts Good()   // VHCheck 1.1 on 4125 (2026-11-20): all there
{
    GateFacts f = { 0, 0, 0, 0, 4125, false };
    return f;
}

int main()
{
    GateFacts f = Good();
    Check(GateProblems(f) == 0 && GateAllows(GateProblems(f)), "4125 as VHCheck saw it - no problem, allowed");
    f.shellSw = 4120;
    Check(GateProblems(f) == 0, "4120 - the same");

    f = Good(); f.spm = -1;
    Check(GateProblems(f) == GATE_NO_SPM && !GateAllows(GateProblems(f)), "no SPIPowerMgr - refused");
    f = Good(); f.spm = 3;
    Check(GateProblems(f) == GATE_SPM_MISS && !GateAllows(GateProblems(f)), "SPIPowerMgr without 3 functions - refused");
    f = Good(); f.flc = -1;
    Check(GateProblems(f) == GATE_NO_FLC && GateAllows(GateProblems(f)), "no Bluetooth SDK - told, allowed");
    f = Good(); f.flc = 1;
    Check(GateProblems(f) == GATE_FLC_MISS && GateAllows(GateProblems(f)), "the SDK without one - told, allowed");
    f = Good(); f.btapi = -1;
    Check(GateProblems(f) == GATE_BTAPI && GateAllows(GateProblems(f)), "no btapi - told, allowed");
    f = Good(); f.btapi = 2;
    Check(GateProblems(f) == GATE_BTAPI, "btapi without two - told");
    f = Good(); f.sv = -1;
    Check(GateProblems(f) == GATE_SV && GateAllows(GateProblems(f)), "no desay_sdk_api - told, allowed");
    f = Good(); f.sv = 1;
    Check(GateProblems(f) == GATE_SV, "desay_sdk_api without one - told");
    f = Good(); f.shellSw = 0;
    Check(GateProblems(f) == GATE_SHELL && !GateAllows(GateProblems(f)), "no shell version - refused");
    f = Good(); f.canPanel = true;
    Check(GateProblems(f) == GATE_CANPANEL && !GateAllows(GateProblems(f)), "the CAN panel - refused (for now)");
    f.spm = -1; f.flc = 2; f.sv = -1;
    Check(GateProblems(f) == (GATE_CANPANEL | GATE_NO_SPM | GATE_FLC_MISS | GATE_SV), "several at once");
    Check(GateAllows(GATE_NO_FLC | GATE_BTAPI | GATE_SV | GATE_FLC_MISS), "only the told ones - allowed");
    static const unsigned kAll[] = { GATE_NO_SPM, GATE_SPM_MISS, GATE_NO_FLC, GATE_FLC_MISS, GATE_BTAPI, GATE_SV,
                                     GATE_SHELL, GATE_CANPANEL };
    unsigned seen = 0;
    bool single = true;
    for (int i = 0; i < 8; i++) {
        single = single && kAll[i] && !(kAll[i] & (kAll[i] - 1)) && !(seen & kAll[i]);
        seen |= kAll[i];
    }
    Check(single && seen == 0xFF, "each problem its own bit (the installer words them one by one)");
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
