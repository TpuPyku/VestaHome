#!/usr/bin/env python3
"""check_fingerprints.py - before the installer is built (build.bat): the proxies it packs (..\\out) must be the ones
fingerprints.h names, or the installer refuses to put them in (2026-11-08: the Bluetooth proxy was rebuilt on 11-06
without gen_fingerprints.py - no installer since then put it on the unit). Exit 1 with what to run."""
import hashlib
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PAIRS = (('kProxyMd5', 'SPIPowerMgr.dll'), ('kBtProxyMd5', 'FLC_BLUETOOTH_SDK.dll'))


def lost_proxy(committed, current, install_cpp):
    """The MCU proxy rebuilt (even byte-identical sources: the PE time stamp changes it): the one on the units - the
    committed fingerprint - must be in the installer's kOldProxyMd5, or the update refuses it as a foreign DLL (HU
    2026-11-14-2). None = fine, else what to do."""
    if not committed or committed == current or '"%s"' % committed in install_cpp:
        return None
    return ('SPIPowerMgr.dll changed: the installed %s is not in kOldProxyMd5 (spipower_install.cpp) - add it there, '
            'or restore out\\SPIPowerMgr.dll from git if its sources did not change' % committed)


def committed_md5(key):
    try:
        text = subprocess.check_output(['git', 'show', 'HEAD:SPIPower/install/fingerprints.h'], cwd=HERE,
                                       stderr=subprocess.DEVNULL).decode('utf-8')
    except (OSError, subprocess.CalledProcessError):
        return None
    m = re.search(r'%s = "([0-9a-f]{32})"' % key, text)
    return m.group(1) if m else None


def main():
    text = open(os.path.join(HERE, 'fingerprints.h'), encoding='utf-8').read()
    bad = []
    for key, name in PAIRS:
        m = re.search(r'%s = "([0-9a-f]{32})"' % key, text)
        path = os.path.join(HERE, '..', 'out', name)
        got = hashlib.md5(open(path, 'rb').read()).hexdigest() if os.path.exists(path) else None
        if not m or got != m.group(1):
            bad.append('%s: fingerprints.h %s, out %s' % (name, m.group(1) if m else 'none', got or 'missing'))
    m = re.search(r'kProxyMd5 = "([0-9a-f]{32})"', text)
    lost = lost_proxy(committed_md5('kProxyMd5'), m.group(1) if m else None,
                      open(os.path.join(HERE, 'spipower_install.cpp'), encoding='utf-8-sig').read())
    if lost:
        print(lost)
        return 1
    if bad:
        print('\n'.join(bad))
        print('=== run: python VestaHome\\SPIPower\\tools\\gen_fingerprints.py, then this build again')
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
