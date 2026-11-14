"""The PC bench of the phone's USB tethering (the unit's VestaTether.dll + TetherProbe, 2026-11-14: on the unit the
driver never logged a line - first the phone's side here): the phone by cable, its debugging interface (ADB), our key,
the same command as the unit's driver ("svc usb setFunctions rndis"), then what the phone comes back as - its VID:PID
and interfaces (for the unit's USBD keys) and whether it offers RNDIS.

Needs: pip install pyusb libusb-package cryptography; on the phone "USB debugging" on. Windows usually gives the ADB
interface a WinUSB driver by itself (if libusb cannot open it: Zadig -> WinUSB on "ADB Interface").

    python tether_bench.py          the whole probe
    python tether_bench.py list     every USB device with its interfaces, nothing sent
The key: adbkey_pc.pem beside this script (made once, not in git) - the phone asks once "Allow USB debugging?".
"""
import os
import sys
import time

import libusb_package
import usb.backend.libusb1
import usb.core
import usb.util
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import padding, rsa, utils

import adbwire as w

HERE = os.path.dirname(os.path.abspath(__file__))
BACKEND = usb.backend.libusb1.get_backend(find_library=libusb_package.find_library)
ADB_CLASS = (0xFF, 0x42, 0x01)
RNDIS_CLASSES = {(0xE0, 0x01, 0x03): 'RNDIS (E0/01/03)', (0xEF, 0x04, 0x01): 'RNDIS (EF/04/01)',
                 (0x02, 0x02, 0xFF): 'RNDIS (02/02/FF, Microsoft)', (0x02, 0x06, 0x00): 'CDC ECM',
                 (0x02, 0x0D, 0x00): 'CDC NCM'}
COMMAND = b'shell:svc usb setFunctions rndis 2>&1 || svc usb setFunction rndis 2>&1\0'   # (the unit's kAdbCmd)


def interfaces(dev):
    out = []
    try:
        for cfg in dev:
            for itf in cfg:
                out.append((itf.bInterfaceNumber, itf.bAlternateSetting,
                            (itf.bInterfaceClass, itf.bInterfaceSubClass, itf.bInterfaceProtocol)))
    except (usb.core.USBError, NotImplementedError, ValueError):
        pass
    return out


def describe(dev):
    s = '%04X:%04X class %02X/%02X/%02X' % (dev.idVendor, dev.idProduct, dev.bDeviceClass, dev.bDeviceSubClass,
                                           dev.bDeviceProtocol)
    for num, alt, c in interfaces(dev):
        kind = 'ADB' if c == ADB_CLASS else RNDIS_CLASSES.get(c, '')
        s += '\n    interface %d.%d: %02X/%02X/%02X %s' % (num, alt, c[0], c[1], c[2], kind)
    return s


def list_all():
    for dev in usb.core.find(find_all=True, backend=BACKEND):
        print(describe(dev))


def find_adb():
    for dev in usb.core.find(find_all=True, backend=BACKEND):
        for num, alt, c in interfaces(dev):
            if c == ADB_CLASS:
                return dev, num
    return None, None


def key():
    path = os.path.join(HERE, 'adbkey_pc.pem')
    if os.path.exists(path):
        with open(path, 'rb') as f:
            return serialization.load_pem_private_key(f.read(), None)
    k = rsa.generate_private_key(65537, 2048)
    with open(path, 'wb') as f:
        f.write(k.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8,
                                serialization.NoEncryption()))
    return k


class Link:
    def __init__(self, dev, num):
        self.dev = dev
        try:
            dev.set_configuration()
        except usb.core.USBError:
            pass   # (already set)
        usb.util.claim_interface(dev, num)
        itf = dev.get_active_configuration()[(num, 0)]
        self.ep_in = usb.util.find_descriptor(itf, custom_match=lambda e: usb.util.endpoint_direction(
            e.bEndpointAddress) == usb.util.ENDPOINT_IN)
        self.ep_out = usb.util.find_descriptor(itf, custom_match=lambda e: usb.util.endpoint_direction(
            e.bEndpointAddress) == usb.util.ENDPOINT_OUT)

    def send(self, cmd, a0, a1, data=b''):
        print('->', w.NAMES.get(cmd, hex(cmd)), a0, a1, data[:60])
        self.ep_out.write(w.header(cmd, a0, a1, data))
        if data:
            self.ep_out.write(data)

    def recv(self, timeout=30000):
        m = w.parse(bytes(self.ep_in.read(24, timeout)))
        if not m:
            raise RuntimeError('not an ADB header')
        cmd, a0, a1, n = m
        data = bytes(self.ep_in.read(n, timeout)) if n else b''
        print('<-', w.NAMES.get(cmd, hex(cmd)), a0, a1, data[:80])
        return cmd, a0, a1, data


def adb_session(link, k):
    """Connect, prove the key (or offer it), run the command; True when it ran."""
    link.send(w.CNXN, w.VERSION, w.MAXDATA, b'host::VestaTether\0')
    signed = False
    while True:
        cmd, a0, a1, data = link.recv(60000)
        if cmd == w.AUTH and a0 == w.AUTH_TOKEN and not signed:
            sig = k.sign(data, padding.PKCS1v15(), utils.Prehashed(hashes.SHA1()))
            link.send(w.AUTH, w.AUTH_SIGNATURE, 0, sig)
            signed = True
        elif cmd == w.AUTH and a0 == w.AUTH_TOKEN:
            print('the phone asks its user: "Allow USB debugging?" - allow it (better with "Always"), up to 60 s')
            pub = k.public_key().public_numbers()
            link.send(w.AUTH, w.AUTH_RSAPUBLICKEY, 0, w.pubkey(pub.n, pub.e, 'VestaTether@PC'))
        elif cmd == w.CNXN:
            print('connected:', data.decode('ascii', 'replace'))
            break
    link.send(w.OPEN, 1, 0, COMMAND)
    while True:
        try:
            cmd, a0, a1, data = link.recv(15000)
        except usb.core.USBError:
            print('the link went (the phone switching its USB - expected after the command)')
            return True
        if cmd == w.WRTE:
            print('the phone says:', data.decode('utf-8', 'replace').strip())
            link.send(w.OKAY, 1, a0)
        elif cmd == w.CLSE:
            return True


def main():
    if sys.argv[1:] == ['list']:
        list_all()
        return 0
    dev, num = find_adb()
    if not dev:
        print('no phone with its debugging interface (ADB): cable, "USB debugging" on? Every device:')
        list_all()
        return 1
    print('the phone before:', describe(dev))
    adb_session(Link(dev, num), key())
    usb.util.dispose_resources(dev)
    print('waiting 20 s for the phone to come back as a modem...')
    time.sleep(20)
    found = False
    for d in usb.core.find(find_all=True, backend=BACKEND):
        kinds = [c for _, _, c in interfaces(d) if c in RNDIS_CLASSES]
        if kinds:
            found = True
            print('the phone after (a modem):', describe(d))
    if not found:
        print('no RNDIS / ECM / NCM device came. Every device now:')
        list_all()
    else:
        print('Windows should now show a new network adapter with the phone\'s internet.')
    return 0 if found else 2


if __name__ == '__main__':
    sys.exit(main())
