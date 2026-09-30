#!/usr/bin/env python3
"""Build an iPod-fast NiBiRu scene stream from owned extracted resources.

Every frame is rasterized from the retail meshes, skin weights, animation
matrices, and texture atlases.  The original executable is never launched and
no framebuffer/video capture is consumed.
"""

from __future__ import annotations

import argparse
import concurrent.futures
import hashlib
import json
import os
import shutil
import struct
import subprocess
from pathlib import Path

import numpy as np
from PIL import Image

from nibiru_model_probe import locate_tracks, probe_mesh
from nibiru_render_owned_model import (
    HEIGHT,
    WIDTH,
    animation_track_matrices,
    animation_track_matrices_at,
    compose_room,
    render,
    skin_vertices,
)


MAGIC = b"NBS1"
VERSION = 2
RENDERER_VERSION = 7
SOURCE_KEY_DELAY_MS = 150
FRAME_RATE = 24
HEADER = struct.Struct("<4sHHHHHHII")
ENTRY = struct.Struct("<IIHHHH")
SEQUENCE = (
    ("martin_intro_intro.x", None),
    ("martin_intro_mluvi1.x", "martin_intro_mluvi1.x"),
    ("martin_intro_mluvi2.x", "martin_intro_mluvi2.x"),
    ("martin_intro_mluvi3.x", "martin_intro_mluvi3.x"),
)


def render_pose(job: tuple[str, ...]) -> str:
    (
        model, martin_texture, objects_texture, animation, background,
        foreground, cache, time_text,
    ) = job
    image = render(
        Path(model), Path(martin_texture), None, Path(objects_texture),
        Path(animation) if animation else None, Path(background),
        Path(foreground), None, int(time_text),
    )
    cache_path = Path(cache)
    temporary = cache_path.with_suffix(cache_path.suffix + ".tmp")
    image.save(temporary, format="PNG", optimize=False)
    temporary.replace(cache_path)
    return cache


def build_audio_sidecars(source: Path, output_dir: Path) -> list[Path]:
    """Transcode owned compressed samples for the bounded Rockbox PCM path."""
    ffmpeg = shutil.which("ffmpeg")
    compressed = []
    for path in sorted(source.iterdir()):
        if not path.is_file():
            continue
        with path.open("rb") as stream:
            if stream.read(4) == b"OggS":
                compressed.append(path)
    if compressed and ffmpeg is None:
        raise FileNotFoundError("ffmpeg is required for NiBiRu Ogg PCM sidecars")
    output_dir.mkdir(parents=True, exist_ok=True)
    generated = []
    for path in compressed:
        destination = output_dir / f"{path.stem}.wav"
        if (
            destination.is_file()
            and destination.stat().st_mtime_ns >= path.stat().st_mtime_ns
        ):
            generated.append(destination)
            continue
        temporary = destination.with_suffix(".wav.tmp")
        subprocess.run(
            [
                ffmpeg,
                "-nostdin",
                "-v",
                "error",
                "-y",
                "-i",
                str(path),
                "-acodec",
                "pcm_s16le",
                "-ar",
                "44100",
                "-ac",
                "2",
                "-f",
                "wav",
                str(temporary),
            ],
            check=True,
        )
        temporary.replace(destination)
        generated.append(destination)
        print(f"direct ISO audio: {path.name} -> {destination.name}", flush=True)
    return generated


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def frame_times(path: Path) -> list[int]:
    data = path.read_bytes()
    name_offset, record_size, frame_count = locate_tracks(data)
    times = list(struct.unpack_from(f"<{frame_count}I", data, name_offset + 252))
    if any(time % SOURCE_KEY_DELAY_MS for time in times):
        raise ValueError(f"{path.name}: animation time is not on a 150 ms tick")
    track_count = (len(data) - name_offset) // record_size
    for index in range(1, track_count):
        offset = name_offset + index * record_size + 252
        if list(struct.unpack_from(f"<{frame_count}I", data, offset)) != times:
            raise ValueError(f"{path.name}: skeleton tracks have different times")
    return times


def validate_authored_motion(source: Path) -> dict[str, float]:
    """Reject the old static-chair or world-zero root interpretation."""
    intro = (source / "martin_intro_intro.x").read_bytes()
    _, _, frame_count = locate_tracks(intro)
    first = animation_track_matrices(intro, 0)
    last = animation_track_matrices(intro, frame_count - 1)
    root_travel_z = float(
        last["Frame_Bip01"][3, 2] - first["Frame_Bip01"][3, 2]
    )
    chair_travel_z = float(
        last["Frame_Cylinder02"][3, 2]
        - first["Frame_Cylinder02"][3, 2]
    )
    chair_rotation_change = float(np.max(np.abs(
        last["Frame_Cylinder02"][:3, :3]
        - first["Frame_Cylinder02"][:3, :3]
    )))
    first_phone_distance = abs(
        first["Frame_Bip01"][3, 2] - first["Frame_sluchatko"][3, 2]
    )
    last_phone_distance = abs(
        last["Frame_Bip01"][3, 2] - last["Frame_sluchatko"][3, 2]
    )
    if (
        abs(root_travel_z) < 50.0
        or abs(chair_travel_z) < 50.0
        or abs(root_travel_z - chair_travel_z) > 5.0
        or chair_rotation_change < 0.25
        or last_phone_distance >= first_phone_distance
    ):
        raise ValueError(
            "intro tracks do not roll Martin and his chair toward the phone"
        )
    return {
        "root_travel_z": root_travel_z,
        "chair_travel_z": chair_travel_z,
        "chair_rotation_change": chair_rotation_change,
        "phone_distance_start_z": float(first_phone_distance),
        "phone_distance_end_z": float(last_phone_distance),
    }


def validate_authored_skinning(source: Path) -> dict[str, float]:
    """Reject the old bit-mask interpretation of direct retail bone indices."""
    model_data = (source / "martin_intro_intro.x").read_bytes()
    mesh = probe_mesh(model_data)
    if mesh is None:
        raise ValueError("Martin mesh missing from intro model")
    faces = np.frombuffer(
        model_data,
        dtype="<u4",
        count=int(mesh["face_count"]) * 3,
        offset=int(mesh["face_offset"]),
    ).reshape(-1, 3)
    worst_edge = 0.0
    minimum_height = float("inf")
    maximum_height = 0.0
    for time_ms in (900, 33000, 40050, 47250):
        local = animation_track_matrices_at(model_data, time_ms)
        vertices = skin_vertices(model_data, mesh, 0, model_data, local)
        if not np.all(np.isfinite(vertices)):
            raise ValueError("Martin skin contains non-finite vertices")
        height = float(np.ptp(vertices[:, 1]))
        minimum_height = min(minimum_height, height)
        maximum_height = max(maximum_height, height)
        for corner in range(3):
            edge_lengths = np.linalg.norm(
                vertices[faces[:, corner]]
                - vertices[faces[:, (corner + 1) % 3]],
                axis=1,
            )
            worst_edge = max(worst_edge, float(edge_lengths.max()))
    if worst_edge > 32.0 or minimum_height < 125.0 or maximum_height > 170.0:
        raise ValueError(
            "Martin skin is torn or outside the authored seated scale "
            f"(edge={worst_edge:.3f}, height={minimum_height:.3f}.."
            f"{maximum_height:.3f})"
        )
    return {
        "maximum_triangle_edge": worst_edge,
        "minimum_pose_height": minimum_height,
        "maximum_pose_height": maximum_height,
        "bone_indices": "direct uint32 retail indices",
    }


def encode_delta(image, previous) -> tuple[int, int, int, int, bytes]:
    rgb_frame = np.asarray(image.convert("RGB"))
    rgb_previous = np.asarray(previous.convert("RGB"))
    changed = np.any(rgb_frame != rgb_previous, axis=2)
    ys, xs = np.nonzero(changed)
    if not len(xs):
        return 0, 0, 0, 0, b""
    x = int(xs.min())
    y = int(ys.min())
    width = int(xs.max()) - x + 1
    height = int(ys.max()) - y + 1
    crop_changed = changed[y : y + height, x : x + width].reshape(-1)
    mask = np.packbits(crop_changed, bitorder="little").tobytes()
    rgb = rgb_frame[y : y + height, x : x + width].reshape(-1, 3)[crop_changed]
    rgb565 = (
        ((rgb[:, 0].astype(np.uint16) >> 3) << 11)
        | ((rgb[:, 1].astype(np.uint16) >> 2) << 5)
        | (rgb[:, 2].astype(np.uint16) >> 3)
    ).astype("<u2")
    return x, y, width, height, mask + rgb565.tobytes()


def validate_rendered_projection(
    cache_paths: dict[tuple[str, int], Path], room: Image.Image
) -> dict[str, object]:
    """Lock Martin's retail camera scale, seated anchor and room scissor."""
    intro_keys = [key for key in cache_paths if key[0] == "martin_intro_intro.x"]
    if not intro_keys:
        raise ValueError("rendered intro poses missing")
    first = min(intro_keys, key=lambda key: key[1])
    roll = min(intro_keys, key=lambda key: abs(key[1] - 40050))
    room_pixels = np.asarray(room.convert("RGB"))
    bounds: dict[str, list[int]] = {}
    for label, key in (("seated", first), ("chair_roll", roll)):
        pixels = np.asarray(Image.open(cache_paths[key]).convert("RGB"))
        if np.any(pixels[round(696 * 5 / 16) :, :]):
            raise ValueError(f"{label} pose escapes room 1864 retail scissor")
        ys, xs = np.nonzero(np.any(pixels != room_pixels, axis=2))
        if len(xs) < 1800:
            raise ValueError(f"{label} pose has insufficient model coverage")
        bounds[label] = [
            int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max())
        ]
    seated = bounds["seated"]
    rolled = bounds["chair_roll"]
    seated_width = seated[2] - seated[0] + 1
    seated_height = seated[3] - seated[1] + 1
    if (
        not 54 <= seated_width <= 62
        or not 78 <= seated_height <= 84
        or not 120 <= seated[0] <= 126
        or not 88 <= seated[1] <= 93
        or not 168 <= seated[3] <= 173
    ):
        raise ValueError(f"Martin seated projection scale/anchor changed: {seated}")
    seated_center = (seated[0] + seated[2]) / 2
    rolled_center = (rolled[0] + rolled[2]) / 2
    if rolled_center >= seated_center - 8:
        raise ValueError("Martin/chair projection does not roll toward the phone")
    return {
        "camera": {
            "pitch_degrees": 13,
            "distance": 210,
            "fov_degrees": 50,
        },
        "seated_bounds_320x240": seated,
        "chair_roll_bounds_320x240": rolled,
        "retail_clip_canvas_y": 696,
    }


def build(
    source: Path, output: Path, max_frames: int | None, cache_dir: Path,
    workers: int,
) -> dict[str, object]:
    model = source / "martin_intro_intro.x"
    martin_texture = source / "Martin_podzim-int.bmp"
    objects_texture = source / "objektyintro.bmp"
    background = source / "intro_byt.bmp"
    foreground = source / "intro_byt_stul.bmp"
    required = [model, martin_texture, objects_texture, background, foreground]
    required.extend(source / name for name, _ in SEQUENCE[1:])
    missing = [str(path) for path in required if not path.is_file()]
    if missing:
        raise FileNotFoundError("missing extracted resource(s): " + ", ".join(missing))
    authored_motion = validate_authored_motion(source)
    authored_skinning = validate_authored_skinning(source)

    unique_frames: dict[tuple[str, int], tuple[int, int, int, int, bytes]] = {}
    rendered_count = 0
    room = compose_room(background, foreground)
    # The runtime must not depend on the AGDS script having finished drawing
    # an equivalent office framebuffer before the first 24 fps pose arrives.
    # Make frame zero a self-contained room+model keyframe; every later entry
    # remains a sparse absolute delta over the preceding rendered pose.
    previous = Image.new("RGB", (WIDTH, HEIGHT), (0, 0, 0))
    cache_dir = cache_dir / f"renderer-v{RENDERER_VERSION}"
    cache_dir.mkdir(parents=True, exist_ok=True)
    resources = []
    for resource_name, animation_name in SEQUENCE:
        resource = source / resource_name
        times = frame_times(resource)
        resources.append((resource_name, animation_name, times))

    last_time = max(times[-1] for _, _, times in resources)
    frame_keys: list[tuple[str, int] | None] = []
    # Loading an AGDS animation presents its first authored pose immediately;
    # timestamps offset that pose's phase clock, not the object's visibility.
    # Starting with no resource made Martin disappear for the first 900 ms.
    current_resource = resources[0]
    frame_count = (last_time * FRAME_RATE + 999) // 1000 + 1
    for slot in range(frame_count):
        time_ms = min(
            last_time,
            (slot * 1000 + FRAME_RATE // 2) // FRAME_RATE,
        )
        for candidate in resources:
            if candidate[2][0] <= time_ms:
                current_resource = candidate
        if current_resource is None:
            frame_keys.append(None)
            continue
        resource_name, _, times = current_resource
        frame_keys.append((resource_name, min(time_ms, times[-1])))
        if max_frames is not None and len(frame_keys) >= max_frames:
            break

    ordered_keys = list(dict.fromkeys(
        key for key in frame_keys if key is not None
    ))
    cache_paths = {}
    jobs = []
    for key in ordered_keys:
        resource_name, time_ms = key
        animation_name = next(
            animation for name, animation, _ in resources
            if name == resource_name
        )
        cache = cache_dir / f"{resource_name}.{time_ms:06d}.png"
        cache_paths[key] = cache
        if not cache.is_file():
            jobs.append((
                str(model), str(martin_texture), str(objects_texture),
                str(source / animation_name) if animation_name else "",
                str(background), str(foreground), str(cache), str(time_ms),
            ))
    if jobs:
        with concurrent.futures.ProcessPoolExecutor(
            max_workers=max(1, workers)
        ) as executor:
            for completed, _ in enumerate(executor.map(render_pose, jobs), 1):
                if completed == 1 or completed % 25 == 0:
                    print(
                        f"direct ISO raster: {completed}/{len(jobs)} "
                        "interpolated pose(s)", flush=True,
                    )

    authored_projection = validate_rendered_projection(cache_paths, room)
    for key in ordered_keys:
        resource_name, time_ms = key
        image = Image.open(cache_paths[key]).convert("RGB").copy()
        unique_frames[key] = encode_delta(image, previous)
        previous = image
        rendered_count += 1
        if rendered_count == 1 or rendered_count % 10 == 0:
            print(
                f"direct ISO render: {rendered_count} interpolated pose(s), "
                f"{resource_name} at {time_ms} ms",
                flush=True,
            )

    if not unique_frames:
        raise ValueError("no frames rendered")
    table_offset = HEADER.size
    data_offset = table_offset + len(frame_keys) * ENTRY.size
    payload = bytearray()
    offsets: dict[tuple[str, int], tuple[int, int]] = {}
    for key, (_, _, _, _, encoded) in unique_frames.items():
        offsets[key] = (data_offset + len(payload), len(encoded))
        payload.extend(encoded)
    table = bytearray()
    for key in frame_keys:
        if key is None:
            table.extend(ENTRY.pack(data_offset, 0, 0, 0, 0, 0))
            continue
        x, y, width, height, _ = unique_frames[key]
        offset, size = offsets[key]
        table.extend(ENTRY.pack(offset, size, x, y, width, height))

    header = HEADER.pack(
        MAGIC,
        VERSION,
        WIDTH,
        HEIGHT,
        len(frame_keys),
        FRAME_RATE,
        ENTRY.size,
        table_offset,
        data_offset,
    )
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_suffix(output.suffix + ".tmp")
    temporary.write_bytes(header + table + payload)
    temporary.replace(output)
    manifest = {
        "format": "NiBiRu direct-owned scene stream",
        "version": VERSION,
        "renderer_version": RENDERER_VERSION,
        "retail_room_light": {
            "opcode_object": "1864.11b6",
            "index": 0,
            "position_xyzw": [0, 40, 70, 0],
            "ambient_rgb": [10, 10, 20],
            "diffuse_rgb": [90, 211, 252],
            "specular_rgb": [255, 255, 255],
            "material_diffuse_rgba": [1.0, 1.0, 1.0, 1.0],
            "material_power": 3.0,
        },
        "authored_motion": authored_motion,
        "authored_skinning": authored_skinning,
        "authored_projection": authored_projection,
        "screen_capture_derived": False,
        "canvas": [WIDTH, HEIGHT],
        "frame_rate": FRAME_RATE,
        "frame_period_ms": f"1000/{FRAME_RATE}",
        "timeline_frames": len(frame_keys),
        "unique_frames": len(unique_frames),
        "encoding": "sparse absolute RGB565 deltas over the exact room composite",
        "self_contained_first_frame": True,
        "size": output.stat().st_size,
        "sha256": sha256(output),
        "sources": {path.name: sha256(path) for path in required},
    }
    output.with_suffix(output.suffix + ".json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="extracted first-cutscene directory")
    parser.add_argument("output", type=Path)
    parser.add_argument(
        "--max-frames", type=int,
        help="development-only limit for a fast format/runtime smoke test",
    )
    parser.add_argument(
        "--cache-dir", type=Path,
        help="resumable direct-render cache (defaults beside the output)",
    )
    parser.add_argument(
        "--audio-only", action="store_true",
        help="only build owned Ogg-to-PCM sidecars for runtime testing",
    )
    parser.add_argument(
        "--workers", type=int,
        default=min(6, os.cpu_count() or 1),
        help="parallel owned-model raster workers (default: up to 6)",
    )
    args = parser.parse_args()
    sidecars = build_audio_sidecars(args.source, args.output.parent / "audio")
    if args.audio_only:
        print(json.dumps({"audio_sidecars": [str(path) for path in sidecars]}, indent=2))
        return 0
    cache_dir = args.cache_dir or args.output.with_name(args.output.name + ".frames")
    if args.workers < 1:
        parser.error("--workers must be positive")
    manifest = build(
        args.source, args.output, args.max_frames, cache_dir, args.workers
    )
    print(json.dumps(manifest, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
