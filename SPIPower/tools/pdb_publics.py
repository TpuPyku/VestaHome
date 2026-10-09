#!/usr/bin/env python3
"""Extract public symbols (S_PUB32) from a MSVC PDB 7.0 (MSF) file.

Usage: python pdb_publics.py <file.pdb> [keyword ...]
Prints name, section, and section-relative offset for each public symbol.
If keywords are given, only prints symbols whose name contains one of them.

For SPIPowerMgr.dll: section 1 = .text (base RVA 0x1000), so a function's
RVA = 0x1000 + off.
"""
import sys, struct


def parse_msf(path):
    d = open(path, 'rb').read()
    assert d[:32] == b'Microsoft C/C++ MSF 7.00\r\n\x1aDS\x00\x00\x00', 'not MSF 7.0'
    BS = struct.unpack_from('<I', d, 32)[0]
    nb = struct.unpack_from('<I', d, 40)[0]
    ndb = struct.unpack_from('<I', d, 44)[0]
    bma = struct.unpack_from('<I', d, 52)[0]
    bmap = [struct.unpack_from('<I', d, bma * BS + i * 4)[0] for i in range(nb)]
    nblk = (ndb + BS - 1) // BS
    dirdata = b''.join(d[bmap[i] * BS:bmap[i] * BS + BS] for i in range(nblk))
    ns = struct.unpack_from('<I', dirdata, 0)[0]
    sizes = [struct.unpack_from('<I', dirdata, 4 + i * 4)[0] for i in range(ns)]
    o = 4 + ns * 4
    streams = []
    for sz in sizes:
        n = (sz + BS - 1) // BS
        blks = [struct.unpack_from('<I', dirdata, o + i * 4)[0] for i in range(n)]
        o += n * 4
        streams.append(blks)

    def sd(i):
        return b''.join(d[b * BS:b * BS + BS] for b in streams[i])[:sizes[i]]

    return sd


def publics(path):
    sd = parse_msf(path)
    dbi = sd(3)  # DBI stream
    symrec = struct.unpack_from('<H', dbi, 0x14)[0]  # SymRecordStream
    data = sd(symrec)
    recs = []
    i = 0
    while i < len(data) - 14:
        reclen = struct.unpack_from('<H', data, i)[0]
        rectype = struct.unpack_from('<H', data, i + 2)[0]
        if rectype == 0x110e and 14 <= reclen <= 512:  # S_PUB32
            off = struct.unpack_from('<I', data, i + 8)[0]
            seg = struct.unpack_from('<H', data, i + 12)[0]
            nm = data[i + 14:i + reclen].split(b'\0')[0].decode('latin1', 'replace')
            recs.append((nm, seg, off))
            i += reclen
        else:
            i += 1
    return recs


if __name__ == '__main__':
    path = sys.argv[1]
    kws = sys.argv[2:]
    for nm, seg, off in sorted(publics(path)):
        if not kws or any(k in nm for k in kws):
            print('%-52s seg=%d off=0x%X' % (nm, seg, off))
