"""crapper - the CRAP score of each function (ub-mode, SKILL.md: every function under 10).

    python crapper.py [--cobertura cov.xml ...] [--pycov cov.json ...] [--max 10] files...

CRAP = CC^2 * (1 - coverage)^3 + CC: CC is the function's cyclomatic complexity (lizard), coverage the share of its
covered lines - from OpenCppCoverage's Cobertura XML (C++) and coverage.py's JSON (Python). A function with no
coverage data counts as uncovered. Prints the functions over --max, worst first; exit code: how many.
"""
import argparse
import json
import os
import sys
import xml.etree.ElementTree as ET

import lizard


def norm(path):
    return os.path.normcase(os.path.abspath(path))


def cobertura_hits(paths):
    """{file: {line: hits}} from Cobertura XMLs (OpenCppCoverage: filename relative to a <source>)."""
    out = {}
    for p in paths:
        root = ET.parse(p).getroot()
        # (OpenCppCoverage writes the drive alone, "e:": its root is meant, not the drive's current folder)
        sources = [(s.text or '') + ('\\' if (s.text or '').endswith(':') else '') for s in root.iter('source')] or ['']
        for cls in root.iter('class'):
            fn = cls.get('filename', '')
            full = next((os.path.join(s, fn) for s in sources if os.path.exists(os.path.join(s, fn))), fn)
            lines = out.setdefault(norm(full), {})
            for ln in cls.iter('line'):
                n = int(ln.get('number'))
                lines[n] = max(lines.get(n, 0), int(ln.get('hits', 0)))
    return out


def pycov_hits(paths):
    """{file: {line: hits}} from coverage.py's JSON reports (coverage json)."""
    out = {}
    for p in paths:
        data = json.load(open(p, encoding='utf-8'))
        for fn, f in data.get('files', {}).items():
            lines = out.setdefault(norm(fn), {})
            for n in f.get('executed_lines', []):
                lines[n] = max(lines.get(n, 0), 1)
            for n in f.get('missing_lines', []):
                lines.setdefault(n, 0)
    return out


def coverage_of(func, lines):
    """The share of the function's coverable lines that ran (0 with no data)."""
    mine = [h for n, h in lines.items() if func.start_line <= n <= func.end_line]
    return sum(1 for h in mine if h > 0) / len(mine) if mine else 0.0


def crap(cc, cov):
    return cc * cc * (1 - cov) ** 3 + cc


def scores(files, hits):
    rows = []
    for f in files:
        info = lizard.analyze_file(f)
        lines = hits.get(norm(f), {})
        for fn in info.function_list:
            cov = coverage_of(fn, lines)
            rows.append((crap(fn.cyclomatic_complexity, cov), fn.cyclomatic_complexity, cov, f, fn.start_line, fn.name))
    return sorted(rows, reverse=True)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--cobertura', action='append', default=[])
    ap.add_argument('--pycov', action='append', default=[])
    ap.add_argument('--max', type=float, default=10)
    ap.add_argument('--all', action='store_true', help='print every function, not only the ones over --max')
    ap.add_argument('files', nargs='+')
    a = ap.parse_args(argv)
    hits = cobertura_hits(a.cobertura)
    for k, v in pycov_hits(a.pycov).items():
        hits.setdefault(k, {}).update(v)
    rows = scores(a.files, hits)
    bad = [r for r in rows if r[0] >= a.max]
    for score, cc, cov, f, line, name in (rows if a.all else bad):
        print('%7.1f  CC %3d  cov %3d%%  %s:%d %s' % (score, cc, cov * 100, os.path.relpath(f), line, name))
    print('crapper: %d functions, %d at CRAP >= %g' % (len(rows), len(bad), a.max))
    return len(bad)


if __name__ == '__main__':
    sys.exit(min(main(), 255))
