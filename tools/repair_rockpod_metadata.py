#!/usr/bin/env python3
"""Repair known source-library and iPod metadata splits.

The operation is dry-run by default.  With ``--apply`` it updates both the
source library and a mounted iPod, verifies that AIFF sound chunks and FLAC
stream signatures did not change, and renames the two misnumbered device
files (including their lyric sidecars).
"""

from __future__ import annotations

import argparse
import hashlib
import struct
from pathlib import Path

from mutagen.aiff import AIFF
from mutagen.flac import FLAC
from mutagen.id3 import TALB, TPE1, TPE2


OLIVER_SOURCE = "Alone In A Crowd - Oliver Tree"
BAT_SOURCE = "Bat Out Of Hell - Meat Loaf"
EGYPT_SOURCE = "Egypt Station - Paul McCartney"
OLIVER_DEVICE = Path("Music/Oliver Tree/Alone in a Crowd")
BAT_DEVICE = Path("Music/Meat Loaf/Bat Out of Hell")
EGYPT_DEVICE = Path("Music/Paul McCartney/Egypt Station")


def aiff_sound_hash(path: Path) -> str:
    """Hash only the AIFF SSND chunk, excluding editable ID3 metadata."""
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        header = handle.read(12)
        if len(header) != 12 or header[:4] != b"FORM" or header[8:] not in (b"AIFF", b"AIFC"):
            raise ValueError(f"not an AIFF file: {path}")
        while True:
            chunk_header = handle.read(8)
            if not chunk_header:
                break
            if len(chunk_header) != 8:
                raise ValueError(f"truncated AIFF chunk header: {path}")
            chunk_id, chunk_size = struct.unpack(">4sI", chunk_header)
            if chunk_id == b"SSND":
                remaining = chunk_size
                while remaining:
                    block = handle.read(min(1024 * 1024, remaining))
                    if not block:
                        raise ValueError(f"truncated AIFF sound chunk: {path}")
                    digest.update(block)
                    remaining -= len(block)
                return digest.hexdigest()
            handle.seek(chunk_size + (chunk_size & 1), 1)
    raise ValueError(f"AIFF sound chunk missing: {path}")


def id3_text(audio: AIFF, key: str) -> str:
    frame = audio.tags.get(key) if audio.tags else None
    return str(frame) if frame is not None else ""


def repair_aiff(path: Path, values: dict[str, str], apply: bool) -> bool:
    audio = AIFF(path)
    before = {key: id3_text(audio, key) for key in values}
    if before == values:
        return False
    print(f"AIFF {path}")
    for key, value in values.items():
        print(f"  {key}: {before[key]!r} -> {value!r}")
    if not apply:
        return True

    sound_before = aiff_sound_hash(path)
    if audio.tags is None:
        audio.add_tags()
    frame_types = {"TALB": TALB, "TPE1": TPE1, "TPE2": TPE2}
    for key, value in values.items():
        audio.tags.delall(key)
        audio.tags.add(frame_types[key](encoding=3, text=[value]))
    audio.save()

    verified = AIFF(path)
    after = {key: id3_text(verified, key) for key in values}
    if after != values:
        raise RuntimeError(f"AIFF tag verification failed for {path}: {after}")
    if aiff_sound_hash(path) != sound_before:
        raise RuntimeError(f"AIFF sound data changed while editing {path}")
    return True


def repair_flac(path: Path, track_number: int, apply: bool) -> bool:
    audio = FLAC(path)
    before = (audio.get("tracknumber") or [""])[0]
    wanted = str(track_number)
    if before == wanted:
        return False
    print(f"FLAC {path}\n  TRACKNUMBER: {before!r} -> {wanted!r}")
    if not apply:
        return True

    stream_md5 = getattr(audio.info, "md5_signature", None)
    audio["tracknumber"] = wanted
    audio.save()
    verified = FLAC(path)
    if (verified.get("tracknumber") or [""])[0] != wanted:
        raise RuntimeError(f"FLAC tag verification failed for {path}")
    if getattr(verified.info, "md5_signature", None) != stream_md5:
        raise RuntimeError(f"FLAC stream signature changed while editing {path}")
    return True


def require_files(folder: Path, suffix: str, expected: int) -> list[Path]:
    files = sorted(folder.glob(f"*{suffix}"))
    if len(files) != expected:
        raise RuntimeError(f"expected {expected} {suffix} files in {folder}, found {len(files)}")
    return files


def rename_pair(folder: Path, old_stem: str, new_stem: str, apply: bool) -> bool:
    changed = False
    for suffix in (".flac", ".lrc"):
        source = folder / f"{old_stem}{suffix}"
        target = folder / f"{new_stem}{suffix}"
        if target.exists() and not source.exists():
            continue
        if not source.exists():
            raise FileNotFoundError(source)
        if target.exists():
            raise FileExistsError(target)
        print(f"RENAME {source} -> {target}")
        changed = True
        if apply:
            source.rename(target)
            if not target.exists() or source.exists():
                raise RuntimeError(f"rename verification failed: {source}")
    return changed


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--device-root", type=Path, required=True)
    parser.add_argument("--apply", action="store_true")
    args = parser.parse_args()

    source = args.source_root
    device = args.device_root
    if not source.is_dir() or not device.is_dir() or not (device / ".rockbox").is_dir():
        parser.error("source library or mounted Rockbox device is unavailable")

    changed = 0
    oliver_source = source / OLIVER_SOURCE
    oliver_device = device / OLIVER_DEVICE
    oliver_repairs = (
        (oliver_source / "08 - Oliver Tree - Highlight Of My Life.flac", 8),
        (oliver_source / "09 - Oliver Tree - The First Night.flac", 9),
        (oliver_device / "00 - Highlight Of My Life.flac", 8),
        (oliver_device / "00 - The First Night.flac", 9),
    )
    for path, track_number in oliver_repairs:
        if not path.exists():
            # Permit verification after the device files have been renamed.
            path = path.with_name(path.name.replace("00 - Highlight", "08 - Highlight")
                                  .replace("00 - The First", "09 - The First"))
        changed += repair_flac(path, track_number, args.apply)

    for folder in (source / BAT_SOURCE, device / BAT_DEVICE):
        for path in require_files(folder, ".aiff", 7):
            changed += repair_aiff(path, {"TALB": "Bat Out of Hell"}, args.apply)

    for folder in (source / EGYPT_SOURCE, device / EGYPT_DEVICE):
        for path in require_files(folder, ".aiff", 16):
            changed += repair_aiff(
                path,
                {"TPE1": "Paul McCartney", "TPE2": "Paul McCartney"},
                args.apply,
            )

    changed += rename_pair(
        oliver_device, "00 - Highlight Of My Life", "08 - Highlight Of My Life", args.apply
    )
    changed += rename_pair(
        oliver_device, "00 - The First Night", "09 - The First Night", args.apply
    )
    print(f"{'Applied' if args.apply else 'Planned'} {changed} metadata/file repairs")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
