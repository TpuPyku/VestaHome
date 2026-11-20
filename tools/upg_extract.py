#!/usr/bin/env python3
"""upg_extract.py - the PE modules inside a firmware's *.upg package (SwDnl.upg: the \\ResidentFlash part) as files,
for VHCheckPC to scan (the user 2026-11-20: any firmware of the unit; 4125's \\ResidentFlash comes only as this).
The package is a flat image: each module a whole PE file somewhere in it, no file names. A DLL is named by its
export directory's own name (FLC_BLUETOOTH_SDK.dll); a module without one - mod_<offset>.exe / .dll.

    python upg_extract.py <package.upg> <out folder>
"""
import os
import struct
import sys


def u16(d, o):
    return struct.unpack_from('<H', d, o)[0] if 0 <= o <= len(d) - 2 else 0


def u32(d, o):
    return struct.unpack_from('<I', d, o)[0] if 0 <= o <= len(d) - 4 else 0


def module(d, i):
    """(end, is_dll, export name or '') of the PE file at offset i, None when there is none."""
    e = u32(d, i + 0x3C)
    if not 0 < e < 0x1000 or d[i + e:i + e + 4] != b'PE\0\0':
        return None
    nsec, optsz, flags = u16(d, i + e + 6), u16(d, i + e + 20), u16(d, i + e + 22)
    opt, secs, end = i + e + 24, [], 0
    for k in range(nsec):
        s = opt + optsz + 40 * k
        va, raw, ptr = u32(d, s + 12), u32(d, s + 16), u32(d, s + 20)
        secs.append((va, raw, ptr))
        end = max(end, ptr + raw)
    if not end:
        return None

    def off(rva):
        for va, raw, ptr in secs:
            if va <= rva < va + raw:
                return i + ptr + rva - va
        return -1
    name = ''
    exp = off(u32(d, opt + 96)) if u32(d, opt + 92) else -1
    if exp >= 0:
        n = off(u32(d, exp + 12))
        if n >= 0:
            name = d[n:d.find(b'\0', n, n + 64)].decode('latin1')
    return i + end, bool(flags & 0x2000), name


def modules(d):
    """[(offset, end, is_dll, name)] of every PE file in the image, in order."""
    out, i = [], 0
    while True:
        i = d.find(b'MZ', i)
        if i < 0:
            return out
        m = module(d, i)
        if m:
            out.append((i, m[0], m[1], m[2]))
            i = m[0]
        else:
            i += 2


def file_name(off, is_dll, name):
    return name if name else 'mod_%08X.%s' % (off, 'dll' if is_dll else 'exe')


def main(argv=None):
    a = argv if argv is not None else sys.argv[1:]
    d = open(a[0], 'rb').read()
    os.makedirs(a[1], exist_ok=True)
    mods = modules(d)
    for off, end, is_dll, name in mods:
        with open(os.path.join(a[1], file_name(off, is_dll, name)), 'wb') as f:
            f.write(d[off:end])
    print('%s: modules %d -> %s' % (a[0], len(mods), a[1]))
    return 0


if __name__ == '__main__':
    sys.exit(main())
