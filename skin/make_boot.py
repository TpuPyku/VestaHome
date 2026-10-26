#!/usr/bin/env python3
"""make_boot.py - VestaHome's boot picture -> VestaHome/VestaHome/Skin/boot.png (800 x 480).

Shown by VestaStub from its start until VestaHome is in sight (three dots run under it, drawn by the stub). It
follows the firmware's own boot logo (chrome on black, MAIN/4120/startup.raw) and leads to VestaHome's design
(the user 2026-10-26): the same black, the name in the middle with a chrome-like light from above, and VestaHome's
rhombus mosaic coming up from the bottom; the stub lays the saver's soft spot of the accent over it. 24-bit colour.

Usage: python VestaHome/skin/make_boot.py
"""
import os

from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..', 'VestaHome', 'VestaHome', 'Skin', 'boot.png')
FONT = os.path.join(HERE, '..', 'VestaHome', 'VestaHome', 'Fonts', 'GolosText-Bold.ttf')
W, H = 800, 480
CW, CH = 64, 112   # the mosaic's cell, as VestaHome's background and saver
SHADES = {'left': (0x17, 0x1a, 0x20), 'right': (0x12, 0x14, 0x19), 'top': (0x1b, 0x1e, 0x24), 'bottom': (0x0f, 0x11, 0x14)}


def mosaic():
    im = Image.new('RGB', (W, H), (0, 0, 0))
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
    # black on top (as the firmware's logo), the mosaic coming up from the bottom
    tiles = mosaic()
    base = Image.new('RGB', (W, H), (0, 0, 0))
    ramp = Image.new('L', (W, H))
    rp = ramp.load()
    for y in range(H):
        t = max(0.0, (y - H * 0.45) / (H * 0.55))   # 0 above the middle, 1 at the bottom
        v = int(255 * t ** 1.6 + 0.5)
        for x in range(W):
            rp[x, y] = v
    base = Image.composite(tiles, base, ramp)

    # the name: white light from above to grey below (the logo's chrome), centred a little above the middle
    font = ImageFont.truetype(FONT, 76)
    text = 'VestaHome'
    box = font.getbbox(text)
    tw, th = box[2] - box[0], box[3] - box[1]
    tx, ty = (W - tw) // 2 - box[0], 196 - th // 2 - box[1]
    mask = Image.new('L', (W, H))
    ImageDraw.Draw(mask).text((tx, ty), text, font=font, fill=255)
    chrome = Image.new('RGB', (W, H))
    cp = chrome.load()
    top, bottom = ty + box[1], ty + box[3]
    for y in range(H):
        t = min(1.0, max(0.0, (y - top) / max(1, bottom - top)))
        c = int(250 - 120 * t) if t < 0.55 else int(184 + 40 * (t - 0.55) / 0.45)   # a bright top, a dark band, a lift
        for x in range(W):
            cp[x, y] = (c, c, min(255, c + 6))
    base = Image.composite(chrome, base, mask)

    # (no accent line: VestaStub lays the saver's spot of the accent chosen in VestaHome over the picture)
    d = ImageDraw.Draw(base)
    ly = 196 + th // 2 + 14
    small = ImageFont.truetype(FONT.replace('Bold', 'Regular'), 20)
    sub = 'оболочка для Lada Vesta'
    sb = small.getbbox(sub)
    d.text(((W - (sb[2] - sb[0])) // 2 - sb[0], ly + 22), sub, font=small, fill=(0x8a, 0x8f, 0x98))

    base.save(OUT, optimize=True)
    print('%s %d B' % (os.path.normpath(OUT), os.path.getsize(OUT)))


if __name__ == '__main__':
    main()
