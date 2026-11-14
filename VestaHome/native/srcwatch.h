// srcwatch.h - a change of the MCU's source seen: ours or the user's. Ours only if we asked for that very source in
// the last kSrcOursMs (HU 2026-11-14-3: our preset after the radio key marked "ours" for 6 s whatever came - MODE to
// the SD card 3 s later was let pass, no music, no screen).
#ifndef SRCWATCH_H
#define SRCWATCH_H

enum { kSrcOursMs = 6000 };
static const unsigned kSrcAny = 0xFFFFFFFFu;   // (AUX: the MCU's number for it learnt as it comes)

static unsigned SrcBit(int src)
{
    return src >= 0 && src < 32 ? 1u << src : 0;
}

// now, asked - ticks (asked 0: never); want - the sources asked for (SrcBit); src - the one now
static bool SrcOurs(unsigned now, unsigned asked, unsigned want, int src)
{
    return asked && now - asked <= (unsigned)kSrcOursMs && (want & SrcBit(src)) != 0;
}

#endif
