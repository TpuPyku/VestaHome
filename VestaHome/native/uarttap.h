// uarttap.h - listening to a port nobody uses (COM2, the CPU's second UART: UartProbe 2026-11-12 heard nothing in
// 2 s; the user 2026-11-25: listen all the drive while the logs are on, in case something comes now and then).
// The log's lines: the bytes as hex and text, the line errors, at most TAP_LINES_MIN lines a minute.
#pragma once
#include <windows.h>
#include <stdio.h>
#include <string.h>

// ClearCommError's error bits (winbase.h names, kept here so the PC test needs nothing else)
enum { TAP_CE_RXOVER = 0x01, TAP_CE_OVERRUN = 0x02, TAP_CE_RXPARITY = 0x04, TAP_CE_FRAME = 0x08, TAP_CE_BREAK = 0x10,
       TAP_LINES_MIN = 30, TAP_ERR_CAP = 48 };

// The printable bytes, dots for the rest, from out[k] on (cut to cap, ended); the new length.
inline int TapText(const unsigned char* b, int n, wchar_t* out, int k, int cap)
{
    for (int i = 0; i < n && k < cap - 1; i++)
        out[k++] = b[i] >= 0x20 && b[i] < 0x7F ? (wchar_t)b[i] : L'.';
    out[k] = 0;
    return k;
}

// "05 41 FF | .A." - each byte as hex, then as text; cut to cap.
inline void TapLine(const unsigned char* b, int n, wchar_t* out, int cap)
{
    static const wchar_t kHex[] = L"0123456789ABCDEF";
    int k = 0;
    for (int i = 0; i < n && k + 3 < cap; i++) {
        if (i)
            out[k++] = L' ';
        out[k++] = kHex[b[i] >> 4];
        out[k++] = kHex[b[i] & 15];
    }
    k = TapText((const unsigned char*)" | ", 3, out, k, cap);
    TapText(b, n, out, k, cap);
}

// The line errors in words, comma separated ("" - none); all five fit in TAP_ERR_CAP.
inline void TapErrors(DWORD ce, wchar_t* out)
{
    static const DWORD bits[5] = { TAP_CE_FRAME, TAP_CE_RXPARITY, TAP_CE_OVERRUN, TAP_CE_RXOVER, TAP_CE_BREAK };
    static const wchar_t* names[5] = { L"кадр", L"чётность", L"переполнение", L"очередь", L"break" };
    out[0] = 0;
    for (int i = 0; i < 5; i++) {
        if (!(ce & bits[i]))
            continue;
        if (out[0])
            wcscat(out, L", ");
        wcscat(out, names[i]);
    }
}

// At most TAP_LINES_MIN lines a minute; what is refused is counted and told when the minute ends.
struct TapBudget {
    DWORD minute;   // the tick the minute started
    int lines;
    unsigned skippedLines, skippedBytes;
};

inline void TapBudgetInit(TapBudget* t, DWORD now)
{
    memset(t, 0, sizeof(*t));
    t->minute = now;
}

inline bool TapAllow(TapBudget* t, unsigned bytes)
{
    if (t->lines < TAP_LINES_MIN) {
        t->lines++;
        return true;
    }
    t->skippedLines++;
    t->skippedBytes += bytes;
    return false;
}

// A minute over: the counts start again; true (and what was skipped) when lines were refused in it.
inline bool TapRoll(TapBudget* t, DWORD now, unsigned* lines, unsigned* bytes)
{
    if (now - t->minute < 60000)
        return false;
    bool said = t->skippedLines > 0;
    *lines = t->skippedLines;
    *bytes = t->skippedBytes;
    TapBudgetInit(t, now);
    return said;
}
