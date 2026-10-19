// Secrets of the settings, sealed with a key of this head unit (see secret.h).
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "secret.h"

extern "C" {
#include "bearssl.h"
}

static unsigned char s_key[32];    // ChaCha20 key
static unsigned char s_mac[32];    // HMAC key
static bool          s_ready;

enum { kNonce = 12, kTag = 8, kMaxPlain = 128 };

#ifdef UNDER_CE
extern "C" BOOL KernelIoControl(DWORD code, LPVOID in, DWORD inSize, LPVOID out, DWORD outSize, LPDWORD ret);
#endif

// What makes this unit itself: its device id (the HAL's), otherwise the MAC addresses of its adapters.
static int s_source;   // 1 the device id, 2 the MAC addresses, 3 the PC's volume, 0 nothing

static int UnitId(unsigned char* buf, int cap)
{
    int n = 0;
#ifdef UNDER_CE
    const DWORD IOCTL_HAL_GET_DEVICEID = 0x01010054;   // CTL_CODE(FILE_DEVICE_HAL, 21, METHOD_BUFFERED, 0)
    DWORD got = 0;
    memset(buf, 0, cap);
    *(DWORD*)buf = cap;   // DEVICE_ID.dwSize
    if (KernelIoControl(IOCTL_HAL_GET_DEVICEID, NULL, 0, buf, cap, &got) && got > 20) {
        s_source = 1;
        return (int)got;
    }
    struct IpAddr { void* next; char ip[16]; char mask[16]; DWORD ctx; };
    struct Adapter { Adapter* next; DWORD combo; char name[260]; char descr[132]; UINT alen; BYTE addr[8]; };
    typedef DWORD (WINAPI *AdaptersFn)(void*, ULONG*);
    HMODULE ip = LoadLibrary(L"iphlpapi.dll");
    AdaptersFn f = ip ? (AdaptersFn)GetProcAddress(ip, L"GetAdaptersInfo") : NULL;
    ULONG size = 0;
    if (f)
        f(NULL, &size);
    if (f && size && size < 64 * 1024) {
        Adapter* a = (Adapter*)malloc(size);
        if (a && f(a, &size) == 0)
            for (Adapter* p = a; p && n + 8 <= cap; p = p->next)
                if (p->alen >= 6 && p->alen <= 8) {
                    memcpy(buf + n, p->addr, p->alen);
                    n += p->alen;
                }
        free(a);
    }
    s_source = n ? 2 : 0;
#else
    DWORD serial = 0;   // PC test build: the system volume's serial number
    GetVolumeInformation(L"C:\\", NULL, 0, &serial, NULL, NULL, NULL, 0);
    memcpy(buf, &serial, 4);
    n = 4;
    s_source = 3;
#endif
    return n;
}

int SecretInit()
{
    unsigned char id[256];
    int n = UnitId(id, sizeof(id));
    br_sha256_context h;
    unsigned char k[32];
    br_sha256_init(&h);
    br_sha256_update(&h, "VestaHome secret key v1", 23);
    br_sha256_update(&h, id, n);
    br_sha256_out(&h, k);
    br_sha256_init(&h);   // two keys from it
    br_sha256_update(&h, k, 32);
    br_sha256_update(&h, "enc", 3);
    br_sha256_out(&h, s_key);
    br_sha256_init(&h);
    br_sha256_update(&h, k, 32);
    br_sha256_update(&h, "mac", 3);
    br_sha256_out(&h, s_mac);
    s_ready = n > 0;
    return s_source;
}

static void Tag(const unsigned char* data, int len, unsigned char* out)
{
    br_hmac_key_context kc;
    br_hmac_context hc;
    br_hmac_key_init(&kc, &br_sha256_vtable, s_mac, 32);
    br_hmac_init(&hc, &kc, kTag);
    br_hmac_update(&hc, data, len);
    br_hmac_out(&hc, out);
}

// A nonce that never repeats for this key: the clock, the counters and a sequence through SHA-256.
static void Nonce(unsigned char* out)
{
    static DWORD s_seq;
    struct { DWORD tick, seq, thread; LARGE_INTEGER perf; SYSTEMTIME t; } src;
    src.tick = GetTickCount();
    src.seq = ++s_seq;
    src.thread = GetCurrentThreadId();
    QueryPerformanceCounter(&src.perf);
    GetSystemTime(&src.t);
    unsigned char h[32];
    br_sha256_context c;
    br_sha256_init(&c);
    br_sha256_update(&c, &src, sizeof(src));
    br_sha256_update(&c, s_key, 32);
    br_sha256_out(&c, h);
    memcpy(out, h, kNonce);
}

bool SecretIsSealed(const wchar_t* s)
{
    return !wcsncmp(s, L"v1:", 3);
}

bool SecretSeal(const wchar_t* plain, wchar_t* out, int cap)
{
    unsigned char buf[kNonce + kMaxPlain + kTag];
    int n = WideCharToMultiByte(CP_UTF8, 0, plain, -1, (char*)buf + kNonce, kMaxPlain, NULL, NULL) - 1;
    int total = kNonce + n + kTag;
    if (!s_ready || n < 0 || cap < 4 + total * 2)
        return false;
    Nonce(buf);
    br_chacha20_ct_run(s_key, buf, 1, buf + kNonce, n);
    Tag(buf, kNonce + n, buf + kNonce + n);
    wcscpy(out, L"v1:");
    for (int i = 0; i < total; i++)
        _snwprintf(out + 3 + i * 2, 3, L"%02x", buf[i]);
    out[3 + total * 2] = 0;
    return true;
}

bool SecretOpen(const wchar_t* sealed, wchar_t* plain, int cap)
{
    plain[0] = 0;
    if (!s_ready || !SecretIsSealed(sealed))
        return false;
    const wchar_t* h = sealed + 3;
    int total = (int)wcslen(h) / 2;
    if (total < kNonce + kTag || total > kNonce + kMaxPlain + kTag)
        return false;
    unsigned char buf[kNonce + kMaxPlain + kTag], tag[kTag];
    for (int i = 0; i < total; i++) {
        unsigned v;
        if (swscanf(h + i * 2, L"%2x", &v) != 1)
            return false;
        buf[i] = (unsigned char)v;
    }
    int n = total - kNonce - kTag;
    Tag(buf, kNonce + n, tag);
    if (memcmp(tag, buf + kNonce + n, kTag))
        return false;   // another unit's key, or damaged
    br_chacha20_ct_run(s_key, buf, 1, buf + kNonce, n);
    buf[kNonce + n] = 0;
    return MultiByteToWideChar(CP_UTF8, 0, (char*)buf + kNonce, -1, plain, cap) > 0;
}
