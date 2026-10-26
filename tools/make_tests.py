#!/usr/bin/env python3
"""Lay out <root>\\Tests: the files to try on the head unit, one folder per HU volume.

    Tests\\SDMemory\\<App>\\     ->  \\SDMemory\\<App>\\  (external SD card: experiments go here)
    Tests\\SDMMC\\<App>\\        ->  \\SDMMC\\<App>\\     (built-in card: only what the shell itself starts)
    Tests\\ResidentFlash\\...   ->  \\ResidentFlash\\... (built-in partition, the shell's home)
    Tests\\StorageFlash\\...    ->  \\StorageFlash\\...  (built-in memory; nothing of ours yet)
    Tests\\USBDevice\\...       ->  \\USBDevice\\...     (USB flash drive; nothing of ours yet)

The repositories stay as they are: each build.bat still writes into its own output folder (YaMapsCE\\,
VestaHome\\VestaHome\\ ...), and this script copies the CE exe and what it needs from there. PC builds,
*.ini (API keys), logs, caches and test tracks are not copied. SDMemory\\SafeMode and
SDMemory\\SPIPower_rescue hold files for the ROOT of the card, not a folder (see Tests\\README.md).
The SPIPower set is laid out by VestaHome/SPIPower/tools/make_deploy.py (md5-checked), called from here.
Large third-party payloads that have no build ("GpsGate 2.0") live only in Tests:
only our own files are copied over them.

Usage: python VestaHome/tools/make_tests.py
"""
import importlib.util
import os
import shutil
import subprocess
import sys

ROOT  = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
TESTS = os.path.join(ROOT, 'Tests')
V     = os.path.join(ROOT, 'VestaHome')
VL    = os.path.join(ROOT, 'VestaLaunch')   # its own repo since 2026-10-13, with LaunchInstall

# (source folder, Tests subfolder, (entries)); an entry is a file or folder name in the source folder,
# or (source name, name in Tests).
PLAN = (
    (os.path.join(ROOT, 'YaMaps', 'YaMapsCE'), r'SDMMC\YaMapsCE',
        ('YaMapsCE.exe', 'toolhelp.dll', 'Images', 'GpsGate')),
    (os.path.join(ROOT, 'OSMapsCE', 'OSMapsCE'), r'SDMMC\OSMapsCE',
        ('OSMapsCE.exe', 'Images')),   # toolhelp.dll: the firmware's own (\Windows) is used
    # the vector tiles speed test (OSMapsCE/native_vtest; its tiles from get_tiles.py), run from the SD card
    (os.path.join(ROOT, 'OSMapsCE', 'VTileTest'), r'SDMemory\VTileTest',
        ('VTileTest.exe', 'tiles')),
    # ExternNavi.cfg points the shell's "Navigation" at \SDMMC\VestaLaunch: without the SD card the shell would
    # lose it, so VestaLaunch stays on the built-in card
    (os.path.join(VL, 'VestaLaunch'), r'SDMMC\VestaLaunch',
        ('VestaLaunch.exe', 'Skin')),
    # VestaHome itself is installed by VestaHomeInstall (SDMemory\VestaHomeInstall, from make_deploy.py)
    # short audio/video files for VestaHome's "Diagnostics -> Sound and video" (made by make_testmedia.py)
    (os.path.join(V, 'VestaHome', 'VestaHome'), r'SDMemory',
        ('TestMedia',)),
    # the user widgets' examples and their guide (VestaHome reads \SDMemory\VestaWidgets\*.ini)
    (os.path.join(V, 'VestaHome'), r'SDMemory',
        ('VestaWidgets',)),
    (os.path.join(VL, 'LaunchInstall', 'LaunchInstall'), r'SDMemory\LaunchInstall',
        ('LaunchInstall.exe',)),
    # the sample add-on (VestaCore/plugins/sample): VestaHome starts Plugins\<Name>\<Name>.exe next to itself
    (os.path.join(ROOT, 'VestaCore', 'plugins', 'sample', 'out'), r'SDMMC\VestaHome\Plugins\Sample',
        ('Sample.exe',)),
)


def copy(src, dst):
    if os.path.isdir(src):
        for name in os.listdir(src):
            copy(os.path.join(src, name), os.path.join(dst, name))
        return 0
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    shutil.copy2(src, dst)
    return os.path.getsize(dst)


def main():
    if not os.path.isdir(os.path.join(V, 'VestaHome', 'VestaHome', 'TestMedia')):   # generated once (needs ffmpeg)
        subprocess.call([sys.executable, os.path.join(V, 'tools', 'make_testmedia.py')])
    for src_dir, sub, entries in PLAN:
        print('[%s]' % sub)
        for e in entries:
            name, to = e if isinstance(e, tuple) else (e, e)
            src = os.path.join(src_dir, name)
            if not os.path.exists(src):
                print('  SKIPPED, not built: %s' % src)
                continue
            size = copy(src, os.path.join(TESTS, sub, to))
            print('  %-32s %s' % (to + ('\\' if os.path.isdir(src) else ''), size or ''))
    print()
    spec = importlib.util.spec_from_file_location(
        'make_deploy', os.path.join(V, 'SPIPower', 'tools', 'make_deploy.py'))
    md = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(md)
    md.main(TESTS)
    shutil.copy2(os.path.join(V, 'tools', 'tests_README.md'), os.path.join(TESTS, 'README.md'))
    print('done: %s' % TESTS)


if __name__ == '__main__':
    sys.exit(main())
