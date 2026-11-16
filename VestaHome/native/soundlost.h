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

// VestaHome's beep (a speed camera, a trouble code's glass) and the MCU's source: on the media source our own output is
// what is heard - the beep there (device 0, mixed with the music), no navigation voice (the user 2026-11-16: on the media
// the beep on the voice's device was not heard); elsewhere over the source: the navigation voice on the beep's device
// (dev -1: WAVE_MAPPER)
struct BeepWay { bool navi; int dev; };

static BeepWay BeepRoute(int src, int beepDev)
{
    BeepWay w;
    w.navi = !SoundMedia(src);
    w.dev = w.navi ? beepDev : 0;
    return w;
}

#endif
