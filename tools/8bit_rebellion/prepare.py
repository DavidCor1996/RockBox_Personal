#!/usr/bin/env python3
"""Convert a local original 1.4.5 IPA into bounded Rockbox game resources.

Usage: rockpod/.venv/bin/python tools/8bit_rebellion/prepare.py GAME.ipa OUT
Requires Pillow, numpy and ffmpeg. Original resources remain separate from
the engine. This tool does not download or execute application code.
"""
import argparse
import hashlib
import json
import plistlib
import struct
import subprocess
import tempfile
import xml.etree.ElementTree as ET
import zipfile
from pathlib import Path

import numpy as np
from PIL import Image
from resources import anu_data, png_image
from campaign import HUBS, NAMES, TRACKS, SPRITES, STEPS, hub

HEIGHT = 200
SCALE = HEIGHT / 320


def rgb565(image, transparency=False):
    a = np.array(image.convert('RGBA'), dtype=np.uint16)
    out = ((a[:, :, 0] >> 3) << 11) | ((a[:, :, 1] >> 2) << 5) | (a[:, :, 2] >> 3)
    out[out == 0xf81f] = 0xf81e
    if transparency:
        out[a[:, :, 3] < 128] = 0xf81f
    return out.astype('<u2').tobytes()


def scene_image(app, stem):
    xml = ET.fromstring((app / (stem + '_scene.xml')).read_bytes())
    data = anu_data((app / xml.findtext('anu')).read_bytes())
    sheets = [png_image((app / node.text.strip()).read_bytes())
              for node in xml.findall('img')]
    widths = [int(n.findtext('width')) for n in xml.findall('layer')]
    reference = int(xml.findtext('referenceLayer'))
    world_width = max(480, widths[reference])
    canvas = Image.new('RGBA', (world_width, 320), '#171721')
    # Bake reference-layer scenery. Rear layers use proportionate placement
    # and repeat to fill the wider world, replacing original live parallax.
    for layer in range(len(data['sequences'])):
        width = widths[layer] if layer < reference else world_width
        first = data['sequences'][layer][0]
        group = data['groups'][data['frames'][first][0]]
        layer_im = Image.new('RGBA', (max(width, 480), 320))
        for index, x, y, flags in group:
            if flags & 7 == 7 or index >= len(data['images']):
                continue  # collision/dummy records, not image rectangles
            sheet, sx, sy, w, h = data['images'][index]
            if sheet >= len(sheets) or not (0 < w <= 2048 and 0 < h <= 2048):
                raise ValueError('Invalid scene image rectangle')
            part = sheets[sheet].crop((sx, sy, sx + w, sy + h))
            if flags & 2:
                part = part.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
            if flags & 4:
                part = part.transpose(Image.Transpose.FLIP_TOP_BOTTOM)
            layer_im.alpha_composite(part, (x, y))
        if layer < reference:
            # Tiling keeps distant skyline pixels at their original scale.
            for x in range(0, world_width, width):
                canvas.alpha_composite(layer_im, (x, 0))
        else:
            canvas.alpha_composite(layer_im)
    return canvas.resize((round(world_width * SCALE), HEIGHT),
                         Image.Resampling.NEAREST)


def character(app, name, phase):
    im = png_image((app / (name + '.png')).read_bytes()).resize((256, 256))
    canvas = Image.new('RGBA', (192, 280))
    # Compose original atlas pieces in a click-wheel-sized articulated pose.
    bob = (0, -4, 0, -4)[phase]
    stride = (0, -10, 0, 10)[phase]
    if name.startswith('group'):
        # Hi-res NPC atlases use a head/body row and limb rows.
        parts = [(0,0,128,128,32,0), (128,0,128,128,46,94),
                 (0,128,64,64,27,116), (64,128,64,64,97,116),
                 (128,128,64,64,44+stride,182),
                 (192,128,64,64,82-stride,182)]
    else:
        parts = [(64,192,32,64,71,128), (192,192,64,64,45,143),
                 (128,128,64,64,51+stride,156),
                 (128,96,64,32,48+stride,201),
                 (64,128,64,64,66,156),
                 (192,128,64,64,76-stride,157),
                 (192,96,64,32,71-stride,202),
                 (0,128,64,128,64,104), (0,0,128,128,39,25),
                 (96,192,32,64,98,128), (128,192,64,64,80,143)]
    for x,y,w,h,dx,dy in parts:
        canvas.alpha_composite(im.crop((x,y,x+w,y+h)),(dx,dy+bob))
    return canvas.resize((40,58),Image.Resampling.NEAREST)


def clean(s):
    return ' '.join(s.replace('|', '/').split()).replace('by tapping on', 'with Select')


def build(ipa, output):
    output.mkdir(parents=True, exist_ok=True)
    ready = output / 'ready.dat'
    ready.unlink(missing_ok=True)
    with tempfile.TemporaryDirectory(prefix='8br-import-') as temp:
        temp = Path(temp)
        with zipfile.ZipFile(ipa) as archive:
            names = archive.namelist()
            prefix = next(n.rsplit('/', 1)[0] + '/' for n in names
                          if n.endswith('.app/Info.plist'))
            for info in archive.infolist():
                if not info.filename.startswith(prefix) or info.is_dir():
                    continue
                relative = Path(info.filename[len(prefix):])
                if '..' in relative.parts or relative.is_absolute():
                    raise ValueError('Unsafe archive path')
                if info.file_size > 64 * 1024 * 1024:
                    raise ValueError('Oversized resource')
                dest = temp / relative
                dest.parent.mkdir(parents=True, exist_ok=True)
                dest.write_bytes(archive.read(info))
            art = archive.read('iTunesArtwork')
        strings = plistlib.loads(next(temp.rglob('Localizable.strings')).read_bytes())
        stems = sorted(f.name.removesuffix('_scene.xml')
                       for f in temp.glob('*_scene.xml') if '_darken' not in f.name)
        if not all(s in stems for s in HUBS):
            raise ValueError('Complete iPhone district resources required')
        records = []
        (output / 'scenes').mkdir(exist_ok=True)
        for i, stem in enumerate(stems):
            im = scene_image(temp, stem)
            if im.width > 4096:
                raise ValueError('Scene exceeds runtime width limit')
            (output / 'scenes' / f'{i:02}.rgb').write_bytes(rgb565(im))
            ground = 175 if stem not in HUBS else 166
            track = 9 if 'apartment' in stem else hub(stem)
            if stem == 'mall': track = 10
            if stem == 'boss1arena': track = 11
            if stem == 'boss2arena': track = 12
            name = NAMES.get(stem, NAMES.get(stem.split('_')[0], stem) + ' Apartment')
            records.append(f'{clean(name)}|{im.width}|{ground}|{hub(stem)}|{track}|{int(stem in HUBS)}')
        (output / 'scenes.tsv').write_text('\n'.join(records)+'\n')
        rows = []
        for scene, kind, count, actor, reward, key, dialogue, label in STEPS:
            objective = clean(strings[key]).replace('(%d/%d)', '').replace('(%d/20)', '').replace('(%d/6)', '')
            row = [str(-1 if scene == '*' else stems.index(scene)),str(kind),str(count),
                   str(actor),str(reward),label,objective,clean(strings.get(dialogue,''))]
            rows.append('|'.join(row))
        (output / 'quests.tsv').write_text('\n'.join(rows)+'\n')
        with (output / 'sprites.rgb').open('wb') as f:
            for name in SPRITES:
                for phase in range(4):
                    f.write(rgb565(character(temp,name,phase),True))
        for i, track in enumerate(TRACKS):
            subprocess.run(['ffmpeg','-v','fatal','-nostdin','-y','-i',str(temp/(track+'.mp3')),'-vn',
                            '-f','s16le','-ac','2','-ar','44100',str(output/f'{i:02}.pcm')],check=True)
        Image.open(__import__('io').BytesIO(art)).convert('RGB').resize((144,108),Image.Resampling.LANCZOS).save(output/'cover.bmp')
        manifest = {'format':1,'ipa_sha256':hashlib.sha256(ipa.read_bytes()).hexdigest(),
                    'bundle_version':plistlib.loads((temp/'Info.plist').read_bytes()).get('CFBundleVersion'),
                    'scenes':stems,'tracks':TRACKS,'sprites':SPRITES,
                    'adaptation':'Original assets/objective text; reconstructed movement, combat, placements and quest transitions.',
                    'files':{p.relative_to(output).as_posix():{'size':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()}
                             for p in sorted(output.rglob('*')) if p.is_file() and p.name not in ('manifest.json','ready.dat')}}
        (output/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
        ready.write_bytes(b'8BR1')
    print(f'Prepared {len(stems)} scenes, {len(STEPS)} quest stages and {len(TRACKS)} tracks in {output}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('ipa',type=Path)
    parser.add_argument('output',type=Path)
    args = parser.parse_args()
    build(args.ipa,args.output)
