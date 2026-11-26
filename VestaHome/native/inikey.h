// inikey.h - one "Key = value" line of an ini text set (Navitel's settings.*.ini: its GPSEpoch, see vh_apps.inc).
// Pure: no files.
#ifndef VESTA_INIKEY_H
#define VESTA_INIKEY_H

// in (len bytes, lines ending CR LF or LF) with the key's line as "key = val": that line replaced where it is (the
// first of them; key compared whole, case and all), else added at the end (after a line end of its own). Into out:
// its length; -1 when it does not fit cap. *changed: whether out differs from in.
int IniPut(const char* in, int len, const char* key, const char* val, char* out, int cap, bool* changed);
// The same with the line written "key<sep>val" (IniPut: " = "; the CAN panel's ini: "=", it looks for "Key=").
int IniPutSep(const char* in, int len, const char* key, const char* sep, const char* val, char* out, int cap,
              bool* changed);

#endif
