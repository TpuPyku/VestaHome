#!/usr/bin/env python3
"""make_osm_v3_mockup.py - OSMapsCE design v3 mockups -> osm-skin-v3.html + .pdf, screens in osm_v3/*.png.

The user's brief (2026-10-26): OSMapsCE on VestaHome's design, looking at Škoda's Red Dot winning design system
(Škoda Flow, 2021: the map over the whole screen, floating rounded cards, one big number per card, one accent,
air), with vector maps in mind. So the map here is drawn from real vector tiles (OpenFreeMap, OpenMapTiles schema,
Samara - the tiles of VTileTest: OSMapsCE/native_vtest/get_tiles.py) in two own palettes, day and night, the way
a vector OSMapsCE would draw it: the street names stay upright while the map turns with the course. The route is
a real OSRM one (osm_v3/route.json); its traffic colours are made up for the picture.

    python VestaHome/skin/make_osm_v3_mockup.py
"""
import glob
import json
import math
import os
import subprocess

from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
OUT = os.path.join(HERE, 'osm_v3')
TILES = os.path.join(ROOT, 'OSMapsCE', 'VTileTest', 'tiles', '14')
FONTS = os.path.join(ROOT, 'VestaHome', 'VestaHome', 'VestaHome', 'Fonts')
W, H, K = 800, 480, 2          # the screen; everything is drawn K times larger and scaled down (smooth edges)

# VestaHome's palette (skin.ini, make_maps_mockup.py). The accent - and the car's glow - is VestaHome's theme
# (Settings -> Screen -> Theme: a Lada Vesta paint, screen.accent of VestaHome.ini; VestaHome's kAccents), the
# user's rule 2026-10-26: OSMapsCE takes it from there.
INK, PANEL, PANEL2, LINE = (0x0e, 0x10, 0x13), (0x23, 0x27, 0x2e), (0x2c, 0x31, 0x39), (0x2a, 0x2f, 0x37)
TEXT, MUTED, DIM = (0xf5, 0xf3, 0xee), (0xaa, 0xb0, 0xb8), (0x6c, 0x72, 0x7c)
# (the user 2026-10-26: the cards in VestaHome's tile colour 23272E, solid; the controls on them a step lighter)
ACCENTS = {130: ('Марс', (0xff, 0x6b, 0x1a)), 195: ('Сердолик', (0xdc, 0x3c, 0x3c)), 247: ('Карфаген', (0xc3, 0xb8, 0x9c)),
           372: ('Криптон', (0x72, 0xc0, 0x59)), 476: ('Дайвинг', (0x4f, 0x7a, 0xca)), 619: ('Булат', (0xab, 0xba, 0xd8)),
           691: ('Платина', (0xee, 0xee, 0xee))}
ACCENT = ACCENTS[130][1]


def theme_accent():
    """The accent of the PC build's VestaHome.ini (what the user has chosen), Марс without one."""
    ini = os.path.join(ROOT, 'VestaHome', 'VestaHome', 'VestaHome', 'VestaHome.ini')
    try:
        for line in open(ini, encoding='utf-8', errors='replace'):
            if line.startswith('screen.accent='):
                return ACCENTS.get(int(line.split('=')[1]), ACCENTS[130])
    except (OSError, ValueError):
        pass
    return ACCENTS[130]
FREE, SLOW, JAM, STAND = (40, 190, 70), (255, 196, 0), (232, 40, 30), (140, 16, 16)
ROUTE, ROUTE_EDGE = (61, 139, 255), (18, 52, 128)

# the vector map's two palettes: areas, roads (fill, casing), labels (text, halo)
DAY = dict(bg=(243, 240, 235), resid=(236, 232, 226), industrial=(234, 228, 232), grass=(216, 234, 202),
           wood=(198, 222, 186), park=(206, 232, 196), water=(168, 204, 226), building=(224, 217, 208),
           minor=((255, 255, 255), (214, 208, 200)), secondary=((255, 247, 214), (222, 204, 160)),
           primary=((255, 228, 176), (224, 182, 120)), major=((255, 206, 150), (214, 150, 90)),
           rail=(184, 180, 174), path=(226, 220, 212), label=((66, 70, 76), (255, 255, 255)),
           place=((40, 44, 50), (255, 255, 255)))
NIGHT = dict(bg=(17, 19, 23), resid=(22, 25, 30), industrial=(25, 25, 31), grass=(20, 31, 25),
             wood=(18, 33, 25), park=(21, 37, 29), water=(13, 34, 50), building=(31, 35, 42),
             minor=((44, 50, 58), None), secondary=((62, 70, 80), None), primary=((84, 80, 68), None),
             major=((112, 88, 58), None), rail=(48, 52, 58), path=(32, 36, 42), label=((150, 156, 166), (11, 12, 14)),
             place=((205, 208, 214), (11, 12, 14)))


def font(size, bold=False):
    return ImageFont.truetype(os.path.join(FONTS, 'GolosText-Bold.ttf' if bold else 'GolosText-Regular.ttf'), size * K)


# ---------------------------------------------------------------- vector tiles (Mapbox Vector Tile, protobuf)
def varint(b, i):
    v = s = 0
    while True:
        c = b[i]
        i += 1
        v |= (c & 0x7f) << s
        s += 7
        if c < 0x80:
            return v, i


def fields(b):
    i = 0
    while i < len(b):
        tag, i = varint(b, i)
        f, w = tag >> 3, tag & 7
        if w == 0:
            v, i = varint(b, i)
        elif w == 2:
            n, i = varint(b, i)
            v, i = b[i:i + n], i + n
        elif w == 1:
            v, i = b[i:i + 8], i + 8
        elif w == 5:
            v, i = b[i:i + 4], i + 4
        else:
            raise ValueError(w)
        yield f, v


def packed(b):
    out, i = [], 0
    while i < len(b):
        v, i = varint(b, i)
        out.append(v)
    return out


def geometry(cmds):
    parts, cur, x, y, i = [], None, 0, 0, 0
    while i < len(cmds):
        c, n = cmds[i] & 7, cmds[i] >> 3
        i += 1
        if c == 7:
            continue
        for _ in range(n):
            dx, dy = cmds[i], cmds[i + 1]
            i += 2
            x += (dx >> 1) ^ -(dx & 1)
            y += (dy >> 1) ^ -(dy & 1)
            if c == 1:
                cur = []
                parts.append(cur)
            cur.append((x, y))
    return parts


def load_tiles():
    feats = []   # (layer, props, geomtype, parts in world units of z14 * 4096)
    for path in glob.glob(os.path.join(TILES, '*.pbf')):
        tx, ty = map(int, os.path.basename(path)[:-4].split('_'))
        data = open(path, 'rb').read()
        for f, layer in fields(data):
            if f != 3:
                continue
            name, keys, vals, raw = '', [], [], []
            for lf, lv in fields(layer):
                if lf == 1:
                    name = lv.decode()
                elif lf == 3:
                    keys.append(lv.decode())
                elif lf == 4:
                    v = None
                    for vf, vv in fields(lv):
                        v = vv.decode() if vf == 1 else vv
                    vals.append(v)
                elif lf == 2:
                    raw.append(lv)
            if name not in ('water', 'landcover', 'landuse', 'park', 'building', 'transportation',
                            'transportation_name', 'place', 'waterway'):
                continue
            for fb in raw:
                tags, gtype, geom = [], 0, []
                for ff, fv in fields(fb):
                    if ff == 2:
                        tags = packed(fv)
                    elif ff == 3:
                        gtype = fv
                    elif ff == 4:
                        geom = packed(fv)
                props = {keys[tags[i]]: vals[tags[i + 1]] for i in range(0, len(tags) - 1, 2)}
                parts = [[(tx * 4096 + px, ty * 4096 + py) for px, py in p] for p in geometry(geom)]
                feats.append((name, props, gtype, parts))
    return feats


FEATS = None


def lonlat_to_world(lon, lat):   # z14 tile units (4096 per tile)
    n = 2 ** 14 * 4096
    x = (lon + 180) / 360 * n
    la = math.radians(lat)
    y = (1 - math.log(math.tan(la) + 1 / math.cos(la)) / math.pi) / 2 * n
    return x, y


class View:
    """The map on the screen: center (lon, lat), zoom, the course up (degrees), the screen point of the center."""

    def __init__(self, lon, lat, zoom, angle=0.0, sx=W / 2, sy=H / 2):
        self.cx, self.cy = lonlat_to_world(lon, lat)
        self.s = 256 * 2 ** (zoom - 14) / 4096       # z14 tile unit -> screen px
        self.a = math.radians(angle)
        self.sx, self.sy = sx, sy
        self.zoom = zoom

    def pt(self, wx, wy):   # -> canvas (K x) pixels
        dx, dy = (wx - self.cx) * self.s, (wy - self.cy) * self.s
        c, s = math.cos(self.a), math.sin(self.a)
        return ((self.sx + dx * c + dy * s) * K, (self.sy - dx * s + dy * c) * K)

    def ll(self, lon, lat):
        return self.pt(*lonlat_to_world(lon, lat))


def fit(coords, box, margin=40):
    """A north-up view showing all the points inside box (x0, y0, x1, y1) of the screen."""
    ws = [lonlat_to_world(lon, lat) for lon, lat in coords]
    xs, ys = [w[0] for w in ws], [w[1] for w in ws]
    bw, bh = box[2] - box[0] - 2 * margin, box[3] - box[1] - 2 * margin
    s = min(bw / max(1, max(xs) - min(xs)), bh / max(1, max(ys) - min(ys)))   # px per z14 unit
    zoom = 14 + math.log2(s * 4096 / 256)
    cxw, cyw = (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2
    n = 2 ** 14 * 4096
    lon = cxw / n * 360 - 180
    lat = math.degrees(math.atan(math.sinh(math.pi * (1 - 2 * cyw / n))))
    return View(lon, lat, zoom, 0, (box[0] + box[2]) / 2, (box[1] + box[3]) / 2)


def area(p):
    return sum(p[i][0] * p[i + 1][1] - p[i + 1][0] * p[i][1] for i in range(len(p) - 1))


ROAD = {'motorway': 'major', 'trunk': 'major', 'primary': 'primary', 'secondary': 'secondary',
        'tertiary': 'secondary', 'minor': 'minor', 'service': 'minor'}
WIDTH = {'minor': 6, 'secondary': 9, 'primary': 11, 'major': 13}


def draw_map(view, pal, traffic=False):
    global HAIR
    HAIR = pal is NIGHT
    im = Image.new('RGB', (W * K, H * K), pal['bg'])
    d = ImageDraw.Draw(im)
    areas = [('landuse', ('residential', 'suburb', 'neighbourhood'), 'resid'),
             ('landuse', ('industrial', 'commercial', 'retail', 'railway'), 'industrial'),
             ('landcover', ('grass', 'farmland', 'meadow', 'scrub'), 'grass'),
             ('landcover', ('wood', 'forest'), 'wood'), ('park', None, 'park'), ('water', None, 'water')]
    for layer, classes, key in areas:
        for name, props, gt, parts in FEATS:
            if name != layer or gt != 3 or (classes and props.get('class') not in classes):
                continue
            for p in parts:
                pts = [view.pt(*q) for q in p]
                if len(pts) > 2:
                    d.polygon(pts, fill=pal[key] if area(p) > 0 else pal['bg'])
    if view.zoom >= 15:
        for name, props, gt, parts in FEATS:
            if name == 'building' and gt == 3:
                for p in parts:
                    if area(p) > 0 and len(p) > 2:
                        d.polygon([view.pt(*q) for q in p], fill=pal['building'])
    for name, props, gt, parts in FEATS:
        if name == 'waterway' and gt == 2:
            for p in parts:
                d.line([view.pt(*q) for q in p], fill=pal['water'], width=3 * K)
    scale = 1.0 if view.zoom >= 16 else 0.75 if view.zoom >= 15 else 0.5
    roads = [(f, ROAD.get(f[1].get('class'))) for f in FEATS if f[0] == 'transportation' and f[2] == 2]
    for name, props, gt, parts in FEATS:   # rails and paths under the roads
        if name == 'transportation' and gt == 2 and props.get('class') in ('rail', 'transit'):
            for p in parts:
                d.line([view.pt(*q) for q in p], fill=pal['rail'], width=int(2 * K))
        elif name == 'transportation' and gt == 2 and props.get('class') in ('path', 'track') and view.zoom >= 16:
            for p in parts:
                d.line([view.pt(*q) for q in p], fill=pal['path'], width=int(1.5 * K))
    for pass_ in (0, 1):
        for kind in ('minor', 'secondary', 'primary', 'major'):
            fill, case = pal[kind]
            w = WIDTH[kind] * scale * K
            if pass_ == 0 and not case:
                continue
            for (name, props, gt, parts), k in roads:
                if k != kind:
                    continue
                for p in parts:
                    pts = [view.pt(*q) for q in p]
                    d.line(pts, fill=case if pass_ == 0 else fill, width=int(w + (2 * K if pass_ == 0 else 0)),
                           joint='curve')
    if traffic:   # the traffic, thin along the bigger roads (made up: mostly free)
        for (name, props, gt, parts), k in roads:
            if k not in ('primary', 'major', 'secondary'):
                continue
            for p in parts:
                h = hash((p[0], len(p))) % 20
                c = FREE if h < 13 else SLOW if h < 17 else JAM
                d.line([view.pt(*q) for q in p], fill=c, width=int(3 * K * scale + K), joint='curve')
    return im


def labels(im, view, pal, avoid=()):
    """Street and place names: upright whatever the map's turn (the vector map's advantage)."""
    d = ImageDraw.Draw(im)
    taken = [tuple(v * K for v in a) for a in avoid]
    fs = font(11 if view.zoom >= 15 else 10)

    def free(box):
        for b in taken:
            if box[0] < b[2] and box[2] > b[0] and box[1] < b[3] and box[3] > b[1]:
                return False
        return True

    for name, props, gt, parts in FEATS:   # places first
        if name == 'place' and props.get('name') and props.get('class') in ('city', 'suburb', 'quarter'):
            x, y = view.pt(*parts[0][0])
            t = props['name']
            f = font(13, True) if props.get('class') == 'city' else font(11, True)
            w = d.textlength(t, font=f)
            box = (x - w / 2 - 4 * K, y - 10 * K, x + w / 2 + 4 * K, y + 10 * K)
            if 0 < x < W * K and 0 < y < H * K and free(box):
                d.text((x, y), t, font=f, fill=pal['place'][0], anchor='mm', stroke_width=2 * K,
                       stroke_fill=pal['place'][1])
                taken.append(box)
    seen = set()
    for name, props, gt, parts in sorted(FEATS, key=lambda f: -len(f[3][0]) if f[3] else 0):
        if name != 'transportation_name' or gt != 2 or not props.get('name') or props['name'] in seen:
            continue
        best = None
        for p in parts:
            for i in range(len(p) - 1):
                a, b = view.pt(*p[i]), view.pt(*p[i + 1])
                L = math.hypot(b[0] - a[0], b[1] - a[1])
                if (not best or L > best[0]) and 40 * K < (a[0] + b[0]) / 2 < (W - 40) * K and \
                        60 * K < (a[1] + b[1]) / 2 < (H - 30) * K:
                    best = (L, a, b)
        t = props['name']
        if not best or best[0] < d.textlength(t, font=fs) + 10 * K:
            continue
        L, a, b = best
        ang = math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))
        if ang > 90:
            ang -= 180
        elif ang < -90:
            ang += 180
        mx, my = (a[0] + b[0]) / 2, (a[1] + b[1]) / 2
        w = d.textlength(t, font=fs)
        r = max(w, 14 * K) / 2 + 4 * K
        box = (mx - r, my - r * abs(math.sin(math.radians(ang))) - 8 * K, mx + r, my + r * abs(math.sin(math.radians(ang))) + 8 * K)
        if not free(box):
            continue
        tile = Image.new('RGBA', (int(w + 12 * K), int(24 * K)), (0, 0, 0, 0))
        ImageDraw.Draw(tile).text((tile.width / 2, tile.height / 2), t, font=fs, fill=pal['label'][0] + (255,),
                                  anchor='mm', stroke_width=2 * K, stroke_fill=pal['label'][1] + (255,))
        rt = tile.rotate(-ang, resample=Image.BICUBIC, expand=True)
        im.paste(rt, (int(mx - rt.width / 2), int(my - rt.height / 2)), rt)
        taken.append(box)
        seen.add(t)


# ---------------------------------------------------------------- the route, markers
RT = None


def route_pts(view, k=0):
    return [view.ll(lon, lat) for lon, lat in RT['routes'][k]['coords']]


def draw_route(im, view, k=0, traffic=True, grey=False, start=0):
    d = ImageDraw.Draw(im)
    pts = route_pts(view, k)[start:]
    if grey:
        d.line(pts, fill=(70, 76, 86), width=10 * K, joint='curve')
        d.line(pts, fill=(160, 170, 186), width=6 * K, joint='curve')
        return
    d.line(pts, fill=ROUTE_EDGE, width=12 * K, joint='curve')
    n = len(pts)
    for i in range(n - 1):   # the route coloured by the traffic on it (made up for the picture)
        f = i / n
        c = ROUTE if not traffic else FREE if f < 0.12 else SLOW if f < 0.2 else JAM if f < 0.27 else FREE
        d.line(pts[i:i + 2], fill=c, width=8 * K)
        d.ellipse([pts[i][0] - 4 * K, pts[i][1] - 4 * K, pts[i][0] + 4 * K, pts[i][1] + 4 * K], fill=c)


def marker(im, x, y, fill, text=None, r=13):
    d = ImageDraw.Draw(im)
    d.ellipse([x - (r + 3) * K, y - (r + 3) * K, x + (r + 3) * K, y + (r + 3) * K], fill=(255, 255, 255))
    d.ellipse([x - r * K, y - r * K, x + r * K, y + r * K], fill=fill)
    if text:
        d.text((x, y), text, font=font(12, True), fill=(255, 255, 255), anchor='mm')


def car(im, x, y, ang=0.0):
    d = ImageDraw.Draw(im)
    a = math.radians(ang)
    shape = [(0, -19), (13, 14), (0, 7), (-13, 14)]
    pts = [(x * K + (px * math.cos(a) - py * math.sin(a)) * K, y * K + (px * math.sin(a) + py * math.cos(a)) * K)
           for px, py in shape]
    halo = Image.new('L', im.size, 0)
    ImageDraw.Draw(halo).ellipse([x * K - 30 * K, y * K - 30 * K, x * K + 30 * K, y * K + 30 * K], fill=70)
    halo = halo.filter(ImageFilter.GaussianBlur(10 * K))
    im.paste(Image.new('RGB', im.size, ACCENT), (0, 0), halo)
    d.polygon(pts, fill=ACCENT, outline=(255, 255, 255), width=3 * K)


# ---------------------------------------------------------------- cards (Škoda Flow: floating, rounded, air)
HAIR = False   # the night map: a floating card gets its 1 px edge (the shadow does not show on the dark map)


def card(im, box, fill=PANEL, alpha=255, radius=22, border=None, shadow=True):
    """A card (the rule 2026-10-26, the user: as modern designs - no borders on buttons and cards; a chosen thing
    gets the accent's 2 px (border=ACCENT); a card floating over the night map - a hairline edge)."""
    x0, y0, x1, y1 = [v * K for v in box]
    if shadow:
        sh = Image.new('L', im.size, 0)
        ImageDraw.Draw(sh).rounded_rectangle([x0, y0 + 6 * K, x1, y1 + 6 * K], radius * K, fill=120)
        sh = sh.filter(ImageFilter.GaussianBlur(10 * K))
        im.paste(Image.new('RGB', im.size, (0, 0, 0)), (0, 0), sh)
    m = Image.new('L', im.size, 0)
    ImageDraw.Draw(m).rounded_rectangle([x0, y0, x1, y1], radius * K, fill=alpha)
    im.paste(Image.new('RGB', im.size, fill), (0, 0), m)
    if border:
        ImageDraw.Draw(im).rounded_rectangle([x0, y0, x1, y1], radius * K, outline=border, width=2 * K)
    elif HAIR and shadow:
        ImageDraw.Draw(im).rounded_rectangle([x0, y0, x1, y1], radius * K, outline=(64, 68, 77), width=K)


def text(im, xy, t, size, color=TEXT, bold=False, anchor='la'):
    ImageDraw.Draw(im).text((xy[0] * K, xy[1] * K), t, font=font(size, bold), fill=color, anchor=anchor)


def pill(im, box, t, fill, color, size=15, bold=True):
    x0, y0, x1, y1 = [v * K for v in box]
    d = ImageDraw.Draw(im)
    d.rounded_rectangle([x0, y0, x1, y1], (y1 - y0) / 2, fill=fill)
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    if t == 'x':   # a cross
        r = 6 * K
        d.line([(cx - r, cy - r), (cx + r, cy + r)], fill=color, width=2 * K)
        d.line([(cx - r, cy + r), (cx + r, cy - r)], fill=color, width=2 * K)
    else:
        text(im, (cx / K, cy / K), t, size, color, bold, 'mm')


def turn_arrow(im, cx, cy, kind, size=1.0, color=TEXT):
    d = ImageDraw.Draw(im)
    s = size * K
    w = max(2, int(9 * s))
    P = lambda dx, dy: (cx * K + dx * s, cy * K + dy * s)
    if kind == 'right':
        stem, ang = [P(0, 28), P(0, 2), P(18, -16)], -45
    elif kind == 'left':
        stem, ang = [P(0, 28), P(0, 2), P(-18, -16)], 45
    else:
        stem, ang = [P(0, 28), P(0, -14)], 0
    tip = stem[-1]
    d.line(stem, fill=color, width=w, joint='curve')
    for p in stem:
        d.ellipse([p[0] - w / 2, p[1] - w / 2, p[0] + w / 2, p[1] + w / 2], fill=color)
    a = math.radians(ang)
    head = [(0, -16), (13, 4), (-13, 4)]
    d.polygon([(tip[0] + (x * math.cos(a) + y * math.sin(a)) * s, tip[1] + (-x * math.sin(a) + y * math.cos(a)) * s)
               for x, y in head], fill=color)


def round_btn(im, cx, cy, sign, r=26, on=False):
    x0, y0 = cx - r, cy - r
    card(im, (x0, y0, cx + r, cy + r), fill=PANEL, alpha=230, radius=r, shadow=True)
    d = ImageDraw.Draw(im)
    c = ACCENT if on else TEXT
    if sign == '+':
        d.line([((cx - 9) * K, cy * K), ((cx + 9) * K, cy * K)], fill=c, width=3 * K)
        d.line([(cx * K, (cy - 9) * K), (cx * K, (cy + 9) * K)], fill=c, width=3 * K)
    elif sign == '-':
        d.line([((cx - 9) * K, cy * K), ((cx + 9) * K, cy * K)], fill=c, width=3 * K)
    elif sign == 'compass':   # the north arrow: red north half
        d.polygon([(cx * K, (cy - 13) * K), ((cx + 6) * K, cy * K), ((cx - 6) * K, cy * K)], fill=(232, 60, 50))
        d.polygon([(cx * K, (cy + 13) * K), ((cx + 6) * K, cy * K), ((cx - 6) * K, cy * K)], fill=MUTED)
    elif sign == 'gps':
        d.polygon([(cx * K, (cy - 11) * K), ((cx + 8) * K, (cy + 9) * K), (cx * K, (cy + 4) * K),
                   ((cx - 8) * K, (cy + 9) * K)], fill=c)
    elif sign == 'layers':
        for dy, f in ((5, None), (0, PANEL)):
            q = [(cx * K, (cy - 8 + dy) * K), ((cx + 11) * K, (cy - 2 + dy) * K), (cx * K, (cy + 4 + dy) * K),
                 ((cx - 11) * K, (cy - 2 + dy) * K)]
            d.polygon(q, fill=f, outline=c, width=2 * K)
    elif sign == 'sound':
        d.polygon([((cx - 9) * K, (cy - 4) * K), ((cx - 4) * K, (cy - 4) * K), ((cx + 3) * K, (cy - 10) * K),
                   ((cx + 3) * K, (cy + 10) * K), ((cx - 4) * K, (cy + 4) * K), ((cx - 9) * K, (cy + 4) * K)], fill=c)
        d.arc([(cx + 1) * K, (cy - 7) * K, (cx + 11) * K, (cy + 7) * K], -50, 50, fill=c, width=2 * K)


def status(im, night=False, route=False):
    """The top: no bar - a soft shade over the map, the clock and the chips (as VestaHome's top line)."""
    sh = Image.new('L', im.size, 0)
    sd = ImageDraw.Draw(sh)
    for y in range(56 * K):
        sd.line([(0, y), (W * K, y)], fill=int(170 * (1 - y / (56 * K)) ** 1.5))
    im.paste(Image.new('RGB', im.size, INK), (0, 0), sh)
    text(im, (W - 18, 22), '14:32', 24, TEXT, True, 'rm')
    pill(im, (16, 10, 120, 34), 'GPS · 9', (20, 60, 34), (140, 230, 160), 12)
    pill(im, (128, 10, 252, 34), 'Пробки · Яндекс', PANEL2, MUTED, 12, False)


def speed(im, x, y, kmh, limit=None):
    card(im, (x, y, x + 76, y + 76), radius=38)
    text(im, (x + 38, y + 33), str(kmh), 28, TEXT, True, 'mm')
    text(im, (x + 38, y + 56), 'км/ч', 11, MUTED, False, 'mm')
    if limit:
        d = ImageDraw.Draw(im)
        lx, ly, r = (x + 76) * K, (y + 6) * K, 21 * K
        d.ellipse([lx - r, ly - r, lx + r, ly + r], fill=(255, 255, 255))
        d.ellipse([lx - r + 4 * K, ly - r + 4 * K, lx + r - 4 * K, ly + r - 4 * K], outline=(214, 30, 30), width=5 * K)
        text(im, (x + 76, y + 6), str(limit), 15, (10, 10, 10), True, 'mm')


def finish(im, name):
    im = im.resize((W, H), Image.LANCZOS)
    im.save(os.path.join(OUT, name + '.png'))
    return name + '.png'


# ---------------------------------------------------------------- screens
def course_at(view_pts_ll, i):
    (a, b), (c, d) = view_pts_ll[i], view_pts_ll[min(i + 4, len(view_pts_ll) - 1)]
    kx = math.cos(math.radians(b))
    return math.degrees(math.atan2((c - a) * kx, d - b)) % 360


def screen_drive(night):
    pal = NIGHT if night else DAY
    coords = RT['routes'][0]['coords']
    ci = 34
    lon, lat = coords[ci]
    crs = course_at(coords, ci)
    view = View(lon, lat, 16.3, crs, W / 2 + 40, H * 0.70)   # the car low and a little right: the card is left
    im = draw_map(view, pal, traffic=False)
    draw_route(im, view, 0, traffic=True, start=ci)
    labels(im, view, pal, avoid=[(16, 56, 266 if not night else 182, 222 if not night else 112), (16, 392, 784, 470),
                                 (700, 150, 790, 400)])
    x, y = view.ll(*coords[-1])
    marker(im, x, y, ROUTE_EDGE)
    car(im, W / 2 + 40, H * 0.70)
    status(im, night, True)
    if not night:   # before the turn (300 m / 20 s): the big card, 250 px wide
        card(im, (16, 56, 266, 222))
        ImageDraw.Draw(im).rounded_rectangle([28 * K, 68 * K, 96 * K, 136 * K], 16 * K, fill=ACCENT)
        turn_arrow(im, 62, 98, 'right', 0.9, INK)
        text(im, (108, 72), '250', 40, TEXT, True)
        text(im, (108 + ImageDraw.Draw(im).textlength('250', font=font(40, True)) / K + 5, 90), 'м', 20, MUTED, True)
        text(im, (108, 118), 'направо', 14, MUTED)
        text(im, (28, 150), 'ул. Льва Толстого', 19, TEXT, True)
        ImageDraw.Draw(im).line([(28 * K, 188 * K), (254 * K, 188 * K)], fill=LINE, width=K)
        text(im, (28, 196), 'затем — налево: Ленинская', 12, MUTED)
    else:   # far from it: the compact one, a quarter of the big
        card(im, (16, 56, 182, 112), radius=18)
        ImageDraw.Draw(im).rounded_rectangle([22 * K, 62 * K, 66 * K, 106 * K], 12 * K, fill=ACCENT)
        turn_arrow(im, 44, 82, 'right', 0.55, INK)
        text(im, (78, 84), '1,2 км', 21, TEXT, True, 'lm')
    # the trip: arrival, minutes, km - and the end
    card(im, (196, 400, 604, 466), radius=33)
    for i, (big, small) in enumerate((('14:41', 'прибытие'), ('9 мин', 'с пробками 9–12'), ('4,1 км', 'осталось'))):
        cx = 262 + i * 122
        text(im, (cx, 424), big, 21, TEXT, True, 'mm')
        text(im, (cx, 447), small, 11, MUTED, False, 'mm')
    pill(im, (548, 414, 592, 452), 'x', PANEL2, TEXT, 16)
    speed(im, 20, 392, 47, 60)
    for i, s in enumerate(('+', '-', 'compass', 'sound')):
        round_btn(im, 754, 196 + i * 62, s)
    return finish(im, 'drive_night' if night else 'drive_day')


def screen_browse():
    pal = DAY
    view = View(50.112, 53.1965, 15.2, 0)
    im = draw_map(view, pal, traffic=True)
    labels(im, view, pal, avoid=[(16, 56, 380, 190), (0, 404, 800, 480), (700, 150, 790, 400)])
    car(im, 470, 300, 30)
    status(im)
    card(im, (16, 56, 380, 190))
    card(im, (28, 68, 368, 112), fill=PANEL2, alpha=255, radius=22, shadow=False)
    d = ImageDraw.Draw(im)
    d.ellipse([44 * K, 80 * K, 60 * K, 96 * K], outline=MUTED, width=2 * K)
    d.line([(58 * K, 94 * K), (64 * K, 100 * K)], fill=MUTED, width=2 * K)
    text(im, (76, 90), 'Куда едем?', 17, MUTED, False, 'lm')
    for i, (t, sub) in enumerate((('Дом', '12 мин'), ('Работа', '25 мин'), ('Мичурина, 64', 'недавно'))):
        x = 28 + i * 116
        card(im, (x, 122, x + 108, 178), fill=PANEL2, alpha=255, radius=16, shadow=False, border=None)
        text(im, (x + 12, 130), t, 13, TEXT, True)
        text(im, (x + 12, 152), sub, 11, MUTED)
    # VestaHome's bottom bar (a tap on the map), with search and the route in the middle places
    sh = Image.new('L', im.size, 0)
    ImageDraw.Draw(sh).rectangle([0, 404 * K, W * K, H * K], fill=235)
    im.paste(Image.new('RGB', im.size, INK), (0, 0), sh)
    ImageDraw.Draw(im).line([(0, 404 * K), (W * K, 404 * K)], fill=LINE, width=K)
    imgs = os.path.join(ROOT, 'OSMapsCE', 'OSMapsCE', 'Images')
    for i, (cap, sign, on) in enumerate((('ДОМОЙ', 'Bhome', False), ('ПРОБКИ', 'Btrf_on', True),
                                         ('ЯНДЕКС', 'Btsrc_ya', False), ('ПОИСК', 'Bsearch', False),
                                         ('МАРШРУТ', None, False), ('НАСТРОЙКИ', 'Bset', False),
                                         ('НОЧЬ', 'Bnight_off', False))):
        cx = 60 + i * 112
        c = ACCENT if on else MUTED
        if sign:
            ic = Image.open(os.path.join(imgs, sign + '_N.png')).convert('RGBA').resize((44 * K, 44 * K), Image.LANCZOS)
            im.paste(ic, ((cx - 22) * K, 410 * K), ic)
        else:   # a route: start, a bend, end
            d = ImageDraw.Draw(im)
            d.line([((cx - 9) * K, 446 * K), ((cx - 9) * K, 432 * K), ((cx + 9) * K, 428 * K), ((cx + 9) * K, 418 * K)],
                   fill=c, width=3 * K, joint='curve')
            for px, py in ((cx - 9, 446), (cx + 9, 418)):
                d.ellipse([(px - 4) * K, (py - 4) * K, (px + 4) * K, (py + 4) * K], fill=c)
        text(im, (cx, 463), cap, 11, c, True, 'mm')
    for i, s in enumerate(('+', '-', 'gps', 'layers')):
        round_btn(im, 754, 150 + i * 62, s, on=s == 'gps')
    return finish(im, 'browse_day')


def screen_preview():
    pal = DAY
    view = fit(RT['routes'][0]['coords'] + RT['routes'][1]['coords'], (344, 56, 800, 480))
    im = draw_map(view, pal, traffic=False)
    draw_route(im, view, 1, grey=True)
    draw_route(im, view, 0, traffic=True)
    labels(im, view, pal, avoid=[(0, 0, 344, 480)])
    a, b = RT['routes'][0]['coords'][0], RT['routes'][0]['coords'][-1]
    x, y = view.ll(*a)
    marker(im, x, y, (40, 150, 70), 'А')
    x, y = view.ll(*b)
    marker(im, x, y, ROUTE_EDGE, 'Б')
    m = RT['routes'][1]['coords'][len(RT['routes'][1]['coords']) // 2]
    x, y = view.ll(*m)
    card(im, (x / K - 44, y / K - 34, x / K + 44, y / K - 8), fill=PANEL, alpha=240, radius=13)
    text(im, (x / K, y / K - 21), '+1 мин', 12, TEXT, True, 'mm')
    status(im)
    card(im, (16, 56, 336, 466), radius=24)
    d = ImageDraw.Draw(im)
    marker(im, 44 * K, 90 * K, (40, 150, 70), 'А', 10)
    marker(im, 44 * K, 130 * K, ROUTE_EDGE, 'Б', 10)
    d.line([(44 * K, 103 * K), (44 * K, 117 * K)], fill=DIM, width=2 * K)
    text(im, (66, 90), 'Моё местоположение', 15, TEXT, False, 'lm')
    text(im, (66, 130), 'ул. Мичурина, 64', 15, TEXT, True, 'lm')
    for i, (t, km, rng, note, col) in enumerate((('7 мин', '4,6 км', 'с пробками 9–12 мин', 'пробка 400 м', (255, 130, 110)),
                                                ('8 мин', '4,6 км', 'с пробками ~8 мин', 'без сильных пробок', (120, 220, 130)))):
        y0 = 160 + i * 104
        card(im, (28, y0, 324, y0 + 94), fill=PANEL2, alpha=255, radius=18, shadow=False,
             border=ACCENT if i == 0 else None)
        text(im, (44, y0 + 12), t, 26, TEXT, True)
        text(im, (44 + d.textlength(t, font=font(26, True)) / K + 10, y0 + 22), km, 14, MUTED)
        text(im, (44, y0 + 50), rng, 13, col, True)
        text(im, (44, y0 + 70), note, 12, MUTED)
        if i == 1:   # the server's time is longer, the traffic says it is the better one
            pill(im, (214, y0 + 12, 312, y0 + 36), 'меньше пробок', (24, 60, 34), (140, 230, 160), 11)
    card(im, (28, 384, 324, 450), fill=ACCENT, alpha=255, radius=22, shadow=False, border=None)
    text(im, (176, 417), 'ПОЕХАЛИ', 20, INK, True, 'mm')
    return finish(im, 'preview_day')


def screen_editor():
    pal = NIGHT
    view = fit(RT['routes'][0]['coords'], (344, 56, 800, 480))
    im = draw_map(view, pal, traffic=False)
    draw_route(im, view, 0, traffic=False)
    labels(im, view, pal, avoid=[(0, 0, 344, 480)])
    c = RT['routes'][0]['coords']
    for pt, fill, t in ((c[0], (40, 150, 70), 'А'), (c[len(c) // 2], ACCENT, '1'), (c[-1], ROUTE_EDGE, 'Б')):
        x, y = view.ll(*pt)
        marker(im, x, y, fill, t)
    status(im, True)
    card(im, (16, 56, 336, 466), radius=24)
    text(im, (32, 72), 'Маршрут', 22, TEXT, True)
    d = ImageDraw.Draw(im)
    rows = (('А', (40, 150, 70), 'Откуда', 'Моё местоположение'), ('1', ACCENT, 'Заехать', 'Московская ул., 5'),
            ('Б', ROUTE_EDGE, 'Куда', 'ул. Мичурина, 64'))
    for i, (m, fill, lab, val) in enumerate(rows):
        y0 = 112 + i * 76
        if i < 2:
            d.line([(44 * K, (y0 + 44) * K), (44 * K, (y0 + 76) * K)], fill=DIM, width=2 * K)
        marker(im, 44 * K, (y0 + 30) * K, fill, m, 11)
        card(im, (66, y0 + 6, 324 if i != 1 else 280, y0 + 60), fill=PANEL2, alpha=255, radius=16, shadow=False,
             border=ACCENT if i == 1 else None)
        text(im, (80, y0 + 14), lab, 11, MUTED)
        text(im, (80, y0 + 32), val, 15, TEXT, True)
        if i == 1:
            pill(im, (288, y0 + 14, 324, y0 + 50), 'x', PANEL2, TEXT, 15)
    card(im, (66, 340, 324, 380), fill=PANEL, alpha=255, radius=20, shadow=False)
    text(im, (195, 360), '+ Заехать', 15, ACCENT, True, 'mm')
    card(im, (28, 400, 324, 452), fill=ACCENT, alpha=255, radius=22, shadow=False, border=None)
    text(im, (176, 426), 'ПОЕХАЛИ · 11 мин', 18, INK, True, 'mm')
    return finish(im, 'editor_night')


def screen_longpress():
    pal = NIGHT
    view = View(50.1105, 53.1975, 15.6, 0)
    im = draw_map(view, pal, traffic=True)
    labels(im, view, pal, avoid=[(160, 300, 640, 466)])
    d = ImageDraw.Draw(im)
    px, py = 400 * K, 220 * K   # the pin
    d.ellipse([px - 10 * K, py - 4 * K, px + 10 * K, py + 4 * K], fill=(0, 0, 0))
    d.polygon([(px, py), (px - 14 * K, py - 26 * K), (px + 14 * K, py - 26 * K)], fill=ACCENT)
    d.ellipse([px - 16 * K, py - 50 * K, px + 16 * K, py - 18 * K], fill=ACCENT)
    d.ellipse([px - 6 * K, py - 40 * K, px + 6 * K, py - 28 * K], fill=INK)
    car(im, 250, 360, 70)
    status(im, True)
    card(im, (160, 300, 640, 466), radius=24)
    text(im, (184, 318), 'улица Венцека, 35', 21, TEXT, True)
    text(im, (184, 348), 'Самарский район · 1,2 км от вас', 13, MUTED)
    card(im, (180, 384, 400, 446), fill=ACCENT, alpha=255, radius=20, shadow=False, border=None)
    text(im, (290, 415), 'Маршрут сюда', 17, INK, True, 'mm')
    card(im, (412, 384, 620, 446), fill=PANEL2, alpha=255, radius=20, shadow=False)
    text(im, (516, 415), 'Заехать сюда', 17, TEXT, True, 'mm')
    for i, s in enumerate(('+', '-', 'gps')):
        round_btn(im, 754, 150 + i * 62, s)
    return finish(im, 'longpress_night')


def screen_search():
    pal = DAY
    view = View(50.1125, 53.1955, 14.8, 0, 560, 250)
    im = draw_map(view, pal, traffic=False)
    labels(im, view, pal, avoid=[(0, 0, 344, 480)])
    hits = (('Чапаевская улица, 79', '1,2 км'), ('Чапаевская улица, 69', '1,4 км'), ('Чапаевский сквер', '1,9 км'),
            ('Чапаевская, 141', '2,6 км'), ('Чапаевск (город)', '42 км'))
    for i, (x, y) in enumerate(((600, 200), (650, 330), (470, 300), (720, 140))):
        marker(im, x * K, y * K, ACCENT if i == 0 else PANEL, str(i + 1))
    status(im)
    card(im, (16, 56, 336, 466), radius=24)
    card(im, (28, 68, 324, 112), fill=PANEL2, alpha=255, radius=22, shadow=False, border=ACCENT)
    text(im, (46, 90), 'Чапаевская|', 17, TEXT, True, 'lm')
    for i, (t, dist) in enumerate(hits):
        y0 = 124 + i * 66
        if i == 0:
            card(im, (28, y0, 324, y0 + 60), fill=PANEL2, alpha=255, radius=16, shadow=False, border=None)
        marker(im, 48 * K, (y0 + 30) * K, ACCENT if i == 0 else LINE, str(i + 1), 10)
        text(im, (70, y0 + 12), t, 14, TEXT, True)
        text(im, (70, y0 + 34), 'Самара', 12, MUTED)
        text(im, (314, y0 + 30), dist, 13, MUTED, False, 'rm')
    return finish(im, 'search_day')


# ---------------------------------------------------------------- the page
PAGE = """<!doctype html><html lang="ru"><head><meta charset="utf-8"><title>OSMapsCE — дизайн v3</title>
<style>
@page {{ size: A4 landscape; margin: 12mm; }}
body {{ font-family: 'Golos Text', 'Segoe UI', sans-serif; background: #0b0c0e; color: #f3f1ec; margin: 0; padding: 24px; }}
h1 {{ font-size: 26px; margin: 0 0 6px; }} h2 {{ font-size: 18px; margin: 26px 0 8px; color: #ff6b1a; }}
p, li {{ color: #a3a8b0; font-size: 13px; line-height: 1.5; max-width: 1100px; }}
.grid {{ display: grid; grid-template-columns: repeat(2, 1fr); gap: 22px; }}
figure {{ margin: 0; break-inside: avoid; }} img {{ width: 100%; border-radius: 10px; border: 1px solid #34383f; }}
figcaption {{ font-size: 12.5px; color: #a3a8b0; margin-top: 6px; line-height: 1.45; }} figcaption b {{ color: #f3f1ec; }}
.sw {{ display: inline-flex; align-items: center; gap: 6px; margin: 0 14px 6px 0; font-size: 12px; color: #a3a8b0; }}
.sw i {{ width: 22px; height: 16px; border-radius: 4px; display: inline-block; border: 1px solid #34383f; }}
</style></head><body>
<h1>OSMapsCE — дизайн v3 (макет)</h1>
<p>На основе VestaHome 0.6 (палитра, шрифт Golos, акцент из темы VH, нижняя панель) с оглядкой на дизайн-систему
Škoda Flow (Red Dot 2021): карта на весь экран, поверх — плавающие скруглённые карточки с тенью вместо полос; на
карточке одна главная цифра; один акцент; кнопки — круглые плитки справа; много воздуха. Карта — <b>векторная</b>
(настоящие тайлы OpenFreeMap, Самара), своей палитрой: дневная тёплая светлая и ночная своя (не инверсия), подписи
улиц всегда ровные — и когда карта повёрнута по курсу. Маршрут — настоящий OSRM, цвета пробок на нём — для примера.</p>
<p>{palettes}</p>
<div class="grid">{figs}</div>
<h2>Что меняется по сравнению с нынешней OSMapsCE</h2>
<ul>
<li>Верхняя полоса статуса → мягкая тень сверху: часы и «чипы» (GPS, источник пробок); карта видна до края.</li>
<li>Подсказка поворота: большая карточка (стрелка в квадрате акцента, расстояние крупно, улица, «затем…»,
250 px) — только перед поворотом (300 м или 20 с); в остальное время — компактная, ~¼ большой: знак и расстояние.</li>
<li>Цвета — как у VestaHome: карточки и круглые кнопки цвета его плиток (23272E), сплошные; нижняя панель —
сплошная, как у VestaHome; значки — Lucide, как у VestaHome; «назад» — круг слева внизу.</li>
<li>Поездка — «таблетка» внизу: прибытие, минуты (с вилкой по пробкам), километры, «✕» — закончить.</li>
<li>Скорость и знак лимита — круг слева внизу; «+ / −», компас, звук — круглые кнопки справа.</li>
<li>Поиск, варианты, редактор — <b>левая панель</b> 320 px поверх карты: карта справа видна и показывает выбор (номера
мест, А/Б, варианты серым с «+1 мин»).</li>
<li>Долгое нажатие — булавка и карточка снизу: адрес, расстояние, «Маршрут сюда» (акцент) и «Заехать сюда».</li>
<li>Векторная карта даёт: подписи не переворачиваются, ночная палитра своя, линии ровные на любом масштабе, нет
плиток OSM по сети (файл региона на SD). Растровая версия этого дизайна тоже возможна — меняется только карта.</li>
<li>Шрифт — Golos Text (как VestaHome; в OSMapsCE сейчас Tahoma).</li>
<li><b>Обводки</b> (правило 2026-10-26, как в современных системах — Material 3, iOS, Škoda Flow): у кнопок и карточек
обводок нет, они отделяются заливкой и тенью; выбранное (вариант, поле ввода, точка «Заехать») — рамка цвета
акцента 2 px; карточки поверх <b>ночной</b> карты — едва заметная светлая кромка 1 px (тень на тёмном не видна).</li>
<li>Акцент и подсветка машины — из темы VestaHome.</li>
</ul>
</body></html>"""

CAPTIONS = {
    'drive_day': '<b>Ведение, день, перед поворотом</b> (300 м или 20 с): большая карточка, 250 px. Карта по курсу, маршрут окрашен по пробкам, подписи улиц ровные при повороте.',
    'drive_night': '<b>Ведение, ночь, до поворота далеко:</b> компактная подсказка (знак и расстояние, ~¼ большой). Своя ночная палитра (не инверсия дня).',
    'browse_day': '<b>Просмотр.</b> «Куда едем?» и быстрые места; пробки тонкими линиями вдоль улиц; нижняя панель VH по касанию — «Поиск» и «Маршрут» на средних местах.',
    'preview_day': '<b>Варианты.</b> Слева — А/Б, варианты с временем, вилкой по пробкам и пометкой, «ПОЕХАЛИ»; на карте другой вариант серым с «+1 мин».',
    'editor_night': '<b>Редактор маршрута.</b> Точки лесенкой А — «Заехать» — Б, «+ Заехать», «ПОЕХАЛИ» со временем.',
    'longpress_night': '<b>Долгое нажатие.</b> Булавка, карточка с адресом и расстоянием, «Маршрут сюда» / «Заехать сюда».',
    'search_day': '<b>Поиск.</b> Найденное — нумерованные места в панели и те же номера на карте.',
}


def main():
    global FEATS, RT
    if not glob.glob(os.path.join(TILES, '*.pbf')):
        raise SystemExit('no vector tiles: python OSMapsCE/native_vtest/get_tiles.py')
    os.makedirs(OUT, exist_ok=True)
    FEATS = load_tiles()
    RT = json.load(open(os.path.join(OUT, 'route.json'), encoding='utf-8'))
    global ACCENT
    ACCENT = ACCENTS[130][1]   # the page shows the design's own orange; make_vh_v07_mockup.py - the other paints
    shots = [screen_drive(False), screen_drive(True), screen_browse(), screen_preview(), screen_editor(),
             screen_longpress(), screen_search()]
    pal = []
    for name, p in (('день', DAY), ('ночь', NIGHT)):
        sw = ''.join('<span class="sw"><i style="background:rgb%s"></i>%s</span>' % (str(p[k] if not isinstance(p[k][0], tuple) else p[k][0]), k)
                     for k in ('bg', 'resid', 'park', 'water', 'building', 'minor', 'primary', 'major'))
        pal.append('<b>Карта, %s:</b> %s' % (name, sw))
    figs = ''.join('<figure><img src="osm_v3/%s"><figcaption>%s</figcaption></figure>' % (s, CAPTIONS[s[:-4]])
                   for s in shots)
    html = os.path.join(HERE, 'osm-skin-v3.html')
    open(html, 'w', encoding='utf-8').write(PAGE.format(palettes='<br>'.join(pal), figs=figs))
    edge = r'C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe'
    if os.path.exists(edge):
        subprocess.run([edge, '--headless', '--disable-gpu', '--no-pdf-header-footer',
                        '--print-to-pdf=' + os.path.join(HERE, 'osm-skin-v3.pdf'), 'file:///' + html.replace('\\', '/')],
                       check=False, timeout=120)
    print(len(shots), 'screens ->', os.path.normpath(OUT), '+ osm-skin-v3.html / .pdf')


if __name__ == '__main__':
    main()
