#!/usr/bin/env python3
"""Generate full-height SBS preview art for mounted iPod videos and games."""

from __future__ import annotations

import argparse
import hashlib
import os
import re
import subprocess
import tempfile
from pathlib import Path

from PIL import Image


VIDEO_EXTS = {".avi", ".mkv", ".mp4", ".mpeg", ".mpg", ".m4v"}
PREVIEW_SIZE = (174, 240)
THUMB_SIZE = (32, 32)


def clean_title(value: str) -> str:
    text = Path(value).stem
    text = text.replace("_", " ")
    text = re.sub(r"\s+", " ", text).strip()
    return text or "Untitled Video"


def normalized_key(value: str) -> str:
    return re.sub(r"[^a-z0-9]+", "", value.casefold())


def video_id(material: str) -> str:
    return hashlib.sha1(material.encode("utf-8", "replace")).hexdigest()[:24]


def image_fit(source: Path, target: Path, size: tuple[int, int]) -> None:
    with Image.open(source) as frame:
        frame = frame.convert("RGB")
        frame.thumbnail(size, Image.LANCZOS)
        canvas = Image.new("RGB", size, "black")
        left = max((size[0] - frame.width) // 2, 0)
        top = max((size[1] - frame.height) // 2, 0)
        canvas.paste(frame, (left, top))
        canvas.save(target, "BMP")


def image_fill(source: Path, target: Path, size: tuple[int, int]) -> None:
    with Image.open(source) as frame:
        frame = frame.convert("RGB")
        frame = trim_dark_border(frame)
        source_ratio = frame.width / max(frame.height, 1)
        target_ratio = size[0] / max(size[1], 1)
        if source_ratio > target_ratio:
            scaled_height = size[1]
            scaled_width = int(size[1] * source_ratio)
        else:
            scaled_width = size[0]
            scaled_height = int(size[0] / max(source_ratio, 0.01))
        frame = frame.resize((scaled_width, scaled_height), Image.LANCZOS)
        left = max((scaled_width - size[0]) // 2, 0)
        top = max((scaled_height - size[1]) // 2, 0)
        canvas = frame.crop((left, top, left + size[0], top + size[1]))
        canvas.save(target, "BMP")


def trim_dark_border(frame: Image.Image) -> Image.Image:
    mask = frame.convert("L").point(lambda value: 255 if value > 18 else 0)
    bbox = mask.getbbox()
    if not bbox:
        return frame
    left, top, right, bottom = bbox
    border_x = left + (frame.width - right)
    border_y = top + (frame.height - bottom)
    if border_x < 4 and border_y < 4:
        return trim_dark_letterbox_rows(frame)
    if right - left < frame.width // 2 or bottom - top < frame.height // 2:
        return trim_dark_letterbox_rows(frame)
    return trim_dark_letterbox_rows(frame.crop(bbox))


def trim_dark_letterbox_rows(frame: Image.Image) -> Image.Image:
    if frame.height < 8 or frame.width < 8:
        return frame
    pixels = frame.load()
    limit = max(frame.height // 4, 1)
    threshold = frame.width // 3

    def dark_count(y: int) -> int:
        return sum(1 for x in range(frame.width) if max(pixels[x, y]) < 24)

    top = 0
    while top < limit and dark_count(top) > threshold:
        top += 1

    bottom = frame.height
    while bottom - 1 > frame.height - limit and dark_count(bottom - 1) > threshold:
        bottom -= 1

    if top == 0 and bottom == frame.height:
        return frame
    if bottom - top < frame.height // 2:
        return frame
    return frame.crop((0, top, frame.width, bottom))


def collect_sources(search_roots: list[Path]) -> dict[str, Path]:
    sources: dict[str, Path] = {}
    for root in search_roots:
        if not root.is_dir():
            continue
        for path in root.rglob("*"):
            if path.suffix.casefold() in VIDEO_EXTS and path.is_file():
                sources.setdefault(normalized_key(path.stem), path)
    return sources


def find_source_for_rvp(rvp: Path, sources: dict[str, Path]) -> Path | None:
    key = normalized_key(rvp.stem)
    if key in sources:
        return sources[key]
    best_path = None
    best_score = 0
    for source_key, path in sources.items():
        score = 0
        for token in re.findall(r"[a-z0-9]+", rvp.stem.casefold()):
            if len(token) >= 3 and token in source_key:
                score += len(token)
        if score > best_score:
            best_path = path
            best_score = score
    return best_path if best_score >= 12 else None


def extract_frame(ffmpeg: str, source: Path, target: Path) -> bool:
    for seek in ("30", "10", "3"):
        command = [
            ffmpeg,
            "-hide_banner",
            "-loglevel",
            "error",
            "-y",
            "-ss",
            seek,
            "-i",
            str(source),
            "-frames:v",
            "1",
            str(target),
        ]
        result = subprocess.run(command, check=False)
        if result.returncode == 0 and target.exists() and target.stat().st_size:
            return True
    return False


def classify_video(path: Path) -> tuple[str, str, str]:
    title = clean_title(path.name)
    lowered = title.casefold()
    if "recess" in lowered:
        episode = re.sub(r"(?i)^disney'?s\s+recess\s*-\s*", "", title).strip()
        return episode or title, "show", "Recess / Season 03"
    if "kenny" in lowered and "spenny" in lowered:
        episode = re.sub(r"(?i)\s*-\s*kenny\s+vs\.?\s+spenny.*$", "", title).strip()
        return episode or title, "show", "Kenny vs. Spenny / Season 03"
    return title, "movie", title


def relpath_for_manifest(path: Path, mount: Path) -> str:
    return path.relative_to(mount).as_posix()


def generate_video_assets(mount: Path, source_roots: list[Path], ffmpeg: str) -> int:
    video_root = mount / "Videos"
    list_root = mount / ".rockbox" / "videolist"
    thumbs = list_root / "thumbs"
    previews = list_root / "previews"
    thumbs.mkdir(parents=True, exist_ok=True)
    previews.mkdir(parents=True, exist_ok=True)
    sources = collect_sources(source_roots)
    rows = []
    rendered = 0

    with tempfile.TemporaryDirectory(prefix="rockpod-video-previews-") as tmp:
        tmpdir = Path(tmp)
        for rvp in sorted(video_root.rglob("*.rvp")):
            source = find_source_for_rvp(rvp, sources)
            title, kind, group = classify_video(source or rvp)
            identity = relpath_for_manifest(rvp, mount)
            item_id = video_id(identity)
            frame = tmpdir / f"{item_id}.jpg"
            if source and extract_frame(ffmpeg, source, frame):
                image_fit(frame, thumbs / f"{item_id}.bmp", THUMB_SIZE)
                image_fill(frame, previews / f"{item_id}.bmp", PREVIEW_SIZE)
                rendered += 1
            rows.append(
                [
                    item_id,
                    f"thumbs/{item_id}.bmp",
                    f"previews/{item_id}.bmp",
                    title,
                    kind,
                    group,
                    identity,
                ]
            )

    lines = [
        "# rockpod videolist v1",
        "video_id\tthumb\tpreview\ttitle\tkind\tgroup_key\tdevice_path",
    ]
    lines.extend("\t".join(field.replace("\t", " ") for field in row) for row in rows)
    (list_root / "index.tsv").write_text("\n".join(lines) + "\n", encoding="utf-8")
    return rendered


def generate_game_previews(mount: Path) -> int:
    count = 0
    cover_dirs = (
        mount / "gameboy",
        mount / "PokeMini",
        mount / ".rockbox" / "rocks" / "games" / "rockboy_launcher" / "covers",
        mount / ".rockbox" / "rocks" / "games" / "pokemini_launcher" / "covers",
    )
    for root in cover_dirs:
        if not root.is_dir():
            continue
        for cover in sorted(root.glob("*.bmp")):
            if cover.name.endswith(".pane.bmp"):
                continue
            target = cover.with_name(f"{cover.stem}.pane.bmp")
            image_fill(cover, target, PREVIEW_SIZE)
            count += 1
    return count


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("mount", type=Path)
    parser.add_argument(
        "--source-root",
        action="append",
        type=Path,
        default=[Path.home() / "Videos"],
    )
    parser.add_argument("--ffmpeg", default="ffmpeg")
    args = parser.parse_args()
    mount = args.mount
    if not mount.is_dir():
        raise SystemExit(f"mount path not found: {mount}")
    video_count = generate_video_assets(mount, args.source_root, args.ffmpeg)
    game_count = generate_game_previews(mount)
    print(f"generated {video_count} video previews and {game_count} game previews")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
