"""Tests of adbwire.py: python test_adbwire.py (exit code: the failures)."""
import sys

import adbwire as w

failed = checks = 0


def check(ok, what):
    global failed, checks
    checks += 1
    if not ok:
        failed += 1
        print('FAIL:', what)


h = w.header(w.CNXN, w.VERSION, w.MAXDATA, b'host::\0')
check(len(h) == 24, 'a header: 24 bytes')
check(w.parse(h) == (w.CNXN, w.VERSION, w.MAXDATA, 7), 'parsed back: the command, its arguments, the length')
check(h[16:20] == (sum(b'host::\0')).to_bytes(4, 'little'), 'the data sum')
bad = bytearray(h)
bad[20] ^= 1
check(w.parse(bytes(bad)) is None, 'a wrong magic: not a header')
check(w.parse(h[:23]) is None, 'short: not a header')

# a known 2048-bit modulus (two primes' product is not needed for the encoding's maths: any odd n)
n = (1 << 2047) | (0x1234567 << 800) | 0xF1
t = w.pubkey(n, 65537, 'VestaTether@PC')
check(t.endswith(b' VestaTether@PC\0'), 'the name after a space, a zero at the end')
words, n0inv, n2, rr, e = w.pubkey_fields(t)
check(words == 64, '64 words')
check(n2 == n, 'the modulus back')
check((n0inv * (n & 0xFFFFFFFF)) & 0xFFFFFFFF == 0xFFFFFFFF, 'n0inv: -1 / n[0] mod 2^32')
check(rr == pow(2, 4096, n), 'rr: R^2 mod n, R = 2^2048')
check(e == 65537, 'the exponent')
print('%d checks, %d failed' % (checks, failed))
sys.exit(failed)
