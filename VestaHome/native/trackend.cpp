// trackend.cpp - see trackend.h.
#include "trackend.h"

bool NextTrackPlays(int src, bool switching)
{
    return switching || src < 0 || src == 6 || src == 7 || src == 9;   // the media, Bluetooth's path: ours sound
}

bool CardOutRadio(int src, bool netOn, int lost, int playing)
{
    return (src == 6 || src == 7) && !netOn && (lost & playing) != 0;
}

int LearnSource(int known, unsigned askedAgoMs, int was, int now)
{
    bool other = now > 0 && now != 6 && now != 7 && now != 9;   // (0 radio, 6 / 7 media, 9 Bluetooth)
    return askedAgoMs < 6000 && other && now != was ? now : known;
}

bool SrcJustTurned(int src, int seen, bool ours, unsigned sinceTurnMs)
{
    return !ours && (src != seen || sinceTurnMs < 3000);
}

bool AuxOutOnSource(bool plugged, int was, int now, unsigned sinceAskMs)
{
    return plugged && was >= -1 && now >= 0 && was != now && sinceAskMs >= 6000;
}

bool AuxOutOnScreen(bool plugged, bool wasAuxPage, bool nowAudioPage)
{
    return plugged && wasAuxPage && nowAudioPage;
}

bool TabOpened(int was, int now, int tab)
{
    return now == tab && was != tab;
}

bool MusicBackOnTab(bool heldBySource, bool playing)
{
    return heldBySource && !playing;
}

bool PhonePaused(int src, unsigned sinceOursMs, unsigned playingMs, bool host)
{
    return src != 9 && sinceOursMs > 5000 && playingMs >= 3000 && host;
}
