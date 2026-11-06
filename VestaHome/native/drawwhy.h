// drawwhy.h - who asked for the full frames: the load line of the log names the callers that cost the most
// (a full frame is 48 ms on the unit, a partial one 6-12 ms, sheet 24's log).
#ifndef DRAWWHY_H
#define DRAWWHY_H

#include <wchar.h>

// One more frame asked for at file:line (the file's folder is not kept).
void WhyCount(const char* file, int line);
// The most frequent three since the last call, "home.cpp:812 x60, vh_gps.inc:88 x12" (empty: none), and a new count.
void WhyTake(wchar_t* out, int cap);

#endif
