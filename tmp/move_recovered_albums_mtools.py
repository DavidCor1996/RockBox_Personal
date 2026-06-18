#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import re
import subprocess
import sys
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


def run_mtools(cmd: list[str], timeout: int = 120) -> subprocess.CompletedProcess[str]:
    return subprocess.run(cmd, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=timeout)


def path_exists(device: str, path: str) -> bool:
    proc = run_mtools(["mdir", "-i", device, mpath(path)], timeout=60)
    return proc.returncode == 0


def mkdir_p(device: str, path: str, made: set[str]) -> None:
    current = ""
    for part in path.split("/"):
        if not part:
            continue
        current = f"{current}/{part}" if current else part
        if current in made:
            continue
        if current == "Music" or path_exists(device, current):
            made.add(current)
            continue
        proc = run_mtools(["mmd", "-i", device, mpath(current)], timeout=60)
        text = (proc.stdout or "").lower()
        if proc.returncode and "already exists" not in text and "file exists" not in text:
            print(proc.stdout, file=sys.stderr, end="")
            raise subprocess.CalledProcessError(proc.returncode, proc.args, proc.stdout)
        made.add(current)


def try_rename(device: str, old: str, new: str, timeout: int = 120) -> bool:
    proc = run_mtools(["mren", "-D", "s", "-i", device, mpath(old), mpath(new)], timeout=timeout)
    return proc.returncode == 0


def move_file_with_collision(device: str, old: str, new: str) -> tuple[bool, bool]:
    if try_rename(device, old, new):
        return True, False
    root, dot, ext = new.rpartition(".")
    if not dot or "/" in ext:
        root, ext = new, ""
    else:
        ext = "." + ext
    for idx in range(1, 100):
        suffix = " [Recovered]" if idx == 1 else f" [Recovered {idx}]"
        candidate = f"{root}{suffix}{ext}"
        if try_rename(device, old, candidate):
            return True, True
    return False, False


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("device")
    parser.add_argument("manifest")
    parser.add_argument("--cleanup-empty", action="store_true")
    args = parser.parse_args()

    prefix = "Music/Recovered/"
    albums: dict[str, set[str]] = defaultdict(set)
    with open(args.manifest, newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle, delimiter="\t")
        for row in reader:
            old_file = normalize_dest(row.get("destination", ""))
            if not old_file.startswith(prefix):
                continue
            old_album = old_file.rsplit("/", 1)[0]
            albums[old_album].add(old_file)
            albums[old_album].add(f"{old_album}/cover.jpg")

    made: set[str] = set()
    album_moved = 0
    file_moved = 0
    skipped = 0
    failed = 0
    collisions = 0

    for old_album, files in sorted(albums.items()):
        new_album = "Music/" + old_album[len(prefix):]
        try:
            mkdir_p(args.device, new_album.rsplit("/", 1)[0], made)
        except Exception as exc:
            print(f"failed creating parent for {new_album}: {exc}", file=sys.stderr)
            failed += 1
            continue

        if try_rename(args.device, old_album, new_album):
            album_moved += 1
            if album_moved % 25 == 0:
                print(
                    f"progress album_moved={album_moved} file_moved={file_moved} skipped={skipped} failed={failed} collisions={collisions}",
                    flush=True,
                )
            continue

        try:
            mkdir_p(args.device, new_album, made)
        except Exception as exc:
            print(f"failed creating album {new_album}: {exc}", file=sys.stderr)
            failed += 1
            continue

        for old_file in sorted(files):
            new_file = "Music/" + old_file[len(prefix):]
            ok, collided = move_file_with_collision(args.device, old_file, new_file)
            if ok:
                file_moved += 1
                collisions += 1 if collided else 0
            else:
                skipped += 1
        if (album_moved + file_moved) % 100 == 0:
            print(
                f"progress album_moved={album_moved} file_moved={file_moved} skipped={skipped} failed={failed} collisions={collisions}",
                flush=True,
            )

    cleanup = "not_requested"
    if args.cleanup_empty and failed == 0:
        proc = run_mtools(["mdeltree", "-i", args.device, mpath("Music/Recovered")], timeout=300)
        cleanup = "removed" if proc.returncode == 0 else "failed_or_not_empty"
        if proc.returncode:
            print(proc.stdout, file=sys.stderr, end="")

    print(
        f"summary album_moved={album_moved} file_moved={file_moved} skipped={skipped} failed={failed} collisions={collisions} cleanup={cleanup}",
        flush=True,
    )
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
