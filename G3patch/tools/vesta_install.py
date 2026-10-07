#!/usr/bin/env python3
"""vesta_install -- installer / uninstaller for the VESTA skin on G3NavHMI.exe.

What it does
  install   : rewrite the baked-in skin paths  res/img/Itelma/MMC27/...  ->
              res/img/Itelma/VESTA/...  inside G3NavHMI.exe, and add the VESTA
              skin folder (a clone of MMC27, optionally overlaid with custom art).
  uninstall : rewrite the paths back to MMC27 and delete the VESTA skin folder,
              i.e. everything returns to the original (pre-install) state.

The byte patch is taken from  patches/skin-path-vesta.json  (single source of
truth). It is length-preserving (Itelma/MMC27 -> Itelma/VESTA), so the exe keeps
its exact size and no file offsets move.

Usage
  python tools/vesta_install.py status    --app <APP_DIR>
  python tools/vesta_install.py install   --app <APP_DIR> [--skin <DIR>] [--no-backup]
  python tools/vesta_install.py uninstall --app <APP_DIR>

<APP_DIR> is the folder that holds G3NavHMI.exe and res/img/Itelma/MMC27.
On the HU that is  \\ResidentFlash\\navigation\\app .

Backup: install copies the current exe to  G3NavHMI.exe.vesta.bak  (once). A
pre-existing  G3NavHMI.exe.bak  (the factory file) is left untouched.
"""
import os
import sys
import json
import shutil
import argparse

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import g3patch  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_SPEC = os.path.join(ROOT, 'patches', 'skin-path-vesta.json')
EXE_NAME = 'G3NavHMI.exe'
SKIN_REL = os.path.join('res', 'img', 'Itelma')     # relative to APP_DIR
SRC_SKIN = 'MMC27'                                  # factory folder name
DST_SKIN = 'VESTA'                                  # our folder name
MARKER = '.vesta-skin.json'


def load_patch(spec_path):
    with open(spec_path, 'r', encoding='utf-8') as f:
        spec = json.load(f)
    p = spec['patches'][0]
    old = p['find_all'].encode('ascii')
    new = p['replace'].encode('ascii')
    if len(old) != len(new):
        raise SystemExit('spec error: find_all/replace must be equal length')
    return old, new, p.get('expect')


def check_app(app):
    if not os.path.isdir(app):
        raise SystemExit(f'app dir not found: {app}')
    if not os.path.isfile(exe_path(app)):
        raise SystemExit(f'{EXE_NAME} not found in {app}')


def exe_path(app):
    return os.path.join(app, EXE_NAME)


def skin_dir(app, name):
    return os.path.join(app, SKIN_REL, name)


def read_exe(path):
    with open(path, 'rb') as f:
        return bytearray(f.read())


def write_exe(path, data):
    try:
        with open(path, 'wb') as f:
            f.write(data)
    except PermissionError:
        raise SystemExit(f'cannot write {path} (is {EXE_NAME} running? kill it first)')


def overlay(src, dst):
    """Copy every file under src into dst (creating subdirs), overwriting."""
    n = 0
    for base, _dirs, files in os.walk(src):
        rel = os.path.relpath(base, src)
        target = dst if rel == '.' else os.path.join(dst, rel)
        os.makedirs(target, exist_ok=True)
        for name in files:
            shutil.copy2(os.path.join(base, name), os.path.join(target, name))
            n += 1
    return n


def do_status(a):
    check_app(a.app)
    data = read_exe(exe_path(a.app))
    old_n = len(g3patch.find_hex(data, a.aold))
    new_n = len(g3patch.find_hex(data, a.anew))
    has_vesta = os.path.isdir(skin_dir(a.app, DST_SKIN))
    has_mmc = os.path.isdir(skin_dir(a.app, SRC_SKIN))
    print(f'app       : {a.app}')
    print(f'exe sha256: {g3patch.sha256(data)}')
    print(f'paths     : MMC27={old_n}  VESTA={new_n}')
    print(f'folders   : {SRC_SKIN}={has_mmc}  {DST_SKIN}={has_vesta}')
    if new_n and not old_n and has_vesta:
        print('state     : INSTALLED (VESTA)')
    elif old_n and not new_n:
        print('state     : ORIGINAL (MMC27)')
    else:
        print('state     : MIXED / unknown')
    return 0


def do_install(a):
    check_app(a.app)
    expect = a.expect
    path = exe_path(a.app)
    data = read_exe(path)
    old_n = len(g3patch.find_hex(data, a.aold))
    new_n = len(g3patch.find_hex(data, a.anew))

    if old_n and new_n:
        raise SystemExit(f'error: exe has both MMC27 ({old_n}) and VESTA ({new_n}) paths; '
                         'refusing to patch a mixed binary (run uninstall first)')
    if expect is not None and old_n and old_n != expect:
        raise SystemExit(f'error: expected {expect} MMC27 paths, found {old_n}; '
                         'unknown exe build, aborting')

    if new_n:
        print(f'exe patch : already patched (VESTA x{new_n}); skipping')
    else:
        bak = path + '.vesta.bak'
        if not a.no_backup and not os.path.exists(bak):
            shutil.copy2(path, bak)
            print(f'backup    : {bak}')
        hits = g3patch.find_hex(data, a.aold)
        for h in hits:
            data[h:h + len(a.aold)] = a.anew
        write_exe(path, data)
        print(f'exe patch : {len(hits)} x {a.aold.decode()} -> {a.anew.decode()}')
        print(f'exe sha256: {g3patch.sha256(data)}')

    src = skin_dir(a.app, SRC_SKIN)
    dst = skin_dir(a.app, DST_SKIN)
    if not os.path.isdir(src):
        raise SystemExit(f'error: factory skin folder not found: {src}')
    if not os.path.isdir(dst):
        shutil.copytree(src, dst)
        print(f'skin      : cloned {SRC_SKIN} -> {DST_SKIN}')
    else:
        print(f'skin      : {DST_SKIN} already present')

    if a.skin:
        if not os.path.isdir(a.skin):
            raise SystemExit(f'error: --skin dir not found: {a.skin}')
        n = overlay(a.skin, dst)
        print(f'skin      : overlaid {n} file(s) from {a.skin}')

    with open(os.path.join(dst, MARKER), 'w', encoding='utf-8') as f:
        json.dump({'skin': DST_SKIN, 'source': SRC_SKIN,
                   'exe_sha256': g3patch.sha256(read_exe(path))}, f, indent=2)
    print('install   : DONE')
    return 0


def do_uninstall(a):
    check_app(a.app)
    path = exe_path(a.app)
    data = read_exe(path)
    new_n = len(g3patch.find_hex(data, a.anew))
    if new_n:
        for h in g3patch.find_hex(data, a.anew):
            data[h:h + len(a.anew)] = a.aold
        write_exe(path, data)
        print(f'exe patch : {new_n} x {a.anew.decode()} -> {a.aold.decode()} (reverted)')
        print(f'exe sha256: {g3patch.sha256(data)}')
    else:
        print('exe patch : no VESTA paths found; nothing to revert')

    dst = skin_dir(a.app, DST_SKIN)
    if os.path.isdir(dst):
        shutil.rmtree(dst)
        print(f'skin      : removed {dst}')
    else:
        print(f'skin      : {dst} not present')
    print('uninstall : DONE (returned to original)')
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(prog='vesta_install')
    sub = ap.add_subparsers(dest='cmd', required=True)

    s = sub.add_parser('status'); s.add_argument('--app', required=True)
    s.add_argument('--spec', default=DEFAULT_SPEC); s.set_defaults(fn=do_status)
    s = sub.add_parser('install'); s.add_argument('--app', required=True)
    s.add_argument('--skin', default=None); s.add_argument('--no-backup', action='store_true')
    s.add_argument('--spec', default=DEFAULT_SPEC); s.set_defaults(fn=do_install)
    s = sub.add_parser('uninstall'); s.add_argument('--app', required=True)
    s.add_argument('--spec', default=DEFAULT_SPEC); s.set_defaults(fn=do_uninstall)

    a = ap.parse_args(argv)
    a.aold, a.anew, a.expect = load_patch(a.spec)
    return a.fn(a) or 0


if __name__ == '__main__':
    sys.exit(main())
