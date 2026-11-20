// privacy.h - what VHCheck's log leaves out, so the log can be passed around: the unit's serial (the MCU's 0x4112,
// the last 2 digits kept), its Bluetooth address (0x7922, the maker's 3 bytes kept) and anything after the type of a
// port's line (a GPS line's position and time). Pure, header-only (test_privacy).
#ifndef VHC_PRIVACY_H
#define VHC_PRIVACY_H

// Byte k of an MCU answer d[0..len) is not to be logged.
static bool InfoHidden(int code, const unsigned char* d, int len, int k)
{
    if (code == 0x7922)   // 00 06 <6 bytes of the address>
        return k >= 5;
    if (code != 0x4112 || d[k] == ' ' || k == 0)   // 00 <the serial as text, spaces after>
        return false;
    int after = 0;
    for (int i = k + 1; i < len; i++)
        after += d[i] != ' ';
    return after >= 2;
}

// The type of a port's line (up to ',' or '*'): "$GPRMC" of "$GPRMC,123519,A,5545.12,N,...".
static void LineKind(const char* line, char* out, int cap)
{
    int n = 0;
    while (n < cap - 1 && line[n] && line[n] != ',' && line[n] != '*') {
        out[n] = line[n];
        n++;
    }
    out[n] = 0;
}

#endif
