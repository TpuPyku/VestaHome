#!/usr/bin/env python3
"""make_obd_codes.py - the OBD trouble codes' meanings for VestaHome ("Car -> Ошибки (OBD)", a tap on a code).

Reads the table the user gave (cp1251, ';'-separated: code;ECU;system;meaning[;more]; by default
MAIN/obdII.csv next to the repos) and writes VestaHome/VestaHome/obd_codes.txt (UTF-8, one code a line, the same
fields; a code may have several lines - for different ECUs), which VestaHomeInstall carries next to the exe.

Usage: python VestaHome/tools/make_obd_codes.py [table.csv]
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', '..', 'MAIN', 'obdII.csv')
OUT = os.path.join(HERE, '..', 'VestaHome', 'VestaHome', 'obd_codes.txt')


def clean(f):
    f = f.strip()
    if len(f) > 1 and f[0] == '"' and f[-1] == '"':
        f = f[1:-1]
    return ' '.join(f.replace('""', '"').replace('\xa0', ' ').split())


def main():
    rows = []
    for line in open(SRC, encoding='cp1251'):
        f = [clean(x) for x in line.rstrip('\r\n').split(';')]
        if len(f) < 4 or len(f[0]) != 5:
            continue
        while f and not f[-1]:
            f.pop()
        rows.append((f[0].upper(), ';'.join(f[1:])))
    rows.sort(key=lambda r: r[0])
    with open(OUT, 'w', encoding='utf-8', newline='\r\n') as o:
        o.write('# OBD trouble codes: code;ECU;system;meaning[;more] (tools/make_obd_codes.py)\n')
        for code, rest in rows:
            o.write('%s;%s\n' % (code, rest))
    print('%s: %d codes' % (os.path.normpath(OUT), len(rows)))


if __name__ == '__main__':
    main()
