// trackend.h - a track of our player ended: the next one only while the sound is ours (HU 2026-11-07: MODE to the
// radio, the next track took the media back, again and again - the radio never stayed).
#pragma once

// src: the MCU's source (-1 not known); switching: our own switch to the media still going (MediaSource)
bool NextTrackPlays(int src, bool switching);

// The SD card taken out: the radio if the media is the source (our music from the card can't go on; the user
// 2026-11-09), not under the internet radio (the card is not needed - the stub keeps the SD "in" for the MCU).
bool CardOutRadio(int src, bool netOn);

// "Music" opened: our music goes on if the radio or Bluetooth paused it (OursQuiet; sheet 28 p. 1-2: from the menu it
// stayed paused, MODE resumed it); a pause by the user stays.
bool MusicBackOnTab(bool heldBySource, bool playing);
