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

// The check picture (the user 2026-11-28: "виджет с графическим сопоставлением данных, что бы тестер мог напрямую
// посмотреть соответствуют ли данные"): the car drawn with what CAN says beside the tester's own eyes - each part
// not come yet (dim), off / closed, on / open; the numbers in rows under it. can_panel 1.3.6.1's reading (vcan3.h).
enum { CNP_NONE, CNP_OFF, CNP_ON };
inline int CanPart(bool has, bool on)
{
    return !has ? CNP_NONE : on ? CNP_ON : CNP_OFF;
}

// v halves: "47" / "47,5"
inline void CanHalf(wchar_t* out, int cap, int v2)
{
    _snwprintf(out, cap, v2 % 2 ? L"%d,5" : L"%d", v2 / 2);
    out[cap - 1] = 0;
}

inline void CanWheelText(wchar_t* out, int cap, const VCan3* c)
{
    if (!c->hasWheel) {
        _snwprintf(out, cap, L"руль —");
        out[cap - 1] = 0;
        return;
    }
    _snwprintf(out, cap, L"руль %.1f°", c->wheel ? (c->wheel - 32767) / 15.0 : 0.0);
    out[cap - 1] = 0;
    wchar_t* dot = wcschr(out, L'.');
    if (dot)
        *dot = L',';
}

enum { CRR_COUNT = 6 };

// a value by its format, a dash when not come yet
inline void CanVal(wchar_t* out, int cap, bool has, const wchar_t* fmt, int v)
{
    if (has)
        _snwprintf(out, cap, fmt, v);
    else
        _snwprintf(out, cap, L"—");
    out[cap - 1] = 0;
}

// The run's result on the diagnostics' button (the user 2026-11-28: a failure as a short code, its table in
// PROJECT.md): the letter - what failed (P COM3 not opened, F the file not made, T no thread), the number - Windows'
// GetLastError
enum { CRF_PORT = 'P', CRF_FILE = 'F', CRF_THREAD = 'T' };

inline void CanRecResult(wchar_t* out, int cap, bool ok, unsigned long bytes, int kind, unsigned long err)
{
    if (ok) {
        CanVal(out, cap, true, L"записано %d КБ", (int)(bytes / 1024));
        return;
    }
    _snwprintf(out, cap, L"ошибка %c%lu", (wchar_t)kind, err);
    out[cap - 1] = 0;
}

// the ignition 0x50 data[0] & 7: 5 on, 4 off
inline void CanIgn(wchar_t* out, const VCan3* c)
{
    if (c->hasBody)
        _snwprintf(out, 8, c->ign == 5 ? L"вкл" : c->ign == 4 ? L"выкл" : L"%d", c->ign);
    else
        wcscpy(out, L"—");
}

// the climate's d1: 8 LOW, 10 HI, else halves of a degree; the fan's d3 >> 4: 15 off, 0 auto, else its step
inline void CanRowClima(wchar_t* out, const VCan3* c)
{
    wchar_t t[12] = L"—", f[12] = L"—";
    if (c->hasClima) {
        if (c->climaT == 8 || c->climaT == 10)
            _snwprintf(t, 12, c->climaT == 8 ? L"LOW" : L"HI");
        else {
            CanHalf(t, 10, c->climaT);
            wcscat(t, L"°");
        }
        _snwprintf(f, 12, c->fan == 15 ? L"выкл" : c->fan ? L"%d" : L"авто", c->fan);
    }
    _snwprintf(out, 96, L"Климат %s · вентилятор %s", t, f);
}

inline void CanRowTrip(wchar_t* out, const VCan3* c)
{
    if (c->hasTrip)
        _snwprintf(out, 96, L"БК %d,%d л/100 · %d,%d км · %d,%d л · %d,%d км/ч", c->use10 / 10, c->use10 % 10,
                   c->trip10 / 10, c->trip10 % 10, c->used10 / 10, c->used10 % 10, c->avg10 / 10, c->avg10 % 10);
    else
        _snwprintf(out, 96, L"БК —");
}

// the GPS speed from knots x 100 (x 1.852)
inline void CanRowGps(wchar_t* out, const VCan3* c)
{
    wchar_t gps[48] = L"нет";
    int kmh10 = c->kn100 * 1852 / 10000;
    if (c->hasPos)
        _snwprintf(gps, 48, c->hasMove ? L"%d,%d км/ч, курс %d°" : L"есть", kmh10 / 10, kmh10 % 10, c->course100 / 100);
    _snwprintf(out, 96, L"GPS %s · кадров %lu", gps, c->frames);
}

inline void CanRecRows(wchar_t (*rows)[96], const VCan3* c)
{
    wchar_t sp[24] = L"—", rp[12], ig[8], co[12], ai[12], l[16] = L"—", km[16];
    if (c->hasCar)
        _snwprintf(sp, 24, L"%d,%02d км/ч", c->carKmh100 / 100, c->carKmh100 % 100);
    CanVal(rp, 12, c->hasRpm, L"%d", c->rpm);
    CanIgn(ig, c);
    _snwprintf(rows[0], 96, L"Скорость %s · обороты %s · зажигание %s", sp, rp, ig);
    CanVal(co, 12, c->hasCool, L"%d°", c->coolC);
    CanVal(ai, 12, c->hasAir, L"%d°", c->airC);
    _snwprintf(rows[1], 96, L"ОЖ %s · улица %s", co, ai);
    if (c->hasFuel) {
        CanHalf(l, 12, c->fuel2);
        wcscat(l, L" л");
    }
    CanVal(km, 16, c->hasFuel, L"%d км", c->rangeKm);
    _snwprintf(rows[2], 96, L"Топливо %s · запас %s", l, km);
    CanRowClima(rows[3], c);
    CanRowTrip(rows[4], c);
    CanRowGps(rows[5], c);
    for (int i = 0; i < CRR_COUNT; i++)
        rows[i][95] = 0;
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
