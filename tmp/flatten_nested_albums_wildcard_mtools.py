#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import re
import subprocess
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


def run(cmd: list[str], timeout: int = 300) -> subprocess.CompletedProcess[str]:
    return subprocess.run(cmd, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=timeout)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("device")
    parser.add_argument("manifest")
    args = parser.parse_args()

    prefix = "Music/Recovered/"
    albums = set()
    with open(args.manifest, newline="", encoding="utf-8") as handle:
        for row in csv.DictReader(handle, delimiter="\t"):
            recovered = normalize_dest(row.get("destination", ""))
            if recovered.startswith(prefix):
                album = ("Music/" + recovered[len(prefix):]).rsplit("/", 1)[0]
                albums.add(album)

    moved = 0
    skipped = 0
    removed = 0
    failed = 0
    for album in sorted(albums):
        nested = f"{album}/{album.rsplit('/', 1)[-1]}"
        proc = run(["mmove", "-i", args.device, mpath(f"{nested}/*"), mpath(album)])
        if proc.returncode == 0:
            moved += 1
            rm = run(["mdeltree", "-i", args.device, mpath(nested)], timeout=120)
            if rm.returncode == 0:
                removed += 1
        else:
            text = (proc.stdout or "").lower()
            if "no match" in text or "not found" in text or "cannot initialize" in text:
                skipped += 1
            else:
                failed += 1
                print(proc.stdout, end="")
        if (moved + skipped + failed) % 25 == 0:
            print(f"progress moved_albums={moved} skipped={skipped} removed={removed} failed={failed}", flush=True)

    print(f"summary moved_albums={moved} skipped={skipped} removed={removed} failed={failed}", flush=True)
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
