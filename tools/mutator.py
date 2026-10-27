"""mutator - mutation testing (ub-mode, SKILL.md: cover the uncovered, kill the survivors, delete dead code).

    python mutator.py --build "cmd" --test "cmd" [--cobertura cov.xml] [--max 200] [--timeout 60] files...

One mutation at a time (== / !=, < / <=, > / >=, && / ||, + / -, true / false, n / n+1, !x / x) on the lines the
tests cover (--cobertura; without it every line), then the build and the tests: the tests failing (or hanging past
--timeout) kill it, a passing run - a survivor, a failed build - not counted. Crash-safe and repeatable: the file's
original waits beside it as <file>.mutator-orig until restored, and a later run restores a left one first.
Prints the survivors; exit code: how many.
"""
import argparse
import os
import re
import subprocess
import sys

from crapper import cobertura_hits, norm

# (pattern, replacement) on a line with its strings and comments masked; one match replaced per mutant
OPS = [
    (r'==', '!='), (r'!=', '=='),
    (r'(?<![<>=!])<=', '<'), (r'(?<![<>=!-])>=', '>'),
    (r'(?<![<\w])<(?![<=])', '<='), (r'(?<![->\w])>(?![>=])', '>='),
    (r'&&', '||'), (r'\|\|', '&&'),
    (r'(?<![+\w])\+(?![+=])', '-'), (r'(?<![-\w])(?<!-)-(?![-=>])', '+'),
    (r'\btrue\b', 'false'), (r'\bfalse\b', 'true'),
    (r'(?<![\w.])(\d+)(?![\w.])', None),          # n -> n + 1
    (r'!(?=[\w(])', ''),                          # !x -> x
]
SKIP = re.compile(r'^\s*(#|//|/\*|\*)')


def masked(line):
    """The line with string / char literals and a trailing // comment blanked (same length)."""
    out = re.sub(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', lambda m: ' ' * len(m.group()), line)
    c = out.find('//')
    return out if c < 0 else out[:c] + ' ' * (len(out) - c)


def mutants(lines, wanted):
    """(line index, new line, what) for each mutation of the wanted lines (1-based numbers)."""
    for i, line in enumerate(lines):
        if (wanted is not None and i + 1 not in wanted) or SKIP.match(line):
            continue
        m_line = masked(line)
        for pat, rep in OPS:
            for m in re.finditer(pat, m_line):
                new = rep if rep is not None else str(int(m.group(1)) + 1)
                yield i, line[:m.start()] + new + line[m.end():], '%s -> %s' % (m.group(), new)


def run(cmd, timeout):
    try:
        return subprocess.run(cmd, shell=True, capture_output=True, timeout=timeout).returncode
    except subprocess.TimeoutExpired:
        return 'timeout'


def restore(path):
    orig = path + '.mutator-orig'
    if os.path.exists(orig):
        os.replace(orig, path)


def mutate_file(path, wanted, a, budget):
    restore(path)
    raw = open(path, 'rb').read()
    bom = raw.startswith(b'\xef\xbb\xbf')
    text = raw.decode('utf-8-sig')
    lines = text.split('\n')
    all_m = list(mutants(lines, wanted))
    step = max(1, len(all_m) // budget) if budget else 1
    stats = {'killed': 0, 'survived': 0, 'invalid': 0}
    survivors = []
    open(path + '.mutator-orig', 'wb').write(raw)
    try:
        for i, new, what in all_m[::step][:budget]:
            changed = lines[:i] + [new] + lines[i + 1:]
            open(path, 'wb').write((b'\xef\xbb\xbf' if bom else b'') + '\n'.join(changed).encode('utf-8'))
            if run(a.build, a.timeout * 4) != 0:
                stats['invalid'] += 1
                continue
            r = run(a.test, a.timeout)
            if r == 0:
                stats['survived'] += 1
                survivors.append('%s:%d  %s   | %s' % (os.path.relpath(path), i + 1, what, lines[i].strip()))
                print('SURVIVED ' + survivors[-1], flush=True)
            else:
                stats['killed'] += 1
    finally:
        restore(path)
    return stats, survivors, len(all_m)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--build', required=True)
    ap.add_argument('--test', required=True)
    ap.add_argument('--cobertura', action='append', default=[])
    ap.add_argument('--max', type=int, default=200, help='mutants per file (evenly spread), 0 = all')
    ap.add_argument('--timeout', type=int, default=60)
    ap.add_argument('files', nargs='+')
    a = ap.parse_args(argv)
    hits = cobertura_hits(a.cobertura) if a.cobertura else None
    total = []
    for f in a.files:
        wanted = None if hits is None else {n for n, h in hits.get(norm(f), {}).items() if h > 0}
        stats, surv, n = mutate_file(f, wanted, a, a.max)
        total += surv
        print('mutator: %s: %d mutants possible, killed %d, survived %d, not built %d' % (
            os.path.relpath(f), n, stats['killed'], stats['survived'], stats['invalid']), flush=True)
    run(a.build, a.timeout * 4)   # the build of the originals back in place
    return len(total)


if __name__ == '__main__':
    sys.exit(min(main(), 255))
