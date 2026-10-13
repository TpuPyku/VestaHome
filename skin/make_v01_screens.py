#!/usr/bin/env python3
"""make_v01_screens.py - every MMC screen in the style of vesta-skin-v0.1.

vesta-skin-v0.1.html draws six screens (main menu x2, radio, player, phone
keypad, display settings). The shell has many more (see _ai/all_screens.png
and mmc_screens/). This script draws the rest with the same tokens, triangle
patterns and line icons: the <head> and the shared <defs> are taken from
vesta-skin-v0.1.html as is, extra icons are appended to the defs.

    python skin/make_v01_screens.py      -> skin/vesta-skin-v0.1-screens.html

The PDF is printed from the HTML with headless Edge (see PROJECT.md).
"""
import os
import re
from html import escape

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "vesta-skin-v0.1.html")
OUT = os.path.join(HERE, "vesta-skin-v0.1-screens.html")

# ---- tokens (vesta-skin-v0.1.html :root) ---------------------------------
INK, PANEL, PANEL2, LINE = "#0b0c0e", "#17191c", "#22252a", "#34383f"
TEXT, MUTED, DIM, OFF = "#f3f1ec", "#a3a8b0", "#5d636c", "#3d424a"
OR, FACET, OK, STOP = "#ff6b1a", "#c24a0b", "#2fbf71", "#e8483b"
# Shell text has three state colours (G3NavHMI.exe ARGB literals): normal
# 0xFFB3F2BE, active 0xFF5A5A5A, inactive (probably) 0xFFA4A89F. The skin
# uses them as TEXT / ON_TEXT / DIM; ON_TEXT is dark, so the v0.1 rule holds:
# pressed = solid orange fill with dark text. Stock 0x5A5A5A on #ff6b1a is only
# ~2.4:1, hence the patch of the active colour to INK (see PROJECT.md).
ACT = OR
ON_TEXT = INK

# ---- extra icons, 48 grid, 3.5 px line like the v0.1 set -----------------
_S = 'fill="none" stroke="currentColor" stroke-width="3.5" stroke-linecap="round" stroke-linejoin="round"'
ICONS = {
    "folder": '<path d="M5 13 a3 3 0 0 1 3 -3 h10 l4 5 h18 a3 3 0 0 1 3 3 v19 a3 3 0 0 1 -3 3 H8 a3 3 0 0 1 -3 -3 Z" %s/>' % _S,
    "up": '<path d="M9 30 L24 15 L39 30" fill="none" stroke="currentColor" stroke-width="4" stroke-linecap="round" stroke-linejoin="round"/>',
    "down": '<path d="M9 18 L24 33 L39 18" fill="none" stroke="currentColor" stroke-width="4" stroke-linecap="round" stroke-linejoin="round"/>',
    "star": '<path d="M24 5 L29.5 17.5 L43 18.5 L32.5 27.5 L36 41 L24 33.5 L12 41 L15.5 27.5 L5 18.5 L18.5 17.5 Z" %s/>' % _S,
    "star-f": '<path d="M24 5 L29.5 17.5 L43 18.5 L32.5 27.5 L36 41 L24 33.5 L12 41 L15.5 27.5 L5 18.5 L18.5 17.5 Z" fill="currentColor" stroke="currentColor" stroke-width="3.5" stroke-linejoin="round"/>',
    "user": '<g %s><circle cx="24" cy="16" r="8"/><path d="M8 42 a16 14 0 0 1 32 0"/></g>' % _S,
    "users": '<g %s><circle cx="18" cy="17" r="7"/><path d="M4 41 a14 12 0 0 1 28 0"/><path d="M30 10.5 a7 7 0 0 1 0 13 M36 30 a14 12 0 0 1 8 11"/></g>' % _S,
    "clock": '<g %s><circle cx="24" cy="24" r="18"/><path d="M24 13 V24 L32 29"/></g>' % _S,
    "keypad": '<g fill="currentColor"><circle cx="13" cy="10" r="3.5"/><circle cx="24" cy="10" r="3.5"/><circle cx="35" cy="10" r="3.5"/><circle cx="13" cy="21" r="3.5"/><circle cx="24" cy="21" r="3.5"/><circle cx="35" cy="21" r="3.5"/><circle cx="13" cy="32" r="3.5"/><circle cx="24" cy="32" r="3.5"/><circle cx="35" cy="32" r="3.5"/><circle cx="24" cy="42" r="3.5"/></g>',
    "mic": '<g %s><rect x="17" y="5" width="14" height="24" rx="7"/><path d="M10 23 a14 14 0 0 0 28 0 M24 37 V43 M17 43 h14"/></g>' % _S,
    "mic-off": '<g %s><rect x="17" y="5" width="14" height="24" rx="7"/><path d="M10 23 a14 14 0 0 0 28 0 M24 37 V43 M17 43 h14"/><path d="M7 7 L41 41"/></g>' % _S,
    "handover": '<g %s><rect x="6" y="6" width="18" height="34" rx="3"/><path d="M12 34 h6"/><path d="M30 23 h13 M37 17 l6 6 -6 6"/></g>' % _S,
    "play": '<path d="M14 8 L40 24 L14 40 Z" fill="currentColor"/>',
    "rot-l": '<g %s><path d="M12 22 a14 14 0 1 1 4 14"/><path d="M5 15 l7 7 7 -7"/></g>' % _S,
    "rot-r": '<g %s><path d="M36 22 a14 14 0 1 0 -4 14"/><path d="M43 15 l-7 7 -7 -7"/></g>' % _S,
    "info": '<g %s><circle cx="24" cy="24" r="18"/><path d="M24 22 V34"/><circle cx="24" cy="15" r="1.2" fill="currentColor"/></g>' % _S,
    "warn": '<g %s><path d="M24 6 L44 40 H4 Z"/><path d="M24 19 V29"/><circle cx="24" cy="34.5" r="1.2" fill="currentColor"/></g>' % _S,
    "search": '<g %s><circle cx="21" cy="21" r="13"/><path d="M31 31 L42 42"/></g>' % _S,
    "plus": '<path d="M24 9 V39 M9 24 H39" %s/>' % _S,
    "minus": '<path d="M9 24 H39" %s/>' % _S,
    "x": '<path d="M12 12 L36 36 M36 12 L12 36" %s/>' % _S,
    "sun": '<g %s><circle cx="24" cy="24" r="8"/><path d="M24 4 v5 M24 39 v5 M4 24 h5 M39 24 h5 M10 10 l3.5 3.5 M34.5 34.5 l3.5 3.5 M10 38 l3.5 -3.5 M34.5 13.5 l3.5 -3.5"/></g>' % _S,
    "call-in": '<g %s><path d="M38 10 L18 30 M18 16 V30 H32"/></g>' % _S,
    "call-out": '<g %s><path d="M14 34 L34 14 M20 14 H34 V28"/></g>' % _S,
    "call-miss": '<g %s><path d="M6 16 L20 30 L30 20 L42 32 M6 16 h9 M6 16 v9"/></g>' % _S,
    "lock": '<g %s><rect x="10" y="21" width="28" height="21" rx="3"/><path d="M16 21 v-6 a8 8 0 0 1 16 0 v6"/></g>' % _S,
    "refresh": '<g %s><path d="M39 20 a16 16 0 0 0 -29 -4 M9 28 a16 16 0 0 0 29 4"/><path d="M39 8 v12 h-12 M9 40 v-12 h12"/></g>' % _S,
    "check": '<path d="M9 25 L19 35 L39 13" %s/>' % _S,
    "eq": '<g %s><path d="M12 6 V42 M24 6 V42 M36 6 V42"/><rect x="7" y="26" width="10" height="7" rx="2" fill="currentColor"/><rect x="19" y="12" width="10" height="7" rx="2" fill="currentColor"/><rect x="31" y="30" width="10" height="7" rx="2" fill="currentColor"/></g>' % _S,
    "globe": '<g %s><circle cx="24" cy="24" r="18"/><path d="M6 24 h36 M24 6 a28 28 0 0 1 0 36 M24 6 a28 28 0 0 0 0 36"/></g>' % _S,
    "gps": '<g %s><circle cx="24" cy="24" r="12"/><circle cx="24" cy="24" r="3" fill="currentColor"/><path d="M24 4 v8 M24 36 v8 M4 24 h8 M36 24 h8"/></g>' % _S,
    "apps": '<g %s><rect x="7" y="7" width="14" height="14" rx="3"/><rect x="27" y="7" width="14" height="14" rx="3"/><rect x="7" y="27" width="14" height="14" rx="3"/><rect x="27" y="27" width="14" height="14" rx="3"/></g>' % _S,
    "camera": '<g %s><path d="M6 16 a3 3 0 0 1 3 -3 h6 l3 -5 h12 l3 5 h6 a3 3 0 0 1 3 3 v20 a3 3 0 0 1 -3 3 H9 a3 3 0 0 1 -3 -3 Z"/><circle cx="24" cy="25" r="7.5"/></g>' % _S,
    "edit": '<g %s><path d="M30 8 L40 18 L18 40 H8 V30 Z M26 12 L36 22"/></g>' % _S,
    "disc": '<g %s><circle cx="24" cy="24" r="18"/><circle cx="24" cy="24" r="5"/></g>' % _S,
    "signal": '<g fill="currentColor"><rect x="6" y="32" width="7" height="10" rx="1.5"/><rect x="17" y="24" width="7" height="18" rx="1.5"/><rect x="28" y="15" width="7" height="27" rx="1.5"/><rect x="39" y="6" width="5" height="36" rx="1.5" opacity=".35"/></g>',
    "eye": '<g %s><path d="M4 24 C 12 12, 36 12, 44 24 C 36 36, 12 36, 4 24 Z"/><circle cx="24" cy="24" r="6"/></g>' % _S,
    "caps": '<path d="M24 7 L41 25 H32 V40 H16 V25 H7 Z" %s/>' % _S,
    "enter": '<g %s><path d="M40 10 V26 H10"/><path d="M17 19 l-7 7 7 7"/></g>' % _S,
    "film": '<g %s><rect x="6" y="8" width="36" height="32" rx="3"/><path d="M14 8 V40 M34 8 V40 M6 18 h8 M6 30 h8 M34 18 h8 M34 30 h8"/></g>' % _S,
    "car": '<g %s><path d="M14 6 h20 a6 6 0 0 1 6 6 v24 a6 6 0 0 1 -6 6 H14 a6 6 0 0 1 -6 -6 V12 a6 6 0 0 1 6 -6 Z"/><path d="M13 16 h22 M13 33 h22"/></g>' % _S,
    "power": '<g %s><path d="M16 12 a15 15 0 1 0 16 0 M24 5 V23"/></g>' % _S,
}


def extra_defs():
    return "\n".join('    <symbol id="i-%s" viewBox="0 0 48 48">%s</symbol>' % (k, v) for k, v in ICONS.items())


# ---- drawing helpers (return SVG text) -----------------------------------

def e(s):
    return escape(s, quote=True)


def t(x, y, s, size=18, fill=TEXT, weight=600, anchor="start", cls="t-disp", ls=None, extra=""):
    a = ' text-anchor="%s"' % anchor if anchor != "start" else ""
    l = ' letter-spacing="%s"' % ls if ls else ""
    return '<text x="%g" y="%g"%s class="%s" font-size="%g" font-weight="%s"%s fill="%s"%s>%s</text>' % (
        x, y, a, cls, size, weight, l, fill, extra, e(s))


def icon(name, x, y, s, color=TEXT, extra=""):
    return '<use href="#i-%s" x="%g" y="%g" width="%g" height="%g" color="%s"%s/>' % (name, x, y, s, s, color, extra)


def rect(x, y, w, h, fill=PANEL, rx=10, stroke=None, dash=False, extra=""):
    st = ' stroke="%s" stroke-width="2"' % stroke if stroke else ""
    if dash:
        st += ' stroke-dasharray="6 6"'
    out = '<rect x="%g" y="%g" width="%g" height="%g" rx="%g" fill="%s"%s%s/>' % (x, y, w, h, rx, fill, st, extra)
    return out


def panel(x, y, w, h, rx=14):
    return rect(x, y, w, h, PANEL, rx, LINE)


def sep(x, y, w):
    return rect(x, y, w, 2, LINE, 0)


def button(x, y, w, h, label=None, state="n", ic=None, size=18, isz=None):
    """States: n - outlined, sel - selected source (#22252a, orange text),
    act - pressed/active (orange fill, dark sign), dis - unavailable,
    pri - the one main action of a dialog (orange fill)."""
    if state in ("act", "pri"):
        out, fg = rect(x, y, w, h, ACT, 10), ON_TEXT
    elif state == "sel":
        # the shell has no separate "selected" text colour: selected = active
        out, fg = rect(x, y, w, h, ACT, 10), ON_TEXT
    elif state == "dis":
        out, fg = rect(x, y, w, h, "#111316", 10, PANEL2), OFF
    elif state == "fill":
        out, fg = rect(x, y, w, h, PANEL, 10, LINE), TEXT
    else:
        out, fg = rect(x, y, w, h, "none", 10, LINE), MUTED if label else TEXT
    if ic and label:
        isz = isz or min(32, h - 20)
        tw = len(label) * size * 0.62
        x0 = x + (w - isz - 10 - tw) / 2
        out += icon(ic, x0, y + (h - isz) / 2, isz, fg)
        out += t(x0 + isz + 10, y + h / 2 + size * 0.36, label, size, fg, 700, ls=1.5)
    elif ic:
        isz = isz or min(44, h - 14)
        out += icon(ic, x + (w - isz) / 2, y + (h - isz) / 2, isz, fg)
    elif label:
        out += t(x + w / 2, y + h / 2 + size * 0.36, label, size, fg, 700, "middle", ls=1.5)
    return out


def toggle(x, y, on=True):
    if on:
        return rect(x, y, 72, 36, OR, 18) + '<circle cx="%g" cy="%g" r="14" fill="%s"/>' % (x + 54, y + 18, INK)
    return rect(x, y, 72, 36, PANEL2, 18, LINE) + '<circle cx="%g" cy="%g" r="13" fill="%s"/>' % (x + 18, y + 18, DIM)


def seg(x, y, w, h, opts, idx, size=17):
    out = rect(x, y, w, h, INK, 10, LINE)
    cw = (w - 4) / float(len(opts))
    for i, o in enumerate(opts):
        cx = x + 2 + i * cw
        if i == idx:
            out += rect(cx, y + 2, cw, h - 4, ACT, 8)
        out += t(cx + cw / 2, y + h / 2 + size * 0.36, o, size, ON_TEXT if i == idx else MUTED, 700, "middle")
    return out


def slider(x, y, w, frac, val=None):
    out = rect(x, y, w, 8, LINE, 4) + rect(x, y, max(8, w * frac), 8, OR, 4)
    out += '<circle cx="%g" cy="%g" r="13" fill="%s"/>' % (x + w * frac, y + 4, TEXT)
    if val is not None:
        out += t(x + w + 64, y + 12, val, 22, TEXT, 700, "end")
    return out


def vslider(x, y, h, frac, label, val):
    out = rect(x - 4, y, 8, h, LINE, 4) + rect(x - 4, y + h * (1 - frac), 8, h * frac, OR, 4)
    out += '<circle cx="%g" cy="%g" r="13" fill="%s"/>' % (x, y + h * (1 - frac), TEXT)
    out += t(x, y - 14, val, 16, TEXT, 700, "middle")
    out += t(x, y + h + 28, label, 14, MUTED, 600, "middle", ls=1)
    return out


def stepper(x, y, w, value, h=48, size=20):
    out = rect(x, y, w, h, INK, 10, LINE)
    out += icon("back", x + 6, y + (h - 32) / 2, 32, TEXT) + icon("next", x + w - 38, y + (h - 32) / 2, 32, TEXT)
    out += t(x + w / 2, y + h / 2 + size * 0.36, value, size, TEXT, 700, "middle")
    return out


def chevron(x, y):
    return icon("next", x, y, 32, MUTED)


def scrollbar(x, y, h, pos, ln):
    return rect(x, y, 6, h, LINE, 3) + rect(x, y + h * pos, 6, h * ln, DIM, 3)


def avatar(cx, cy, r, initials, size=None):
    size = size or r * 0.8
    return ('<circle cx="%g" cy="%g" r="%g" fill="%s"/>' % (cx, cy, r, PANEL2)
            + t(cx, cy + size * 0.36, initials, size, MUTED, 700, "middle"))


def avatar_or(cx, cy, r, initials):
    return ('<circle cx="%g" cy="%g" r="%g" fill="url(#tri-or)"/>' % (cx, cy, r)
            + '<circle cx="%g" cy="%g" r="%g" fill="none" stroke="%s" stroke-width="2"/>' % (cx, cy, r, LINE)
            + t(cx, cy + r * 0.3, initials, r * 0.8, INK, 700, "middle", extra=' opacity=".75"'))


def dock(items):
    """Bottom bar of v0.1 (y 398..480). items = svg strings."""
    return rect(0, 398, 800, 82, INK, 0) + sep(0, 398, 800) + "".join(items)


def dock_sources(active, extra=()):
    out = []
    for i, s in enumerate(["USB", "SD", "BT", "AUX"]):
        out.append(button(16 + i * 120, 410, 112, 58, s, "sel" if s == active else "n"))
    return dock(out + list(extra))


# ---- screen frame --------------------------------------------------------

def frame(title, body, back=True, label=""):
    head = [rect(0, 0, 800, 480, INK, 0), rect(0, 56, 800, 424, "url(#tri)", 0), '<use href="#statusbar"/>']
    if back:
        head.append(rect(8, 8, 48, 40, PANEL, 8))
        head.append(icon("back", 14, 10, 36, TEXT))
        head.append(t(72, 37, title, 22, TEXT, 600, ls=2.5))
    elif title:
        head.append(t(20, 37, title, 22, TEXT, 600, ls=2.5))
    return ('<svg class="screen" viewBox="0 0 800 480" role="img" aria-label="%s">\n' % e(label or title)
            + "\n".join(head + body) + "\n</svg>")


def list_row(x, y, w, h, text_, right=None, state="n", ic=None, sub=None, num=None, rcolor=MUTED, icolor=None):
    """List row: one tap zone >= 56 px. state: n / cur (orange fill) / dim."""
    fill = ACT if state == "cur" else PANEL
    fg = ON_TEXT if state == "cur" else (OFF if state == "dim" else TEXT)
    mg = ON_TEXT if state == "cur" else MUTED
    out = rect(x, y, w, h, fill, 10)
    tx = x + 16
    if num is not None:
        out += t(tx, y + h / 2 + 6, num, 16, mg if state == "cur" else DIM, 700)
        tx += 40
    if ic:
        out += icon(ic, tx, y + (h - 30) / 2, 30, icolor if (icolor and state != "cur") else (fg if state == "cur" else MUTED))
        tx += 44
    if sub:
        out += t(tx, y + h / 2 - 3, text_, 19, fg, 700, cls="t-body")
        out += t(tx, y + h / 2 + 19, sub, 15, mg, 400, cls="t-body")
    else:
        out += t(tx, y + h / 2 + 7, text_, 19, fg, 700, cls="t-body")
    if right:
        out += t(x + w - 16, y + h / 2 + 6, right, 16, ON_TEXT if state == "cur" else rcolor, 600, "end", cls="t-body")
    return out


def settings_nav(active):
    """8 sections as a 2x4 grid of 102x92 tiles: the v0.1 list of six rows
    does not fit eight sections at 56 px each."""
    items = [("gear", "ОБЩИЕ"), ("vol", "ЗВУК"), ("moon", "ЭКРАН"), ("radio", "РАДИО"),
             ("bt", "ТЕЛЕФОН"), ("wifi", "WI-FI"), ("video", "ВИДЕО"), ("photo", "ФОТО")]
    out = []
    for i, (ic, lab) in enumerate(items):
        x, y = 16 + (i % 2) * 110, 72 + (i // 2) * 98
        on = lab == active
        out.append(rect(x, y, 102, 92, ACT if on else PANEL, 10))
        out.append(icon(ic, x + 33, y + 12, 36, ON_TEXT if on else MUTED))
        out.append(t(x + 51, y + 76, lab, 13, ON_TEXT if on else MUTED, 700, "middle", ls=1))
    return out


def set_row(y, label, right_svg="", h=64, sub=None, last=False):
    out = t(264, y + h / 2 + (0 if sub else 7), label, 19, TEXT, 700, cls="t-body")
    if sub:
        out += t(264, y + h / 2 + 20, sub, 15, MUTED, 400, cls="t-body")
    out += right_svg
    if not last:
        out += sep(256, y + h - 1, 512)
    return out


def phone_nav(active):
    items = [("keypad", "НАБОР"), ("users", "КОНТАКТЫ"), ("star", "ИЗБРАННОЕ"), ("clock", "ЖУРНАЛ"), ("bt", "МУЗЫКА")]
    out = []
    for i, (ic, lab) in enumerate(items):
        y = 68 + i * 78
        on = lab == active
        out.append(rect(16, y, 160, 70, ACT if on else PANEL, 10))
        out.append(icon(ic, 30, y + 19, 32, ON_TEXT if on else MUTED))
        out.append(t(72, y + 41, lab, 13, ON_TEXT if on else MUTED, 700, ls=1))
    return out


# ---- placeholders for picture content -----------------------------------

def road(x, y, w, h):
    """Night road frame for the video player (content, not skin)."""
    hz = y + h * 0.45
    cx = x + w / 2
    return ('<g><rect x="%g" y="%g" width="%g" height="%g" fill="#141a26"/>' % (x, y, w, h)
            + '<rect x="%g" y="%g" width="%g" height="%g" fill="#101114"/>' % (x, hz, w, y + h - hz)
            + '<path d="M%g %g L%g %g L%g %g L%g %g Z" fill="#1f2125"/>' % (cx - 40, hz, cx + 40, hz, x + w * 0.95, y + h, x + w * 0.05, y + h)
            + '<path d="M%g %g L%g %g L%g %g L%g %g Z" fill="#c9bf8f" opacity=".8"/>' % (cx - 2, hz + 8, cx + 2, hz + 8, cx + 7, y + h, cx - 7, y + h)
            + '<path d="M%g %g L%g %g L%g %g L%g %g Z" fill="#0d1018"/>' % (x, hz, x + w * 0.2, hz - 40, x + w * 0.42, hz, x, hz + 10)
            + '<path d="M%g %g L%g %g L%g %g Z" fill="#0d1018"/>' % (x + w * 0.58, hz, x + w * 0.8, hz - 50, x + w, hz)
            + '<circle cx="%g" cy="%g" r="4" fill="#f3e3b0" opacity=".7"/></g>' % (cx + 6, hz + 4))


def landscape(x, y, w, h):
    """Photo placeholder (content): mountains at sunset in muted tones."""
    hz = y + h * 0.62
    return ('<g><rect x="%g" y="%g" width="%g" height="%g" fill="#3a3f4a"/>' % (x, y, w, h)
            + '<rect x="%g" y="%g" width="%g" height="%g" fill="#4c4a4c"/>' % (x, y + h * 0.35, w, h * 0.3)
            + '<circle cx="%g" cy="%g" r="%g" fill="#e7a36b"/>' % (x + w * 0.7, y + h * 0.36, h * 0.09)
            + '<path d="M%g %g L%g %g L%g %g L%g %g L%g %g L%g %g Z" fill="#262a33"/>' % (
                x, hz, x + w * 0.25, y + h * 0.3, x + w * 0.45, y + h * 0.5, x + w * 0.62, y + h * 0.26, x + w, hz, x, hz)
            + '<rect x="%g" y="%g" width="%g" height="%g" fill="#1d2a22"/>' % (x, hz, w, y + h - hz)
            + '<path d="M%g %g L%g %g L%g %g Z" fill="#5b5446"/></g>' % (x + w * 0.45, y + h, x + w * 0.52, hz, x + w * 0.6, y + h))


def thumb(x, y, w, h, kind):
    return road(x, y, w, h) if kind == "video" else landscape(x, y, w, h)


# ---- screens -------------------------------------------------------------
# Each returns (key, title, chip, caption, svg). chip: now / patch / own.

def s_main_p2():
    b = [rect(0, 56, 800, 424, "url(#tri)", 0, extra=' opacity=".9"'), rect(0, 56, 800, 300, "url(#fade-band)", 0),
         rect(0, 404, 800, 76, "url(#tri-or)", 0), rect(0, 404, 800, 76, "url(#fade-up)", 0, extra=' opacity=".55"'),
         rect(0, 402, 800, 3, OR, 0), icon("back", 4, 200, 40, TEXT), icon("next", 756, 200, 40, DIM)]
    for i, (ic, lab) in enumerate([("video", "ВИДЕО"), ("photo", "ФОТО"), ("aux", "AUX")]):
        gx = 23 + 195 * i
        b.append('<g transform="translate(%d 120)">' % gx + rect(6, 6, 166, 166, PANEL, 18, LINE)
                 + '<path d="M126 6 H154 a18 18 0 0 1 18 18 V52 Z" fill="%s"/>' % FACET
                 + icon(ic, 51, 43, 76, TEXT) + t(89, 152, lab, 17, MUTED, 600, "middle", ls=2) + "</g>")
    b.append('<g transform="translate(608 120)">' + rect(6, 6, 166, 166, "none", 18, PANEL2, dash=True) + "</g>")
    b += [rect(372, 336, 20, 6, DIM, 3), rect(400, 336, 28, 6, OR, 3),
          t(24, 450, "Европа Плюс · 106.2 FM", 17, TEXT, 700, cls="t-body"),
          t(776, 450, "Громкость 18", 17, TEXT, 400, "end", cls="t-body")]
    svg = frame("", [], back=False, label="Главное меню, страница 2: Видео, Фото, AUX")
    svg = svg.replace('<use href="#statusbar"/>', '<use href="#statusbar"/>\n' + "\n".join(b) + "\n"
                      + t(20, 37, "ГЛАВНОЕ МЕНЮ", 22, TEXT, 600, ls=2.5), 1)
    return ("main2", "Главное меню, стр. 2", "now",
            "Вторая страница штатной сетки (<code>main_folders_second_screen.png</code>): три плитки, четвёртое место пустое. Точка страницы справа активна, стрелка вправо погашена. Плитки — те же <code>main_symbol_video/photo/aux.png</code> 187×187.", svg)


def s_apps():
    tiles = [("camera", "СКРИНШОТ", "n"), ("map", "YAMAPS", "act"), ("map", "OSMAPS", "n"), ("folder", "ФАЙЛЫ", "n"),
             ("nav", "НАВИТЕЛ", "n"), ("gps", "GPS ТЕСТ", "n"), ("apps", "ПРОГРАММЫ", "n")]
    b = []
    for i, (ic, lab, st) in enumerate(tiles):
        x, y = 16 + (i % 4) * 194, 72 + (i // 4) * 162
        on = st == "act"
        b.append(rect(x, y, 184, 150, ACT if on else PANEL, 14, None if on else LINE))
        if not on and i in (1, 2):
            b.append('<path d="M%g %g H%g a14 14 0 0 1 14 14 V%g Z" fill="%s"/>' % (x + 148, y, x + 170, y + 36, FACET))
        b.append(icon(ic, x + 64, y + 26, 56, ON_TEXT if on else TEXT))
        b.append(t(x + 92, y + 124, lab, 15, ON_TEXT if on else MUTED, 700 if on else 600, "middle", ls=1.5))
    x, y = 16 + 3 * 194, 72 + 162
    b.append(rect(x, y, 184, 150, "none", 14, LINE, dash=True))
    b.append(icon("plus", x + 68, y + 30, 48, DIM))
    b.append(t(x + 92, y + 124, "ДОБАВИТЬ", 15, DIM, 600, "middle", ls=1.5))
    b.append(dock([button(16, 410, 184, 58, "ИЗМЕНИТЬ", "n", "edit", 17), rect(360, 432, 28, 6, OR, 3), rect(396, 432, 20, 6, DIM, 3),
                   button(600, 410, 184, 58, "ГЛАВНОЕ", "n", "home", 17)]))
    return ("apps", "Программы (VestaLaunch)", "own",
            "Своя программа вместо AppLauncher: плитки 184×150 той же формы, что в главном меню. Скошенный угол — у ярлыков карт (YaMapsCE, OSMapsCE). Пустой слот пунктиром, «Изменить» открывает редактор ярлыков.",
            frame("ПРОГРАММЫ", b))


def radio_dock(list_on=False):
    return dock([button(16, 410, 96, 58, "FM", "sel"), button(120, 410, 96, 58, "AM"),
                 icon("prev", 268, 417, 44, TEXT), button(344, 410, 112, 58, "ПОИСК", "n"), icon("fwd", 488, 417, 44, TEXT),
                 rect(590, 410, 64, 58, PANEL2, 10) + icon("list", 600, 417, 44, OR) if list_on else icon("list", 600, 417, 44, TEXT),
                 icon("gear", 712, 417, 44, TEXT)])


def s_radio_list():
    st = [("87.9", "Радио Шансон", "RDS"), ("88.3", "Ретро FM", ""), ("99.2", "Вести FM", "RDS"),
          ("101.7", "Хит FM", "RDS"), ("106.2", "Европа Плюс", "RDS"), ("107.4", "Рок", "")]
    b = [panel(16, 68, 600, 322)]
    for i, (f, n, r) in enumerate(st[:5]):
        y = 76 + i * 62
        cur = f == "106.2"
        b.append(list_row(24, y, 568, 56, "", r, "cur" if cur else "n"))
        b.append(t(40, y + 38, f, 26, ON_TEXT if cur else TEXT, 700))
        b.append(t(148, y + 36, n, 19, ON_TEXT if cur else MUTED, 700, cls="t-body"))
    b.append(scrollbar(600, 80, 296, 0.1, 0.55))
    b += [button(632, 68, 152, 72, "АВТО", "n", "search", 17), button(632, 150, 152, 72, "PTY", "n", None, 18),
          rect(632, 232, 152, 158, PANEL, 12), t(708, 280, "НАЙДЕНО", 13, MUTED, 600, "middle", ls=2),
          t(708, 330, "18", 44, TEXT, 700, "middle"), t(708, 362, "СТАНЦИЙ", 13, MUTED, 600, "middle", ls=2)]
    b.append(radio_dock(True))
    return ("radio_list", "Радио: список станций", "now",
            "Список вместо окна частоты, штатная панель 562×448 (<code>Temp/radio/list_station/*</code>). Строка 56 px, текущая станция — оранжевая заливка. Справа автопоиск и фильтр PTY, кнопка списка в доке подсвечена как выбранная.",
            frame("РАДИО · СПИСОК", b))


def s_radio_pty():
    types = ["ВСЕ", "НОВОСТИ", "СОБЫТИЯ", "ИНФОРМАЦИЯ", "СПОРТ", "ОБРАЗОВАНИЕ", "ДРАМА", "КУЛЬТУРА", "НАУКА",
             "РАЗНОЕ", "ПОП", "РОК", "ЛЁГКАЯ", "КЛАССИКА", "ДРУГАЯ"]
    b = []
    for i, s in enumerate(types):
        x, y = 16 + (i % 3) * 260, 68 + (i // 3) * 64
        b.append(button(x, y, 248, 56, s, "act" if s == "РОК" else "fill", None, 17))
    b.append(dock([button(16, 410, 180, 58, "ОТМЕНА", "n"), t(400, 446, "Ищет станции с типом «Рок»", 17, MUTED, 400, "middle", cls="t-body"),
                   button(604, 410, 180, 58, "ИСКАТЬ", "pri", "search", 18)]))
    return ("radio_pty", "Радио: тип программы (PTY)", "patch",
            "Сетка 3×5 вместо длинного списка: все типы видны сразу, кнопки 248×56. Выбранный тип — заливка; поиск запускает единственная оранжевая кнопка диалога.",
            frame("РАДИО · ТИП ПРОГРАММЫ", b))


def media_tabs(active, tabs):
    out = []
    for i, (ic, lab) in enumerate(tabs):
        y = 68 + i * 82
        on = lab == active
        out.append(rect(16, y, 160, 74, ACT if on else PANEL, 10))
        out.append(icon(ic, 30, y + 21, 32, ON_TEXT if on else MUTED))
        out.append(t(72, y + 43, lab, 13, ON_TEXT if on else MUTED, 700, ls=1))
    return out


def s_audio_list():
    b = media_tabs("ПАПКИ", [("folder", "ПАПКИ"), ("user", "АРТИСТЫ"), ("disc", "АЛЬБОМЫ"), ("list", "ТРЕКИ")])
    b.append(panel(192, 68, 592, 322))
    rows = [("..", None, "folder", "n", None), ("Группа крови", "4:46", None, "cur", "3"), ("Закрой за мной дверь", "4:15", None, "n", "4"),
            ("Кукушка", "6:37", None, "n", "5"), ("Пачка сигарет", "4:28", None, "n", "6")]
    for i, (n, r, ic, st, num) in enumerate(rows):
        y = 76 + i * 62
        if ic:
            b.append(list_row(200, y, 560, 56, "Наверх · /Music/Кино", None, st, "folder"))
        else:
            b.append(list_row(200, y, 560, 56, n, r, st, None, num=num))
    b.append(icon("vol", 646, 76 + 62 + 13, 30, ON_TEXT))
    b.append(scrollbar(768, 80, 296, 0.0, 0.45))
    b.append(dock_sources("USB", [icon("music", 600, 417, 44, TEXT), icon("gear", 712, 417, 44, TEXT)]))
    return ("audio_list", "Аудио: список (папки)", "patch",
            "Слева режимы списка (<code>Playlist/mode_*</code>: папки, артисты, альбомы, треки), справа список 592×322. Играющий трек — оранжевая строка со значком громкости. Первая строка «наверх» вместо отдельной кнопки родительской папки.",
            frame("АУДИО · USB · КИНО", b))


def s_audio_bt():
    b = [rect(24, 76, 236, 236, "url(#tri-or)", 14), rect(24, 76, 236, 236, "none", 14, LINE),
         icon("bt", 92, 144, 100, INK, ' opacity=".55"'),
         t(290, 112, "BLUETOOTH · REDMI NOTE", 14, MUTED, 600, ls=2),
         t(290, 156, "Звезда по имени Солнце", 36, TEXT, 700), t(290, 190, "Кино · «Звезда по имени Солнце»", 20, MUTED, 400, cls="t-body"),
         rect(290, 232, 486, 8, LINE, 4), rect(290, 232, 120, 8, OR, 4), '<circle cx="410" cy="236" r="11" fill="%s"/>' % TEXT,
         t(290, 270, "1:05", 17, MUTED), t(776, 270, "3:45", 17, MUTED, 600, "end"),
         icon("prev", 398, 304, 48, TEXT), '<circle cx="533" cy="328" r="44" fill="%s"/>' % OR, icon("pause", 509, 304, 48, INK),
         icon("fwd", 620, 304, 48, TEXT)]
    b.append(dock_sources("BT", [icon("phone", 600, 417, 44, TEXT), icon("gear", 712, 417, 44, TEXT)]))
    return ("audio_bt", "Аудио по Bluetooth", "now",
            "Та же сетка, что у плеера USB: обложку заменяет знак Bluetooth на оранжевом паттерне (картинки обложки по A2DP нет). Нет перемешивания и повтора — их держит телефон. В доке выбран источник BT.",
            frame("АУДИО · BLUETOOTH", b))


def s_video_player():
    b = [road(0, 56, 800, 424), rect(0, 56, 800, 64, "url(#fade-band)", 0, extra=' opacity=".9"'),
         rect(0, 330, 800, 150, INK, 0, extra=' opacity=".82"'),
         t(16, 368, "1:12", 17, MUTED), t(784, 368, "3:45", 17, MUTED, 600, "end"),
         rect(80, 360, 640, 8, LINE, 4), rect(80, 360, 200, 8, OR, 4), '<circle cx="280" cy="364" r="11" fill="%s"/>' % TEXT,
         icon("list", 24, 404, 44, TEXT), icon("prev", 228, 400, 48, TEXT),
         '<circle cx="400" cy="424" r="40" fill="%s"/>' % OR, icon("pause", 378, 402, 44, INK),
         icon("fwd", 524, 400, 48, TEXT), icon("repeat", 630, 404, 40, OR), icon("gear", 726, 402, 44, TEXT)]
    return ("video_player", "Видео: воспроизведение", "now",
            "Кадр на весь экран, панель управления выезжает по касанию и прячется через 5 с. Панель — графит 82 % поверх кадра, одна главная кнопка «пауза». Повтор включён — значок оранжевый. Файлы: <code>Temp/media/video/*</code>, <code>Temp/media/common/*</code>.",
            frame("ВИДЕО · DASHCAM_0412.MP4", b))


def s_video_list():
    b = media_tabs("ФАЙЛЫ", [("folder", "ФАЙЛЫ"), ("clock", "НЕДАВНИЕ")])
    b.append(panel(192, 68, 592, 322))
    vids = [("DASHCAM_0410.MP4", "2:58 · 180 МБ"), ("DASHCAM_0411.MP4", "3:00 · 182 МБ"),
            ("DASHCAM_0412.MP4", "3:45 · 226 МБ"), ("ROAD_TRIP_SOCHI.AVI", "41:20 · 1,4 ГБ")]
    for i, (n, sub) in enumerate(vids):
        y = 76 + i * 76
        cur = i == 2
        b.append(rect(200, y, 560, 70, ACT if cur else PANEL, 10))
        b.append(thumb(208, y + 7, 100, 56, "video"))
        b.append(icon("play", 244, y + 21, 28, TEXT, ' opacity=".8"'))
        b.append(t(324, y + 31, n, 19, ON_TEXT if cur else TEXT, 700, cls="t-body"))
        b.append(t(324, y + 54, sub, 15, ON_TEXT if cur else MUTED, 400, cls="t-body"))
    b.append(scrollbar(768, 80, 296, 0.2, 0.5))
    b.append(dock_sources("USB", [icon("video", 600, 417, 44, TEXT), icon("gear", 712, 417, 44, TEXT)]))
    return ("video_list", "Видео: список файлов", "patch",
            "Строка 70 px с кадром-превью 100×56, имя и под ним длительность с размером. Последний открытый файл — оранжевая строка. Источник выбирается в доке, как у аудио.",
            frame("ВИДЕО · USB · DASHCAM", b))


def photo_dock():
    return dock([icon("apps", 24, 417, 44, TEXT), icon("prev", 132, 417, 44, TEXT), icon("rot-l", 244, 417, 44, TEXT),
                 rect(330, 410, 140, 58, PANEL2, 10) + icon("play", 378, 417, 44, OR),
                 icon("rot-r", 512, 417, 44, TEXT), icon("fwd", 624, 417, 44, TEXT), icon("gear", 732, 417, 44, TEXT)])


def s_photo_view():
    b = [rect(0, 56, 800, 342, "#08090a", 0), landscape(172, 56, 456, 342), photo_dock()]
    return ("photo_view", "Фото: просмотр", "now",
            "Снимок вписан между шапкой и доком (456×342 для 4:3), поля графитовые. В доке: сетка, назад, поворот влево и вправо, слайд-шоу (идёт — выбрано), вперёд, настройки. Файлы: <code>Temp/media/photo/*</code>.",
            frame("ФОТО · 3 / 24", b))


def s_photo_list():
    b = []
    for i in range(8):
        x, y = 16 + (i % 4) * 194, 68 + (i // 4) * 162
        if i == 0:
            b += [rect(x, y, 184, 152, PANEL, 12, LINE), icon("folder", x + 60, y + 30, 64, MUTED),
                  t(x + 92, y + 132, "КАМЕРА", 15, MUTED, 600, "middle", ls=1.5)]
            continue
        cur = i == 3
        b.append(rect(x, y, 184, 152, ACT if cur else PANEL, 12))
        b.append(landscape(x + 6, y + 6, 172, 110))
        b.append(t(x + 92, y + 138, "IMG_07%02d.JPG" % (10 + i), 14, ON_TEXT if cur else MUTED, 700, "middle"))
    b.append(dock([button(16, 410, 112, 58, "USB", "sel"), button(136, 410, 112, 58, "SD"),
                   button(456, 410, 160, 58, "СЛАЙДЫ", "n", "play", 17), icon("gear", 712, 417, 44, TEXT)]))
    return ("photo_list", "Фото: сетка", "patch",
            "Сетка 4×2 плиток 184×152 вместо текстового списка: по фото выбирают глазами. Папка — плитка со значком, открытый снимок — оранжевая рамка-заливка.",
            frame("ФОТО · USB · ПУТЕШЕСТВИЯ", b))


def s_photo_invalid():
    b = [rect(0, 56, 800, 342, "#08090a", 0), rect(220, 110, 360, 230, PANEL, 14, LINE),
         icon("photo", 360, 140, 80, OFF), '<path d="M352 140 L448 220" stroke="%s" stroke-width="4" stroke-linecap="round"/>' % MUTED,
         t(400, 262, "ФОРМАТ НЕ ПОДДЕРЖИВАЕТСЯ", 18, TEXT, 700, "middle", ls=1.5),
         t(400, 292, "DACHA_2026.HEIC", 17, MUTED, 400, "middle", cls="t-body"),
         t(400, 318, "Пропустить можно стрелками внизу", 15, DIM, 400, "middle", cls="t-body"), photo_dock()]
    return ("photo_invalid", "Фото: формат не поддерживается", "now",
            "Вместо красного знака «запрещено» (<code>symbol_invalid_big_rus.png</code>) — спокойная карточка: ошибка не требует действия, поэтому без оранжевого и красного.",
            frame("ФОТО · 4 / 24", b))


def s_aux():
    b = [rect(24, 76, 236, 236, "url(#tri-or)", 14), rect(24, 76, 236, 236, "none", 14, LINE),
         icon("aux", 92, 144, 100, INK, ' opacity=".55"'),
         t(290, 112, "ВНЕШНИЙ ИСТОЧНИК", 14, MUTED, 600, ls=2), t(290, 162, "AUX", 52, TEXT, 700),
         t(290, 198, "Звук идёт с разъёма 3,5 мм. Управляйте", 18, MUTED, 400, cls="t-body"),
         t(290, 222, "воспроизведением на самом устройстве.", 18, MUTED, 400, cls="t-body"),
         t(290, 282, "Уровень входа", 19, TEXT, 700, cls="t-body"), stepper(560, 252, 216, "+2", 52)]
    b.append(dock_sources("AUX", [icon("gear", 712, 417, 44, TEXT)]))
    return ("aux", "AUX", "patch",
            "Экрана AUX в оболочке почти нет — только плитка. Здесь та же сетка, что у плеера: знак источника на оранжевом паттерне, подсказка и уровень входа. В доке выбран AUX.",
            frame("AUX", b))


def s_phone_contacts():
    b = phone_nav("КОНТАКТЫ")
    b += [rect(192, 68, 560, 56, PANEL, 10, LINE), icon("search", 206, 80, 32, MUTED),
          t(250, 104, "Поиск по имени или номеру", 18, DIM, 400, cls="t-body")]
    people = [("Мама", "+7 912 000-12-34", "М"), ("Матвей Никифоров", "+7 917 123-45-67", "МН"), ("Мария Никифорова", "+7 927 765-43-21", "МН"),
              ("Сервис Лада", "+7 8482 55-12-00", "СЛ"), ("Сева", "+7 903 111-22-33", "С")]
    for i, (n, num, ini) in enumerate(people):
        y = 136 + i * 64
        b.append(rect(192, y, 560, 58, PANEL, 10))
        b.append(avatar(222, y + 29, 19, ini, 14))
        b.append(t(256, y + 25, n, 19, TEXT, 700, cls="t-body"))
        b.append(t(256, y + 47, num, 15, MUTED, 400, cls="t-body"))
        b.append(icon("star-f" if i == 0 else "next", 708, y + 13, 32, OR if i == 0 else DIM))
    letters = "АБВГДЕЖЗИКЛМНОПРСТУФХЦЧШЭЮЯ"
    for i, ch in enumerate(letters):
        if ch == "М":
            b.append('<circle cx="772" cy="%g" r="9" fill="%s"/>' % (136.5 + i * 11.5, ACT))
        b.append(t(772, 140 + i * 11.5, ch, 10, ON_TEXT if ch == "М" else DIM, 700, "middle"))
    return ("phone_contacts", "Телефон: контакты", "patch",
            "Разделы телефона — колонка 160×70 слева (набор, контакты, избранное, журнал, музыка). Список 560 px с поиском и полосой букв справа: буква под пальцем подсвечивается. Избранные отмечены звездой.",
            frame("ТЕЛЕФОН · КОНТАКТЫ", b))


def s_phone_log():
    b = phone_nav("ЖУРНАЛ")
    b.append(seg(192, 68, 592, 52, ["ВСЕ", "ВХОДЯЩИЕ", "ИСХОДЯЩИЕ", "ПРОПУЩ."], 0, 15))
    calls = [("call-in", MUTED, "Мама", "12:05 · 3 мин", ""), ("call-miss", STOP, "+7 495 000-11-22", "11:40", "2"),
             ("call-out", MUTED, "Сервис Лада", "вчера · 1 мин", ""), ("call-out", MUTED, "Матвей Никифоров", "вчера · 12 мин", ""),
             ("call-in", MUTED, "Мария Никифорова", "пн · 5 мин", "")]
    for i, (ic, col, n, sub, cnt) in enumerate(calls):
        y = 132 + i * 66
        b.append(rect(192, y, 592, 60, PANEL, 10))
        b.append(icon(ic, 206, y + 14, 32, col))
        b.append(t(254, y + 27, n, 19, TEXT, 700, cls="t-body"))
        b.append(t(254, y + 49, sub, 15, MUTED, 400, cls="t-body"))
        if cnt:
            b.append(rect(560, y + 18, 34, 24, STOP, 12) + t(577, y + 36, cnt, 15, TEXT, 700, "middle"))
        b.append(rect(704, y + 6, 72, 48, PANEL2, 10) + icon("call", 724, y + 14, 32, OK))
    return ("phone_log", "Телефон: журнал вызовов", "patch",
            "Фильтр сегментами вместо трёх кнопок (<code>phone_calls_inbox/outbox/missed</code>). Тип вызова — значок стрелки, пропущенный красный, как в v0.1. Зелёная кнопка в строке перезванивает.",
            frame("ТЕЛЕФОН · ЖУРНАЛ", b))


def s_phone_call():
    b = [avatar_or(128, 168, 88, "МН"),
         t(256, 112, "РАЗГОВОР", 14, MUTED, 700, ls=2), t(256, 160, "Матвей Никифоров", 40, TEXT, 700),
         t(256, 196, "+7 917 123-45-67 · мобильный", 20, MUTED, 400, cls="t-body"),
         t(256, 246, "00:42", 34, TEXT, 700, extra=' style="font-variant-numeric:tabular-nums"')]
    ctl = [("mic-off", "МИКРОФОН", "act"), ("keypad", "НАБОР", "fill"), ("handover", "НА ТЕЛЕФОН", "fill")]
    for i, (ic, lab, st) in enumerate(ctl):
        x = 16 + i * 136
        b.append(rect(x, 296, 124, 96, ACT if st == "act" else PANEL, 12, None if st == "act" else LINE))
        b.append(icon(ic, x + 42, 308, 40, ON_TEXT if st == "act" else TEXT))
        b.append(t(x + 62, 374, lab, 13, ON_TEXT if st == "act" else MUTED, 700, "middle", ls=1))
    b.append(rect(424, 296, 360, 96, STOP, 12) + icon("call", 580, 320, 48, INK, ' transform="rotate(135 604 344)"'))
    b.append(dock([icon("vol", 24, 417, 44, MUTED), slider(88, 435, 560, 0.45), t(700, 448, "12", 22, TEXT, 700, "middle")]))
    return ("phone_call", "Телефон: разговор", "patch",
            "Крупное имя и таймер, три кнопки 124×96: выключенный микрофон — нажатое состояние (заливка). Сброс — широкая красная, как в наборе. Внизу громкость разговора: в звонке это главная регулировка.",
            frame("ТЕЛЕФОН", b))


def s_phone_incoming():
    b = [t(40, 160, "106.2", 92, TEXT, 700, extra=' opacity=".25"'), rect(0, 56, 800, 424, INK, 0, extra=' opacity=".72"'),
         rect(100, 84, 600, 340, PANEL, 16, LINE), avatar_or(196, 184, 64, "МН"),
         t(286, 150, "ВХОДЯЩИЙ ВЫЗОВ", 14, MUTED, 700, ls=2), t(286, 194, "Мария Никифорова", 32, TEXT, 700),
         t(286, 226, "+7 927 765-43-21", 20, MUTED, 400, cls="t-body"),
         rect(124, 300, 266, 92, OK, 12) + icon("call", 233, 322, 48, INK),
         rect(410, 300, 266, 92, STOP, 12) + icon("call", 519, 322, 48, INK, ' transform="rotate(135 543 346)"')]
    return ("phone_incoming", "Телефон: входящий вызов", "now",
            "Окно поверх текущего источника (радио приглушено). Две кнопки 266×92 — зелёная и красная, без оранжевого: здесь важен смысл цвета. Окно не закрывается само, пока идёт вызов.",
            frame("РАДИО", b))


def s_phone_card():
    b = phone_nav("КОНТАКТЫ")
    b += [panel(192, 68, 592, 322), avatar_or(260, 136, 52, "МН"), t(332, 128, "Матвей Никифоров", 28, TEXT, 700),
          icon("star-f", 332, 144, 26, OR), t(366, 164, "В избранном", 16, MUTED, 400, cls="t-body")]
    nums = [("МОБИЛЬНЫЙ", "+7 917 123-45-67"), ("РАБОЧИЙ", "+7 8482 55-12-00"), ("ДОМАШНИЙ", "+7 8482 31-44-90")]
    for i, (k, v) in enumerate(nums):
        y = 204 + i * 62
        b.append(rect(208, y, 560, 56, PANEL2, 10))
        b.append(t(224, y + 22, k, 12, MUTED, 700, ls=1.5))
        b.append(t(224, y + 45, v, 20, TEXT, 700))
        b.append(rect(688, y + 6, 72, 44, OK, 10) + icon("call", 708, y + 10, 32, INK))
    return ("phone_card", "Телефон: карточка контакта", "patch",
            "Карточка внутри панели контактов: аватар из инициалов на оранжевом паттерне, звезда избранного, номера строками по 56 px, у каждого своя зелёная кнопка вызова.",
            frame("ТЕЛЕФОН · КОНТАКТ", b))


def s_phone_off():
    b = [rect(0, 0, 0, 0, INK, 0)]
    b += ['<circle cx="400" cy="140" r="56" fill="%s"/>' % PANEL, icon("bt", 368, 108, 64, MUTED),
          t(400, 236, "ТЕЛЕФОН НЕ ПОДКЛЮЧЁН", 22, TEXT, 700, "middle", ls=1.5),
          t(400, 268, "Включите Bluetooth на телефоне и найдите устройство «VESTA»", 18, MUTED, 400, "middle", cls="t-body"),
          rect(192, 296, 416, 64, PANEL, 10), avatar(226, 328, 18, "R", 14), t(258, 322, "Redmi Note", 19, TEXT, 700, cls="t-body"),
          t(258, 344, "подключался вчера", 15, MUTED, 400, cls="t-body"), button(470, 304, 130, 48, "ПОДКЛЮЧИТЬ", "pri", None, 14)]
    b.append(dock([button(16, 410, 300, 58, "НАСТРОЙКИ BLUETOOTH", "n", "gear", 16)]))
    return ("phone_off", "Телефон: не подключён", "now",
            "Пустое состояние вместо голой клавиатуры: что сделать и последний телефон с кнопкой «подключить» — единственное действие экрана, поэтому оранжевое.",
            frame("ТЕЛЕФОН", b))


def settings(title, active, rows_svg, key, caption, chip="now"):
    b = settings_nav(active) + [panel(240, 72, 544, 392)] + rows_svg
    return (key, title, chip, caption, frame("НАСТРОЙКИ", b))


def s_set_general():
    r = [set_row(76, "Дата", stepper(512, 84, 248, "26.03.2026", 48, 18)),
         set_row(140, "Время", stepper(512, 148, 248, "14:32", 48, 20)),
         set_row(204, "Синхронизация по GPS", toggle(688, 218, True)),
         set_row(268, "Формат времени", seg(560, 278, 200, 44, ["12", "24"], 1)),
         set_row(332, "Язык", t(720, 371, "Русский", 18, MUTED, 400, "end", cls="t-body") + chevron(728, 348)),
         set_row(396, "Заводские настройки", button(616, 404, 144, 48, "СБРОС", "n", None, 16), last=True)]
    return settings("Настройки: общие", "ОБЩИЕ", r, "set_general",
                    "Разделы — сетка 2×4 плиток 102×92: восемь разделов оболочки (общие, звук, экран, радио, телефон, Wi-Fi, видео, фото) в список по 56 px не влезают. Дата и время — степперы, при включённой синхронизации по GPS их можно не трогать. Файлы: <code>Temp/setting/main/*</code>.")


def s_set_sound():
    r = [t(264, 104, "Эквалайзер", 19, TEXT, 700, cls="t-body"),
         seg(256, 116, 512, 48, ["СВОЙ", "ПОП", "РОК", "ДЖАЗ", "КЛАСС.", "ГОЛОС"], 2, 14)]
    bands = [("60", 0.62, "+4"), ("250", 0.55, "+2"), ("1К", 0.45, "-1"), ("4К", 0.58, "+3"), ("12К", 0.7, "+6")]
    for i, (lab, f, v) in enumerate(bands):
        r.append(vslider(300 + i * 80, 206, 150, f, lab, v))
    r.append(rect(696, 196, 64, 168, PANEL2, 10) + icon("eq", 704, 252, 48, MUTED))
    r.append(sep(256, 396, 512))
    r.append(t(264, 438, "Тонкомпенсация", 19, TEXT, 700, cls="t-body") + toggle(688, 412, True))
    return settings("Настройки: звук", "ЗВУК", r, "set_sound",
                    "Пресеты одной строкой сегментов вместо значков-инструментов (<code>set_audio2_symbol_*</code>), под ними пять полос ±6 дБ, ползунки вертикальные. При правке полосы пресет переключается на «Свой».")


def s_set_balance():
    car = (rect(276, 92, 200, 340, PANEL2, 40) + rect(296, 150, 70, 80, INK, 10) + rect(386, 150, 70, 80, INK, 10)
           + rect(296, 290, 70, 80, INK, 10) + rect(386, 290, 70, 80, INK, 10)
           + '<path d="M376 100 V424 M284 262 H468" stroke="%s" stroke-width="2" stroke-dasharray="4 6"/>' % DIM
           + '<circle cx="396" cy="232" r="18" fill="%s"/><circle cx="396" cy="232" r="6" fill="%s"/>' % (OR, INK))
    r = [car, t(504, 110, "Баланс", 19, TEXT, 700, cls="t-body"), stepper(504, 124, 256, "П 2", 48),
         t(504, 208, "Фейдер", 19, TEXT, 700, cls="t-body"), stepper(504, 222, 256, "ПЕРЕД 3", 48),
         t(504, 306, "Громкость от скорости", 19, TEXT, 700, cls="t-body"), seg(504, 320, 256, 44, ["ВЫКЛ", "1", "2", "3"], 2, 15),
         button(504, 396, 256, 48, "В ЦЕНТР", "n", None, 16)]
    return settings("Настройки: баланс", "ЗВУК", r, "set_balance",
                    "Вторая страница звука (<code>set_audio1_*</code>): точку слушателя двигают пальцем по схеме салона или степперами справа. Точка — оранжевая, как ползунок.", "patch")


def s_set_radio():
    items = [("Шумоподавление", True), ("Диапазон СВ", False), ("RDS", True), ("AF · альтернативные частоты", False),
             ("CT · время по радио", True), ("TA · дорожные сообщения", True)]
    r = []
    for i, (lab, on) in enumerate(items):
        y = 76 + i * 64
        r.append(set_row(y, lab, toggle(688, y + 14, on), last=i == len(items) - 1))
    return settings("Настройки: радио", "РАДИО", r, "set_radio",
                    "Шесть переключателей строками по 64 px. Расшифровка аббревиатур RDS прямо в подписи: на ходу некогда вспоминать, что такое AF.")


def s_set_bt():
    r = [set_row(76, "Имя устройства", t(720, 115, "VESTA", 18, MUTED, 700, "end") + chevron(728, 92)),
         set_row(140, "Bluetooth", toggle(688, 154, True)),
         set_row(204, "Подключено к", button(560, 212, 200, 48, "ОТКЛЮЧИТЬ", "n", "power", 15), sub="Redmi Note"),
         set_row(268, "Автосоединение", toggle(688, 282, True)),
         set_row(332, "Автоответ", toggle(688, 346, False)),
         set_row(396, "Телефонная книга", button(600, 404, 160, 48, "ОБНОВИТЬ", "n", "refresh", 15), sub="обновлена вчера, 412 контактов", last=True)]
    return settings("Настройки: Bluetooth", "ТЕЛЕФОН", r, "set_bt",
                    "Строки 64 px: под подписью вторая строка с состоянием (кто подключён, когда обновлялась книга). Действия — обведённые кнопки со значком, переключатели — оранжевые во включённом виде.")


def s_set_wifi():
    r = [set_row(76, "Wi-Fi", toggle(688, 90, True))]
    nets = [("Vesta_Garage", "подключено", True, True), ("Keenetic-4213", "", True, False), ("MTS_Router_5G", "", False, False),
            ("AndroidAP", "", True, False)]
    for i, (n, sub, lock, cur) in enumerate(nets):
        y = 148 + i * 78
        r.append(rect(256, y, 512, 70, ACT if cur else PANEL2, 10))
        r.append(icon("wifi", 272, y + 17, 36, ON_TEXT if cur else MUTED))
        r.append(t(324, y + (32 if sub else 43), n, 19, ON_TEXT if cur else TEXT, 700, cls="t-body"))
        if sub:
            r.append(t(324, y + 54, sub, 15, ON_TEXT, 400, cls="t-body"))
        if lock:
            r.append(icon("lock", 716, y + 21, 28, ON_TEXT if cur else DIM))
    return settings("Настройки: Wi-Fi", "WI-FI", r, "set_wifi",
                    "Список сетей прямо в панели вместо отдельного окна (<code>set_wifi_panel.png</code>): подключённая — оранжевая строка, замок у закрытых. Пароль спрашивает экран клавиатуры.")


def s_wifi_kb():
    b = [rect(16, 68, 600, 60, PANEL, 10, LINE), t(32, 106, "vesta2026", 26, TEXT, 700), rect(170, 84, 3, 28, OR, 0, extra=' class="pulse"'),
         icon("eye", 566, 82, 32, MUTED), button(628, 68, 156, 60, "ГОТОВО", "pri", None, 18)]
    rows = ["1234567890", "qwertyuiop", "asdfghjkl-", "zxcvbnm_.@"]
    for r_, row in enumerate(rows):
        for c, ch in enumerate(row):
            x, y = 13 + c * 78, 140 + r_ * 66
            on = ch == "s"
            b.append(rect(x, y, 72, 60, ACT if on else PANEL, 10, None if on else LINE))
            b.append(t(x + 36, y + 40, ch, 26, ON_TEXT if on else TEXT, 700, "middle"))
    y = 408
    b += [button(13, y, 150, 60, None, "fill", "caps", isz=32), button(169, y, 150, 60, "АБВ", "fill", None, 20),
          rect(325, y, 306, 60, PANEL, 10, LINE), rect(410, y + 34, 136, 4, MUTED, 2),
          button(637, y, 150, 60, None, "fill", "del", isz=36)]
    return ("wifi_kb", "Wi-Fi: ввод пароля", "now",
            "Клавиатура на весь экран, клавиша 72×60 — крупнее штатной сетки из линий (<code>Temp/setting/wifi/keyboard_*</code>). Нажатая клавиша — заливка. Пароль можно показать значком глаза, «Готово» — единственное оранжевое действие.",
            frame("WI-FI · VESTA_GARAGE", b))


def s_set_video():
    r = [set_row(76, "Масштаб", seg(512, 88, 248, 44, ["ОБЫЧНЫЙ", "АВТО"], 1, 15)),
         set_row(140, "Формат", seg(512, 152, 248, 44, ["16:9", "4:3"], 0, 16)),
         set_row(204, "Видео в движении", toggle(688, 218, False), sub="по ПДД — выключено"),
         set_row(268, "Субтитры", toggle(688, 282, True), last=True)]
    return settings("Настройки: видео", "ВИДЕО", r, "set_video",
                    "Два столбца штатной панели (масштаб / формат, <code>Settings/Video/*</code>) стали двумя строками сегментов. Пустое место внизу панели — без растягивания строк.")


def s_set_photo():
    r = [set_row(76, "Интервал слайд-шоу", stepper(560, 84, 200, "5 С", 48, 18)),
         set_row(140, "Масштаб", seg(512, 152, 248, 44, ["ВПИСАТЬ", "ЗАПОЛН."], 0, 15)),
         set_row(204, "Слайд-шоу при открытии", toggle(688, 218, False)),
         set_row(268, "Автоповорот", toggle(688, 282, True), last=True)]
    return settings("Настройки: фото", "ФОТО", r, "set_photo",
                    "Интервал степпером вместо двух стрелок по краям, масштаб — сегменты (<code>set_photo_switch_scale.png</code>), автоповорот — переключатель.")


def s_info():
    b = [panel(16, 68, 768, 322)]
    rows = [("Модель", "LADA Vesta · MMC"), ("Версия ПО", "G3 4120"), ("ОС", "Windows CE 6.0"),
            ("Bluetooth", "00:1A:7D:DA:71:13"), ("VestaHome", "0.2 · 2026-10-08")]
    for i, (k, v) in enumerate(rows):
        y = 76 + i * 52
        b.append(t(40, y + 32, k, 19, MUTED, 400, cls="t-body"))
        b.append(t(760, y + 32, v, 19, TEXT, 700, "end", cls="t-body"))
        b.append(sep(32, y + 51, 736))
    b += [t(40, 364, "SD-карта", 19, MUTED, 400, cls="t-body"), rect(200, 352, 380, 12, LINE, 6), rect(200, 352, 288, 12, MUTED, 6),
          t(760, 364, "11,2 из 14,8 ГБ", 19, TEXT, 700, "end", cls="t-body")]
    b.append(dock([button(16, 410, 240, 58, "ОБНОВЛЕНИЕ ПО", "n", "refresh", 16), button(560, 410, 224, 58, "ЛИЦЕНЗИИ", "n", "info", 16)]))
    return ("info", "Информация", "patch",
            "Экран кнопки «i» главного меню: пары «название — значение», заполненность SD полосой. Полоса серая: это состояние, а не действие.",
            frame("ИНФОРМАЦИЯ", b))


def s_volume():
    b = [t(40, 160, "106.2", 92, TEXT, 700, extra=' opacity=".25"'), rect(0, 56, 800, 424, INK, 0, extra=' opacity=".6"'),
         rect(150, 150, 500, 180, PANEL, 16, LINE), icon("vol", 182, 182, 48, TEXT),
         t(246, 216, "ГРОМКОСТЬ", 15, MUTED, 700, ls=2), t(618, 222, "18", 48, TEXT, 700, "end")]
    for i in range(30):
        x = 182 + i * 15
        h = 14 + i * 1.2
        b.append(rect(x, 300 - h, 10, h, OR if i < 18 else LINE, 2))
    return ("volume", "Громкость (поверх экрана)", "now",
            "Окно 500×180 на 2 с при повороте ручки (<code>Main/volume/0..50.png</code>). Столбики растут слева направо, включённые — оранжевые. Источник под окном приглушается, но виден.",
            frame("РАДИО", b))


def s_warning():
    b = [rect(100, 84, 600, 320, PANEL, 16, LINE), icon("warn", 132, 112, 56, TEXT),
         t(204, 150, "ВНИМАНИЕ", 26, TEXT, 700, ls=2)]
    lines = ["Не отвлекайтесь от дороги. Управляйте системой", "только когда это безопасно. Правила дорожного",
             "движения важнее подсказок навигации."]
    for i, s in enumerate(lines):
        b.append(t(132, 212 + i * 30, s, 19, MUTED, 400, cls="t-body"))
    b.append(button(452, 324, 216, 60, "ПРИНЯТЬ", "pri", None, 19))
    b.append(t(132, 360, "Окно закроется само через 10 с", 15, DIM, 400, cls="t-body"))
    return ("warning", "Предупреждение при запуске", "now",
            "Стартовое окно (<code>Main/warning_label_*.png</code>): знак светлый — это не действие, оранжевая только кнопка «Принять».",
            frame("", b, back=False))


def s_screensaver():
    b = [rect(0, 0, 800, 480, "url(#tri)", 0), rect(0, 404, 800, 76, "url(#tri-or)", 0),
         rect(0, 404, 800, 76, "url(#fade-up)", 0, extra=' opacity=".55"'), rect(0, 402, 800, 3, OR, 0),
         t(400, 260, "14:32", 168, TEXT, 700, "middle"), t(400, 316, "Четверг, 26 марта", 26, MUTED, 400, "middle", cls="t-body"),
         icon("music", 24, 424, 36, TEXT), t(72, 450, "Кино — Группа крови", 18, TEXT, 700, cls="t-body"),
         t(776, 450, "+4 °C", 18, TEXT, 400, "end", cls="t-body")]
    svg = ('<svg class="screen" viewBox="0 0 800 480" role="img" aria-label="Заставка с часами">\n'
           + rect(0, 0, 800, 480, INK, 0) + "\n".join(b) + "\n</svg>")
    return ("screensaver", "Заставка с часами", "own",
            "Включается переключателем «Заставка с часами» в настройках экрана. Без строки состояния: часы 168 px читаются с водительского места, внизу оранжевая полоса главного меню и то, что играет.", svg)


def s_kit():
    b = [rect(0, 0, 800, 480, INK, 0)]
    lab = lambda x, y, s: t(x, y, s, 12, DIM, 600, "middle", ls=1.5)
    b += [t(16, 30, "КНОПКИ", 14, MUTED, 700, ls=2)]
    for i, (st, l) in enumerate([("n", "ОБЫЧНАЯ"), ("sel", "ВЫБРАНА"), ("act", "НАЖАТА"), ("dis", "НЕДОСТУПНА"), ("pri", "ДЕЙСТВИЕ")]):
        b.append(button(16 + i * 156, 44, 144, 56, "USB" if i < 4 else "ГОТОВО", st))
        b.append(lab(88 + i * 156, 120, l))
    b += [t(16, 160, "ПЕРЕКЛЮЧАТЕЛИ, СЕГМЕНТЫ, СТЕППЕР, ПОЛЗУНОК", 14, MUTED, 700, ls=2),
          toggle(16, 178, True), toggle(100, 178, False), seg(196, 174, 264, 44, ["АВТО", "ВКЛ", "ВЫКЛ"], 0),
          stepper(476, 172, 172, "+2"), slider(16, 250, 420, 0.6, "60"),
          lab(52, 238, "ВКЛ"), lab(136, 238, "ВЫКЛ")]
    b += [t(16, 300, "СТРОКИ СПИСКА", 14, MUTED, 700, ls=2),
          list_row(16, 314, 376, 56, "Кино", None, "n", "folder"),
          list_row(408, 314, 376, 56, "Группа крови", "4:46", "cur", None, num="3"),
          list_row(16, 378, 376, 56, "Матвей Никифоров", None, "n", None, "+7 917 123-45-67"),
          list_row(408, 378, 376, 56, "Нет файлов", None, "dim", "x"),
          scrollbar(16, 446, 4, 0, 1)]
    b += [t(16, 466, "Текст оболочки: обычный / активный (тёмный, на оранжевом) / неактивный — три цвета из G3NavHMI.exe.", 15, MUTED, 400, cls="t-body")]
    svg = '<svg class="screen" viewBox="0 0 800 480" role="img" aria-label="Набор компонентов">\n' + "\n".join(b) + "\n</svg>"
    return ("kit", "Компоненты", "now",
            "Всё, из чего собраны экраны: пять состояний кнопки, переключатель, сегменты, степпер, ползунок, строки списка (папка, текущий трек, контакт, пустая). Текст оболочки бывает трёх цветов по состоянию: обычный, активный (тёмный — поэтому на нажатом лежит сплошная оранжевая заливка, как в v0.1) и неактивный. Выбранный источник и нажатая кнопка в оболочке не различаются — обе показаны заливкой.", svg)


GROUPS = [
    ("Главное меню и программы", [s_main_p2, s_apps, s_screensaver]),
    ("Радио", [s_radio_list, s_radio_pty]),
    ("Аудио и AUX", [s_audio_list, s_audio_bt, s_aux]),
    ("Видео", [s_video_player, s_video_list]),
    ("Фото", [s_photo_view, s_photo_list, s_photo_invalid]),
    ("Телефон", [s_phone_contacts, s_phone_log, s_phone_card, s_phone_call, s_phone_incoming, s_phone_off]),
    ("Настройки", [s_set_general, s_set_sound, s_set_balance, s_set_radio, s_set_bt, s_set_wifi, s_wifi_kb, s_set_video, s_set_photo]),
    ("Системные окна", [s_info, s_volume, s_warning]),
    ("Компоненты", [s_kit]),
]

CHIPS = {"now": '<span class="chip now">текущая раскладка</span>',
         "patch": '<span class="chip patch">нужна правка раскладки</span>',
         "own": '<span class="chip">своя программа</span>'}


def main():
    src = open(SRC, encoding="utf-8").read()
    head_end = src.index("<div class=\"wrap\">")
    head = src[:head_end]
    head = head.replace("<title>Скин ММС Весты</title>", "<title>Скин ММС Весты · экраны</title>")
    head = head.replace("  </defs>", extra_defs() + "\n  </defs>", 1)
    n = sum(len(g[1]) for g in GROUPS)
    out = [head, '<div class="wrap">',
           '  <header class="top">\n    <div>\n      <div class="eyebrow">Концепт скина v0.1 · остальные экраны</div>\n'
           '      <h1>Скин ММС Весты: все экраны</h1>\n'
           '      <p class="lede">Продолжение <code>vesta-skin-v0.1</code>: экраны штатной оболочки, которых в концепте не было, '
           'в тех же правилах — графит с паттерном, оранжевый только для действия и выбранного, нажатие заливкой, линии не тоньше 2 px, '
           'зона нажатия от 56 px. Тексты, номера и файлы — примеры.</p>\n'
           '      <p class="lede"><b>Цвета текста задаёт оболочка по состоянию.</b> В <code>G3NavHMI.exe</code> это константы: обычный 0xFFB3F2BE, активный 0xFF5A5A5A, неактивный (вероятно) 0xFFA4A89F; их можно заменить патчем. Макет рассчитан на замену: обычный → #F3F1EC, активный → #0B0C0E (штатный #5A5A5A на оранжевом даёт только 2,4:1), неактивный → #5D636C. Цветного текста (красного, оранжевого) нет — цвет несут значки.</p>\n    </div>\n'
           '    <div class="spec"><span>Экран <b>800 × 480</b></span><span>Экранов <b>%d</b></span><span>Версия <b>0.1</b></span></div>\n  </header>' % n]
    for gname, fns in GROUPS:
        out.append("  <h2>%s</h2>" % e(gname))
        out.append('  <div class="grid">')
        for fn in fns:
            key, title, chip, cap, svg = fn()
            out.append('    <figure id="%s">\n%s\n      <figcaption>\n        <div class="cap-head"><strong>%s</strong>%s</div>\n'
                       '        <p>%s</p>\n      </figcaption>\n    </figure>' % (key, svg, e(title), CHIPS[chip], cap))
        out.append("  </div>")
    out.append("</div>\n\n</body>\n</html>\n")
    open(OUT, "w", encoding="utf-8", newline="\n").write("\n".join(out))
    print(OUT, n, "screens")


if __name__ == "__main__":
    main()
