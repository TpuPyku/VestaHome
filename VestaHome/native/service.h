// service.h - the car's scheduled service (ТО, the user 2026-11-14): due after a distance (the Vesta's 15000 km, or
// the recommended 7000) or a year after the last one, whichever comes first. The odometer is the user's typed number
// plus the km VestaHome counted since (the car's own is not on the CAN stream we see).
#pragma once

// yyyymmdd -> a day number (days since 2000-01-01); the dates are compared as these
int YmdDays(int ymd);

// The last service's date (yyyymmdd) -> the same date a year later (29 Feb -> 1 Mar)
int ServiceDueYmd(int lastYmd);

// Left until the service: km (negative: over it by), days (negative: past the date by; lastYmd 0 - no date known:
// days = kNoDays). everyKm: the interval.
enum { kNoDays = 1000000 };
struct ServiceLeft { int km; int days; };
ServiceLeft ServiceLeftAt(double odoKm, double lastKm, int lastYmd, int todayYmd, int everyKm);

// The widget's line for it into out (n chars); the level: 0 fine, 1 soon (under 1000 km or 30 days), 2 overdue.
// odoKm < 0: the odometer not typed yet - a hint to the settings (level 0).
int ServiceText(double odoKm, double lastKm, int lastYmd, int todayYmd, int everyKm, wchar_t* out, int n);

// Typed: km ("31 200", "31200,5"; 0..9 999 999) -> true and *km; a date "14.11.2026" -> yyyymmdd (0: not one)
bool ParseKm(const wchar_t* s, double* km);
int ParseDmy(const wchar_t* s);
// The service interval, km (the user 2026-11-24: one's own number by the number pad): typed "10000", "7 500" ->
// true and *km rounded, kSvcEveryMin..kSvcEveryMax; otherwise false, *km kept. ServiceEveryOf: the settings' value
// (car.service.every, 0: none) or the old switch car.service.7000 (7000), else the Vesta's 15000.
enum { kSvcEveryMin = 1000, kSvcEveryMax = 100000, kSvcEveryVesta = 15000 };
bool ParseEvery(const wchar_t* s, int* km);
int  ServiceEveryOf(int every, bool old7000);
