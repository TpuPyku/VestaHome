// TetherProbe.exe - run from the card, by hand: lets VestaTether.dll (beside it) see what a phone offers. Copies it into
// \Windows and writes USBD's keys, both new at every boot: after a restart there is no trace of it. Wi-Fi's and Bluetooth's
// drivers are not touched; the keys are for RNDIS classes, and the last resort for a device nothing else takes (logged,
// then left alone).
#include <windows.h>

static const wchar_t* const kKeys[] = {
    L"Drivers\\USB\\LoadClients\\Default\\Default\\224_1_3\\VestaTether",    // the interface: E0/01/03
    L"Drivers\\USB\\LoadClients\\Default\\Default\\239_4_1\\VestaTether",    // the interface: EF/04/01
    L"Drivers\\USB\\LoadClients\\Default\\224_1_3\\Default\\VestaTether",    // the whole device
    L"Drivers\\USB\\LoadClients\\Default\\239_2_1\\Default\\VestaTether",    // a composite with interface associations
    L"Drivers\\USB\\LoadClients\\Default\\Default\\Default\\VestaTether"     // anything without a driver: only logged
};

static DWORD Install()
{
    wchar_t src[MAX_PATH];
    DWORD n = GetModuleFileName(NULL, src, MAX_PATH);
    while (n && src[n - 1] != L'\\')
        n--;
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
        wcscpy(s, L"Готово. Включите на телефоне «USB-модем» и подключите его к USB.\r\n\r\n"
                  L"Журнал: VestaLogs\\VestaTether.log на SD-карте.\r\nПосле перезагрузки ГУ проба исчезает сама.");
    MessageBox(NULL, s, L"Проба телефона по USB", MB_OK | MB_TOPMOST);
    return err ? 1 : 0;
}
