#!/usr/bin/env python3
"""Split an oversized device RVP bundle into playable parts."""

from __future__ import annotations

import argparse
import os
from pathlib import Path

CHANNELS = 2


def marker_text(name: str, width: int, height: int, fps: int, sample_rate: int = 44100) -> str:
    stem = Path(name).stem
    return (
        "ROCKPOD_RAW_VIDEO_V1\n"
        f"width={width}\n"
        f"height={height}\n"
        f"fps={fps}\n"
        f"sample_rate={sample_rate}\n"
        f"channels={CHANNELS}\n"
        "fit=contain\n"
        f"video={stem}.yuv\n"
        f"audio={stem}.pcm\n"
    )


def segmented_marker_text(parts: list[tuple[str, str]], width: int, height: int, fps: int, sample_rate: int = 44100) -> str:
    lines = [
        "ROCKPOD_RAW_VIDEO_V1",
        f"width={width}",
        f"height={height}",
        f"fps={fps}",
        f"sample_rate={sample_rate}",
        f"channels={CHANNELS}",
        "fit=contain",
        f"segments={len(parts)}",
    ]
    for index, (video_name, audio_name) in enumerate(parts, start=1):
        lines.append(f"segment{index}_video={video_name}")
        lines.append(f"segment{index}_audio={audio_name}")
    return "\n".join(lines) + "\n"


def _read_marker(path: str) -> dict[str, str]:
    data = {}
    try:
        with open(path, "r", encoding="utf-8") as handle:
            for raw in handle:
                line = raw.strip()
                if not line or "=" not in line:
                    continue
                key, value = line.split("=", 1)
                data[key.strip()] = value.strip()
    except OSError as exc:
        raise SystemExit(f"could not read marker file {path}: {exc}") from exc
    return data


def _int_or_default(value: str | None, default: int) -> int:
    try:
        return int(str(value or "").strip())
    except (TypeError, ValueError):
        return default


def _marker_dims_and_rates(marker_path: str) -> tuple[int, int, int, int]:
    data = _read_marker(marker_path)
    width = _int_or_default(data.get("width"), 320)
    height = _int_or_default(data.get("height"), 240)
    fps = _int_or_default(data.get("fps"), 20)
    sample_rate = _int_or_default(data.get("sample_rate"), 44100)
    return width, height, fps, sample_rate


def copy_exact(src, dest, byte_count: int, chunk_size: int = 4 * 1024 * 1024):
    remaining = byte_count
    while remaining > 0:
        chunk = src.read(min(chunk_size, remaining))
        if not chunk:
            raise RuntimeError(f"short read while writing {dest}")
        dest.write(chunk)
        remaining -= len(chunk)


def main(argv=None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("rvp_path")
    parser.add_argument("--seconds", type=int, default=120)
    parser.add_argument("--remove-original", action="store_true")
    args = parser.parse_args(argv)

    rvp_path = os.path.abspath(args.rvp_path)
    base, ext = os.path.splitext(rvp_path)
    if ext.lower() != ".rvp":
        raise SystemExit("input must be a .rvp file")

    yuv_path = base + ".yuv"
    pcm_path = base + ".pcm"
    if not os.path.isfile(yuv_path) or not os.path.isfile(pcm_path):
        raise SystemExit("matching .yuv and .pcm sidecars are required")

    width, height, fps, sample_rate = _marker_dims_and_rates(rvp_path)
    if width <= 0 or height <= 0:
        raise SystemExit("RVP marker is missing valid width/height")
    if fps <= 0:
        raise SystemExit("RVP marker is missing valid fps")

    frame_bytes = width * height * 3 // 2
    if frame_bytes <= 0:
        raise SystemExit("RVP marker has invalid frame geometry")

    yuv_size = os.path.getsize(yuv_path)
    pcm_size = os.path.getsize(pcm_path)
    if yuv_size % frame_bytes:
        raise SystemExit(f"YUV sidecar is not an exact YUV420 frame stream for width={width} height={height}")

    pcm_bytes_per_frame = max(1, sample_rate * CHANNELS * 2 // fps)
    frames_total = yuv_size // frame_bytes
    frames_per_part = max(fps, int(args.seconds) * fps)
    parts = []
    stem = Path(base).name
    parent = os.path.dirname(base)

    with open(yuv_path, "rb") as yuv_in, open(pcm_path, "rb") as pcm_in:
        frame_start = 0
        part_index = 1
        while frame_start < frames_total:
            part_frames = min(frames_per_part, frames_total - frame_start)
            part_stem = f"{stem}.seg{part_index:02d}"
            part_base = os.path.join(parent, part_stem)
            part_yuv = part_base + ".yuv"
            part_pcm = part_base + ".pcm"

            with open(part_yuv + ".rockpod_tmp", "wb") as yuv_out:
                copy_exact(yuv_in, yuv_out, part_frames * frame_bytes)
                yuv_out.flush()
                os.fsync(yuv_out.fileno())
            os.replace(part_yuv + ".rockpod_tmp", part_yuv)

            pcm_bytes = part_frames * pcm_bytes_per_frame
            if frame_start + part_frames >= frames_total:
                pcm_bytes = pcm_size - pcm_in.tell()
            with open(part_pcm + ".rockpod_tmp", "wb") as pcm_out:
                copy_exact(pcm_in, pcm_out, pcm_bytes)
                pcm_out.flush()
                os.fsync(pcm_out.fileno())
            os.replace(part_pcm + ".rockpod_tmp", part_pcm)

            parts.append((os.path.basename(part_yuv), os.path.basename(part_pcm)))
            frame_start += part_frames
            part_index += 1

    with open(rvp_path + ".rockpod_tmp", "w", encoding="utf-8", newline="\n") as marker:
        width, height, fps, sample_rate = _marker_dims_and_rates(rvp_path)
        marker.write(segmented_marker_text(parts, width, height, fps, sample_rate))
        marker.flush()
        os.fsync(marker.fileno())
    os.replace(rvp_path + ".rockpod_tmp", rvp_path)

    if args.remove_original:
        for path in (yuv_path, pcm_path):
            if os.path.exists(path):
                os.remove(path)

    print(rvp_path)
    for video_name, audio_name in parts:
        print(os.path.join(parent, video_name))
        print(os.path.join(parent, audio_name))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
