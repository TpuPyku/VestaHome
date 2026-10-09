#!/usr/bin/env python3
"""Assemble a self-contained SPIPower_test folder for the Level 0 run.

The harness (test/test.cpp) loads the proxy from \\SDMMC\\SPIPower_test\\ or
\\StaticStore\\SPIPower_test\\; the proxy in turn loads SPIPowerMgr_orig.dll beside
itself, and that factory DLL imports desay_sdk_api.dll (a \\Windows ROM module on
the HU) and NCL_BaseCom.dll (the app folder). Both are copied into the folder so
it also loads in the CE6 emulator, whose image has neither.

Usage: python make_test_dir.py [outdir]
Default outdir: MAIN/WinCE_Emulator_StaticStore+SDMMC/StaticStore/SPIPower_test
"""
import hashlib
import os
import shutil
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
APP = os.path.join(ROOT, 'MAIN', 'ResidentFlash', 'navigation', 'app')
OUT = os.path.join(ROOT, 'Vesta', 'SPIPower', 'out')

FACTORY_MD5 = '438505b76467ee393156f1aaacb9e3ab'
PROXY_MD5 = 'fa7b65e000c6f33bbd2f8be18fe85808'

# (source, name in the test folder)
FILES = (
    (os.path.join(OUT, 'SPIPowerMgr.dll'),   'SPIPowerMgr.dll'),        # our proxy
    (os.path.join(OUT, 'SPIPowerTest.exe'),  'SPIPowerTest.exe'),       # harness (rebuild first)
    (os.path.join(APP, 'SPIPowerMgr.dll'),   'SPIPowerMgr_orig.dll'),   # factory DLL = rollback
    (os.path.join(APP, 'NCL_BaseCom.dll'),   'NCL_BaseCom.dll'),        # factory dependency
    (os.path.join(ROOT, 'MAIN', '4120_unpacked', 'NK', 'fs', 'Windows',
                  'desay_sdk_api.dll'),      'desay_sdk_api.dll'),      # factory dependency
)

DEFAULT_OUT = os.path.join(ROOT, 'MAIN', 'WinCE_Emulator_StaticStore+SDMMC',
                           'StaticStore', 'SPIPower_test')


def md5(path):
    h = hashlib.md5()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 16), b''):
            h.update(chunk)
    return h.hexdigest()


def main(outdir):
    if not os.path.isdir(outdir):
        os.makedirs(outdir)
    for src, dst in FILES:
        if not os.path.exists(src):
            raise SystemExit('missing source: ' + src)
        shutil.copyfile(src, os.path.join(outdir, dst))
        print('  %-22s %8d  md5=%s  <- %s'
              % (dst, os.path.getsize(src), md5(src), os.path.relpath(src, ROOT)))

    # guard: the copy named _orig must be the factory DLL and the proxy must be ours
    h_proxy = md5(os.path.join(outdir, 'SPIPowerMgr.dll'))
    h_orig = md5(os.path.join(outdir, 'SPIPowerMgr_orig.dll'))
    if h_proxy != PROXY_MD5:
        raise SystemExit('SPIPowerMgr.dll in outdir is not the proxy (md5 %s)' % h_proxy)
    if h_orig != FACTORY_MD5:
        raise SystemExit('SPIPowerMgr_orig.dll in outdir is not the factory DLL (md5 %s)' % h_orig)
    print('done: %s\n  proxy=%s factory=%s' % (outdir, h_proxy[:8], h_orig[:8]))


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else DEFAULT_OUT)
