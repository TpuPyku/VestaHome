// trackend.h - a track of our player ended: the next one only while the sound is ours (HU 2026-11-07: MODE to the
// radio, the next track took the media back, again and again - the radio never stayed).
#pragma once

// src: the MCU's source (-1 not known); switching: our own switch to the media still going (MediaSource)
bool NextTrackPlays(int src, bool switching);

// A removable volume taken out (lost: 1 the SD card, 2 the USB disk): the radio if the media is the source and our
// music or video plays from it (playing: the same bits; the user 2026-11-09, the USB disk too - HU 2026-11-11);
// not under the internet radio (no volume needed), not for one played from (the built-in card: 0).
bool CardOutRadio(int src, bool netOn, int lost, int playing);

// "Music" opened: our music goes on if the radio or Bluetooth paused it (OursQuiet; sheet 28 p. 1-2: from the menu it
// stayed paused, MODE resumed it); a pause by the user stays.
bool MusicBackOnTab(bool heldBySource, bool playing);

// A tab (screen) opened by a button: it was not on before (its own button again - not opened anew)
bool TabOpened(int was, int now, int tab);

// The MCU's number of a source switched by a key, learned on the unit (AUX: not in the shell's code, Ghidra
// 2026-11-10): the first change of the source (was -> now) within 6 s after the key (askedAgoMs), not to one of the
// numbers known (0 radio, 6 / 7 media, 9 Bluetooth: the user went on, or the MCU refused); otherwise the number known
// stays (-1 none).
int LearnSource(int known, unsigned askedAgoMs, int was, int now);

// The phone starts playing by itself while another source sounds: paused at once, as the stock shell does (the user
// 2026-11-11; sheet 29 p. 34: an SMS notification took the radio to Bluetooth) - not under Bluetooth (src 9), not
// within 5 s of our own switching (sinceOursMs: to Bluetooth, the source still to come), only under our host.
bool PhonePaused(int src, unsigned sinceOursMs, bool host);
