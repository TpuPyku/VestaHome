#!/usr/bin/env python3
"""make_shots.py - the screens of VestaHome for the user's guide (docs/manual-outline.md), from the PC build.

Starts VestaHome/VestaHome/VestaHomePC.exe (build it first), stands in for the Bluetooth proxy (a phone connected,
its book and call log: the shared memories VestaBT5 / VestaBTPB1, demo data) and the MCU proxy (the radio on
FM 101.2 with RDS and presets: VestaMCU2), plays a made-up drive as the GPS (shots.nmea) and walks the screens by taps,
saving what the window drew into docs/shots/<NN>_<name>.png (not in git: made again on demand). The window's
VestaHome.ini is kept and put back. Windows only (ctypes, Pillow).

Usage: python VestaHome/tools/make_shots.py [out_dir]   (VH_SHOTS=name,name... - only those)
"""
import ctypes
import json
import os
import shutil
import subprocess
import sys
import threading
import time
from ctypes import wintypes as W

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
VH = os.path.join(HERE, '..', 'VestaHome', 'VestaHome')
EXE = os.path.join(VH, 'VestaHomePC.exe')
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'docs', 'shots')

# the nav bar and the settings' sections (800 x 480)
HOME, RADIO, MEDIA, PHONE, APPS, SETTINGS = (60, 440), (170, 440), (282, 440), (394, 440), (506, 440), (618, 440)
SET = {'general': (60, 90), 'sound': (170, 90), 'screen': (60, 160), 'radio': (170, 160), 'bt': (60, 230),
       'wifi': (170, 230), 'video': (60, 300), 'net': (170, 300), 'car': (60, 370), 'info': (170, 370)}   # (video:
       # "Медиа" - video and photo; net: "Сеть", 2026-10-30)
TAB = {'music': (62, 88), 'net': (160, 88), 'bt': (260, 88), 'aux': (360, 88), 'video': (458, 88),
       'photo': (557, 88), 'list': (760, 88)}
SHOTS = [
    ('home', [HOME]), ('home_page2', [HOME, ('swipe',)]), ('home_page3', [HOME, ('swipe',)]),   # the page stays: from page 2
    ('radio', [RADIO]), ('radio_list', [RADIO, (712, 98)]), ('radio_pty', [RADIO, (594, 98)]),
    ('music', [MEDIA, TAB['music']]), ('music_list', [MEDIA, TAB['music'], TAB['list']]),
    ('netradio', [MEDIA, TAB['net']]), ('netradio_list', [MEDIA, TAB['net'], TAB['list']]),
    ('bt_music', [MEDIA, TAB['bt']]), ('aux', [MEDIA, TAB['aux']]), ('video', [MEDIA, TAB['video']]),
    ('photo', [MEDIA, TAB['photo']]),
    ('phone', [PHONE]), ('contacts', [PHONE, (100, 376), ('sleep', 3)]),
    ('contact', [PHONE, (100, 376), ('sleep', 3), (400, 150)]), ('favourites', [PHONE, (100, 376), (88, 260)]),
    ('calls', [PHONE, (285, 376), ('sleep', 3)]),
    ('apps', [APPS]), ('tasks', [APPS, (445, 28), ('sleep', 2)]),
] + [('settings_' + k, [SETTINGS, v]) for k, v in SET.items()] + [
    ('settings_bt_paired', [SETTINGS, SET['bt'], (400, 328)]),
    ('settings_wifi_saved', [SETTINGS, SET['wifi'], (710, 372)]),
    ('set_plugins', [SETTINGS, SET['general'], ('sleep', 2), (400, 376)]),
    ('plugin_screen', [SETTINGS, SET['general'], ('sleep', 2), (400, 376), (614, 92)]),
    ('plugin_tap', [SETTINGS, SET['general'], ('sleep', 2), (400, 376), (614, 92), ('sleep', 2), (744, 208), ('sleep', 1), (744, 208), ('sleep', 1), (400, 96), ('sleep', 1)]),
    ('home_widgets_p2', [SETTINGS, SET['general'], ('sleep', 3), (400, 295), (756, 440)]),
    ('home_page4', [HOME, ('swipe',), ('sleep', 1), ('swipe',), ('sleep', 1), ('swipe',), ('sleep', 3)]),
    ('plugin_nav', [HOME, ('sleep', 3)]),
    ('plugin_nav_open', [HOME, ('sleep', 3), (547, 448), ('sleep', 1)]),
    ('plugin_own', [HOME, ('sleep', 3), (547, 448), ('sleep', 1), (400, 376), ('sleep', 1), (400, 376), ('sleep', 2)]),
    ('plugin_back', [HOME, ('sleep', 3), (547, 448), ('sleep', 1), (400, 376), ('sleep', 1), (44, 446), ('sleep', 1)]),
    ('settings_home_widgets', [SETTINGS, SET['general'], (400, 328)]),
    ('info_more', [SETTINGS, SET['info'], (355, 355)]), ('diagnostics', [SETTINGS, SET['info'], (590, 355)]),
    ('licenses', [SETTINGS, SET['info'], (355, 355), (627, 433)]),
    ('power', [SETTINGS, SET['info'], (728, 356)]),
    ('car_sensors', [HOME, ('swipeback',), ('sleep', 2), (130, 330)]),   # from page 3 to 2: the car widget
    ('car_widget_set', [HOME, ('sleep', 2), (240, 256)]),
    ('speed_set', [HOME, (760, 86)]),   # (home's page 1: run alone - VH_SHOTS=speed_set,weather_set,camdl)
    ('weather_set', [HOME, (760, 256)]), ('camdl', [HOME, (760, 86), (400, 440)]),
    # the internet radio's catalogue (radio-browser.info: needs the internet), the list's tab "Каталог"
    ('netradio_catalog', [MEDIA, TAB['net'], TAB['list'], (470, 84)]),
    ('netradio_genres', [MEDIA, TAB['net'], TAB['list'], (470, 84), (660, 180), ('sleep', 6)]),
    ('netradio_genre', [MEDIA, TAB['net'], TAB['list'], (470, 84), (660, 180), ('sleep', 6), (300, 104), ('sleep', 6)]),
    ('netradio_top_star', [MEDIA, TAB['net'], TAB['list'], (470, 84), (140, 180), ('sleep', 6), (674, 148)]),
    ('netradio_more', [MEDIA, TAB['net'], TAB['list'], (470, 84), (140, 180), ('sleep', 6), (520, 438), ('sleep', 6)]),
]

if os.environ.get('VH_SHOTS'):   # only these (comma-separated names), from a fresh start
    SHOTS = [st for st in SHOTS if st[0] in os.environ['VH_SHOTS'].split(',')]

k32 = ctypes.windll.kernel32
u32 = ctypes.windll.user32
g32 = ctypes.windll.gdi32
u32.SetProcessDPIAware()
k32.CreateFileMappingW.restype = W.HANDLE
k32.MapViewOfFile.restype = ctypes.c_void_p


class BtSt(ctypes.Structure):
    _fields_ = [('ready', ctypes.c_int), ('av', ctypes.c_int), ('avrcp', ctypes.c_int), ('hf', ctypes.c_int),
                ('hfAudio', ctypes.c_int), ('lengthMs', ctypes.c_int), ('posMs', ctypes.c_int),
                ('playing', ctypes.c_int), ('battery', ctypes.c_int), ('signal', ctypes.c_int),
                ('devAddr', ctypes.c_char * 18), ('devName', ctypes.c_char * 128), ('paired', ctypes.c_int),
                ('pairedAddr', (ctypes.c_char * 18) * 16), ('pairedName', (ctypes.c_char * 48) * 16),
                ('title', ctypes.c_char * 128), ('artist', ctypes.c_char * 128), ('album', ctypes.c_char * 128),
                ('trackNo', ctypes.c_int), ('trackCount', ctypes.c_int), ('durationMs', ctypes.c_int),
                ('call', ctypes.c_int * 2), ('callNumber', ctypes.c_char * 32), ('operatorName', ctypes.c_char * 32),
                ('callTick', W.DWORD), ('avTick', W.DWORD), ('playTick', W.DWORD), ('mediaTick', W.DWORD),
                ('devTick', W.DWORD), ('pan', ctypes.c_int), ('panTick', W.DWORD)]


class BtEv(ctypes.Structure):
    _fields_ = [('seq', ctypes.c_long), ('tick', W.DWORD), ('code', ctypes.c_int), ('data', ctypes.c_ubyte * 64)]


class BtShm(ctypes.Structure):
    _fields_ = [('magic', W.DWORD), ('version', W.DWORD), ('seq', ctypes.c_long), ('evSeq', ctypes.c_long),
                ('attach', ctypes.c_long), ('inits', ctypes.c_long), ('origOk', ctypes.c_long), ('st', BtSt),
                ('cmdSeq', ctypes.c_long), ('cmd', ctypes.c_int), ('cmdText', ctypes.c_char * 40),
                ('cmdDone', ctypes.c_long), ('cmdResult', ctypes.c_int), ('ev', BtEv * 64)]


class PbE(ctypes.Structure):
    _fields_ = [('name', ctypes.c_char * 64), ('num', (ctypes.c_char * 24) * 3), ('numKind', ctypes.c_ubyte * 3),
                ('callType', ctypes.c_ubyte), ('time', ctypes.c_char * 20)]


class Pb(ctypes.Structure):
    _fields_ = [('magic', W.DWORD), ('seq', ctypes.c_long), ('storage', ctypes.c_int), ('list', ctypes.c_int),
                ('state', ctypes.c_int), ('count', ctypes.c_int), ('total', ctypes.c_int), ('pbap', ctypes.c_int),
                ('tick', W.DWORD), ('e', PbE * 1000)]


def mapping(cls, name):
    h = k32.CreateFileMappingW(W.HANDLE(-1), None, 4, 0, ctypes.sizeof(cls), name)
    return cls.from_address(k32.MapViewOfFile(h, 0xF001F, 0, 0, ctypes.sizeof(cls)))


NAMES = ['Алексей Петров', 'Анна', 'Борис Иванович', 'Вера Сергеевна', 'Гараж', 'Дмитрий', 'Евгения', 'Игорь',
         'Катя', 'Лена', 'Мама', 'Николай', 'Ольга', 'Папа', 'Сервис', 'Такси']


def bluetooth():   # the proxy's stand-in: a phone connected, playing; the book and the calls when asked
    m = mapping(BtShm, 'VestaBT5')
    pb = mapping(Pb, 'VestaBTPB1')
    m.magic, m.version, m.origOk = 0x54425456, 5, 1
    s = m.st
    s.ready, s.av, s.avrcp, s.hf, s.battery, s.signal = 1, 2, 1, 2, 4, 5
    s.devAddr, s.devName, s.operatorName = b'A4:E2:87:98:D6:9F', 'Телефон'.encode(), b'OPERATOR'
    s.lengthMs, s.posMs, s.playing, s.playTick = 225000, 61000, 1, k32.GetTickCount()
    s.title, s.artist = 'Звезда по имени Солнце'.encode(), 'Кино'.encode()
    s.paired = 3
    for i, (a, n) in enumerate([(b'A4:E2:87:98:D6:9F', 'Телефон'), (b'40:A7:86:83:8F:E4', 'Планшет'),
                                (b'00:1D:A5:68:98:8B', 'OBDII')]):
        s.pairedAddr[i].value, s.pairedName[i].value = a, n.encode()
    m.seq = 2
    pb.magic, pb.pbap = 0x54425456, -1
    last = m.cmdSeq
    while True:
        time.sleep(0.2)
        if m.cmdSeq == last:
            continue
        last = m.cmdSeq
        if m.cmd == 25:   # BT_CMD_PBAP_PULL "storage list"
            storage, lst = (int(x) for x in m.cmdText.split())
            pb.state, pb.list, pb.storage = 2, lst, storage
            if lst == 2:
                for i, n in enumerate(NAMES):
                    e = pb.e[i]
                    e.name, e.num[0].value = n.encode(), ('+7900%07d' % (1000000 + i * 3711)).encode()
                pb.count = pb.total = len(NAMES)
            else:
                for i in range(12):
                    e = pb.e[i]
                    e.name = (NAMES[i * 5 % 16] if i % 4 else '').encode()
                    e.num[0].value = ('+7900%07d' % (2000000 + i * 555)).encode()
                    e.callType, e.time = i % 3, ('202610%02dT%02d%02d00' % (21 - i // 3, (18 - i) % 24, i * 7 % 60)).encode()
                pb.count = pb.total = 12
            pb.state = 3
            pb.seq += 1
        elif m.cmd == 27:   # BT_CMD_GETOPTS
            m.cmdResult = 1
        m.cmdDone = m.cmdSeq


class McuMsg(ctypes.Structure):   # vh_mcu.inc / SPIPower/native/mcushm.h
    _fields_ = [('seq', ctypes.c_long), ('tick', W.DWORD), ('id', ctypes.c_ushort), ('len', ctypes.c_ubyte),
                ('skel', ctypes.c_ubyte), ('data', ctypes.c_ubyte * 40)]


class McuShm(ctypes.Structure):
    _fields_ = [('magic', W.DWORD), ('version', W.DWORD), ('seq', ctypes.c_long), ('attach', ctypes.c_long),
                ('wraps', ctypes.c_long), ('reserved', W.DWORD * 3), ('msg', McuMsg * 256)]


def fm_names(first, names):   # 0x0930: four station names of 8 characters from <first>
    return [0, first, first + len(names) - 1, 0] + list(b''.join(n.ljust(8).encode() for n in names))


def mcu():   # the MCU proxy's stand-in: the tuner on FM1 101.2 with RDS, its presets, the volume
    m = mapping(McuShm, 'VestaMCU2')
    m.magic, m.version = 0x55434D56, 2
    freqs = [8790, 9160, 9580, 9900, 10120, 10380, 10570, 10780]
    names = ['RETRO', 'SPORT', 'MAXIMUM', 'NASHE', 'DFM', 'MONTE', 'AVTO', 'HIT FM']
    rt = 'Dance Radio - Dua Lipa - Levitating'.ljust(64).encode()
    msgs = [(0x0908, [0, 1, 0, 0, 8] + sum([[f >> 8, f & 0xFF] for f in freqs], [])),
            (0x0908, [0, 0, 0, 0, 8] + sum([[f >> 8, f & 0xFF] for f in freqs], [])),
            (0x0930, fm_names(0, names[:4])), (0x0930, fm_names(4, names[4:])),
            (0x0901, [0, 0, 10120 >> 8, 10120 & 0xFF]), (0x0907, [0, 0, 4]),
            (0x090B, [0, 0, 0] + list(b'  DFM   ')),
            (0x0931, [0, 0, 0, 0] + list(rt[:32])), (0x0931, [0, 0, 0, 1] + list(rt[32:])),
            (0x3101, [0, 0, 14]), (0x4904, [0, 0])]   # the volume; the source: the radio
    for i, (mid, data) in enumerate(msgs):
        e = m.msg[i]
        e.tick, e.id, e.len = k32.GetTickCount(), mid, len(data)
        e.data[:len(data)] = data
        e.seq = i + 1
    m.seq = len(msgs)
    while True:   # keep the mapping (and the radio's state) while VestaHome runs
        time.sleep(1)


def track(path):   # a drive for the GPS replay (gps.port=<name>.nmea): north-east from Samara at 62 km/h
    def nmea(s):
        c = 0
        for ch in s.encode():
            c ^= ch
        return '$%s*%02X\r\n' % (s, c)

    def deg(v, w):   # ddmm.mmmm
        d = int(v)
        return ('%0' + str(w) + 'd%07.4f') % (d, (v - d) * 60)

    lat, lon = 53.2001, 50.1500
    with open(path, 'w', newline='') as f:
        for i in range(3600):
            t = '%02d%02d%02d.00' % (16 + i // 3600, i // 60 % 60, i % 60)
            la, lo = deg(lat, 2), deg(lon, 3)
            f.write(nmea('GPGGA,%s,%s,N,%s,E,1,09,0.9,62.0,M,0.0,M,,' % (t, la, lo)))
            f.write(nmea('GPRMC,%s,A,%s,N,%s,E,33.5,48.0,311026,,,A' % (t, la, lo)))
            lat += 0.000115
            lon += 0.000190


def grab(h, path):   # what the window drew (its DC)
    rc = W.RECT()
    u32.GetClientRect(h, ctypes.byref(rc))
    w, hh = rc.right, rc.bottom
    hdc = u32.GetDC(h)
    mdc = g32.CreateCompatibleDC(hdc)
    bmp = g32.CreateCompatibleBitmap(hdc, w, hh)
    g32.SelectObject(mdc, bmp)
    g32.BitBlt(mdc, 0, 0, w, hh, hdc, 0, 0, 0x00CC0020)

    class BIH(ctypes.Structure):
        _fields_ = [('biSize', ctypes.c_uint32), ('biWidth', ctypes.c_int32), ('biHeight', ctypes.c_int32),
                    ('biPlanes', ctypes.c_uint16), ('biBitCount', ctypes.c_uint16),
                    ('biCompression', ctypes.c_uint32), ('rest', ctypes.c_uint32 * 5)]
    bi = BIH(40, w, -hh, 1, 32, 0)
    buf = ctypes.create_string_buffer(w * hh * 4)
    g32.GetDIBits(mdc, bmp, 0, hh, buf, ctypes.byref(bi), 0)
    Image.frombuffer('RGBA', (w, hh), buf, 'raw', 'BGRA', 0, 1).convert('RGB').save(path)
    g32.DeleteObject(bmp)
    g32.DeleteDC(mdc)
    u32.ReleaseDC(h, hdc)


def main():
    if not os.path.exists(EXE):
        sys.exit('no %s: build VestaHome first' % EXE)
    os.makedirs(OUT, exist_ok=True)
    ini = os.path.join(VH, 'VestaHome.ini')
    saved = open(ini, 'rb').read() if os.path.exists(ini) else None
    nmea = os.path.join(VH, 'shots.nmea')   # (*.nmea: not in git)
    track(nmea)
    lines = [l for l in (saved or b'').split(b'\r\n') if not l.startswith(b'gps.port=')]
    open(ini, 'wb').write(b'\r\n'.join([b'gps.port=shots.nmea'] + lines))
    threading.Thread(target=bluetooth, daemon=True).start()
    threading.Thread(target=mcu, daemon=True).start()
    p = subprocess.Popen([EXE], cwd=VH)
    try:
        time.sleep(4)
        h = u32.FindWindowW('VestaHome_Wnd', None)

        def msg(m, x, y, wp=0):
            u32.SendMessageW(h, m, wp, (y << 16) | (x & 0xFFFF))

        def tap(x, y):
            msg(0x201, x, y, 1)
            time.sleep(0.05)
            msg(0x202, x, y)
            time.sleep(0.7)

        for n, (name, steps) in enumerate(SHOTS):
            h = u32.FindWindowW('VestaHome_Wnd', None)
            if not h or not u32.IsWindowVisible(h) or p.poll() is not None:
                print('%02d %s: the window is gone (%s)' % (n + 1, name, 'exited' if p.poll() is not None else 'hidden'))
                break
            for st in steps:
                if st[0] == 'sleep':
                    time.sleep(st[1])
                elif st[0] in ('swipe', 'swipeback'):   # to the next / the previous page
                    xs = (550, 450, 300, 200) if st[0] == 'swipe' else (250, 350, 500, 600)
                    msg(0x201, xs[0] + (50 if st[0] == 'swipe' else -50), 250, 1)
                    for x in xs:
                        msg(0x200, x, 250, 1)
                        time.sleep(0.05)
                    msg(0x202, xs[-1], 250)
                    time.sleep(1.2)
                else:
                    tap(*st)
            time.sleep(0.8)
            grab(h, os.path.join(OUT, '%02d_%s.png' % (n + 1, name)))
            print('%02d %s' % (n + 1, name))
            if name == 'power':
                tap(194, 314)   # "ОТМЕНА"
            for i in range(4):   # back to a main screen ("back" at the bottom left of the nested ones), then home
                tap(44, 446)
            if u32.IsIconic(h):
                u32.ShowWindow(h, 9)   # SW_RESTORE
            tap(60, 440)
    finally:
        u32.PostMessageW(u32.FindWindowW('VestaHome_Wnd', None), 0x10, 0, 0)
        time.sleep(1)
        p.kill()
        if saved is not None:
            open(ini, 'wb').write(saved)
        else:
            os.remove(ini)
        os.remove(nmea)
    print('done: %s' % os.path.abspath(OUT))


if __name__ == '__main__':
    main()
