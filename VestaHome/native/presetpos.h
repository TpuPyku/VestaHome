// presetpos.h - which preset plays: the MCU names only its place on a page of 8 (0x0907 00 01 <0..7>, every HU
// record 2026-10-16..27), FM's three pages are ours (HU 2026-11-11, sheet 29 p. 16: the arrows jumped FM1 <-> FM2).
#pragma once

// The preset playing, 0..23: the MCU's place (pos 0..7, -1 none) on the page of the preset we asked for last
// (sent, -1 none; the first page without one).
int PresetPlaying(int pos, int sent);

// The preset dir (+1 / -1) away on the same page of `per` presets (round); none playing (cur < 0): the page's first.
int PresetNext(int cur, int dir, int per);

// The presets the MCU changed: was / now = a message's n presets, the first one numbered from+1; one known before
// (was > 0) and now different counts. out: "3: 98.40 -> 104.70, ..." (cut to cap). The count.
int PresetChanges(const int* was, const int* now, int from, int n, wchar_t* out, int cap);
