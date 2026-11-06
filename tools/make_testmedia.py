#!/usr/bin/env python3
"""make_testmedia.py - short test files for VestaHome's players (music, video) and its photo viewer.

Writes VestaHome\\VestaHome\\VestaHome\\TestMedia\\ (the build output folder; make_tests.py copies it to
Tests\\SDCard\\TestMedia; VestaHomePC finds it next to the exe):
    lr.mp3      9 s: 3 s left only (440 Hz), 3 s right only (660 Hz), 3 s both (550 Hz) - balance
    sweep.mp3   12 s: a sine from 40 Hz to 12 kHz - equalizer (low / mid / high)
    test.mp3 / test.m4a (AAC) / test.flac / test.wav   10 s: a tone stepping each second, with tags
    test.mp4    10 s 640x360 H.264 baseline + AAC (the phone kind)
    test.avi    10 s 640x360 XviD + MP3
    test.mkv    10 s 640x360 H.264 + AAC in Matroska
The pictures are ffmpeg's testsrc (colour bars and a running counter), the video's sound a 1 kHz beep a second.
    Photos\\     for "Media -> Photo" (drawn with Pillow; a yellow "ВЕРХ" in each picture's top-left corner shows
                whether it stands the right way up):
        IMG_0001_phone.jpg      4000 x 3000 baseline, EXIF orientation 6 (turn a quarter clockwise) and an EXIF
                                thumbnail - as a phone's photo: the thumbnail from EXIF, the viewer upright
        IMG_0002.jpg            1600 x 1200 baseline
        IMG_0003_progressive.jpg 1200 x 900 progressive (stb_image: small ones only)
        IMG_0004_prog_big.jpg   3000 x 2000 progressive: "too big" expected
        picture.png, picture.bmp 800 x 600, 640 x 480
        broken.jpg              a JPEG cut short: an error expected, no crash
        Отпуск\\DSC_0100.jpg     a subfolder

Needs ffmpeg: from PATH, else from the imageio-ffmpeg package (python -m pip install imageio-ffmpeg); the photos
need Pillow (python -m pip install pillow).

    python VestaHome/tools/make_testmedia.py            (all)
    python VestaHome/tools/make_testmedia.py photos     (the photos only)
"""
import io
import os
import shutil
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.normpath(os.path.join(HERE, '..', 'VestaHome', 'VestaHome', 'TestMedia'))


def ffmpeg():
    exe = shutil.which('ffmpeg')
    if exe:
        return exe
    try:
        import imageio_ffmpeg
        return imageio_ffmpeg.get_ffmpeg_exe()
    except ImportError:
        sys.exit('no ffmpeg: put it on PATH or run  python -m pip install imageio-ffmpeg')


def run(ff, args, name):
    path = os.path.join(OUT, name)
    subprocess.run([ff, '-hide_banner', '-loglevel', 'error', '-y'] + args + [path], check=True)
    print('  %-10s %8d' % (name, os.path.getsize(path)))


def tags(fmt):
    return ['-metadata', 'title=Тест VestaHome (%s)' % fmt, '-metadata', 'artist=VestaHome',
            '-metadata', 'album=TestMedia']


def picture(w, h, label):
    """A test picture: a sky-to-ground gradient, bars, the label in the middle, "ВЕРХ" top-left."""
    from PIL import Image, ImageDraw, ImageFont
    im = Image.new('RGB', (w, h))
    d = ImageDraw.Draw(im)
    for y in range(h):
        k = y / h
        d.line([(0, y), (w, y)], fill=(int(40 + 180 * k), int(110 + 60 * (1 - k)), int(210 - 140 * k)))
    bars = [(235, 235, 235), (235, 235, 30), (30, 235, 235), (30, 235, 30), (235, 30, 235), (235, 30, 30),
            (30, 30, 235)]
    bw = w // len(bars)
    for i, c in enumerate(bars):
        d.rectangle((i * bw, int(h * 0.62), (i + 1) * bw, int(h * 0.82)), fill=c)
    try:
        font = ImageFont.truetype(r'C:\Windows\Fonts\tahomabd.ttf', max(12, h // 10))
        small = ImageFont.truetype(r'C:\Windows\Fonts\tahomabd.ttf', max(10, h // 14))
    except OSError:
        font = small = ImageFont.load_default()
    d.rectangle((w * 0.03, h * 0.04, w * 0.34, h * 0.2), fill=(250, 220, 30))
    d.text((w * 0.05, h * 0.06), 'ВЕРХ', font=small, fill=(0, 0, 0))
    d.text((w * 0.5, h * 0.42), label, font=font, fill=(255, 255, 255), anchor='mm')
    return im


def exif_app1(orientation, thumb_jpeg):
    """An EXIF APP1 payload (little-endian TIFF): IFD0 with the orientation, IFD1 with a JPEG thumbnail - as phones
    write them (Pillow writes no IFD1)."""
    ifd0 = 8
    ifd0_size = 2 + 1 * 12 + 4
    ifd1 = ifd0 + ifd0_size
    ifd1_size = 2 + 2 * 12 + 4
    thumb_ofs = ifd1 + ifd1_size
    t = b'II*\x00' + struct.pack('<I', ifd0)
    t += struct.pack('<H', 1) + struct.pack('<HHIHH', 0x0112, 3, 1, orientation, 0) + struct.pack('<I', ifd1)
    t += struct.pack('<H', 2)
    t += struct.pack('<HHII', 0x0201, 4, 1, thumb_ofs)
    t += struct.pack('<HHII', 0x0202, 4, 1, len(thumb_jpeg))
    t += struct.pack('<I', 0)
    return b'Exif\x00\x00' + t + thumb_jpeg


def photos():
    try:
        from PIL import Image
    except ImportError:
        print('Photos: no Pillow (python -m pip install pillow) - skipped')
        return
    out = os.path.join(OUT, 'Photos')
    os.makedirs(os.path.join(out, 'Отпуск'), exist_ok=True)
    print('Photos -> %s' % out)

    def save(im, name, **kw):
        path = os.path.join(out, name)
        im.save(path, **kw)
        print('  %-28s %8d' % (name, os.path.getsize(path)))

    # the phone's: stored as the sensor saw it (landscape), EXIF says turn a quarter clockwise; the thumbnail too
    big = picture(4000, 3000, 'IMG_0001 (EXIF 6)')
    tb = io.BytesIO()
    big.resize((160, 120)).save(tb, 'JPEG', quality=80)
    main = io.BytesIO()
    big.save(main, 'JPEG', quality=88)
    data = main.getvalue()
    app1 = exif_app1(6, tb.getvalue())
    data = data[:2] + b'\xff\xe1' + struct.pack('>H', len(app1) + 2) + app1 + data[2:]
    with open(os.path.join(out, 'IMG_0001_phone.jpg'), 'wb') as f:
        f.write(data)
    print('  %-28s %8d' % ('IMG_0001_phone.jpg', len(data)))
    save(picture(1600, 1200, 'IMG_0002'), 'IMG_0002.jpg', quality=90)
    save(picture(1200, 900, 'progressive'), 'IMG_0003_progressive.jpg', quality=85, progressive=True)
    save(picture(3000, 2000, 'progressive big'), 'IMG_0004_prog_big.jpg', quality=85, progressive=True)
    save(picture(800, 600, 'PNG'), 'picture.png')
    save(picture(640, 480, 'BMP'), 'picture.bmp')
    b = io.BytesIO()
    picture(1600, 1200, 'broken').save(b, 'JPEG', quality=90)
    with open(os.path.join(out, 'broken.jpg'), 'wb') as f:
        f.write(b.getvalue()[:len(b.getvalue()) // 3])   # cut short
    print('  %-28s %8d' % ('broken.jpg', len(b.getvalue()) // 3))
    save(picture(2000, 1500, 'DSC_0100'), os.path.join('Отпуск', 'DSC_0100.jpg'), quality=88)


def main():
    if sys.argv[1:] == ['photos']:
        photos()
        return
    ff = ffmpeg()
    os.makedirs(OUT, exist_ok=True)
    print('TestMedia -> %s' % OUT)
    sr = ['-ar', '44100']
    # balance: left, right, both
    lr = ('aevalsrc=0.5*sin(2*PI*440*t)*lt(t\\,3)+0.5*sin(2*PI*550*t)*gte(t\\,6)'
          '|0.5*sin(2*PI*660*t)*gte(t\\,3)*lt(t\\,6)+0.5*sin(2*PI*550*t)*gte(t\\,6):s=44100:d=9')
    run(ff, ['-f', 'lavfi', '-i', lr, '-c:a', 'libmp3lame', '-b:a', '128k'] + tags('лево-право'), 'lr.mp3')
    # log sweep 40 Hz .. 12 kHz in 12 s: phase = 2*pi*f0*T/ln(k)*(k^(t/T)-1), k = 300
    sweep = 'aevalsrc=0.5*sin(2*PI*40*12/log(300)*(pow(300\\,t/12)-1)):s=44100:d=12'
    run(ff, ['-f', 'lavfi', '-i', sweep, '-ac', '2', '-c:a', 'libmp3lame', '-b:a', '128k'] + tags('свип'),
        'sweep.mp3')
    # the format tests: a tone that steps every second (440, 554, 659, 880 Hz)
    tone = 'aevalsrc=0.4*sin(2*PI*t*(440+114*eq(mod(floor(t)\\,4)\\,1)+219*eq(mod(floor(t)\\,4)\\,2)' \
           '+440*eq(mod(floor(t)\\,4)\\,3))):s=44100:d=10'
    src = ['-f', 'lavfi', '-i', tone, '-ac', '2'] + sr
    run(ff, src + ['-c:a', 'libmp3lame', '-b:a', '128k'] + tags('MP3'), 'test.mp3')
    run(ff, src + ['-c:a', 'aac', '-b:a', '128k'] + tags('M4A'), 'test.m4a')
    run(ff, src + ['-c:a', 'flac'] + tags('FLAC'), 'test.flac')
    run(ff, src + ['-c:a', 'pcm_s16le'], 'test.wav')
    # video: colour bars + counter, a 1 kHz beep each second
    vid = ['-f', 'lavfi', '-i', 'testsrc=size=640x360:rate=25:duration=10',
           '-f', 'lavfi', '-i', 'aevalsrc=0.4*sin(2*PI*1000*t)*lt(mod(t\\,1)\\,0.15):s=44100:d=10',
           '-ac', '2', '-shortest']
    h264 = ['-c:v', 'libx264', '-profile:v', 'baseline', '-level', '3.0', '-pix_fmt', 'yuv420p', '-b:v', '800k']
    run(ff, vid + h264 + ['-c:a', 'aac', '-b:a', '128k', '-movflags', '+faststart'] + tags('MP4'), 'test.mp4')
    run(ff, vid + ['-c:v', 'libxvid', '-vtag', 'XVID', '-q:v', '5', '-pix_fmt', 'yuv420p',
                   '-c:a', 'libmp3lame', '-b:a', '128k'], 'test.avi')
    run(ff, vid + h264 + ['-c:a', 'aac', '-b:a', '128k'] + tags('MKV'), 'test.mkv')
    photos()


if __name__ == '__main__':
    main()
