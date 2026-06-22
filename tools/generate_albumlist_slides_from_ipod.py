#!/usr/bin/env python3
"""Generate pre-rendered albumlist slideshow BMPs on a mounted iPod."""

from __future__ import annotations

import argparse
import csv
from pathlib import Path

from PIL import Image, ImageOps


COVER_NAMES = (
    "cover.jpg",
    "cover.png",
    "cover.240x240.bmp",
    "cover.160x160.bmp",
    "cover.138x138.bmp",
    "cover.bmp",
    "folder.bmp",
    "albumart.bmp",
)


def first_cover(mount: Path, dirs: str) -> Path | None:
    for raw_dir in dirs.replace(";", "|").split("|"):
        raw_dir = raw_dir.strip().lstrip("/")
        if not raw_dir:
            continue
        base = mount / raw_dir
        for name in COVER_NAMES:
            candidate = base / name
            if candidate.is_file():
                return candidate
    return None


def bmp_size(path: Path) -> tuple[int, int] | None:
    try:
        with Image.open(path) as img:
            return img.size
    except Exception:
        return None


def render_slide(source: Path, target: Path, size: int) -> None:
    with Image.open(source) as img:
        img = img.convert("RGB")
        rendered = ImageOps.fit(img, (size, size), Image.Resampling.LANCZOS)
        target.parent.mkdir(parents=True, exist_ok=True)
        rendered.save(target, "BMP")


def generate(
    mount: Path,
    size: int,
    force: bool,
    output_dir: Path | None,
) -> tuple[dict[str, int], list[str]]:
    index = mount / ".rockbox" / "albumlist" / "index.tsv"
    slides = output_dir or (mount / ".rockbox" / "albumlist" / "slides")
    stats = {"created": 0, "skipped": 0, "missing": 0, "failed": 0}
    failures: list[str] = []

    with index.open("r", encoding="utf-8", newline="") as fh:
        rows = (
            line for line in fh
            if line.strip() and not line.startswith("#")
        )
        reader = csv.DictReader(rows, delimiter="\t")
        for row in reader:
            album_id = (row.get("album_id") or "").strip()
            if not album_id:
                continue

            target = slides / f"{album_id}.bmp"
            if not force and target.is_file() and bmp_size(target) == (size, size):
                stats["skipped"] += 1
                continue

            source = first_cover(mount, row.get("device_dirs") or "")
            if source is None:
                stats["missing"] += 1
                continue

            try:
                render_slide(source, target, size)
                stats["created"] += 1
            except Exception as exc:
                stats["failed"] += 1
                if len(failures) < 8:
                    failures.append(f"{album_id}: {source}: {exc}")

    return stats, failures


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("mount", type=Path)
    parser.add_argument("--size", type=int, default=384)
    parser.add_argument("--force", action="store_true")
    parser.add_argument("--output-dir", type=Path)
    args = parser.parse_args()

    mount = args.mount
    if not (mount / ".rockbox" / "albumlist" / "index.tsv").is_file():
        raise SystemExit(f"missing albumlist index under {mount}")

    stats, failures = generate(mount, args.size, args.force, args.output_dir)
    for key in ("created", "skipped", "missing", "failed"):
        print(f"{key}={stats[key]}")
    for failure in failures:
        print(f"failure={failure}")
    return 1 if stats["failed"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
