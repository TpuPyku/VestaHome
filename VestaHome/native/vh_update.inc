// vh_update.inc - part of home.cpp (included there, in order): VestaHome updated from "О системе" (the user
// 2026-11-25: after a restart VestaHome had to be closed within 30 s or the installer found its files held). The
// installer on the SD card or USB disk (VestaHomeInstall\VestaHomeInstall.exe) tells its version and is signed by
// the author (VestaHomeInstall.sig; VestaCore vupdate.h); a newer one - a boot job on \SDMMC and a restart: the loader
// (VestaStub) runs the installer before VestaHome and its add-ons start, if the exe still has the SHA-256 checked here,
// and leaves its outcome in \SDMMC\VestaUpdate.res, shown here after.
#define WM_APP_UPDATE (WM_APP + 35)

static const wchar_t* const kUpdDirs[2] = { L"\\SDMemory\\VestaHomeInstall\\", L"\\USBDevice\\VestaHomeInstall\\" };
static const wchar_t kUpdJob[] = L"\\SDMMC\\VestaUpdate.job";
static const wchar_t kUpdRes[] = L"\\SDMMC\\VestaUpdate.res";

struct UpdState {
    int           state;    // 0 not looked, 1 looking, 2 looked
    int           result;   // UPDATE_*, -1 no installer on the cards
    char          ver[32];
    unsigned char sha[32];
    wchar_t       exe[MAX_PATH];
};
static UpdState      s_upd;
static UpdState      s_updNew;    // the thread's, taken by the window (WM_APP_UPDATE)
static volatile LONG s_updBusy;
static wchar_t       s_updLast[200];   // the last update's outcome (VestaUpdate.res), "" none

static DWORD WINAPI UpdThread(LPVOID)
{
    UpdState u;
    memset(&u, 0, sizeof(u));
    u.result = -1;
    for (int i = 0; i < 2 && u.result < 0; i++) {
        wchar_t sig[MAX_PATH];
        _snwprintf(u.exe, MAX_PATH, L"%sVestaHomeInstall.exe", kUpdDirs[i]);
        _snwprintf(sig, MAX_PATH, L"%sVestaHomeInstall.sig", kUpdDirs[i]);
        if (GetFileAttributes(u.exe) != 0xFFFFFFFF)
            u.result = UpdateCheck(u.exe, sig, NULL, 0, u.ver, 32, u.sha);
    }
    u.state = 2;
    s_updNew = u;
    PostMessage(s_wnd, WM_APP_UPDATE, 0, 0);
    return 0;
}

static void UpdCheckStart()
{
    if (InterlockedExchange((LONG*)&s_updBusy, 1))
        return;
    s_upd.state = 1;
    HANDLE t = CreateThread(NULL, 0, UpdThread, NULL, 0, NULL);
    if (t)
        CloseHandle(t);
    else
        s_updBusy = 0;
}

static void UpdTaken()   // WM_APP_UPDATE: the look is done
{
    s_upd = s_updNew;
    s_updBusy = 0;
    VLog(L"обновление: на карте %S, проверка %d", s_upd.ver[0] ? s_upd.ver : "-", s_upd.result);
}

static int UpdVsMine()   // the card's version against ours (>0 newer)
{
    char mine[32];
    WideCharToMultiByte(CP_ACP, 0, VhVersion(), -1, mine, 32, NULL, NULL);
    return UpdateVersionCmp(s_upd.ver, mine);
}

static bool UpdReady()   // a signed installer of a newer VestaHome
{
    return s_upd.state == 2 && s_upd.result == UPDATE_OK && UpdVsMine() > 0;
}

// "Обновить и перезагрузить": the boot job, the restart (the loader installs before VestaHome starts).
static void UpdGo()
{
    if (!UpdReady())
        return;
    char t[700];
    int n = UpdateJobText(s_upd.exe, s_upd.ver, s_upd.sha, t, sizeof(t));
    HANDLE f = n > 0 ? CreateFile(kUpdJob, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL) : INVALID_HANDLE_VALUE;
    DWORD w = 0;
    bool ok = f != INVALID_HANDLE_VALUE && WriteFile(f, t, n, &w, NULL) && (int)w == n;
    if (f != INVALID_HANDLE_VALUE)
        CloseHandle(f);
    if (!ok) {
        wcscpy(s_status, L"не удалось записать задание обновления");
        return;
    }
    DeleteFile(kUpdRes);
    VLog(L"обновление: задание до %S (%s), перезагрузка", s_upd.ver, s_upd.exe);
    Reboot(true);
}

// The last update's outcome (the installer run by the loader): logged at the start, shown on the screen.
static void UpdResultLoad()
{
    wchar_t r[600];
    DWORD got = 0;
    HANDLE f = CreateFile(kUpdRes, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return;
    ReadFile(f, r, sizeof(r) - 2, &got, NULL);
    CloseHandle(f);
    r[got / 2] = 0;
    wchar_t* ver = wcschr(r, L'\n');
    wchar_t* why = ver ? wcschr(ver + 1, L'\n') : NULL;
    if (ver)
        *ver++ = 0;
    if (why)
        *why++ = 0;
    if (!wcscmp(r, L"ok"))
        _snwprintf(s_updLast, 200, L"Последнее обновление: установлена %s", ver ? ver : L"?");
    else
        _snwprintf(s_updLast, 200, L"Последнее обновление не удалось: %s", why ? why : L"?");
    s_updLast[199] = 0;
    VLog(L"обновление: %s", s_updLast);
}

static void ScrUpdate(HDC dc)
{
    Frame(dc, L"Обновление VestaHome");
    Panel(dc, 16, 68, 768, 344);
    T(dc, 40, 112, L"Установлена", 17, kMuted, 400);
    T(dc, 760, 112, VhVersion(), 17, kText, 700, AL_END);
    wchar_t s[200], v[40];
    MultiByteToWideChar(CP_ACP, 0, s_upd.ver, -1, v, 40);
    const wchar_t* line;
    COLORREF c = kText;
    if (s_upd.state < 2) {
        line = L"Ищу установщик на SD-карте и флешке…";
    } else if (s_upd.result < 0) {
        line = L"Установщика нет: положите папку VestaHomeInstall из архива в корень SD-карты или флешки";
        c = kMuted;
    } else if (s_upd.result == UPDATE_NO_MARK || s_upd.result == UPDATE_NO_FILE) {
        line = L"VestaHomeInstall.exe на карте не читается или старый (без версии) — скачайте архив заново";
        c = kStop;
    } else if (s_upd.result != UPDATE_OK) {
        _snwprintf(s, 200, L"%s %s: %s", L"На карте", v, s_upd.result == UPDATE_NO_SIG ?
                   L"нет подписи VestaHomeInstall.sig — скопируйте папку из архива целиком" :
                   L"подпись не подходит — файл изменён, не запускаю");
        line = s;
        c = kStop;
    } else {
        int cmp = UpdVsMine();
        _snwprintf(s, 200, L"На карте %s — %s", v, cmp > 0 ? L"новее, можно обновить" :
                   cmp == 0 ? L"та же версия" : L"старее установленной");
        line = s;
        c = cmp > 0 ? kOrange : kMuted;
    }
    T(dc, 40, 160, L"На карте", 17, kMuted, 400);
    TWrap(dc, 40, 196, line, 16, c, 400, 720, 3, 24);
    if (s_updLast[0])
        TWrap(dc, 40, 266, s_updLast, 14, kMuted, 400, 720, 2, 20);
    TWrap(dc, 40, 318, L"После «Обновить» ГУ перезагрузится; установка пройдёт до запуска VestaHome, затем она откроется сама.",
          13, kDim, 400, 720, 2, 18);
    Button(dc, 40, 338, 260, 56, L"Проверить снова", s_upd.state == 1 ? B_DIS : B_N, IC_REFRESH, 15, 26, A_UPDCHECK);
    Button(dc, 316, 338, 444, 56, L"Обновить и перезагрузить", UpdReady() ? B_SEL : B_DIS, IC_DOWN, 15, 26, A_UPDGO);
}
