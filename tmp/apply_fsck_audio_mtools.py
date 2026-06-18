#!/usr/bin/env python3
"""Apply an FSCK audio recovery manifest with mtools on an unmounted FAT volume."""

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


def run(cmd: list[str], ok_exists: bool = False) -> subprocess.CompletedProcess[str]:
    proc = subprocess.run(cmd, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=60)
    if proc.returncode and not ok_exists:
        print(proc.stdout, file=sys.stderr, end="")
        raise subprocess.CalledProcessError(proc.returncode, cmd, proc.stdout)
    if proc.returncode and ok_exists:
        text = proc.stdout.lower()
        if "already exists" not in text and "file exists" not in text:
            print(proc.stdout, file=sys.stderr, end="")
            raise subprocess.CalledProcessError(proc.returncode, cmd, proc.stdout)
    return proc


def ensure_dirs(device: str, dest: str, made: set[str]) -> None:
    parts = dest.split("/")[:-1]
    current = ""
    for part in parts:
        current = f"{current}/{part}" if current else part
        if current in made:
            continue
        probe = subprocess.run(
            ["mdir", "-i", device, mpath(current)],
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=60,
        )
        if probe.returncode:
            run(["mmd", "-i", device, mpath(current)])
        made.add(current)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("device")
    parser.add_argument("manifest")
    parser.add_argument("--limit", type=int)
    parser.add_argument("--skip-existing", action="store_true")
    args = parser.parse_args()

    made: set[str] = set()
    moved = 0
    skipped = 0
    failed = 0
    with open(args.manifest, newline="", encoding="utf-8") as fh:
        reader = csv.DictReader(fh, delimiter="\t")
        for row in reader:
            if args.limit is not None and moved >= args.limit:
                break
            source = row["source"]
            dest = normalize_dest(row["destination"])
            if not source or not dest:
                skipped += 1
                continue
            source_path = mpath(source)
            dest_path = mpath(dest)
            try:
                ensure_dirs(args.device, dest, made)
                proc = subprocess.run(
                    ["mren", "-D", "s", "-i", args.device, source_path, dest_path],
                    text=True,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT,
                    timeout=60,
                )
                if proc.returncode:
                    text = proc.stdout.lower()
                    if args.skip_existing and ("not found" in text or "no match" in text):
                        skipped += 1
                        continue
                    print(proc.stdout, file=sys.stderr, end="")
                    failed += 1
                    continue
            except subprocess.CalledProcessError:
                failed += 1
                continue
            moved += 1
            print(f"moved\t{source}\t{dest}", flush=True)
            if moved % 50 == 0:
                print(f"progress moved={moved} skipped={skipped} failed={failed}", file=sys.stderr, flush=True)

    print(f"summary moved={moved} skipped={skipped} failed={failed}", file=sys.stderr)
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
