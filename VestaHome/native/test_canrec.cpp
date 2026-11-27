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
    char line[40];
    CanRecMarkLine(line, 40, 123456, CRM_DOORS);
    Check(!strcmp(line, "123456 doors\r\n"), "mark: ms since the start, the kind");
    CanRecMarkLine(line, 40, 7, CRM_WHEEL);
    Check(!strcmp(line, "7 wheel\r\n"), "mark: wheel");
    CanRecMarkLine(line, 40, 8, CRM_REVERSE);
    Check(!strcmp(line, "8 reverse\r\n"), "mark: reverse");
    CanRecMarkLine(line, 40, 9, CRM_DRIVE);
    Check(!strcmp(line, "9 drive\r\n"), "mark: drive");
    Check(CRM_COUNT == 4, "four kinds");

    // the user 2026-11-27: the decoded values live on the screen while recording
    VCan3 c;
    memset(&c, 0, sizeof(c));
    wchar_t a[96], b[96];
    CanRecLive(a, 96, b, 96, &c);
    Check(!wcscmp(a, L"Скорость — · руль — · передача —"), "live: nothing seen yet - dashes");
    Check(!wcscmp(b, L"Двери — · статус — · GPS нет · кадров 0"), "live: second line, nothing yet");
    c.hasSpeed = true;
    c.speed10 = 1234;
    c.hasSteer = true;
    c.steer = -951;
    c.hasGear = true;
    c.gear = 5;
    c.hasDoors = true;
    c.doors = 0x81;
    c.hasStatus = true;
    c.status = 0x8005;
    c.hasPos = true;
    c.hasMove = true;
    c.kn100 = 1000;        // 10 knots = 18.5 km/h
    c.course100 = 35999;
    c.frames = 19292;
    CanRecLive(a, 96, b, 96, &c);
    Check(!wcscmp(a, L"Скорость 123,4 км/ч · руль -951 · передача 5"), "live: speed, wheel, gear");
    Check(!wcscmp(b, L"Двери 81 · статус 8005 · GPS 18,5 км/ч, курс 359° · кадров 19292"), "live: doors, GPS");
    c.hasMove = false;
    CanRecLive(a, 96, b, 96, &c);
    Check(!wcscmp(b, L"Двери 81 · статус 8005 · GPS есть · кадров 19292"), "live: a position, no speed yet");
    wchar_t s4[4];
    CanRecLive(s4, 4, b, 96, &c);
    Check(s4[3] == 0, "live: a short buffer ended");

    Check(kCanRecMs == 30 * 60 * 1000, "the user 2026-11-27: up to 30 min, till stop");
    printf("canrec: %d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
