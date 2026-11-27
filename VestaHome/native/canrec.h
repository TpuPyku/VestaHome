// canrec.h - the CAN panel's COM3 recorded raw (branch canrec, the user 2026-11-26): its frames come only from COM3,
// which can_panel.exe holds while it runs, and its exe cannot be renamed then - so VestaHome ends the panel, reads
// COM3 for a few minutes into the SD card, and starts the panel again from its place (\ResidentFlash\CAN). Never
// SwUpdate.flag (Init.exe skips the panel while it exists, but Main.exe / SwDnl read it as "an update runs").
// The file: chunks as read, each [ms since the start: u32 LE][length: u16 LE][bytes], to line the frames up with
// can.log (the panel's NMEA on COM4, recorded after it is back).
#pragma once
#include <windows.h>
#include <stdio.h>
#include <wchar.h>
#include <string.h>
#include "vcan3.h"   // VestaCore: the frames decoded (the live values)

// the panel among the processes (their exe names), -1 none
inline int CanRecPanelAt(const wchar_t (*procs)[40], int n)
{
    for (int i = 0; i < n; i++)
        if (!_wcsicmp(procs[i], L"can_panel.exe"))
            return i;
    return -1;
}

// one chunk into out (6 + len bytes); 0 when nothing was read
inline int CanRecPack(unsigned char* out, DWORD ms, const unsigned char* data, int len)
{
    if (len <= 0)
        return 0;
    out[0] = (unsigned char)ms;
    out[1] = (unsigned char)(ms >> 8);
    out[2] = (unsigned char)(ms >> 16);
    out[3] = (unsigned char)(ms >> 24);
    out[4] = (unsigned char)len;
    out[5] = (unsigned char)(len >> 8);
    memcpy(out + 6, data, len);
    return 6 + len;
}

inline void CanRecName(wchar_t* out, int cap, const SYSTEMTIME* t)
{
    _snwprintf(out, cap, L"com3_%02d%02d%02d_%02d%02d%02d.bin", t->wYear % 100, t->wMonth, t->wDay, t->wHour,
               t->wMinute, t->wSecond);
    out[cap - 1] = 0;
}

// The user 2026-11-27: the recording till "stop" (a drive with the doors, the wheel, the reverse), 30 min at most
// (~3 MB at the panel's 1.7 KB/s).
enum { kCanRecMs = 30 * 60 * 1000 };

// Marks while recording (the user 2026-11-27, after vhcan6: the panel's functions listed - a recording step by step
// through them): the screen names one step at a time, "Готово" marks it, "Пропустить" marks it skipped, "Назад"
// goes back without a mark. <recording>_marks.txt beside it, one line each: "<ms since the start> <NN> <key>[ skip]".
struct CanRecStep { const char* key; const wchar_t* text; };
enum { CRS_COUNT = 21 };
enum { CRB_DONE, CRB_SKIP, CRB_BACK };

inline const CanRecStep* CanRecSteps()
{
    static const CanRecStep steps[CRS_COUNT] = {
        { "door_fl", L"откройте и закройте переднюю левую дверь" },
        { "door_fr", L"откройте и закройте переднюю правую дверь" },
        { "door_rear", L"откройте и закройте заднюю дверь (любую)" },
        { "hood", L"откройте и закройте капот" },
        { "trunk", L"откройте и закройте багажник" },
        { "ignition", L"включите зажигание, не заводя двигатель" },
        { "low_beam", L"включите ближний свет, через 3 с выключите" },
        { "high_beam", L"включите дальний свет, через 3 с выключите" },
        { "turn_left", L"включите левый поворотник, через 3 с выключите" },
        { "turn_right", L"включите правый поворотник, через 3 с выключите" },
        { "rear_fog", L"включите задний противотуманный, через 3 с выключите" },
        { "brake", L"нажмите тормоз на 3 с и отпустите" },
        { "handbrake", L"опустите ручник, через 3 с поднимите" },
        { "climate_temp", L"климат: температура +1, через 3 с обратно" },
        { "climate_fan", L"климат: вентилятор +1, через 3 с обратно" },
        { "wheel", L"руль до упора влево, затем вправо, затем прямо" },
        { "engine", L"заведите двигатель" },
        { "rpm", L"подержите обороты около 3000 три секунды" },
        { "reverse", L"на тормозе включите задний ход, через 3 с выключите" },
        { "trip_reset", L"сбросьте на щитке один показатель бортового компьютера" },
        { "drive", L"поезжайте — нажмите, когда тронулись" },
    };
    return steps;
}

inline void CanRecMarkLine(char* out, int cap, DWORD ms, int step, bool skipped)
{
    _snprintf(out, cap, "%lu %02d %s%s\r\n", ms, step + 1, CanRecSteps()[step].key, skipped ? " skip" : "");
    out[cap - 1] = 0;
}

inline void CanRecStepTitle(wchar_t* out, int cap, int step)
{
    if (step >= CRS_COUNT)
        _snwprintf(out, cap, L"Все шаги пройдены — нажмите «стоп»");
    else
        _snwprintf(out, cap, L"Шаг %d из %d: %s", step + 1, CRS_COUNT, CanRecSteps()[step].text);
    out[cap - 1] = 0;
}

inline int CanRecStepAfter(int step, int button)
{
    if (button == CRB_BACK)
        return step > 0 ? step - 1 : 0;
    return step < CRS_COUNT ? step + 1 : CRS_COUNT;
}

inline bool CanRecStepMarks(int button)
{
    return button != CRB_BACK;
}

inline void CanRecMarkPath(wchar_t* out, int cap, const wchar_t* bin)
{
    int n = (int)wcslen(bin);
    if (n >= 4 && !_wcsicmp(bin + n - 4, L".bin"))
        n -= 4;
    _snwprintf(out, cap, L"%.*s_marks.txt", n, bin);
    out[cap - 1] = 0;
}

// The decoded values as two lines for the screen; a dash: not come yet (can_panel 1.3.6.1's own reading, 2026-11-27):
// the car's speed 0xd7 (km/h x 100), the wheel 0xc6 ((raw - 32767) / 15 degrees, raw 0 - 0, as its @ANG), the
// ignition 0x50 data[0] & 7 (5 on, 4 off), the lights 0xde data[0]; the GPS speed from knots x 100 (x 1.852).
inline void CanRecLive(wchar_t* a, int na, wchar_t* b, int nb, const VCan3* c)
{
    wchar_t sp[24] = L"—", st[16] = L"—", ig[8] = L"—", li[8] = L"—", gps[48] = L"нет";
    if (c->hasCar)
        _snwprintf(sp, 24, L"%d,%02d км/ч", c->carKmh100 / 100, c->carKmh100 % 100);
    if (c->hasWheel) {
        _snwprintf(st, 16, L"%.1f°", c->wheel ? (c->wheel - 32767) / 15.0 : 0.0);
        st[15] = 0;
        wchar_t* dot = wcschr(st, L'.');
        if (dot)
            *dot = L',';
    }
    if (c->hasGear)
        _snwprintf(ig, 8, c->gear == 5 ? L"вкл" : c->gear == 4 ? L"выкл" : L"%d", c->gear);
    if (c->hasDoors)
        _snwprintf(li, 8, L"%02X", c->doors);
    int kmh10 = c->kn100 * 1852 / 10000;
    if (c->hasPos)
        _snwprintf(gps, 48, c->hasMove ? L"%d,%d км/ч, курс %d°" : L"есть", kmh10 / 10, kmh10 % 10, c->course100 / 100);
    _snwprintf(a, na, L"Скорость %s · руль %s · зажигание %s", sp, st, ig);
    a[na - 1] = 0;
    _snwprintf(b, nb, L"Свет %s · GPS %s · кадров %lu", li, gps, c->frames);
    b[nb - 1] = 0;
}

// The bridge (the user 2026-11-27: "NMEA с рулём в ком 4, 5 ... когда панель выключена"): with the panel's firmware
// here (its exe) but the panel not running, VestaHome reads COM3 itself and writes what the panel writes - VSP4 (read
// as COM4): RMC, @ANG, "@,"; VSP5 / COM7 (the navigators): RMC, GGA. Not in the first minute (Init.exe starts the
// panel just after the shell, and COM3 is one reader's), not while COM3 is read already (the recording does it too).
inline bool CanBridgeWanted(bool panelExe, bool panelRuns, DWORD upMs, bool comRead)
{
    return panelExe && !panelRuns && upMs >= 60000 && !comRead;
}

// What to write now: after each new 0xa2 the position lines (the module sends it 1/s), after a new 0xc6 the wheel -
// not more often than every 400 ms (it comes ~5/s; the panel's own ~2/s)
enum { CBO_POS = 1, CBO_ANG = 2 };
struct CanOut { unsigned long posN, wheelN; DWORD angTick; };

inline int CanBridgeDue(const VCan3* c, CanOut* o, DWORD now)
{
    int due = 0;
    if (c->posN != o->posN) {
        o->posN = c->posN;
        due |= CBO_POS;
    }
    if (c->wheelN != o->wheelN && now - o->angTick >= 400) {
        o->wheelN = c->wheelN;
        o->angTick = now;
        due |= CBO_ANG;
    }
    return due;
}
