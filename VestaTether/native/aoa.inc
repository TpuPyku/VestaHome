// aoa.inc - Android Auto by cable (2026-11-15, the user: "AA, если открыт VestaAuto", else tethering), part of
// vtether.cpp. While VestaAuto runs (it holds the mutex VestaAuto_AA) a phone that comes is switched into accessory
// mode (Android Open Accessory: requests 51, 52, 53 - as VestaAuto/tools/aa_bench.py on the PC); it comes again as
// 18D1:2D00..2D05, and its accessory interface (FF/FF/00, a bulk pair) becomes the stream device AOA1: - ReadFile and
// WriteFile there are the phone's bulk transfers, for VestaAuto's Android Auto session.

enum { AOA_GET_PROTOCOL = 51, AOA_SEND_STRING = 52, AOA_START = 53 };
static const char* const kAoaStrings[6] = { "Android", "Android Auto", "Android Auto", "2.0.1",
                                            "https://github.com/TpuPyku/VestaAuto", "VestaAuto-0001" };
static DWORD s_aoaReadMs = 1000;   // a read with nothing from the phone ends with 0 after it
static DWORD s_aoaWriteMs = 3000;

static bool AaWanted()   // VestaAuto is open
{
    HANDLE m = CreateMutex(NULL, FALSE, L"VestaAuto_AA");
    bool open = m && GetLastError() == ERROR_ALREADY_EXISTS;
    if (m)
        CloseHandle(m);
    return open;
}

static bool IsAccessory(LPCUSB_DEVICE dev)
{
    return dev->Descriptor.idVendor == 0x18D1 && dev->Descriptor.idProduct >= 0x2D00 && dev->Descriptor.idProduct <= 0x2D05;
}

static bool IsAoaItf(LPCUSB_INTERFACE itf)
{
    const USB_INTERFACE_DESCRIPTOR& d = itf->Descriptor;
    return d.bInterfaceClass == 0xFF && d.bInterfaceSubClass == 0xFF && d.bInterfaceProtocol == 0 && HasBulkPair(itf);
}

static LPCUSB_INTERFACE FindAoa(LPCUSB_CONFIGURATION cfg)
{
    for (DWORD i = 0; cfg && i < cfg->dwNumInterfaces; i++)
        if (IsAoaItf(ItfAt(cfg, i)))
            return ItfAt(cfg, i);
    return NULL;
}

// The phone asked to come again as an accessory: false - it has no accessory mode (or refused)
static bool SwitchToAccessory(USB_HANDLE h, LPCUSB_FUNCS f)
{
    u8 ver[2] = { 0, 0 };
    DWORD got = 0;
    if (!Request(h, f, USB_REQUEST_VENDOR, true, AOA_GET_PROTOCOL, 0, ver, 2, &got) || got < 2 || !(ver[0] | ver[1])) {
        Log(L"AOA: телефон не умеет режим аксессуара - как обычно");
        return false;
    }
    for (WORD i = 0; i < 6; i++)
        if (!Request(h, f, USB_REQUEST_VENDOR, false, AOA_SEND_STRING, i, (void*)kAoaStrings[i], (WORD)(strlen(kAoaStrings[i]) + 1), &got))
            return false;
    if (!Request(h, f, USB_REQUEST_VENDOR, false, AOA_START, 0, NULL, 0, &got))
        return false;
    Log(L"AOA: VestaAuto открыт - телефон переводится в Android Auto (AOA %u)", ver[0] | ver[1] << 8);
    return true;
}

struct AoaDev {
    USB_HANDLE    h;
    LPCUSB_FUNCS  f;
    volatile bool gone;
    USB_PIPE      in, out;
    HANDLE        stream;   // AOA1:
    volatile LONG busy;     // reads and writes under way
};

static AoaDev* volatile s_aoa;   // one phone at a time
static CRITICAL_SECTION s_aoaCs;
static volatile LONG s_aoaCsOn;

static void AoaLock()
{
    if (!InterlockedExchange((LONG*)&s_aoaCsOn, 1))   // (the first user: the attach, before any open)
        InitializeCriticalSection(&s_aoaCs);
    EnterCriticalSection(&s_aoaCs);
}

#ifdef UNDER_CE
static HANDLE StreamOn()   // AOA1: of this same DLL (the key: the unit's registry is new at each start)
{
    HKEY k;
    DWORD disp, index = 1;
    if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, L"Drivers\\VestaAOA", 0, NULL, 0, 0, NULL, &k, &disp) != ERROR_SUCCESS)
        return NULL;
    RegSetValueEx(k, L"Dll", 0, REG_SZ, (const BYTE*)L"VestaTether.dll", sizeof(L"VestaTether.dll"));
    RegSetValueEx(k, L"Prefix", 0, REG_SZ, (const BYTE*)L"AOA", sizeof(L"AOA"));
    RegSetValueEx(k, L"Index", 0, REG_DWORD, (const BYTE*)&index, sizeof(index));
    RegCloseKey(k);
    return ActivateDeviceEx(L"Drivers\\VestaAOA", NULL, 0, NULL);
}

static void StreamOff(HANDLE s)
{
    if (s)
        DeactivateDevice(s);
}
#else
static HANDLE StreamOn() { return (HANDLE)1; }   // (the PC's tests call AOA_* themselves)
static void StreamOff(HANDLE) {}
#endif

static BOOL WINAPI AoaNotify(LPVOID param, DWORD code, LPDWORD*, LPDWORD*, LPDWORD*, LPDWORD*)
{
    if (code != USB_CLOSE_DEVICE)
        return FALSE;
    AoaDev* d = (AoaDev*)param;
    AoaLock();
    d->gone = true;
    s_aoa = NULL;
    LeaveCriticalSection(&s_aoaCs);
    while (d->busy)   // (they end within a slice of Bulk's wait)
        Sleep(20);
    StreamOff(d->stream);
    if (d->in)
        d->f->lpClosePipe(d->in);
    if (d->out)
        d->f->lpClosePipe(d->out);
    free(d);
    Log(L"AOA: телефон отключён");
    return TRUE;
}

static void StartAoa(USB_HANDLE h, LPCUSB_FUNCS f, LPCUSB_INTERFACE itf, LPBOOL accept)
{
    AoaDev* d = (AoaDev*)calloc(1, sizeof(AoaDev));
    if (!d)
        return;
    d->h = h;
    d->f = f;
    d->in = f->lpOpenPipe(h, &FindEp(itf, true)->Descriptor);
    d->out = f->lpOpenPipe(h, &FindEp(itf, false)->Descriptor);
    f->lpRegisterNotificationRoutine(h, AoaNotify, d);
    *accept = TRUE;
    AoaLock();
    s_aoa = d;
    LeaveCriticalSection(&s_aoaCs);
    d->stream = d->in && d->out ? StreamOn() : NULL;
    Log(d->stream ? L"AOA: аксессуар Android Auto - AOA1: готов" : L"AOA: аксессуар есть, AOA1: не создан");
}

// ---- AOA1: (a stream driver's entry points; the device manager loads this DLL once more - the same module, the same s_aoa)

static AoaDev* AoaTake()   // the phone, held for one transfer; NULL - gone
{
    AoaLock();
    AoaDev* d = s_aoa;
    if (d)
        InterlockedIncrement((LONG*)&d->busy);
    LeaveCriticalSection(&s_aoaCs);
    return d;
}

static DWORD AoaMove(bool in, void* buf, DWORD len, DWORD ms)
{
    AoaDev* d = AoaTake();
    if (!d)
        return (DWORD)-1;
    DWORD got = 0;
    bool ok = Bulk(d, in ? d->in : d->out, in, buf, len, &got, ms);
    bool gone = d->gone;
    InterlockedDecrement((LONG*)&d->busy);
    if (gone || (!ok && !in))
        return (DWORD)-1;
    return got;   // (a read with nothing in time: 0)
}

extern "C" DWORD AOA_Init(DWORD) { return 1; }
extern "C" BOOL AOA_Deinit(DWORD) { return TRUE; }
extern "C" DWORD AOA_Open(DWORD, DWORD, DWORD) { return s_aoa ? 1 : 0; }
extern "C" BOOL AOA_Close(DWORD) { return TRUE; }
extern "C" DWORD AOA_Read(DWORD, LPVOID buf, DWORD len) { return AoaMove(true, buf, len, s_aoaReadMs); }
extern "C" DWORD AOA_Write(DWORD, LPCVOID buf, DWORD len) { return AoaMove(false, (void*)buf, len, s_aoaWriteMs); }
extern "C" DWORD AOA_Seek(DWORD, long, DWORD) { return (DWORD)-1; }
extern "C" BOOL AOA_IOControl(DWORD, DWORD, PBYTE, DWORD, PBYTE, DWORD, PDWORD) { return FALSE; }
