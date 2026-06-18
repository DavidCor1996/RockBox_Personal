#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import re
import subprocess
import sys
import unicodedata
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
    proc = subprocess.run(cmd, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=timeout)
    if proc.returncode:
        print(proc.stdout, file=sys.stderr, end="")
        raise subprocess.CalledProcessError(proc.returncode, cmd, proc.stdout)
    return proc


def exists(device: str, path: str) -> bool:
    proc = subprocess.run(
        ["mdir", "-i", device, mpath(path)],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=60,
    )
    return proc.returncode == 0


def ensure_dirs(device: str, dest: str, made: set[str]) -> None:
    parts = dest.split("/")[:-1]
    current = ""
    for part in parts:
        current = f"{current}/{part}" if current else part
        if current in made:
            continue
        if not exists(device, current):
            run(["mmd", "-i", device, mpath(current)])
        made.add(current)


def unique_target(device: str, target: str, planned: set[str]) -> str:
    if target not in planned and not exists(device, target):
        planned.add(target)
        return target
    root, dot, ext = target.rpartition(".")
    if not dot or "/" in ext:
        root, ext = target, ""
    else:
        ext = "." + ext
    for idx in range(1, 10000):
        suffix = " [Recovered]" if idx == 1 else f" [Recovered {idx}]"
        candidate = f"{root}{suffix}{ext}"
        if candidate not in planned and not exists(device, candidate):
            planned.add(candidate)
            return candidate
    raise RuntimeError(f"could not find unique target for {target}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("device")
    parser.add_argument("manifest")
    parser.add_argument("--cleanup-empty", action="store_true")
    args = parser.parse_args()

    moves: list[tuple[str, str]] = []
    cover_moves: set[tuple[str, str]] = set()
    with open(args.manifest, newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle, delimiter="\t")
        for row in reader:
            old = normalize_dest(row.get("destination", ""))
            prefix = "Music/Recovered/"
            if not old.startswith(prefix):
                continue
            new = "Music/" + old[len(prefix):]
            moves.append((old, new))
            album_old = old.rsplit("/", 1)[0]
            album_new = new.rsplit("/", 1)[0]
            cover_moves.add((f"{album_old}/cover.jpg", f"{album_new}/cover.jpg"))

    all_moves = list(cover_moves) + moves
    made: set[str] = set()
    planned_targets: set[str] = set()
    moved = 0
    skipped = 0
    failed = 0
    collisions = 0

    for old, requested_new in all_moves:
        if not exists(args.device, old):
            skipped += 1
            continue
        try:
            new = unique_target(args.device, requested_new, planned_targets)
            if new != requested_new:
                collisions += 1
            ensure_dirs(args.device, new, made)
            proc = subprocess.run(
                ["mren", "-D", "s", "-i", args.device, mpath(old), mpath(new)],
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                timeout=120,
            )
            if proc.returncode:
                print(proc.stdout, file=sys.stderr, end="")
                failed += 1
                continue
        except Exception as exc:
            print(f"failed {old} -> {requested_new}: {exc}", file=sys.stderr)
            failed += 1
            continue
        moved += 1
        if moved % 100 == 0:
            print(f"progress moved={moved} skipped={skipped} failed={failed} collisions={collisions}", flush=True)

    cleanup = "not_requested"
    if args.cleanup_empty and failed == 0:
        proc = subprocess.run(
            ["mdeltree", "-i", args.device, mpath("Music/Recovered")],
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=120,
        )
        cleanup = "removed" if proc.returncode == 0 else "failed"
        if proc.returncode:
            print(proc.stdout, file=sys.stderr, end="")

    print(f"summary moved={moved} skipped={skipped} failed={failed} collisions={collisions} cleanup={cleanup}", flush=True)
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
