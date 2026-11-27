"""Every switch that ControlsToCfg keeps in VestaHome.ini (s_on[T_...] read there) must be in IsCfgToggle - else a tap
changes only the picture: nothing applied, nothing saved (sheet 45 p. 27, 2026-11-27: "Телефон по USB" switched on
stayed off after a reboot; it went off only because the Wi-Fi switch, tapped in the same second, saved everything).
Exit code: the number of switches missing."""
import io, os, re, sys

src = io.open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'vh_sound.inc'), encoding='utf-8-sig').read()


def body(name):
    at = src.index('static %s(' % name)
    depth, i = 0, src.index('{', at)
    for j in range(i, len(src)):
        depth += {'{': 1, '}': -1}.get(src[j], 0)
        if not depth:
            return src[i:j]
    raise SystemExit('no end of ' + name)


kept = set(re.findall(r's_on\[(T_\w+)\]', body('void ControlsToCfg')))
listed = set(re.findall(r'\b(T_\w+)\b', body('bool IsCfgToggle')))
missing = sorted(kept - listed)
for t in missing:
    print('FAIL: %s kept by ControlsToCfg, but its tap does not call it (IsCfgToggle)' % t)
print('%d switches kept, %d missing' % (len(kept), len(missing)))
sys.exit(len(missing))
