// trackend.cpp - see trackend.h.
#include "trackend.h"

bool NextTrackPlays(int src, bool switching)
{
    return switching || src < 0 || src == 6 || src == 7 || src == 9;   // the media, Bluetooth's path: ours sound
}
