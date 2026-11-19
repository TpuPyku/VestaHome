#!/usr/bin/env python3
"""The radio stations' table for VestaHome: a city and a frequency -> the station's name and its logo (the user
2026-11-18: RDS in the CIS often gives no name or a wrong one; the station by the city and the frequency instead).

    python make_radio.py <radio_data.zip> <cities1000.zip> <out folder> [--size 64]

radio_data.zip (from the user; not in any repository - the logos are the stations'): radio_freqs_RU.json and
radio_freqs_BY.json ({"radio": [{cityName, country, freqNum (x10 kHz), freqName, freqLogo}]}) and logo300x300/<id>.png.
cities1000.zip: GeoNames' cities of 1000 people and more (CC BY 4.0, download.geonames.org/export/dump) - the cities'
places, by their Russian names.

Out: stations.txt (UTF-8, a station a line "lat;lon;f10;logo;name", by the city) and logos/<id>.png (size x size).
"""
import argparse
import io
import json
import os
import sys
import zipfile

NEAR = ('RU', 'BY', 'UA', 'KZ')   # the countries a city is looked for in (its own first; Crimea is UA in GeoNames)


def norm(name):
    return name.strip().lower().replace('ё', 'е')


LAT = dict(zip('абвгдеёжзийклмнопрстуфхцчшщъыьэюя',
               ['a', 'b', 'v', 'g', 'd', 'e', 'e', 'zh', 'z', 'i', 'y', 'k', 'l', 'm', 'n', 'o', 'p', 'r', 's', 't',
                'u', 'f', 'kh', 'ts', 'ch', 'sh', 'shch', '', 'y', '', 'e', 'yu', 'ya']))


def sound(name):
    """A name spelt loosely in Latin letters, to compare a Russian one with GeoNames' Latin one (Мирный ~ Mirnyy)."""
    s = ''.join(LAT.get(c, c) for c in name.lower())
    s = ''.join(c for c in s if 'a' <= c <= 'z').replace('j', 'y').replace('iy', 'y').replace('yy', 'y')
    return s.rstrip('y')


def city_index(lines):
    """GeoNames' lines -> {(name, country): (lat, lon, rank)}: of a name, the city whose own (Latin) name it is
    before one that only had it (an old name: Тольятти was Ставрополь), then the most populous."""
    index = {}
    for line in lines:
        f = line.rstrip('\n').split('\t')
        if len(f) < 15 or f[8] not in NEAR:
            continue
        own = sound(f[2])
        for n in {norm(f[1]), norm(f[2])} | {norm(a) for a in f[3].split(',') if a}:
            place = (float(f[4]), float(f[5]), (sound(n) == own, int(f[14] or 0)))
            if (n, f[8]) not in index or place[2] > index[(n, f[8])][2]:
                index[(n, f[8])] = place
    return index


def locate(name, country, index):
    """The city's (lat, lon): in its country, else the neighbours'; None when not found."""
    for cc in (country,) + NEAR:
        place = index.get((norm(name), cc))
        if place:
            return place[:2]
    return None


def station_lines(radio, index):
    """The stations' lines (by the city, then the frequency) and the cities not found."""
    rows, missed = [], []
    for r in radio:
        place = locate(r['cityName'], r['country'], index)
        if not place:
            city = '%s (%s)' % (r['cityName'], r['country'])
            if city not in missed:
                missed.append(city)
            continue
        name = ' '.join(r['freqName'].replace(';', ' ').split())
        rows.append((place, int(r['freqNum']), r['freqLogo'], name))
    rows.sort()
    return ['%.3f;%.3f;%d;%s;%s' % (p[0], p[1], f, logo, name) for p, f, logo, name in rows], missed


def logos(data, out, size):
    """The logos the table names, size x size PNG (RGBA kept)."""
    from PIL import Image
    os.makedirs(out, exist_ok=True)
    n = 0
    for name in data.namelist():
        if name.startswith('logo300x300/') and name.endswith('.png'):
            im = Image.open(io.BytesIO(data.read(name))).convert('RGBA')
            im.resize((size, size), Image.LANCZOS).save(os.path.join(out, name.split('/')[-1]), optimize=True)
            n += 1
    return n


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument('data')
    ap.add_argument('geonames')
    ap.add_argument('out')
    ap.add_argument('--size', type=int, default=64)
    a = ap.parse_args(argv)
    data = zipfile.ZipFile(a.data)
    radio = []
    for j in ('radio_freqs_RU.json', 'radio_freqs_BY.json'):
        radio += json.loads(data.read(j))['radio']
    with zipfile.ZipFile(a.geonames) as g:
        index = city_index(io.TextIOWrapper(g.open('cities1000.txt'), encoding='utf-8'))
    lines, missed = station_lines(radio, index)
    os.makedirs(a.out, exist_ok=True)
    with open(os.path.join(a.out, 'stations.txt'), 'w', encoding='utf-8', newline='\r\n') as f:
        f.write('\n'.join(lines) + '\n')
    n = logos(data, os.path.join(a.out, 'logos'), a.size)
    print('stations %d, logos %d (%dx%d)' % (len(lines), n, a.size, a.size))
    for m in missed:
        print('  city not found: %s' % m)
    return 0


if __name__ == '__main__':
    sys.exit(main())
