// adb.h - just enough of Android's debug bridge (ADB, the host's side) to run one shell command on a phone over USB:
// "svc usb setFunctions rndis" turns its USB tethering on even where the phone greys the switch out (the head unit is
// no PC to it). The way CheryLink's head unit talks to phones (2026-11-12). The phone asks its user once to allow this
// unit's key ("Allow USB debugging?"), then knows it. Plain bytes only: no USB here, so it runs in the PC's tests.
#ifndef ADB_H
#define ADB_H
#include "rndis.h"   // (u8, u32)

enum {
    ADB_HEADER  = 24,       // a message: its header, then its data (len bytes) as a transfer of its own
    ADB_MAXDATA = 4096,     // the most data in a message we take
    ADB_VERSION = 0x01000000,
    ADB_CNXN = 0x4E584E43, ADB_AUTH = 0x48545541, ADB_OPEN = 0x4E45504F,
    ADB_OKAY = 0x59414B4F, ADB_CLSE = 0x45534C43, ADB_WRTE = 0x45545257,
    ADB_AUTH_TOKEN = 1, ADB_AUTH_SIGNATURE = 2, ADB_AUTH_RSAPUBLICKEY = 3,
    ADB_TOKEN = 20,         // the phone's token: signed as a SHA-1 digest
    ADB_KEY_BYTES = 256     // RSA-2048, as adb's own keys
};

struct AdbMsg { u32 cmd, a0, a1, len; };

int  AdbHeader(u8* out, u32 cmd, u32 a0, u32 a1, const u8* data, u32 len);   // 24 bytes: the header of that message
bool AdbParse(const u8* h, int n, AdbMsg* m);   // a whole header with its magic, data no longer than ADB_MAXDATA

// The public key as the phone stores it (adb's "adbkey.pub": Android's mincrypt RSAPublicKey in base64, a space, a
// name), with its 0 at the end; returns the bytes with it, 0 - no room
int AdbPubKey(const u8* n, u32 e, const char* name, char* out, int cap);   // n: the modulus, ADB_KEY_BYTES, big endian

// The key of this unit, kept in a file beside TetherProbe.exe: the RSA parts, each big endian
struct AdbBlob { int len; u8 b[ADB_KEY_BYTES]; };
struct AdbKey { AdbBlob n, e, p, q, dp, dq, iq; };
bool AdbKeyMake(const u8* seed, int len, AdbKey* k);      // a new RSA-2048 key from that randomness (e = 65537)
int  AdbKeyPack(const AdbKey* k, u8* out, int cap);       // the file's bytes; 0 - no room
bool AdbKeyParse(const u8* in, int len, AdbKey* k);       // false - not such a file
bool AdbSign(const AdbKey* k, const u8* token, u8* sig);  // sig: ADB_KEY_BYTES - the token signed (PKCS#1, SHA-1)

// One session: connect, prove the key (or offer it to the phone's user), open the shell command, take its output
enum AdbState { ADB_HELLO, ADB_SIGNED, ADB_KEYSENT, ADB_OPENING, ADB_STREAM, ADB_DONE, ADB_FAILED };
enum AdbDo {
    ADB_DO_WAIT,      // nothing to send
    ADB_DO_SIGN,      // AUTH SIGNATURE of the message's token
    ADB_DO_SENDKEY,   // AUTH RSAPUBLICKEY: the phone asks its user
    ADB_DO_OPEN,      // OPEN the command
    ADB_DO_OKAY,      // the command's output (the message's data) taken: OKAY back
    ADB_DO_DONE,      // the command is over
    ADB_DO_FAIL       // the key refused
};
struct Adb { int state; u32 local, remote; };
void AdbStart(Adb* a, u32 local);           // before CNXN is sent; local - our stream's id
int  AdbOn(Adb* a, const AdbMsg& m);        // the phone's message: what to do (ADB_DO_*)

#endif
