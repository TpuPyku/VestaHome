#!/usr/bin/env python3
"""Dump the import table of a PE (dll, ordinal or name, IAT RVA).

Usage: python pe_imports.py <pe> [<pe> ...]
       python pe_imports.py <pe> <dll-name-part>   # only that dll (case-insensitive)
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
    imp_rva = struct.unpack_from('<I', d, dd + 8)[0]
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

    out = []
    o = r2o(imp_rva) if imp_rva else None
    while o:
        oft, _ts, _fc, nrva, ft = struct.unpack_from('<IIIII', d, o)
        if nrva == 0 and oft == 0:
            break
        no = r2o(nrva)
        dll = d[no:d.find(b'\0', no)].decode('latin1')
        thunk = oft or ft
        i = 0
        while True:
            t = struct.unpack_from('<I', d, r2o(thunk) + i * 4)[0]
            if t == 0:
                break
            if t & 0x80000000:
                out.append((dll, t & 0xffff, None, ft + i * 4))
            else:
                nn = r2o(t) + 2
                out.append((dll, None, d[nn:d.find(b'\0', nn)].decode('latin1'), ft + i * 4))
            i += 1
        o += 20
    return ib, out


if __name__ == '__main__':
    args = sys.argv[1:]
    filt = args[-1].lower() if len(args) > 1 and not args[-1].lower().endswith(('.exe', '.dll')) else None
    for p in (args[:-1] if filt else args):
        ib, im = parse(p)
        print('==== %s  (imagebase 0x%X, %d imports)' % (p, ib, len(im)))
        for dll, ordv, nm, iat in im:
            if filt and filt not in dll.lower():
                continue
            print('  %-16s %-6s %-40s IAT 0x%08X' % (dll, ('#%d' % ordv) if ordv else '', nm or '', ib + iat))
