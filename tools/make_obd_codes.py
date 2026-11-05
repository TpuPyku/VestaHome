#!/usr/bin/env python3
"""make_obd_codes.py - the OBD trouble codes' meanings for VestaHome ("Car -> Ошибки (OBD)", a tap on a code).

Reads the tables (cp1251, ';'-separated: code;ECU;system;meaning[;more]) and writes
VestaHome/VestaHome/obd_codes.txt (UTF-8, one code a line, the same fields; a code may have several lines -
for different ECUs), which VestaHomeInstall carries next to the exe. By default MAIN/obdII.csv (the user's
table), MAIN/obdII_vesta.csv (tools/fetch_obd_codes.py) and MAIN/obdII_vesta_modules.csv (the units' own
dealer lists, filled in by hand).

A code may be claimed by several tables: the unit's own list wins over the community one over the generic
VAZ one (RANK), and a row whose meaning repeats a row already taken for that code is dropped - the window
shows only the first kDtcInfoMax (3) lines of a code, so duplicates would hide the useful ones. U-codes are
the exception: 'no link with X' names the unit's own partner, so lists of different units may keep their own
wording for one U-code (see the loop below).

Usage: python VestaHome/tools/make_obd_codes.py [table.csv ...]
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
MAIN = os.path.join(HERE, '..', '..', 'MAIN')
SRC = sys.argv[1:] or [os.path.join(MAIN, 'obdII.csv'), os.path.join(MAIN, 'obdII_vesta.csv'),
                       os.path.join(MAIN, 'obdII_vesta_modules.csv')]
OUT = os.path.join(HERE, '..', 'VestaHome', 'VestaHome', 'obd_codes.txt')
# the ECU's own (dealer) list first, then the community one, the rest last
RANK = (('БДФКЭ', 0), ('ЦБКЭ', 0), ('HFM', 0), ('БКЭД', 0), ('АБС', 0), ('Приборка', 0), ('УБКЭ', 0),
        ('ЭРА-ГЛОНАСС', 0), ('М86', 1), ('VestaXR', 2))
TYPE = re.compile(r'^[0-9A-F]{2} - ')     # "11 - text": the failure type byte of a UDS code B1001-11


def clean(f):
    f = f.strip()
    if len(f) > 1 and f[0] == '"' and f[-1] == '"':
        f = f[1:-1]
    return ' '.join(f.replace('""', '"').replace('\xa0', ' ').split())


def rank(ecu):
    for key, r in RANK:
        if key in ecu:
            return r
    return 9


def meaning_key(code, meaning):
    """A code is told apart by its failure type byte (B1001-11) when the table has one, by the text otherwise:
    the same type is written differently by different tables (сиденья/сидения, нагр./нагреватели)."""
    m = TYPE.match(meaning)
    return (code, m.group(0)) if m else (code, '=' + meaning)


def text_key(meaning):
    return ' '.join(meaning.lower().replace('ё', 'е').split())


def main():
    rows = []                                  # (code, rank, seq, rest, meaning)
    for src in SRC:
        if not os.path.exists(src):
            continue
        for line in open(src, encoding='cp1251'):
            f = [clean(x) for x in line.rstrip('\r\n').split(';')]
            if len(f) < 4 or len(f[0]) != 5:
                continue
            while f and not f[-1]:
                f.pop()
            rows.append((f[0].upper(), rank(f[1]), len(rows), ';'.join(f[1:]), ';'.join(f[3:])))
    rows.sort(key=lambda r: r[:3])             # by code, then by the table's rank, then by the file order
    out = []
    seen = set()                               # the code+type already taken (a repeat would only eat a line)
    claimed = set()                            # code+type a unit's own list answers for
    texts = {}                                 # ...and the wordings it already has for them
    for code, rk, _seq, rest, meaning in rows:
        k = meaning_key(code, meaning)
        if code[0] == 'U' and rk == 0:
            # U-codes say 'no link with X', and X is the unit's own business: U1001-87 is 'no frame from the
            # ECM' for БДФКЭ and 'no link with the airbag unit' for the cluster. Different wordings from
            # different units stay side by side, the same wording twice does not.
            t = text_key(meaning)
            if t in texts.get(k, ()):
                continue
            texts.setdefault(k, set()).add(t)
            claimed.add(k)
            out.append((code, rest))
            continue
        if k in claimed or k in seen:          # the unit's list wins over the community text
            continue
        seen.add(k)
        out.append((code, rest))
    with open(OUT, 'w', encoding='utf-8', newline='\r\n') as o:
        o.write('# OBD trouble codes: code;ECU;system;meaning[;more] (tools/make_obd_codes.py)\n')
        for code, rest in out:
            o.write('%s;%s\n' % (code, rest))
    print('%s: %d codes, %d rows' % (os.path.normpath(OUT), len(set(c for c, _ in out)), len(out)))




if __name__ == '__main__':
    main()
