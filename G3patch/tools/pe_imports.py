#!/usr/bin/env python3
"""List the DLLs a PE file imports (handy for checking WinCE exe dependencies)."""
import sys
import struct


def imports(path):
    d = open(path, 'rb').read()
    e = struct.unpack_from('<I', d, 0x3c)[0]
    nsec = struct.unpack_from('<H', d, e + 6)[0]
    szopt = struct.unpack_from('<H', d, e + 20)[0]
    opt = e + 24
    magic = struct.unpack_from('<H', d, opt)[0]
    dd = opt + (96 if magic == 0x10b else 112)
    imp_rva = struct.unpack_from('<I', d, dd + 8)[0]
    secs = []
    so = opt + szopt
    for i in range(nsec):
        o = so + i * 40
        vsz = struct.unpack_from('<I', d, o + 8)[0]
        va = struct.unpack_from('<I', d, o + 12)[0]
        rsz = struct.unpack_from('<I', d, o + 16)[0]
        rptr = struct.unpack_from('<I', d, o + 20)[0]
        secs.append((va, max(vsz, rsz), rptr))

    def r2o(rva):
        for va, sz, rptr in secs:
            if va <= rva < va + sz:
                return rptr + (rva - va)
        return None

    off = r2o(imp_rva)
    out = []
    if off is None:
        return out
    i = 0
    while True:
        ent = d[off + i * 20:off + i * 20 + 20]
        if len(ent) < 20 or ent == b'\x00' * 20:
            break
        namerva = struct.unpack_from('<I', ent, 12)[0]
        if namerva == 0:
            break
        no = r2o(namerva)
        out.append(d[no:d.find(b'\x00', no)].decode('latin1'))
        i += 1
    return out


for p in sys.argv[1:]:
    print(p)
    for n in imports(p):
        print('   ', n)
