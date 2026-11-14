"""test_check_fingerprints.py - the guard that a new MCU proxy build keeps the installed one recognised."""
import unittest

import check_fingerprints as cf

CPP = '''static const char* kOldProxyMd5[] = {
    "fa7b65e000c6f33bbd2f8be18fe85808",   // 2026-10-11
    "04b3ecab9eef88fa35fc97f1b0ac7573",   // 2026-10-21
};'''


class LostProxyTest(unittest.TestCase):
    def test_same_proxy_is_fine(self):
        self.assertIsNone(cf.lost_proxy('a' * 32, 'a' * 32, CPP))

    def test_new_proxy_with_the_old_one_listed_is_fine(self):
        self.assertIsNone(cf.lost_proxy('04b3ecab9eef88fa35fc97f1b0ac7573', 'b' * 32, CPP))

    def test_new_proxy_with_the_old_one_unlisted_is_named(self):
        msg = cf.lost_proxy('c' * 32, 'b' * 32, CPP)
        self.assertIn('c' * 32, msg)
        self.assertIn('kOldProxyMd5', msg)

    def test_no_committed_proxy_is_fine(self):
        self.assertIsNone(cf.lost_proxy(None, 'b' * 32, CPP))


if __name__ == '__main__':
    unittest.main()
