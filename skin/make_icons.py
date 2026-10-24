#!/usr/bin/env python3
"""make_icons.py - VestaHome's icons from Lucide (lucide.dev, ISC licence; design 0.6, the user 2026-10-23).

Each VestaHome icon (its skin key, kIconKey in home.cpp) takes a Lucide icon: the SVG (cached in skin/lucide/, the
package's licence beside them) is drawn white with its own antialiasing into a 48 x 48 PNG with transparency,
VestaHome/VestaHome/Skin/<key>.png; VestaHome tints it with the colour it draws in. The skin.ini [images] list is
written again (the saver's picture kept). A few are made of two (the phone's music: a note with the Bluetooth sign)
or filled (the favourite's star, the player's buttons).

Usage: python VestaHome/skin/make_icons.py
"""
import os
import re
import urllib.request

import pymupdf

HERE = os.path.dirname(os.path.abspath(__file__))
SVG = os.path.join(HERE, 'lucide')
SKIN = os.path.join(HERE, '..', 'VestaHome', 'VestaHome', 'Skin')
VERSION = '1.61.0'
BASE = 'https://unpkg.com/lucide-static@%s/' % VERSION
SIZE = 48

# VestaHome's icon key -> Lucide's name ("a+b": b small in the corner; "!name": filled)
ICONS = {
    'map': 'map', 'nav': 'navigation', 'radio': 'radio', 'music': 'music', 'phone': 'phone',
    'settings': 'settings', 'home': 'house', 'night': 'moon', 'prev': 'chevron-left', 'day': 'sun',
    'apps': 'layout-grid', 'video': 'clapperboard', 'next': 'chevron-right', 'exit': 'x', 'folder': 'folder',
    'shift': 'arrow-big-up', 'backspace': 'delete', 'aux_ico': 'cable', 'photo': 'image', 'info': 'info',
    'rew': '!skip-back', 'fwd': '!skip-forward', 'pause': '!pause', 'bt': 'bluetooth', 'usb': 'usb', 'list': 'list',
    'vol': 'volume-2', 'search': 'search', 'shuffle': 'shuffle', 'repeat': 'repeat', 'call': 'phone',
    'hangup': 'phone-off', 'wifi': 'wifi', 'star': 'star', 'star_f': '!star', 'user': 'user', 'users': 'users',
    'clock': 'clock', 'keypad': 'grid-3x3', 'mic_off': 'mic-off', 'handover': 'smartphone', 'play': '!play',
    'rot_l': 'rotate-ccw', 'rot_r': 'rotate-cw', 'warn': 'triangle-alert', 'call_in': 'phone-incoming',
    'call_out': 'phone-outgoing', 'call_miss': 'phone-missed', 'lock': 'lock', 'refresh': 'refresh-cw',
    'eq': 'sliders-vertical', 'disc': 'disc', 'eye': 'eye', 'power': 'power', 'edit': 'pencil', 'add': 'plus',
    'file': 'file', 'up': 'chevron-up', 'car': 'car', 'cloud': 'cloud', 'tasks': 'activity', 'sd': 'save',
    'down': 'chevron-down', 'gallery': 'images', 'full': 'maximize', 'unfull': 'minimize',
    'netradio': 'radio-tower', 'btmusic': 'music+bluetooth', 'return': 'undo-2',
}


def svg_of(name):
    path = os.path.join(SVG, name + '.svg')
    if not os.path.exists(path):
        os.makedirs(SVG, exist_ok=True)
        data = urllib.request.urlopen(BASE + 'icons/%s.svg' % name, timeout=30).read()
        open(path, 'wb').write(data)
    return open(path, encoding='utf-8').read()


def inner(svg):   # the drawing inside <svg>: its elements, the comment line dropped
    body = svg[svg.index('>', svg.index('<svg')) + 1:svg.rindex('</svg>')]
    return re.sub(r'<!--.*?-->', '', body, flags=re.S)


def compose(spec):
    fill = spec.startswith('!')
    spec = spec.lstrip('!')
    parts = spec.split('+')
    body = '<g>%s</g>' % inner(svg_of(parts[0]))
    if len(parts) > 1:   # the second small in the lower right corner, the first moved up-left to make room
        body = ('<g transform="translate(-2 -1) scale(.9)">%s</g><g transform="translate(13 11) scale(.5)">'
                '<rect x="-2" y="-2" width="28" height="28" fill="none" stroke="none"/>%s</g>'
                % (inner(svg_of(parts[0])), inner(svg_of(parts[1]))))
    return ('<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="%s" '
            'stroke="#ffffff" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">%s</svg>'
            % ('#ffffff' if fill else 'none', body))


def main():
    os.makedirs(SKIN, exist_ok=True)
    lic = os.path.join(SVG, 'LICENSE')
    if not os.path.exists(lic):
        os.makedirs(SVG, exist_ok=True)
        open(lic, 'wb').write(urllib.request.urlopen(BASE + 'LICENSE', timeout=30).read())
    made = []
    for key, spec in ICONS.items():
        try:
            svg = compose(spec)
        except Exception as e:   # not in this version of the set
            print('MISSING %s (%s): %s' % (key, spec, e))
            continue
        doc = pymupdf.open(stream=svg.encode('utf-8'), filetype='svg')
        pix = doc[0].get_pixmap(matrix=pymupdf.Matrix(SIZE / 24.0, SIZE / 24.0), alpha=True)
        pix.save(os.path.join(SKIN, key + '.png'))
        made.append(key)
    # skin.ini: its [images] again, the rest (colours, the saver) as it is
    ini = os.path.join(SKIN, 'skin.ini')
    lines = open(ini, encoding='utf-8').read().splitlines()
    at = [i for i, l in enumerate(lines) if l.strip().lower() == '[images]']   # the section's own line, not a comment
    head, tail = (lines[:at[0]], lines[at[0] + 1:]) if at else (lines, [])
    keep = [l for l in tail if l.strip() and l.split('=')[0].strip() in ('saver', 'background', 'tile')]
    out = head + ['[images]'] + ['%s = %s.png' % (k, k) for k in made] + keep
    open(ini, 'w', encoding='utf-8', newline='\r\n').write('\n'.join(out) + '\n')
    print('%d icons -> %s' % (len(made), os.path.normpath(SKIN)))


if __name__ == '__main__':
    main()
