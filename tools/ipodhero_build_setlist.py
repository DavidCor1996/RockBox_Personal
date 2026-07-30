#!/usr/bin/env python3
"""Generate iPod Hero charts and index rows for a whole setlist.

This is a batch driver around ``ipodhero_generate.py``. It exists because a
setlist is many songs and every chart must be paired with the exact audio
master the iPod will play; doing that by hand invites mismatched identities.

The setlist is a tab separated file. Comments and blank lines are ignored and
each remaining row is:

    device_path  title  artist  skin_id

``device_path`` is the path as Rockbox reports it on the player, for example
``/Music/The Beatles/Revolver/01 - Taxman.flac``. ``--music-root`` is where
that same tree is mounted on this computer, so the generator can read the real
audio and record its size and CRC identity.

Charts are always labeled GENERATED. As the generator's own report states,
they still need human timing and musical review.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import sys


REPO = Path(__file__).resolve().parent.parent
GENERATE = REPO / "tools" / "ipodhero_generate.py"


def read_setlist(path: Path) -> list[tuple[str, str, str, str]]:
    rows = []
    for number, line in enumerate(
        path.read_text(encoding="utf-8").splitlines(), 1
    ):
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        fields = line.split("\t")
        if len(fields) != 4:
            raise ValueError(
                f"{path}:{number}: expected 4 tab separated fields, "
                f"got {len(fields)}"
            )
        device_path, title, artist, skin = (field.strip() for field in fields)
        if not device_path.startswith("/"):
            raise ValueError(f"{path}:{number}: device path must be absolute")
        for label, value, limit in (
            ("title", title, 63), ("artist", artist, 63), ("skin", skin, 31)
        ):
            if not value:
                raise ValueError(f"{path}:{number}: empty {label}")
            if len(value.encode("utf-8")) > limit:
                raise ValueError(
                    f"{path}:{number}: {label} exceeds {limit} bytes: {value!r}"
                )
        rows.append((device_path, title, artist, skin))
    return rows


def local_audio(music_root: Path, device_path: str) -> Path:
    """Map a player-side path onto the same tree mounted on this computer."""
    relative = device_path.lstrip("/")
    candidate = music_root / relative
    if candidate.is_file():
        return candidate
    # Allow --music-root to point straight at the directory that holds the
    # first path component, e.g. a Music folder that is not called "Music".
    parts = relative.split("/", 1)
    if len(parts) == 2:
        candidate = music_root / parts[1]
        if candidate.is_file():
            return candidate
    raise FileNotFoundError(f"no local audio for {device_path} under {music_root}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--setlist", required=True, type=Path)
    parser.add_argument("--music-root", required=True, type=Path,
                        help="local mount point holding the player's audio")
    parser.add_argument("--charts", required=True, type=Path,
                        help="chart output directory")
    parser.add_argument("--index", required=True, type=Path,
                        help="index.tsv to create or update")
    parser.add_argument("--skip-existing", action="store_true",
                        help="leave songs already present in the index alone")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    rows = read_setlist(args.setlist)
    known = set()
    if args.skip_existing and args.index.is_file():
        for line in args.index.read_text(encoding="utf-8").splitlines():
            if line and not line.startswith("#"):
                known.add(line.split("\t")[0])

    failures = []
    for number, (device_path, title, artist, skin) in enumerate(rows, 1):
        if device_path in known:
            print(f"[{number}/{len(rows)}] skip (already indexed): {title}")
            continue
        audio = local_audio(args.music_root, device_path)
        print(f"[{number}/{len(rows)}] {artist} - {title}")
        command = [
            sys.executable, str(GENERATE),
            "--audio", str(audio),
            "--output-dir", str(args.charts),
            "--title", title,
            "--artist", artist,
            "--skin-id", skin,
            "--index-output", str(args.index),
            "--device-path", device_path,
            "--confirm-audio-identity",
        ]
        if args.dry_run:
            print("   " + " ".join(command))
            continue
        result = subprocess.run(command, stdout=subprocess.DEVNULL)
        if result.returncode != 0:
            failures.append(f"{artist} - {title}")
            print(f"   FAILED ({result.returncode})", file=sys.stderr)

    if failures:
        print(f"\n{len(failures)} song(s) failed:", file=sys.stderr)
        for failure in failures:
            print(f"  {failure}", file=sys.stderr)
        return 1
    print(f"\ncompleted {len(rows)} setlist row(s)")
    print("GENERATED CHARTS - HUMAN TIMING AND MUSICAL REVIEW REQUIRED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
