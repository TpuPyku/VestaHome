// Secrets kept in VestaHome.ini (the Wi-Fi passwords): sealed with a key of this head unit, so that the SD card
// read elsewhere (another unit, a PC) does not give them away. The key = SHA-256 of the unit's device id
// (KernelIoControl IOCTL_HAL_GET_DEVICEID; without it the MAC of the Wi-Fi adapter); ChaCha20 + HMAC-SHA-256
// (BearSSL). The text form is "v1:" + hex (nonce, cipher text, tag).
#ifndef VESTA_SECRET_H
#define VESTA_SECRET_H

int  SecretInit();   // once, before the settings are read; the key's source: 1 device id, 2 MAC, 3 PC, 0 none
bool SecretSeal(const wchar_t* plain, wchar_t* out, int cap);     // false: too long
bool SecretOpen(const wchar_t* sealed, wchar_t* plain, int cap);  // false: not sealed here, or damaged
bool SecretIsSealed(const wchar_t* s);

#endif
