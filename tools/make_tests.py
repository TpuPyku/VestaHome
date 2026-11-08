#!/usr/bin/env python3
"""Lay out <root>\\Tests: the files to try on the head unit.

    Tests\\SDCard\\<App>\\      ->  a memory card: \\SDMemory\\<App>\\ (the external one) by default
    Tests\\ResidentFlash\\...  ->  \\ResidentFlash\\... (the built-in partition: VHContent, VestaHome's widgets and add-ons)

The repositories stay as they are: each build.bat still writes into its own output folder, and this script copies
the CE exe and what it needs from there. PC builds, *.ini (API keys), logs, caches and test tracks are not copied.
SDCard\\SafeMode, SDCard\\SPIPower_rescue and SDCard\\Updater hold files for the ROOT of the card, not a folder (see
Tests\\README.md). The SPIPower set is laid out by VestaHome/SPIPower/tools/make_deploy.py (md5-checked), called from here.
Everything in Tests is generated: the old places are removed, nothing is kept by hand.

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
    (os.path.join(ROOT, 'OSMapsCE', 'OSMapsCE'), r'SDCard\OSMapsCE',
        ('OSMapsCE.exe', 'Images', 'Fonts', 'Lang', 'Voice')),   # toolhelp.dll: the firmware's own (\Windows) is used; Fonts: Golos (OFL); Voice: the prompts
    # GpsSim: a drive in place of the GPS (a unit on a desk; a Shturmann's OSMapsCE too), run from the card
    (os.path.join(ROOT, 'VestaHome', 'GpsSim', 'GpsSim'), r'SDCard\GpsSim', ('GpsSim.exe',)),
    # VestaHome is installed by VestaHomeInstall (SDCard\VestaHomeInstall, from make_deploy.py), VestaLaunch by LaunchInstall
    # short audio / video files and photos to try VestaHome's players and photo viewer on (make_testmedia.py)
    (os.path.join(V, 'VestaHome', 'VestaHome'), r'SDCard',
        ('TestMedia',)),
    # the user widgets' examples and their guide (VestaHome reads \ResidentFlash\VHContent\VHWidgets\*.ini)
    (os.path.join(V, 'VestaHome', 'VestaWidgets'), r'ResidentFlash\VHContent\VHWidgets',
        ('README.txt', 'demo.ini', 'demo.txt', 'navi.ini', 'navi.mscr', 'probki.ini')),
    (os.path.join(VL, 'LaunchInstall', 'LaunchInstall'), r'SDCard\LaunchInstall',
        ('LaunchInstall.exe',)),
    # the installers through the stock shell's "Navigation" (it runs exactly \SDMemory\UPDATE\Updater.exe): the
    # CONTENTS of SDCard\Updater go to the card's root. Updater.exe + MortScript.exe + mortzip.dll - CityGuide's
    # MortScript (third-party, MAIN\UPDATE), the script ours (VestaHome\Updater\Updater.mscr)
    (os.path.join(ROOT, 'MAIN', 'UPDATE'), r'SDCard\Updater\UPDATE',
        ('Updater.exe', 'MortScript.exe', 'mortzip.dll')),
    (os.path.join(V, 'Updater'), r'SDCard\Updater\UPDATE',
        ('Updater.mscr',)),
    (os.path.join(V, 'SPIPower', 'out'), r'SDCard\Updater\UPDATE',
        ('VestaHomeInstall.exe',)),
    (os.path.join(VL, 'LaunchInstall', 'LaunchInstall'), r'SDCard\Updater\UPDATE',
        ('LaunchInstall.exe',)),
    (os.path.join(V, 'VestaTether', 'VestaTether'), r'SDCard\TetherProbe',
        ('TetherProbe.exe', 'VestaTether.dll')),
    # the sample add-on (VestaCore/plugins/sample): VestaHome starts VHContent\VHPlugins\<Name>\<Name>.exe
    (os.path.join(ROOT, 'VestaCore', 'plugins', 'sample', 'out'), r'ResidentFlash\VHContent\VHPlugins\Sample',
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


# places of Tests no longer made: removed, so the folder has no stale copies (the user 2026-11-06: one SDCard for the
# two cards; gone with them: GpsGate 2.0 - an old GPS test, MAIN has it; YaMapsCE - finished, its archive in build\;
# VestaLaunch - inside LaunchInstall)
RETIRED = (r'SDMMC', r'SDMemory', r'StorageFlash', r'USBDevice', r'SDCard\VestaWidgets', r'ResidentFlash\VestaHome')


def main():
    for sub in RETIRED:
        old = os.path.join(TESTS, sub)
        if os.path.isdir(old):
            shutil.rmtree(old)
            print('removed (retired): %s' % sub)
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
