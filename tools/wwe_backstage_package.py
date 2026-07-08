#!/usr/bin/env python3
"""Build a Rockbox WWE Backstage package from a local branching manifest.

Input is either a .twv-style manifest whose node video= fields point at local
source videos, or the Flashpoint/TweeVee game.tw file. The output package is
ready to copy below:

    .rockbox/rocks/games/wwe_backstage/

The script intentionally does not download videos. Use preserved Flashpoint
assets or other local files the user is authorized to convert.
"""

from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
WIDTH = 320
HEIGHT = 240
FPS = 20
SAMPLE_RATE = 44100
CHANNELS = 2
PCM_BYTES_PER_SAMPLE = 2
SEGMENT_YUV_LIMIT = 512 * 1024 * 1024


def safe_stem(value: str) -> str:
    text = re.sub(r"[^A-Za-z0-9._ -]+", "_", value or "node").strip(" ._-")
    return (text or "node")[:64]


def run(command: list[str]) -> None:
    print("+", " ".join(command), flush=True)
    subprocess.run(command, check=True)


def read_manifest(path: Path) -> tuple[list[tuple[str, str, str]], str, str]:
    """Return parsed lines plus title/start metadata.

    Each line tuple is (kind, key, value). kind is one of raw, global, section,
    or node. For node lines, key is "<node_id>\0<field>".
    """

    parsed: list[tuple[str, str, str]] = []
    title = "Can You Survive Backstage in WWE?"
    start = ""
    current_node = ""

    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or line.startswith(";"):
            parsed.append(("raw", raw, ""))
            continue
        if line.startswith("[") and line.endswith("]"):
            section = line[1:-1].strip()
            parsed.append(("section", section, ""))
            current_node = ""
            if section.lower().startswith("node "):
                current_node = section[5:].strip()
                if not start:
                    start = current_node
            continue
        if "=" not in line:
            parsed.append(("raw", raw, ""))
            continue

        key, value = [item.strip() for item in line.split("=", 1)]
        if current_node:
            parsed.append(("node", f"{current_node}\0{key}", value))
        else:
            parsed.append(("global", key, value))
            if key.lower() == "title":
                title = value
            elif key.lower() == "start":
                start = value

    return parsed, title, start


def read_twee(path: Path) -> tuple[list[tuple[str, str, str]], str, str]:
    parsed: list[tuple[str, str, str]] = []
    title = "Can You Survive Backstage in WWE?"
    start = "Start"
    current_node = ""
    choice_count = 0

    parsed.append(("global", "title", title))
    parsed.append(("global", "start", start))
    parsed.append(("global", "cover", "cover.bmp"))

    for raw in path.read_text(encoding="utf-8-sig").splitlines():
        line = raw.strip()
        if not line:
            continue
        if line.startswith("::"):
            current_node = line[2:].strip().split(None, 1)[0]
            if not current_node:
                raise SystemExit(f"empty passage title in {path}")
            parsed.append(("raw", "", ""))
            parsed.append(("section", f"node {current_node}", ""))
            choice_count = 0
            continue
        if not current_node:
            continue

        video = re.search(r"\{([^|{}\r\n]+)(?:\|[^{}\r\n]*)?\}", line)
        if video:
            parsed.append(("node", f"{current_node}\0video", video.group(1).strip()))
            continue

        for match in re.finditer(r"\[\[([^\]]+)\]\]", line):
            parts = [part.strip() for part in match.group(1).split("|", 1)]
            if len(parts) == 1:
                label = parts[0]
                target = parts[0]
            else:
                label, target = parts
            choice_count += 1
            parsed.append(("node", f"{current_node}\0choice{choice_count}_label", label))
            parsed.append(("node", f"{current_node}\0choice{choice_count}_target", target))

    return parsed, title, start


def find_twee_in_zip(path: Path, temp_dir: Path) -> tuple[Path, Path]:
    with zipfile.ZipFile(path) as archive:
        names = archive.namelist()
        game_tw = next((name for name in names if name.endswith("/game.tw")), "")
        if not game_tw:
            raise SystemExit(f"no game.tw found in {path}")
        root = game_tw.rsplit("/", 1)[0]
        archive.extractall(temp_dir)
    return temp_dir / game_tw, temp_dir / root / "videos"


def read_graph(path: Path) -> tuple[list[tuple[str, str, str]], str, str]:
    if path.suffix.lower() == ".tw":
        return read_twee(path)
    return read_manifest(path)


def write_rvp_marker(path: Path, stem: str, fps: int) -> None:
    path.write_text(
        "\n".join(
            [
                "ROCKPOD_RAW_VIDEO_V1",
                f"width={WIDTH}",
                f"height={HEIGHT}",
                f"fps={fps}",
                f"sample_rate={SAMPLE_RATE}",
                f"channels={CHANNELS}",
                "fit=contain",
                f"video={stem}.yuv",
                f"audio={stem}.pcm",
                "",
            ]
        ),
        encoding="utf-8",
        newline="\n",
    )


def read_marker_values(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}

    for raw in path.read_text(encoding="utf-8").splitlines():
        if "=" not in raw:
            continue
        key, value = raw.split("=", 1)
        values[key.strip()] = value.strip()

    return values


def validate_rvp_marker(path: Path, fps: int) -> None:
    values = read_marker_values(path)
    expected = {
        "width": str(WIDTH),
        "height": str(HEIGHT),
        "fps": str(fps),
        "sample_rate": str(SAMPLE_RATE),
        "channels": str(CHANNELS),
    }

    for key, value in expected.items():
        if values.get(key) != value:
            raise SystemExit(
                f"{path} has {key}={values.get(key)!r}, expected {value!r}"
            )


def convert_video(
    ffmpeg: str,
    source: Path,
    target_rvp: Path,
    fps: int,
    segment_seconds: int,
) -> None:
    stem = target_rvp.stem
    yuv = target_rvp.with_suffix(".yuv")
    pcm = target_rvp.with_suffix(".pcm")
    vf = (
        f"scale={WIDTH}:{HEIGHT}:force_original_aspect_ratio=decrease,"
        f"pad={WIDTH}:{HEIGHT}:(ow-iw)/2:(oh-ih)/2:black,fps={fps}"
    )

    run(
        [
            ffmpeg,
            "-y",
            "-loglevel",
            "error",
            "-i",
            str(source),
            "-vf",
            vf,
            "-an",
            "-pix_fmt",
            "yuv420p",
            "-f",
            "rawvideo",
            str(yuv),
        ]
    )

    audio = subprocess.run(
        [
            ffmpeg,
            "-y",
            "-loglevel",
            "error",
            "-i",
            str(source),
            "-vn",
            "-map",
            "0:a:0?",
            "-ac",
            str(CHANNELS),
            "-ar",
            str(SAMPLE_RATE),
            "-f",
            "s16le",
            str(pcm),
        ]
    )
    if audio.returncode != 0 or not pcm.exists() or pcm.stat().st_size == 0:
        frame_size = WIDTH * HEIGHT * 3 // 2
        frames = max(1, yuv.stat().st_size // frame_size)
        pcm_bytes = frames * SAMPLE_RATE * CHANNELS * PCM_BYTES_PER_SAMPLE // fps
        pcm.write_bytes(b"\0" * pcm_bytes)

    write_rvp_marker(target_rvp, stem, fps)

    frame_size = WIDTH * HEIGHT * 3 // 2
    frames = yuv.stat().st_size // frame_size
    split_for_size = yuv.stat().st_size > SEGMENT_YUV_LIMIT
    split_for_duration = (
        segment_seconds > 0 and frames > max(fps, segment_seconds * fps)
    )
    if split_for_size or split_for_duration:
        splitter = ROOT / "tools" / "rvp_split_device_video.py"
        command = [sys.executable, str(splitter), str(target_rvp)]
        if segment_seconds > 0:
            command.extend(["--seconds", str(segment_seconds)])
        command.append("--remove-original")
        run(command)

    validate_rvp_marker(target_rvp, fps)


def convert_cover(ffmpeg: str, source: Path, output_dir: Path, title: str) -> None:
    cover = output_dir / "cover.bmp"
    covers_dir = output_dir / "covers"
    covers_dir.mkdir(parents=True, exist_ok=True)
    vf = (
        f"scale={WIDTH}:{HEIGHT}:force_original_aspect_ratio=decrease,"
        f"pad={WIDTH}:{HEIGHT}:(ow-iw)/2:(oh-ih)/2:black"
    )
    run(
        [
            ffmpeg,
            "-y",
            "-loglevel",
            "error",
            "-i",
            str(source),
            "-frames:v",
            "1",
            "-vf",
            vf,
            str(cover),
        ]
    )
    shutil.copy2(cover, covers_dir / f"{safe_stem(title)}.bmp")


def build_package(args: argparse.Namespace) -> None:
    ffmpeg = shutil.which(args.ffmpeg) or args.ffmpeg
    if not ffmpeg or not Path(ffmpeg).exists() and shutil.which(ffmpeg) is None:
        raise SystemExit("ffmpeg is required")
    if args.fps < 10 or args.fps > 20:
        raise SystemExit("--fps must be between 10 and 20")
    if args.segment_seconds < 0:
        raise SystemExit("--segment-seconds must be zero or greater")

    manifest = Path(args.manifest).resolve()
    temp_context = tempfile.TemporaryDirectory(prefix="wwe_backstage_zip_")
    temp_path = Path(temp_context.name)
    if manifest.suffix.lower() == ".zip":
        manifest, zip_source_root = find_twee_in_zip(manifest, temp_path)
        source_root = zip_source_root
    else:
        source_root = Path(args.source_root).resolve()
    output_dir = Path(args.output_dir).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    parsed, title, start = read_graph(manifest)
    converted: dict[str, str] = {}
    first_video: Path | None = None

    for kind, key, value in parsed:
        if kind != "node":
            continue
        node_id, field = key.split("\0", 1)
        if field.lower() != "video":
            continue
        source = Path(value)
        if not source.is_absolute():
            source = (source_root / source).resolve()
        if not source.exists():
            raise SystemExit(f"missing source video for node {node_id}: {source}")
        if first_video is None:
            first_video = source
        target = output_dir / f"{safe_stem(node_id)}.rvp"
        convert_video(ffmpeg, source, target, args.fps, args.segment_seconds)
        converted[node_id] = target.name

    cover_source = Path(args.cover).resolve() if args.cover else first_video
    if cover_source and cover_source.exists():
        convert_cover(ffmpeg, cover_source, output_dir, title)

    out_lines: list[str] = []
    wrote_cover = False
    wrote_start = False
    for kind, key, value in parsed:
        if kind == "raw":
            out_lines.append(key)
        elif kind == "section":
            out_lines.append(f"[{key}]")
        elif kind == "global":
            lower = key.lower()
            if lower == "cover":
                out_lines.append("cover=cover.bmp")
                wrote_cover = True
            elif lower == "start":
                out_lines.append(f"start={value or start}")
                wrote_start = True
            else:
                out_lines.append(f"{key}={value}")
        elif kind == "node":
            node_id, field = key.split("\0", 1)
            if field.lower() == "video":
                out_lines.append(f"video={converted[node_id]}")
            else:
                out_lines.append(f"{field}={value}")

    if not wrote_start:
        out_lines.insert(0, f"start={start}")
    if not wrote_cover:
        out_lines.insert(0, "cover=cover.bmp")
    if not any(line.startswith("title=") for line in out_lines):
        out_lines.insert(0, f"title={title}")
    if not out_lines or out_lines[0] != "ROCKPOD_TWEEVEE_V1":
        out_lines.insert(0, "ROCKPOD_TWEEVEE_V1")

    (output_dir / "wwe-backstage.twv").write_text(
        "\n".join(out_lines).rstrip() + "\n",
        encoding="utf-8",
        newline="\n",
    )
    print(f"package written to {output_dir}")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", help="Input .twv graph with local video paths")
    parser.add_argument("--source-root", default=".", help="Base path for relative video fields")
    parser.add_argument("--output-dir", default="wwe_backstage_package")
    parser.add_argument("--cover", default="", help="Optional image/video for cover.bmp")
    parser.add_argument("--ffmpeg", default="ffmpeg")
    parser.add_argument("--fps", type=int, default=FPS,
                        help="Output video FPS, 10..20")
    parser.add_argument("--segment-seconds", type=int, default=0,
                        help="Force RVP segmenting above this duration")
    args = parser.parse_args(argv)
    build_package(args)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
