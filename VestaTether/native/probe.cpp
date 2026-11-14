// TetherProbe.exe - run from the card, by hand: lets VestaTether.dll (beside it) see what a phone offers. Copies it into
// \Windows and writes USBD's keys, both new at every boot: after a restart there is no trace of it. Wi-Fi's and Bluetooth's
// drivers are not touched; the keys are for RNDIS classes, and the last resort for a device nothing else takes (logged,
// then left alone).
// 2026-11-12: also the phone's debugging interface (ADB, CheryLink's way: the tethering switched on by a command), with
// this unit's key - made once beside this exe (VestaTetherKey.bin), its path for the driver in the registry.
#include <windows.h>
#include <usbdi.h>
#define USBCLI_LOG L"\\VestaTether.log"
#include "../../VestaModem/native/usbcli.h"   // (its Log: the driver's log, a line of ours before the phone's)
#include "adb.h"

static const wchar_t* const kKeys[] = {
    L"Drivers\\USB\\LoadClients\\Default\\Default\\224_1_3\\VestaTether",    // the interface: E0/01/03
    L"Drivers\\USB\\LoadClients\\Default\\Default\\239_4_1\\VestaTether",    // the interface: EF/04/01
    L"Drivers\\USB\\LoadClients\\Default\\224_1_3\\Default\\VestaTether",    // the whole device
    L"Drivers\\USB\\LoadClients\\Default\\239_2_1\\Default\\VestaTether",    // a composite with interface associations
    L"Drivers\\USB\\LoadClients\\Default\\Default\\255_66_1\\VestaTether",   // a phone's ADB interface: FF/42/01
    L"Drivers\\USB\\LoadClients\\Default\\Default\\Default\\VestaTether",    // anything without a driver: only logged
    // Whole devices by VID_PID, as VestaModem's keys that work on the unit (HU 2026-11-14: the class keys above never
    // brought the phone to us). The PC bench (tools/tether_bench.py, Xiaomi 23117RA68G): with "USB debugging" it is
    // 18D1:4EE7 (ADB alone), after the command 2717:FF88 (RNDIS E0/01/03 + CDC data + ADB).
    L"Drivers\\USB\\LoadClients\\6353_20199\\Default\\Default\\VestaTether",  // 18D1:4EE7 - Google's ADB id, many phones
    L"Drivers\\USB\\LoadClients\\10007_65416\\Default\\Default\\VestaTether"  // 2717:FF88 - Xiaomi's tethering
};

// Randomness for the key: the system's generator where there is one, and whatever differs at each run
static int Seed(BYTE* out)
{
    typedef BOOL (WINAPI *GenRandom)(DWORD, PBYTE);
    int n = 0;
    HMODULE core = LoadLibrary(L"coredll.dll");
    GenRandom gen = core ? (GenRandom)GetProcAddress(core, L"CeGenRandom") : NULL;
    if (gen && gen(32, out))
        n = 32;
    DWORD more[4] = { GetTickCount(), (DWORD)(INT_PTR)&n, GetCurrentProcessId(), 0 };
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    more[3] = t.LowPart;
    memcpy(out + n, more, sizeof(more));
    return n + (int)sizeof(more);
}

// The key made in a thread while a window shows how long (HU 2026-11-13: RSA-2048 takes minutes here; with only an
// hourglass the probe looked hung and was started four times, the copies slowing each other)
struct Making { BYTE seed[64]; int len; AdbKey* k; bool made; };
static DWORD s_keySec;   // how long it took

static DWORD WINAPI MakeThread(void* p)
{
    Making* m = (Making*)p;
    m->made = AdbKeyMake(m->seed, m->len, m->k);
    return 0;
}

static bool MakeKey(AdbKey* k)
{
    Making m;
    m.len = Seed(m.seed);
    m.k = k;
    m.made = false;
    HWND w = CreateWindow(L"STATIC", L"", WS_POPUP | WS_BORDER | WS_VISIBLE | SS_CENTER, 100, 140, 600, 200, NULL, NULL,
                          GetModuleHandle(NULL), NULL);
    SetWindowPos(w, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    HANDLE t = CreateThread(NULL, 0, MakeThread, &m, 0, NULL);
    DWORD t0 = GetTickCount();
    while (t && MsgWaitForMultipleObjects(1, &t, FALSE, 1000, QS_ALLINPUT) != WAIT_OBJECT_0) {
        wchar_t s[200];
        wsprintf(s, L"\r\nПроба USB: создаю ключ ГУ (только в первый раз).\r\nЭто до нескольких минут: %u с.\r\n\r\n"
                    L"Не выключайте ГУ и не запускайте пробу снова.", (GetTickCount() - t0) / 1000);
        SetWindowText(w, s);
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
            DispatchMessage(&msg);
    }
    s_keySec = (GetTickCount() - t0) / 1000;
    if (t)
        CloseHandle(t);
    DestroyWindow(w);
    return m.made;
}

// This unit's ADB key beside the exe: read, or made and written
static bool Key(const wchar_t* path)
{
    BYTE buf[2000];
    DWORD got = 0;
    AdbKey k;
    HANDLE f = CreateFile(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (f != INVALID_HANDLE_VALUE) {
        ReadFile(f, buf, sizeof(buf), &got, NULL);
        CloseHandle(f);
        if (AdbKeyParse(buf, (int)got, &k))
            return true;
    }
    int len = MakeKey(&k) ? AdbKeyPack(&k, buf, sizeof(buf)) : 0;
    f = len ? CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL) : INVALID_HANDLE_VALUE;
    if (f == INVALID_HANDLE_VALUE)
        return false;
    bool ok = WriteFile(f, buf, (DWORD)len, &got, NULL) && got == (DWORD)len;
    CloseHandle(f);
    return ok;
}

static DWORD KeyForDriver(const wchar_t* dir)
{
    wchar_t path[MAX_PATH];
    wcscpy(path, dir);
    wcscat(path, L"VestaTetherKey.bin");
    if (!Key(path))
        return ERROR_WRITE_FAULT;
    HKEY k;
    DWORD disp;
    LONG r = RegCreateKeyEx(HKEY_LOCAL_MACHINE, L"Software\\VestaTether", 0, NULL, 0, 0, NULL, &k, &disp);
    if (r != ERROR_SUCCESS)
        return (DWORD)r;
    RegSetValueEx(k, L"AdbKey", 0, REG_SZ, (const BYTE*)path, (DWORD)(wcslen(path) + 1) * sizeof(wchar_t));
    RegCloseKey(k);
    return 0;
}

static DWORD Install()
{
    wchar_t src[MAX_PATH];
    DWORD n = GetModuleFileName(NULL, src, MAX_PATH);
    while (n && src[n - 1] != L'\\')
        n--;
    src[n] = 0;
    DWORD err = KeyForDriver(src);
    if (err)
        return err;
    wcscpy(src + n, L"VestaTether.dll");
    if (!CopyFile(src, L"\\Windows\\VestaTether.dll", FALSE) && GetFileAttributes(L"\\Windows\\VestaTether.dll") == 0xFFFFFFFF)
        return GetLastError();
    for (int i = 0; i < (int)(sizeof(kKeys) / sizeof(kKeys[0])); i++) {
        HKEY k;
        DWORD disp;
        LONG r = RegCreateKeyEx(HKEY_LOCAL_MACHINE, kKeys[i], 0, NULL, 0, 0, NULL, &k, &disp);
        if (r != ERROR_SUCCESS)
            return (DWORD)r;
        static const wchar_t kDll[] = L"VestaTether.dll";
        RegSetValueEx(k, L"DLL", 0, REG_SZ, (const BYTE*)kDll, sizeof(kDll));
        RegCloseKey(k);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
{
    CreateMutex(NULL, TRUE, L"VestaTetherProbe");   // (one copy: freed when it exits)
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBox(NULL, L"Проба уже работает (создаёт ключ ГУ) - подождите её окна.", L"Проба телефона по USB",
                   MB_OK | MB_TOPMOST);
        return 1;
    }
    InitializeCriticalSection(&s_logCs);   // (usbcli.h's lock: its DllMain is the driver's)
    DWORD err = Install();
    // (HU 2026-11-14, sheet 33 p. 9-10: the cable in, nothing - no log at all: did USBD never call the driver? This line
    // says the probe did its part; the driver's own "устройство ..." follows when USBD hands it the phone)
    Log(L"проба: %s; теперь подключите телефон кабелем (без перезагрузки ГУ - она стирает драйвер)",
        err ? L"НЕ ВЫШЛО" : L"драйвер в \\Windows, ключи USBD записаны");
    if (err)
        Log(L"проба: ошибка %u", err);
    wchar_t s[600] = L"";
    if (err)
        wsprintf(s, L"Не вышло, ошибка %u. Драйвер не установлен.", err);
    else if (s_keySec)
        wsprintf(s, L"Ключ ГУ создан за %u с.\r\n", s_keySec);
    if (!err)
        wcscat(s, L"Готово. На телефоне включите «Отладка по USB» (меню «Для разработчиков») и подключите его к USB. "
                  L"Телефон спросит «Разрешить отладку по USB?» - разрешите, ГУ само включит на нём «USB-модем».\r\n"
                  L"(Или включите «USB-модем» на телефоне сами.)\r\n\r\n"
                  L"Журнал: VestaLogs\\VestaTether.log на SD-карте.\r\nПосле перезагрузки ГУ проба исчезает сама.");
    MessageBox(NULL, s, L"Проба телефона по USB", MB_OK | MB_TOPMOST);
    return err ? 1 : 0;
}
