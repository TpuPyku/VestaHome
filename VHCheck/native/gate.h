// gate.h: may VestaHome's loader go on this unit? (the user 2026-11-20: VHCheck's checks in the installer, the setup
// allowed by them). Pure, header only: the facts are found by the installer (pecheck.h PeMissing, FwVersion), the
// words are its own. A problem in kGateRefuse stops the setup; the others are told and the setup goes on.
#ifndef GATE_H
#define GATE_H

enum {
    GATE_NO_SPM    = 1,     // SPIPowerMgr.dll missing or not a PE: no MCU - no keys, sound, radio
    GATE_SPM_MISS  = 2,     // ... without some function we call
    GATE_NO_FLC    = 4,     // FLC_BLUETOOTH_SDK.dll missing: no Bluetooth
    GATE_FLC_MISS  = 8,     // ... without some function we call
    GATE_BTAPI     = 16,    // btapi.dll missing, or without a function (pairing, PAN)
    GATE_SV        = 32,    // desay_sdk_api.dll missing, or without a function (the sound's route)
    GATE_SHELL     = 64,    // the stock shell's own version string not found: not this unit's firmware
    GATE_CANPANEL  = 128,   // the CAN panel's firmware (can_panel.exe): its own way, not checked yet
};
static const unsigned kGateRefuse = GATE_NO_SPM | GATE_SPM_MISS | GATE_SHELL | GATE_CANPANEL;

struct GateFacts {
    int  spm, flc, btapi, sv;   // functions missing in each module; -1 - the module is not there
    int  shellSw;               // the stock shell's SW number (FwVersion part 2, e.g. 4120); 0 - none
    bool canPanel;
};

static unsigned GateModule(int missing, unsigned gone, unsigned short_)   // one module's problem, 0 none
{
    return missing < 0 ? gone : missing > 0 ? short_ : 0;
}

static unsigned GateProblems(const GateFacts& f)
{
    return GateModule(f.spm, GATE_NO_SPM, GATE_SPM_MISS) | GateModule(f.flc, GATE_NO_FLC, GATE_FLC_MISS) |
           GateModule(f.btapi, GATE_BTAPI, GATE_BTAPI) | GateModule(f.sv, GATE_SV, GATE_SV) |
           (f.shellSw ? 0 : GATE_SHELL) | (f.canPanel ? GATE_CANPANEL : 0);
}

static bool GateAllows(unsigned problems)
{
    return !(problems & kGateRefuse);
}

#endif
