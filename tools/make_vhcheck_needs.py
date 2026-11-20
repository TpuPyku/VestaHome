#!/usr/bin/env python3
"""The vendor functions our code calls by name, for VHCheck (VHCheck/native/needs.inc): it checks on a unit that its
DLLs export them all before VestaHome is moved to the stub alone (the user 2026-11-20: one build for the stock
firmware and the CAN panel's). Scanned: VestaStub, the proxies (SPIPower/native, SPIPower/bt), VestaHome's native
(not the tests). Grouped by the DLL that has them:

    SPApi_*                      SPIPowerMgr.dll        (\\ResidentFlash\\navigation\\app)
    Flc_*                        FLC_BLUETOOTH_SDK.dll  (\\ResidentFlash\\navigation\\app)
    BTDDB_*, BTRF_*, XCBM_*      btapi.dll              (\\ResidentFlash\\dll)
    SV_*                         desay_sdk_api.dll      (\\Windows)

    python make_vhcheck_needs.py        (VHCheck/native/build.bat runs it)
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, '..'))
DIRS = ('VestaStub/native', 'SPIPower/native', 'SPIPower/bt', 'VestaHome/native')
GROUPS = (('kNeedSpm', r'SPApi_\w+'), ('kNeedFlc', r'Flc_\w+'), ('kNeedBtapi', r'(?:BTDDB|BTRF|XCBM)_\w+'),
          ('kNeedSv', r'SV_\w+'))


def names(texts):
    """{group: sorted names} quoted in the texts ("Name")."""
    out = {}
    texts = list(texts)   # (a generator: read once, every group looks through it)
    for group, pat in GROUPS:
        found = set()
        for t in texts:
            found.update(re.findall(r'"(%s)"' % pat, t))
        out[group] = sorted(found)
    return out


def sources(repo):
    for d in DIRS:
        root = os.path.join(repo, d)
        for f in sorted(os.listdir(root)):
            if f.endswith(('.cpp', '.inc', '.h')) and not f.startswith('test_'):
                yield open(os.path.join(root, f), encoding='utf-8-sig', errors='replace').read()


def include(groups):
    lines = ['// needs.inc - made by VestaHome/tools/make_vhcheck_needs.py: the vendor functions our code calls by name.']
    for group, _ in GROUPS:
        lines.append('static const char* const %s[] = {' % group)
        lines += ['    "%s",' % n for n in groups[group]] or ['    "",']
        lines.append('};')
    return '\n'.join(lines) + '\n'


def main(argv=None):
    groups = names(sources(REPO))
    out = os.path.join(REPO, 'VHCheck', 'native', 'needs.inc')
    with open(out, 'w', encoding='utf-8', newline='\r\n') as f:
        f.write(include(groups))
    print('needs.inc: ' + ', '.join('%s %d' % (g, len(groups[g])) for g, _ in GROUPS))
    return 0


if __name__ == '__main__':
    sys.exit(main())
