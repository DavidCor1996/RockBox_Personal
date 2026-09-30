#!/usr/bin/env python3
"""Exercise Twitter's real profile, photos, MPEG launch, and return in SDL."""
import os
import shutil
import subprocess
import tempfile
import time
from pathlib import Path
from PIL import Image, ImageChops
from social2010_ui_sim_gate import prepare_root, wait_for_frame

REPO = Path(__file__).resolve().parents[1]
BUILD = REPO / 'build-sim-ipod6g'
OUTPUT = REPO / 'build-sim-ipod6g/twitter-ui-evidence'
DATA = Path('/tmp/rockpod-twitter-device/.rockbox/twitter')


def main():
    OUTPUT.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='twitter-ui-') as temp:
        root = Path(temp)
        prepare_root(BUILD, root, 'twitter', DATA)
        # Use the requested real profile only so frame positions are repeatable.
        accounts = root / '.rockbox/twitter/accounts.tsv'
        lines = accounts.read_text().splitlines()
        accounts.write_text(lines[0] + '\n' + next(x for x in lines[1:] if x.startswith('reiivalentinaa\t')) + '\n')
        rb = root / '.rockbox'
        for directory in ('codecs',):
            source = BUILD / 'simdisk/.rockbox' / directory
            if source.exists():
                shutil.copytree(source, rb / directory)
        viewers = rb / 'rocks/viewers'
        viewers.mkdir(parents=True, exist_ok=True)
        shutil.copy2(BUILD / 'apps/plugins/mpegplayer/mpegplayer.rock', viewers / 'mpegplayer.rock')
        with (rb / 'config.cfg').open('a') as f:
            f.write('font: /.rockbox/fonts/15-Adobe-Helvetica.fnt\n')
        frame = root / 'frame.bmp'
        env = dict(os.environ, SDL_AUDIODRIVER='dummy', SDL_VIDEODRIVER='dummy', SDL_RENDER_DRIVER='software', ROCKPOD_SIM_HIDDEN='1', ROCKPOD_SIM_PREVIEW_BMP=str(frame), ROCKPOD_SIM_PREVIEW_INTERVAL_MS='25')
        keys = {'select':'SELECT', 'menu':'MENU', 'right':'RIGHT', 'left':'LEFT', 'down':'SCROLL_FWD', 'up':'SCROLL_BACK'}
        for name, variable in keys.items():
            env[f'ROCKPOD_SIM_{variable}_GATE'] = str(root / (name + '.gate'))
        log = (OUTPUT / 'simulator.log').open('w')
        proc = subprocess.Popen([str(BUILD/'rockboxui'), '--zoom', '1', '--nobackground', '--root', str(root)], cwd=BUILD, env=env, stdout=log, stderr=subprocess.STDOUT)
        def tap(name, hold=.12):
            gate=root/(name+'.gate');gate.touch();time.sleep(hold);gate.unlink();time.sleep(.7)
        def capture(name):
            Image.open(frame).convert('RGB').save(OUTPUT/(name+'.png'))
        try:
            wait_for_frame(frame);time.sleep(7)
            capture('01-feed')
            tap('select');capture('02-tweet')
            tap('select');capture('03-photo')
            tap('menu');tap('menu');tap('right');capture('04-media-tab')
            tap('down');tap('down');capture('05-video-feed')
            tap('select');capture('06-video-tweet')
            tap('select');capture('07-video-attachment')
            tap('right');capture('08-mixed-post-photo')
            tap('left');tap('select');time.sleep(.4);capture('09-video-playing')
            tap('menu')
            before=Image.open(OUTPUT/'06-video-tweet.png').convert('RGB')
            deadline=time.monotonic()+20
            while time.monotonic()<deadline:
                time.sleep(.3)
                capture('10-returned-tweet')
                after=Image.open(OUTPUT/'10-returned-tweet.png').convert('RGB')
                if not ImageChops.difference(before,after).getbbox():
                    break
            if ImageChops.difference(before,after).getbbox():
                raise AssertionError('Video did not return to the same tweet screen')
            tap('menu');tap('select',1.0);capture('11-options')
            tap('select');capture('12-saved-tweet')
            if not (rb/'twitter/saved.txt').is_file():
                raise AssertionError('Offline bookmark was not written')
            tap('menu');tap('right');tap('right');capture('13-saved-tab')
            tap('select');capture('14-saved-detail')
            print('PASS: real feed, photo, mixed attachments, video handoff/return, offline bookmark')
        finally:
            proc.terminate()
            try: proc.wait(timeout=5)
            except subprocess.TimeoutExpired: proc.kill();proc.wait()
            log.close()

if __name__ == '__main__':
    main()
