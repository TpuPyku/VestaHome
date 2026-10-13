#!/usr/bin/env python3
"""make_mmc_screens.py - draw the MMC screens missing from MAIN/VestaMMCskin.ai.

The .ai holds 21 screens (see _ai/all_screens.png). The stock shell G3NavHMI
has more: main menu page 2, video, photo, AUX, call / incoming call, contact
card, Wi-Fi keyboard, info, PTY list, volume and warning pop-ups. This script
composes those screens at 800x480 in the same style, reusing:

- the stock skin pictures from MAIN/MMC27 (keycaps, icons, list rows), so
  every button is the real asset the shell would load;
- fragments of the .ai renders in _ai/imgs (status bar, list panel, side
  columns), so frames match the existing screens pixel for pixel;
- the shell font MAIN/ResidentFlash/fonts/micra.ttf.

Output: mmc_screens/NN_<name>.png, mmc_screens/kit.png (buttons, switches,
list rows) and mmc_screens/all_screens_new.png (contact sheet, new only).

    python skin/make_mmc_screens.py

Needs Pillow, MAIN/MMC27 and the 800x480 rasters embedded in the .ai,
saved as _ai/imgs/img_<xref>.png (parse_skin.py does not write them).
"""
import os
from PIL import Image, ImageDraw, ImageFont, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
MMC = os.path.join(ROOT, "MAIN", "MMC27")
AIIMG = os.path.join(HERE, "_ai", "imgs")
FONT = os.path.join(ROOT, "MAIN", "ResidentFlash", "fonts", "micra.ttf")
OUT = os.path.join(HERE, "mmc_screens")

# ---- palette sampled from the .ai renders --------------------------------
WHITE = (255, 255, 255)
GREEN = (179, 242, 190)      # inactive caption / list text
ORANGE = (255, 134, 0)       # accent line, active icon
LIST_BG = (52, 50, 46)       # list panel fill
GREY = (158, 158, 158)
DARK = (41, 41, 41)

_fonts = {}


def font(size):
    if size not in _fonts:
        _fonts[size] = ImageFont.truetype(FONT, size)
    return _fonts[size]


def asset(rel):
    return Image.open(os.path.join(MMC, rel.replace("/", os.sep))).convert("RGBA")


def ai(n):
    return Image.open(os.path.join(AIIMG, "img_%05d.png" % n)).convert("RGBA")


def put(im, src, x, y):
    im.alpha_composite(src, (int(x), int(y)))


def putc(im, src, cx, cy):
    """Paste centred on (cx, cy)."""
    put(im, src, cx - src.width // 2, cy - src.height // 2)


def text(im, xy, s, size, fill=GREEN, anchor="la"):
    d = ImageDraw.Draw(im)
    f = font(size)
    if "_" not in s:
        d.text(xy, s, font=f, fill=fill, anchor=anchor)
        return
    # micra.ttf has no '_': draw a space and put the underscore in by hand
    t = s.replace("_", " ")
    d.text(xy, t, font=f, fill=fill, anchor=anchor)
    x0, _, x1, y1 = d.textbbox(xy, t, font=f, anchor=anchor)
    base = d.textbbox(xy, "A", font=f, anchor=anchor[0] + anchor[1:])[3] if anchor[0] == "l" else y1
    w = f.getlength(t)
    left = xy[0] - (w / 2 if anchor[0] == "m" else w if anchor[0] == "r" else 0)
    sw = f.getlength(" ")
    for i, ch in enumerate(s):
        if ch == "_":
            x = left + f.getlength(t[:i])
            d.line([(x + 1, base), (x + sw - 1, base)], fill=fill, width=max(1, size // 9))


def fit(s, size, width):
    """Cut s with an ellipsis to fit width (the shell does the same)."""
    f = font(size)
    if f.getlength(s) <= width:
        return s
    while s and f.getlength(s + "...") > width:
        s = s[:-1]
    return s + "..."


def scale(src, k):
    return src.resize((int(src.width * k), int(src.height * k)), Image.LANCZOS)


def orangify(src):
    """Turn a stock yellow/red highlight into the .ai orange, keep shading."""
    r, g, b, a = src.split()
    lum = Image.merge("RGB", (r, g, b)).convert("L")
    lut_r = [min(255, int(v * 1.15)) for v in range(256)]
    lut_g = [int(v * 0.62) for v in range(256)]
    lut_b = [int(v * 0.15) for v in range(256)]
    return Image.merge("RGBA", (lum.point(lut_r), lum.point(lut_g), lum.point(lut_b), a))


def hline(im, y, x0, x1, color=ORANGE, w=2, fade=60):
    """Separator line that fades out at both ends like the .ai lines."""
    d = ImageDraw.Draw(im)
    for x in range(x0, x1):
        k = min(1.0, (x - x0) / float(fade), (x1 - x) / float(fade))
        c = tuple(int(v * k) for v in color) + (255,)
        d.line([(x, y), (x, y + w - 1)], fill=c)


def gradient(w, h, stops):
    """Vertical gradient; stops = [(pos 0..1, (r,g,b)), ...]."""
    g = Image.new("RGBA", (w, h))
    d = ImageDraw.Draw(g)
    for y in range(h):
        t = y / float(max(1, h - 1))
        for i in range(len(stops) - 1):
            p0, c0 = stops[i]
            p1, c1 = stops[i + 1]
            if p0 <= t <= p1:
                k = (t - p0) / (p1 - p0) if p1 > p0 else 0
                c = tuple(int(c0[j] + (c1[j] - c0[j]) * k) for j in range(3))
                d.line([(0, y), (w, y)], fill=c + (255,))
                break
    return g


def orange_bar(w, h):
    """Active row / tab: the glossy orange of the .ai (pale top, deep bottom)."""
    return gradient(w, h, [(0, (250, 214, 180)), (0.45, (255, 150, 40)),
                           (0.75, (230, 110, 0)), (1, (120, 50, 0))])


def grey_bar(w, h):
    """Selected list row: silver gloss (audio_list_tracks_panel_select_track)."""
    return gradient(w, h, [(0, (90, 90, 90)), (0.35, (185, 185, 185)),
                           (0.5, (150, 150, 150)), (1, (70, 70, 70))])


def panel(w, h, glow=True):
    """Grey settings panel (set_*_panel look) with a soft white rim."""
    p = Image.new("RGBA", (w + 24, h + 24), (0, 0, 0, 0))
    if glow:
        g = Image.new("RGBA", p.size, (0, 0, 0, 0))
        ImageDraw.Draw(g).rounded_rectangle([10, 10, w + 13, h + 13], 8, fill=(255, 255, 255, 120))
        p.alpha_composite(g.filter(ImageFilter.GaussianBlur(6)))
    body = gradient(w, h, [(0, (112, 112, 112)), (0.25, (70, 70, 70)), (1, (38, 38, 38))])
    mask = Image.new("L", (w, h), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, w - 1, h - 1], 6, fill=255)
    body.putalpha(mask)
    p.alpha_composite(body, (12, 12))
    ImageDraw.Draw(p).rounded_rectangle([12, 12, w + 11, h + 11], 6, outline=(150, 150, 150, 255))
    return p


def list_panel(w, h):
    """Dark list panel (list_media_panel look): brown-grey with a white rim."""
    p = Image.new("RGBA", (w + 24, h + 24), (0, 0, 0, 0))
    g = Image.new("RGBA", p.size, (0, 0, 0, 0))
    ImageDraw.Draw(g).rounded_rectangle([8, 8, w + 15, h + 15], 8, fill=(255, 255, 255, 150))
    p.alpha_composite(g.filter(ImageFilter.GaussianBlur(5)))
    ImageDraw.Draw(p).rounded_rectangle([12, 12, w + 11, h + 11], 6, fill=(40, 39, 36, 255),
                                         outline=(120, 120, 120, 255), width=2)
    ImageDraw.Draw(p).rounded_rectangle([17, 17, w + 6, h + 6], 4, fill=LIST_BG + (255,))
    return p


# ---- shared frame pieces -------------------------------------------------

def base(black=False):
    """Background with orange band + status bar of the .ai (img_00225)."""
    im = asset("Temp/common/black.png" if black else "Temp/common/background.png")
    put(im, ai(225).crop((0, 0, 800, 64)), 0, 0)
    return im


def status_line(im, y=70):
    hline(im, y, 16, 784, ORANGE, 2, 120)


def side_lines(im, ys, left=True, right=True):
    ln = asset("Temp/media/common/side_line_left.png")
    for y in ys:
        if left:
            put(im, ln, 0, y)
        if right:
            put(im, ln.transpose(Image.FLIP_LEFT_RIGHT), 642, y)


def cap(name):
    """Keycap button 83x55 from the stock skin (Temp/...)."""
    return asset(name)


def ret_icon():
    """Green 'back' arrow used without a keycap in the .ai (bottom-left),
    cut out of the orange band by its greenness."""
    src = ai(227).crop((50, 400, 105, 440))
    r, g, b, _ = src.split()
    mask = Image.merge("RGB", (r, g, b))
    a = Image.new("L", src.size, 0)
    px, pa = mask.load(), a.load()
    for y in range(src.height):
        for x in range(src.width):
            rr, gg, bb = px[x, y]
            pa[x, y] = max(0, min(255, (gg - rr) * 4))
    src.putalpha(a)
    return src


def scroll_arrows(im, x, y_up, y_dn):
    put(im, asset("Temp/common/symbol_arrow_up.png"), x, y_up)
    put(im, asset("Temp/common/symbol_arrow_down.png"), x, y_dn)


def list_rows(im, x, y, rows, step=47, width=390, sel=None, size=17, icon=None, num=True):
    """Rows of the playlist (img_00232): number, name, white separator line."""
    line = asset("Temp/media/common/list_media_panel_line_white.png")
    for i, r in enumerate(rows):
        cy = y + i * step
        if i == sel:
            bar = asset("Temp/media/audio/audio_playlist/audio_list_tracks_panel_select_track.png")
            put(im, bar, x - 12, cy - bar.height // 2)
        else:
            put(im, line, x - 6, cy + step // 2 - 2)
        tx = x + 10
        if icon is not None:
            ic = icon if not isinstance(icon, list) else icon[i]
            if ic is not None:
                putc(im, ic, x + 26, cy)
                tx = x + 62
        col = (90, 90, 90) if i == sel else GREEN
        if num and isinstance(r, tuple) and r[0]:
            text(im, (tx, cy), r[0], size, col, "lm")
            text(im, (tx + 58, cy), fit(r[1], size, width - 70), size, col, "lm")
        else:
            text(im, (tx, cy), fit(r if not isinstance(r, tuple) else r[1], size, width), size, col, "lm")


def bottom_caps(im, items, y=393):
    """Row of keycaps on the orange band, evenly spaced."""
    n = len(items)
    step = 800.0 / n
    for i, it in enumerate(items):
        if it is None:
            continue
        putc(im, it, int(step * i + step / 2), y + 27)


def dim(im, k=0.35):
    return Image.alpha_composite(im, Image.new("RGBA", im.size, (0, 0, 0, int(255 * (1 - k)))))


# ---- screens -------------------------------------------------------------

def scr_main_page2():
    """Main menu, page 2: video, photo, AUX (main_folders_second_screen)."""
    im = ai(278)
    put(im, asset("Temp/common/background.png").crop((0, 120, 800, 340)), 0, 120)
    names = [("main_symbol_video", "ВИДЕО"), ("main_symbol_photo", "ФОТО"), ("main_symbol_aux", "AUX")]
    for i, (n, cap_) in enumerate(names):
        t = asset("Temp/main/%s.png" % n)
        put(im, t, 28 + 186 * i, 143)
        text(im, (28 + 186 * i + 150, 183), cap_, 11, GREEN, "rm")
    dots = ai(278).crop((365, 348, 415, 372)).transpose(Image.FLIP_LEFT_RIGHT)
    put(im, dots, 365, 348)
    return im


def road_scene(w=800, h=480):
    """Placeholder frame: night road, drawn so the mock-up has content."""
    sc = gradient(w, h, [(0, (20, 28, 48)), (0.45, (60, 62, 80)), (0.46, (28, 28, 30)), (1, (12, 12, 12))])
    d = ImageDraw.Draw(sc)
    d.polygon([(330, 218), (470, 218), (760, 480), (40, 480)], fill=(45, 45, 48))
    for k in range(6):
        y0 = 225 + k * k * 9
        d.polygon([(398 - k * 2, y0), (402 + k * 2, y0), (404 + k * 4, y0 + 12 + k * 6), (396 - k * 4, y0 + 12 + k * 6)],
                  fill=(210, 200, 150))
    d.polygon([(0, 216), (120, 170), (260, 200), (330, 218), (0, 240)], fill=(18, 22, 30))
    d.polygon([(470, 218), (600, 175), (800, 205), (800, 240)], fill=(18, 22, 30))
    return sc


def photo_scene(w=800, h=480):
    sc = gradient(w, h, [(0, (90, 140, 200)), (0.55, (230, 200, 160)), (0.56, (70, 110, 60)), (1, (40, 70, 40))])
    d = ImageDraw.Draw(sc)
    d.ellipse([560, 70, 640, 150], fill=(255, 230, 170))
    d.polygon([(0, 270), (180, 150), (330, 250), (470, 140), (640, 260), (800, 200), (800, 300), (0, 300)], fill=(70, 80, 105))
    d.polygon([(0, 300), (800, 280), (800, 480), (0, 480)], fill=(58, 95, 52))
    d.polygon([(300, 480), (380, 300), (420, 300), (520, 480)], fill=(150, 130, 100))
    return sc


def scr_video_player():
    im = base()
    frame = road_scene().crop((0, 0, 800, 330))
    put(im, frame.resize((790, 270)), 5, 75)
    status_line(im)
    text(im, (20, 92), "DASHCAM_0412.MP4", 16, WHITE, "lm")
    # progress: like the audio player, but orange (the .ai colour)
    text(im, (20, 360), "01:12", 13, GREEN, "lm")
    text(im, (780, 360), "03:45", 13, GREEN, "rm")
    hline(im, 374, 10, 790, (90, 90, 90), 2, 10)
    hline(im, 374, 10, 260, ORANGE, 3, 4)
    putc(im, asset("Temp/media/common/symbol_slider.png"), 260, 375)
    bottom_caps(im, [ret_icon(), cap("Temp/common/symbol_setting.png"),
                     cap("Temp/media/common/symbol_previous_file.png"),
                     asset("Player/pause.png"),
                     cap("Temp/media/common/symbol_next_file.png"),
                     cap("Temp/media/common/repeat_file.png"),
                     cap("Temp/media/common/symbol_folder.png")])
    return im


def media_list_screen(big_icon, small_icon, rows, sel, right_caps):
    """Playlist layout of img_00232: left column, list panel, right column."""
    im = base()
    left = ai(232).crop((0, 70, 158, 480))
    put(im, left, 0, 70)
    put(im, Image.new("RGBA", (158, 135), (0, 0, 0, 255)), 0, 80)
    putc(im, big_icon, 94, 150)
    status_line(im)
    put(im, list_panel(462, 400), 152, 40)
    folder = asset("Temp/media/audio/audio_symbol_folder.png")
    list_rows(im, 190, 100, rows, sel=sel, icon=[folder if r[0] == "" else None for r in rows])
    scroll_arrows(im, 576, 96, 380)
    put(im, Image.new("RGBA", (158, 135), (0, 0, 0, 255)), 642, 80)
    putc(im, small_icon, 712, 150)
    side_lines(im, [218, 283, 337, 390], left=False)
    for i, c in enumerate(right_caps):
        putc(im, c, 721, 254 + i * 54)
    return im


def scr_video_list():
    rows = [("", "DASHCAM"), ("", "МУЛЬТФИЛЬМЫ"), ("1.", "DASHCAM_0410.MP4"), ("2.", "DASHCAM_0411.MP4"),
            ("3.", "DASHCAM_0412.MP4"), ("4.", "ROAD_TRIP_SOCHI.AVI"), ("5.", "VESTA_REVIEW.MKV"),
            ("6.", "TRAILER.3GP")]
    big = asset("Temp/media/video/video_playlist/video_symbol_video.png")
    small = scale(big, 0.6)
    return media_list_screen(big, small, rows, 4,
                             [cap("Temp/media/common/symbol_play.png"),
                              cap("Temp/media/common/repeat_off.png"),
                              cap("Temp/media/common/shuffle_off.png"),
                              orangify(asset("Temp/media/common/symbol_folder.png"))])


def scr_photo_list():
    rows = [("", "КАМЕРА"), ("", "ПУТЕШЕСТВИЯ"), ("1.", "IMG_20260712_101530.JPG"), ("2.", "IMG_20260712_101544.JPG"),
            ("3.", "ELBRUS_PANORAMA.JPG"), ("4.", "DACHA_2026.PNG"), ("5.", "VESTA_SW_CROSS.JPG"),
            ("6.", "SCREENSHOT_01.BMP")]
    big = asset("Temp/media/photo/photo_playlist/photo_symbol_photo.png")
    small = scale(big, 0.6)
    return media_list_screen(big, small, rows, 4,
                             [cap("Temp/media/common/symbol_play.png"),
                              cap("Temp/setting/common/set_symbol_photo.png"),
                              cap("Temp/common/symbol_setting.png"),
                              orangify(asset("Temp/media/common/symbol_folder.png"))])


def photo_bar(im):
    bottom_caps(im, [ret_icon(), cap("Temp/common/symbol_setting.png"),
                     cap("Temp/media/common/symbol_previous_file.png"),
                     cap("Temp/media/photo/photo_symbol_rotation_left.png"),
                     cap("Temp/media/photo/photo_symbol_rotation_right.png"),
                     cap("Temp/media/common/symbol_next_file.png"),
                     cap("Temp/media/common/symbol_folder.png")])


def scr_photo_view():
    im = base()
    put(im, photo_scene().resize((560, 336)), 120, 46)
    ImageDraw.Draw(im).rectangle([120, 46, 679, 381], outline=(120, 120, 120, 255))
    put(im, ai(225).crop((240, 0, 720, 64)), 240, 0)
    text(im, (20, 92), "3 / 24", 14, GREEN, "lm")
    text(im, (20, 120), "ELBRUS_PANORAMA.JPG", 11, GREY, "lm")
    photo_bar(im)
    return im


def scr_photo_invalid():
    im = base()
    status_line(im)
    inv = asset("Temp/media/photo/symbol_invalid_big_rus.png")
    putc(im, scale(inv, 0.85), 400, 228)
    text(im, (20, 92), "4 / 24", 14, GREEN, "lm")
    text(im, (20, 120), "DACHA_2026.PNG", 11, GREY, "lm")
    photo_bar(im)
    return im


def scr_aux():
    im = base()
    status_line(im)
    put(im, asset("Temp/main/main_symbol_aux.png"), 40, 110)
    text(im, (270, 140), "AUX", 46, WHITE, "lm")
    text(im, (272, 190), "ВНЕШНИЙ ИСТОЧНИК ЗВУКА", 15, GREEN, "lm")
    hline(im, 214, 260, 780, (200, 200, 200), 2, 80)
    text(im, (272, 250), "ПОДКЛЮЧИТЕ УСТРОЙСТВО К РАЗЪЁМУ AUX", 12, GREY, "lm")
    text(im, (272, 274), "И ВКЛЮЧИТЕ ВОСПРОИЗВЕДЕНИЕ НА НЁМ", 12, GREY, "lm")
    # input gain, the same < value > pattern as the volume algorithm setting
    text(im, (272, 322), "УРОВЕНЬ ВХОДА", 13, GREEN, "lm")
    put(im, asset("Temp/common/symbol_arrow_left.png"), 500, 302)
    text(im, (600, 323), "+2", 22, WHITE, "mm")
    put(im, asset("Temp/common/symbol_arrow_right.png"), 680, 302)
    bottom_caps(im, [ret_icon(), cap("Temp/common/symbol_setting.png"), None, None, None, None,
                     cap("Temp/setting/common/set_symbol_audio.png")])
    return im


def phone_left(im):
    """Left column of the phone screens (img_00227): user icon, call, back."""
    put(im, ai(227).crop((0, 75, 160, 470)), 0, 75)


def phone_right(im):
    """Right column of the phone list (img_00228)."""
    put(im, ai(228).crop((640, 75, 800, 470)), 640, 75)


def scr_phone_call():
    """Outgoing / active call: contact card + call controls."""
    im = base()
    status_line(im, 74)
    photo = asset("Temp/phone/phone_lists/phone_card_contact_photo.png")
    put(im, photo, 40, 100)
    ImageDraw.Draw(im).rectangle([40, 100, 40 + photo.width - 1, 100 + photo.height - 1], outline=(120, 120, 120, 255))
    text(im, (250, 118), "НИКИФОРОВ МАТВЕЙ", 22, WHITE, "lm")
    text(im, (250, 160), "+7 917 123-45-67", 18, GREEN, "lm")
    text(im, (250, 196), "МОБИЛЬНЫЙ", 11, GREY, "lm")
    hline(im, 220, 240, 780, (200, 200, 200), 2, 80)
    text(im, (250, 250), "РАЗГОВОР", 15, ORANGE, "lm")
    text(im, (770, 250), "00:42", 26, WHITE, "rm")
    # call controls: mic off, keypad, handset / car, speakerphone
    xs = [290, 400, 510, 620]
    caps = [mic_off_cap(), keypad_cap(), a2dp_cap(),
            cap("Temp/phone/phone_symbol_speakerphone.png")]
    labels = ["МИКРОФОН", "КЛАВИАТУРА", "НА ТЕЛЕФОН", "ГРОМКАЯ"]
    for x, c, l in zip(xs, caps, labels):
        putc(im, c, x, 315)
        text(im, (x, 352), l, 9, GREEN, "mm")
    putc(im, asset("Temp/phone/phone_symbol_end_call.png"), 129, 330)
    bottom_caps(im, [ret_icon(), None, None, None, None, None, None])
    return im


def keypad_cap():
    body = blank_cap()
    putc(body, asset("Temp/phone/phone_symbol_grid.png"), 41, 30)
    return body


def a2dp_cap():
    body = blank_cap()
    putc(body, asset("Temp/phone/phone_symbol_a2dp.png"), 41, 30)
    return body


def mic_off_cap():
    body = blank_cap()
    d = ImageDraw.Draw(body)
    d.rounded_rectangle([35, 15, 47, 35], 6, outline=WHITE + (255,), width=3)
    d.arc([29, 22, 53, 42], 0, 180, fill=WHITE + (255,), width=3)
    d.line([(41, 42), (41, 47)], fill=WHITE + (255,), width=3)
    d.line([(28, 14), (55, 48)], fill=ORANGE + (255,), width=3)
    return body


def blank_cap():
    """Empty keycap: the face of symbol_setting is a vertical gradient, so a
    clean column copied over the gear gives the cap without an icon."""
    body = asset("Temp/common/symbol_setting.png").copy()
    col = body.crop((14, 12, 15, 52))
    for x in range(15, 69):
        body.paste(col, (x, 12))
    return body


def scr_phone_incoming():
    """Incoming call pop-up over the current source (radio)."""
    im = dim(ai(243), 0.3)
    p = panel(560, 300)
    put(im, p, 120 - 12, 92 - 12)
    photo = scale(asset("Temp/phone/phone_lists/phone_card_contact_photo.png"), 0.75)
    put(im, photo, 145, 120)
    text(im, (300, 130), "ВХОДЯЩИЙ ВЫЗОВ", 14, ORANGE, "lm")
    text(im, (300, 172), "НИКИФОРОВА МАРИЯ", 19, WHITE, "lm")
    text(im, (300, 208), "+7 927 765-43-21", 16, GREEN, "lm")
    hline(im, 262, 140, 660, (220, 220, 220), 2, 60)
    putc(im, asset("Temp/phone/phone_symbol_call.png"), 250, 326)
    text(im, (250, 376), "ОТВЕТИТЬ", 10, GREEN, "mm")
    putc(im, asset("Temp/phone/phone_symbol_end_call.png"), 550, 326)
    text(im, (550, 376), "ОТКЛОНИТЬ", 10, GREEN, "mm")
    return im


def scr_phone_card():
    """Contact card: photo, name, numbers; selected number is orange."""
    im = base()
    status_line(im, 74)
    phone_left(im)
    phone_right(im)
    put(im, list_panel(462, 380), 152, 66)
    photo = scale(asset("Temp/phone/phone_lists/phone_card_contact_photo.png"), 0.62)
    put(im, photo, 182, 96)
    text(im, (310, 116), "НИКИФОРОВ", 18, WHITE, "lm")
    text(im, (310, 146), "МАТВЕЙ", 18, WHITE, "lm")
    put(im, scale(asset("Phone/favorites_active_.png"), 1.6), 314, 178)
    text(im, (348, 190), "ИЗБРАННОЕ", 11, GREY, "lm")
    line = asset("Temp/phone/phone_lists/phone_card_line_white.png")
    put(im, line, 186, 214)
    rows = [("МОБИЛЬНЫЙ", "+7 917 123-45-67"), ("РАБОЧИЙ", "+7 8482 55-12-00"), ("ДОМАШНИЙ", "+7 8482 31-44-90")]
    for i, (k, v) in enumerate(rows):
        cy = 250 + i * 56
        if i == 0:
            bar = orangify(asset("Temp/phone/phone_lists/phone_card_select_contact.png"))
            put(im, bar.resize((400, 50)), 176, cy - 25)
            col, col2 = (60, 30, 0), (40, 20, 0)
        else:
            put(im, line, 186, cy + 26)
            col, col2 = GREEN, GREY
        text(im, (192, cy - 9), k, 10, col2, "lm")
        text(im, (192, cy + 10), v, 17, col, "lm")
    putc(im, scale(asset("Temp/phone/phone_lists/phone_card_symbol_call.png"), 0.55), 545, 250)
    scroll_arrows(im, 576, 82, 384)
    return im


def scr_phone_disconnected():
    im = base()
    status_line(im, 74)
    put(im, ai(225).crop((0, 75, 160, 470)), 0, 75)
    big = scale(asset("Temp/common/symbol_phone_connect.png"), 3)
    putc(im, big, 400, 150)
    text(im, (400, 222), "ТЕЛЕФОН НЕ ПОДКЛЮЧЁН", 20, WHITE, "mm")
    text(im, (400, 258), "ВКЛЮЧИТЕ BLUETOOTH НА ТЕЛЕФОНЕ", 12, GREEN, "mm")
    text(im, (400, 280), "И ВЫБЕРИТЕ УСТРОЙСТВО «VESTA»", 12, GREEN, "mm")
    btn = asset("Temp/setting/wifi/wi-fi_button_password.png")
    putc(im, btn, 400, 334)
    text(im, (400, 336), "НАСТРОЙКИ BLUETOOTH", 13, (60, 30, 0), "mm")
    return im


def scr_wifi_keyboard():
    """Wi-Fi password entry: full-screen keyboard (Temp/setting/wifi/keyboard_*)."""
    im = base(black=True)
    put(im, ai(225).crop((0, 0, 800, 64)), 0, 0)
    win = asset("Temp/setting/wifi/keyboard_window_password.png")
    put(im, win, 1, 68)
    text(im, (24, 96), "ПАРОЛЬ:", 13, GREEN, "lm")
    text(im, (150, 96), "VESTA_2026", 20, WHITE, "lm")
    ImageDraw.Draw(im).line([(352, 84), (352, 108)], fill=ORANGE + (255,), width=2)
    rows = ["1234567890", "QWERTYUIOP", "ASDFGHJKL-", "ZXCVBNM_.@"]
    top, kh, kw = 130, 66, 80
    hl = asset("Temp/setting/wifi/keyboard_line_horizontal.png")
    vl = asset("Temp/setting/wifi/keyboard_line_vertical_short.png").resize((3, kh - 8))
    for r, row in enumerate(rows):
        y = top + r * kh
        for c, ch in enumerate(row):
            x = c * kw
            if r == 1 and c == 2:   # pressed key
                put(im, grey_bar(kw - 4, kh - 6), x + 2, y + 3)
                text(im, (x + kw // 2, y + kh // 2), ch, 24, (40, 40, 40), "mm")
            else:
                text(im, (x + kw // 2, y + kh // 2), ch, 24, GREEN, "mm")
            if c:
                put(im, vl, x - 1, y + 4)
        put(im, hl, 0, y + kh - 2)
    y = top + 4 * kh
    keys = [(asset("Temp/setting/wifi/keyboard_symbol_capslock.png"), 0, 120),
            (None, 120, 160), (asset("Temp/setting/wifi/keyboard_symbol_space.png"), 280, 260),
            (asset("Temp/setting/wifi/keyboard_symbol_delete.png"), 540, 120),
            (asset("Temp/setting/wifi/keyboard_symbol_enter.png"), 660, 140)]
    for ic, x, w in keys:
        if x:
            put(im, vl, x - 1, y + 4)
        if ic is None:
            text(im, (x + w // 2, y + 30), "АБВ", 18, GREEN, "mm")
        else:
            putc(im, ic, x + w // 2, y + 30)
    return im


def scr_info():
    """'i' button of the main menu: versions and storage."""
    im = base()
    status_line(im)
    put(im, panel(520, 300), 140 - 12, 84 - 12)
    text(im, (400, 108), "ИНФОРМАЦИЯ", 17, WHITE, "mm")
    hline(im, 128, 150, 650, (230, 230, 230), 2, 60)
    rows = [("МОДЕЛЬ", "LADA VESTA MMC"), ("ВЕРСИЯ ПО", "G3 4120"), ("ОС", "WINDOWS CE 6.0"),
            ("BLUETOOTH", "00:1A:7D:DA:71:13"), ("SD-КАРТА", "11.2 / 14.8 ГБ"),
            ("VESTAHOME", "0.2 (2026-10-08)")]
    for i, (k, v) in enumerate(rows):
        y = 154 + i * 40
        text(im, (160, y), k, 12, GREEN, "lm")
        text(im, (640, y), v, 12, WHITE, "rm")
        if i < len(rows) - 1:
            hline(im, y + 19, 150, 650, (140, 140, 140), 1, 40)
    bottom_caps(im, [ret_icon(), None, None, None, None, None, None])
    return im


def scr_radio_pty():
    """PTY: choose a programme type; the list frame of img_00250."""
    im = ai(250)
    ImageDraw.Draw(im).rectangle([166, 76, 560, 452], fill=LIST_BG + (255,))
    rows = ["НОВОСТИ", "ТЕКУЩИЕ СОБЫТИЯ", "ИНФОРМАЦИЯ", "СПОРТ", "ОБРАЗОВАНИЕ", "КУЛЬТУРА",
            "ПОП-МУЗЫКА", "РОК-МУЗЫКА"]
    line = asset("Temp/media/common/list_media_panel_line_white.png")
    for i, r in enumerate(rows):
        cy = 96 + i * 48
        if i == 6:
            bar = asset("Temp/radio/list_station/radio_list_station_panel_select_station.png")
            put(im, bar, 168, cy - bar.height // 2)
            text(im, (176, cy), r, 17, (60, 60, 60), "lm")
        else:
            text(im, (176, cy), r, 17, GREEN, "lm")
            put(im, line, 170, cy + 22)
    # left column: autosearch/list -> 'ВСЕ ТИПЫ' / 'ВЫБРАТЬ'; right: PTY active
    put(im, Image.new("RGBA", (158, 110), (0, 0, 0, 255)), 0, 268)
    text(im, (80, 287), "ВСЕ ТИПЫ", 14, GREEN, "mm")
    side_lines(im, [310], right=False)
    sel = ai(250).crop((0, 312, 158, 358))
    put(im, sel, 0, 312)
    put(im, Image.new("RGBA", (150, 30), (0, 0, 0, 0)), 0, 320)
    ImageDraw.Draw(im).rectangle([4, 318, 154, 352], fill=(60, 60, 60, 255))
    text(im, (80, 335), "ВЫБРАТЬ", 15, WHITE, "mm")
    pty = ai(250).crop((712, 262, 800, 312))
    put(im, Image.new("RGBA", (160, 100), (0, 0, 0, 255)), 640, 262)
    text(im, (686, 287), "CB", 15, GREY, "mm")
    text(im, (750, 287), "FM1", 15, GREY, "mm")
    put(im, orange_bar(155, 46), 645, 312)
    text(im, (722, 335), "PTY", 17, (60, 30, 0), "mm")
    return im


def scr_volume():
    """Volume pop-up over the current source (Main/volume/0..50)."""
    im = dim(ai(243), 0.35)
    put(im, ai(243).crop((0, 0, 800, 64)), 0, 0)
    p = panel(520, 150)
    put(im, p, 140 - 12, 150 - 12)
    putc(im, scale(asset("Temp/common/symbol_volume.png"), 2.4), 190, 200)
    text(im, (240, 178), "ГРОМКОСТЬ", 15, GREEN, "lm")
    text(im, (630, 190), "12", 40, WHITE, "rm")
    d = ImageDraw.Draw(im)
    n, on = 30, 12
    for i in range(n):
        x = 170 + i * 16
        h = 10 + i
        c = ORANGE if i < on else (85, 85, 85)
        d.rectangle([x, 278 - h, x + 10, 278], fill=c + (255,))
    putc(im, asset("Temp/common/symbol_arrow_left.png"), 100, 225)
    putc(im, asset("Temp/common/symbol_arrow_right.png"), 700, 225)
    return im


def scr_warning():
    """Start-up warning (warning_label_*): accept to continue."""
    im = asset("Main/new_logo.png")
    im = dim(im, 0.25)
    p = panel(600, 300)
    put(im, p, 100 - 12, 90 - 12)
    d = ImageDraw.Draw(im)
    d.polygon([(160, 170), (200, 104), (240, 170)], outline=ORANGE + (255,), width=5)
    text(im, (200, 150), "!", 30, ORANGE, "mm")
    text(im, (270, 122), "ВНИМАНИЕ", 24, ORANGE, "lm")
    lines = ["НЕ ОТВЛЕКАЙТЕСЬ ОТ ДОРОГИ.", "УПРАВЛЯЙТЕ СИСТЕМОЙ ТОЛЬКО ТОГДА,",
             "КОГДА ЭТО БЕЗОПАСНО.", "ПДД ИМЕЮТ ПРИОРИТЕТ НАД УКАЗАНИЯМИ", "НАВИГАЦИИ."]
    for i, s in enumerate(lines):
        text(im, (130, 200 + i * 26), s, 13, WHITE if i == 0 else GREEN, "lm")
    btn = asset("Temp/setting/wifi/wi-fi_button_password.png")
    putc(im, btn, 400, 362)
    text(im, (400, 364), "ПРИНЯТЬ", 15, (60, 30, 0), "mm")
    return im


# ---- component kit -------------------------------------------------------

def scr_kit():
    """Buttons, switches, list rows, tabs: normal and active states."""
    W, H = 800, 1100
    im = Image.new("RGBA", (W, H), (0, 0, 0, 255))

    def head(y, s):
        text(im, (20, y), s, 14, ORANGE, "lm")
        hline(im, y + 16, 16, 784, (120, 120, 120), 1, 40)

    lf = ImageFont.truetype("arial.ttf", 11) if os.name == "nt" else font(8)

    def label(x, y, s):
        ImageDraw.Draw(im).text((x, y), s, font=lf, fill=GREY, anchor="mm")

    y = 24
    head(y, "КЛАВИШИ 83x55: ОБЫЧНАЯ / НАЖАТА (_a)")
    pairs = ["Temp/common/symbol_return", "Temp/common/symbol_setting",
             "Temp/media/common/symbol_previous_file", "Temp/media/common/symbol_play",
             "Temp/media/common/symbol_next_file", "Temp/media/common/repeat_on",
             "Temp/media/common/shuffle_on", "Temp/phone/phone_symbol_delete"]
    for i, p in enumerate(pairs):
        x = 60 + (i % 4) * 190
        yy = y + 60 + (i // 4) * 86
        putc(im, asset(p + ".png"), x, yy)
        a = p + "_a.png"
        if os.path.exists(os.path.join(MMC, a.replace("/", os.sep))):
            putc(im, asset(a), x + 90, yy)
        label(x + 45, yy + 36, os.path.basename(p))

    y = 250
    head(y, "НОВЫЕ КЛАВИШИ (ДЛЯ ЗВОНКА, ПУСТАЯ ДЛЯ ТЕКСТА)")
    for i, (c, l) in enumerate([(mic_off_cap(), "mic_off"), (keypad_cap(), "keypad"), (a2dp_cap(), "a2dp")]):
        putc(im, c, 60 + i * 140, y + 60)
        label(60 + i * 140, y + 96, l)
    t = blank_cap()
    putc(im, t, 480, y + 60)
    text(im, (480, y + 61), "ОБЗОР", 10, WHITE, "mm")
    label(480, y + 96, "blank + текст")

    y = 360
    head(y, "ПЕРЕКЛЮЧАТЕЛИ ВКЛ/ВЫКЛ И КНОПКИ-ДЕЙСТВИЯ")
    sw = asset("Temp/setting/wifi/set_wifi_switch.png")
    put(im, sw, 40, y + 34)
    text(im, (40 + sw.width // 2, y + 34 + sw.height // 2), "ВКЛ", 13, (60, 30, 0), "mm")
    put(im, grey_bar(109, 47), 170, y + 34)
    text(im, (170 + 54, y + 57), "ВЫКЛ", 13, (60, 60, 60), "mm")
    text(im, (360, y + 57), "ВЫКЛ", 13, GREEN, "mm")
    label(94, y + 92, "вкл (orange)")
    label(224, y + 92, "выкл (нажат)")
    label(360, y + 92, "выкл (текст)")
    btn = asset("Temp/setting/wifi/wi-fi_button_password.png")
    put(im, btn, 480, y + 34)
    text(im, (480 + 127, y + 58), "ВВЕДИТЕ ПАРОЛЬ", 12, (60, 30, 0), "mm")
    label(607, y + 92, "кнопка-действие 254x47")

    y = 480
    head(y, "СТРОКИ СПИСКА: ПАПКА / ФАЙЛ / ВЫБРАНА / ОРАНЖЕВАЯ")
    put(im, list_panel(560, 230), 108, y + 26)
    folder = asset("Temp/media/audio/audio_symbol_folder.png")
    list_rows(im, 150, y + 66, [("", "ПАПКА"), ("1.", "ОБЫЧНЫЙ ФАЙЛ.MP3"), ("2.", "ВЫБРАННЫЙ ФАЙЛ.MP3")],
              sel=2, icon=[folder, None, None], width=420)
    put(im, orange_bar(440, 46), 136, y + 66 + 141 - 23)
    text(im, (150, y + 66 + 141), "АКТИВНЫЙ ПУНКТ (ТЕЛЕФОН, НАСТРОЙКИ)", 15, (60, 30, 0), "lm")
    scroll_arrows(im, 610, y + 40, y + 190)

    y = 770
    head(y, "БОКОВОЕ МЕНЮ: ВКЛАДКА ОБЫЧНАЯ / АКТИВНАЯ, РАЗДЕЛИТЕЛИ")
    put(im, ai(238).crop((645, 120, 800, 330)), 40, y + 30)
    put(im, ai(296).crop((0, 250, 157, 395)), 260, y + 30)
    label(118, y + 250, "справа (настройки)")
    label(338, y + 250, "слева (настройки)")
    put(im, orange_bar(155, 64), 470, y + 60)
    text(im, (547, y + 92), "ВКЛАДКА", 13, (60, 30, 0), "mm")
    label(547, y + 140, "orange_bar 155x64")
    put(im, ai(232).crop((0, 283, 158, 337)), 470, y + 160)
    label(547, y + 225, "переключатель USB / SD")
    sl = asset("Temp/media/common/symbol_slider.png")
    put(im, sl, 680, y + 70)
    label(700, y + 140, "ползунок")

    y = 1040
    head(y, "СТРОКА СОСТОЯНИЯ И РАЗДЕЛИТЕЛИ")
    hline(im, y + 40, 16, 784, ORANGE, 2, 120)
    hline(im, y + 52, 16, 784, (200, 200, 200), 2, 80)
    return im


SCREENS = [
    ("01_main_page2", "Главное меню, стр. 2", scr_main_page2),
    ("02_video_player", "Видео: воспроизведение", scr_video_player),
    ("03_video_list", "Видео: список файлов", scr_video_list),
    ("04_photo_view", "Фото: просмотр", scr_photo_view),
    ("05_photo_list", "Фото: список файлов", scr_photo_list),
    ("06_photo_invalid", "Фото: формат не поддерживается", scr_photo_invalid),
    ("07_aux", "AUX", scr_aux),
    ("08_phone_call", "Телефон: разговор", scr_phone_call),
    ("09_phone_incoming", "Телефон: входящий вызов", scr_phone_incoming),
    ("10_phone_card", "Телефон: карточка контакта", scr_phone_card),
    ("11_phone_disconnected", "Телефон: не подключён", scr_phone_disconnected),
    ("12_wifi_keyboard", "Wi-Fi: ввод пароля", scr_wifi_keyboard),
    ("13_info", "Информация", scr_info),
    ("14_radio_pty", "Радио: выбор PTY", scr_radio_pty),
    ("15_volume", "Громкость (поверх экрана)", scr_volume),
    ("16_warning", "Предупреждение при запуске", scr_warning),
]


def contact_sheet(items, path, cols=3, w=266, h=160):
    lab = 18
    rows = (len(items) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * w, rows * (h + lab)), (0, 0, 0))
    d = ImageDraw.Draw(sheet)
    lf = ImageFont.truetype("arial.ttf", 12) if os.name == "nt" else font(10)
    for i, (name, title, im) in enumerate(items):
        x, y = (i % cols) * w, (i // cols) * (h + lab)
        sheet.paste(im.convert("RGB").resize((w, h), Image.LANCZOS), (x, y))
        d.text((x + 4, y + h + 2), "%s  %s" % (name[:2], title), font=lf, fill=(255, 220, 120))
    sheet.save(path)


def main():
    os.makedirs(OUT, exist_ok=True)
    done = []
    for name, title, fn in SCREENS:
        im = fn()
        im.convert("RGB").save(os.path.join(OUT, name + ".png"))
        done.append((name, title, im))
        print("%-24s %s" % (name, title))
    scr_kit().convert("RGB").save(os.path.join(OUT, "kit.png"))
    contact_sheet(done, os.path.join(OUT, "all_screens_new.png"))
    print("->", OUT)


if __name__ == "__main__":
    main()
