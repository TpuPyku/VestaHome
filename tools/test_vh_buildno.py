"""Tests of vh_buildno.py (VestaHome's build of the day: 1.2.261128, then 1.2.261128.2, ...) and of
make_vh_payload.vh_version reading it from the exe's mark."""
import os
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, '..', 'SPIPower', 'install'))
import vh_buildno  # noqa: E402
import make_vh_payload  # noqa: E402


class TestNext(unittest.TestCase):
    def test_first_build_of_a_day(self):
        self.assertEqual(vh_buildno.next_build(None, '261128', 'aa'), ('261128', 1, 'aa'))
        self.assertEqual(vh_buildno.next_build(('261127', 4, 'aa'), '261128', 'bb'), ('261128', 1, 'bb'))

    def test_the_same_code_keeps_its_number(self):
        self.assertEqual(vh_buildno.next_build(('261128', 2, 'aa'), '261128', 'aa'), ('261128', 2, 'aa'))

    def test_changed_code_the_same_day_counts_on(self):
        self.assertEqual(vh_buildno.next_build(('261128', 2, 'aa'), '261128', 'bb'), ('261128', 3, 'bb'))

    def test_state_file_round_trip(self):
        d = tempfile.mkdtemp()
        p = os.path.join(d, 'buildno.txt')
        self.assertIsNone(vh_buildno.load(p))
        vh_buildno.save(p, ('261128', 3, 'cc'))
        self.assertEqual(vh_buildno.load(p), ('261128', 3, 'cc'))
        open(p, 'w').write('rubbish')
        self.assertIsNone(vh_buildno.load(p))

    def test_header(self):
        h = vh_buildno.header(3)
        self.assertIn('#define VH_BUILDNO 3\n', h)
        self.assertIn('#define VH_BUILDNO_S "3"\n', h)

    def test_digest_follows_sources_only(self):
        d = tempfile.mkdtemp()
        os.mkdir(os.path.join(d, 'obj'))
        open(os.path.join(d, 'a.cpp'), 'w').write('x')
        open(os.path.join(d, 'b.inc'), 'w').write('y')
        first = vh_buildno.digest([d])
        open(os.path.join(d, 'obj', 'vh_buildno.h'), 'w').write('#define VH_BUILDNO 9')   # its own output
        open(os.path.join(d, 'notes.txt'), 'w').write('z')
        self.assertEqual(vh_buildno.digest([d]), first)
        open(os.path.join(d, 'b.inc'), 'w').write('y2')
        self.assertNotEqual(vh_buildno.digest([d]), first)
        os.rename(os.path.join(d, 'a.cpp'), os.path.join(d, 'c.cpp'))   # a renamed file: other code
        self.assertNotEqual(vh_buildno.digest([d]), first)


class TestMain(unittest.TestCase):
    def test_builds_of_a_day(self):
        d = tempfile.mkdtemp()
        src = os.path.join(d, 'src')
        os.mkdir(src)
        open(os.path.join(src, 'home.cpp'), 'w').write('v1')
        st, hd = os.path.join(d, 'buildno.txt'), os.path.join(d, 'vh_buildno.h')
        vh_buildno.main([st, hd, src])
        self.assertIn('VH_BUILDNO 1\n', open(hd).read())
        os.utime(hd, (1, 1))
        vh_buildno.main([st, hd, src])   # the same code: the header left as it was
        self.assertEqual(os.path.getmtime(hd), 1)
        open(os.path.join(src, 'home.cpp'), 'w').write('v2')
        vh_buildno.main([st, hd, src])
        self.assertIn('VH_BUILDNO 2\n', open(hd).read())
        self.assertEqual(vh_buildno.load(st)[1], 2)


class TestPayloadVersion(unittest.TestCase):
    def test_first_of_the_day_has_no_number(self):
        self.assertEqual(make_vh_payload.vh_version(b'..VHVER:1.2|Nov 28 2026|1\x00..'), '1.2.261128')

    def test_later_builds_numbered(self):
        self.assertEqual(make_vh_payload.vh_version(b'..VHVER:1.2|Nov  8 2026|2\x00..'), '1.2.261108.2')

    def test_old_mark_without_number(self):
        self.assertEqual(make_vh_payload.vh_version(b'..VHVER:1.2|Nov 28 2026\x00..'), '1.2.261128')

    def test_mark_at_the_very_start(self):
        self.assertEqual(make_vh_payload.vh_version(b'VHVER:1.2|Nov 28 2026|3\x00'), '1.2.261128.3')

    def test_no_mark(self):
        self.assertIsNone(make_vh_payload.vh_version(b'nothing'))


if __name__ == '__main__':
    unittest.main()
