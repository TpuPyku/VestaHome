#!/usr/bin/env python3
"""add_dtc_block.py - add a unit's own (dealer) trouble code list to MAIN/obdII_vesta_modules.csv.

The lists are pasted as text, one code a line: "B1001-11 Короткое замыкание ..." (the failure type byte of
the scanner - one or two hex digits, B2FA1-1 is the byte 01 - a dash, the meaning) or just "B1001 meaning".
Cyrillic look-alikes - in the code and in the type byte (В2001, В2053-1С) - are straightened to Latin, an en
dash becomes '-', the line turns into a row of the table (code;ECU;system;'type - meaning'), the same
code+type already in the block is skipped, and the file stays cp1251 with CRLF.
tools/make_obd_codes.py then merges the block into obd_codes.txt.

Usage: python VestaHome/tools/add_dtc_block.py <list.txt> "<БДФКЭ (Веста)>" "<Кузов (комфорт)>"
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..', '..', 'MAIN', 'obdII_vesta_modules.csv')
HOMOGLYPHS = {'Р': 'P', 'В': 'B', 'С': 'C', 'Е': 'E', 'А': 'A', 'О': 'O', 'К': 'K', 'М': 'M', 'Т': 'T',
              'Х': 'X', 'Н': 'H'}
CODE = re.compile(r'^([0-9A-Za-zА-Яа-я]{4,6})\s*[-–—]\s*([0-9A-Za-zА-Яа-я]{1,2})\s+(.+)$')
TYPE = re.compile(r'^[0-9A-F]{2}$')
PLAIN = re.compile(r'^([0-9A-Za-zА-Яа-я]{4,6})\s+(.+)$')


def latin(s):
    return ''.join(HOMOGLYPHS.get(c, c) for c in s).upper()


def field(s):
    """One CSV field: no ';', the typography cp1251 keeps but the tables avoid, single spaces."""
    for a, b in ((';', ','), ('–', '-'), ('—', '-'), ('«', '"'), ('»', '"'), ('…', '...'), ('\u2116', 'N'),
                 ('\u00a0', ' ')):
        s = s.replace(a, b)
    s = ' '.join(s.split())
    return s.encode('cp1251', 'replace').decode('cp1251')


def parse(text):
    """'B1001-11 text' -> ('B1001', '11 - text'); 'B1001 text' -> ('B1001', 'text').
    A type byte written with one digit is the same byte with a lost leading zero (B2FA1-1 -> 01), so it is
    padded: the tables and the merge key keep two digits."""
    text = text.replace('\ufeff', '').strip()
    m = CODE.match(text)
    if m:
        t = latin(m.group(2))
        if len(t) == 1:
            t = '0' + t
        if TYPE.match(t):
            return latin(m.group(1)), '%s - %s' % (t, m.group(3))
    m = PLAIN.match(text)                      # a list without the failure type byte
    if m:
        return latin(m.group(1)), m.group(2)
    return latin(text), ''


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__.strip().splitlines()[-1])
    src, ecu, system = sys.argv[1], sys.argv[2], (sys.argv[3] if len(sys.argv) > 3 else 'Кузов')
    rows, seen = [], set()
    if os.path.exists(OUT):
        for line in open(OUT, encoding='cp1251'):
            line = line.rstrip('\r\n')
            if line.strip():
                rows.append(line)
                f = line.split(';')
                if len(f) > 3:
                    seen.add((f[1], f[0], f[3].split(' - ')[0]))
    added = 0
    for line in open(src, encoding='utf-8-sig'):
        if not line.strip():
            continue
        code, rest = parse(line)
        if len(code) != 5 or not rest:
            print('skipped: %s' % line.strip())
            continue
        if (ecu, code, rest.split(' - ')[0]) in seen:      # the same code and type is already in the block
            continue
        rows.append(';'.join([code, field(ecu), field(system), field(rest)]))
        seen.add((ecu, code, rest.split(' - ')[0]))
        added += 1
    if not added:
        sys.exit('nothing added, %s is left as it was' % os.path.normpath(OUT))
    rows.sort(key=lambda r: (r.split(';')[0], r))
    tmp = OUT + '.new'
    with open(tmp, 'w', encoding='cp1251', newline='') as f:
        f.write('\r\n'.join(rows) + '\r\n')
    os.replace(tmp, OUT)
    print('%s: +%d, %d codes' % (os.path.normpath(OUT), added, len(rows)))


if __name__ == '__main__':
    main()
