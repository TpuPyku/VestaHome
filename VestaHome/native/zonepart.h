// zonepart.h - a partial frame (RedrawArea) draws the whole frame clipped and keeps the full frame's buttons; their
// actions may change with what is drawn (the radio widget's arrows: the next preset - HU sheet 28 p. 35, the preset
// switched at the second tap only, the arrow still pointing at the one playing).
#pragma once
#include <windows.h>

// A partial frame's button at (x, y, w, h) against the full frame's at the same index (was; NULL - it had fewer):
// true = the same place (it takes the action drawn now), false = the layout is another - the whole frame is needed.
bool ZoneSame(const RECT* was, int x, int y, int w, int h);
