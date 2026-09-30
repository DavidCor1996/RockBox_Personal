#!/usr/bin/env python3
"""Build a bounded Rockbox character sprite from an owned NiBiRu model.

The sprite is rasterized directly from retail mesh and texture data.  Its
row-run RGB565 encoding can be streamed with one small scanline buffer on the
iPod; no original-game framebuffer or screen recording is used.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path

import numpy as np

from nibiru_render_owned_model import SOURCE_SCALE, render_character_pose


HEADER = struct.Struct("<4sHHHHH")
RUN = struct.Struct("<HH")
MAGIC = b"NCS1"
VERSION = 1


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_sprite(
    model: Path,
    texture: Path,
    animation: Path,
    yaw: float,
    output: Path,
    image,
    pose: dict[str, object],
    write_manifest_file: bool = True,
) -> dict[str, object]:
    rgba = np.asarray(image.convert("RGBA"))
    coverage = rgba[:, :, 3] != 0
    ys, xs = np.nonzero(coverage)
    if not len(xs):
        raise ValueError("owned character model rendered no covered pixels")
    left, right = int(xs.min()), int(xs.max())
    top, bottom = int(ys.min()), int(ys.max())
    rgba = rgba[top : bottom + 1, left : right + 1]
    coverage = coverage[top : bottom + 1, left : right + 1]
    height, width = coverage.shape
    anchor_x = (left + right) // 2 - left
    anchor_y = bottom - top
    payload = bytearray(HEADER.pack(
        MAGIC, VERSION, width, height, anchor_x, anchor_y,
    ))
    rgb = rgba[:, :, :3]
    for y in range(height):
        runs: list[tuple[int, int]] = []
        x = 0
        while x < width:
            while x < width and not coverage[y, x]:
                x += 1
            start = x
            while x < width and coverage[y, x]:
                x += 1
            if x > start:
                runs.append((start, x - start))
        payload.extend(struct.pack("<H", len(runs)))
        for start, length in runs:
            payload.extend(RUN.pack(start, length))
            colors = rgb[y, start : start + length].astype(np.uint16)
            rgb565 = (
                ((colors[:, 0] >> 3) << 11)
                | ((colors[:, 1] >> 2) << 5)
                | (colors[:, 2] >> 3)
            ).astype("<u2")
            payload.extend(rgb565.tobytes())
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_suffix(output.suffix + ".tmp")
    temporary.write_bytes(payload)
    temporary.replace(output)
    manifest = {
        "format": "NiBiRu owned character sprite",
        "version": VERSION,
        "screen_capture_derived": False,
        "size": output.stat().st_size,
        "dimensions": [width, height],
        "anchor": [anchor_x, anchor_y],
        "sha256": sha256(output),
        "pose": pose,
        "sources": {
            model.name: sha256(model),
            texture.name: sha256(texture),
            animation.name: sha256(animation),
        },
    }
    if write_manifest_file:
        output.with_suffix(output.suffix + ".json").write_text(
            json.dumps(manifest, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
    return manifest


def build(
    model: Path,
    texture: Path,
    animation: Path,
    frame: int,
    yaw: float,
    output: Path,
    source_scale: float = SOURCE_SCALE,
) -> dict[str, object]:
    image = render_character_pose(
        model, texture, animation, frame, yaw, source_scale=source_scale
    )
    return write_sprite(
        model, texture, animation, yaw, output, image,
        {"animation": animation.name, "frame": frame, "yaw": yaw},
    )


def build_at_time(
    model: Path,
    texture: Path,
    animation: Path,
    time_ms: int,
    yaw: float,
    output: Path,
    write_manifest_file: bool = True,
    source_scale: float = SOURCE_SCALE,
) -> dict[str, object]:
    image = render_character_pose(
        model, texture, animation, 0, yaw, time_ms=time_ms,
        source_scale=source_scale,
    )
    return write_sprite(
        model, texture, animation, yaw, output, image,
        {"animation": animation.name, "time_ms": time_ms, "yaw": yaw},
        write_manifest_file,
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("model", type=Path)
    parser.add_argument("texture", type=Path)
    parser.add_argument("animation", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--frame", type=int, required=True)
    parser.add_argument("--yaw", type=float, required=True)
    parser.add_argument("--source-scale", type=float, default=SOURCE_SCALE)
    args = parser.parse_args()
    print(json.dumps(build(
        args.model, args.texture, args.animation,
        args.frame, args.yaw, args.output, args.source_scale,
    ), indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
