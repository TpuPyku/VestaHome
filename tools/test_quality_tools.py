"""The ub-mode lever tools (crapper, dryer, mutator) on small made-up projects: what their user sees.

    python -m unittest test_quality_tools        (from VestaHome/tools)
"""
import io
import os
import shutil
import sys
import tempfile
import unittest
from contextlib import redirect_stdout

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import crapper   # noqa: E402
import dryer     # noqa: E402
import mutator   # noqa: E402

C_SOURCE = '''int simple(int a)
{
    return a + 1;
}

int branchy(int a)
{
    if (a > 0) {
        if (a > 10)
            return 2;
        return 1;
    }
    if (a < -10)
        return -2;
    return 0;
}
'''


def run(main, argv):
    out = io.StringIO()
    with redirect_stdout(out):
        code = main(argv)
    return code, out.getvalue()


class Tools(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self.dir)

    def write(self, name, text):
        path = os.path.join(self.dir, name)
        with open(path, 'w', encoding='utf-8', newline='\n') as f:
            f.write(text)
        return path

    def test_crapper_scores_by_complexity_and_coverage(self):
        src = self.write('a.cpp', C_SOURCE)
        # branchy (CC 4) uncovered: 4^2 + 4 = 20 >= 10; simple (CC 1): 2
        code, out = run(crapper.main, [src])
        self.assertEqual(code, 1)
        self.assertIn('branchy', out)
        self.assertNotIn(' simple', out)
        # covered in full (Cobertura, OpenCppCoverage's shape: the drive alone as the source): CRAP = CC = 4
        cov = self.write('cov.xml', '<coverage><sources><source>%s</source></sources><packages><package><classes>'
                         '<class filename="%s"><lines>%s</lines></class></classes></package></packages></coverage>' % (
                             src[:2], src[3:], ''.join('<line number="%d" hits="1"/>' % n for n in range(1, 17))))
        code, out = run(crapper.main, ['--cobertura', cov, src])
        self.assertEqual(code, 0)
        self.assertIn('0 at CRAP >= 10', out)

    def test_crapper_exact_scores(self):
        src = self.write('a.cpp', C_SOURCE)
        # simple: only its first line known, covered -> 100 %; branchy (lines 6..16): 3 of 6 known lines ran
        lines = {1: 1, 6: 1, 8: 1, 9: 0, 13: 0, 15: 0, 16: 1}
        cov = self.write('cov.xml', '<coverage><sources><source>%s</source></sources><packages><package><classes>'
                         '<class filename="%s"><lines>%s</lines></class></classes></package></packages></coverage>' % (
                             src[:2], src[3:], ''.join('<line number="%d" hits="%d"/>' % kv for kv in lines.items())))
        code, out = run(crapper.main, ['--max', '1', '--cobertura', cov, src])
        self.assertEqual(code, 2)                               # at the bound counts: 1.0 >= 1
        code, out = run(crapper.main, ['--all', '--cobertura', cov, src])
        self.assertIn('    1.0  CC   1  cov 100%', out)        # 1^2 * 0^3 + 1
        self.assertIn('    6.0  CC   4  cov  50%', out)        # 3 of its 6 known lines: 16 * 0.5^3 + 4
        self.assertEqual(code, 0)

    def test_crapper_reads_coverage_py(self):
        src = self.write('m.py', 'def f(a):\n    if a:\n        return 1\n    if a is None:\n        return 2\n'
                         '    return 3\n')
        rep = self.write('cov.json', '{"files": {"%s": {"executed_lines": [1, 2, 3, 4], "missing_lines": [5, 6]}}}'
                         % src.replace('\\', '\\\\'))
        code, out = run(crapper.main, ['--pycov', rep, '--max', '3', src])   # CC 3, 4 of 6 lines: 9 / 27 + 3
        self.assertEqual(code, 1)
        self.assertIn('  3.3  CC   3  cov  66%', out)

    def test_dryer_finds_a_renamed_copy(self):
        copy = C_SOURCE.replace('branchy', 'other').replace('a', 'b')
        src = self.write('a.cpp', C_SOURCE + '\n' + copy)
        code, out = run(dryer.main, ['--min-tokens', '20', src])
        self.assertEqual(code, 1)
        self.assertIn('branchy', out)
        self.assertIn('other', out)
        code, out = run(dryer.main, ['--min-tokens', '20', self.write('b.cpp', C_SOURCE)])
        self.assertEqual(code, 0)

    def test_dryer_rank_bounds_and_order(self):
        body = C_SOURCE.split('\n\n')[1]
        same = body.replace('branchy', 'twin')
        near = body.replace('branchy', 'near').replace('return 2;', 'return 2 + a * 3;')
        src = self.write('a.cpp', '\n\n'.join([body, same, near]) + '\n')
        n = len(dryer.tokens(body))
        code, out = run(dryer.main, ['--rank', '1.0', '--min-tokens', str(n), src])
        self.assertEqual(code, 1)                    # identical (rank exactly 1.0) only; the function at the bound
        self.assertIn('twin', out)
        code, out = run(dryer.main, ['--rank', '0.5', '--min-tokens', str(n), src])
        first = out.splitlines()[0]
        self.assertTrue(first.startswith('1.00'), first)   # the highest rank first
        code, out = run(dryer.main, ['--min-tokens', str(n + 1), src])
        self.assertEqual(code, 0)                    # all shorter than the bound: none compared

    def test_dryer_leaves_a_nested_function_and_its_owner(self):
        body = ''.join('        x%d = a + b * %d - url[%d] + c(a, b, %d)\n' % (i, i, i, i) for i in range(8))
        src = self.write('n.py', 'def outer(url):\n    def inner(a, b):\n' + body + '        return x0\n'
                                  '    return inner\n')
        code, out = run(dryer.main, ['--min-tokens', '5', src])
        self.assertEqual(code, 0, out)
        one = self.write('o.cpp', 'int f(int a) { return a * 2 + 3 - a; } int g(int b) { return b * 2 + 3 - b; }\n')
        code, out = run(dryer.main, ['--min-tokens', '5', one])
        self.assertEqual(code, 1, out)               # copies on one line: neither holds the other

    def test_dryer_names_with_underscores_are_names(self):
        a = 'int f(int _alpha)\n{\n    if (_alpha > 1) return _alpha * 2 + 3;\n    return _alpha - 7;\n}\n'
        b = a.replace('_alpha', '_beta').replace('int f', 'int g')
        code, out = run(dryer.main, ['--rank', '1.0', '--min-tokens', '5', self.write('u.cpp', a + '\n' + b)])
        self.assertEqual(code, 1)

    def test_mutants_leave_python_text(self):
        lines = ['x = 1 + 2', '"""doc - text', 'more + text"""', "w = set(\'\'\'a - b", "c\'\'\'.split())", 'y = 3 - 4',
                 'z = 5  # n - 1']
        self.assertEqual(sorted({i for i, _, _ in mutator.mutants(lines, None)}), [0, 5, 6])
        self.assertEqual([w for i, _, w in mutator.mutants(lines, None) if i == 6], ['5 -> 6'])   # not the comment

    def test_mutants_leave_strings_and_comments(self):
        lines = ['x = a + b;  // a + b', 's = "a + b";', '# define A 1', 'if (a == b && c) y = !z;']
        made = [(i, what) for i, _, what in mutator.mutants(lines, None)]
        self.assertIn((0, '+ -> -'), made)
        self.assertNotIn(1, [i for i, _ in made])      # inside a string
        self.assertNotIn(2, [i for i, _ in made])      # the preprocessor
        self.assertEqual(sum(1 for i, _ in made if i == 0), 1)   # the comment's "+" left alone
        self.assertTrue({'== -> !=', '&& -> ||', '! -> '} <= {w for i, w in made if i == 3})

    def test_mutator_kills_and_finds_survivors_and_restores(self):
        code_py = self.write('calc.py', 'def add(a, b):\n    return a + b\n\n\ndef unused(a):\n    return a * 2 + 1\n')
        test_py = self.write('check.py', 'import sys\nsys.path.insert(0, %r)\nimport calc\n'
                             'sys.exit(0 if calc.add(2, 3) == 5 else 1)\n' % self.dir)
        before = open(code_py, 'rb').read()
        exe = '"%s"' % sys.executable
        code, out = run(mutator.main, ['--build', exe + ' -c "pass"', '--test', '%s "%s"' % (exe, test_py),
                                       '--timeout', '30', code_py])
        self.assertEqual(open(code_py, 'rb').read(), before)          # the original back
        self.assertFalse(os.path.exists(code_py + '.mutator-orig'))
        self.assertNotIn('calc.py:2  + -> -   |', out)                  # a + b mutated: the test kills it
        self.assertIn('calc.py:6  + -> -', out)                         # unused(): untested, it survives
        self.assertEqual(code, out.count('SURVIVED'))

    def test_mutator_counts_and_covered_lines_only(self):
        code_py = self.write('calc.py', 'def add(a, b):\n    return a + b\n\n\ndef unused(a):\n    return a * 2 + 1\n')
        test_py = self.write('check.py', 'import sys\nsys.path.insert(0, %r)\nimport calc\n'
                             'sys.exit(0 if calc.add(2, 3) == 5 else 1)\n' % self.dir)
        cov = self.write('cov.xml', '<coverage><sources><source>%s</source></sources><packages><package><classes>'
                         '<class filename="calc.py"><lines><line number="2" hits="1"/><line number="6" hits="0"/>'
                         '</lines></class></classes></package></packages></coverage>' % self.dir)
        exe = '"%s"' % sys.executable
        code, out = run(mutator.main, ['--build', exe + ' -c "pass"', '--test', '%s "%s"' % (exe, test_py),
                                       '--cobertura', cov, code_py])
        self.assertIn('1 mutants possible, killed 1, survived 0, not built 0', out)   # line 2 only: covered
        self.assertEqual(code, 0)
        code, out = run(mutator.main, ['--build', exe + ' -c "pass"', '--test', '%s "%s"' % (exe, test_py),
                                       '--lines', '1-1,6', code_py])          # the change's lines only: unused()'s
        self.assertIn('3 mutants possible, killed 0, survived 3', out)        # + -> -, 2 -> 3, 1 -> 2
        code, out = run(mutator.main, ['--build', exe + ' -c "pass"', '--test', '%s "%s"' % (exe, test_py),
                                       '--cobertura', cov, '--lines', '2-6', code_py])   # and covered
        self.assertIn('1 mutants possible', out)

    def test_write_mutant_replaces_one_line_and_moves_the_time_on(self):
        path = self.write('f.py', 'a = 1\nb = 2\nc = 3\n')
        shutil.copy2(path, path + '.mutator-orig')
        t0 = os.path.getmtime(path + '.mutator-orig')
        mutator.write_mutant(path, ['a = 1', 'b = 2', 'c = 3', ''], False, 2, 1, 'b = 9')
        self.assertEqual(open(path, 'rb').read(), b'a = 1\nb = 9\nc = 3\n')
        self.assertEqual(os.path.getmtime(path), t0 + 3)
        mutator.restore(path)

    def test_mutator_stops_when_the_tests_fail_unmutated(self):
        code_py = self.write('calc.py', 'def add(a, b):\n    return a - b\n')
        test_py = self.write('check.py', 'import sys\nsys.path.insert(0, %r)\nimport calc\n'
                             'sys.exit(0 if calc.add(2, 3) == 5 else 1)\n' % self.dir)
        exe = '"%s"' % sys.executable
        code, out = run(mutator.main, ['--build', exe + ' -c "pass"', '--test', '%s "%s"' % (exe, test_py), code_py])
        self.assertEqual(code, 255)
        self.assertIn('fail before any mutation', out)

    def test_a_hanging_test_is_stopped_with_its_children(self):
        """A mutant looping for ever (2026-10-28: a test hung 3 h past --timeout - only the shell was killed, the
        python under it kept the pipes open): the whole tree stopped, 'timeout' soon after the time."""
        import time
        t = time.time()
        self.assertEqual(mutator.run('"%s" -c "import time; time.sleep(60)"' % sys.executable, 2), 'timeout')
        self.assertLess(time.time() - t, 15)
        self.assertEqual(mutator.run('"%s" -c "pass"' % sys.executable, 10), 0)

    def test_mutator_restores_a_left_original_first(self):
        path = self.write('f.py', 'broken by a crash\n')
        with open(path + '.mutator-orig', 'w', encoding='utf-8', newline='\n') as f:
            f.write('x = 1 + 2\n')
        mutator.restore(path)
        self.assertEqual(open(path, encoding='utf-8').read(), 'x = 1 + 2\n')
        self.assertFalse(os.path.exists(path + '.mutator-orig'))


if __name__ == '__main__':
    unittest.main()
