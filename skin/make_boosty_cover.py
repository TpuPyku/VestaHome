#!/usr/bin/env python3
"""make_boosty_cover.py - the Boosty page's covers -> VestaHome/skin/boosty/cover_8x1.png (2400 x 300) and
cover_3x1.png (1500 x 500).

VestaHome's rhombus mosaic (as its background and saver) under an orange glow (the brand colour), the name on the
left, and real screens on the right: VestaHome's from the PC build's shots (docs/shots, tools/make_shots.py) and
OSMapsCE's in design v3, taken here from OSMapsPC.exe (--osm: its ini kept and put back, the GPS replayed from
track.nmea, a day screen and a night one in 3D; saved as boosty/osm_day.png, osm_night.png).

Usage: python VestaHome/skin/make_boosty_cover.py [--osm]   (Windows for --osm; Pillow)
"""
import os
import subprocess
import sys
import time

from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, 'boosty')
SHOTS = os.path.join(HERE, '..', 'docs', 'shots')
FONTS = os.path.join(HERE, '..', 'VestaHome', 'VestaHome', 'Fonts')
OSM = os.path.join(HERE, '..', '..', 'OSMapsCE', 'OSMapsCE')
CW, CH = 64, 112   # the mosaic's cell, as VestaHome's background
SHADES = ((0x17, 0x1a, 0x20), (0x12, 0x14, 0x19), (0x1b, 0x1e, 0x24), (0x0f, 0x11, 0x14))   # left right top bottom
ORANGE = (255, 107, 26)


def osm_shots():
    """OSMapsPC.exe's window, a day screen and a night one in 3D, while the replayed drive goes."""
    sys.path.insert(0, os.path.join(HERE, '..', 'tools'))
    import ctypes
    from make_shots import grab
    import winreg
    ini = os.path.join(OSM, 'OSMapsCE.ini')
    saved = open(ini, 'rb').read()
    key = winreg.CreateKey(winreg.HKEY_CURRENT_USER, r'Software\Vesta')   # the night shared with VestaHome (vnight)
    try:
        night_was = winreg.QueryValueEx(key, 'Night')[0]
    except OSError:
        night_was = None
    try:
        for name, night, over in (('osm_day', 0, {'night': '0', 'tilt': '0', 'zoom': '16', 'follow': '1'}),
                                  ('osm_night', 1, {'night': '1', 'tilt': '1', 'zoom': '17', 'follow': '2'})):
            winreg.SetValueEx(key, 'Night', 0, winreg.REG_DWORD, night)
            over.update({'gps': '1', 'gps_port@PC': 'track.nmea', 'vector': '1'})   # follow 2: heading-up (3D)
            lines = [l for l in saved.split(b'\r\n') if l.split(b'=')[0].decode('latin-1') not in over]
            lines = ['%s=%s' % kv for kv in over.items()] + [l.decode('latin-1') for l in lines]
            open(ini, 'wb').write('\r\n'.join(lines).encode('latin-1'))
            p = subprocess.Popen([os.path.join(OSM, 'OSMapsPC.exe')], cwd=OSM)
            try:
                time.sleep(25)   # the GPS fix, the tiles
                h = ctypes.windll.user32.FindWindowW('OSMapsCE_Wnd', None)
                if not h:
                    sys.exit('no OSMapsCE window')
                ctypes.windll.user32.SetForegroundWindow(h)
                time.sleep(1)
                grab(h, os.path.join(OUT, name + '.png'))
            finally:
                p.kill()
                p.wait()
    finally:
        open(ini, 'wb').write(saved)
        if night_was is None:
            winreg.DeleteValue(key, 'Night')
        else:
            winreg.SetValueEx(key, 'Night', 0, winreg.REG_DWORD, night_was)


def mosaic(w, h):
    im = Image.new('RGB', (w, h))
    d = ImageDraw.Draw(im)
    for y in range(0, h, CH):
        for x in range(0, w, CW):
            m = (x + CW / 2, y + CH / 2)
            d.polygon([(x, y), m, (x, y + CH)], fill=SHADES[0])
            d.polygon([(x + CW, y), m, (x + CW, y + CH)], fill=SHADES[1])
            d.polygon([(x, y), (x + CW, y), m], fill=SHADES[2])
            d.polygon([(x, y + CH), (x + CW, y + CH), m], fill=SHADES[3])
    return im


def glow(w, h, cx, cy, r, alpha):
    """A soft orange spot (as the saver's, in the accent) at cx, cy."""
    m = Image.new('L', (w, h))
    ImageDraw.Draw(m).ellipse([cx - r, cy - r * 0.6, cx + r, cy + r * 0.6], fill=alpha)
    return m.filter(ImageFilter.GaussianBlur(r * 0.45))


def screen(path, height):
    """A screen as on the unit: rounded corners, a thin frame, a soft shadow below."""
    im = Image.open(path).convert('RGB')
    im = im.resize((height * 800 // 480, height), Image.LANCZOS)
    w = im.width
    rad = max(6, height // 24)
    mask = Image.new('L', (w, height))
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, w - 1, height - 1], rad, fill=255)
    pad = height // 6
    out = Image.new('RGBA', (w + 2 * pad, height + 2 * pad), (0, 0, 0, 0))
    sh = Image.new('L', out.size)
    ImageDraw.Draw(sh).rounded_rectangle([pad, pad + height // 30, pad + w, pad + height + height // 30], rad, fill=170)
    out.putalpha(sh.filter(ImageFilter.GaussianBlur(pad // 2)))
    face = Image.new('RGBA', (w, height))
    face.paste(im, (0, 0))
    face.putalpha(mask)
    out.alpha_composite(face, (pad, pad))
    ImageDraw.Draw(out).rounded_rectangle([pad, pad, pad + w - 1, pad + height - 1], rad, outline=(70, 76, 88), width=1)
    return out, pad


def cover(w, h, left, right, size, title, sub, gap, clear):
    """The name in the middle (the user 2026-11-04: the 3:1 one is the phones' view, cut at the sides), the screens
    to its left and right, `clear` px kept free around the middle."""
    im = mosaic(w, h)
    dark = Image.new('RGB', (w, h), (0, 0, 0))
    im = Image.blend(im, dark, 0.15)
    orange = Image.new('RGB', (w, h), ORANGE)
    im = Image.composite(orange, im, glow(w, h, w // 2, int(h * 0.5), int(h * 1.1), 120))
    im = im.convert('RGBA')
    sh = int(h * size)
    placed = []
    for side, names in ((-1, left), (1, right)):
        x = w // 2 + side * clear
        for i, name in enumerate(names):   # from the middle outwards, the nearer on top
            path = os.path.join(SHOTS if not name.startswith('osm_') else OUT, name)
            s, pad = screen(path, sh)
            sw = s.width - 2 * pad
            sx = x - sw if side < 0 else x
            placed.append((i, s, sx - pad, (h - sh) // 2 - pad + (i % 2) * int(h * 0.04) - int(h * 0.02)))
            x += side * (sw + gap)
    for i, s, sx, sy in sorted(placed, key=lambda p: -p[0]):
        im.alpha_composite(s, (sx, sy))
    d = ImageDraw.Draw(im)
    big = ImageFont.truetype(os.path.join(FONTS, 'GolosText-Bold.ttf'), title)
    small = ImageFont.truetype(os.path.join(FONTS, 'GolosText-Regular.ttf'), sub)
    ty = h // 2 - title // 2 - sub
    lines = (('VestaHome', big, (255, 255, 255), ty),
             ('Оболочка для MMC Лада Веста', small, (190, 196, 206), ty + int(title * 1.15)),
             ('Карты OSMapsCE', small, ORANGE, ty + int(title * 1.15) + int(sub * 1.4)))
    for text, font, col, y in lines:
        box = font.getbbox(text)
        d.text(((w - (box[2] - box[0])) // 2 - box[0], y), text, font=font, fill=col)
    return im.convert('RGB')


def main():
    os.makedirs(OUT, exist_ok=True)
    if '--osm' in sys.argv:
        osm_shots()
    for name, w, h, left, right, size, title, sub, gap, clear in (
            ('cover_8x1', 2400, 300, ['04_radio.png', 'osm_day.png'], ['01_home.png', 'osm_night.png'], 0.72, 64, 24,
             28, 300),
            ('cover_3x1', 1500, 500, ['osm_night.png'], ['01_home.png'], 0.46, 70, 26, 0, 300)):
        cover(w, h, left, right, size, title, sub, gap, clear).save(os.path.join(OUT, name + '.png'))
        print('%s: %d x %d' % (name, w, h))


if __name__ == '__main__':
    main()
