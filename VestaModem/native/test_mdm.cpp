// test_mdm.cpp - VestaModem's parts with no USB in them, on the PC: the APN of a SIM, the status from the modem's
// lines, the lines out of the byte stream, the queue between the USB reader and the COM port.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "mdm.h"

static int s_fails;

static void Check(bool ok, const char* what)
{
    printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok)
        s_fails++;
}

static char s_lines[4][160];
static int  s_lineCount;

static void OnLine(void*, const char* line)
{
    if (s_lineCount < 4)
        strcpy(s_lines[s_lineCount], line);
    s_lineCount++;
}

static void Feed(LineAcc* a, const char* s)
{
    LineFeed(a, (const BYTE*)s, (int)strlen(s), OnLine, NULL);
}

int main()
{
    Check(!strcmp(MdmApn("250011234567890"), "internet.mts.ru"), "APN: MTS");
    Check(!strcmp(MdmApn("250021234567890"), "internet"), "APN: MegaFon");
    Check(!strcmp(MdmApn("250991234567890"), "internet.beeline.ru"), "APN: Beeline");
    Check(!strcmp(MdmApn("250201234567890"), "internet.tele2.ru"), "APN: Tele2");
    Check(!strcmp(MdmApn("250111234567890"), "internet.yota"), "APN: Yota");
    Check(!strcmp(MdmApn("999991234567890"), "internet"), "APN: not known - internet");
    Check(!strcmp(MdmApn(""), "internet"), "APN: no IMSI - internet");

    // the operators' presets (VestaHome: Settings -> Network; the user 2026-10-30: Russia's and the CIS's - Tele2 of
    // Kazakhstan and T2 of Russia two operators)
    int n = 0;
    const MdmOp* ops = MdmOps(&n);
    const MdmOp* t2 = MdmOpBySim("250201234567890");
    const MdmOp* kz = MdmOpBySim("401771234567890");
    Check(n >= 18 && t2 && kz && t2 != kz && !wcscmp(t2->name, L"T2 (Россия)") && !wcscmp(kz->name, L"Tele2 (Казахстан)"),
          "presets: T2 of Russia and Tele2 of Kazakhstan apart");
    Check(!strcmp(kz->apn, "internet") && !strcmp(t2->apn, "internet.tele2.ru"), "presets: their APNs");
    const MdmOp* mts = MdmOpBySim("250011234567890");
    Check(mts && !strcmp(mts->user, "mts") && !strcmp(mts->pass, "mts"), "presets: MTS's login and password");
    Check(!MdmOpBySim("999991234567890") && !MdmOpBySim(""), "presets: an unknown SIM - none");
    bool apns = true;   // the APN by the SIM: the preset's
    for (int i = 0; i < n; i++) {
        char imsi[16];
        _snprintf(imsi, sizeof(imsi), "%s1234567890", ops[i].sim);
        apns = apns && !strcmp(MdmApn(imsi), ops[i].apn) && MdmOpBySim(imsi) == &ops[i];
    }
    Check(apns, "presets: each one's SIM gives its APN");
    char cmd[96];
    Check(MdmApnCommand("internet.mts.ru", cmd, sizeof(cmd)) && !strcmp(cmd, "AT+CGDCONT=1,\"IP\",\"internet.mts.ru\"\r"),
          "APN: the modem's command");
    Check(!MdmApnCommand("bad\"apn", cmd, sizeof(cmd)) && !MdmApnCommand("", cmd, sizeof(cmd)),
          "APN: none, or with a quote - no command");

    MdmStat st;
    MdmStatInit(&st);
    Check(st.rssi == -1 && st.mode == -1 && st.sim == -1 && !st.imsi[0], "status: nothing heard");
    Check(MdmLine(&st, "^RSSI:28") && st.rssi == 28, "^RSSI");
    Check(!MdmLine(&st, "^RSSI:28"), "^RSSI the same: no change");
    Check(MdmLine(&st, "+CSQ: 17,99") && st.rssi == 17, "+CSQ");
    Check(MdmLine(&st, "^MODE:5,4") && st.mode == 5, "^MODE");
    Check(MdmLine(&st, "^SIMST:255") && st.sim == 255, "^SIMST");
    Check(MdmLine(&st, "^SYSINFO:2,3,0,3,1,,1") && st.mode == 3 && st.sim == 1, "^SYSINFO: mode and SIM");
    Check(!MdmLine(&st, "X50011234567890") && !st.imsi[0], "IMSI: not a digit first - not one");
    Check(MdmLine(&st, "250011234567890") && !strcmp(st.imsi, "250011234567890"), "IMSI: 15 digits");
    Check(!MdmLine(&st, "OK") && !MdmLine(&st, "12345") && !MdmLine(&st, "^BOOT:1,0,0,0,61"),
          "other lines: no change");

    LineAcc a;
    memset(&a, 0, sizeof(a));
    Feed(&a, "\r\n^RSSI:2");
    Check(s_lineCount == 0, "lines: none before the end");
    Feed(&a, "8\r\n\r\nOK\r\n");
    Check(s_lineCount == 2 && !strcmp(s_lines[0], "^RSSI:28") && !strcmp(s_lines[1], "OK"),
          "lines: split over two pieces, empty ones skipped");
    char longer[300];
    memset(longer, 'x', sizeof(longer) - 3);
    strcpy(longer + sizeof(longer) - 3, "\r\n");
    s_lineCount = 0;
    Feed(&a, longer);
    Check(s_lineCount == 1 && strlen(s_lines[0]) == 127 && a.len == 0, "lines: a too long one cut to the buffer");

    BYTE store[8], out[8];
    Fifo f = { store, sizeof(store), 0, 0 };
    Check(FifoPut(&f, (const BYTE*)"abcdef", 6) == 6 && FifoFree(&f) == 2, "queue: put");
    out[4] = '#';
    Check(FifoGet(&f, out, 4) == 4 && !memcmp(out, "abcd", 4) && out[4] == '#', "queue: get - not a byte more");
    Check(FifoPut(&f, (const BYTE*)"ghijkl", 6) == 6 && f.count == 8, "queue: put round the end");
    Check(FifoPut(&f, (const BYTE*)"z", 1) == 0, "queue: full - nothing taken");
    Check(FifoGet(&f, out, 8) == 8 && !memcmp(out, "efghijkl", 8) && f.count == 0, "queue: get round the end");
    Check(FifoGet(&f, out, 8) == 0, "queue: empty");

    printf(s_fails ? "%d FAILED\n" : "all ok\n", s_fails);
    return s_fails ? 1 : 0;
}
