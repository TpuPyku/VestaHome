#!/usr/bin/env python3
"""make_manual_pdf.py - the user's guides as PDF with their pictures, for build/ (the user 2026-11-08).

Each guide (Markdown) -> HTML (its pictures by their paths beside the .md, links inside the guide kept) -> PDF printed
by headless Edge, A4.

Usage: python VestaHome/tools/make_manual_pdf.py [out_dir]     (default: build/ at the top of the shared folder)
Needs: pip install markdown-it-py mdit-py-plugins
"""
import os
import re
import subprocess
import sys
import tempfile

from markdown_it import MarkdownIt                     # CommonMark, as GitHub shows the guides
from mdit_py_plugins.anchors import anchors_plugin

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.normpath(os.path.join(HERE, '..', '..'))   # the folder holding the repos
GUIDES = [   # (the guide, the PDF's name)
    ('VestaHome/docs/manual.md', 'VestaHome-manual.pdf'),
    ('OSMapsCE/README.md', 'OSMapsCE-manual.pdf'),
    ('OSMapsCE/README.en.md', 'OSMapsCE-manual-en.pdf'),
    ('VestaLaunch/README.md', 'VestaLaunch-manual.pdf'),
]
EDGE = r'C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe'
CSS = '''
@page { size: A4; margin: 16mm 15mm; }
body { font: 10.5pt/1.45 "Segoe UI", Arial, sans-serif; color: #1d1d1f; }
h1 { font-size: 21pt; margin: 0 0 6pt; color: #e8590c; }
h2 { font-size: 15pt; margin: 18pt 0 6pt; padding-bottom: 3pt; border-bottom: 1.5pt solid #e8590c;
     break-after: avoid; }
h3 { font-size: 12pt; margin: 12pt 0 4pt; break-after: avoid; }
p, li { orphans: 3; widows: 3; }
img { display: block; max-width: 58%; margin: 8pt auto; border: 0.5pt solid #bbb; border-radius: 4pt;
      break-inside: avoid; }
td img { max-width: 100%; }
table { border-collapse: collapse; width: 100%; margin: 6pt 0; font-size: 9.5pt; break-inside: avoid; }
th, td { border: 0.5pt solid #ccc; padding: 3pt 5pt; vertical-align: top; }
th { background: #f3f3f3; }
code { font: 9pt Consolas, monospace; background: #f3f3f3; padding: 0 2pt; border-radius: 2pt; }
pre { background: #f3f3f3; padding: 6pt; white-space: pre-wrap; font-size: 9pt; }
pre code { padding: 0; }
a { color: #c2410c; text-decoration: none; }
hr { display: none; }
'''


def slug(text):
    """GitHub's anchor of a heading (the guides link their own sections so): lower case, no punctuation, spaces -> -."""
    text = re.sub(r'[^\w\- ]', '', text.strip().lower())
    return text.replace(' ', '-')


def page(md_path):
    src = open(md_path, encoding='utf-8-sig').read()   # (a BOM would turn the first heading into text)
    md = MarkdownIt('commonmark').enable('table').use(anchors_plugin, max_level=3, slug_func=slug)
    body = md.render(src)
    base = 'file:///' + os.path.dirname(md_path).replace('\\', '/') + '/'
    title = re.search(r'^# (.+)$', src, re.M)
    return ('<!doctype html><html><head><meta charset="utf-8"><base href="%s"><title>%s</title><style>%s</style>'
            '</head><body>%s</body></html>' % (base, title.group(1) if title else '', CSS, body))


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(TOP, 'build')
    tmp = os.path.join(tempfile.gettempdir(), 'vesta_manual.html')   # (its pictures: by <base>, the guide's folder)
    for md, name in GUIDES:
        open(tmp, 'w', encoding='utf-8').write(page(os.path.join(TOP, md)))
        pdf = os.path.join(out, name)
        if os.path.exists(pdf):
            os.remove(pdf)
        subprocess.run([EDGE, '--headless', '--disable-gpu', '--no-pdf-header-footer', '--print-to-pdf=' + pdf,
                        'file:///' + tmp.replace('\\', '/')], check=False, timeout=120,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        print(name, 'ok' if os.path.exists(pdf) else 'FAILED')
    os.remove(tmp)


if __name__ == '__main__':
    main()
