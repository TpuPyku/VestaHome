#!/usr/bin/env python3
"""check_fingerprints.py - before the installer is built (build.bat): the proxies it packs (..\\out) must be the ones
fingerprints.h names, or the installer refuses to put them in (2026-11-08: the Bluetooth proxy was rebuilt on 11-06
without gen_fingerprints.py - no installer since then put it on the unit). Exit 1 with what to run."""
import hashlib
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PAIRS = (('kProxyMd5', 'SPIPowerMgr.dll'), ('kBtProxyMd5', 'FLC_BLUETOOTH_SDK.dll'))


def main():
    text = open(os.path.join(HERE, 'fingerprints.h'), encoding='utf-8').read()
    bad = []
    for key, name in PAIRS:
        m = re.search(r'%s = "([0-9a-f]{32})"' % key, text)
        path = os.path.join(HERE, '..', 'out', name)
        got = hashlib.md5(open(path, 'rb').read()).hexdigest() if os.path.exists(path) else None
        if not m or got != m.group(1):
            bad.append('%s: fingerprints.h %s, out %s' % (name, m.group(1) if m else 'none', got or 'missing'))
    if bad:
        print('\n'.join(bad))
        print('=== run: python VestaHome\\SPIPower\\tools\\gen_fingerprints.py, then this build again')
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
