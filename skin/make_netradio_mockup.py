#!/usr/bin/env python3
"""make_netradio_mockup.py - Internet radio with favourites and the radio-browser.info catalogue -> netradio-v0.7.html
+ .pdf (the user 2026-11-04: "the catalogue as switching the playlist with the stations we have, laid out by
categories; a separate mockup on VestaHome's").

The screens in VestaHome's design (make_home_mockup.py's CSS, the Lucide icons), the accent of the user's theme
(screen.accent of the PC build's VestaHome.ini, as make_vh_v07_mockup.py), beside each the screen as it is now
(docs/shots). The PDF is printed by headless Edge.

Usage: python VestaHome/skin/make_netradio_mockup.py
"""
import importlib.util
import os
import subprocess

HERE = os.path.dirname(os.path.abspath(__file__))
SHOTS = os.path.join(HERE, '..', 'docs', 'shots')
import make_home_mockup as hm
from make_home_mockup import ico

_spec = importlib.util.spec_from_file_location('osm', os.path.join(HERE, 'make_osm_v3_mockup.py'))
_osm = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_osm)
ACC_NAME, ACC = _osm.theme_accent()
hm.P.update({'tower': 'radio-tower', 'trend': 'trending-up', 'globe': 'globe', 'lang': 'languages', 'tag': 'tag',
             'up': 'chevron-up', 'down': 'chevron-down', 'undo': 'undo-2', 'starF': '!star', 'pin': 'map-pin'})
def _shade(k):
    return '%%23%02x%02x%02x' % tuple(int(c * k) for c in ACC)


FACET = ('<svg xmlns="http://www.w3.org/2000/svg" width="40" height="70" viewBox="0 0 40 70">'   # the cover's facets
         '<path d="M0 0 20 35 0 70z" fill="%s"/><path d="M40 0 20 35 40 70z" fill="%s"/>'
         '<path d="M0 0h40L20 35z" fill="%s"/><path d="M0 70h40L20 35z" fill="%s"/></svg>') % (
             _shade(.62), _shade(.52), _shade(.7), _shade(.45))
CSS = hm.CSS.replace(hm.FACET, FACET).replace('--or:#ff6b1a; --orS:rgba(255,107,26,.16)',
                     '--or:#%02x%02x%02x; --orS:rgba(%d,%d,%d,.16)' % (ACC + ACC)) + '''
.seg { position:absolute; left:16px; right:16px; top:60px; height:48px; display:flex; gap:8px; }
.seg .pill { height:44px; font-size:16px; }
.lst { position:absolute; left:16px; right:88px; top:116px; bottom:72px; background:var(--s1); border-radius:16px;
       padding:4px 8px; overflow:hidden; }
.it { display:flex; align-items:center; gap:14px; height:58px; padding:0 14px; border-radius:12px; font-size:17px;
      font-weight:bold; color:var(--t2); }
.it.a { background:var(--orS); color:var(--or); } .it .s { font-size:13px; font-weight:normal; color:var(--t3); }
.it .f { margin-left:auto; color:var(--t3); } .it .f.on { color:var(--or); }
.scroll { position:absolute; right:16px; top:116px; bottom:72px; width:56px; display:flex; flex-direction:column;
          justify-content:space-between; align-items:center; }
.scroll .btn { width:56px; height:92px; border-radius:20px; background:var(--s1); color:var(--t2); }
.scroll .rail { width:4px; flex:1; margin:10px 0; border-radius:2px; background:var(--s2); position:relative; }
.scroll .rail i { position:absolute; left:0; right:0; top:30%; height:14%; border-radius:2px; background:var(--t2); }
.foot { position:absolute; left:16px; right:16px; bottom:10px; height:52px; display:flex; align-items:center; gap:12px; }
.foot .btn { width:52px; height:52px; color:var(--t2); }
.tile { background:var(--s1); border-radius:18px; padding:16px 18px; display:flex; flex-direction:column; gap:8px; }
.tile .n { font-size:19px; font-weight:bold; } .tile .s { font-size:13px; color:var(--t2); }
.tile svg { color:var(--or); }
.chip { display:flex; align-items:center; justify-content:space-between; height:52px; padding:0 18px; border-radius:14px;
        background:var(--s1); font-size:17px; font-weight:bold; color:var(--t2); }
.chip span { font-size:13px; font-weight:normal; color:var(--t3); }
.key { background:var(--s2); border-radius:8px; height:44px; display:flex; align-items:center; justify-content:center;
       font-size:18px; color:var(--tx); }
'''

MINE = ['Радио 7 на семи холмах', 'Монте-Карло', 'DFM', 'Радио Рекорд', 'Дискотека 90-х']
FAV = {'DFM', 'Радио Рекорд'}


def head(title, back=False):
    return hm.top(title, back=back, play='radio').replace('12:47', '16:21')


def tabs(active):   # the media sources' pills, as VestaHome now
    out = ''
    for n in ('Музыка', 'Интернет', 'Bluetooth', 'AUX', 'Видео', 'Фото'):
        out += '<span class="pill%s" style="height:40px;font-size:14px">%s</span>' % (' a' if n == active else '', n)
    return ('<div style="position:absolute;left:16px;right:16px;top:62px;display:flex;gap:8px;align-items:center">%s'
            '<span style="flex:1"></span><span class="btn" style="width:48px;height:48px;color:var(--t2)">%s</span></div>'
            % (out, ico('list', 24)))


def lists(active, counts=(28, 6)):   # the playlist switch: ours, the favourites, the catalogue
    items = [('Мои станции', counts[0]), ('Избранное', counts[1]), ('Каталог', None)]
    out = ''
    for name, n in items:
        lab = name + ('' if n is None else ' <span style="opacity:.7;font-weight:normal">%d</span>' % n)
        icon = ico('starF', 18) if name == 'Избранное' else ico('globe', 18) if name == 'Каталог' else ico('radio', 18)
        out += '<span class="pill%s">%s%s</span>' % (' a' if name == active else '', icon, lab)
    return '<div class="seg">%s<span style="flex:1"></span><span class="pill">%s Поиск</span></div>' % (out, ico('search', 18))


def rows(items):   # (name, second line or '', active, favourite)
    out = ''
    for name, sub, act, fav in items:
        out += ('<div class="it%s">%s<div>%s%s</div><span class="f%s">%s</span></div>' % (
            ' a' if act else '', ico('radio', 22), name, '<div class="s">%s</div>' % sub if sub else '',
            ' on' if fav else '', ico('starF' if fav else 'star', 24)))
    return '<div class="lst">%s</div>' % out


def scroll():
    return ('<div class="scroll"><span class="btn">%s</span><span class="rail"><i></i></span><span class="btn">%s</span>'
            '</div>' % (ico('up', 26), ico('down', 26)))


def foot(extra=''):
    return '<div class="foot"><span class="btn">%s</span><span style="flex:1"></span>%s</div>' % (ico('undo', 24), extra)


def scr_player():
    return '''<div class="scr">%s%s
<div class="facet" style="position:absolute;left:16px;top:124px;width:232px;height:232px;border-radius:20px;
  display:flex;align-items:center;justify-content:center">
  <span style="color:#0e1013">%s</span></div>
<div style="position:absolute;left:272px;top:132px;right:24px">
  <span class="pill a" style="height:34px;font-size:13px">%s Избранное · 2 из 6 %s</span>
  <div style="display:flex;align-items:center;gap:14px;margin-top:12px">
    <div style="font-size:34px;font-weight:bold">DFM</div><span class="or">%s</span></div>
  <div class="t2" style="font-size:15px;margin-top:4px">Dua Lipa — Levitating</div>
  <div class="t3" style="font-size:13px;margin-top:2px">Россия · AAC 64 кбит/с</div></div>
<div style="position:absolute;left:390px;top:294px;display:flex;gap:46px;align-items:center;color:var(--t2)">
  %s<span class="btn play" style="width:84px;height:84px">%s</span>%s</div>
%s</div>''' % (head('Медиа · Интернет-радио'), tabs('Интернет'), ico('tower', 96, '#0e1013'), ico('starF', 14),
               ico('chev', 14), ico('starF', 30), ico('prev', 40), ico('pause', 40, '#0e1013'), ico('next', 40),
               hm.bot('music'))


KEEP = ('<span class="btn" style="background:none">%s</span><span class="btn" style="background:none">%s</span>'
        % (ico('radio', 30), ico('gear', 30)))   # the list's own buttons, as now (to the radio; the settings)


def scr_mine():
    items = [(n, '', n == 'DFM', n in FAV) for n in MINE]
    return '<div class="scr">%s%s%s%s%s</div>' % (head('Интернет-радио · Станции'), lists('Мои станции'), rows(items),
                                                  scroll(), foot(KEEP))


def scr_fav():
    items = [('DFM', 'Мои станции', True, True), ('Радио Рекорд', 'Мои станции', False, True),
             ('Jazz24', 'Каталог · США · джаз', False, True), ('SomaFM Groove Salad', 'Каталог · США · эмбиент', False, True),
             ('Радио Ваня', 'Каталог · Россия · поп', False, True)]
    return '<div class="scr">%s%s%s%s%s</div>' % (head('Интернет-радио · Избранное'), lists('Избранное'), rows(items),
                                                  scroll(), foot())


def scr_cat():
    tiles = [('trend', 'Популярные', 'чаще всего слушают'), ('pin', 'Россия', 'страна по GPS'),
             ('tag', 'Жанры', 'поп, рок, джаз, новости…'), ('globe', 'Страны', '200+'),
             ('lang', 'Языки', 'русский, английский…'), ('clock', 'Недавние', 'что вы включали')]
    cells = ''.join('<div class="tile">%s<div class="n">%s</div><div class="s">%s</div></div>' % (ico(i, 28), n, s)
                    for i, n, s in tiles)
    return '''<div class="scr">%s%s
<div style="position:absolute;left:16px;right:16px;top:116px;display:grid;grid-template-columns:repeat(3,1fr);gap:12px">%s</div>
<div class="t3" style="position:absolute;left:24px;bottom:84px;font-size:12px">radio-browser.info · ~50 000 станций · список
обновляется при открытии, раз в сутки</div>%s</div>''' % (head('Интернет-радио · Каталог'), lists('Каталог'), cells, foot())


def scr_genres():
    g = [('Поп', 6120), ('Рок', 4310), ('Новости', 3870), ('Электроника', 2950), ('Танцевальная', 2400),
         ('Классика', 1980), ('Джаз', 1760), ('Ретро', 1540), ('Хиты 80-х', 1210), ('Разговорное', 1150),
         ('Шансон', 310), ('Детское', 290)]
    cells = ''.join('<div class="chip">%s<span>%s</span></div>' % (n, '{:,}'.format(c).replace(',', ' ')) for n, c in g)
    return '''<div class="scr">%s
<div style="position:absolute;left:24px;top:64px;font-size:15px" class="t2">Каталог › <b style="color:var(--tx)">Жанры</b>
  <span class="t3"> · по числу станций</span></div>
<div style="position:absolute;left:16px;right:88px;top:96px;bottom:72px;display:grid;grid-template-columns:repeat(3,1fr);
  gap:10px;align-content:start">%s</div>%s%s</div>''' % (head('Интернет-радио · Каталог', back=False), cells, scroll(), foot())


def scr_stations():
    items = [('Jazz24', 'США · MP3 128', False, True), ('Smooth Jazz Florida', 'США · MP3 128', False, False),
             ('Radio Swiss Jazz', 'Швейцария · AAC 96', True, False), ('Jazz FM', 'Россия · AAC 64', False, False),
             ('TSF Jazz', 'Франция · MP3 128', False, False)]
    return '''<div class="scr">%s
<div style="position:absolute;left:24px;top:64px;right:24px;display:flex;font-size:15px" class="t2">Каталог › Жанры ›
  <b style="color:var(--tx);margin-left:4px">Джаз</b><span style="flex:1"></span><span class="t3">1–50 из 1 760</span></div>
%s%s%s</div>''' % (head('Интернет-радио · Каталог'), rows(items).replace('top:116px', 'top:96px'),
                   scroll().replace('top:116px', 'top:96px'),
                   foot('<span class="pill">Ещё 50</span><span class="pill">%s Сорт.: популярные</span>' % ico('trend', 18)))


def scr_search():
    keys = ''
    for row in ('йцукенгшщзхъ', 'фывапролджэ', 'ячсмитьбю'):
        keys += ('<div style="display:grid;grid-template-columns:repeat(%d,1fr);gap:6px">%s</div>' % (
            len(row), ''.join('<div class="key">%s</div>' % c for c in row)))
    return '''<div class="scr">%s
<div style="position:absolute;left:16px;right:16px;top:62px;height:50px;border-radius:14px;background:var(--s1);
  display:flex;align-items:center;gap:12px;padding:0 16px;font-size:20px"><span class="t3">%s</span>джаз<span
  style="width:2px;height:26px;background:var(--or)"></span><span style="flex:1"></span><span class="t3"
  style="font-size:13px">в названии и жанре</span></div>
<div style="position:absolute;left:16px;right:16px;top:122px;display:flex;gap:8px">
  <span class="pill a">Jazz24</span><span class="pill">Радио Джаз</span><span class="pill">Jazz FM</span>
  <span class="pill">TSF Jazz</span><span class="pill">ещё 236 ›</span></div>
<div style="position:absolute;left:16px;right:16px;top:180px;display:flex;flex-direction:column;gap:6px">%s
  <div style="display:grid;grid-template-columns:2fr 6fr 2fr;gap:6px"><div class="key">123</div><div class="key">пробел</div>
  <div class="key" style="background:var(--or);color:#0e1013;font-weight:bold">Найти</div></div></div>
%s</div>''' % (head('Интернет-радио · Поиск'), ico('search', 22), keys, foot())


SCREENS = [
    ('1. Плеер: из какого списка играет', scr_player, '09_netradio.png', [
        '<b>Над названием — список, из которого играет:</b> «Избранное · 2 из 6». Касание открывает выбор списка.',
        '<b>⏮ ⏭, кнопки руля и крутилка</b> листают станции <b>только этого списка</b>: «Мои станции» (radio.txt), '
        '«Избранное» или открытая страница каталога (например, «Джаз», 50 станций).',
        '<b>★ рядом с названием</b> — в избранное / убрать. Под названием — что играет (ICY), страна и поток.',
        'Обложка остаётся нашей плиткой: значки станций из каталога (favicon) — разные форматы и размеры, '
        'на ГУ это память; можно позже, только PNG/JPEG до 64 КБ и с кэшем на SD.']),
    ('2. Списки: «Мои станции»', scr_mine, '10_netradio_list.png', [
        '<b>Сверху — переключатель списков:</b> Мои станции (28) · Избранное (6) · Каталог; справа — Поиск.',
        'Мои станции — как сейчас: radio.txt рядом с VestaHome, порядок файла.',
        '<b>★ в каждой строке</b>: касание звезды — в избранное / убрать; касание строки — играть.']),
    ('3. Избранное', scr_fav, None, [
        'Станции из любых списков в одном месте. Вторая строка — откуда станция (Мои / Каталог · страна · жанр).',
        'Хранится в <b>radio_fav.txt</b> рядом с VestaHome (тот же формат «название|адрес», плюс id каталога): '
        'переживает перезагрузку, правится руками, как radio.txt.',
        'Порядок — как добавляли; позже можно «поднять / опустить» долгим нажатием.']),
    ('4. Каталог: категории', scr_cat, None, [
        '<b>Шесть входов:</b> Популярные (по прослушиваниям), Россия (страна по GPS, без GPS — по языку), Жанры, '
        'Страны, Языки, Недавние (последние 20 включённых — на SD).',
        'Каталог — radio-browser.info: бесплатный, открытый, без ключа; ~50 000 станций.']),
    ('5. Жанры (и так же страны, языки)', scr_genres, None, [
        'Плитки по числу станций (числа здесь — для примера). Названия жанров каталога — по-английски («jazz», '
        '«news»); частые переводим своим словарём, редкие показываем как есть.',
        'Страны и языки — тем же экраном, с флагом-буквами страны (RU, DE).']),
    ('6. Станции жанра', scr_stations, None, [
        '<b>Страница по 50 станций</b>, сорт.: популярные (или по названию); «Ещё 50» — следующая страница. '
        'В памяти — только открытая страница (~15 КБ).',
        'Вторая строка: страна, формат и поток. Только то, что VestaHome играет: MP3 и AAC, HTTP и HTTPS; '
        'нерабочие станции каталог скрывает сам.',
        'Касание — играть; ⏮ ⏭ после этого листают эти 50.']),
    ('7. Поиск', scr_search, None, [
        'Своя клавиатура VestaHome (как для Wi-Fi), по-русски и латиницей. Ищет в названии и жанре; '
        'первые результаты сразу над клавиатурой, «ещё…» — полный список.']),
]

NOTES = '''<h2>Как это работает</h2><div class="note" style="font-size:13px"><ul>
<li><b>Источник:</b> API radio-browser.info по HTTPS (наш клиент https.cpp и корни из trust_anchors.inc; корень
сертификата серверов проверить при реализации; Let's Encrypt — ISRG X1 уже есть). Сервер — из списка all.api.radio-browser.info, при ошибке — следующий.
Свой User-Agent «VestaHome/0.7», как просят правила каталога.</li>
<li><b>Запросы:</b> жанры <code>/json/tags?order=stationcount&amp;reverse=true&amp;limit=100</code>, страны
<code>/json/countries</code>, языки <code>/json/languages</code>, станции <code>/json/stations/bytag/джаз?order=clickcount
&amp;reverse=true&amp;limit=50&amp;offset=0&amp;hidebroken=true&amp;codec=…</code>, популярные <code>/json/stations/topclick/50</code>,
поиск <code>/json/stations/search?name=…</code>. При включении станции — <code>/json/url/&lt;id&gt;</code> (каталог так считает
прослушивания и отдаёт рабочий адрес).</li>
<li><b>Память и сеть:</b> ответы JSON читаются потоком, в памяти — только нужные поля открытой страницы; списки
жанров, стран и языков кэшируются на SD на сутки (без SD — только в памяти). Без интернета — «Каталог недоступен»,
Мои станции и Избранное работают как раньше.</li>
<li><b>Файлы:</b> radio.txt (Мои станции) — без изменений; radio_fav.txt — Избранное; radio_recent.txt — Недавние (на SD).</li>
<li><b>Выбранный список и станция</b> запоминаются, как сейчас станция: после перезапуска играет тот же список.</li>
<li><b>Акцент</b> на макете — из вашей темы (%s); на ГУ — тот, что выбран.</li>
</ul></div>''' % ACC_NAME


def main():
    pages = ['<div class="page"><h1>Интернет-радио: избранное и каталог</h1><p class="lead">Макет 2026-11-04 на основе '
             'VestaHome 0.6. Списки станций переключаются, как плейлисты: Мои станции, Избранное, Каталог '
             '(radio-browser.info) по категориям. На каждой странице сверху — экран, ниже — как он выглядит сейчас и пояснения.</p>%s</div>' % NOTES]
    for title, fn, shot, notes in SCREENS:
        was = ''
        if shot and os.path.exists(os.path.join(SHOTS, shot)):
            was = '<div class="was"><span>Сейчас</span><img src="../docs/shots/%s"></div>' % shot
        pages.append('<div class="page"><h2>%s</h2><div class="zoom">%s</div><div class="row" style="margin-top:12px">%s'
                     '<div class="note" style="flex:1"><ul>%s</ul></div></div></div>'
                     % (title, fn(), was, ''.join('<li>%s</li>' % n for n in notes)))
    html = ('<!doctype html><html lang="ru"><head><meta charset="utf-8"><title>Интернет-радио VestaHome</title>'
            '<style>%s</style></head><body>%s</body></html>' % (CSS, '\n'.join(pages)))
    hp = os.path.join(HERE, 'netradio-v0.7.html')
    open(hp, 'w', encoding='utf-8').write(html)
    edge = r'C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe'
    pdf = os.path.join(HERE, 'netradio-v0.7.pdf')
    subprocess.run([edge, '--headless', '--disable-gpu', '--no-pdf-header-footer', '--print-to-pdf=' + pdf,
                    'file:///' + hp.replace('\\', '/')], check=False, timeout=120)
    print(hp, pdf, os.path.exists(pdf))


if __name__ == '__main__':
    main()
