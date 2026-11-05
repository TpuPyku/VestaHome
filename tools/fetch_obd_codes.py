#!/usr/bin/env python3
"""fetch_obd_codes.py - the dealer/community OBD trouble code tables for VestaHome, as MAIN/obdII_vesta.csv.

Both sources publish the codes and their meanings openly:
  * the Lada.Online digest of the AVTOVAZ instruction TH 3100.25100.12056 ("Electronic engine
    management system 21129 with the M86 controller, Euro-5"): the engine controller's own codes
    (P2128 and the CAN ones, U0001...), one block "VAZ (M86)";
  * the VestaXR owners' list "The full list of Lada trouble codes" (drive2.ru, 802 codes for Lada
    and Vesta NG, updated 20.03.2025): code + failure type byte + meaning, one block "Vesta (VestaXR)".

Writes MAIN/obdII_vesta.csv (cp1251, the same ';' fields as MAIN/obdII.csv: code;ECU;system;meaning),
which tools/make_obd_codes.py reads together with the user's table.

Usage: python VestaHome/tools/fetch_obd_codes.py
"""
import os
import re
import sys
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..', '..', 'MAIN', 'obdII_vesta.csv')

LADA = ('https://xn--80aal0a.xn--80asehdb/do-my-self/repair/'
        'repair-lada-vesta/803-rasshifrovka-kodov-oshibok-lada-vesta.html')
DRIVE2 = 'https://www.drive2.ru/l/700154162234543575/'
UA = {'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 '
                    '(KHTML, like Gecko) Chrome/124.0 Safari/537.36',
      'Accept-Language': 'ru-RU,ru;q=0.9'}

# the SAE letter of a code says the system it belongs to
SYSTEM = {'P': 'ЭСУД (силовой агрегат)',
          'B': 'Кузов (BCM)',
          'C': 'Шасси (тормоза, АБС/ESP)',
          'U': 'CAN-шина / связь'}
# the digest was typed with Cyrillic look-alikes in most codes (Р instead of P)
HOMOGLYPHS = {'Р': 'P', 'В': 'B', 'С': 'C', 'Е': 'E', 'А': 'A', 'О': 'O'}
# plain typos of the digest's text
FIXES = (('стишком', 'слишком'), ('посте нейтрализатора', 'после нейтрализатора'),
         ('бортов\\ю', 'бортовую'), ('цилиндра(датчика', 'цилиндра (датчика'))


def get(url, timeout=90):
    return urllib.request.urlopen(urllib.request.Request(url, headers=UA),
                                  timeout=timeout).read().decode('utf-8', 'replace')


def safe(s):
    """One CSV field of the table's encoding: no ';' separator, no typography cp1251 may lack."""
    for a, b in ((';', ','), ('\u2014', '-'), ('\u2013', '-'), ('\u00ab', '"'), ('\u00bb', '"'),
                 ('\u2026', '...'), ('\u2116', 'N'), ('\u00a0', ' ')):
        s = s.replace(a, b)
    s = ' '.join(s.split())
    return s.encode('cp1251', 'replace').decode('cp1251')


def parse_lada(html):
    """The M86 engine controller's own codes (TH 3100.25100.12056): 'P0601 <meaning>' lines of the digest."""
    i = html.find('<h1')
    j = html.find('Комментарии')
    seg = html[i:j] if i >= 0 and j > i else html
    seg = re.sub(r'(?is)<script.*?</script>|<style.*?</style>', '', seg)
    lines = [re.sub(r'[\s\u00a0]+', ' ', l).strip()
             for l in re.sub(r'(?s)<[^>]+>', '\n', seg).splitlines()]
    rows = []
    for l in lines:
        m = re.match(r'^(\S{5}) (\S.*)$', l)
        if not m:
            continue
        code, desc = m.group(1), m.group(2)
        code = HOMOGLYPHS.get(code[0], code[0]) + code[1:]
        if not re.match(r'^[PCBU][0-9A-F]{4}$', code) or len(desc) < 7:
            continue
        for a, b in FIXES:
            desc = desc.replace(a, b)
        rows.append((code.upper(), 'ВАЗ (М86)', SYSTEM[code[0]], desc))
    return rows



def parse_drive2(html):
    """'P0226-F1 - <meaning>' lines of the VestaXR list (the code + its failure type byte)."""
    m = re.search(r'(?s)itemprop="articleBody"[^>]*>(.*?)(?:</div>|<footer)', html)
    body = m.group(1) if m else html
    body = re.sub(r'(?i)<br\s*/?>|</p>', '\n', body)
    lines = [re.sub(r'[\s\u00a0]+', ' ', l).strip()
             for l in re.sub(r'(?s)<[^>]+>', '', body).splitlines()]
    rows = []
    for l in lines:
        m = re.match(r'^([PCBU][0-9A-F]{4})-([0-9A-F]{2})\s+[-\u2013\u2014]+\s*(\S.*)$', l)
        if m:
            rows.append((m.group(1), 'Веста (VestaXR)', SYSTEM[m.group(1)[0]],
                         '%s - %s' % (m.group(2), m.group(3))))
    return rows


def main():
    rows = []
    for name, url, parse in (('TH 3100.25100.12056 (M86)', LADA, parse_lada),
                             ('VestaXR list', DRIVE2, parse_drive2)):
        try:
            rows += parse(get(url))
        except Exception as e:
            sys.exit('%s: %s' % (name, e))   # keep the previous file, say why
    seen = set()
    out = []
    for code, ecu, system, meaning in rows:
        row = ';'.join([code, safe(ecu), safe(system), safe(meaning)])
        if row not in seen:
            seen.add(row)
            out.append(row)
    if not out:
        sys.exit('nothing found, the table is left as it was')
    tmp = OUT + '.new'
    with open(tmp, 'w', encoding='cp1251', newline='') as f:
        f.write('\r\n'.join(sorted(out, key=lambda r: r.split(';', 1)[0])) + '\r\n')
    os.replace(tmp, OUT)
    print('%s: %d codes' % (os.path.normpath(OUT), len(out)))


if __name__ == '__main__':
    main()
