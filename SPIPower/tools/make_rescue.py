# make_rescue.py - build a minimal rescue ".upg" that restores only SPIPowerMgr.dll.
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

def main(src_upg, outdir):
    b = open(src_upg, 'rb').read()
    files = ub.parse_files(b, 0x03276D3F)
    sp = next(f for f in files if f['name'] == DLL_NAME)
    sp['data'] = b[sp['off']:sp['off'] + sp['size']]
    print('source %s: %s size=%d cks=%d' % (src_upg, DLL_NAME, sp['size'], sp['cks']))

    # recompute the checksum from the decoded algorithm and cross-check it
    cks = ub.compute_cks(sp['data'])
    assert (cks & 0xffffffff) == (sp['cks'] & 0xffffffff), (cks, sp['cks'])
    sp['cks'] = cks

    pkg = ub.build([{'data': sp['data'], 'dir': sp['dir'], 'name': sp['name'],
                     'cks': sp['cks'], 'unk': sp['unk']}])

    os.makedirs(outdir, exist_ok=True)
    upg_path = os.path.join(outdir, 'SwDnl.upg')
    cfg_path = os.path.join(outdir, 'SwDnl.cfg')
    open(upg_path, 'wb').write(pkg)
    open(cfg_path, 'wb').write(MINIMAL_CFG.encode('latin1'))

    # self-check: parse the built package back
    recs = ub.parse_files(pkg, len(sp['data']))
    assert len(recs) == 1 and recs[0]['name'] == DLL_NAME
    assert recs[0]['size'] == sp['size'] and recs[0]['cks'] == sp['cks']
    assert pkg[:sp['size']] == sp['data']
    print('wrote %s (%d bytes) and %s -- self-check OK' % (upg_path, len(pkg), cfg_path))

if __name__ == '__main__':
    src = sys.argv[1] if len(sys.argv) > 1 else '..\\..\\..\\MAIN\\4120\\SwDnl.upg'
    out = sys.argv[2] if len(sys.argv) > 2 else '..\\rescue'
    main(src, out)
