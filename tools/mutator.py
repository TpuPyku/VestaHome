"""mutator - mutation testing (ub-mode, SKILL.md: cover the uncovered, kill the survivors, delete dead code).

    python mutator.py --build "cmd" --test "cmd" [--cobertura cov.xml] [--lines 10-40,52] [--max 200] [--timeout 60]
                      files...

One mutation at a time (== / !=, < / <=, > / >=, && / ||, + / -, true / false, n / n+1, !x / x) on the lines the
tests cover (--cobertura; without it every line) - of --lines only, when given (a change's lines), then the build
and the tests: the tests failing (or hanging past --timeout) kill it, a passing run - a survivor, a failed build -
not counted. Crash-safe and repeatable: the file's
original waits beside it as <file>.mutator-orig until restored, and a later run restores a left one first.
Prints the survivors; exit code: how many.
"""
import argparse
import os
import re
import shutil
import subprocess
import sys

from crapper import cobertura_hits, norm, show

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
SKIP = re.compile(r'^\s*(#|//|/\*|\*)|\bLog\(|\bprintf\(')   # comments, the preprocessor, the logs (not behaviour)


def masked(line):
    """The line with its string / char literals blanked (same length) and a trailing // or # comment cut off."""
    out = re.sub(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', lambda m: ' ' * len(m.group()), line)
    return re.split(r'//|#', out)[0]


def in_text(lines):
    """The lines that are (or touch) a Python triple-quoted string: text, not code."""
    inside, out = None, set()
    for i, line in enumerate(lines):
        for q in re.findall('"{3}|\'{3}', line):   # (written so: this line itself holds no triple quote)
            if inside is None:
                inside = q
            elif q == inside:
                inside = None
            out.add(i)
        if inside:
            out.add(i)
    return out


def mutants(lines, wanted):
    """(line index, new line, what) for each mutation of the wanted lines (1-based numbers)."""
    text = in_text(lines)
    for i, line in enumerate(lines):
        if (wanted is not None and i + 1 not in wanted) or SKIP.match(line) or i in text:
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


def write_mutant(path, lines, bom, k, i, new):
    """The file with line i replaced; its time moved on by k + 1 s: a same-size change in the same second would
    otherwise look unchanged (Python's .pyc cache trusts size and time)."""
    changed = lines[:i] + [new] + lines[i + 1:]
    with open(path, 'wb') as f:
        f.write((b'\xef\xbb\xbf' if bom else b'') + '\n'.join(changed).encode('utf-8'))
    stamp = os.path.getmtime(path + '.mutator-orig') + k + 1
    os.utime(path, (stamp, stamp))


def judge(a):
    """'invalid' (no build), 'survived' (the tests pass), 'killed'."""
    if run(a.build, a.timeout * 4) != 0:
        return 'invalid'
    return 'survived' if run(a.test, a.timeout) == 0 else 'killed'


def mutate_file(path, wanted, a, budget):
    restore(path)
    with open(path, 'rb') as f:
        raw = f.read()
    bom = raw.startswith(b'\xef\xbb\xbf')
    lines = raw.decode('utf-8-sig').split('\n')
    all_m = list(mutants(lines, wanted))
    chosen = all_m[::max(1, len(all_m) // budget)][:budget] if budget else all_m
    stats = {'killed': 0, 'survived': 0, 'invalid': 0}
    survivors = []
    shutil.copy2(path, path + '.mutator-orig')   # (its time too: the original comes back as it was)
    try:
        for k, (i, new, what) in enumerate(chosen):
            write_mutant(path, lines, bom, k, i, new)
            verdict = judge(a)
            stats[verdict] += 1
            if verdict == 'survived':
                survivors.append('%s:%d  %s   | %s' % (show(path), i + 1, what, lines[i].strip()))
                print('SURVIVED ' + survivors[-1], flush=True)
    finally:
        restore(path)
    return stats, survivors, len(all_m)


def line_set(spec):
    """'10-40,52' -> {10, ..., 40, 52}"""
    out = set()
    for part in spec.split(','):
        lo, _, hi = part.partition('-')
        out |= set(range(int(lo), int(hi or lo) + 1))
    return out


def wanted_lines(hits, f, lines):
    """The lines to mutate: the covered ones (all without coverage), of --lines when given; None = every line."""
    wanted = None if hits is None else {n for n, h in hits.get(norm(f), {}).items() if h > 0}
    if lines is None:
        return wanted
    return lines if wanted is None else wanted & lines


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--build', required=True)
    ap.add_argument('--test', required=True)
    ap.add_argument('--cobertura', action='append', default=[])
    ap.add_argument('--lines', type=line_set, help="only these lines (of each file): '10-40,52'")
    ap.add_argument('--max', type=int, default=200, help='mutants per file (evenly spread), 0 = all')
    ap.add_argument('--timeout', type=int, default=60)
    ap.add_argument('files', nargs='+')
    a = ap.parse_args(argv)
    for f in a.files:
        restore(f)   # (a crash's leftover first: the baseline runs on the originals)
    if judge(a) != 'survived':   # the tests must pass on the originals, or every mutant would look killed
        print('mutator: the build or the tests fail before any mutation - nothing to measure')
        return 255
    hits = cobertura_hits(a.cobertura) if a.cobertura else None
    total = []
    for f in a.files:
        stats, surv, n = mutate_file(f, wanted_lines(hits, f, a.lines), a, a.max)
        total += surv
        print('mutator: %s: %d mutants possible, killed %d, survived %d, not built %d' % (
            show(f), n, stats['killed'], stats['survived'], stats['invalid']), flush=True)
    run(a.build, a.timeout * 4)   # the build of the originals back in place
    return len(total)


if __name__ == '__main__':
    sys.exit(min(main(), 255))
