#!/usr/bin/env python3
"""make_home_mockup.py - VestaHome 0.6 mockup ("more air", after Škoda's Flow HMI) -> vesta-home-v0.6.html + .pdf.

Screens 800 x 480 drawn in HTML/CSS in VestaHome's own face Golos Text (its folder Fonts; the unit has only Tahoma), the orange accent and the rhombus
mosaic of VestaHome kept; beside each one the current screen (docs/shots, made by tools/make_shots.py) for
comparison. The PDF is printed by headless Edge (as make_maps_mockup.py).

Usage: python VestaHome/skin/make_home_mockup.py
"""
import os
import subprocess

HERE = os.path.dirname(os.path.abspath(__file__))
SHOTS = os.path.join(HERE, '..', 'docs', 'shots')

# ---- icons: Lucide (lucide.dev, ISC), the set VestaHome's skin is made of (make_icons.py; its SVGs cached in lucide/);
# "!name" filled, as VestaHome's transport buttons
from make_icons import inner, svg_of

P = {
    'home': 'house', 'radio': 'radio', 'music': 'music', 'phone': 'phone', 'apps': 'layout-grid', 'gear': 'settings',
    'moon': 'moon', 'bt': 'bluetooth', 'wifi': 'wifi', 'nav': 'navigation', 'vol': 'volume-2', 'play': '!play',
    'pause': '!pause', 'prev': '!skip-back', 'next': '!skip-forward', 'shuffle': 'shuffle', 'repeat': 'repeat',
    'list': 'list', 'cloud': 'cloud', 'car': 'car', 'clock': 'clock', 'user': 'user', 'star': 'star',
    'keypad': 'grid-3x3', 'warn': 'triangle-alert', 'search': 'search', 'chev': 'chevron-right', 'back': 'chevron-left',
    'folder': 'folder', 'film': 'film', 'sd': 'save', 'info': 'info', 'media': 'clapperboard', 'photo': 'image', 'sun': 'sun', 'call': 'phone', 'end': 'phone-off', 'trip': 'route', 'fuel': 'fuel', 'drop': 'droplet', 'bolt': 'zap',
}


def ico(name, size=24, color='currentColor', sw=2):
    spec = P[name]
    return ('<svg width="%d" height="%d" viewBox="0 0 24 24" fill="%s" stroke="%s" stroke-width="%s" '
            'stroke-linecap="round" stroke-linejoin="round">%s</svg>' % (
                size, size, color if spec.startswith('!') else 'none', color, sw, inner(svg_of(spec.lstrip('!')))))


# ---- the rhombus mosaic (the background) and the orange facets (accents), as SVG patterns
MOSAIC = ('<svg xmlns="http://www.w3.org/2000/svg" width="64" height="112" viewBox="0 0 64 112">'
          '<path d="M0 0 32 56 0 112z" fill="%23171a20"/><path d="M64 0 32 56 64 112z" fill="%23121419"/>'
          '<path d="M0 0h64L32 56z" fill="%231b1e24"/><path d="M0 112h64L32 56z" fill="%230f1114"/></svg>')
FACET = ('<svg xmlns="http://www.w3.org/2000/svg" width="40" height="70" viewBox="0 0 40 70">'
         '<path d="M0 0 20 35 0 70z" fill="%23e65a12"/><path d="M40 0 20 35 40 70z" fill="%23c24a0b"/>'
         '<path d="M0 0h40L20 35z" fill="%23ff6b1a"/><path d="M0 70h40L20 35z" fill="%23a83f09"/></svg>')

CSS = '''
@font-face { font-family: 'Golos Text'; font-weight: 400; src: url('../VestaHome/VestaHome/Fonts/GolosText-Regular.ttf'); }
@font-face { font-family: 'Golos Text'; font-weight: 700; src: url('../VestaHome/VestaHome/Fonts/GolosText-Bold.ttf'); }
@page { size: 297mm 210mm; margin: 0; }
:root { --bg:#101018; --s1:#181c20; --s2:#202428; --sep:#283038; --tx:#f8f4f0; --t2:#a8b0b8; --t3:#707480;
        --or:#f86c18; --orS:rgba(248,108,24,.16); --ok:#33c27a; }
* { box-sizing: border-box; }
html, body { -webkit-print-color-adjust: exact; print-color-adjust: exact; }
body { margin: 0; background: #08090b; color: var(--tx); font-family: 'Golos Text', Tahoma, sans-serif; }
.page { width: 297mm; height: 210mm; padding: 9mm 12mm; page-break-after: always; position: relative; overflow: hidden;
        display: flex; flex-direction: column; background: #08090b; }
.zoom { zoom: 1.08; align-self: center; }
.saver { background: radial-gradient(90% 70% at 50% 40%, rgba(255,107,26,.12), transparent 60%),
         linear-gradient(180deg, rgba(11,12,14,.6), rgba(11,12,14,.85)),
         url('data:image/svg+xml;utf8,MOSAIC') 0 0/64px 112px, #0b0c0e !important; }
h1 { font-size: 26px; margin: 0 0 4px; } h2 { font-size: 20px; margin: 0 0 6px; color: var(--or); }
.lead { color: var(--t2); font-size: 13px; margin: 0 0 10px; }
.row { display: flex; gap: 14px; align-items: flex-start; }
.note { font-size: 11.5px; color: var(--t2); line-height: 1.45; } .note b { color: var(--tx); }
.note ul { margin: 4px 0; padding-left: 16px; } .note li { margin: 2px 0; }
.was { width: 240px; flex: none; } .was img { width: 240px; border-radius: 6px; display: block; opacity: .95; }
.was span { font-size: 11px; color: var(--t3); }
/* ---- the screen */
.scr { width: 800px; height: 480px; position: relative; overflow: hidden; border-radius: 10px; flex: none;
       background: radial-gradient(120% 90% at 85% 0%, rgba(255,107,26,.07), transparent 55%),
                   linear-gradient(180deg, rgba(14,16,19,.25), rgba(10,11,13,.7)),
                   url('data:image/svg+xml;utf8,MOSAIC') 0 0/64px 112px, #101018;
       box-shadow: 0 0 0 1px #20242a; }
.top { position: absolute; left: 0; right: 0; top: 0; height: 48px; display: flex; align-items: center;
       padding: 0 24px; gap: 14px; }
.top .t { font-size: 20px; font-weight: bold; } .top .d { font-size: 18px; color: var(--t2); }
.top .sp { flex: 1; } .top .st { display: flex; gap: 14px; color: var(--t3); align-items: center; }
.top .clk { font-size: 22px; font-weight: bold; margin-left: 6px; } .top .on { color: var(--or); }
.bot { position: absolute; left: 0; right: 0; bottom: 0; height: 64px; display: flex; align-items: center;
       justify-content: space-between; padding: 0 24px; background: linear-gradient(0deg, rgba(10,11,13,.96), rgba(10,11,13,.6)); }
.bot .i { width: 64px; height: 44px; display: flex; align-items: center; justify-content: center; color: var(--t2);
          border-radius: 22px; gap: 8px; font-size: 15px; font-weight: bold; }
.bot .a { width: auto; padding: 0 18px; background: var(--orS); color: var(--or); }
.card { position: absolute; background: var(--s1); border-radius: 20px; padding: 18px 20px; }
.cap { font-size: 13px; color: var(--t2); display: flex; align-items: center; gap: 8px; }
.cap svg { color: var(--or); }
.big { font-size: 52px; font-weight: bold; line-height: 1; } .mid { font-size: 26px; font-weight: bold; }
.t2 { color: var(--t2); } .t3 { color: var(--t3); } .or { color: var(--or); }
.pill { display: inline-flex; align-items: center; gap: 8px; height: 40px; padding: 0 18px; border-radius: 20px;
        background: var(--s2); font-size: 15px; color: var(--t2); }
.pill.a { background: var(--orS); color: var(--or); font-weight: bold; }
.btn { display: flex; align-items: center; justify-content: center; border-radius: 50%; background: var(--s2); }
.play { background: var(--or); color: #101018; }
.facet { background: url('data:image/svg+xml;utf8,FACET') 0 0/40px 70px; }
.bar { height: 6px; border-radius: 3px; background: var(--s2); position: relative; }
.bar i { position: absolute; left: 0; top: 0; bottom: 0; border-radius: 3px; background: var(--or); }
.knob { position: absolute; width: 18px; height: 18px; border-radius: 50%; background: var(--tx); top: -6px; }
.lrow { display: flex; align-items: center; height: 60px; gap: 14px; border-bottom: 1px solid var(--sep); }
.lrow:last-child { border-bottom: 0; }
.tog { width: 52px; height: 30px; border-radius: 15px; background: var(--s2); position: relative; }
.tog::after { content: ''; position: absolute; left: 4px; top: 4px; width: 22px; height: 22px; border-radius: 50%;
              background: var(--t3); }
.tog.on { background: var(--or); } .tog.on::after { left: 26px; background: #101018; }
.tok { display: inline-block; width: 30px; height: 30px; border-radius: 8px; vertical-align: middle; margin-right: 6px;
       box-shadow: 0 0 0 1px #2c3038; }
table.tk { border-collapse: collapse; font-size: 12px; } table.tk td { padding: 3px 10px 3px 0; color: var(--t2); }
table.tk td b { color: var(--tx); }
'''.replace('MOSAIC', MOSAIC).replace('FACET', FACET)


def top(title=None, date=None, back=False, play=None):
    left = ''
    if back:
        left += '<span class="t2">%s</span>' % ico('back', 26)
    if title:
        left += '<span class="t">%s</span>' % title
    if date:
        left += '<span class="d">%s</span>' % date
    st = ''
    if play:
        st += '<span class="on">%s</span>' % ico(play, 20)
    st += ico('nav', 20) + '<span class="on">%s</span>' % ico('bt', 20) + ico('wifi', 20) + ico('vol', 20)
    return ('<div class="top">%s<span class="sp"></span><span class="st">%s<span style="color:var(--t2);font-size:16px">12'
            '</span></span><span class="clk">12:47</span></div>' % (left, st))


def bot(active):
    items = [('home', 'Главная'), ('radio', 'Радио'), ('music', 'Медиа'), ('phone', 'Телефон'), ('apps', 'Приложения'),
             ('gear', 'Настройки'), ('moon', '')]
    out = ''
    for name, label in items:
        if name == active:
            out += '<span class="i a">%s%s</span>' % (ico(name, 24), label)
        else:
            out += '<span class="i">%s</span>' % ico(name, 26)
    return '<div class="bot">%s</div>' % out


# ---- the screens
def scr_home():
    return '''<div class="scr">%s
<div class="card" style="left:24px;top:60px;width:470px;height:340px">
  <div class="cap">%s Сейчас играет · интернет-радио</div>
  <div style="display:flex;gap:20px;margin-top:22px">
    <div class="facet" style="width:132px;height:132px;border-radius:18px;display:flex;align-items:center;justify-content:center;color:#3a1604">%s</div>
    <div style="flex:1;padding-top:6px"><div class="mid">Relax FM</div><div class="t2" style="font-size:17px;margin-top:6px">ATB — Love the Silence</div>
      <div class="t3" style="font-size:14px;margin-top:10px">MP3 · 128 кбит/с · в эфире</div></div></div>
  <div style="display:flex;align-items:center;justify-content:center;gap:34px;margin-top:40px">
    <span class="t2">%s</span><span class="btn play" style="width:72px;height:72px">%s</span><span class="t2">%s</span></div>
</div>
<div class="card" style="left:510px;top:60px;width:266px;height:162px">
  <div class="cap">%s Погода · Тольятти</div>
  <div style="display:flex;align-items:center;gap:14px;margin-top:16px"><span class="t2">%s</span><span class="big">+6°</span></div>
  <div class="t2" style="font-size:14px;margin-top:12px">Облачно · днём +9°, ночью +1°</div>
</div>
<div class="card" style="left:510px;top:238px;width:266px;height:162px">
  <div class="cap">%s Поездка</div>
  <div style="display:flex;gap:22px;margin-top:16px"><div><div class="mid">42,8</div><div class="t3" style="font-size:13px">км</div></div>
  <div><div class="mid">7,4</div><div class="t3" style="font-size:13px">л/100 км</div></div></div>
  <div class="t2" style="font-size:14px;margin-top:12px">Запас хода 420 км</div>
</div>
<div style="position:absolute;left:0;right:0;bottom:72px;display:flex;justify-content:center;gap:8px">
  <i style="width:22px;height:5px;border-radius:3px;background:var(--or)"></i><i style="width:8px;height:5px;border-radius:3px;background:var(--t3)"></i><i style="width:8px;height:5px;border-radius:3px;background:var(--t3)"></i></div>
%s</div>''' % (top(date='Пятница, 23 октября', play='radio'), ico('music', 16), ico('radio', 64, '#3a1604', 2.2),
               ico('prev', 30), ico('pause', 30), ico('next', 30), ico('cloud', 16), ico('cloud', 44), ico('trip', 16),
               bot('home'))


def scr_music():
    tabs = ''.join('<span class="pill%s">%s</span>' % (' a' if t == 'Музыка' else '', t)
                   for t in ('Музыка', 'Интернет', 'Bluetooth', 'AUX', 'Видео', 'Фото'))
    return '''<div class="scr">%s
<div style="position:absolute;left:24px;top:60px;display:flex;gap:10px">%s</div>
<div class="facet" style="position:absolute;left:24px;top:124px;width:196px;height:196px;border-radius:22px;display:flex;align-items:center;justify-content:center">%s</div>
<div style="position:absolute;left:248px;top:128px;right:24px">
  <div class="t3" style="font-size:13px">Трек 4 из 18 · Rock Hits 2024</div>
  <div style="font-size:34px;font-weight:bold;margin-top:8px">Valhalla Calling</div>
  <div class="t2" style="font-size:19px;margin-top:6px">Miracle of Sound</div>
  <div class="bar" style="margin-top:30px"><i style="width:38%%"></i><span class="knob" style="left:calc(38%% - 9px)"></span></div>
  <div style="display:flex;justify-content:space-between;font-size:14px;margin-top:10px" class="t2"><span>1:24</span><span>3:41</span></div>
</div>
<div style="position:absolute;left:248px;right:24px;top:312px;display:flex;align-items:center;justify-content:space-between">
  <span class="t3">%s</span><span class="t2">%s</span><span class="btn play" style="width:76px;height:76px">%s</span><span class="t2">%s</span><span class="or">%s</span></div>
%s</div>''' % (top('Музыка', play='music'), tabs, ico('music', 84, '#3a1604', 2), ico('shuffle', 26), ico('prev', 34),
               ico('pause', 32), ico('next', 34), ico('repeat', 26), bot('music'))


def media_head(tab, crumbs, action=''):   # the sources' pills, then the path: back, the folders, an action at the right
    tabs = ''.join('<span class="pill%s">%s</span>' % (' a' if t == tab else '', t)
                   for t in ('Музыка', 'Интернет', 'Bluetooth', 'AUX', 'Видео', 'Фото'))
    path = '<span class="t3"> › </span>'.join('<span class="%s">%s</span>' % ('' if k == len(crumbs) - 1 else 't2', c)
                                              for k, c in enumerate(crumbs))
    return ('<div style="position:absolute;left:24px;top:60px;display:flex;gap:10px">%s</div>'
            '<div style="position:absolute;left:16px;right:24px;top:110px;height:40px;display:flex;align-items:center;gap:10px">'
            '<span class="t2" style="display:flex">%s</span><span style="font-size:17px;font-weight:bold;flex:1">%s</span>%s</div>'
            % (tabs, ico('back', 24), path, action))


def file_rows(items):   # (icon, name, second line, state) in two columns: state 'dir' a folder, 'now' the playing one
    out = ''
    for icn, name, sub, st in items:
        now = st == 'now'
        out += ('<div style="display:flex;align-items:center;gap:12px;height:52px;padding:0 14px;border-radius:14px;'
                'background:%s;%s"><span style="display:flex;color:%s">%s</span><div style="flex:1;min-width:0">'
                '<div style="font-size:16px;%swhite-space:nowrap;overflow:hidden;text-overflow:ellipsis">%s</div>%s</div>%s</div>' % (
                    'var(--orS)' if now else 'var(--s1)', 'color:var(--or)' if now else '',
                    'var(--or)' if st in ('dir', 'now') else 'var(--t2)', ico(icn, 22),
                    'font-weight:bold;' if st in ('dir', 'now') else '', name,
                    '<div class="%s" style="font-size:12px">%s</div>' % ('or' if now else 't3', sub) if sub else '',
                    '<span class="or" style="font-size:12px;font-weight:bold">▶ играет</span>' if now else ''))
    return ('<div style="position:absolute;left:24px;right:24px;top:160px;display:grid;grid-template-columns:1fr 1fr;'
            'gap:8px 12px">%s</div>' % out)


def page_dots(n, top_px=396):
    return ('<div style="position:absolute;left:0;right:0;top:%dpx;display:flex;justify-content:center;gap:8px">%s</div>' % (
        top_px, ''.join('<i style="width:%dpx;height:5px;border-radius:3px;background:%s"></i>' % (
            22 if k == 0 else 8, 'var(--or)' if k == 0 else 'var(--t3)') for k in range(n))))


def scr_music_list():
    items = [('folder', 'Rock Hits 2024', '18 треков', 'dir'), ('music', '01 · Thunderstruck', 'MP3 · 11,2 МБ', ''),
             ('music', '02 · Kickstart My Heart', 'MP3 · 10,9 МБ', ''), ('music', '03 · Holy Diver', 'FLAC · 38,4 МБ', ''),
             ('music', '04 · Valhalla Calling', 'MP3 · 8,5 МБ', 'now'),
             ('music', '05 · Paranoid', 'MP3 · 6,5 МБ', ''), ('music', '06 · Ace of Spades', 'WMA · 4,1 МБ', ''),
             ('music', '07 · The Trooper', 'MP3 · 9,7 МБ', '')]
    act = '<span class="pill a">%s Играть всё</span>' % ico('play', 16)
    return '<div class="scr">%s%s%s%s%s</div>' % (top('Медиа', play='music'),
                                                 media_head('Музыка', ['SD-карта', 'Музыка', 'Rock'], act),
                                                 file_rows(items), page_dots(3), bot('music'))


def scr_video_list():
    items = [('folder', 'Сериалы', '3 папки', 'dir'), ('folder', 'Мультфильмы', '12 файлов', 'dir'),
             ('film', 'Дорога на Байкал.mp4', 'MP4 · 1,4 ГБ', ''),
             ('film', 'Видеорегистратор 0612.avi', 'AVI · 210 МБ', ''),
             ('film', 'Концерт 2019.mkv', 'MKV · 2,1 ГБ', ''), ('film', 'Отпуск Крым.mp4', 'MP4 · 640 МБ', ''),
             ('film', 'Трейлер.wmv', 'WMV · 38 МБ', ''), ('film', 'Урок 1.mp4', 'MP4 · 520 МБ', '')]
    return '<div class="scr">%s%s%s%s%s</div>' % (top('Медиа'), media_head('Видео', ['USB-диск', 'Видео']),
                                                 file_rows(items), page_dots(2), bot('music'))


# photo stand-ins: sea and sunset gradients
SUN = ['#f6b25e 0%, #ee7a45 38%, #3a4a5a 52%, #1e2a36 100%',
       '#ffd27a 0%, #f39a52 30%, #c25a3a 48%, #2a3440 60%, #151c24 100%',
       '#9cc6e8 0%, #d9e6ef 40%, #c9b28a 58%, #8a7656 100%', '#f4c48a 0%, #e0905c 42%, #405468 55%, #2a3846 100%',
       '#a8d0ef 0%, #e8f0f4 45%, #3f7fa8 55%, #1f4a6a 100%', '#ffb36b 0%, #e2683f 35%, #5a3a46 52%, #241c26 100%',
       '#bcd8ea 0%, #e2ecf2 38%, #b9a27c 52%, #6c5a42 100%']


def scr_photo():
    tiles = ('<div><div style="height:86px;border-radius:14px;background:var(--s1);display:flex;align-items:center;'
             'justify-content:center;gap:8px;color:var(--or)">%s<span class="t3" style="font-size:13px">42</span></div>'
             '<div style="font-size:13px;font-weight:bold;margin-top:5px">Бахчисарай 2010</div></div>' % ico('folder', 36))
    for k, g in enumerate(SUN):
        sel = k == 2
        tiles += ('<div><div style="height:86px;border-radius:14px;background:linear-gradient(180deg, %s);%s"></div>'
                  '<div class="%s" style="font-size:13px;margin-top:5px">IMAGE_%03d.jpg</div></div>' % (
                      g, 'box-shadow:0 0 0 3px var(--or)' if sel else '', 'or' if sel else 't2', k + 2))
    act = '<span class="pill">%s Слайд-шоу</span>' % ico('play', 16)
    return ('<div class="scr">%s%s<div style="position:absolute;left:24px;right:24px;top:162px;display:grid;'
            'grid-template-columns:repeat(4,1fr);gap:12px 12px">%s</div>%s%s</div>' % (
                top('Медиа'), media_head('Фото', ['SD-карта', 'Фото', 'Крым 2010'], act), tiles, page_dots(4),
                bot('music')))


def scr_radio():
    pre = ''
    for i, (f, n) in enumerate([('87,5', 'Европа+'), ('91,1', 'Радио 7'), ('96,3', 'Русское'), ('101,4', 'Ретро FM'),
                                ('103,7', 'Relax'), ('105,2', 'Шансон'), ('106,8', 'Маяк'), ('107,9', 'DFM')]):
        a = i == 4
        pre += ('<div style="background:%s;border-radius:16px;padding:12px 16px;%s"><div style="font-size:21px;font-weight:bold">%s'
                '</div><div style="font-size:13px;margin-top:2px" class="%s">%s</div></div>' % (
                    'var(--orS)' if a else 'var(--s1)', 'color:var(--or)' if a else '', f, 'or' if a else 't2', n))
    dots = ''.join('<i style="width:%dpx;height:5px;border-radius:3px;background:%s"></i>' % (
        22 if k == 0 else 8, 'var(--or)' if k == 0 else 'var(--t3)') for k in range(5))
    ticks = ''.join('<i style="position:absolute;bottom:6px;left:%.1f%%;width:2px;height:%dpx;background:%s"></i>' % (
        k * 100.0 / 48, 8 if k % 5 else 16, 'var(--or)' if k == 30 else 'var(--t3)') for k in range(49))

    def seek(icon, left):   # the scale's end: its ticks go on into a rounded key with the arrow
        t = ''.join('<i style="position:absolute;bottom:6px;%s:%dpx;width:2px;height:%dpx;background:var(--t2)"></i>' % (
            'right' if left else 'left', 4 + k * 9, 8 if k % 2 else 12) for k in range(3))
        return ('<div style="position:relative;width:96px;height:44px;flex:none;display:flex;align-items:center;'
                'justify-content:%s;padding:0 6px;color:var(--tx)">%s%s</div>' % (
                    'flex-start' if left else 'flex-end', icon, t))
    return '''<div class="scr">%s
<div style="position:absolute;left:24px;top:58px;right:24px;display:flex;align-items:flex-end;gap:14px">
  <div style="font-size:76px;font-weight:bold;line-height:.9">103,7</div>
  <div style="padding-bottom:6px"><span class="t2" style="font-size:20px">МГц · </span><span class="or" style="font-size:20px;font-weight:bold">FM1</span></div>
  <span style="flex:1"></span><span class="pill">PTY</span><span class="pill">%s Станции</span></div>
<div style="position:absolute;left:24px;top:136px;right:24px">
  <div class="mid">Relax FM</div><div class="t2" style="font-size:15px;margin-top:2px">RDS: Spokojnaya muzyka dlya otdykha</div></div>
<div style="position:absolute;left:24px;right:24px;top:196px;display:flex;gap:6px;align-items:center">
  %s<div style="position:relative;flex:1;height:44px">%s</div>%s</div>
<div style="position:absolute;left:24px;right:24px;top:252px;display:grid;grid-template-columns:repeat(4,1fr);gap:10px 12px">%s</div>
<div style="position:absolute;left:0;right:0;top:408px;display:flex;justify-content:center;gap:8px">%s</div>
%s</div>''' % (top('Радио', play='radio'), ico('list', 18), seek(ico('prev', 32), True), ticks,
               seek(ico('next', 32), False), pre, dots, bot('radio'))


def scr_phone():
    keys = ''.join('<span class="btn" style="width:56px;height:52px;border-radius:26px;font-size:22px;font-weight:bold">%s</span>' % k
                   for k in '123456789*0#')
    calls = ''
    for n, t, s in [('Мама', 'Сегодня, 11:32', 'call'), ('Сервис Lada', 'Вчера, 17:05', 'call'),
                    ('+7 927 123-45-67', 'Вчера, 09:12', 'warn'), ('Андрей', '21 окт', 'call')]:
        calls += ('<div class="lrow"><span class="btn" style="width:40px;height:40px;color:var(--t2)">%s</span><div style="flex:1">'
                  '<div style="font-size:17px">%s</div><div class="t3" style="font-size:13px">%s</div></div>'
                  '<span class="t2">%s</span></div>' % (ico('user', 20), n, t, ico('call', 20)))
    return '''<div class="scr">%s
<div style="position:absolute;left:24px;top:60px;display:flex;gap:10px"><span class="pill a">Набор</span><span class="pill">Контакты</span><span class="pill">Избранное</span><span class="pill">Журнал</span></div>
<div style="position:absolute;left:24px;top:112px;width:300px">
  <div style="font-size:28px;font-weight:bold;height:36px;letter-spacing:1px">8 927 12</div>
  <div style="display:grid;grid-template-columns:repeat(3,56px);gap:8px 22px;margin-top:4px">%s</div></div>
<div style="position:absolute;left:300px;top:200px;"><span class="btn" style="width:72px;height:72px;background:var(--ok);color:#101018">%s</span></div>
<div class="card" style="left:420px;top:116px;width:356px;height:284px;padding:6px 18px">%s</div>
%s</div>''' % (top('Телефон · Redmi Note 13'), keys, ico('call', 28), calls, bot('phone'))


def scr_settings():
    secs = [('gear', 'Общие'), ('vol', 'Звук'), ('sun', 'Экран'), ('radio', 'Радио'), ('bt', 'Bluetooth'),
            ('wifi', 'Wi-Fi'), ('media', 'Видео'), ('photo', 'Фото'), ('car', 'Автомобиль'), ('info', 'О системе')]
    cur = 'Bluetooth'
    nav = ''.join('<div style="display:flex;flex-direction:column;align-items:center;justify-content:center;gap:4px;'
                  'height:62px;border-radius:16px;%s">%s<span style="font-size:13px;%s">%s</span></div>' % (
                      'background:var(--orS);color:var(--or)' if l == cur else 'background:var(--s1);color:var(--t2)', ico(n, 24),
                      'font-weight:bold' if l == cur else '', l) for n, l in secs)   # two columns, icons with names
    rows = ''
    for lab, sub, ctl in [('Redmi Note 13', 'подключён · музыка и звонки', '<span class="pill">Отключить</span>'),
                          ('Сопряжённые устройства', '3 устройства', ico('chev', 22, 'var(--t3)')),
                          ('Новый телефон', 'поиск или видимость ГУ', ico('chev', 22, 'var(--t3)')),
                          ('Автосоединение', 'при включении ГУ', '<span class="tog on"></span>'),
                          ('Автоответ', 'через 5 секунд', '<span class="tog"></span>')]:
        rows += ('<div class="lrow"><div style="flex:1"><div style="font-size:18px">%s</div><div class="t3" style="font-size:13px">%s'
                 '</div></div>%s</div>' % (lab, sub, ctl))
    return '''<div class="scr">%s
<div style="position:absolute;left:16px;top:60px;width:196px;display:grid;grid-template-columns:1fr 1fr;gap:7px 8px">%s</div>
<div class="card" style="left:228px;top:60px;width:548px;height:340px;padding:4px 24px">%s</div>
%s</div>''' % (top('Настройки'), nav, rows, bot('gear'))


def scr_car():
    tiles = ''
    for icn, val, unit, lab, warn in [('bolt', '2 140', 'об/мин', 'Обороты', 0), ('drop', '92°', '', 'Охлаждающая жидкость', 0),
                                      ('fuel', '38%', '', 'Топливо · запас 420 км', 0), ('trip', '7,4', 'л/100 км', 'Расход поездки', 0),
                                      ('bolt', '14,1', 'В', 'Напряжение', 0), ('warn', '2', 'кода', 'Ошибки двигателя', 1)]:
        tiles += ('<div style="background:%s;border-radius:18px;padding:16px 18px"><div class="cap" style="%s">%s %s</div>'
                  '<div style="margin-top:14px"><span style="font-size:38px;font-weight:bold;%s">%s</span> <span class="t2" style="font-size:15px">%s</span></div></div>'
                  % ('rgba(255,107,26,.12)' if warn else 'var(--s1)', 'color:var(--or)' if warn else '', ico(icn, 16), lab,
                     'color:var(--or)' if warn else '', val, unit))
    return '''<div class="scr">%s
<div style="position:absolute;left:24px;right:24px;top:62px;display:grid;grid-template-columns:repeat(3,1fr);gap:14px">%s</div>
<div style="position:absolute;left:24px;right:24px;top:338px;display:flex;gap:10px;align-items:center">
  <span class="pill a">%s Прочитать ошибки</span><span class="pill">Все показания</span><span class="sp" style="flex:1"></span><span class="t3" style="font-size:13px">OBD · ELM327 · подключён</span></div>
%s</div>''' % (top('Автомобиль', back=True).replace('<span class="st">', '<span class="st"><span class="on">%s</span>' % ico('warn', 20)),
               tiles, ico('warn', 18), bot('none'))


def scr_pair():
    return '''<div class="scr">%s
<div style="position:absolute;inset:0;background:rgba(6,7,9,.66)"></div>
<div style="position:absolute;left:150px;top:78px;width:500px;height:324px;background:var(--s1);border-radius:26px;padding:26px 30px;text-align:center">
  <div class="cap" style="justify-content:center">%s Сопряжение Bluetooth</div>
  <div style="font-size:26px;font-weight:bold;margin-top:18px">Redmi Note 13</div>
  <div class="t2" style="font-size:16px;margin-top:8px">просит сопряжение. Код на телефоне тот же?</div>
  <div style="font-size:54px;font-weight:bold;letter-spacing:6px;margin-top:16px" class="or">482 915</div>
  <div style="display:flex;gap:16px;margin-top:26px"><span class="pill" style="flex:1;justify-content:center;height:54px;font-size:17px">Отклонить</span>
  <span class="pill" style="flex:1;justify-content:center;height:54px;font-size:17px;background:var(--or);color:#101018;font-weight:bold">Принять</span></div>
</div>
%s</div>''' % (top('Настройки'), ico('bt', 16), bot('gear'))


def scr_saver():
    return '''<div class="scr saver">
<div style="position:absolute;left:0;right:0;top:96px;text-align:center"><div style="font-size:150px;font-weight:bold;line-height:1">12:47</div>
<div class="t2" style="font-size:22px;margin-top:10px">Пятница, 23 октября</div>
<div style="display:flex;justify-content:center;align-items:center;gap:12px;margin-top:22px"><span class="t2">%s</span><span style="font-size:30px;font-weight:bold">+6°</span><span class="t3" style="font-size:16px">облачно</span></div></div>
<div style="position:absolute;left:0;right:0;bottom:34px;display:flex;justify-content:center;gap:10px;align-items:center;font-size:17px"><span class="or">%s</span>Relax FM · ATB — Love the Silence</div>
</div>''' % (ico('cloud', 34), ico('radio', 20))


SCREENS = [
    ('Главная', scr_home, '01_home.png', [
        'Три крупных виджета вместо шести: на странице меньше, зато каждый читается с одного взгляда; остальные — на соседних страницах.',
        'Под виджетами — точки страниц (текущая — оранжевая чёрточка): видно, что страницы листаются.',
        'Карточки без рамок — поверхность чуть светлее фона, скругление 20 px, отступы 24 / 16 px.',
        'Верхняя строка тоньше (48 px): дата, справа значки состояния тусклые, активное — оранжевое, часы.',
        'Нижняя панель (64 px): значки без подписей, текущий раздел — оранжевая «таблетка» с подписью.']),
    ('Музыка', scr_music, '07_music.png', [
        'Источники — «таблетки» в одну строку вместо прямоугольных вкладок; текущий — мягкая оранжевая заливка.',
        'Обложка 196 px с оранжевой гранью-ромбами; название 34 px, исполнитель 19 px — иерархия размером, не рамками.',
        'Полоса прогресса тонкая (6 px), круглый бегунок 18 px; кнопки без подложек, кроме главной «▶».']),
    ('Радио', scr_radio, '04_radio.png', [
        'Частота — самое крупное на экране (76 px); после «МГц» — текущая страница пресетов (FM1, FM2, FM3, УКВ, СВ) '
        'оранжевым: отдельная кнопка диапазона не нужна, диапазон — это страница.',
        'Поиск — продолжение шкалы: её края переходят в стрелки без подложки; тап по стрелке или по '
        'краю шкалы — поиск станции назад / вперёд. Отдельных кнопок поиска и «Скан» нет.',
        'Пресеты — 8 карточек (две строки по 4), под ними точки страниц: видно, что листаются пальцем. '
        'Текущая станция — оранжевая.',
        'PTY и список станций — «таблетки» справа от частоты.']),
    ('Музыка: папка', scr_music_list, '08_music_list.png', [
        'Списки — внутри раздела «Медиа»: «таблетки» источников остаются сверху, отдельного экрана с «назад» нет.',
        'Путь — строка «‹ SD-карта › Музыка › Rock»: стрелка — на уровень выше (вместо плитки «Наверх»), '
        'текущая папка — белым; справа действие папки — «Играть всё».',
        'Строки 52 px в два столбца без рамок: значок, название, вторая строка тусклее — формат и размер (теги не читаются); '
        'папки — оранжевый значок и жирное название; играющий трек — оранжевая подложка и «▶ играет».',
        'Под списком — точки страниц.']),
    ('Видео: папка', scr_video_list, '13_video.png', [
        'Те же строки, что у музыки: папки сверху, у файла второй строкой — формат и размер.',
        'Носители — первый уровень пути: вместо букв «C: D: E: H:» — названия «SD-карта», «USB-диск», '
        '«Встроенная карта».']),
    ('Фото', scr_photo, '14_photo.png', [
        'Сетка 4×2: превью без рамок со скруглением 14 px, подпись под картинкой мелко и тусклее.',
        'Папка — такая же плитка: оранжевый значок и число фото в ней; «Наверх» — стрелка в строке пути.',
        'Последнее открытое фото — оранжевая обводка (видно, откуда продолжать); справа в строке пути — «Слайд-шоу».']),
    ('Телефон', scr_phone, '15_phone.png', [
        'Разделы телефона — «таблетки» сверху (боковых вкладок нет).',
        'Скруглённые клавиши 56×52 с воздухом между ними; зелёная кнопка вызова — единственное исключение из оранжевого.',
        'Справа — недавние звонки списком без рамок между карточками, только тонкие разделители.']),
    ('Настройки', scr_settings, '26_settings_bt.png', [
        'Разделы — плитки «значок + подпись под ним» в два столбца; ничего не сдвигается. Текущая — '
        'мягкая оранжевая подложка, значок и подпись оранжевые.',
        'Сейчас 10 разделов — 5 строк по 62 px; при 11–12 разделах — 6 строк по ~52 px, колонка та же.',
        'Строки 60 px, подписи второй строкой тусклее, переключатели крупнее (52×30).',
        'Одна карточка справа вместо панели с рамкой.']),
    ('Автомобиль', scr_car, '38_car_sensors.png', [
        'Показания — 6 крупных плиток (38 px) вместо таблицы из 27 строк; полный список — «Все показания».',
        'Ошибка — плитка с оранжевым оттенком и знак «!» в верхней строке.']),
    ('Окно (сопряжение)', scr_pair, None, [
        'Окна поверх экрана — затемнение и карточка со скруглением 26 px.',
        'Главное действие — залитая оранжевая «таблетка», второе — серая.']),
    ('Заставка', scr_saver, None, [
        'Часы 150 px по центру, мягкое оранжевое свечение поверх ромбов; что играет — внизу одной строкой.']),
]


def main():
    pages = ['''<div class="page"><h1>VestaHome 0.6 — больше воздуха</h1>
<p class="lead">Макет 2026-10-23 по мотивам Škoda Flow HMI (Superb / Kodiaq / Enyaq 2024: «modern and minimalist design with
flatter 2D graphics», постоянная нижняя панель, верхняя — состояние и быстрые функции). Наше остаётся: тёмная тема, оранжевый
акцент, ромбы, 800×480; шрифт — свой, Golos Text (в папке VestaHome, на ГУ только Tahoma).</p>
<div class="row"><div class="note" style="flex:1"><h2>Что меняется</h2><ul>
<li><b>Меньше элементов на экране, больше пустого места.</b> Отступ от края 24 px, между карточками 16 px.</li>
<li><b>Без рамок.</b> Карточки отличаются от фона тоном (поверхность #181c20 на #101018), разделители в списках — 1 px.</li>
<li><b>Скругления крупнее:</b> карточки 20, окна 26, кнопки-«таблетки» — полукруглые.</li>
<li><b>Иерархия размером:</b> главное число 52–76 px, заголовок 26–34, текст 16–19, подписи 13–15. Регистр обычный, без
СПЛОШНЫХ ЗАГЛАВНЫХ.</li>
<li><b>Оранжевый — только для активного и главного действия.</b> Выбранное — мягкая оранжевая заливка (16%), главное
действие — сплошная.</li>
<li><b>Ромбы:</b> фон — крупная мозаика (64×112) почти в тон фону; оранжевые грани — на обложках и акцентных плитках.
Лёгкое оранжевое свечение в углу экрана.</li>
<li><b>Значки — Lucide</b> (тот же набор, что в скине VestaHome): линейные, 24-сетка, линия 2 px, кнопки плеера залиты; в нижней панели без подписей — подпись только у текущего раздела.</li>
<li><b>Контраст:</b> текст #f8f4f0 на #181c20 — 15:1; вторичный #a8b0b8 — 8:1; третичный #707480 — для неважного.</li>
</ul></div>
<div class="note" style="width:330px"><h2>Токены</h2><table class="tk">
<tr><td><span class="tok" style="background:#101018"></span><b>фон</b> #101018</td></tr>
<tr><td><span class="tok" style="background:#181c20"></span><b>карточка</b> #181c20</td></tr>
<tr><td><span class="tok" style="background:#202428"></span><b>кнопка / вложенное</b> #202428</td></tr>
<tr><td><span class="tok" style="background:#283038"></span><b>разделитель</b> #283038</td></tr>
<tr><td><span class="tok" style="background:#f86c18"></span><b>акцент</b> #f86c18, заливка выбранного 16%</td></tr>
<tr><td><span class="tok" style="background:#f8f4f0"></span><b>текст</b> #f8f4f0 · #a8b0b8 · #707480</td></tr>
<tr><td><span class="tok" style="background:#33c27a"></span><b>вызов / норма</b> #33c27a</td></tr>
<tr><td><b>16 бит.</b> Экран ГУ — RGB565: красный и синий шагами 8, зелёный 4. Все цвета выше — точные 565 (иначе тёмные серые уходят в зелень); фон и засветка — с упорядоченным шумом (дизеринг).</td></tr>
</table><h2 style="margin-top:12px">Размеры</h2><table class="tk">
<tr><td><b>Верхняя строка</b> 48 · нижняя панель 64</td></tr><tr><td><b>Поля</b> 24 · промежутки 16 · строка списка 60</td></tr>
<tr><td><b>Радиусы</b> 20 карточка · 26 окно · ½ высоты «таблетка»</td></tr>
<tr><td><b>Касание</b> ≥ 44 px, основные кнопки 56–76</td></tr></table></div></div>
<div class="note" style="margin-top:10px"><h2>Реализация на ГУ</h2>Всё рисуется теми же средствами VH (GDI, 16 бит): прозрачность
и свечение — заранее посчитанные тона, без настоящего смешивания на лету; мозаика — периодическая плитка, как сейчас.
Плавность не страдает: элементов на кадр меньше, чем сейчас.</div></div>''']
    # the font: VestaHome may carry its own (its folder Fonts\, AddFontResource at start: the unit has only Tahoma);
    # samples drawn by Windows GDI as VestaHome draws text (fonts_sample/, made by a GDI script from static TTFs)
    fonts = [('Tahoma', 'сейчас, есть на ГУ', 'привычный, узкий; жирный — грубоватый'),
             ('Inter', 'OFL · 2 × 340 КБ', 'лучшая читаемость мелкого текста, различимы l / I / 1; тяжёлые файлы'),
             ('GolosText', 'OFL · 2 × 70 КБ', 'для русских интерфейсов: плотный, крупные цифры; самые лёгкие файлы'),
             ('Manrope', 'OFL · 2 × 97 КБ', 'геометричный, «автомобильный»; на 13 px тесно, I / l похожи'),
             ('Onest', 'OFL · 2 × 98 КБ', 'геометричный, широкий; мелкий текст хуже Inter')]
    cells = ''
    for f, lic, why in fonts:
        img = os.path.join(HERE, 'fonts_sample', 'font-%s.png' % f)
        if os.path.exists(img):
            cells += ('<div><div class="note"><b>%s</b> · %s — %s</div><img src="fonts_sample/font-%s.png" '
                      'style="width:470px;border-radius:8px;display:block;margin-top:4px"></div>' % (f, lic, why, f))
    pages.append('<div class="page"><h2>Шрифт</h2><p class="lead">На ГУ только Tahoma, но VestaHome может нести свой: '
                  'обычный TrueType (.ttf, статические начертания — обычный и жирный) в своей папке Fonts, загрузка при '
                  'старте (AddFontResource), реестр не нужен. Образцы ниже нарисованы GDI Windows так же, как рисует VH '
                  '(сглаживание, размеры 52 / 30 / 19 / 18 / 15 / 13). Лицензия всех кандидатов — SIL OFL (можно '
                  'класть в программу).</p><div style="display:grid;grid-template-columns:1fr 1fr;gap:10px 18px">%s'
                  '</div></div>' % cells)
    for title, fn, shot, notes in SCREENS:
        was = ''
        if shot and os.path.exists(os.path.join(SHOTS, shot)):
            was = ('<div class="was"><span>Сейчас (0.5)</span><img src="../docs/shots/%s"></div>' % shot)
        pages.append('<div class="page"><h2>%s</h2><div class="zoom">%s</div><div class="row" style="margin-top:12px">%s'
                     '<div class="note" style="flex:1"><ul>%s</ul></div></div></div>'
                     % (title, fn(), was, ''.join('<li>%s</li>' % n for n in notes)))
    html = ('<!doctype html><html lang="ru"><head><meta charset="utf-8"><title>VestaHome 0.6 mockup</title><style>%s</style>'
            '</head><body>%s</body></html>' % (CSS, '\n'.join(pages)))
    hp = os.path.join(HERE, 'vesta-home-v0.6.html')
    open(hp, 'w', encoding='utf-8').write(html)
    edge = r'C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe'
    pdf = os.path.join(HERE, 'vesta-home-v0.6.pdf')
    subprocess.run([edge, '--headless', '--disable-gpu', '--no-pdf-header-footer', '--print-to-pdf=' + pdf,
                    'file:///' + hp.replace('\\', '/')], check=False, timeout=120)
    print(hp, pdf, os.path.exists(pdf))


if __name__ == '__main__':
    main()
