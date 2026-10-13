"""Mockups of the new YaMapsCE / OSMapsCE skin in the VestaHome style -> maps-skin-v2.html + .pdf.

Screens 800 x 480 drawn with PIL on real cached maps (OSM tiles of OSMapsCE, a Yandex picture of YaMapsCE),
the button signs are the ones of YaMaps/native/make_images.py. The PDF is printed by headless Edge.

    python make_maps_mockup.py
"""
import importlib.util
import os
import subprocess
from PIL import Image, ImageDraw, ImageFont, ImageEnhance

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
OUT = os.path.join(HERE, 'maps_mockup')
spec = importlib.util.spec_from_file_location('mi', os.path.join(ROOT, 'YaMaps', 'native', 'make_images.py'))
mi = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mi)

INK, PANEL, PANEL2, LINE = (0x0b, 0x0c, 0x0e), (0x17, 0x19, 0x1c), (0x22, 0x25, 0x2a), (0x34, 0x38, 0x3f)
TEXT, MUTED, DIM, ORANGE = (0xf3, 0xf1, 0xec), (0xa3, 0xa8, 0xb0), (0x5d, 0x63, 0x6c), (0xff, 0x6b, 0x1a)
FONTS = os.path.join(os.environ.get('WINDIR', r'C:\Windows'), 'Fonts')
K = 4


def font(size, bold=False):
    return ImageFont.truetype(os.path.join(FONTS, 'tahomabd.ttf' if bold else 'tahoma.ttf'), size)


# ---------------------------------------------------------------- maps
def osm_map():
    z, x0, y0 = 15, 19816, 10235
    im = Image.new('RGB', (1024, 768))
    for i in range(4):
        for j in range(3):
            x, y = x0 + i, y0 + j
            p = os.path.join(ROOT, 'OSMapsCE', 'OSMapsCE', 'Cache', str(z), '%d_%d' % (x // 64, y // 64),
                             '%d_%d.img' % (x, y))
            im.paste(Image.open(p).convert('RGB'), (i * 256, j * 256))
    return im.crop((112, 144, 912, 624))


def yandex_map():
    d = os.path.join(ROOT, 'YaMaps', 'YaMapsCE', 'Cache', '15')
    name = '16908_11681'
    sub = '%d_%d' % (16908 // 64 // 2, 11681 // 64 // 2)
    for s in os.listdir(d):
        if os.path.exists(os.path.join(d, s, '0_0_' + name + '.img')):
            sub = s
            break
    base = Image.open(os.path.join(d, sub, '0_0_' + name + '.img')).convert('RGBA')
    trf = Image.open(os.path.join(d, sub, '0_1_' + name + '.img')).convert('RGBA')
    return Image.alpha_composite(base, trf).convert('RGB')


# ---------------------------------------------------------------- pieces
def button(size, sign, state='n', label=None):
    """A square tile: 'n' normal, 'on' (the mode is on: orange sign), 'p' pressed (orange tile)."""
    im = Image.new('RGBA', (50 * K, 50 * K), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    fill, border = (ORANGE, None) if state == 'p' else (PANEL, LINE)
    d.rounded_rectangle([K, K, 49 * K - 1, 49 * K - 1], radius=12 * K, fill=fill + (235,),
                        outline=border, width=2 * K if border else 0)
    c = INK if state == 'p' else ORANGE if state == 'on' else MUTED if sign == 'gps_none' else TEXT
    mi.sign(d, sign, c, fill)
    return im.resize((size, size), Image.LANCZOS)


def sign_only(size, sign, c, bg):
    im = Image.new('RGBA', (50 * K, 50 * K), bg + (0,))
    mi.sign(ImageDraw.Draw(im), sign, c, bg)
    return im.resize((size, size), Image.LANCZOS)


def car(img, x, y):
    big = Image.new('RGBA', (60 * K, 60 * K), (0, 0, 0, 0))
    d = ImageDraw.Draw(big)
    pts = [(30, 6), (48, 52), (30, 42), (12, 52)]
    d.polygon([(px * K, py * K) for px, py in pts], fill=TEXT)
    pts = [(30, 12), (43, 46), (30, 38), (17, 46)]
    d.polygon([(px * K, py * K) for px, py in pts], fill=ORANGE)
    big = big.resize((60, 60), Image.LANCZOS)
    img.paste(big, (x - 30, y - 30), big)


def shade(img, box, alpha=215):
    ov = Image.new('RGBA', img.size, (0, 0, 0, 0))
    ImageDraw.Draw(ov).rectangle(box, fill=INK + (alpha,))
    return Image.alpha_composite(img.convert('RGBA'), ov)


def top_bar(img, gps='9 спутн. · 60 км/ч', info='Пробки · 2 мин назад', clock='14:56', h=44):
    img = shade(img, [0, 0, 799, h - 1])
    d = ImageDraw.Draw(img)
    s = sign_only(26, 'gps_on', ORANGE, INK)
    img.paste(s, (12, (h - 26) // 2), s)
    d.text((44, h // 2), gps, font=font(17), fill=TEXT, anchor='lm')
    d.text((330, h // 2), info, font=font(16), fill=MUTED, anchor='lm')
    d.text((786, h // 2 + 1), clock, font=font(30, True), fill=TEXT, anchor='rm')
    return img


def put(img, im, x, y):
    img.paste(im, (x, y), im)


DOCK = [('set', 'НАСТРОЙКИ'), ('night', 'НОЧЬ'), ('trf', 'ПРОБКИ'), ('layers', 'СЛОИ'), ('exit', 'ВЫХОД')]


def dock(img, active=('trf',), pressed=None, hide=0.65, items=DOCK):
    """The bar of the VestaHome bottom-bar kind, slid up on a tap; the orange line shrinks to the hiding."""
    img = shade(img, [0, 404, 799, 479], 235)
    d = ImageDraw.Draw(img)
    d.rectangle([0, 404, 799, 405], fill=LINE)
    d.rectangle([0, 404, int(800 * hide), 405], fill=ORANGE)
    n = len(items)
    w = 784 // n
    for i, (sg, cap) in enumerate(items):
        x = 8 + i * w
        on = pressed == sg
        if on:
            d.rounded_rectangle([x + 4, 412, x + w - 4, 472], radius=12, fill=ORANGE)
        c = INK if on else ORANGE if sg in active else TEXT
        s = sign_only(34, sg, c, ORANGE if on else INK)
        put(img, s, x + w // 2 - 17, 416)
        d.text((x + w // 2, 462), cap, font=font(13, True), fill=c, anchor='mm')
    return img


def zoom_and_gps(img, gps_state='on'):
    put(img, button(72, 'plus'), 712, 248)
    put(img, button(72, 'minus'), 712, 328)
    put(img, button(72, {'on': 'gps_on', 'off': 'gps_off', 'none': 'gps_none'}[gps_state],
                    'on' if gps_state == 'on' else 'n'), 16, 328)
    return img


def night(img):
    img = ImageEnhance.Brightness(img.convert('RGB')).enhance(0.42)
    tint = Image.new('RGB', img.size, (20, 26, 40))
    return Image.blend(img, tint, 0.25)


# ---------------------------------------------------------------- screens
def scr_drive():
    img = osm_map().convert('RGBA')
    car(img, 400, 300)
    img = top_bar(img)
    return zoom_and_gps(img)


def scr_tap():
    img = osm_map().convert('RGBA')
    car(img, 400, 300)
    img = top_bar(img)
    img = zoom_and_gps(img)
    return dock(img, active=('trf',), pressed=None)


def scr_tap_osm():
    img = osm_map().convert('RGBA')
    car(img, 400, 300)
    img = top_bar(img, info='OSM · пробки 2ГИС')
    img = zoom_and_gps(img, 'off')
    items = [('set', 'НАСТРОЙКИ'), ('night', 'НОЧЬ'), ('trf', 'ПРОБКИ'), ('tsrc_2g', 'ИСТОЧНИК'), ('exit', 'ВЫХОД')]
    return dock(img, active=('trf',), pressed='night', hide=0.9, items=items)


def scr_night():
    img = night(osm_map()).convert('RGBA')
    car(img, 400, 300)
    img = top_bar(img, clock='22:41')
    return zoom_and_gps(img)


def scr_settings():
    img = shade(osm_map().convert('RGBA'), [0, 0, 799, 479], 245)
    d = ImageDraw.Draw(img)
    d.rectangle([0, 0, 799, 55], fill=INK)
    put(img, sign_only(32, 'left', TEXT, INK), 14, 12)
    d.text((60, 28), 'НАСТРОЙКИ КАРТЫ', font=font(22, True), fill=TEXT, anchor='lm')
    d.text((786, 29), '14:56', font=font(30, True), fill=TEXT, anchor='rm')
    d.rectangle([0, 55, 799, 56], fill=LINE)
    d.rounded_rectangle([16, 68, 784, 468], radius=16, fill=PANEL, outline=LINE, width=2)
    rows = [('Порт GPS', 'фикс · спутников 9', 'COM5'), ('Скорость порта', 'или как у оболочки', '115200'),
            ('Кэш карт', 'занято 212 МБ', '500 МБ')]
    y = 76
    for label, sub, val in rows:
        d.text((36, y + 20), label, font=font(19, True), fill=TEXT, anchor='lm')
        d.text((36, y + 42), sub, font=font(15), fill=MUTED, anchor='lm')
        d.rounded_rectangle([512, y + 8, 760, y + 52], radius=10, fill=INK, outline=LINE, width=2)
        put(img, sign_only(30, 'left', TEXT, INK), 520, y + 15)
        put(img, sign_only(30, 'right', TEXT, INK), 722, y + 15)
        d.text((636, y + 31), val, font=font(20, True), fill=TEXT, anchor='mm')
        d.rectangle([24, y + 62, 776, y + 62], fill=LINE)
        y += 64
    d.text((36, y + 24), 'Найти GPS', font=font(19, True), fill=TEXT, anchor='lm')
    d.text((36, y + 46), 'перебрать COM1…COM9 и скорости', font=font(15), fill=MUTED, anchor='lm')
    d.rounded_rectangle([616, y + 12, 760, y + 56], radius=10, fill=PANEL2, outline=LINE, width=2)
    d.text((688, y + 34), 'ПОИСК', font=font(16, True), fill=TEXT, anchor='mm')
    y += 72
    for i, (t, c) in enumerate((('NMEA: 1 284 строк, с ошибкой 0', TEXT),
                                ('Координаты: 55.78222, 37.70411 (9 спутн.)', TEXT),
                                ('Запросов сегодня: 112 из 900', MUTED))):
        d.text((36, y + 14 + i * 26), t, font=font(16), fill=c, anchor='lm')
    return img


def scr_yamaps():
    """YaMapsCE: the old layout kept (status line on top, 600 x 450 picture, the right panel), only the design
    is new - drawn from the real pictures of YaMaps/YaMapsCE/Images (make_images.py)."""
    imgs = os.path.join(ROOT, 'YaMaps', 'YaMapsCE', 'Images')
    img = Image.open(os.path.join(imgs, 'MapGL.png')).convert('RGBA')
    img.paste(yandex_map(), (2, 28))
    car(img, 302, 290)
    for x, y, n in ((610, 28, 'Bset_N'), (675, 28, 'Bnight_off_N'), (740, 28, 'Bexit_N'), (610, 93, 'Btrf_on_N'),
                    (675, 93, 'Blayers_N'), (740, 93, 'Bgps_on_N'), (615, 290, 'Bleft_N'), (735, 290, 'Bright_N'),
                    (675, 230, 'Bup_N'), (675, 350, 'Bdown_N'), (640, 420, 'Bminus_N'), (710, 420, 'Bplus_P')):
        img.paste(Image.open(os.path.join(imgs, n + '.png')).convert('RGBA'), (x, y))
    d = ImageDraw.Draw(img)
    f22, f15 = ImageFont.truetype(os.path.join(FONTS, 'tahomabd.ttf'), 22), font(15)
    d.text((10, 14), 'Пн 13.10.2026  14:56:07', font=f22, fill=TEXT, anchor='lm')
    d.text((330, 14), 'GPS: 9 спутн., 60 км/ч', font=f15, fill=TEXT, anchor='lm')
    d.text((795, 14), 'Обновлено 2 мин назад', font=f15, fill=TEXT, anchor='rm')
    for i, (t, c) in enumerate((('Масштаб: 15', TEXT), ('Слой: схема', TEXT), ('Запросов: 112 из 900', MUTED))):
        d.text((612, 165 + i * 20), t, font=f15, fill=c, anchor='lm')
    return img


def sheet():
    names = [('set', 'Настройки'), ('night', 'Ночь'), ('trf', 'Пробки'), ('layers', 'Слои'), ('tsrc_ya', 'Яндекс'),
             ('tsrc_2g', '2ГИС'), ('gps_on', 'По курсу'), ('gps_off', 'Север'), ('gps_none', 'Без слежения'),
             ('plus', 'Крупнее'), ('minus', 'Мельче'), ('exit', 'Выход')]
    img = Image.new('RGB', (800, 300), INK)
    d = ImageDraw.Draw(img)
    for row, (state, cap) in enumerate((('n', 'обычная'), ('on', 'режим включён'), ('p', 'нажата'))):
        d.text((10, 40 + row * 92), cap, font=font(14), fill=MUTED, anchor='lm')
        for i, (sg, t) in enumerate(names):
            if state == 'on' and sg not in ('night', 'trf', 'gps_on', 'tsrc_ya', 'tsrc_2g'):
                continue
            put(img, button(56, sg, state), 120 + i * 56, 12 + row * 92)
    for i, (sg, t) in enumerate(names):
        d.text((148 + i * 56, 290), t, font=font(10), fill=MUTED, anchor='mm')
    return img


SCREENS = [
    ('drive', scr_drive, 'OSMapsCE. Едем: карта на весь экран',
     'Карта 800 × 480. Поверх — тёмная строка статуса 44 px (GPS, свежесть карты/пробок, крупные часы, как в '
     'верхнем баре VestaHome). Всегда на месте только то, что нужно на ходу: «+» / «−» 72 px справа и режим '
     'слежения GPS 72 px слева (оранжевый — «по курсу»). Стрелок сдвига нет: карту двигают пальцем.'),
    ('tap', scr_tap, 'OSMapsCE. Касание карты: выезжает нижняя панель',
     'Касание карты (без сдвига) поднимает панель того же вида, что нижний бар VestaHome: Настройки, Ночь, '
     'Пробки, Слои, Выход — значок + подпись. Включённый режим — оранжевым. Оранжевая полоска сверху '
     'укорачивается: через 6 с без касаний панель уезжает вниз. Повторное касание карты прячет её сразу.'),
    ('tap_osm', scr_tap_osm, 'OSMapsCE: панель с выбором источника пробок',
     'В OSMapsCE вместо «Слои» — источник пробок (Яндекс / 2ГИС, кнопка показывает текущий). Нажатие — '
     'сплошная оранжевая заливка (здесь «Ночь»). Режим GPS «север» — белая стрелка.'),
    ('night', scr_night, 'OSMapsCE ночью',
     'Ночной фильтр карты как сейчас (затемнение), кнопки и строка те же — они и так тёмные.'),
    ('settings', scr_settings, 'Настройки (оба приложения)',
     'Экран настроек в стиле «Настройки → Общие» VestaHome: строки со стрелками (порт COM1…COM9, скорость, '
     'размер кэша), поиск GPS, живое состояние приёмника. Применяется сразу, без кнопки «Применить»; «назад» — '
     'стрелка слева вверху.'),
    ('yamaps', scr_yamaps, 'YaMapsCE: раскладка прежняя, оформление новое',
     'Решение: у YaMapsCE остаются верхняя строка статуса, картинка 600 × 450 и правая панель на прежних местах '
     '(без мозаики и роста запросов). Новые — фон, кнопки 50 px, цвета текста, маркер машины (оранжевый) и экран '
     'настроек. Кадр собран из настоящих новых картинок YaMapsCE/Images.'),
]


def main():
    os.makedirs(OUT, exist_ok=True)
    for key, fn, _, _ in SCREENS:
        fn().convert('RGB').save(os.path.join(OUT, key + '.png'))
    sheet().save(os.path.join(OUT, 'buttons.png'))
    blocks = ''.join(
        '<section><h2>%s</h2><img src="maps_mockup/%s.png"><p>%s</p></section>' % (t, k, p)
        for k, _, t, p in SCREENS)
    html = '''<!doctype html><html lang="ru"><head><meta charset="utf-8"><title>Скин карт v2</title>
<style>
@page { size: A4 landscape; margin: 12mm; }
body { font-family: Tahoma, sans-serif; background: #0b0c0e; color: #f3f1ec; margin: 0; }
header { padding: 8mm 0 4mm; } h1 { margin: 0; font-size: 22pt; } h1 b { color: #ff6b1a; }
header p, section p { color: #a3a8b0; font-size: 10.5pt; line-height: 1.45; max-width: 250mm; }
section { page-break-inside: avoid; break-inside: avoid; margin: 0 0 6mm; }
section + section { page-break-before: always; break-before: page; }
h2 { font-size: 14pt; margin: 0 0 3mm; border-left: 4px solid #ff6b1a; padding-left: 3mm; }
img { width: 200mm; display: block; border: 1px solid #34383f; border-radius: 3mm; }
ul { color: #a3a8b0; font-size: 10.5pt; line-height: 1.5; }
</style></head><body>
<header><h1>YaMapsCE · OSMapsCE — <b>скин в стиле VestaHome</b>, v2</h1>
<p>Макеты 800 × 480 (экран ГУ) на настоящих картах из кэша: OSM — OSMapsCE, Яндекс с пробками — YaMapsCE.
Палитра VestaHome: графит #0B0C0E, панели #17191C, линии #34383F, текст #F3F1EC / #A3A8B0, один акцент —
оранжевый #FF6B1A. Значки — линией, как в VestaHome.</p>
<ul>
<li>Ориентиры автоинтерфейсов: CarPlay прячет панель кнопок после короткого бездействия и показывает по касанию;
Android Auto — цели касания не меньше 76 dp (≈ 62 px на этом экране), значки 36–44 dp; NHTSA — взгляд на экран
не дольше 2 с: крупные цели, минимум текста.</li>
<li>Строка статуса и панель на ГУ — непрозрачные (полупрозрачность в 16 бит на CE дорогая); на макете слегка
прозрачные только для наглядности.</li>
</ul></header>
%s
<section><h2>Кнопки: состояния</h2><img src="maps_mockup/buttons.png">
<p>Обычная — тёмная плитка с рамкой и светлым значком; режим включён — оранжевый значок; нажата — оранжевая
заливка, тёмный значок. «Без слежения» — приглушённый перечёркнутый значок.</p></section>
<section><h2>Решено и открыто</h2><ul>
<li><b>YaMapsCE</b> — только новый дизайн, раскладка и верхняя строка статуса прежние.</li>
<li><b>OSMapsCE</b> — карта на весь экран, панель по касанию (прячется через 6 с), постоянно видны «+» / «−» 72 px
и режим GPS. Значка масштаба нет.</li>
<li><b>Значки «+» / «−»</b> — цельные скруглённые полоски (раньше круглые концы были шире линии и торчали).</li>
</ul></section>
</body></html>''' % blocks
    hp = os.path.join(HERE, 'maps-skin-v2.html')
    open(hp, 'w', encoding='utf-8').write(html)
    edge = r'C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe'
    pdf = os.path.join(HERE, 'maps-skin-v2.pdf')
    subprocess.run([edge, '--headless', '--disable-gpu', '--no-pdf-header-footer', '--print-to-pdf=' + pdf,
                    'file:///' + hp.replace('\\', '/')], check=False, timeout=120)
    print(hp, pdf, os.path.exists(pdf))


if __name__ == '__main__':
    main()
