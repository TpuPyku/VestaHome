#!/usr/bin/env python3
"""Dump the export table of a PE (ordinal, RVA, name).

Usage: python pe_exports.py <pe> [<pe> ...]
"""
import sys, struct


def parse(path):
    d = open(path, 'rb').read()
    if d[:2] != b'MZ':
        raise SystemExit('not a PE: ' + path)
    e = struct.unpack_from('<I', d, 0x3c)[0]
    if d[e:e + 4] != b'PE\0\0':
        raise SystemExit('no PE signature: ' + path)
    coff = e + 4
    nsec = struct.unpack_from('<H', d, coff + 2)[0]
    szopt = struct.unpack_from('<H', d, coff + 16)[0]
    opt = coff + 20
    magic = struct.unpack_from('<H', d, opt)[0]
    dd = opt + (96 if magic == 0x10b else 112)
    ib = struct.unpack_from('<I', d, opt + 28)[0]
    exp_rva = struct.unpack_from('<I', d, dd)[0]
    if exp_rva == 0:
        return ib, []
    so = opt + szopt
    secs = []
    for i in range(nsec):
        o = so + i * 40
        vsz = struct.unpack_from('<I', d, o + 8)[0]
        va = struct.unpack_from('<I', d, o + 12)[0]
        rsz = struct.unpack_from('<I', d, o + 16)[0]
        rp = struct.unpack_from('<I', d, o + 20)[0]
        secs.append((va, max(vsz, rsz), rp))

    def r2o(rva):
        for va, sz, rp in secs:
            if va <= rva < va + sz:
                return rp + (rva - va)
        return None

    o = r2o(exp_rva)
    base = struct.unpack_from('<I', d, o + 16)[0]
    nn = struct.unpack_from('<I', d, o + 24)[0]
    af = struct.unpack_from('<I', d, o + 28)[0]
    an = struct.unpack_from('<I', d, o + 32)[0]
    ao = struct.unpack_from('<I', d, o + 36)[0]
    out = []
    for i in range(nn):
        nr = struct.unpack_from('<I', d, r2o(an) + i * 4)[0]
        ordw = struct.unpack_from('<H', d, r2o(ao) + i * 2)[0]
        no = r2o(nr)
        nm = d[no:d.find(b'\0', no)].decode('latin1')
        frva = struct.unpack_from('<I', d, r2o(af) + ordw * 4)[0]
        out.append((base + ordw, nm, frva))
    out.sort()
    return ib, out


if __name__ == '__main__':
    for p in sys.argv[1:]:
        ib, ex = parse(p)
        print('==== %s  (imagebase 0x%X, %d exports)' % (p, ib, len(ex)))
        for ordv, nm, rva in ex:
            print('%4d  0x%08X  %s' % (ordv, ib + rva, nm))
