#!/usr/bin/env python3
"""Build every authored key pose/direction from owned NiBiRu resources.

Each output is rasterized from the retail model, texture, hierarchy, skin
matrices, and animation key.  The tool never launches or captures the game.
"""

from __future__ import annotations

import argparse
import concurrent.futures
import hashlib
import json
import math
import os
import re
import struct
from pathlib import Path

from nibiru_build_owned_character import build_at_time
from nibiru_render_owned_model import (
    SOURCE_SCALE,
    animation_track_matrices_at,
    animation_world_matrices,
)
from nibiru_model_probe import locate_tracks


DIRECTIONS = tuple(range(0, 360, 45))
POSE_FPS = 24
SAFE_DESCRIPTOR = re.compile(r"^[A-Za-z0-9_.-]+$")
MOTION_HEADER = struct.Struct("<4sHHHH")
MOTION_RECORD = struct.Struct("<32sHH")
MOTION_SAMPLE = struct.Struct("<ii")
MOTION_MAGIC = b"NCM1"
MOTION_VERSION = 1
MOTION_Q = 1 << 16


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def render_job(job: tuple[str, str, str, str, int, int, str, float]) -> str:
    model, texture, animation, descriptor, time_ms, yaw, output, scale = job
    build_at_time(
        Path(model), Path(texture), Path(animation), time_ms, float(yaw),
        Path(output), write_manifest_file=False, source_scale=scale,
    )
    return output


def parse_animation(value: str) -> tuple[str, Path]:
    descriptor, separator, path = value.partition("=")
    if not separator or not SAFE_DESCRIPTOR.fullmatch(descriptor):
        raise argparse.ArgumentTypeError(
            "animation must be SAFE_DESCRIPTOR=/path/to/owned.x"
        )
    animation = Path(path)
    if not animation.is_file():
        raise argparse.ArgumentTypeError(f"animation not found: {animation}")
    return descriptor, animation


def build_motion_sidecar(
    model: Path,
    animations: list[tuple[str, Path]],
    output: Path,
) -> dict[str, object]:
    """Write sampled retail root transforms used by the iPod locomotion VM."""
    model_data = model.read_bytes()
    records: list[tuple[str, int, int]] = []
    samples: list[tuple[int, int]] = []
    motion_animations = []
    for descriptor, animation in animations:
        animation_data = animation.read_bytes()
        name_offset, _, key_count = locate_tracks(animation_data)
        times = struct.unpack_from(
            f"<{key_count}I", animation_data, name_offset + 252
        )
        period_ms = times[1] - times[0] if key_count > 1 else 1
        duration_ms = key_count * period_ms
        pose_count = max(1, math.ceil(duration_ms * POSE_FPS / 1000))
        first_sample = len(samples)
        origin_x = origin_z = None
        for frame in range(pose_count):
            time_ms = times[0] + frame * 1000 // POSE_FPS
            local = animation_track_matrices_at(animation_data, time_ms)
            root = animation_world_matrices(model_data, local)["Frame_Bip01"][
                3, :3
            ]
            if origin_x is None:
                origin_x, origin_z = float(root[0]), float(root[2])
            samples.append((
                round((float(root[0]) - origin_x) * MOTION_Q),
                round((float(root[2]) - origin_z) * MOTION_Q),
            ))
        records.append((descriptor, first_sample, pose_count))
        motion_animations.append({
            "descriptor": descriptor,
            "samples": pose_count,
            "root_x_end_q16": samples[-1][0],
            "root_z_end_q16": samples[-1][1],
        })

    payload = bytearray(MOTION_HEADER.pack(
        MOTION_MAGIC, MOTION_VERSION, POSE_FPS, len(records), len(samples)
    ))
    for descriptor, first_sample, sample_count in records:
        encoded = descriptor.encode("ascii")
        if len(encoded) >= 32:
            raise ValueError(f"motion descriptor too long: {descriptor}")
        payload.extend(MOTION_RECORD.pack(
            encoded.ljust(32, b"\0"), first_sample, sample_count
        ))
    for root_x, root_z in samples:
        payload.extend(MOTION_SAMPLE.pack(root_x, root_z))
    temporary = output.with_suffix(output.suffix + ".tmp")
    temporary.write_bytes(payload)
    temporary.replace(output)
    return {
        "file": output.name,
        "format": "NCM1",
        "version": MOTION_VERSION,
        "pose_fps": POSE_FPS,
        "sample_count": len(samples),
        "size": len(payload),
        "sha256": sha256(output),
        "animations": motion_animations,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("model", type=Path)
    parser.add_argument("texture", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument(
        "--animation", action="append", type=parse_animation, required=True,
        help="retail descriptor=owned animation path; may be repeated",
    )
    parser.add_argument(
        "--workers", type=int,
        default=max(1, min(8, os.cpu_count() or 1)),
    )
    parser.add_argument("--source-scale", type=float, default=SOURCE_SCALE)
    parser.add_argument(
        "--motion-only", action="store_true",
        help="refresh only owned-character-motion.ncm and its manifest data",
    )
    args = parser.parse_args()
    if not args.model.is_file() or not args.texture.is_file():
        parser.error("owned model and texture must exist")
    if not 1 <= args.workers <= 32:
        parser.error("--workers must be between 1 and 32")

    args.output.mkdir(parents=True, exist_ok=True)
    if not 0.125 <= args.source_scale <= 1.0:
        parser.error("--source-scale must be between 0.125 and 1")
    jobs: list[tuple[str, str, str, str, int, int, str, float]] = []
    animations = []
    for descriptor, animation in args.animation:
        animation_data = animation.read_bytes()
        name_offset, _, key_count = locate_tracks(animation_data)
        times = struct.unpack_from(
            f"<{key_count}I", animation_data, name_offset + 252
        )
        period_ms = times[1] - times[0] if key_count > 1 else 1
        duration_ms = key_count * period_ms
        pose_count = max(1, math.ceil(duration_ms * POSE_FPS / 1000))
        animations.append(
            {
                "descriptor": descriptor,
                "resource": animation.name,
                "authored_key_frames": key_count,
                "pose_frames": pose_count,
                "duration_ms": duration_ms,
                "key_period_ms": period_ms,
                "sha256": sha256(animation),
            }
        )
        for yaw in DIRECTIONS:
            for frame in range(pose_count):
                time_ms = times[0] + frame * 1000 // POSE_FPS
                output = (
                    args.output / descriptor / f"{yaw:03d}" /
                    f"{frame:03d}.ncs"
                )
                jobs.append((
                    str(args.model), str(args.texture), str(animation),
                    descriptor, time_ms, yaw, str(output), args.source_scale,
                ))

    if not args.motion_only:
        with concurrent.futures.ProcessPoolExecutor(
            max_workers=args.workers
        ) as executor:
            for _ in executor.map(render_job, jobs):
                pass

    motion = build_motion_sidecar(
        args.model, args.animation, args.output / "owned-character-motion.ncm"
    )
    manifest = {
        "format": "NiBiRu owned authored character pose set",
        "version": 2,
        "screen_capture_derived": False,
        "directions_degrees": list(DIRECTIONS),
        "pose_fps": POSE_FPS,
        "pose_count": len(jobs),
        "source_pixels_per_world_unit": args.source_scale,
        "model": {args.model.name: sha256(args.model)},
        "texture": {args.texture.name: sha256(args.texture)},
        "animations": animations,
        "motion": motion,
    }
    manifest_path = args.output / "owned-character-set.json"
    manifest_path.write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    print(json.dumps(manifest, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
