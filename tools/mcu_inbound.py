"""mcu_inbound.py - the MCU messages seen in SPIPower.log files (the proxy's "SKEL id=.. msg=0x.... len=.. p=.."
lines): for each message id its skeleton(s), length(s), count and the most common first 4 payload bytes.

    python mcu_inbound.py <folder with HU log folders> [out.tsv]

Every SPIPower.log under the folder is read (UTF-16 or UTF-8). Output: tab-separated, sorted by id; it is the
"inbound" table of docs/MCU.md.
"""
import collections
import glob
import os
import re
import sys


def main():
    root = sys.argv[1]
    out = open(sys.argv[2], 'w', encoding='utf-8') if len(sys.argv) > 2 else sys.stdout
    stat = {}
    for f in sorted(glob.glob(os.path.join(root, '**', 'SPIPower.log'), recursive=True)):
        d = open(f, 'rb').read()
        t = d.decode('utf-16le', 'ignore') if d[1:2] == b'\x00' else d.decode('utf-8', 'ignore')
        for m in re.finditer(r'SKEL id=(\d+) msg=0x([0-9A-F]{4}) len=(\d+) p=((?:[0-9A-F]{2} ?){4})', t):
            e = stat.setdefault(m.group(2), {'skel': set(), 'len': set(), 'n': 0, 'p': collections.Counter()})
            e['skel'].add(int(m.group(1)))
            e['len'].add(int(m.group(3)))
            e['n'] += 1
            e['p'][m.group(4).strip()] += 1
    for k in sorted(stat):
        e = stat[k]
        out.write('0x%s\t%s\t%s\t%d\t%s\n' % (k, ','.join(map(str, sorted(e['skel']))),
                                              ','.join(map(str, sorted(e['len']))), e['n'],
                                              ' | '.join(p for p, _ in e['p'].most_common(4))))


if __name__ == '__main__':
    main()
