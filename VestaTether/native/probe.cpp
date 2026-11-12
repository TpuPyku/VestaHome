// TetherProbe.exe - run from the card, by hand: lets VestaTether.dll (beside it) see what a phone offers. Copies it into
// \Windows and writes USBD's keys, both new at every boot: after a restart there is no trace of it. Wi-Fi's and Bluetooth's
// drivers are not touched; the keys are for RNDIS classes, and the last resort for a device nothing else takes (logged,
// then left alone).
// 2026-11-12: also the phone's debugging interface (ADB, CheryLink's way: the tethering switched on by a command), with
// this unit's key - made once beside this exe (VestaTetherKey.bin), its path for the driver in the registry.
#include <windows.h>
#include "adb.h"

static const wchar_t* const kKeys[] = {
    L"Drivers\\USB\\LoadClients\\Default\\Default\\224_1_3\\VestaTether",    // the interface: E0/01/03
    L"Drivers\\USB\\LoadClients\\Default\\Default\\239_4_1\\VestaTether",    // the interface: EF/04/01
    L"Drivers\\USB\\LoadClients\\Default\\224_1_3\\Default\\VestaTether",    // the whole device
    L"Drivers\\USB\\LoadClients\\Default\\239_2_1\\Default\\VestaTether",    // a composite with interface associations
    L"Drivers\\USB\\LoadClients\\Default\\Default\\255_66_1\\VestaTether",   // a phone's ADB interface: FF/42/01
    L"Drivers\\USB\\LoadClients\\Default\\Default\\Default\\VestaTether"     // anything without a driver: only logged
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

// This unit's ADB key beside the exe: read, or made (RSA-2048: up to a minute here) and written
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
    BYTE seed[64];
    SetCursor(LoadCursor(NULL, IDC_WAIT));
    bool made = AdbKeyMake(seed, Seed(seed), &k);
    SetCursor(NULL);
    int len = made ? AdbKeyPack(&k, buf, sizeof(buf)) : 0;
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
    DWORD err = Install();
    wchar_t s[300];
    if (err)
        wsprintf(s, L"Не вышло, ошибка %u. Драйвер не установлен.", err);
    else
        wcscpy(s, L"Готово. На телефоне включите «Отладка по USB» (меню «Для разработчиков») и подключите его к USB. "
                  L"Телефон спросит «Разрешить отладку по USB?» - разрешите, ГУ само включит на нём «USB-модем».\r\n"
                  L"(Или включите «USB-модем» на телефоне сами.)\r\n\r\n"
                  L"Журнал: VestaLogs\\VestaTether.log на SD-карте.\r\nПосле перезагрузки ГУ проба исчезает сама.");
    MessageBox(NULL, s, L"Проба телефона по USB", MB_OK | MB_TOPMOST);
    return err ? 1 : 0;
}
