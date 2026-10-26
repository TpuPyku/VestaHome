#!/usr/bin/env python3
"""make_vh_v07_mockup.py - what VestaHome can take from OSMapsCE design v3 -> vesta-home-v0.7-ideas.html + .pdf.

The user 2026-10-26: "some of its decisions can go into VestaHome's design, work it out with a VestaHome mockup";
the accents and the cursor's (the car's) glow come from VestaHome's theme. Each idea: the screen as it is now (the
PC build's shot, docs/shots, tools/make_shots.py) and the same screen with it; the accent is the user's theme
(screen.accent of the PC build's VestaHome.ini). The OSMapsCE pieces (the vector map, cards, arrows) are
make_osm_v3_mockup.py's.

    python VestaHome/skin/make_vh_v07_mockup.py
"""
import importlib.util
import os
import subprocess

from PIL import Image, ImageDraw, ImageEnhance, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
SHOTS = os.path.join(HERE, '..', 'docs', 'shots')
OUT = os.path.join(HERE, 'vh_v07')
spec = importlib.util.spec_from_file_location('osm', os.path.join(HERE, 'make_osm_v3_mockup.py'))
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
K, W, H = m.K, m.W, m.H


def shot(name):
    return Image.open(os.path.join(SHOTS, name)).convert('RGB').resize((W * K, H * K), Image.LANCZOS)


def clear_middle(im, src, y0=60, y1=404):
    """The middle of the screen back to VestaHome's background (the mosaic of an empty part of the shot)."""
    bg = shot('04_radio.png').crop((0, 140 * K, W * K, 200 * K))   # an empty band of the mosaic
    for y in range(y0 * K, y1 * K, bg.height):
        im.paste(bg.crop((0, 0, W * K, min(bg.height, y1 * K - y))), (0, y))


def vh_card(im, box, title=None, icon_col=None):
    """A VestaHome widget tile: PANEL, radius 16, the title row at the top left; with the soft shadow of v3 and no
    edge (VestaHome's background is not a map)."""
    hair, m.HAIR = m.HAIR, False
    m.card(im, box, fill=(0x22, 0x25, 0x2a), alpha=255, radius=16, border=None, shadow=True)
    m.HAIR = hair
    if title:
        x0, y0 = box[0], box[1]
        d = ImageDraw.Draw(im)
        d.ellipse([(x0 + 16) * K, (y0 + 16) * K, (x0 + 30) * K, (y0 + 30) * K], outline=icon_col or m.ACCENT, width=2 * K)
        m.text(im, (x0 + 38, y0 + 23), title, 12, m.MUTED, True, 'lm')


def limit_sign(im, cx, cy, v, r=22):
    d = ImageDraw.Draw(im)
    d.ellipse([(cx - r) * K, (cy - r) * K, (cx + r) * K, (cy + r) * K], fill=(255, 255, 255))
    d.ellipse([(cx - r + 4) * K, (cy - r + 4) * K, (cx + r - 4) * K, (cy + r - 4) * K], outline=(214, 30, 30),
              width=5 * K)
    m.text(im, (cx, cy), str(v), 16, (10, 10, 10), True, 'mm')


def nav_widget(im, box):
    """Idea 1: the "Navigation" widget (OSMapsCE through the add-on API while a route is on): the live map with the
    route, on it the next turn's card and the trip line."""
    x0, y0, x1, y1 = box
    coords = m.RT['routes'][0]['coords']
    ci = 34
    lon, lat = coords[ci]
    view = m.View(lon, lat, 15.8, m.course_at(coords, ci), (x0 + x1) / 2 + 60, y0 + (y1 - y0) * 0.72)
    mp = m.draw_map(view, m.NIGHT, traffic=False)
    m.draw_route(mp, view, 0, traffic=True, start=ci)
    m.car(mp, (x0 + x1) / 2 + 60, y0 + (y1 - y0) * 0.72)
    mask = Image.new('L', mp.size, 0)
    ImageDraw.Draw(mask).rounded_rectangle([x0 * K, y0 * K, x1 * K, y1 * K], 16 * K, fill=255)
    sh = Image.new('L', im.size, 0)
    ImageDraw.Draw(sh).rounded_rectangle([x0 * K, (y0 + 6) * K, x1 * K, (y1 + 6) * K], 16 * K, fill=120)
    im.paste(Image.new('RGB', im.size, (0, 0, 0)), (0, 0), sh.filter(ImageFilter.GaussianBlur(10 * K)))
    im.paste(mp, (0, 0), mask)
    # the turn, as OSMapsCE's card, smaller
    m.card(im, (x0 + 12, y0 + 12, x0 + 232, y0 + 116), radius=16)
    ImageDraw.Draw(im).rounded_rectangle([(x0 + 22) * K, (y0 + 22) * K, (x0 + 82) * K, (y0 + 82) * K], 14 * K,
                                         fill=m.ACCENT)
    m.turn_arrow(im, x0 + 52, y0 + 48, 'right', 0.8, m.INK)
    m.text(im, (x0 + 94, y0 + 26), '350', 32, m.TEXT, True)
    m.text(im, (x0 + 94 + ImageDraw.Draw(im).textlength('350', font=m.font(32, True)) / K + 4, y0 + 40), 'м', 16,
           m.MUTED, True)
    m.text(im, (x0 + 94, y0 + 66), 'направо', 12, m.MUTED)
    m.text(im, (x0 + 22, y0 + 92), 'ул. Льва Толстого', 14, m.TEXT, True)
    m.card(im, (x0 + 12, y1 - 52, x1 - 12, y1 - 12), radius=20)
    for i, (big, small) in enumerate((('14:41', 'прибытие'), ('9 мин', 'с пробками 9–12'), ('4,1 км', 'осталось'))):
        cx = x0 + 70 + i * 150
        m.text(im, (cx, y1 - 38), big, 15, m.TEXT, True, 'mm')
        m.text(im, (cx, y1 - 22), small, 10, m.MUTED, False, 'mm')
    m.text(im, (x1 - 18, y0 + 22), 'OSMapsCE', 11, m.MUTED, True, 'rm')


def screen_home():
    im = shot('01_home.png')
    clear_middle(im, im)
    nav_widget(im, (16, 64, 524, 390))
    # idea 2: one big number per widget - the speed with the camera's limit beside it
    vh_card(im, (536, 64, 784, 222), 'Скорость · GPS')
    m.text(im, (556, 150), '47', 54, m.TEXT, True, 'lm')
    m.text(im, (640, 166), 'км/ч', 15, m.MUTED, True, 'lm')
    limit_sign(im, 744, 150, 60)
    m.text(im, (556, 200), 'камера 60 · 450 м', 12, m.MUTED, False, 'lm')
    vh_card(im, (536, 234, 784, 390), 'Погода · Самара')
    m.text(im, (556, 312), '+12°', 46, m.TEXT, True, 'lm')
    m.text(im, (556, 356), 'облачно · ветер 3 м/с', 12, m.MUTED, False, 'lm')
    d = ImageDraw.Draw(im)
    for i in range(3):   # the page dots, as VestaHome's
        x = 372 + i * 32
        d.rounded_rectangle([x * K, 398 * K, (x + 26) * K, 404 * K], 3 * K, fill=m.ACCENT if i == 0 else m.LINE)
    return finish(im, 'home_nav')


def screen_radio():
    """Idea 3: the next turn in the top line on every screen while OSMapsCE guides (VestaHome in front)."""
    im = shot('04_radio.png')
    m.pill(im, (110, 10, 430, 46), '', m.PANEL2, m.TEXT)
    ImageDraw.Draw(im).rounded_rectangle([114 * K, 14 * K, 142 * K, 42 * K], 14 * K, fill=m.ACCENT)
    m.turn_arrow(im, 128, 26, 'right', 0.36, m.INK)
    m.text(im, (152, 28), '350 м', 17, m.TEXT, True, 'lm')
    m.text(im, (210, 28), 'ул. Льва Толстого · 14:41', 13, m.MUTED, False, 'lm')
    return finish(im, 'radio_chip')


def screen_power():
    """Idea 4: windows as a sheet from below with the shadow of v3, the main action in the accent at the right."""
    im = shot('31_settings_info.png')
    im = ImageEnhance.Brightness(im).enhance(0.45)
    box = (16, 196, 784, 404)
    m.HAIR = False
    m.card(im, box, fill=(0x1c, 0x1f, 0x24), alpha=255, radius=24)
    d = ImageDraw.Draw(im)
    d.arc([40 * K, 216 * K, 72 * K, 248 * K], -60, 240, fill=m.TEXT, width=3 * K)
    d.line([(56 * K, 212 * K), (56 * K, 232 * K)], fill=m.TEXT, width=3 * K)
    m.text(im, (88, 232), 'VestaHome', 22, m.TEXT, True, 'lm')
    m.text(im, (40, 274), 'Свернуть — останется открытым позади.  Штатная — вернётся штатная оболочка.', 13, m.MUTED)
    for i, (t, main) in enumerate((('Отмена', False), ('Свернуть', False), ('Штатная', False), ('Выход', True))):
        x = 40 + i * 184
        m.card(im, (x, 318, x + 168, 380), fill=m.ACCENT if main else m.PANEL2, alpha=255, radius=22, shadow=False,
               border=None)
        m.text(im, (x + 84, 349), t, 16, m.INK if main else m.TEXT, True, 'mm')
    return finish(im, 'power_sheet')


def osm_accent(code):
    name, col = m.ACCENTS[code]
    m.ACCENT = col
    f = m.screen_drive(False)
    os.replace(os.path.join(m.OUT, f), os.path.join(OUT, 'osm_%d.png' % code))
    m.ACCENT = m.ACCENTS[130][1]
    return 'osm_%d.png' % code, name


def finish(im, name):
    im.resize((W, H), Image.LANCZOS).save(os.path.join(OUT, name + '.png'))
    return name + '.png'


PAGE = """<!doctype html><html lang="ru"><head><meta charset="utf-8"><title>VestaHome — идеи 0.7</title>
<style>
@page {{ size: A4 landscape; margin: 12mm; }}
body {{ font-family: 'Golos Text', 'Segoe UI', sans-serif; background: #0b0c0e; color: #f3f1ec; margin: 0; padding: 24px; }}
h1 {{ font-size: 26px; margin: 0 0 6px; }} h2 {{ font-size: 18px; margin: 28px 0 8px; color: {acc}; }}
p, li {{ color: #a3a8b0; font-size: 13px; line-height: 1.5; max-width: 1150px; }} b {{ color: #f3f1ec; }}
.pair {{ display: grid; grid-template-columns: 1fr 1fr; gap: 18px; break-inside: avoid; }}
.three {{ display: grid; grid-template-columns: repeat(3, 1fr); gap: 14px; break-inside: avoid; }}
img {{ width: 100%; border-radius: 10px; border: 1px solid #34383f; }}
.cap {{ font-size: 12px; color: #5d636c; margin: 4px 0 0; }}
</style></head><body>
<h1>VestaHome 0.7 — что взять из дизайна OSMapsCE v3</h1>
<p>Слева — экран сейчас (ПК-сборка), справа — с идеей. Акцент — из темы VestaHome (у пользователя «{accname}»):
им же в OSMapsCE окрашены машина и её подсветка, квадрат манёвра, активные кнопки и главная кнопка окна.</p>
{body}
<h2>Правило обводок (обе программы)</h2>
<p>Как в современных системах (Material 3, iOS, Škoda Flow): у кнопок и плиток <b>обводок нет</b> — их отделяют
заливка и тень; главное действие — заливка акцентом. Рамка цвета акцента 2 px — только у выбранного, но не
главного (выбранный вариант, поле ввода, текущая точка). Карточки, которые лежат поверх <b>ночной карты</b>, получают
едва заметную кромку 1 px (тень на тёмной карте не видна); у VestaHome фон не карта — кромки нет. VestaHome уже
так нарисован; OSMapsCE — с этой версии.</p>
<h2>Правило темы для OSMapsCE</h2>
<p>OSMapsCE берёт акцент из темы VestaHome: при подключении к VestaHome — из его сообщения (API дополнений: тема и
её смена), без VestaHome — из <b>screen.accent</b> файла <b>\\SDMMC\\VestaHome\\VestaHome.ini</b> (как заглушка для
картинки загрузки); без файла — Марс. Им окрашены: машина и её мягкая подсветка, квадрат манёвра, «вкл.» кнопок,
главная кнопка карточек, выбранный вариант и поле ввода. Цвета пробок и маршрута от темы не зависят.</p>
<div class="three">{accents}</div>
</body></html>"""

IDEAS = (
    ('home_nav', '01_home.png', '1. Виджет «Навигация» и одна большая цифра',
     'Пока в OSMapsCE идёт маршрут, она через API дополнений показывает на главной VestaHome виджет 2×2: живая карта по '
     'курсу, на ней карточка манёвра (стрелка в квадрате акцента, «350 м», улица) и строка поездки (прибытие, минуты с '
     'вилкой по пробкам, километры) — то же, что в самой OSMapsCE. Без маршрута — нынешний виджет «Карта OSM». '
     'Виджеты справа — по правилу «одна главная цифра»: скорость крупно, рядом знак лимита камеры; погода — градусы. '
     'Плитки получают мягкую тень (готовые 9-частные картинки тени — дёшево для CE).'),
    ('radio_chip', '04_radio.png', '2. Манёвр в верхней строке на любом экране',
     'Если OSMapsCE ведёт по маршруту, а впереди VestaHome (радио, музыка, телефон), в верхней строке рядом с названием '
     'экрана — «таблетка»: стрелка, расстояние, улица и время прибытия; касание — OSMapsCE вперёд. Данные — та же '
     'подписка API, что и у виджета.'),
    ('power_sheet', '37_power.png', '3. Окна — «лист» снизу с тенью',
     'Окно питания, подтверждения («Забыть сеть?», «Сбросить ошибки?»), входящий вызов — не по центру, а карточкой '
     'над нижней панелью на всю ширину, с тенью; главное действие — справа в цвет акцента, остальные — тёмные. Палец '
     'водителя ближе к низу экрана; так же сделано долгое нажатие в OSMapsCE.'),
)


def main():
    os.makedirs(OUT, exist_ok=True)
    m.FEATS = m.load_tiles()
    import json
    m.RT = json.load(open(os.path.join(m.OUT, 'route.json'), encoding='utf-8'))
    accname, m.ACCENT = m.theme_accent()
    shots = {'home_nav': screen_home(), 'radio_chip': screen_radio(), 'power_sheet': screen_power()}
    for name, before, _, _ in IDEAS:
        Image.open(os.path.join(SHOTS, before)).save(os.path.join(OUT, 'now_' + before))
    body = ''.join('<h2>%s</h2><p>%s</p><div class="pair"><div><img src="vh_v07/now_%s"><p class="cap">сейчас</p></div>'
                   '<div><img src="vh_v07/%s"><p class="cap">идея</p></div></div>' % (t, txt, before, shots[n])
                   for n, before, t, txt in IDEAS)
    acc = []
    for code in (130, 372, 476):
        f, name = osm_accent(code)
        acc.append('<div><img src="vh_v07/%s"><p class="cap">OSMapsCE в теме «%s»</p></div>' % (f, name))
    html = os.path.join(HERE, 'vesta-home-v0.7-ideas.html')
    open(html, 'w', encoding='utf-8').write(PAGE.format(body=body, accents=''.join(acc), accname=accname,
                                                         acc='rgb(%d,%d,%d)' % m.theme_accent()[1]))
    edge = r'C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe'
    if os.path.exists(edge):
        subprocess.run([edge, '--headless', '--disable-gpu', '--no-pdf-header-footer',
                        '--print-to-pdf=' + os.path.join(HERE, 'vesta-home-v0.7-ideas.pdf'),
                        'file:///' + html.replace('\\', '/')], check=False, timeout=120)
    print('->', os.path.normpath(html), '(accent %s)' % accname)


if __name__ == '__main__':
    main()
