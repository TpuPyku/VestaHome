"""Tests of upg_extract.py: PE modules found in a flat image, named by their export directory."""
import os
import struct
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import upg_extract as u  # noqa: E402


def pe(dll_name=None, dll=True):
    """A PE32 of one section (file 0x200..0x400, RVA 0x1000); an export directory named dll_name when given."""
    b = bytearray(0x400)
    b[0:2] = b'MZ'
    struct.pack_into('<I', b, 0x3C, 0x80)
    b[0x80:0x84] = b'PE\0\0'
    struct.pack_into('<HHI', b, 0x84, 0x1C2, 1, 0x5D66113B)
    struct.pack_into('<HH', b, 0x94, 0xE0, 0x2000 if dll else 0x0002)
    opt = 0x98
    struct.pack_into('<H', b, opt, 0x10B)
    if dll_name:
        struct.pack_into('<III', b, opt + 92, 16, 0x1000, 0x40)
        struct.pack_into('<I', b, 0x200 + 12, 0x1000 + 0x40)       # its Name RVA
        b[0x240:0x240 + len(dll_name) + 1] = dll_name.encode() + b'\0'
    sec = opt + 0xE0
    b[sec:sec + 6] = b'.text\0'
    struct.pack_into('<IIII', b, sec + 8, 0x200, 0x1000, 0x200, 0x200)
    return bytes(b)


class TestModules(unittest.TestCase):
    def test_found_and_named(self):
        """Every PE in the image, its end, DLL or not, its export name; stray "MZ" bytes and gaps skipped."""
        a, b = pe('FLC_BLUETOOTH_SDK.dll'), pe(None, dll=False)
        img = b'\xff' * 16 + b'MZjunk' + a + b'\0' * 7 + b
        mods = u.modules(img)
        self.assertEqual(len(mods), 2)
        self.assertEqual(mods[0], (22, 22 + 0x400, True, 'FLC_BLUETOOTH_SDK.dll'))
        self.assertEqual(mods[1][2:], (False, ''))
        self.assertEqual(img[mods[1][0]:mods[1][1]], b)

    def test_names(self):
        self.assertEqual(u.file_name(0x1234, True, 'btapi.dll'), 'btapi.dll')
        self.assertEqual(u.file_name(0x1234, False, ''), 'mod_00001234.exe')
        self.assertEqual(u.file_name(0x1234, True, ''), 'mod_00001234.dll')

    def test_cut(self):
        """A PE header cut by the image's end: not taken."""
        self.assertEqual(u.modules(pe('x.dll')[:0x82]), [])


if __name__ == '__main__':
    unittest.main()
