"""ADB's wire format, the host's side (as native/adb.cpp on the unit): message headers and the public key in Android's
mincrypt form. Plain bytes, no USB: tested by test_adbwire.py."""
import base64
import struct

CNXN, AUTH, OPEN, OKAY, CLSE, WRTE = 0x4E584E43, 0x48545541, 0x4E45504F, 0x59414B4F, 0x45534C43, 0x45545257
AUTH_TOKEN, AUTH_SIGNATURE, AUTH_RSAPUBLICKEY = 1, 2, 3
VERSION, MAXDATA = 0x01000000, 4096
NAMES = {CNXN: 'CNXN', AUTH: 'AUTH', OPEN: 'OPEN', OKAY: 'OKAY', CLSE: 'CLSE', WRTE: 'WRTE'}


def header(cmd, a0, a1, data=b''):
    """The 24 bytes before a message's data: command, two arguments, length, sum of the data's bytes, the magic."""
    return struct.pack('<6I', cmd, a0, a1, len(data), sum(data) & 0xFFFFFFFF, cmd ^ 0xFFFFFFFF)


def parse(h):
    """A header -> (cmd, a0, a1, len), or None when it is not one."""
    if len(h) != 24:
        return None
    cmd, a0, a1, n, _, magic = struct.unpack('<6I', h)
    if magic != cmd ^ 0xFFFFFFFF or n > MAXDATA * 64:
        return None
    return cmd, a0, a1, n


def pubkey(n, e, name):
    """The public key as the phone stores it: mincrypt's RSAPublicKey (64 words of the modulus little endian, n0inv,
    R^2 mod n), base64, a space, a name, a zero byte."""
    words = 64
    r = 1 << (32 * words)
    n0inv = (-pow(n, -1, 1 << 32)) & 0xFFFFFFFF
    rr = (r * r) % n
    blob = struct.pack('<II', words, n0inv)
    blob += b''.join(struct.pack('<I', (n >> (32 * i)) & 0xFFFFFFFF) for i in range(words))
    blob += b''.join(struct.pack('<I', (rr >> (32 * i)) & 0xFFFFFFFF) for i in range(words))
    blob += struct.pack('<i', e)
    return base64.b64encode(blob) + b' ' + name.encode() + b'\0'


def pubkey_fields(text):
    """pubkey's text back -> (words, n0inv, n, rr, e) (for the tests)."""
    blob = base64.b64decode(text.split(b' ')[0])
    words, n0inv = struct.unpack_from('<II', blob)
    n = sum(struct.unpack_from('<I', blob, 8 + 4 * i)[0] << (32 * i) for i in range(words))
    rr = sum(struct.unpack_from('<I', blob, 8 + 4 * (words + i))[0] << (32 * i) for i in range(words))
    e = struct.unpack_from('<i', blob, 8 + 8 * words)[0]
    return words, n0inv, n, rr, e
