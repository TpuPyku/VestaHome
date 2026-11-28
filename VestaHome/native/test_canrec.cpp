// test_canrec.cpp - the CAN panel's COM3 recording (canrec.h), on the PC.
#include <stdio.h>
#include <string.h>
#include "canrec.h"

static int s_failed, s_checks;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAIL: %s\n", what);
    }
}

int main()
{
    wchar_t procs[3][40] = { L"nk.exe", L"CAN_PANEL.EXE", L"VestaHome.exe" };
    Check(CanRecPanelAt(procs, 3) == 1, "can_panel.exe found in any case");
    Check(CanRecPanelAt(procs, 1) == -1, "no panel: -1");
    wcscpy(procs[1], L"can_panel.exe.bak");
    Check(CanRecPanelAt(procs, 3) == -1, "another name is not the panel");

    unsigned char out[64];
    const unsigned char data[3] = { 0x00, 0xA2, 0xFF };
    int n = CanRecPack(out, 0x01020304, data, 3);
    Check(n == 9, "a chunk: 6 bytes of header and its bytes");
    Check(out[0] == 0x04 && out[1] == 0x03 && out[2] == 0x02 && out[3] == 0x01, "ms since the start, little-endian");
    Check(out[4] == 3 && out[5] == 0, "its length, little-endian");
    Check(out[6] == 0x00 && out[7] == 0xA2 && out[8] == 0xFF, "the bytes as read");
    Check(CanRecPack(out, 0, data, 0) == 0, "nothing read: nothing written");
    Check(CanRecPack(out, 0, data, 1) == 7 && out[6] == 0x00, "one byte read: written");
    static unsigned char big[300];
    big[0] = 0x55;
    unsigned char wide[320] = { 0 };
    Check(CanRecPack(wide, 0, big, 300) == 306 && wide[4] == 44 && wide[5] == 1 && wide[6] == 0x55,
          "300 bytes: the length's high byte");

    wchar_t name[40];
    SYSTEMTIME t = { 2026, 11, 4, 26, 9, 5, 7, 0 };
    CanRecName(name, 40, &t);
    Check(!wcscmp(name, L"com3_261126_090507.bin"), "the file's name by the time");
    wchar_t shortName[10];
    for (int i = 0; i < 10; i++)
        shortName[i] = L'x';
    CanRecName(shortName, 10, &t);
    Check(shortName[9] == 0 && !wcscmp(shortName, L"com3_2611"), "a short buffer: cut and ended");
    wchar_t first[1][40] = { L"can_panel.exe" };
    Check(CanRecPanelAt(first, 1) == 0, "the panel first in the list");
    // the user 2026-11-27: marks while recording (doors, wheel, reverse, drive) beside the recording
    wchar_t mp[MAX_PATH];
    CanRecMarkPath(mp, MAX_PATH, L"\\SDMemory\\VestaLogs\\com3_261127_214806.bin");
    Check(!wcscmp(mp, L"\\SDMemory\\VestaLogs\\com3_261127_214806_marks.txt"), "marks: beside it, _marks.txt");
    CanRecMarkPath(mp, MAX_PATH, L"com3");
    Check(!wcscmp(mp, L"com3_marks.txt"), "marks: a name without .bin");
    // the user 2026-11-27 (after vhcan6): a recording step by step through the panel's functions - each step's mark
    // its number and key; "Пропустить" marks it skipped; "Назад" goes back without a mark
    char line[48];
    CanRecMarkLine(line, 48, 123456, 0, false);
    Check(!strcmp(line, "123456 01 door_fl\r\n"), "mark: ms, the step's number from 1, its key");
    CanRecMarkLine(line, 48, 7, 4, true);
    Check(!strcmp(line, "7 05 trunk skip\r\n"), "mark: a skipped step");
    CanRecMarkLine(line, 48, 9, CRS_COUNT - 1, false);
    Check(!strcmp(line, "9 21 drive\r\n"), "mark: the last step");
    bool unique = true;
    for (int i = 0; i < CRS_COUNT; i++)
        for (int j = i + 1; j < CRS_COUNT; j++)
            unique = unique && strcmp(CanRecSteps()[i].key, CanRecSteps()[j].key) != 0;
    Check(CRS_COUNT == 21 && unique, "21 steps, each its own key");
    wchar_t title[160];
    CanRecStepTitle(title, 160, 0);
    Check(!wcscmp(title, L"Шаг 1 из 21: откройте и закройте переднюю левую дверь"), "the first step's title");
    CanRecStepTitle(title, 160, CRS_COUNT);
    Check(!wcscmp(title, L"Все шаги пройдены — нажмите «стоп»"), "after the last: stop");
    CanRecStepTitle(title, 20, 0);
    Check(wcslen(title) == 19, "the title cut to its buffer");
    Check(CanRecStepAfter(0, CRB_DONE) == 1 && CanRecStepAfter(3, CRB_SKIP) == 4, "done / skip: the next step");
    Check(CanRecStepAfter(CRS_COUNT - 1, CRB_DONE) == CRS_COUNT, "the last done: all done");
    Check(CanRecStepAfter(CRS_COUNT, CRB_DONE) == CRS_COUNT, "all done: stays");
    Check(CanRecStepAfter(3, CRB_BACK) == 2 && CanRecStepAfter(0, CRB_BACK) == 0, "back: the step before, not below 1");
    Check(CanRecStepMarks(CRB_DONE) && CanRecStepMarks(CRB_SKIP) && !CanRecStepMarks(CRB_BACK), "back writes no mark");

    // the user 2026-11-27: the decoded values live on the screen while recording
    VCan3 c;
    memset(&c, 0, sizeof(c));
    wchar_t r[CRR_COUNT][96];
    CanRecRows(r, &c);
    Check(!wcscmp(r[0], L"Скорость — · обороты — · зажигание —"), "rows: nothing seen yet - dashes");
    Check(!wcscmp(r[1], L"ОЖ — · улица —"), "rows: the temperatures, nothing yet");
    Check(!wcscmp(r[2], L"Топливо — · запас —"), "rows: the fuel, nothing yet");
    Check(!wcscmp(r[3], L"Климат — · вентилятор —"), "rows: the climate, nothing yet");
    Check(!wcscmp(r[4], L"БК —"), "rows: the trip computer, nothing yet");
    Check(!wcscmp(r[5], L"GPS нет · кадров 0"), "rows: GPS, nothing yet");
    wchar_t w[24];
    CanWheelText(w, 24, &c);
    Check(!wcscmp(w, L"руль —"), "wheel: nothing yet");
    c.hasCar = true;
    c.carKmh100 = 1234;    // (0xd7: km/h x 100)
    c.hasRpm = true;
    c.rpm = 3000;
    c.hasBody = true;
    c.ign = 5;             // (0x50 data[0] & 7: 5 - the ignition on, 4 - off; can_panel 1.3.6.1)
    c.hasCool = true;
    c.coolC = 88;
    c.hasAir = true;
    c.airC = -3;
    c.hasFuel = true;
    c.fuel2 = 95;          // 47.5 l
    c.rangeKm = 656;
    c.hasClima = true;
    c.climaT = 45;         // 22.5
    c.fan = 0;
    c.hasTrip = true;
    c.use10 = 91;
    c.trip10 = 1902;
    c.used10 = 173;
    c.avg10 = 125;        // (12.5: x % 11 would not be x % 10)
    c.hasWheel = true;
    c.wheel = 32767 - 464; // (0xc6: (raw - 32767) / 15 degrees)
    c.hasPos = true;
    c.hasMove = true;
    c.kn100 = 1000;        // 10 knots = 18.5 km/h
    c.course100 = 35999;
    c.frames = 19292;
    CanRecRows(r, &c);
    Check(!wcscmp(r[0], L"Скорость 12,34 км/ч · обороты 3000 · зажигание вкл"), "rows: the speed, the revs, the ignition");
    Check(!wcscmp(r[1], L"ОЖ 88° · улица -3°"), "rows: the temperatures");
    Check(!wcscmp(r[2], L"Топливо 47,5 л · запас 656 км"), "rows: the fuel in 0.5 l, the range");
    Check(!wcscmp(r[3], L"Климат 22,5° · вентилятор авто"), "rows: the climate x 0.5, the fan 0 - auto");
    Check(!wcscmp(r[4], L"БК 9,1 л/100 · 190,2 км · 17,3 л · 12,5 км/ч"), "rows: the trip computer, each x 0.1");
    Check(!wcscmp(r[5], L"GPS 18,5 км/ч, курс 359° · кадров 19292"), "rows: GPS");
    CanWheelText(w, 24, &c);
    Check(!wcscmp(w, L"руль -30,9°"), "wheel: degrees, the comma");
    c.hasMove = false;
    c.ign = 4;
    c.fuel2 = 94;
    c.climaT = 8;
    c.fan = 15;
    c.wheel = 0;
    CanRecRows(r, &c);
    Check(!wcscmp(r[5], L"GPS есть · кадров 19292"), "rows: a position, no speed yet");
    Check(wcsstr(r[0], L"зажигание выкл") != NULL, "rows: the ignition off");
    Check(!wcscmp(r[2], L"Топливо 47 л · запас 656 км"), "rows: whole litres - no ,0");
    Check(!wcscmp(r[3], L"Климат LOW · вентилятор выкл"), "rows: the climate 8 - LOW, the fan 15 - off");
    CanWheelText(w, 24, &c);
    Check(!wcscmp(w, L"руль 0,0°"), "wheel: raw 0 - 0°");
    c.ign = 3;
    c.climaT = 10;
    c.fan = 4;
    CanRecRows(r, &c);
    Check(wcsstr(r[0], L"зажигание 3") != NULL, "rows: another ignition value - the number");
    Check(!wcscmp(r[3], L"Климат HI · вентилятор 4"), "rows: the climate 10 - HI, the fan's step");
    // the picture's parts: not come yet, off / closed, on / open
    Check(CanPart(false, true) == CNP_NONE && CanPart(true, false) == CNP_OFF && CanPart(true, true) == CNP_ON,
          "part: none, off, on");

    // the bridge (the user 2026-11-27: GPS and the wheel to COM4 / COM5 without the panel)
    Check(CanBridgeWanted(true, false, 60000, false), "bridge: the panel's firmware, the panel not running, 60 s up");
    Check(!CanBridgeWanted(false, false, 60000, false), "bridge: no panel firmware (COM3 is not its) - never");
    Check(!CanBridgeWanted(true, true, 600000, false), "bridge: the panel runs - its COM3");
    Check(!CanBridgeWanted(true, false, 59999, false), "bridge: the first minute - Init.exe starts the panel then");
    Check(!CanBridgeWanted(true, false, 60000, true), "bridge: COM3 read already (it, or the recording)");
    VCan3 d;
    memset(&d, 0, sizeof(d));
    CanOut o;
    memset(&o, 0, sizeof(o));
    Check(CanBridgeDue(&d, &o, 1000) == 0, "due: nothing new - nothing");
    d.posN = 1;
    d.wheelN = 3;
    Check(CanBridgeDue(&d, &o, 1000) == (CBO_POS | CBO_ANG), "due: a new position and a new wheel - both");
    Check(CanBridgeDue(&d, &o, 1100) == 0, "due: the same ones again - nothing");
    d.wheelN = 4;
    Check(CanBridgeDue(&d, &o, 1399) == 0, "due: the wheel again within 400 ms - not yet");
    Check(CanBridgeDue(&d, &o, 1400) == CBO_ANG, "due: 400 ms on - the wheel");
    d.posN = 2;
    Check(CanBridgeDue(&d, &o, 1401) == CBO_POS, "due: the next position at once (1/s from the module)");

    Check((CBO_POS & CBO_ANG) == 0, "due: two separate bits");
    CanRecMarkLine(line, 10, 123456, 0, false);
    Check(strlen(line) == 9, "mark: cut to its buffer, ended");
    c.hasCar = true;
    c.carKmh100 = 10000;
    c.hasPos = c.hasMove = true;
    c.kn100 = 10000;       // 100 knots = 185.2 km/h
    CanRecRows(r, &c);
    Check(wcsstr(r[0], L"Скорость 100,00 км/ч") == r[0] && wcsstr(r[5], L"GPS 185,2 км/ч") != NULL,
          "rows: 100 km/h, 100 knots");
    wchar_t w4[4];
    CanWheelText(w4, 4, &c);
    Check(w4[3] == 0, "wheel: a short buffer ended");
    Check(kCanRecMs == 30 * 60 * 1000, "the user 2026-11-27: up to 30 min, till stop");
    wchar_t res[64];
    CanRecResult(res, 64, true, 1024 * 1024 + 1023, CRF_PORT, 0);
    Check(!wcscmp(res, L"записано 1024 КБ"), "result: recorded, its KB");
    CanRecResult(res, 64, false, 0, CRF_FILE, 112);
    Check(!wcscmp(res, L"ошибка F112"), "result: the file not made - F and Windows' error");
    CanRecResult(res, 64, false, 0, CRF_PORT, 55);
    Check(!wcscmp(res, L"ошибка P55"), "result: COM3 not opened - P");
    CanRecResult(res, 64, false, 0, CRF_THREAD, 8);
    Check(!wcscmp(res, L"ошибка T8"), "result: no thread - T");
    wchar_t res4[4];
    CanRecResult(res4, 4, false, 0, CRF_FILE, 112);
    Check(res4[3] == 0, "result: a short buffer ended");
    printf("canrec: %d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
