# make_drill.py - build the harmless "drill" package that exercises the whole
# boot-rescue path before any proxy swap is attempted.
#
# Why: SwDnl.exe (the factory boot updater) is triggered by a single byte in
# \ResidentFlash\SwUpdate.flag (0x02 = Navi) and unpacks \SDMemory\SwDnl.upg into
# \ResidentFlash according to \SDMemory\SwDnl.cfg (see ../SWDNL.md). Risking the
# real SPIPowerMgr.dll on the first hardware run is pointless: this package holds
# one inert text file, so a successful run already proves the SD mount
# (\SDMemory), the cfg parsing, the flag semantics, the extraction into
# SWDNL_DST_DIR and that no delete directive fires.
#
# Output (written to <outdir>/):
#   SwDnl_drill.upg     - one file: \navigation\app\VestaDrill.txt (generated text)
#   SwDnl.cfg           - cfg for this package; "SPIPowerInstall.exe drill" writes
#                         its own copy onto the SD, this one is for manual staging
#   SPIPowerInstall.exe - copied from ../out/ when present, so <outdir> can be
#                         dropped onto the HU/SD as a single folder
#
# Format verified against upg_build.py: unk = 0 in all 2023 records of the factory
# package, checksum = -sum(bytes as signed 8-bit).
import os, shutil, sys
sys.path.insert(0, os.path.dirname(__file__))
import upg_build as ub

DIR  = '\\navigation\\app\\'
NAME = 'VestaDrill.txt'
MARK = b'VestaDrill'

MARKER = (
    'Vesta MMC boot-updater drill marker\r\n'
    '------------------------------------\r\n'
    'SwDnl.exe read \\SDMemory\\SwDnl.cfg, took SwDnl_drill.upg from the SD card\r\n'
    'and unpacked it into \\ResidentFlash. Nothing else was touched.\r\n'
    'Clean up: SPIPowerInstall.exe drill-off\r\n'
)

def build_pkg(data):
    # checksum: -sum(bytes as signed 8-bit) -- keep the sign, struct packs it as '<i'
    cks = ub.compute_cks(data)
    return ub.build([{'data': data, 'dir': DIR, 'name': NAME, 'cks': cks, 'unk': 0}]), cks

def main(outdir):
    data = MARKER.encode('ascii')
    pkg, cks = build_pkg(data)

    os.makedirs(outdir, exist_ok=True)
    upg = os.path.join(outdir, 'SwDnl_drill.upg')
    cfg = os.path.join(outdir, 'SwDnl.cfg')
    open(upg, 'wb').write(pkg)
    open(cfg, 'wb').write((
        'SWDNL_DST_DIR\t\tResidentFlash\r\n'
        'SWDNL_SRC_NV_FILE\t"SDMemory\\SwDnl_drill.upg"\r\n'
    ).encode('latin1'))

    exe = os.path.join(os.path.dirname(__file__), '..', 'out', 'SPIPowerInstall.exe')
    if os.path.exists(exe):
        shutil.copy2(exe, os.path.join(outdir, 'SPIPowerInstall.exe'))
        print('copied SPIPowerInstall.exe into %s' % outdir)
    else:
        print('NOTE: %s not built yet - run install\\build.bat first' % exe)

    # self-check: parse the built package back
    recs = ub.parse_files(pkg, len(data))
    assert len(recs) == 1, recs
    assert recs[0]['dir'] == DIR and recs[0]['name'] == NAME
    assert recs[0]['size'] == len(data) and (recs[0]['cks'] & 0xffffffff) == (cks & 0xffffffff)
    assert recs[0]['unk'] == 0
    assert pkg[:len(data)] == data and MARK in pkg
    print('wrote %s (%d bytes) + %s -- self-check OK' % (upg, len(pkg), cfg))
    print('verdict after a reboot (SPIPowerInstall.exe status / drill-off):')
    print('  flag gone + \\ResidentFlash\\navigation\\app\\VestaDrill.txt present = PASS')

if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else os.path.join('..', 'rescue', 'drill'))
