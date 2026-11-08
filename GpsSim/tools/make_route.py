#!/usr/bin/env python3
"""make_route.py - GpsSim's built-in drive (native/route.inc) from a real route (OSRM, FOSSGIS's server).

The drive, once a second: the position, the speed, the course and the steering wheel's angle. The speed: OSRM's
for each piece of the road, held down in bends (sideways 2.5 m/s2) and changed smoothly (1.5 m/s2 up, 2.5 down);
the wheel: from the bend at that speed (a 2.6 m car, the steering 16:1). It stands 30 s at the start (time to set a
route) and 20 s at the end; GpsSim then starts it again.

Usage: python VestaHome/GpsSim/tools/make_route.py      (writes VestaHome/GpsSim/native/route.inc)
"""
import json
import math
import os
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..', 'native', 'route.inc')
NAME = 'Москва: Ходынка - пл. Комарова - Ленинградка - МКАД - Дмитровское ш.'
POINTS = '37.5300,55.7900;37.5540,55.7942;37.5550,55.9480'   # (lon,lat; the second: the roundabout's own point)
URL = ('https://routing.openstreetmap.de/routed-car/route/v1/driving/%s'
       '?overview=full&geometries=geojson&annotations=distance,duration&steps=false' % POINTS)
STAND0, STAND1 = 30, 20
SIDE, UP, DOWN = 2.5, 1.5, 2.5          # m/s2
BASE, RATIO = 2.6, 16.0                 # the car's wheelbase, m; the steering's ratio


def metres(a, b):
    k = math.cos(math.radians((a[1] + b[1]) / 2))
    return math.hypot((b[0] - a[0]) * k, b[1] - a[1]) * 111320


def bearing(a, b):
    k = math.cos(math.radians(a[1]))
    return (math.degrees(math.atan2((b[0] - a[0]) * k, b[1] - a[1])) + 360) % 360


def turn(c0, c1):   # -180..180, right positive
    return (c1 - c0 + 540) % 360 - 180


def road():
    req = urllib.request.Request(URL, headers={'User-Agent': 'VestaHome-GpsSim'})
    r = json.load(urllib.request.urlopen(req, timeout=60))['routes'][0]
    pts, speeds = [], []
    for leg in r['legs']:
        a = leg['annotation']
        speeds += [d / t if t > 0 else 10 for d, t in zip(a['distance'], a['duration'])]
    pts = r['geometry']['coordinates']
    pts = [p for i, p in enumerate(pts) if i == 0 or metres(pts[i - 1], p) > 0.5]   # (legs' joints: one point)
    n = len(pts) - 1
    speeds = (speeds + [speeds[-1]] * n)[:n]
    return pts, speeds, r['distance']


def limits(pts, speeds):   # each piece's top speed: OSRM's, and the bend's
    v = list(speeds)
    for i in range(1, len(pts) - 1):
        d = (metres(pts[i - 1], pts[i]) + metres(pts[i], pts[i + 1])) / 2
        a = abs(math.radians(turn(bearing(pts[i - 1], pts[i]), bearing(pts[i], pts[i + 1]))))
        if a > 0.02:
            vb = math.sqrt(SIDE * max(d, 1) / a)
            v[i - 1] = min(v[i - 1], vb)
            v[i] = min(v[i], vb)
    return [max(x, 3.0) for x in v]


def drive(pts, vmax):   # once a second: (lon, lat, m/s, course) - smooth speed along the pieces
    cum = [0.0]
    for i in range(len(pts) - 1):
        cum.append(cum[-1] + metres(pts[i], pts[i + 1]))
    # the speed's limit by distance, smoothed backwards (braking) on a 5 m grid
    step = 5.0
    grid = int(cum[-1] / step) + 2
    lim = [0.0] * grid
    j = 0
    for g in range(grid):
        s = g * step
        while j < len(cum) - 2 and cum[j + 1] < s:
            j += 1
        lim[g] = vmax[j]
    lim[-1] = 0
    for g in range(grid - 2, -1, -1):
        lim[g] = min(lim[g], math.sqrt(lim[g + 1] ** 2 + 2 * DOWN * step))
    out, s, v = [], 0.0, 0.0
    j = 0
    while s < cum[-1]:
        while j < len(cum) - 2 and cum[j + 1] < s:
            j += 1
        f = (s - cum[j]) / max(cum[j + 1] - cum[j], 1e-6)
        a, b = pts[j], pts[j + 1]
        out.append((a[0] + (b[0] - a[0]) * f, a[1] + (b[1] - a[1]) * f, v, bearing(a, b)))
        top = lim[min(int(s / step), grid - 1)]
        v = min(v + UP, top) if v < top else max(v - DOWN, top)
        s += max(v, 0.5)
    return out


def wheel(v, dcourse):   # the steering wheel's angle, degrees (right positive)
    if v < 1:
        return 0.0
    w = math.radians(dcourse)   # per second
    return max(-540.0, min(540.0, RATIO * math.degrees(math.atan(BASE * w / v))))


def main():
    pts, speeds, dist = road()
    d = drive(pts, limits(pts, speeds))
    rows = [(d[0][0], d[0][1], 0.0, d[0][3], 0.0)] * STAND0
    for i, (lo, la, v, c) in enumerate(d):
        nc = d[i + 1][3] if i + 1 < len(d) else c
        rows.append((lo, la, v, c, wheel(v, turn(c, nc))))
    rows += [(d[-1][0], d[-1][1], 0.0, d[-1][3], 0.0)] * STAND1
    with open(OUT, 'w', encoding='utf-8-sig', newline='\r\n') as f:   # (the BOM: VS2005 reads the name right)
        f.write('// route.inc - GpsSim\'s drive, made by tools/make_route.py (do not edit): %.1f km, %d s.\n'
                '// Each second: latitude, longitude (millionths of a degree), speed (tenths of km/h), course and the\n'
                '// steering wheel (tenths of a degree).\n' % (dist / 1000, len(rows)))
        f.write('static const wchar_t kRouteName[] = L"%s";\n' % NAME)
        f.write('static const SimPoint kRoute[] = {\n')
        for lo, la, v, c, w in rows:
            f.write('    { %d, %d, %d, %d, %d },\n' % (round(la * 1e6), round(lo * 1e6), round(v * 36), round(c * 10) % 3600,
                                                    round(w * 10)))
        f.write('};\n')
    print('%s: %.1f km, %d s, top %.0f km/h' % (OUT, dist / 1000, len(rows), max(r[2] for r in rows) * 3.6))


if __name__ == '__main__':
    main()
