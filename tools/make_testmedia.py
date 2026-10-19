#!/usr/bin/env python3
"""make_testmedia.py - short test files for VestaHome's "Diagnostics -> Sound and video" (and the player, video).

Writes VestaHome\\VestaHome\\VestaHome\\TestMedia\\ (the build output folder; make_tests.py copies it to
Tests\\SDMemory\\TestMedia; VestaHomePC finds it next to the exe):
    lr.mp3      9 s: 3 s left only (440 Hz), 3 s right only (660 Hz), 3 s both (550 Hz) - balance
    sweep.mp3   12 s: a sine from 40 Hz to 12 kHz - equalizer (low / mid / high)
    test.mp3 / test.m4a (AAC) / test.flac / test.wav   10 s: a tone stepping each second, with tags
    test.mp4    10 s 640x360 H.264 baseline + AAC (the phone kind)
    test.avi    10 s 640x360 XviD + MP3
    test.mkv    10 s 640x360 H.264 + AAC in Matroska
The pictures are ffmpeg's testsrc (colour bars and a running counter), the video's sound a 1 kHz beep a second.

Needs ffmpeg: from PATH, else from the imageio-ffmpeg package (python -m pip install imageio-ffmpeg).

    python VestaHome/tools/make_testmedia.py
"""
import os
import shutil
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


def main():
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


if __name__ == '__main__':
    main()
