#!/usr/bin/env python3
"""Prepare additive TV artwork sidecars; never modify source covers or music.

The version-1 payload is a 272x372 limited-range BT.601 YUV420 pane: a
272px slanted cover plus its 100px white-fading reflection. It matches the
native iPodJS 136x186 artwork pane at twice the sampling resolution.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path

import numpy as np
from PIL import Image, ImageOps

WIDTH, HEIGHT = 272, 372


def fnv1a(data):
    value = 2166136261
    for byte in data:
        value = ((value ^ byte) * 16777619) & 0xffffffff
    return value


def render(source, filtered=True):
    with Image.open(source) as original:
        original.load()
        dimensions = original.size
        cover = ImageOps.exif_transpose(original).convert('RGB')
        # The native WPS cover is drawn across this square; retain its crop.
        cover = cover.resize((WIDTH, WIDTH), Image.Resampling.LANCZOS)
    pane = np.full((HEIGHT, WIDTH, 3), 255, dtype=np.int32)
    for x in range(WIDTH):
        top = x * 20 // (WIDTH - 1)
        height = WIDTH - top
        column = np.asarray(cover.crop((x, 0, x+1, WIDTH)).resize(
            (1, height), Image.Resampling.LANCZOS))[:, 0, :].astype(np.int32)
        pane[top:WIDTH, x] = column
        for y in range(100):
            alpha = 144 + y * 112 // 99
            pane[WIDTH+y, x] = (column[height-1-y] *
                                      (256-alpha) + 255*alpha) // 256
    if filtered:
        padded = np.pad(pane, ((1, 1), (0, 0), (0, 0)), mode='edge')
        pane = (padded[:-2] + 2*padded[1:-1] + padded[2:] + 2) // 4
    return pane.astype(np.uint8), dimensions


def encode(pane):
    red, green, blue = [pane[:, :, n].astype(np.int32) for n in range(3)]
    y = ((66*red + 129*green + 25*blue + 128) >> 8) + 16
    cb = ((-38*red - 74*green + 112*blue + 128) >> 8) + 128
    cr = ((112*red - 94*green - 18*blue + 128) >> 8) + 128
    def subsample(plane):
        return ((plane[::2, ::2] + plane[::2, 1::2] +
                 plane[1::2, ::2] + plane[1::2, 1::2] + 2) // 4)
    payload = b''.join(p.astype(np.uint8).tobytes()
                       for p in (y, subsample(cb), subsample(cr)))
    assert len(payload) == 151776
    return b'TVART001' + struct.pack('<II', len(payload), fnv1a(payload)) + payload


def prepare(root, output):
    manifest = []
    covers = sorted(p for p in root.rglob('*') if p.is_file() and
                    p.name.lower() in ('cover.jpg', 'cover.png', 'cover.jpeg'))
    seen = set()
    for source in covers:
        relative = source.parent.relative_to(root) / 'cover.tvart'
        if relative in seen:
            continue
        seen.add(relative)
        pane, dimensions = render(source)
        encoded = encode(pane)
        destination = output / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(encoded)
        manifest.append(dict(source=str(source), destination=str(relative),
            source_size=dimensions,
            source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
            sha256=hashlib.sha256(encoded).hexdigest(), bytes=len(encoded)))
        if len(manifest) == 1:
            Image.fromarray(pane).save(output / 'preview-filtered.png')
            raw, _ = render(source, filtered=False)
            Image.fromarray(raw).save(output / 'preview-unfiltered.png')
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
    print(json.dumps(dict(covers=len(manifest), bytes=sum(m['bytes'] for m in manifest),
                          manifest=str(output/'manifest.json'))))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('music_root', type=Path)
    parser.add_argument('staging_output', type=Path)
    args = parser.parse_args()
    if args.staging_output.resolve().is_relative_to(args.music_root.resolve()):
        parser.error('stage outside the music tree; deploy is a separate step')
    args.staging_output.mkdir(parents=True, exist_ok=True)
    prepare(args.music_root, args.staging_output)
