"""Wraps each Cyrillic string constant of a Python file (not docstrings, not f-string parts, not already T's) in T(...).
Prints the texts wrapped. ast columns are UTF-8 byte offsets."""
import ast, io, sys

path = sys.argv[1]
src = io.open(path, encoding='utf-8', newline='').read()
tree = ast.parse(src)
docs, skip = set(), set()
for node in ast.walk(tree):
    if isinstance(node, (ast.Module, ast.FunctionDef, ast.ClassDef)) and node.body and \
            isinstance(node.body[0], ast.Expr) and isinstance(node.body[0].value, ast.Constant):
        docs.add(id(node.body[0].value))
    if isinstance(node, ast.JoinedStr):
        for v in ast.walk(node):
            skip.add(id(v))
    if isinstance(node, ast.Call) and getattr(node.func, 'id', '') == 'T' and node.args:
        skip.add(id(node.args[0]))
cyr = lambda v: any('Ѐ' <= ch <= 'ӿ' for ch in v)
spans = [(n.lineno, n.col_offset, n.end_lineno, n.end_col_offset, n.value) for n in ast.walk(tree)
         if isinstance(n, ast.Constant) and isinstance(n.value, str) and cyr(n.value) and id(n) not in docs | skip]
lines = src.splitlines(keepends=True)
b = [l.encode('utf-8') for l in lines]
for l0, c0, l1, c1, v in sorted(spans, reverse=True):
    if l0 == l1:
        b[l0 - 1] = b[l0 - 1][:c0] + b'T(' + b[l0 - 1][c0:c1] + b')' + b[l0 - 1][c1:]
    else:
        b[l1 - 1] = b[l1 - 1][:c1] + b')' + b[l1 - 1][c1:]
        b[l0 - 1] = b[l0 - 1][:c0] + b'T(' + b[l0 - 1][c0:]
    print(repr(v))
io.open(path, 'wb').write(b''.join(b))
print(len(spans), 'wrapped', file=sys.stderr)
