# upg_parse.py - parse a Desay ".upg" firmware package (SwDnl.upg) and list its files.
#
# Format (reverse-engineered 2026-10-09):
#   [ file data blob ][ trailing "File Location Table" (TOC) ]
# TOC is a flat stream of TLV records, tag = 'S'+digit, then 0x00, then 16-bit LE
# length, then the value:
#   S3 = directory path (ASCII, length-prefixed)
#   S4 = file name     (ASCII, length-prefixed)
#   S2 = 28-byte file location: offset_start:u64 LE, offset_end:u64 LE,
#        size:u32 LE, unk:u32, checksum:i32 LE
#   S1 = 24-byte ??? (rare)
import sys, struct

def read_tlv(b, pos, end):
    tag = b[pos:pos+2]
    if len(tag) < 2 or tag[0] != 0x53 or tag[1] not in (0x31,0x32,0x33,0x34):
        return None
    if b[pos+2] != 0:
        return None
    ln = struct.unpack_from('<H', b, pos+3)[0]
    val = b[pos+5:pos+5+ln]
    return (tag.decode('latin1'), val, pos+5+ln)

def parse_toc(b, start, end):
    recs = []
    pos = start
    while pos < end:
        r = read_tlv(b, pos, end)
        if r is None:
            pos += 1
            continue
        tag, val, npos = r
        recs.append((tag, val, pos))
        pos = npos
    return recs

def dec_s2(v):
    off0 = struct.unpack_from('<Q', v, 0)[0]
    off1 = struct.unpack_from('<Q', v, 8)[0]
    size = struct.unpack_from('<I', v, 16)[0]
    unk  = struct.unpack_from('<I', v, 20)[0]
    cks  = struct.unpack_from('<i', v, 24)[0]
    return off0, off1, size, unk, cks

def main(path, start_hex, end_hex):
    b = open(path, 'rb').read()
    start = int(start_hex, 16) if start_hex else 0
    end   = int(end_hex, 16) if end_hex else len(b)
    recs = parse_toc(b, start, end)
    print('parsed %d tlv records from 0x%X to 0x%X' % (len(recs), start, end))
    for tag, val, pos in recs:
        if tag == 'S3':
            print('%08X S3  dir = %r' % (pos, val))
        elif tag == 'S4':
            print('%08X S4  name = %r' % (pos, val))
        elif tag == 'S2':
            o0,o1,sz,unk,ck = dec_s2(val)
            print('%08X S2  off0=0x%X off1=0x%X size=%d unk=%d cks=%d' % (pos,o0,o1,sz,unk,ck))
        elif tag == 'S1':
            print('%08X S1  %s' % (pos, val.hex()))
        else:
            print('%08X %s  %s' % (pos, tag, val.hex()))

if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2] if len(sys.argv)>2 else None, sys.argv[3] if len(sys.argv)>3 else None)
