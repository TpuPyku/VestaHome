#!/usr/bin/env python3
"""Dump printable strings (ASCII and UTF-16LE) from a binary with file offsets.

Usage: python pe_strings.py <file> [keyword ...]

Keywords are matched case-insensitively as substrings; without them every string
of >= MIN_RUN characters is printed. Both encodings are reported because CE
binaries mix them (paths/keys often sit as UTF-16LE).

Why not MSVC-style strings: we need the offsets to line up with PE sections and
with Ghidra addresses (RVA = off - section_raw + section_vaddr).
"""
import re
import sys

MIN_RUN = 4
ASCII_RE = re.compile(rb'[ -~]{%d,}' % MIN_RUN)
UTF16_RE = re.compile(rb'(?:[ -~]\x00){%d,}' % MIN_RUN)


def main():
    path = sys.argv[1]
    kws = [k.lower().encode('latin1') for k in sys.argv[2:]]
    data = open(path, 'rb').read()
    hits = []
    for m in ASCII_RE.finditer(data):
        hits.append((m.start(), m.group().decode('latin1'), 'ascii'))
    for m in UTF16_RE.finditer(data):
        hits.append((m.start(), m.group().decode('utf-16-le'), 'utf16'))
    hits.sort()
    print('%s  (%d bytes, %d strings)' % (path, len(data), len(hits)))
    shown = 0
    for off, s, enc in hits:
        if kws and not any(k.decode('latin1') in s.lower() for k in kws):
            continue
        print('  off=0x%06x %-6s %s' % (off, enc, s))
        shown += 1
    if kws:
        print('--- %d match(es) for %s' % (shown, ' '.join(sys.argv[2:])))


if __name__ == '__main__':
    main()
