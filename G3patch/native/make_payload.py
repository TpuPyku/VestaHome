#!/usr/bin/env python3
"""make_payload.py - pack VestaLaunch.exe, its Skin folder and skin-vesta into payload.bin (embedded by VestaInstall).

Format:  [u32 count][ per file: u16 nameLen | name UTF-8 | u32 dataLen | data ]
Names use forward slashes. "VestaLaunch.exe" and "VestaLaunch/..." go to the launcher folder on \\SDMMC;
all other names are laid out over the VESTA skin folder (Temp/main/... etc.).
"""
import os
import struct
import sys

ROOT = os.path.dirname(os.path.abspath(__file__))  # G3patch/native
LAUNCH = os.path.join(ROOT, "..", "..", "VestaLaunch", "VestaLaunch", "VestaLaunch.exe")
LAUNCH_SKIN = os.path.join(os.path.dirname(LAUNCH), "Skin")
SKIN = os.path.join(ROOT, "..", "skin-vesta")


def rel_files(base):
    out = []
    for dp, _dn, fn in os.walk(base):
        for f in fn:
            full = os.path.join(dp, f)
            rel = os.path.relpath(full, base).replace("\\", "/")
            out.append((rel, full))
    return sorted(out)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "payload.bin")
    entries = [("VestaLaunch.exe", LAUNCH)]
    entries += [("VestaLaunch/Skin/" + n, p) for n, p in rel_files(LAUNCH_SKIN)]
    if os.path.isdir(SKIN):
        entries += rel_files(SKIN)
    entries = [(n, p) for n, p in entries if os.path.isfile(p)]

    with open(out, "wb") as fo:
        fo.write(struct.pack("<I", len(entries)))
        for name, path in entries:
            data = open(path, "rb").read()
            nb = name.encode("utf-8")
            fo.write(struct.pack("<H", len(nb)))
            fo.write(nb)
            fo.write(struct.pack("<I", len(data)))
            fo.write(data)
    print("payload.bin: %d files -> %s" % (len(entries), out))


if __name__ == "__main__":
    main()
