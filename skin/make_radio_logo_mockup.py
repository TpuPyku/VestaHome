"""Mockups: where the radio screen shows the station's logo (the user 2026-11-19). Over the real screen shot
docs/shots/04_radio.png; the logos from make_radio.py's out (VestaHome/VestaHome/RadioLogos/logos, git-ignored - the
logos are the stations', so the picture made here is not kept in the repo either).

    python make_radio_logo_mockup.py <logos folder> <out.png>
"""
import os
import sys

from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
SHOT = os.path.join(HERE, '..', 'docs', 'shots', '04_radio.png')
TILE = (27, 30, 36)
# the presets on the shot: their logos (Спорт FM none in the table - its tile without one)
PRESETS = ['ru024', None, 'ru034', 'ru013', 'ru023', 'ru028', 'ru001', 'ru016']
CUR = 'ru023'   # DFM, playing


def logo(folder, lid, size):
    """The logo on a light rounded plate (a dark logo stays readable at night too)."""
    plate = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(plate)
    d.rounded_rectangle((0, 0, size - 1, size - 1), radius=size // 6, fill=(245, 245, 245, 255))
    im = Image.open(os.path.join(folder, lid + '.png')).convert('RGBA').resize((size - 6, size - 6), Image.LANCZOS)
    plate.alpha_composite(im, (3, 3))
    return plate


def tiles_with_logos(base, folder, size=40):
    """Each preset: the logo at its left, the frequency and the name moved right."""
    for i, lid in enumerate(PRESETS):
        x, y = 16 + (i % 4) * 194, 256 + (i // 4) * 72
        body = base.crop((x + 12, y + 8, x + 172 - size - 10, y + 60))
        bg = base.getpixel((x + 170, y + 10))   # (the tile's own colour)
        ImageDraw.Draw(base).rectangle((x + 10, y + 6, x + 174, y + 60), fill=bg)
        base.paste(body, (x + 12 + size + 10, y + 8))
        if lid:
            base.alpha_composite(logo(folder, lid, size), (x + 12, y + 12))


def label(base, text):
    d = ImageDraw.Draw(base)
    try:
        f = ImageFont.truetype('arialbd.ttf', 22)
    except OSError:
        f = ImageFont.load_default()
    d.rounded_rectangle((300, 4, 500, 40), radius=10, fill=(230, 90, 20))
    d.text((400, 22), text, anchor='mm', fill='white', font=f)


def variant(folder, kind):
    base = Image.open(SHOT).convert('RGBA')
    if kind in ('A', 'D'):   # A: big, at the right of the frequency
        base.alpha_composite(logo(folder, CUR, 96), (440, 62))
    if kind == 'B':          # B: big, at the left: the frequency, the name, RDS moved right
        top = base.crop((20, 60, 560, 200))
        bot = base.crop((20, 140, 780, 200))
        ImageDraw.Draw(base).rectangle((16, 60, 552, 200), fill=(20, 20, 24))
        base.paste(top.crop((0, 0, 430, 80)), (20 + 120, 60))
        base.paste(bot, (20 + 120, 140))
        base.alpha_composite(logo(folder, CUR, 104), (24, 66))
    if kind in ('C', 'D'):   # C: small, on each preset
        tiles_with_logos(base, folder)
    label(base, {'A': 'А: у частоты', 'B': 'Б: слева, крупно', 'C': 'В: на пресетах', 'D': 'Г: А + В'}[kind])
    return base


def main():
    folder, out = sys.argv[1], sys.argv[2]
    shots = [variant(folder, k) for k in 'ABCD']
    sheet = Image.new('RGBA', (800 * 2 + 20, 480 * 2 + 20), (255, 255, 255, 255))
    for i, s in enumerate(shots):
        sheet.paste(s, ((i % 2) * 820, (i // 2) * 500))
    sheet.convert('RGB').save(out)
    print(out)


if __name__ == '__main__':
    main()
