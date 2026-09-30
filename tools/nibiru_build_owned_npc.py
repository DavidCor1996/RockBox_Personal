#!/usr/bin/env python3
"""Rasterize an owned AGDS NPC's authored animation keys into NCS sprites.

Descriptors and yaw come from the NPC's model animation script. Inputs and
outputs contain owned game data and belong outside the distributable source.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

from nibiru_build_owned_character import build, sha256
from nibiru_build_owned_character_set import parse_animation
from nibiru_model_probe import locate_tracks


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('model', type=Path)
    parser.add_argument('texture', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--yaw', type=int, required=True)
    parser.add_argument('--animation', action='append', type=parse_animation,
                        required=True)
    args = parser.parse_args()
    yaw = args.yaw % 360
    records = []
    for descriptor, animation in args.animation:
        _, _, frames = locate_tracks(animation.read_bytes())
        for frame in range(frames):
            output = args.output / descriptor / f'{yaw:03d}' / f'{frame:03d}.ncs'
            build(args.model, args.texture, animation, frame, yaw, output)
        records.append({'descriptor': descriptor, 'frames': frames,
                        'resource': animation.name, 'sha256': sha256(animation)})
    print(json.dumps({'model': sha256(args.model), 'texture': sha256(args.texture),
                      'yaw': yaw, 'animations': records}, indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
