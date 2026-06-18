#!/usr/bin/env python3
"""Recover fsck.vfat FSCK####.REC audio chains into a Rockbox music tree."""

from __future__ import annotations

import argparse
import os
import re
import sys
from collections import Counter
from pathlib import Path
from typing import Iterable

from mutagen import File as MutagenFile


AUDIO_EXT = {
    "flac": ".flac",
    "mp3": ".mp3",
    "m4a": ".m4a",
    "ogg": ".ogg",
    "wav": ".wav",
}


def detect(path: Path) -> str:
    with path.open("rb", buffering=0) as fh:
        head = fh.read(64)
    if head.startswith(b"fLaC"):
        return "flac"
    if head.startswith(b"ID3"):
        return "mp3"
    if len(head) >= 2 and head[0] == 0xFF and head[1] in (0xFB, 0xF3, 0xF2, 0xFA):
        return "mp3"
    if len(head) >= 12 and head[4:8] == b"ftyp":
        return "m4a"
    if head.startswith(b"OggS"):
        return "ogg"
    if head.startswith(b"RIFF") and head[8:12] == b"WAVE":
        return "wav"
    if head.startswith(b"\xff\xd8\xff"):
        return "jpeg"
    if head.startswith(b"#EXTM3U"):
        return "m3u"
    return "unknown"


def clean_part(value: str | None, fallback: str) -> str:
    value = (value or "").strip() or fallback
    value = re.sub(r'[<>:"/\\|?*\x00-\x1f]', "_", value)
    value = re.sub(r"\s+", " ", value).strip(" .")
    return value[:120] or fallback


def first_tag(audio, names: Iterable[str]) -> str | None:
    if not audio or not getattr(audio, "tags", None):
        return None
    for name in names:
        value = audio.tags.get(name)
        if not value:
            continue
        if isinstance(value, list):
            value = value[0]
        if isinstance(value, bytes):
            value = value.decode("utf-8", "replace")
        return str(value)
    return None


def number_tag(value: str | None) -> str:
    if not value:
        return "00"
    match = re.search(r"\d+", value)
    if not match:
        return "00"
    return f"{int(match.group(0)):02d}"


def dest_for(source: Path, root: Path, kind: str) -> tuple[Path, object | None]:
    audio = MutagenFile(source, easy=True)
    artist = clean_part(first_tag(audio, ("artist", "albumartist", "album artist")), "Unknown Artist")
    album = clean_part(first_tag(audio, ("album",)), "Unknown Album")
    title = clean_part(first_tag(audio, ("title",)), source.stem)
    track = number_tag(first_tag(audio, ("tracknumber", "track")))
    disc = number_tag(first_tag(audio, ("discnumber", "disc")))
    prefix = f"{disc}-{track}" if disc != "00" and disc != "01" else track
    ext = AUDIO_EXT[kind]
    return root / "Music" / "Recovered" / artist / album / f"{prefix} - {title}{ext}", audio


def unique_path(path: Path, source_stem: str, claimed: set[Path]) -> Path:
    if path not in claimed and not path.exists():
        claimed.add(path)
        return path
    candidate = path.with_name(f"{path.stem} [{source_stem}]{path.suffix}")
    index = 2
    while candidate in claimed or candidate.exists():
        candidate = path.with_name(f"{path.stem} [{source_stem}-{index}]{path.suffix}")
        index += 1
    claimed.add(candidate)
    return candidate


def extract_cover(source: Path, album_dir: Path, apply: bool) -> str:
    cover = album_dir / "cover.jpg"
    if cover.exists():
        return "exists"
    audio = MutagenFile(source)
    if not audio:
        return "none"
    data = None
    mime = ""
    pictures = getattr(audio, "pictures", None)
    if pictures:
        data = pictures[0].data
        mime = pictures[0].mime or ""
    elif getattr(audio, "tags", None):
        for tag in audio.tags.values():
            if tag.__class__.__name__ == "APIC":
                data = tag.data
                mime = tag.mime or ""
                break
        if data is None:
            covr = audio.tags.get("covr")
            if covr:
                item = covr[0]
                data = bytes(item)
                mime = "image/jpeg"
    if not data:
        return "none"
    if "png" in mime.lower():
        cover = album_dir / "cover.png"
    if apply:
        cover.parent.mkdir(parents=True, exist_ok=True)
        cover.write_bytes(data)
    return cover.name


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("ipod_root", type=Path)
    parser.add_argument("--apply", action="store_true")
    parser.add_argument("--manifest", type=Path)
    args = parser.parse_args()

    root = args.ipod_root
    if not root.is_dir():
        print(f"not a directory: {root}", file=sys.stderr)
        return 2

    rows = []
    counts = Counter()
    claimed: set[Path] = set()
    scanned = 0
    with os.scandir(root) as entries:
        for entry in entries:
            if not (entry.name.startswith("FSCK") and entry.name.endswith(".REC")):
                continue
            scanned += 1
            if scanned % 100 == 0:
                print(f"scanned {scanned}", file=sys.stderr, flush=True)
            source = root / entry.name
            try:
                kind = detect(source)
            except OSError as exc:
                counts[f"read_error:{exc.__class__.__name__}"] += 1
                continue
            counts[kind] += 1
            if kind not in AUDIO_EXT:
                continue
            try:
                dest, _audio = dest_for(source, root, kind)
            except Exception as exc:
                counts[f"tag_error:{exc.__class__.__name__}"] += 1
                dest = root / "Music" / "Recovered" / "Unknown Artist" / "Unknown Album" / f"{source.stem}{AUDIO_EXT[kind]}"
            dest = unique_path(dest, source.stem, claimed)
            cover_status = "dry-run"
            rows.append((source, dest, kind))
            print(f"{source.name}\t{kind}\t{dest.relative_to(root)}", flush=True)
            if args.apply:
                dest.parent.mkdir(parents=True, exist_ok=True)
                os.replace(source, dest)
                cover_status = extract_cover(dest, dest.parent, True)
            else:
                cover_status = extract_cover(source, dest.parent, False)
            if rows:
                rows[-1] = (source, dest, f"{kind};cover={cover_status}")

    if args.manifest:
        args.manifest.parent.mkdir(parents=True, exist_ok=True)
        with args.manifest.open("w", encoding="utf-8") as fh:
            fh.write("source\tkind\tdestination\n")
            for source, dest, kind in rows:
                fh.write(f"{source.name}\t{kind}\t{dest.relative_to(root)}\n")

    print("\nsummary", file=sys.stderr)
    for key, value in counts.most_common():
        print(f"{key}\t{value}", file=sys.stderr)
    print(f"audio_recoverable\t{len(rows)}", file=sys.stderr)
    if not args.apply:
        print("dry run only; rerun with --apply to move files", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
