"""nkdiff.py - the module and file lists of a Windows CE B000FF image (NK.bin / TINYNK.bin), and the diff of two.
Usage: python nkdiff.py list <img>
       python nkdiff.py diff <stock img> <other img>
       python nkdiff.py extract <img> <out dir> <file name> [...]   (names in lower case, as "list" prints them)"""
import datetime
import os
import struct
import sys


def load(path):
    """B000FF records -> {address: bytes}; with a reader of any virtual address."""
    b = open(path, 'rb').read()
    assert b[:7] == b'B000FF\n', 'not a B000FF image'
    start, length = struct.unpack_from('<II', b, 7)
    recs, o = [], 15
    while o + 12 <= len(b):
        addr, ln, chk = struct.unpack_from('<III', b, o)
        o += 12
        if addr == 0:
            break
        recs.append((addr, b[o:o + ln]))
        o += ln
    recs.sort()
    return start, recs


class Image:
    def __init__(self, path):
        self.start, self.recs = load(path)

    def read(self, va, n):
        for a, d in self.recs:
            if a <= va < a + len(d):
                off = va - a
                if off + n <= len(d):
                    return d[off:off + n]
                return d[off:] + self.read(a + len(d), n - (len(d) - off))
        raise KeyError(hex(va))

    def u32(self, va):
        return struct.unpack('<I', self.read(va, 4))[0]

    def cstr(self, va):
        out = b''
        while True:
            c = self.read(va + len(out), 1)
            if c == b'\0' or len(out) > 260:
                return out.decode('latin-1')
            out += c

    def romhdr(self):
        for a, d in self.recs:   # 'CECE' at +0x40 of the image start, then the ROMHDR pointer
            i = d.find(b'ECEC')
            if i >= 0 and i + 8 <= len(d):
                va = a + i
                p = struct.unpack_from('<I', d, i + 4)[0]
                try:
                    self.read(p, 0x54)
                    return p
                except KeyError:
                    continue
        raise SystemExit('no ROMHDR')

    def toc(self):
        h = self.romhdr()
        f = struct.unpack('<5I8I', self.read(h, 13 * 4))
        nummods, numfiles = f[4], f[12]
        mods, files = {}, {}
        o = h + 0x54
        for _ in range(nummods):
            attrs, tlo, thi, size, name, e32, o32, load = struct.unpack('<8I', self.read(o, 32))
            mods[self.cstr(name).lower()] = (size, (thi << 32) | tlo, attrs)
            o += 32
        for _ in range(numfiles):
            attrs, tlo, thi, real, comp, name, load = struct.unpack('<7I', self.read(o, 28))
            files[self.cstr(name).lower()] = (real, (thi << 32) | tlo, attrs, comp, load)
            o += 28
        return h, mods, files


def when(ft):
    try:
        return (datetime.datetime(1601, 1, 1) + datetime.timedelta(microseconds=ft // 10)).strftime('%Y-%m-%d %H:%M')
    except OverflowError:
        return '?'


def content(img, name, entry):   # a file's stored bytes (compressed as stored), to compare two images
    real, ft, attrs, comp, load = entry
    return img.read(load, comp)


def main(a):
    if a[1] == 'extract':
        return extract(a[2], a[4:], a[3])
    if a[1] == 'list':
        img = Image(a[2])
        h, mods, files = img.toc()
        print('ROMHDR', hex(h), len(mods), 'modules', len(files), 'files')
        for n, v in sorted(mods.items()):
            print('M %-32s %9d %s' % (n, v[0], when(v[1])))
        for n, v in sorted(files.items()):
            print('F %-32s %9d %s' % (n, v[0], when(v[1])))
        return
    A, B = Image(a[2]), Image(a[3])
    _, ma, fa = A.toc()
    _, mb, fb = B.toc()
    print('modules %d / %d, files %d / %d' % (len(ma), len(mb), len(fa), len(fb)))
    for kind, x, y in (('M', ma, mb), ('F', fa, fb)):
        for n in sorted(set(x) | set(y)):
            if n not in y:
                print('%s - only stock   %-30s %9d %s' % (kind, n, x[n][0], when(x[n][1])))
            elif n not in x:
                print('%s + only other   %-30s %9d %s' % (kind, n, y[n][0], when(y[n][1])))
            elif x[n][0] != y[n][0] or x[n][1] != y[n][1]:
                print('%s ~ changed      %-30s %9d -> %9d  %s -> %s' % (kind, n, x[n][0], y[n][0], when(x[n][1]),
                                                                       when(y[n][1])))
            elif kind == 'F':
                try:
                    if content(A, n, x[n]) != content(B, n, y[n]):
                        print('F ~ same size/time, other bytes  %s' % n)
                except KeyError:
                    pass



def extract(img_path, names, out):   # files as stored ('.stored' = ROM-compressed: unpack with cedec.py)
    img = Image(img_path)
    _, mods, files = img.toc()
    for n in names:
        if n in files:
            real, ft, attrs, comp, load = files[n]
            data = img.read(load, comp)
            print(n, 'attrs %#x real %d stored %d' % (attrs, real, comp))
            open(os.path.join(out, n + ('.stored' if comp != real else '')), 'wb').write(data)
        elif n in mods:
            print(n, 'is a module (XIP sections, not extracted)')


if __name__ == '__main__':
    main(sys.argv)
