# upg_build.py - build a Desay ".upg" package (SwDnl.upg) and verify the format.
#
# Reverse-engineered 2026-10-09 (see upg_parse.py):
#   .upg = [ file data blob ][ TOC: per-file "S2 S3 S4" TLV records ][ "S1" trailer ]
#   S2 = 28 bytes: offset_start:u64 LE, offset_end:u64 LE, size:u32 LE, unk:u32, cks:i32 LE
#   S3 = dir path (ASCII), S4 = file name (ASCII)
#   S1 = 24 bytes: data_end:u64, toc_end:u64, count:u32, reserved:u32
#   TLV frame = tag('S'+digit) + 0x00 + len:u16 LE + value
#
# The checksum is -sum(bytes interpreted as SIGNED 8-bit, accumulated into a 32-bit
# value) -- verified across all 2023 files of the factory package (0 mismatches):
#   cks = -( sum(x if x < 128 else x - 256 for x in data) )
import struct, sys

def compute_cks(data):
    return -(sum(x if x < 128 else x - 256 for x in data))

def enc_tlv(tag, value):
    return tag + b'\x00' + struct.pack('<H', len(value)) + value

def enc_s2(off0, off1, size, unk, cks):
    v = struct.pack('<QQIIi', off0, off1, size, unk, cks)
    return enc_tlv(b'S2', v)

def enc_s3(directory):
    return enc_tlv(b'S3', directory.encode('latin1'))

def enc_s4(name):
    return enc_tlv(b'S4', name.encode('latin1'))

def enc_s1(data_end, toc_end, count):
    v = struct.pack('<QQII', data_end, toc_end, count, 0)
    return enc_tlv(b'S1', v)

def parse_files(b, toc_start):
    import upg_parse as u
    recs = u.parse_toc(b, toc_start, len(b))
    files = []
    cur = {}
    for tag, val, pos in recs:
        if tag == 'S2':
            cur = {'off': struct.unpack_from('<Q', val, 0)[0],
                   'size': struct.unpack_from('<I', val, 16)[0],
                   'unk': struct.unpack_from('<I', val, 20)[0],
                   'cks': struct.unpack_from('<i', val, 24)[0]}
        elif tag == 'S3':
            cur['dir'] = val.decode('latin1')
        elif tag == 'S4':
            cur['name'] = val.decode('latin1')
            files.append(cur)
            cur = {}
        elif tag == 'S1':
            # trailer: (data_end, toc_end, count, reserved)
            cur['s1'] = (struct.unpack_from('<Q', val, 0)[0],
                         struct.unpack_from('<Q', val, 8)[0],
                         struct.unpack_from('<I', val, 16)[0],
                         struct.unpack_from('<I', val, 20)[0])
    return files

def build(files, data_by_name=None):
    """files: list of dicts with off/size/unk/cks/dir/name (from parse_files);
    data_by_name: optional {name: bytes} to override content (not used for verify)."""
    # data blob = file contents in order
    blob = b''
    toc = b''
    off = 0
    for f in files:
        blob += f['data']
        toc += enc_s2(off, off + len(f['data']), len(f['data']), f['unk'], f['cks'])
        toc += enc_s3(f['dir'])
        toc += enc_s4(f['name'])
        off += len(f['data'])
    s1 = enc_s1(off, off + len(toc), len(files))
    return blob + toc + s1

def verify(path, toc_start):
    b = open(path, 'rb').read()
    files = parse_files(b, toc_start)
    for f in files:
        f['data'] = b[f['off']:f['off'] + f['size']]
    rebuilt = build(files)
    print('original size %d, rebuilt size %d, identical=%s' %
          (len(b), len(rebuilt), rebuilt == b))
    return rebuilt == b

def make_minimal(out_path, name, data, directory, cks, unk=0):
    f = [{'data': data, 'dir': directory, 'name': name, 'cks': cks, 'unk': unk}]
    pkg = build(f)
    open(out_path, 'wb').write(pkg)
    return pkg

if __name__ == '__main__':
    # usage: upg_build.py verify <swdnl.upg> <toc_start_hex>
    if len(sys.argv) >= 4 and sys.argv[1] == 'verify':
        verify(sys.argv[2], int(sys.argv[3], 16))
