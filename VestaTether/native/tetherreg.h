// tetherreg.h - VestaTether.dll put where USBD finds it: copied into \Windows and its client keys written, both new at
// every boot. One list for both who do it: TetherProbe (by hand, from the card) and VestaHome (at its start, while
// "Настройки → Сеть → Телефон по USB" is on - the user 2026-11-15: the driver works, build it in). Wi-Fi's and
// Bluetooth's drivers are not touched; the keys are for RNDIS classes, the phone's ADB, the phones seen on the unit and
// the Android Auto accessory. No catch-all "any device without a driver" (the user 2026-11-15: no use, an attack
// surface - in every HU log it never brought a device); the big makers by vendor below; others need their VID here.
#ifndef TETHERREG_H
#define TETHERREG_H

#include <windows.h>

static const wchar_t* const kTetherKeys[] = {
    L"Drivers\\USB\\LoadClients\\Default\\Default\\224_1_3\\VestaTether",    // the interface: E0/01/03
    L"Drivers\\USB\\LoadClients\\Default\\Default\\239_4_1\\VestaTether",    // the interface: EF/04/01
    L"Drivers\\USB\\LoadClients\\Default\\224_1_3\\Default\\VestaTether",    // the whole device
    L"Drivers\\USB\\LoadClients\\Default\\239_2_1\\Default\\VestaTether",    // a composite with interface associations
    L"Drivers\\USB\\LoadClients\\Default\\Default\\255_66_1\\VestaTether",   // a phone's ADB interface: FF/42/01
    // Whole devices by VID_PID, as VestaModem's keys that work on the unit (HU 2026-11-14: the class keys above never
    // brought the phone to us). The PC bench (tools/tether_bench.py, Xiaomi 23117RA68G): with "USB debugging" it is
    // 18D1:4EE7 (ADB alone), after the command 2717:FF88 (RNDIS E0/01/03 + CDC data + ADB).
    L"Drivers\\USB\\LoadClients\\6353_20199\\Default\\Default\\VestaTether",  // 18D1:4EE7 - Google's ADB id, many phones
    L"Drivers\\USB\\LoadClients\\10007_65416\\Default\\Default\\VestaTether", // 2717:FF88 - Xiaomi's tethering
    // Android Auto (2026-11-15): the phone's ordinary modes (HU 2026-11-15: 2717:FF48 MTP + ADB) switched into accessory
    // mode while VestaAuto is open, and the accessory itself - 18D1:2D00 (alone) / 2D01 (+ ADB): AOA1:
    L"Drivers\\USB\\LoadClients\\10007_65352\\Default\\Default\\VestaTether", // 2717:FF48 - Xiaomi, MTP + ADB
    L"Drivers\\USB\\LoadClients\\10007_65344\\Default\\Default\\VestaTether", // 2717:FF40 - Xiaomi, MTP
    L"Drivers\\USB\\LoadClients\\6353_11520\\Default\\Default\\VestaTether",  // 18D1:2D00 - an accessory
    L"Drivers\\USB\\LoadClients\\6353_11521\\Default\\Default\\VestaTether",  // 18D1:2D01 - an accessory + ADB
    // Every phone of the big makers, by the vendor alone (the user 2026-11-15: other phones without a list by hand). USBD
    // takes the most exact key first (VID_PID before VID: VestaModem's Huawei modems stay its), and a device the driver
    // does not accept goes on to its own driver: it holds only what answers the accessory mode's question - Android (a
    // modem, a flash drive of the same maker: not).
    L"Drivers\\USB\\LoadClients\\1256\\Default\\Default\\VestaTether",    // 04E8 Samsung
    L"Drivers\\USB\\LoadClients\\6353\\Default\\Default\\VestaTether",    // 18D1 Google (Pixel; many phones' ADB / AOA ids)
    L"Drivers\\USB\\LoadClients\\10007\\Default\\Default\\VestaTether",   // 2717 Xiaomi, Redmi, POCO
    L"Drivers\\USB\\LoadClients\\8921\\Default\\Default\\VestaTether",    // 22D9 OPPO, Realme, OnePlus (new)
    L"Drivers\\USB\\LoadClients\\11669\\Default\\Default\\VestaTether",   // 2D95 Vivo
    L"Drivers\\USB\\LoadClients\\10864\\Default\\Default\\VestaTether",   // 2A70 OnePlus
    L"Drivers\\USB\\LoadClients\\8888\\Default\\Default\\VestaTether",    // 22B8 Motorola
    L"Drivers\\USB\\LoadClients\\4046\\Default\\Default\\VestaTether",    // 0FCE Sony
    L"Drivers\\USB\\LoadClients\\4100\\Default\\Default\\VestaTether",    // 1004 LG
    L"Drivers\\USB\\LoadClients\\6610\\Default\\Default\\VestaTether",    // 19D2 ZTE
    L"Drivers\\USB\\LoadClients\\2821\\Default\\Default\\VestaTether",    // 0B05 Asus
    L"Drivers\\USB\\LoadClients\\13211\\Default\\Default\\VestaTether",   // 339B Honor
    L"Drivers\\USB\\LoadClients\\11780\\Default\\Default\\VestaTether",   // 2E04 Nokia (HMD)
    L"Drivers\\USB\\LoadClients\\4817\\Default\\Default\\VestaTether",    // 12D1 Huawei (and old Honor)
    L"Drivers\\USB\\LoadClients\\10821\\Default\\Default\\VestaTether",   // 2A45 Meizu
    L"Drivers\\USB\\LoadClients\\6127\\Default\\Default\\VestaTether",    // 17EF Lenovo
    L"Drivers\\USB\\LoadClients\\2996\\Default\\Default\\VestaTether"     // 0BB4 HTC
};
enum { kTetherKeyCount = sizeof(kTetherKeys) / sizeof(kTetherKeys[0]) };
// "Режим совместимости" (Настройки -> Сеть, off by default - the user 2026-11-15): any device without its own driver
// comes to ours too - a phone of a maker not above. It holds only Android; the rest - one question, then let go.
static const wchar_t kTetherAnyKey[] = L"Drivers\\USB\\LoadClients\\Default\\Default\\Default\\VestaTether";

static LONG TetherKey(const wchar_t* path)
{
    HKEY k;
    DWORD disp;
    LONG r = RegCreateKeyEx(HKEY_LOCAL_MACHINE, path, 0, NULL, 0, 0, NULL, &k, &disp);
    if (r != ERROR_SUCCESS)
        return r;
    static const wchar_t kDll[] = L"VestaTether.dll";
    RegSetValueEx(k, L"DLL", 0, REG_SZ, (const BYTE*)kDll, sizeof(kDll));
    RegCloseKey(k);
    return ERROR_SUCCESS;
}

// The driver (its file: `dll`) into \Windows (already loaded - the copy there stays), the keys written (any: the
// compatibility mode's too, else it deleted); 0 or the error.
static DWORD TetherRegister(const wchar_t* dll, bool any)
{
    if (!CopyFile(dll, L"\\Windows\\VestaTether.dll", FALSE) &&
        GetFileAttributes(L"\\Windows\\VestaTether.dll") == 0xFFFFFFFF)
        return GetLastError();
    for (int i = 0; i < kTetherKeyCount; i++) {
        LONG r = TetherKey(kTetherKeys[i]);
        if (r != ERROR_SUCCESS)
            return (DWORD)r;
    }
    if (!any)
        RegDeleteKey(HKEY_LOCAL_MACHINE, kTetherAnyKey);
    return any ? (DWORD)TetherKey(kTetherAnyKey) : 0;
}

// The keys deleted: the next phone in is not ours (one plugged in keeps the driver until it is out).
static void TetherUnregister()
{
    for (int i = 0; i < kTetherKeyCount; i++)
        RegDeleteKey(HKEY_LOCAL_MACHINE, kTetherKeys[i]);
    RegDeleteKey(HKEY_LOCAL_MACHINE, kTetherAnyKey);
}

#endif
