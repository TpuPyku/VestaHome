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

# ---- icons: 24-grid line icons, stroke 2 (the unit's built-in set redrawn lighter)
P = {
    'home': 'M4 11 12 4l8 7v9h-5v-6H9v6H4z',
    'radio': 'M4 9h16v11H4zM7 9l9-5M15 14.5a2 2 0 1 0 .01 0M7 13h4M7 16h4',
    'music': 'M9 18V6l11-2v12M9 18a3 3 0 1 1-.01 0M20 16a3 3 0 1 1-.01 0',
    'phone': 'M6 3h3l2 5-2.5 1.5a11 11 0 0 0 6 6L16 13l5 2v3a2 2 0 0 1-2 2A16 16 0 0 1 4 5a2 2 0 0 1 2-2z',
    'apps': 'M4 4h6v6H4zM14 4h6v6h-6zM4 14h6v6H4zM14 14h6v6h-6z',
    'gear': ('M12 15a3 3 0 1 0 0-6 3 3 0 0 0 0 6zM19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 1 1-2.83 2.83l-.06-.06a1.65 '
             '1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 1 1-4 0v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33'
             'l-.06.06a2 2 0 1 1-2.83-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 1 1 0-4h.09A1.65 '
             '1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 1 1 2.83-2.83l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 '
             '1.65 0 0 0 1-1.51V3a2 2 0 1 1 4 0v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 1 1 2.83 '
             '2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 1 1 0 4h-.09a1.65 1.65 0 0 0-1.51 1z'),
    'moon': 'M20 14.5A8 8 0 0 1 9.5 4a8 8 0 1 0 10.5 10.5z',
    'bt': 'M7 7l10 10-5 4V3l5 4L7 17',
    'wifi': 'M2 9a15 15 0 0 1 20 0M5 12.5a10 10 0 0 1 14 0M8.5 16a5 5 0 0 1 7 0M12 19.5h.01',
    'nav': 'M12 3l7 18-7-4-7 4z',
    'vol': 'M4 9h4l5-4v14l-5-4H4zM16 9a4 4 0 0 1 0 6M18.5 6.5a8 8 0 0 1 0 11',
    'play': 'M8 5v14l11-7z',
    'pause': 'M7 5h3v14H7zM14 5h3v14h-3z',
    'prev': 'M18 6v12L9 12zM6 6v12',
    'next': 'M6 6v12l9-6zM18 6v12',
    'shuffle': 'M3 7h4l10 10h4M3 17h4l3-3M14 10l3-3h4M18 4l3 3-3 3M18 14l3 3-3 3',
    'repeat': 'M4 11V8a2 2 0 0 1 2-2h13l-3-3M20 13v3a2 2 0 0 1-2 2H5l3 3',
    'list': 'M8 6h12M8 12h12M8 18h12M4 6h.01M4 12h.01M4 18h.01',
    'cloud': 'M7 18a4 4 0 0 1-.5-8A6 6 0 0 1 18 9a4.5 4.5 0 0 1 0 9z',
    'car': 'M3 15v-3l2-5h14l2 5v3zM5 15v3M19 15v3M7 12h.01M17 12h.01',
    'clock': 'M12 3a9 9 0 1 0 .01 0M12 7v5l3 2',
    'user': 'M12 4a4 4 0 1 0 .01 0M4 21a8 8 0 0 1 16 0',
    'star': 'M12 3l2.7 5.7 6.3.8-4.6 4.3 1.2 6.2L12 17l-5.6 3 1.2-6.2L3 9.5l6.3-.8z',
    'keypad': 'M6 5h.01M12 5h.01M18 5h.01M6 11h.01M12 11h.01M18 11h.01M6 17h.01M12 17h.01M18 17h.01',
    'warn': 'M12 3l10 17H2zM12 10v4M12 17h.01',
    'search': 'M10.5 4a6.5 6.5 0 1 0 .01 0M15.5 15.5 20 20',
    'scan': 'M12 12h.01M8.5 15.5a5 5 0 0 1 0-7M15.5 8.5a5 5 0 0 1 0 7M5.6 18.4a9 9 0 0 1 0-12.8M18.4 5.6a9 9 0 0 1 0 12.8',
    'chev': 'M9 5l7 7-7 7',
    'back': 'M15 5l-7 7 7 7',
    'sd': 'M7 3h8l4 4v14H7zM10 7v2M13 7v2',
    'call': 'M6 3h3l2 5-2.5 1.5a11 11 0 0 0 6 6L16 13l5 2v3a2 2 0 0 1-2 2A16 16 0 0 1 4 5a2 2 0 0 1 2-2z',
    'end': 'M3 14c5-5 13-5 18 0l-3 3-3-2v-2a10 10 0 0 0-6 0v2l-3 2z',
    'trip': 'M5 19c4-14 10 2 14-14M5 19h.01M19 5h.01',
    'fuel': 'M5 21V5a2 2 0 0 1 2-2h6a2 2 0 0 1 2 2v16M4 21h12M15 9h2a2 2 0 0 1 2 2v6a1 1 0 0 0 2 0V9l-3-3M8 7h4',
    'drop': 'M12 3c4 5 6 8 6 11a6 6 0 0 1-12 0c0-3 2-6 6-11z',
    'bolt': 'M13 2 4 14h7l-1 8 9-12h-7z',
}


def ico(name, size=24, color='currentColor', sw=2):
    return ('<svg width="%d" height="%d" viewBox="0 0 24 24" fill="none" stroke="%s" stroke-width="%s" '
            'stroke-linecap="round" stroke-linejoin="round"><path d="%s"/></svg>' % (size, size, color, sw, P[name]))


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
:root { --bg:#0e1013; --s1:#1a1d22; --s2:#23272e; --sep:#2a2f37; --tx:#f5f3ee; --t2:#aab0b8; --t3:#6c727c;
        --or:#ff6b1a; --orS:rgba(255,107,26,.16); --ok:#33c27a; }
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
                   url('data:image/svg+xml;utf8,MOSAIC') 0 0/64px 112px, #0e1013;
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
.play { background: var(--or); color: #0e1013; }
.facet { background: url('data:image/svg+xml;utf8,FACET') 0 0/40px 70px; }
.bar { height: 6px; border-radius: 3px; background: var(--s2); position: relative; }
.bar i { position: absolute; left: 0; top: 0; bottom: 0; border-radius: 3px; background: var(--or); }
.knob { position: absolute; width: 18px; height: 18px; border-radius: 50%; background: var(--tx); top: -6px; }
.lrow { display: flex; align-items: center; height: 60px; gap: 14px; border-bottom: 1px solid var(--sep); }
.lrow:last-child { border-bottom: 0; }
.tog { width: 52px; height: 30px; border-radius: 15px; background: var(--s2); position: relative; }
.tog::after { content: ''; position: absolute; left: 4px; top: 4px; width: 22px; height: 22px; border-radius: 50%;
              background: var(--t3); }
.tog.on { background: var(--or); } .tog.on::after { left: 26px; background: #0e1013; }
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
<div style="position:absolute;left:300px;top:200px;"><span class="btn" style="width:72px;height:72px;background:var(--ok);color:#0e1013">%s</span></div>
<div class="card" style="left:420px;top:116px;width:356px;height:284px;padding:6px 18px">%s</div>
%s</div>''' % (top('Телефон · Redmi Note 13'), keys, ico('call', 28), calls, bot('phone'))


def scr_settings():
    nav = ''
    for name, label in [('gear', 'Общие'), ('vol', 'Звук'), ('moon', 'Экран'), ('radio', 'Радио'), ('bt', 'Bluetooth'),
                        ('wifi', 'Wi-Fi'), ('car', 'Автомобиль')]:
        a = label == 'Bluetooth'
        nav += ('<div style="display:flex;align-items:center;gap:12px;height:42px;padding:0 14px;border-radius:14px;%s">%s'
                '<span style="font-size:16px;%s">%s</span></div>' % ('background:var(--orS);color:var(--or)' if a else 'color:var(--t2)',
                                                                      ico(name, 20), 'font-weight:bold' if a else '', label))
    rows = ''
    for lab, sub, ctl in [('Redmi Note 13', 'подключён · музыка и звонки', '<span class="pill">Отключить</span>'),
                          ('Сопряжённые устройства', '3 устройства', ico('chev', 22, 'var(--t3)')),
                          ('Новый телефон', 'поиск или видимость ГУ', ico('chev', 22, 'var(--t3)')),
                          ('Автосоединение', 'при включении ГУ', '<span class="tog on"></span>'),
                          ('Автоответ', 'через 5 секунд', '<span class="tog"></span>')]:
        rows += ('<div class="lrow"><div style="flex:1"><div style="font-size:18px">%s</div><div class="t3" style="font-size:13px">%s'
                 '</div></div>%s</div>' % (lab, sub, ctl))
    return '''<div class="scr">%s
<div style="position:absolute;left:16px;top:60px;width:196px">%s</div>
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
  <span class="pill" style="flex:1;justify-content:center;height:54px;font-size:17px;background:var(--or);color:#0e1013;font-weight:bold">Принять</span></div>
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
    ('Телефон', scr_phone, '15_phone.png', [
        'Разделы телефона — «таблетки» сверху (боковых вкладок нет).',
        'Скруглённые клавиши 56×52 с воздухом между ними; зелёная кнопка вызова — единственное исключение из оранжевого.',
        'Справа — недавние звонки списком без рамок между карточками, только тонкие разделители.']),
    ('Настройки', scr_settings, '26_settings_bt.png', [
        'Разделы — список слева (значок + подпись) вместо сетки плиток 2×5; текущий — оранжевая «таблетка».',
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
<li><b>Без рамок.</b> Карточки отличаются от фона тоном (поверхность #1a1d22 на #0e1013), разделители в списках — 1 px.</li>
<li><b>Скругления крупнее:</b> карточки 20, окна 26, кнопки-«таблетки» — полукруглые.</li>
<li><b>Иерархия размером:</b> главное число 52–76 px, заголовок 26–34, текст 16–19, подписи 13–15. Регистр обычный, без
СПЛОШНЫХ ЗАГЛАВНЫХ.</li>
<li><b>Оранжевый — только для активного и главного действия.</b> Выбранное — мягкая оранжевая заливка (16%), главное
действие — сплошная.</li>
<li><b>Ромбы:</b> фон — крупная мозаика (64×112) почти в тон фону; оранжевые грани — на обложках и акцентных плитках.
Лёгкое оранжевое свечение в углу экрана.</li>
<li><b>Значки линейные</b> 24-сетка, линия 2 px; в нижней панели без подписей — подпись только у текущего раздела.</li>
<li><b>Контраст:</b> текст #f5f3ee на #1a1d22 — 15:1; вторичный #aab0b8 — 8:1; третичный #6c727c — для неважного.</li>
</ul></div>
<div class="note" style="width:330px"><h2>Токены</h2><table class="tk">
<tr><td><span class="tok" style="background:#0e1013"></span><b>фон</b> #0e1013</td></tr>
<tr><td><span class="tok" style="background:#1a1d22"></span><b>карточка</b> #1a1d22</td></tr>
<tr><td><span class="tok" style="background:#23272e"></span><b>кнопка / вложенное</b> #23272e</td></tr>
<tr><td><span class="tok" style="background:#2a2f37"></span><b>разделитель</b> #2a2f37</td></tr>
<tr><td><span class="tok" style="background:#ff6b1a"></span><b>акцент</b> #ff6b1a, заливка выбранного 16%</td></tr>
<tr><td><span class="tok" style="background:#f5f3ee"></span><b>текст</b> #f5f3ee · #aab0b8 · #6c727c</td></tr>
<tr><td><span class="tok" style="background:#33c27a"></span><b>вызов / норма</b> #33c27a</td></tr>
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
