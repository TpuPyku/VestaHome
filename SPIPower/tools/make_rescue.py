# make_rescue.py - build a minimal rescue ".upg" that restores only our two replaced files: SPIPowerMgr.dll and
# FLC_BLUETOOTH_SDK.dll (the Bluetooth proxy's, 2026-10-18).
#
# Produces a boot-time firmware update (applied by SwDnl.exe before the shell starts)
# that overwrites \ResidentFlash\navigation\app\SPIPowerMgr.dll with the factory copy,
# WITHOUT wiping the rest of navigation\app (unlike the stock SwDnl.cfg).
#
# Output (written to <outdir>/):
#   SwDnl.upg   - minimal package: one file (SPIPowerMgr.dll), exact factory bytes
#                 + its original checksum, so SwDnl accepts it.
#   SwDnl.cfg   - minimal config: destination ResidentFlash, source SDMemory\SwDnl.upg,
#                 NO delete directives (just overwrite the one DLL).
#
# The factory DLL is taken from the source firmware image (SwDnl.upg) -- verified
# byte-identical to \ResidentFlash\navigation\app\SPIPowerMgr.dll.
import os, sys, struct
sys.path.insert(0, os.path.dirname(__file__))
import upg_build as ub

DLL_NAME = 'SPIPowerMgr.dll'
DLL_DIR  = '\\navigation\\app\\'

MINIMAL_CFG = (
    'SWDNL_DST_DIR\t\tResidentFlash\r\n'
    'SWDNL_RD_DATA_LEN\t76\r\n'
    'SWDNL_SRC_RD_FILE\t"SDMemory\\RdDnl.upg"\r\n'
    'SWDNL_SRC_NV_FILE\t"SDMemory\\SwDnl.upg"\r\n'
    'SWDNL_CAN_UPDATE_COM\t4\r\n'
)

BT_NAME = 'FLC_BLUETOOTH_SDK.dll'   # the Bluetooth proxy replaces it too (same directory)


def factory(b, files, name):
    f = next(x for x in files if x['name'] == name and x['dir'] == DLL_DIR)
    f['data'] = b[f['off']:f['off'] + f['size']]
    # recompute the checksum from the decoded algorithm and cross-check it
    cks = ub.compute_cks(f['data'])
    assert (cks & 0xffffffff) == (f['cks'] & 0xffffffff), (name, cks, f['cks'])
    f['cks'] = cks
    print('factory %s size=%d cks=%d' % (name, f['size'], f['cks']))
    return f


def main(src_upg, outdir):
    b = open(src_upg, 'rb').read()
    files = ub.parse_files(b, 0x03276D3F)
    print('source %s' % src_upg)
    parts = [factory(b, files, DLL_NAME), factory(b, files, BT_NAME)]
    sp = parts[0]
    pkg = ub.build([{'data': f['data'], 'dir': f['dir'], 'name': f['name'], 'cks': f['cks'], 'unk': f['unk']}
                    for f in parts])

    os.makedirs(outdir, exist_ok=True)
    upg_path = os.path.join(outdir, 'SwDnl.upg')
    cfg_path = os.path.join(outdir, 'SwDnl.cfg')
    open(upg_path, 'wb').write(pkg)
    open(cfg_path, 'wb').write(MINIMAL_CFG.encode('latin1'))

    # self-check: parse the built package back
    recs = ub.parse_files(pkg, sum(len(f['data']) for f in parts))
    assert [x['name'] for x in recs] == [f['name'] for f in parts]
    for x, f in zip(recs, parts):
        assert x['size'] == f['size'] and x['cks'] == f['cks']
        assert pkg[x['off']:x['off'] + x['size']] == f['data']
    print('wrote %s (%d bytes) and %s -- self-check OK' % (upg_path, len(pkg), cfg_path))

if __name__ == '__main__':
    src = sys.argv[1] if len(sys.argv) > 1 else '..\\..\\..\\MAIN\\4120\\SwDnl.upg'
    out = sys.argv[2] if len(sys.argv) > 2 else '..\\rescue'
    main(src, out)
