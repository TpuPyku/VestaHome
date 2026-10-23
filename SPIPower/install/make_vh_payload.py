#!/usr/bin/env python3
"""make_vh_payload.py - pack VestaHome (exe, Fonts, Skin, radio.txt, obd_codes.txt, VestaNMEA.dll, bass) into vh_payload.bin, embedded by VestaHomeInstall.

Format (the same as LaunchInstall's payload.bin):
    [u32 count][count x (u16 nameLen, name UTF-8 with '/', u32 dataLen, data)][u32 CRC-32 of all before]
Names are relative to \\SDMMC\\VestaHome: "VestaHome.exe", "Skin/home.png", "bass/bass.dll", ...; "sd/..." go to
\\SDMemory\\... and only when not there yet (the user widget example: VestaHome reads its widgets from the card).
The installer refuses a payload whose CRC does not match (a damaged copy of the installer).
The files are the ones make_tests.py puts into Tests\\SDMemory\\VestaHome; the user's files (VestaHome.ini, apps.txt,
speedcam\\) are never in it.

    python make_vh_payload.py        (after VestaHome\\native\\build.bat; install\\build.bat runs it)
"""
import os
import struct
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
VH = os.path.normpath(os.path.join(HERE, '..', '..', 'VestaHome', 'VestaHome'))       # the build output folder
BASS = os.path.normpath(os.path.join(HERE, '..', '..', 'third_party', 'bass-ce'))   # BASS 2.4.17 CE/ARM
BASS_FILES = ('bass.dll', 'bass_aac.dll', 'bassflac.dll', 'bassalac.dll', 'bass_mpc.dll', 'bassopus.dll', 'basswv.dll',
              'tags.dll')


def main():
    entries = [('VestaHome.exe', os.path.join(VH, 'VestaHome.exe')),
               ('radio.txt', os.path.join(VH, 'radio.txt')),
               ('obd_codes.txt', os.path.join(VH, 'obd_codes.txt')),   # tools/make_obd_codes.py
               ('VestaNMEA.dll', os.path.join(HERE, '..', '..', 'VestaNMEA', 'VestaNMEA', 'VestaNMEA.dll'))]   # COM6
    fonts = os.path.join(VH, 'Fonts')   # Golos Text (SIL OFL) + its licence: the text's own face
    for name in sorted(os.listdir(fonts)):
        entries.append(('Fonts/' + name, os.path.join(fonts, name)))
    skin = os.path.join(VH, 'Skin')
    for name in sorted(os.listdir(skin)):
        if os.path.isfile(os.path.join(skin, name)):
            entries.append(('Skin/' + name, os.path.join(skin, name)))
    for name in BASS_FILES:
        entries.append(('bass/' + name, os.path.join(BASS, name)))
    widgets = os.path.join(VH, '..', 'VestaWidgets')   # the example: the traffic score and the guide
    for name in ('probki.ini', 'README.txt'):
        entries.append(('sd/VestaWidgets/' + name, os.path.join(widgets, name)))
    out = bytearray(struct.pack('<I', len(entries)))
    for name, path in entries:
        data = open(path, 'rb').read()
        nb = name.encode('utf-8')
        out += struct.pack('<H', len(nb)) + nb + struct.pack('<I', len(data)) + data
    out += struct.pack('<I', zlib.crc32(bytes(out)) & 0xFFFFFFFF)
    path = os.path.join(HERE, 'vh_payload.bin')
    open(path, 'wb').write(out)
    print('vh_payload.bin: %d files, %d bytes' % (len(entries), len(out)))


if __name__ == '__main__':
    main()
