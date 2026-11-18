"""Tests of make_manual_pdf.expand_links: in the PDF every link is printed in full (the user 2026-11-18).
Run: python VestaHome/tools/test_make_manual_pdf.py"""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_manual_pdf import expand_links   # noqa: E402

BASE = 'https://github.com/TpuPyku/OSMapsCE/blob/master/docs/'


class ExpandLinks(unittest.TestCase):
    def test_named_web_link_gets_its_address(self):
        self.assertEqual(expand_links('<a href="https://boosty.to/vesta_home">Boosty</a>', BASE),
                         '<a href="https://boosty.to/vesta_home">Boosty</a> (https://boosty.to/vesta_home)')

    def test_link_showing_its_address_without_scheme_is_written_once_in_full(self):
        self.assertEqual(expand_links('<a href="https://github.com/TpuPyku/YaMaps">github.com/TpuPyku/YaMaps</a>',
                                      BASE),
                         '<a href="https://github.com/TpuPyku/YaMaps">https://github.com/TpuPyku/YaMaps</a>')

    def test_link_already_in_full_is_left(self):
        html = '<a href="https://bearssl.org">https://bearssl.org</a>'
        self.assertEqual(expand_links(html, BASE), html)

    def test_link_inside_the_guide_is_left(self):
        html = '<a href="#3-offline-maps">section 3</a>'
        self.assertEqual(expand_links(html, BASE), html)

    def test_relative_link_becomes_the_repository_address(self):
        self.assertEqual(expand_links('<a href="manual.en.md">English version</a>', BASE),
                         '<a href="%smanual.en.md">English version</a> (%smanual.en.md)' % (BASE, BASE))

    def test_relative_link_upwards_is_resolved(self):
        self.assertEqual(expand_links('<a href="../LICENSE">licence</a>', BASE),
                         '<a href="https://github.com/TpuPyku/OSMapsCE/blob/master/LICENSE">licence</a> '
                         '(https://github.com/TpuPyku/OSMapsCE/blob/master/LICENSE)')

    def test_tags_inside_the_text_count_as_text(self):
        self.assertEqual(expand_links('<a href="https://x.org/a"><strong>A</strong></a>', BASE),
                         '<a href="https://x.org/a"><strong>A</strong></a> (https://x.org/a)')

    def test_every_link_of_a_line(self):
        out = expand_links('<a href="https://a.org">a</a> и <a href="https://b.org">b</a>', BASE)
        self.assertEqual(out, '<a href="https://a.org">a</a> (https://a.org) и <a href="https://b.org">b</a> '
                              '(https://b.org)')


if __name__ == '__main__':
    unittest.main()
