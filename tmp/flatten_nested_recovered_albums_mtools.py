#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import re
import subprocess
import unicodedata
from collections import defaultdict
from pathlib import PurePosixPath


def ascii_part(part: str, is_file: bool = False) -> str:
    part = unicodedata.normalize("NFKD", part).encode("ascii", "ignore").decode("ascii")
    part = re.sub(r'[<>:"/\\|?*;\x00-\x1f]', "_", part)
    part = re.sub(r"\s+", " ", part).strip(" .")
    if is_file and "." in part:
        stem, ext = part.rsplit(".", 1)
        ext = "." + ext[:8]
        stem = stem[: max(1, 96 - len(ext))].rstrip(" .")
        return (stem + ext) or "Unknown"
    return part[:96] or "Unknown"


def normalize_dest(path: str) -> str:
    posix = PurePosixPath(path)
    parts = [part for part in posix.parts if part not in ("", ".")]
    return "/".join(ascii_part(part, index == len(parts) - 1) for index, part in enumerate(parts))


def mpath(path: str) -> str:
    return "::" + path


def run(cmd: list[str], timeout: int = 120) -> subprocess.CompletedProcess[str]:
    return subprocess.run(cmd, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=timeout)


def try_rename(device: str, old: str, new: str) -> bool:
    proc = run(["mren", "-D", "s", "-i", device, mpath(old), mpath(new)], timeout=120)
    return proc.returncode == 0


def move_file(device: str, old: str, new: str) -> tuple[str, bool]:
    if try_rename(device, old, new):
        return "moved", False
    root, dot, ext = new.rpartition(".")
    if not dot or "/" in ext:
        root, ext = new, ""
    else:
        ext = "." + ext
    for idx in range(1, 100):
        suffix = " [Recovered]" if idx == 1 else f" [Recovered {idx}]"
        candidate = f"{root}{suffix}{ext}"
        if try_rename(device, old, candidate):
            return "moved_collision", True
    return "skipped", False


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("device")
    parser.add_argument("manifest")
    args = parser.parse_args()

    prefix = "Music/Recovered/"
    albums: dict[str, set[str]] = defaultdict(set)
    for row in csv.DictReader(open(args.manifest, newline="", encoding="utf-8"), delimiter="\t"):
        recovered = normalize_dest(row.get("destination", ""))
        if not recovered.startswith(prefix):
            continue
        target = "Music/" + recovered[len(prefix):]
        album = target.rsplit("/", 1)[0]
        albums[album].add(target)

    moved = 0
    skipped = 0
    collisions = 0
    removed_dirs = 0
    for album, targets in sorted(albums.items()):
        nested = f"{album}/{album.rsplit('/', 1)[-1]}"
        album_moved = 0
        for target in sorted(targets):
            filename = target.rsplit("/", 1)[-1]
            old = f"{nested}/{filename}"
            if try_rename(args.device, old, target):
                moved += 1
                album_moved += 1
            else:
                skipped += 1
        if album_moved:
            proc = run(["mdeltree", "-i", args.device, mpath(nested)], timeout=120)
            if proc.returncode == 0:
                removed_dirs += 1
        if moved and moved % 100 == 0:
            print(f"progress moved={moved} skipped={skipped} collisions={collisions} removed_dirs={removed_dirs}", flush=True)

    print(f"summary moved={moved} skipped={skipped} collisions={collisions} removed_dirs={removed_dirs}", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
