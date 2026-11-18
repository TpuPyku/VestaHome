"""cedec.py - unpacks files that nkdiff.py extract left as '.stored' (Windows CE ROM compression: LZX in 4 KB blocks).
The decompressor itself is third-party (github.com/KodaSec/wince-decompr, wincedecompr.py; kept beside the study
material, not in this repo) - its path is the first argument. Checked 2026-11-18: 4120's main.exe came out byte for
byte as the known unpacked one.
Usage: python -I cedec.py <wincedecompr.py> <file.stored> <real size> [<file.stored> <real size> ...]"""
import importlib.util
import os
import sys

spec = importlib.util.spec_from_file_location('wcd', sys.argv[1])
wcd = importlib.util.module_from_spec(spec)
spec.loader.exec_module(wcd)
for path, real in zip(sys.argv[2::2], sys.argv[3::2]):
    real = int(real)
    buf = open(path, 'rb').read()
    out = bytearray(real + 4096)
    n = wcd.CEDecompressROM(buf, len(buf), out, real, 0, 1, 4096)
    dst = path[:-7] if path.endswith('.stored') else path + '.out'
    open(dst, 'wb').write(out[:n])
    print(os.path.basename(dst), n)
