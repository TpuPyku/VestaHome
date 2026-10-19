"""make_saver.py - the screen saver's background for VestaHome: Skin/saver.png, 800 x 480.

"The concept at night" (the Vesta Concept photographed in the dark): near-black, the X ">" "<" drawn only by
cool rim light on its edges, the grille's lines faint behind the clock, and the daytime running lights as on the
tuned Vesta (MAIN/Saver/images(1).jfif) in orange: a bracket under each headlight - along its lower edge inwards,
a sharp turn down beside the grille, a round corner out along the bottom - thin at the ends and thickest low,
a white-hot core in an orange glow.
Drawn 4x larger and scaled down (smooth edges), then dithered to RGB565 so the dark gradients don't band on the
unit's 16-bit screen. VestaHome draws the clock, the date, the weather and what plays over it; at night it dims
the picture by a third.

    python make_saver.py            (needs Pillow)
"""
import math
import os
import random
from PIL import Image, ImageDraw, ImageFilter

W, H, S = 800, 480, 4
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'VestaHome', 'VestaHome', 'Skin', 'saver.png')
ORANGE = (255, 107, 26)
CORE = (255, 222, 196)   # the light guide's hot middle
RIM = (150, 170, 190)    # cool rim light on the body
RIM_CORE = (230, 238, 248)

# the left ">" of the X: its outer edge (facing the centre) and inner edge, the top end to the bottom end
X_OUTER = (((-40, 104), (100, 128), (212, 222), (262, 290)), ((262, 290), (214, 352), (100, 400), (-40, 416)))
X_INNER = (((-40, 164), (60, 182), (128, 236), (146, 290)), ((146, 290), (128, 322), (60, 350), (-40, 362)))
# the left running light's centre line: under the headlight inwards, down beside the grille (a sharp corner: the
# points bunched there), out along the bottom (a round one)
DRL = [(36, 146), (120, 174), (196, 201), (204, 204), (204, 209), (202, 236), (198, 296), (193, 324), (178, 337), (130, 340), (52, 340)]
# its width along it (share of the length -> px): thin ends, thickest on the lower leg
DRL_W = ((0.0, 0.6), (0.12, 2.0), (0.36, 2.8), (0.62, 3.6), (0.80, 4.4), (0.93, 3.2), (1.0, 0.6))


def bez(c, n=64):
    (ax, ay), (bx, by), (cx, cy), (dx, dy) = c
    out = []
    for i in range(n + 1):
        t = i / n
        u = 1 - t
        a, b, cc, d = u * u * u, 3 * u * u * t, 3 * u * t * t, t * t * t
        out.append((a * ax + b * bx + cc * cx + d * dx, a * ay + b * by + cc * cy + d * dy))
    return out


def chaikin(pts, n=4):
    """Rounds the corners, keeps the ends."""
    for _ in range(n):
        o = [pts[0]]
        for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
            o += [(0.75 * x0 + 0.25 * x1, 0.75 * y0 + 0.25 * y1), (0.25 * x0 + 0.75 * x1, 0.25 * y0 + 0.75 * y1)]
        o.append(pts[-1])
        pts = o
    return pts


def sc(pts):
    return [(x * S, y * S) for x, y in pts]


def mirror(pts):
    return [(W - x, y) for x, y in pts]


def lerp_table(table, s):
    for (s0, v0), (s1, v1) in zip(table, table[1:]):
        if s <= s1:
            k = (s - s0) / (s1 - s0)
            k = k * k * (3 - 2 * k)
            return v0 + (v1 - v0) * k
    return table[-1][1]


def tapered(pts, table, scale=1.0):
    """The outline of a stroke along pts whose width follows table (share of the length -> px)."""
    d = [0.0]
    for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
        d.append(d[-1] + math.hypot(x1 - x0, y1 - y0))
    left, right = [], []
    for i, (x, y) in enumerate(pts):
        x0, y0 = pts[max(i - 2, 0)]
        x1, y1 = pts[min(i + 2, len(pts) - 1)]
        l = math.hypot(x1 - x0, y1 - y0) or 1
        nx, ny = -(y1 - y0) / l, (x1 - x0) / l
        w = lerp_table(table, d[i] / d[-1]) * scale / 2
        left.append((x + nx * w, y + ny * w))
        right.append((x - nx * w, y - ny * w))
    return left + right[::-1]


def light(img, mask, color, blur, strength):
    """A soft glow of mask in color over img, then the mask itself."""
    halo = mask.filter(ImageFilter.GaussianBlur(blur * S)).point(lambda v: min(255, int(v * strength)))
    img = Image.composite(Image.new('RGB', img.size, color), img, halo)
    return Image.composite(Image.new('RGB', img.size, color), img, mask)


def line_mask(size, pts, width):
    m = Image.new('L', size, 0)
    ImageDraw.Draw(m).line(sc(pts), fill=255, width=max(1, int(width * S)), joint='curve')
    return m


def main():
    img = Image.new('RGB', (W * S, H * S), (5, 6, 8))
    g = Image.new('L', img.size, 0)   # a faint light in the middle, behind the clock
    ImageDraw.Draw(g).ellipse((120 * S, 40 * S, 680 * S, 420 * S), fill=255)
    img = Image.composite(Image.new('RGB', img.size, (11, 12, 15)), img, g.filter(ImageFilter.GaussianBlur(100 * S)))
    d = ImageDraw.Draw(img)
    for i, y in enumerate((150, 172, 194)):   # the grille's lines behind the clock
        d.line(sc([(250, y), (550, y)]), fill=(22 + i * 3, 25 + i * 3, 30 + i * 3), width=2 * S)
    outer = chaikin(bez(X_OUTER[0], 24) + bez(X_OUTER[1], 24)[1:])
    inner = chaikin(bez(X_INNER[0], 24) + bez(X_INNER[1], 24)[1:])
    drl = chaikin(DRL, 4)
    for side in (1, -1):
        o, i, r = (outer, inner, drl) if side > 0 else (mirror(outer), mirror(inner), mirror(drl))
        # the rim light on the X
        img = light(img, line_mask(img.size, o, 1.2), RIM, 4, 0.7)
        img = Image.composite(Image.new('RGB', img.size, RIM_CORE), img, line_mask(img.size, o, 0.5))
        img = light(img, line_mask(img.size, i, 1.0), (60, 66, 74), 2, 0.4)
        # the running light: a wide soft glow, the orange body, the white-hot core
        body = Image.new('L', img.size, 0)
        ImageDraw.Draw(body).polygon(sc(tapered(r, DRL_W)), fill=255)
        wide = body.filter(ImageFilter.GaussianBlur(18 * S)).point(lambda v: min(255, v * 3))
        img = Image.composite(Image.new('RGB', img.size, (120, 42, 6)), img, wide)
        img = light(img, body, ORANGE, 5, 2.2)
        core = Image.new('L', img.size, 0)
        ImageDraw.Draw(core).polygon(sc(tapered(r, DRL_W, 0.45)), fill=255)
        img = Image.composite(Image.new('RGB', img.size, CORE), img, core.filter(ImageFilter.GaussianBlur(0.4 * S)))
    img = img.resize((W, H), Image.LANCZOS)
    # dither to RGB565: the unit keeps 5/6/5 bits, so pick those levels here with noise instead of bands
    rnd = random.Random(1)
    px = img.load()
    for y in range(H):
        for x in range(W):
            r, g_, b = px[x, y]
            r = min(255, r + rnd.randrange(8)) & 0xF8
            g_ = min(255, g_ + rnd.randrange(4)) & 0xFC
            b = min(255, b + rnd.randrange(8)) & 0xF8
            px[x, y] = (r, g_, b)
    img.save(OUT, optimize=True)
    print('written:', os.path.normpath(OUT), os.path.getsize(OUT), 'bytes')


if __name__ == '__main__':
    main()
