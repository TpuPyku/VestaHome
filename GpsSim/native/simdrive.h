// simdrive.h - GpsSim's drive along a line (OSMapsCE's route, VestaCore vroute): once a second its point, speed, course
// and the steering wheel - as tools/make_route.py makes the built-in one, the road's speed taken as kSimTopKmh.
#ifndef SIMDRIVE_H
#define SIMDRIVE_H

#include "simnmea.h"

enum { kSimTopKmh = 60, kSimStand0 = 20, kSimStand1 = 10 };   // the top speed; standing at the start, at the end, s

// n points (latitude, longitude in millionths of a degree) -> out (at most cap): standing kSimStand0 s, the drive
// (up to kSimTopKmh, held down in bends: sideways 2.5 m/s2; 1.5 m/s2 up, 2.5 down), standing kSimStand1 s. Its
// length in seconds (0: fewer than 2 points).
int SimDrive(const int* lat, const int* lon, int n, SimPoint* out, int cap);

#endif
