// trackend.h - a track of our player ended: the next one only while the sound is ours (HU 2026-11-07: MODE to the
// radio, the next track took the media back, again and again - the radio never stayed).
#pragma once

// src: the MCU's source (-1 not known); switching: our own switch to the media still going (MediaSource)
bool NextTrackPlays(int src, bool switching);
