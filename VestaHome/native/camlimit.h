// camlimit.h - a speed camera's limit as speedcam.txt gives it (HU 2026-11-24: openspeedcam.net's red-light cameras
// carry 5 km/h and its cameras without a limit 160 - fillers, not limits; with 5 the camera beeped all the way).
// Under 10 or 160 and above: 0, unknown - as the PC map loader now writes them (OSMapsCE offline/speedcams.py limit),
// so the regions already on the cards read right too.
#pragma once

enum { kCamLimitMin = 10, kCamLimitMax = 160 };

inline int CamLimit(int speed)
{
    return speed >= kCamLimitMin && speed < kCamLimitMax ? speed : 0;
}
