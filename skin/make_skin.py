#!/usr/bin/env python3
"""make_skin.py - generate the VESTA skin pictures for the stock shell G3NavHMI.

Follows the rules in skin/vesta-skin.html (concept v0.2): graphite ground,
one orange accent, line icons not thinner than the panel border, captions
inside the tiles, the *_lettering.png strips left transparent because the
shell draws no caption when the strip is empty.

Only the pieces the shell loads from res/img/Itelma/MMC27/Temp/main plus the
800x480 "desktop" (Main/new_logo.png) are produced, in the exact pixel sizes
of the stock MMC27 dump. Overlay the output folder on MMC27 - "replacing the
pictures is enough", no layout changes (design document, main menu section).

    python skin/make_skin.py [--out G3patch/skin-vesta]

Requires Pillow. Output layout mirrors MMC27:
    <out>/Main/new_logo.png
    <out>/Temp/main/main_symbol_<name>.png           187x187 tile
    <out>/Temp/main/main_symbol_<name>_lettering.png 99x11 transparent
    <out>/Temp/main/main_symbol_<name>_lettering_a.png
    <out>/Temp/main/main_symbol_arrow_{left,right}[_a].png 27x37
    <out>/Temp/main/main_symbol_screen[_a].png       18x18
    <out>/Temp/main/main_line_white_{up,down}.png    634x4
    <out>/Temp/main/main_folders_{first,second}_screen.png
"""
import argparse
import os
from PIL import Image, ImageDraw, ImageFont

# ---- tokens (skin/vesta-skin.html :root) ----------------------------------
INK   = (0x0b, 0x0c, 0x0e)   # graphite ground
PANEL = (0x17, 0x19, 0x1c)   # raised surfaces: tiles
LINE  = (0x34, 0x38, 0x3f)   # 2 px borders
TEXT  = (0xf3, 0xf1, 0xec)   # warm white
MUTED = (0xa3, 0xa8, 0xb0)   # secondary text
DIM   = (0x5d, 0x63, 0x6c)   # inactive
ORANGE = (0xff, 0x6b, 0x1a)  # the one accent
FACET  = (0xc2, 0x4a, 0x0b)  # tile corner
TRI    = [(0x0e, 0x0f, 0x12), (0x14, 0x16, 0x1a), (0x17, 0x19, 0x1d)]

FONT = None
for _cand in ("arial.ttf", "C:/Windows/Fonts/arial.ttf", "segoeui.ttf"):
    try:
        ImageFont.truetype(_cand, 10)
        FONT = _cand
        break
    except Exception:
        FONT = None


def font(size):
    return ImageFont.truetype(FONT, size) if FONT else ImageFont.load_default()


# ---- icon paths: 48x48 design box, list of polylines in (x, y) ------------
def icon_polys(name):
    if name == "navigation":
        return [[(24, 5), (39, 41), (24, 32), (9, 41), (24, 5)]], []
    if name == "radio":
        p = [[(13, 17), (33, 6)], [(11, 25), (21, 25)], [(11, 31), (21, 31)], [(11, 37), (21, 37)]]
        return p, [("rect", (5, 17, 43, 42)), ("circle", (32, 29.5, 5.5))]
    if name == "audio":
        return [[(18, 36), (18, 11), (39, 7), (39, 31)]], [("circle", (13, 36, 5)), ("circle", (34, 31, 5))]
    if name == "video":
        return [[(32, 20), (43, 13), (43, 35), (32, 28), (32, 20)]], [("rect", (5, 12, 32, 36))]
    if name == "photo":
        return [[(8, 35), (19, 24), (27, 31), (33, 25), (41, 33)]], \
               [("rect", (5, 9, 43, 39)), ("circle", (16, 19, 3.5))]
    if name == "phone":
        return [[(15, 6), (21, 6), (24.5, 15), (19.5, 18.5), (22, 23), (25, 26), (29.5, 28.5), (33, 23.5),
                 (42, 27), (42, 33), (38, 37), (26, 35), (16, 27), (11, 16), (11, 10), (15, 6)]], []
    if name == "aux":
        return [[(24, 4), (24, 17)], [(20, 26), (20, 43), (28, 43), (28, 26)]], [("rect", (18, 17, 30, 26))]
    return [], []


def sx(v, box, size):
    return box[0] + v * size / 48.0


def sy(v, box, size):
    return box[1] + v * size / 48.0


def draw_icon(d, name, box, size, color, width=3):
    polys, shapes = icon_polys(name)
    for sh in shapes:
        if sh[0] == "rect":
            x0, y0, x1, y1 = sh[1]
            d.rectangle([sx(x0, box, size), sy(y0, box, size), sx(x1, box, size), sy(y1, box, size)],
                        outline=color, width=width)
        elif sh[0] == "circle":
            cx, cy, r = sh[1]
            d.ellipse([sx(cx - r, box, size), sy(cy - r, box, size), sx(cx + r, box, size), sy(cy + r, box, size)],
                      outline=color, width=width)
    for poly in polys:
        pts = [(sx(x, box, size), sy(y, box, size)) for x, y in poly]
        d.line(pts, fill=color, width=width, joint="curve")



def tile(name, label, size=187):
    """One main-menu tile: rounded panel, facet corner, line icon, caption."""
    ss = 4  # supersample for round corners / text
    w = h = size * ss
    im = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle([0, 0, w - 1, h - 1], radius=18 * ss, fill=PANEL, outline=LINE, width=2 * ss)
    cw = 38 * ss  # facet corner, clipped visually by the rounded top-right corner
    d.polygon([(w - cw, 0), (w, 0), (w, cw)], fill=FACET)
    draw_icon(d, name, (52 * ss, 34 * ss, 0, 0), 68 * ss, TEXT, width=3 * ss)
    if label:
        f = font(17 * ss)
        tw = d.textlength(label, font=f)
        d.text((w / 2 - tw / 2, (size - 42) * ss), label, font=f, fill=MUTED)
    return im.resize((size, size), Image.LANCZOS)


def lettering(size=(99, 11)):
    """Caption strip the shell overlays: keep it fully transparent (design rule)."""
    return Image.new("RGBA", size, (0, 0, 0, 0))


def arrow(right, active, size=(27, 37)):
    ss = 4
    im = Image.new("RGBA", (size[0] * ss, size[1] * ss), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    w, h, pad = size[0] * ss, size[1] * ss, 6 * ss
    if right:
        pts = [(pad, pad), (w - pad, h / 2), (pad, h - pad)]
    else:
        pts = [(w - pad, pad), (pad, h / 2), (w - pad, h - pad)]
    d.line(pts, fill=ORANGE if active else TEXT, width=4 * ss, joint="curve")
    return im.resize(size, Image.LANCZOS)


def sun(size=(18, 18)):
    ss = 4
    im = Image.new("RGBA", (size[0] * ss, size[1] * ss), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    cx = cy = size[0] * ss / 2.0
    d.ellipse([cx - 3.5 * ss, cy - 3.5 * ss, cx + 3.5 * ss, cy + 3.5 * ss], outline=MUTED, width=int(1.5 * ss))
    return im.resize(size, Image.LANCZOS)


def background(size=(800, 480)):
    """Graphite ground, muted triangle mosaic, orange band along the bottom."""
    im = Image.new("RGB", size, INK)
    d = ImageDraw.Draw(im)
    tw, th = 48, 42
    for row, y in enumerate(range(70, 392, th)):
        for col, x in enumerate(range(-tw, 800 + tw, tw)):
            shade = TRI[(row + col) % 3]
            if (row + col) % 2:
                d.polygon([(x, y + th), (x + tw / 2, y), (x + tw, y + th)], fill=shade)
            else:
                d.polygon([(x, y), (x + tw, y), (x + tw / 2, y + th)], fill=shade)
    d.rectangle([0, 389, 800, 392], fill=ORANGE)
    band = Image.new("RGB", (800, 88), (0x8f, 0x36, 0x08))
    db = ImageDraw.Draw(band)
    for x in range(0, 800, tw):
        db.polygon([(x, 88), (x + tw / 2, 0), (x + tw, 88)], fill=(0xc2, 0x4a, 0x0b))
    im.paste(band, (0, 392))
    d.rectangle([83, 68, 717, 70], fill=LINE)
    d.rectangle([0, 398, 800, 400], fill=LINE)
    return im


def folder_bar(width):
    """Panel strip behind the main-menu folder row (779 / 595 x 183 in stock)."""
    im = Image.new("RGBA", (width, 183), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle([0, 0, width - 1, 182], radius=14, fill=PANEL + (255,), outline=LINE + (255,), width=2)
    return im


def line_strip(size=(634, 4)):
    return Image.new("RGBA", size, LINE + (255,))


def save(im, path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    im.save(path)
    print("  ", os.path.relpath(path).replace("\\", "/"), im.size)


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    ap = argparse.ArgumentParser(description="Generate the VESTA shell skin (skin/vesta-skin.html).")
    ap.add_argument("--out", default=os.path.join(root, "G3patch", "skin-vesta"))
    args = ap.parse_args()

    print("VESTA skin ->", args.out)
    tm = os.path.join(args.out, "Temp", "main")
    md = os.path.join(args.out, "Main")

    tiles = [("navigation", "НАВИГАЦИЯ"), ("radio", "РАДИО"), ("audio", "АУДИО"), ("video", "ВИДЕО"),
             ("photo", "ФОТО"), ("phone", "ТЕЛЕФОН"), ("aux", "AUX")]
    for name, label in tiles:
        save(tile(name, label), os.path.join(tm, "main_symbol_%s.png" % name))
        save(lettering(), os.path.join(tm, "main_symbol_%s_lettering.png" % name))
        save(lettering(), os.path.join(tm, "main_symbol_%s_lettering_a.png" % name))

    save(arrow(False, False), os.path.join(tm, "main_symbol_arrow_left.png"))
    save(arrow(False, True), os.path.join(tm, "main_symbol_arrow_left_a.png"))
    save(arrow(True, False), os.path.join(tm, "main_symbol_arrow_right.png"))
    save(arrow(True, True), os.path.join(tm, "main_symbol_arrow_right_a.png"))
    save(sun(), os.path.join(tm, "main_symbol_screen.png"))
    save(sun(), os.path.join(tm, "main_symbol_screen_a.png"))

    save(line_strip(), os.path.join(tm, "main_line_white_up.png"))
    save(line_strip(), os.path.join(tm, "main_line_white_down.png"))
    save(folder_bar(779), os.path.join(tm, "main_folders_first_screen.png"))
    save(folder_bar(595), os.path.join(tm, "main_folders_second_screen.png"))

    save(background(), os.path.join(md, "new_logo.png"))
    print("done")


if __name__ == "__main__":
    main()
