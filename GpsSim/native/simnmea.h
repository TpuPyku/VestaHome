// simnmea.h - GpsSim's lines for a point of the drive: what the head unit's GPS port carries (the GPS receiver's
// NMEA and the CAN module's own "@" lines, VestaCore vgps.h), each with its checksum, CR LF after each.
#ifndef SIMNMEA_H
#define SIMNMEA_H

#include <windows.h>

struct SimPoint {
    int lat, lon;    // millionths of a degree (south, west: negative)
    int kmh10;       // tenths of km/h
    int course10;    // tenths of a degree, 0..3599
    int wheel10;     // the steering wheel, tenths of a degree (right positive)
};

// RMC, GGA (9 satellites), "@,<km/h>,1.19", "@ANG,<degrees>," for the point at the UTC time; their length (0: no
// room in cap).
int SimLines(const SimPoint& p, const SYSTEMTIME& utc, char* out, int cap);

// The point of a drive of n points, ms into it (round and round).
int SimAt(DWORD ms, int n);

#endif
