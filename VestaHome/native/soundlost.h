// soundlost.h - an add-on's own sound (Android Auto: "sound") plays on the MCU's media source; when MODE (or anything)
// takes the MCU to another source the add-on is told "sound.lost" (vh_plugins.inc), so the phone pauses its music
// instead of playing unheard (PLAN: AA step 4, the user 2026-11-15).
#ifndef SOUNDLOST_H
#define SOUNDLOST_H

static bool SoundMedia(int src)   // the MCU's media sources (vh_sources.inc: 6, 7 - our waveOut heard)
{
    return src == 6 || src == 7;
}

// before / now - the MCU's source (-1: not known yet)
static bool SoundLost(int before, int now)
{
    return SoundMedia(before) && now >= 0 && !SoundMedia(now);
}

#endif
