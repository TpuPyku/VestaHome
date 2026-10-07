#!/usr/bin/env python3
"""vestahome_nav.py - put VestaHome on the shell's "Navigation" button, like AppLauncher.

Reproduces what AppLauncher does on install (see ../APPLAUNCHER-INSTALL.md), but
points at VestaHome, and additionally overlays the VESTA main-menu skin:

  1. G3NavHMI.exe : 127.0.0.1 -> 127.0.0.2 (backup <name>.vestahome.bak)
  2. Config/ExternNavi.cfg : first entry = the VestaHome exe (backup ...vestahome.bak)
  3. res/img/Itelma/MMC27/<rel> : every picture from the VESTA skin folder
     (skin/make_skin.py output, default G3patch/skin-vesta) overlays the
     stock MMC27 pictures (each stock file backed up ...vestahome.bak).

uninstall reverses all three; status prints the current state. All of it is
file-based and reversible.

    python tools/vestahome_nav.py status    --app <APP_DIR>
    python tools/vestahome_nav.py install   --app <APP_DIR>
        [--target \\SDMMC\\VestaHome\\VestaHome.exe] [--art <skin_dir>]
    python tools/vestahome_nav.py uninstall --app <APP_DIR>
"""
import argparse
import os
import shutil

EXE = "G3NavHMI.exe"
CFG = os.path.join("Config", "ExternNavi.cfg")
MAINREL = os.path.join("res", "img", "Itelma", "MMC27")
BAK = ".vestahome.bak"
LOOPBACK_OLD = b"127.0.0.1"
LOOPBACK_NEW = b"127.0.0.2"
DEFAULT_TARGET = "\\SDMMC\\VestaHome\\VestaHome.exe"
G3PATCH = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def read(path):
    with open(path, "rb") as f:
        return f.read()


def write(path, data):
    with open(path, "wb") as f:
        f.write(data)


def backup(path):
    """Keep the pre-install copy once, so a second install never clobbers it."""
    b = path + BAK
    if not os.path.exists(b):
        shutil.copy2(path, b)
    return b


# ---------------------------------------------------------------- exe
def exe_install(app):
    p = os.path.join(app, EXE)
    if not os.path.exists(p):
        raise SystemExit("missing file: " + p)
    data = read(p)
    if LOOPBACK_NEW in data:
        print("  exe: already patched (127.0.0.2) - skipped")
        return False
    if data.count(LOOPBACK_OLD) != 1:
        raise SystemExit("expected 1 x 127.0.0.1, found %d" % data.count(LOOPBACK_OLD))
    backup(p)
    write(p, data.replace(LOOPBACK_OLD, LOOPBACK_NEW))
    print("  exe: 127.0.0.1 -> 127.0.0.2")
    return True


def exe_uninstall(app):
    p = os.path.join(app, EXE)
    if os.path.exists(p + BAK):
        shutil.copy2(p + BAK, p)
        os.remove(p + BAK)
        print("  exe: restored from .vestahome.bak")
        return True
    if os.path.exists(p):
        data = read(p)
        if LOOPBACK_NEW in data:
            write(p, data.replace(LOOPBACK_NEW, LOOPBACK_OLD))
            print("  exe: 127.0.0.2 -> 127.0.0.1")
            return True
    print("  exe: nothing to undo")
    return False


# ---------------------------------------------------------------- ExternNavi.cfg
def cfg_install(app, target):
    p = os.path.join(app, CFG)
    if not os.path.exists(p):
        raise SystemExit("missing file: " + p)
    lines = [l for l in read(p).decode("latin-1").splitlines() if l.strip() and l.strip() != target]
    backup(p)
    write(p, (target + "\r\n" + "".join(l + "\r\n" for l in lines)).encode("latin-1"))
    print("  cfg: first entry " + target)


def cfg_uninstall(app):
    p = os.path.join(app, CFG)
    if os.path.exists(p + BAK):
        shutil.copy2(p + BAK, p)
        os.remove(p + BAK)
        print("  cfg: restored from .vestahome.bak")
        return True
    print("  cfg: nothing to undo")
    return False


# ---------------------------------------------------------------- skin pictures
def art_files(art):
    for root, _dirs, files in os.walk(art):
        for f in files:
            src = os.path.join(root, f)
            yield src, os.path.relpath(src, art)


def art_install(app, art):
    if not os.path.isdir(art):
        raise SystemExit("missing skin folder: " + art)
    n = 0
    for src, rel in art_files(art):
        dst = os.path.join(app, MAINREL, rel)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        if os.path.exists(dst):
            backup(dst)
        shutil.copy2(src, dst)
        n += 1
    print("  skin: %d pictures -> %s" % (n, MAINREL))


def art_uninstall(app):
    n = 0
    for root, _dirs, files in os.walk(os.path.join(app, MAINREL)):
        for f in files:
            if f.endswith(BAK):
                orig = os.path.join(root, f[:-len(BAK)])
                shutil.copy2(os.path.join(root, f), orig)
                os.remove(os.path.join(root, f))
                n += 1
    print("  skin: restored %d files" % n)
    return n


# ---------------------------------------------------------------- status
def status(app):
    p = os.path.join(app, EXE)
    if os.path.exists(p):
        patched = LOOPBACK_NEW in read(p)
        print("  exe:       present, loopback %s" % ("127.0.0.2 (patched)" if patched else "127.0.0.1 (stock)"))
    else:
        print("  exe:       missing")
    cp = os.path.join(app, CFG)
    if os.path.exists(cp):
        print("  ExternNavi: %s" % read(cp).decode("latin-1").splitlines()[0])
    else:
        print("  ExternNavi: missing")
    n = 0
    for root, _dirs, files in os.walk(os.path.join(app, MAINREL)):
        n += sum(1 for f in files if f.endswith(BAK))
    print("  skin:      %s" % ("installed (%d files in .vestahome.bak)" % n if n else "stock"))


def main():
    ap = argparse.ArgumentParser(description="VestaHome on the shell's Navigation button (AppLauncher-style).")
    ap.add_argument("cmd", choices=["install", "uninstall", "status"])
    ap.add_argument("--app", required=True, help="shell folder, e.g. \\ResidentFlash\\navigation\\app")
    ap.add_argument("--target", default=DEFAULT_TARGET, help="VestaHome exe (first ExternNavi.cfg entry)")
    ap.add_argument("--art", default=os.path.join(G3PATCH, "skin-vesta"), help="VESTA skin folder")
    a = ap.parse_args()

    print("APP:", a.app)
    if a.cmd == "status":
        status(a.app)
    elif a.cmd == "install":
        exe_install(a.app)
        cfg_install(a.app, a.target)
        art_install(a.app, a.art)
        print("done. Rollback: uninstall")
    else:
        art_uninstall(a.app)
        cfg_uninstall(a.app)
        exe_uninstall(a.app)
        print("removed")


if __name__ == "__main__":
    main()

