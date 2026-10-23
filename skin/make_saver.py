#!/usr/bin/env python3
"""make_saver.py - the screen saver's background of design 0.6 -> VestaHome/VestaHome/Skin/saver.png (800 x 480).

As the mockup's saver (make_home_mockup.py, .saver): the rhombus mosaic in cells of 64 x 112, darkened downwards.
No light: VestaHome draws the soft spot over the clock itself, in the accent colour chosen (Settings -> General ->
Theme, the user 2026-10-23). Full 24-bit colour: VestaHome draws in 32 bits. The previous
picture (the headlights) stays as saver2.png: "saver = saver2.png" in skin.ini brings it back.

Usage: python VestaHome/skin/make_saver.py
"""
import os

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..', 'VestaHome', 'VestaHome', 'Skin', 'saver.png')
W, H = 800, 480
CW, CH = 64, 112   # a mosaic cell: four triangles meeting in its middle
SHADES = {'left': (0x17, 0x1a, 0x20), 'right': (0x12, 0x14, 0x19), 'top': (0x1b, 0x1e, 0x24), 'bottom': (0x0f, 0x11, 0x14)}
INK = (0x0e, 0x10, 0x13)   # VestaHome's background


def mosaic():
    im = Image.new('RGB', (W, H), INK)
    d = ImageDraw.Draw(im)
    for y in range(0, H, CH):
        for x in range(0, W, CW):
            m = (x + CW / 2, y + CH / 2)
            d.polygon([(x, y), m, (x, y + CH)], fill=SHADES['left'])
            d.polygon([(x + CW, y), m, (x + CW, y + CH)], fill=SHADES['right'])
            d.polygon([(x, y), (x + CW, y), m], fill=SHADES['top'])
            d.polygon([(x, y + CH), (x + CW, y + CH), m], fill=SHADES['bottom'])
    return im


def main():
    base = mosaic()
    px = base.load()
    for y in range(H):
        dark = 0.6 + 0.25 * y / (H - 1)   # the shade over the mosaic, more towards the bottom
        for x in range(W):
            r, g, b = px[x, y]
            r, g, b = (c * (1 - dark) + i * dark for c, i in zip((r, g, b), INK))
            px[x, y] = (int(r + 0.5), int(g + 0.5), int(b + 0.5))
    base.save(OUT, optimize=True)
    print('%s %d B' % (os.path.normpath(OUT), os.path.getsize(OUT)))


if __name__ == '__main__':
    main()
