# gen_fingerprints.py - generate install/fingerprints.h: the firmware fingerprint table
# the installer checks before it swaps SPIPowerMgr.dll.
#
# Why: our proxy is rebuilt for the exact bytes of one firmware build. "4120" ships
# SPIPowerMgr.dll = 111104 B / 438505b7... but other builds carry a same-size, different
# DLL (the CE emulator tree here holds 2e1351...), and swapping the wrong one kills the
# shell. The table is therefore derived from the OFFICIAL 4120 package
# (MAIN/4120/SwDnl.upg, TOC 0x03276D3F - same value make_rescue.py uses) plus the 4120
# NK image for the OS-side modules, and cross-checked against our dumps.
#
# Usage (from Vesta/SPIPower):
#   python tools\gen_fingerprints.py                  -> write/report, cross-check dumps
#   python tools\gen_fingerprints.py --check          -> fail if the header on disk differs
#   python tools\gen_fingerprints.py --simulate \ResidentFlash=..\..\MAIN\ResidentFlash
#
# --simulate mirrors the installer's verdict on the PC: it reads the GENERATED header,
# hashes the given mount trees and prints the same ok/SOFT/HARD lines InstallCheck()
# would print, so the gate can be tested without a head unit.
import hashlib
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import upg_build as ub

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
FW_NAME = '4120'
PKG_PATH = os.path.join(ROOT, 'MAIN', '4120', 'SwDnl.upg')
PKG_TOC = 0x03276D3F
NK_FS = os.path.join(ROOT, 'MAIN', '4120_unpacked', 'NK', 'fs')
OUT_H = os.path.join(ROOT, 'Vesta', 'SPIPower', 'install', 'fingerprints.h')
PROXY = os.path.join(ROOT, 'Vesta', 'SPIPower', 'out', 'SPIPowerMgr.dll')
DRILL = os.path.join(ROOT, 'Vesta', 'SPIPower', 'rescue', 'drill', 'SwDnl_drill.upg')

# device path -> where the reference bytes come from
#   ('pkg', dir, name) - inside the official update package
#   ('nk',  rel)       - inside the 4120 OS image (MAIN/4120_unpacked/NK/fs)
ENTRIES = (
    dict(dev=r'\ResidentFlash\navigation\app\SPIPowerMgr.dll', tier='HARD',
         src=('pkg', '\\navigation\\app\\', 'SPIPowerMgr.dll'),
         note='the file the proxy replaces (export table was generated from it)'),
    dict(dev=r'\ResidentFlash\Main.exe', tier='SOFT',
         src=('pkg', '\\', 'Main.exe'),
         note='shell launcher, also LoadLibrary()s SPIPowerMgr.dll'),
    dict(dev=r'\ResidentFlash\navigation\app\NCL_BaseCom.dll', tier='SOFT',
         src=('pkg', '\\navigation\\app\\', 'NCL_BaseCom.dll'),
         note='dependency of the factory DLL we load as _orig'),
    dict(dev=r'\Windows\SPIPowerMgr_OS.dll', tier='SOFT',
         src=('nk', 'Windows\\SPIPowerMgr_OS.dll'),
         note='OS-side twin of SPIPowerMgr from the 4120 NK image'),
    dict(dev=r'\Windows\desay_sdk_api.dll', tier='SOFT',
         src=('nk', 'Windows\\desay_sdk_api.dll'),
         note='SPI transport the factory DLL imports (4120 NK image)'),
    dict(dev=r'\ResidentFlash\SwDnl.exe', tier='INFO',
         src=('pkg', '\\', 'SwDnl.exe'),
         note='boot updater = the rollback path (drill depends on it)'),
    dict(dev=r'\ResidentFlash\navigation\app\G3NavHMI.exe', tier='INFO',
         src=('pkg', '\\navigation\\app\\', 'G3NavHMI.exe'),
         note='pristine 4120 shell; ours is G3patch-modified, so INFO only'),
)

# trees we compare the references against (report only, never written to the header)
MOUNTS = (
    ('HU dump', {r'\ResidentFlash': os.path.join(ROOT, 'MAIN', 'ResidentFlash'),
                 r'\Windows': os.path.join(NK_FS, 'Windows')}),
    ('CE emulator', {r'\ResidentFlash': os.path.join(ROOT, 'MAIN', 'WinCE_Emulator_StaticStore+SDMMC',
                                                     'StaticStore', 'ResidentFlash'),
                     r'\Windows': os.path.join(ROOT, 'MAIN', 'WinCE_Emulator_StaticStore+SDMMC',
                                               'StaticStore', 'Windows')}),
)


def md5_of(path):
    h = hashlib.md5()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 16), b''):
            h.update(chunk)
    return h.hexdigest()


def pkg_files():
    b = open(PKG_PATH, 'rb').read()
    fs = ub.parse_files(b, PKG_TOC)
    out = {}
    for f in fs:
        out[(f['dir'], f['name'])] = (f['size'], hashlib.md5(b[f['off']:f['off'] + f['size']]).hexdigest())
    return out


def resolve(entry, pkg):
    kind = entry['src'][0]
    if kind == 'pkg':
        key = (entry['src'][1], entry['src'][2])
        if key not in pkg:
            raise SystemExit('not in %s: %s%s' % (PKG_PATH, entry['src'][1], entry['src'][2]))
        return pkg[key]
    p = os.path.join(NK_FS, entry['src'][1])
    if not os.path.exists(p):
        raise SystemExit('missing NK image file: ' + p)
    return (os.path.getsize(p), md5_of(p))


def mount_path(mount, dev_path):
    for prefix, base in mount.items():
        if dev_path.lower().startswith(prefix.lower()):
            return os.path.join(base, dev_path[len(prefix):].lstrip('\\'))
    return None


def render_header(rows, proxy_md5, drill):
    L = []
    L.append('// GENERATED by Vesta/SPIPower/tools/gen_fingerprints.py -- DO NOT EDIT.')
    L.append('//')
    L.append('// Firmware fingerprint table the installer enforces before it touches')
    L.append('// \\ResidentFlash\\navigation\\app\\SPIPowerMgr.dll (see README.md, firmware version gate).')
    L.append('// Reference bytes come from the official update package %s' %
             os.path.relpath(PKG_PATH, ROOT).replace('\\', '/'))
    L.append('// (TOC 0x%08X) and from the %s NK image; cross-checked against the dumps here.' % (PKG_TOC, FW_NAME))
    L.append('//   python tools\\gen_fingerprints.py            # regenerate + report')
    L.append('//   python tools\\gen_fingerprints.py --check    # verify this file is current')
    L.append('//   python tools\\gen_fingerprints.py --simulate \\\\ResidentFlash=..\\..\\MAIN\\ResidentFlash')
    L.append('//')
    L.append('// HARD - mismatch/missing blocks install; SOFT - blocks install unless "force";')
    L.append('// INFO - reported by status only. Absent SOFT/INFO files are reported, never fatal.')
    L.append('#ifndef SPIPOWER_FINGERPRINTS_H')
    L.append('#define SPIPOWER_FINGERPRINTS_H')
    L.append('')
    L.append('#define FW_INFO 0')
    L.append('#define FW_SOFT 1')
    L.append('#define FW_HARD 2')
    L.append('')
    L.append('static const wchar_t* kFwName = L"%s";' % FW_NAME)
    L.append('static const wchar_t* kFwSrc  = L"%s";' % os.path.relpath(PKG_PATH, ROOT).replace('\\', '\\\\'))
    L.append('')
    L.append('typedef struct {')
    L.append('    const wchar_t* path;')
    L.append('    unsigned int   size;')
    L.append('    const char*    md5;')
    L.append('    int            tier;')
    L.append('} FwFile;')
    L.append('')
    L.append('static const FwFile kFw[] = {')
    for r in rows:
        L.append('    { L"%s", %uu, "%s", FW_%s },   // %s' %
                 (r['dev'].replace('\\', '\\\\'), r['size'], r['md5'], r['tier'], r['note']))
    L.append('};')
    L.append('static const int kFwN = (int)(sizeof(kFw) / sizeof(kFw[0]));')
    L.append('')
    L.append('// our own artifacts: hashed from the installer folder to prove on the device that')
    L.append('// the MD5 code works and that the folder is not a mix of different builds')
    L.append('static const char* kProxyMd5 = "%s";   // out\\SPIPowerMgr.dll (proxy next to the exe)' % proxy_md5)
    L.append('static const char* kDrillMd5 = "%s";   // rescue\\drill\\SwDnl_drill.upg' % drill[1])
    L.append('static const unsigned int kDrillSize = %uu;' % drill[0])
    L.append('')
    L.append('#endif // SPIPOWER_FINGERPRINTS_H')
    return '\r\n'.join(L) + '\r\n'


def parse_header(text):
    rows = []
    for m in re.finditer(r'\{\s*L"((?:[^"\\]|\\.)+)",\s*(\d+)u,\s*"([0-9a-f]{32})",\s*FW_(\w+)', text):
        rows.append(dict(dev=m.group(1).replace('\\\\', '\\'), size=int(m.group(2)),
                         md5=m.group(3), tier=m.group(4)))
    return rows


def simulate(header_text, mounts):
    """mounts: {device prefix -> directory}, e.g. {r'\\ResidentFlash': 'MAIN/ResidentFlash'}"""
    rows = parse_header(header_text)
    if not rows:
        raise SystemExit('no entries parsed from the header')
    m = re.search(r'kProxyMd5\s*=\s*"([0-9a-f]{32})"', header_text)
    proxy = m.group(1) if m else ''
    hard = soft = 0
    print('simulating %d entries (mirrors the installer verdict)' % len(rows))
    for r in rows:
        worst = 'ok'
        for prefix in sorted(mounts):
            p = mount_path({prefix: mounts[prefix]}, r['dev'])
            if not p or not os.path.exists(p):
                state = 'absent'
            else:
                got = md5_of(p)
                if got == r['md5'] and os.path.getsize(p) == r['size']:
                    state = 'ok'
                elif proxy and got == proxy:
                    state = 'DIFF(%s = our proxy)' % got[:8]
                else:
                    state = 'DIFF(%s)' % got[:8]
            print('  %-8s %-52s %-16s %s' % (r['tier'], r['dev'], prefix, state))
            if state.startswith('DIFF'):
                worst = 'DIFF'
        if r['tier'] == 'HARD' and worst == 'DIFF':
            hard += 1
        elif r['tier'] == 'SOFT' and worst == 'DIFF':
            soft += 1
    if hard:
        print('verdict: install would REFUSE (hard mismatch on %d file(s))' % hard)
        return 2
    if soft:
        print('verdict: install would need "force" (soft mismatch on %d file(s))' % soft)
        return 1
    print('verdict: install would proceed (all references match)')
    return 0


def main(argv):
    if '--simulate' in argv:
        i = argv.index('--simulate')
        mounts = {}
        for spec in argv[i + 1:]:
            if '=' not in spec:
                raise SystemExit('use --simulate <mount>=<dir>, e.g. \\ResidentFlash=..\\..\\MAIN\\ResidentFlash')
            mnt, d = spec.split('=', 1)
            mounts[mnt] = d
        if not mounts:
            raise SystemExit('--simulate needs at least one <mount>=<dir>')
        if not os.path.exists(OUT_H):
            raise SystemExit('run without --simulate first (no %s)' % OUT_H)
        return simulate(open(OUT_H, 'r', newline='').read(), mounts)

    pkg = pkg_files()
    rows = []
    print('firmware %s references (source: %s)' % (FW_NAME, os.path.relpath(PKG_PATH, ROOT)))
    for e in ENTRIES:
        size, md5 = resolve(e, pkg)
        rows.append(dict(dev=e['dev'], size=size, md5=md5, tier=e['tier'], note=e['note']))
        print('  %-8s %-52s %8u %s' % (e['tier'], e['dev'], size, md5[:16]))

    proxy_md5 = md5_of(PROXY) if os.path.exists(PROXY) else ''
    drill = (os.path.getsize(DRILL), md5_of(DRILL)) if os.path.exists(DRILL) else (0, '')
    print('self-test artifacts: proxy=%s drill=%s (%u B)' %
          (proxy_md5[:8] or 'missing', drill[1][:8] or 'missing', drill[0]))

    print('cross-check against local trees (report only):')
    for name, mount in MOUNTS:
        diffs = []
        for r in rows:
            p = mount_path(mount, r['dev'])
            if not p or not os.path.exists(p):
                continue
            got = md5_of(p)
            if got != r['md5'] or os.path.getsize(p) != r['size']:
                diffs.append('%s=%s' % (os.path.basename(r['dev']), got[:8]))
        print('  %-12s %s' % (name, 'all matching' if not diffs else 'differs: ' + ', '.join(diffs)))

    text = render_header(rows, proxy_md5, drill)
    assert text == text.encode('ascii').decode('ascii'), 'header must stay ASCII (MSVC 2005 has no charset switch)'
    if '--check' in argv:
        cur = open(OUT_H, 'r', newline='').read() if os.path.exists(OUT_H) else ''
        if cur == text:
            print('=== %s is current' % os.path.relpath(OUT_H, ROOT))
            return 0
        print('!!! %s is STALE - re-run without --check' % os.path.relpath(OUT_H, ROOT))
        return 1

    if os.path.exists(OUT_H) and open(OUT_H, 'r', newline='').read() == text:
        print('=== %s unchanged (deterministic)' % os.path.relpath(OUT_H, ROOT))
    else:
        with open(OUT_H, 'w', newline='') as f:
            f.write(text)
        print('wrote %s (%d entries)' % (os.path.relpath(OUT_H, ROOT), len(rows)))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
