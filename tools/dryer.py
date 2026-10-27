"""dryer - duplicated functions (ub-mode, SKILL.md: remove duplication ranked 0.8 or above).

    python dryer.py [--rank 0.8] [--min-tokens 40] files...

Each function (lizard) becomes a token stream with the names and literals replaced by placeholders (so a copy with
other names still matches); the rank of a pair is difflib's ratio of their streams. Prints the pairs at or over
--rank, highest first; exit code: how many.
"""
import argparse
import difflib
import os
import re
import sys

import lizard

from crapper import show

TOKEN = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[A-Za-z_]\w*|\d[\w.]*|\S')
KEYWORDS = set('''if else for while do switch case default break continue return goto static const struct class
    enum sizeof new delete true false NULL void int char short long unsigned signed double float bool def lambda
    and or not in is None True False try except finally with as pass raise yield import from'''.split())


def tokens(text):
    out = []
    for t in TOKEN.findall(text):
        if t[0] in '"\'':
            out.append('S')
        elif t[0].isdigit():
            out.append('N')
        elif t[0].isalpha() or t[0] == '_':
            out.append(t if t in KEYWORDS else 'I')
        else:
            out.append(t)
    return out


def functions(path, min_tokens):
    src = open(path, encoding='utf-8-sig', errors='replace').read().splitlines()
    out = []
    for fn in lizard.analyze_file(path).function_list:
        body = '\n'.join(src[fn.start_line - 1:fn.end_line])
        toks = tokens(body)
        if len(toks) >= min_tokens:
            out.append((path, fn.start_line, fn.name, toks))
    return out


def pairs(funcs, rank):
    found = []
    for i in range(len(funcs)):
        for j in range(i + 1, len(funcs)):
            a, b = funcs[i][3], funcs[j][3]
            if min(len(a), len(b)) / max(len(a), len(b)) < rank:
                continue   # lengths alone rule it out
            m = difflib.SequenceMatcher(None, a, b, autojunk=False)
            if m.real_quick_ratio() >= rank and m.quick_ratio() >= rank:
                r = m.ratio()
                if r >= rank:
                    found.append((r, funcs[i], funcs[j]))
    return sorted(found, key=lambda x: -x[0])


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--rank', type=float, default=0.8)
    ap.add_argument('--min-tokens', type=int, default=40)
    ap.add_argument('files', nargs='+')
    a = ap.parse_args(argv)
    funcs = [f for p in a.files for f in functions(p, a.min_tokens)]
    found = pairs(funcs, a.rank)
    for r, x, y in found:
        print('%.2f  %s:%d %s  ~  %s:%d %s' % (r, show(x[0]), x[1], x[2], show(y[0]), y[1], y[2]))
    print('dryer: %d functions, %d pairs at rank >= %g' % (len(funcs), len(found), a.rank))
    return len(found)


if __name__ == '__main__':
    sys.exit(min(main(), 255))
