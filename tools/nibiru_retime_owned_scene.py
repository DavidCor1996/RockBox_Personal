#!/usr/bin/env python3
"""Convert an owned legacy intro raster stream to the retail 24 Hz phase clock.

No game data is distributed. Input is a locally generated v2 NBS stream.
The executable's main loop (0x402b36) divides performance-counter frequency
by 24; model advance at 0x422684 increments an animation phase each update.
The X exporter's 150-unit key spacing is not a millisecond playback delay.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path

HEADER = struct.Struct('<4sHHHHHHII')
ENTRY = struct.Struct('<IIHHHH')
CLIP_STARTS = (6, 321, 357, 393)


def decode_frames(data):
    magic, version, width, height, count, rate, entry_size, table, _ = HEADER.unpack_from(data)
    if (magic, version, width, height, rate, entry_size) != (b'NBS1', 2, 320, 240, 24, ENTRY.size):
        raise ValueError('expected a full 320x240, 24 fps v2 owned intro')
    if count != 1524:
        raise ValueError('input must contain the complete legacy intro')
    pixels = [0] * (width * height)
    for frame in range(count):
        offset, size, x, y, w, h = ENTRY.unpack_from(data, table + frame * ENTRY.size)
        if x + w > width or y + h > height or offset + size > len(data):
            raise ValueError('invalid frame bounds')
        mask_size = (w * h + 7) // 8
        colors = offset + mask_size
        for pos in range(w * h):
            if data[offset + pos // 8] & (1 << (pos % 8)):
                pixels[(y + pos // w) * width + x + pos % w] = struct.unpack_from('<H', data, colors)[0]
                colors += 2
        if colors != offset + size:
            raise ValueError('invalid frame payload')
        yield pixels


def retime(data):
    selected = {round(phase * 150 * 24 / 1000): phase for phase in range(424)}
    frames = []
    previous = None
    for index, pixels in enumerate(decode_frames(data)):
        if index not in selected:
            continue
        phase = selected[index]
        keyframe = phase == 0 or phase in CLIP_STARTS
        changed = [i for i, value in enumerate(pixels)
                   if keyframe or previous[i] != value]
        if changed:
            x = min(i % 320 for i in changed)
            y = min(i // 320 for i in changed)
            w = max(i % 320 for i in changed) - x + 1
            h = max(i // 320 for i in changed) - y + 1
            mask = bytearray((w * h + 7) // 8)
            colors = bytearray()
            for i in changed:
                pos = (i // 320 - y) * w + i % 320 - x
                mask[pos // 8] |= 1 << (pos % 8)
                colors.extend(struct.pack('<H', pixels[i]))
            frames.append((x, y, w, h, mask + colors))
        else:
            frames.append((0, 0, 0, 0, b''))
        previous = pixels.copy()
    assert len(frames) == 424
    offset = HEADER.size + len(frames) * ENTRY.size
    table, payload = bytearray(), bytearray()
    for x, y, w, h, encoded in frames:
        table.extend(ENTRY.pack(offset + len(payload), len(encoded), x, y, w, h))
        payload.extend(encoded)
    return HEADER.pack(b'NBS1', 3, 320, 240, len(frames), 24, ENTRY.size,
                       HEADER.size, offset) + table + payload


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    source = args.source.read_bytes()
    result = retime(source)
    args.output.write_bytes(result)
    manifest = {'version': 3, 'frame_rate': 24, 'timeline_frames': 424,
                'source_sha256': hashlib.sha256(source).hexdigest(),
                'sha256': hashlib.sha256(result).hexdigest(),
                'script_selected_clip_starts': CLIP_STARTS,
                'source': 'locally generated owned v2 raster stream',
                'maximum_pose_quantization_phases': 1 / 7.2}
    args.output.with_suffix(args.output.suffix + '.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(json.dumps(manifest, indent=2))


if __name__ == '__main__':
    main()
