#!/usr/bin/env python3
"""Check that successful and failed H.264 launches redraw the Twitch app."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time

from PIL import Image
from h264_audio_sim_gate import (
    MOVIE_PATH, OPEN_PLUGIN_PARAM_OFFSET, OPEN_PLUGIN_PARAM_SIZE,
    prepare_root, stop_process, write_cstring,
)


def run(build, movie, failed):
    label = 'error' if failed else 'normal'
    with tempfile.TemporaryDirectory(prefix='twitch-return-') as directory:
        root = Path(directory)
        prepare_root(build, root, movie)
        rockbox = root / '.rockbox'
        (rockbox / 'rocks/apps').mkdir(parents=True)
        shutil.copy2(build / 'apps/plugins/twitch.rock',
                     rockbox / 'rocks/apps/twitch.rock')
        (rockbox / 'twitch').mkdir()
        (rockbox / 'twitch/creators.tsv').write_text(
            'creator_key\tlogin\tdisplay_name\tcycle_epoch\tfollower_count\n'
            'gate\tgate\tTwitch Return Test\t100\t0\n')
        (rockbox / 'twitch/vods.tsv').write_text(
            'id\tcreator_key\ttitle\tgame\tduration_seconds\tpublished_date'
            '\tviews\tvideo_path\tthumb_path\tdescription\n'
            f'gate\tgate\tReturned from H.264\tTest\t60\t2026-09-08'
            f'\t0\t{MOVIE_PATH}\t\tRedraw regression\n')
        entry_path = rockbox / 'rocks/plugin.dat'
        entry = bytearray(entry_path.read_bytes())
        entry[OPEN_PLUGIN_PARAM_OFFSET:
              OPEN_PLUGIN_PARAM_OFFSET + OPEN_PLUGIN_PARAM_SIZE] = (
                  b'\0' * OPEN_PLUGIN_PARAM_SIZE)
        write_cstring(entry, OPEN_PLUGIN_PARAM_OFFSET, OPEN_PLUGIN_PARAM_SIZE,
                      'twitch-app:' + MOVIE_PATH)
        entry_path.write_bytes(entry)
        if failed:
            (rockbox / 'codecs/aac.codec').unlink()
        frame = root / 'frame.bmp'
        environment = os.environ.copy()
        environment.update({
            'SDL_AUDIODRIVER': 'dummy', 'SDL_VIDEODRIVER': 'dummy',
            'SDL_RENDER_DRIVER': 'software',
            'ROCKPOD_SIM_H264_AUDIO_TEST_MS': '3000',
            'ROCKPOD_SIM_PREVIEW_BMP': str(frame),
            'ROCKPOD_SIM_PREVIEW_INTERVAL_MS': '50',
        })
        log_path = Path('/tmp') / f'twitch-h264-return-{label}.log'
        with log_path.open('wb') as log:
            process = subprocess.Popen(
                [str(build / 'rockboxui'), '--zoom', '1', '--nobackground',
                 '--root', str(root)], cwd=build, env=environment,
                stdout=log, stderr=subprocess.STDOUT)
            try:
                deadline = time.monotonic() + 40
                while time.monotonic() < deadline:
                    try:
                        with Image.open(frame) as image:
                            image.load()
                            red, green, blue = image.convert('RGB').getpixel(
                                (300, 10))
                            if 130 <= red <= 155 and 55 <= green <= 85 and blue > 240:
                                image.save(Path('/tmp') /
                                           f'twitch-h264-return-{label}.png')
                                print(f'Twitch {label} return redraw passed')
                                return
                    except (OSError, ValueError):
                        pass
                    if process.poll() is not None:
                        break
                    time.sleep(0.1)
                raise SystemExit(f'Twitch {label} return failed; see {log_path}')
            finally:
                stop_process(process)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('movie', type=Path)
    parser.add_argument('--build-dir', type=Path,
                        default=Path('build-sim-ipod6g'))
    args = parser.parse_args()
    for failed in (False, True):
        run(args.build_dir.resolve(), args.movie.resolve(), failed)


if __name__ == '__main__':
    main()
