#!/usr/bin/env python3
from __future__ import annotations

import argparse
import base64
import csv
import os
import re
import shutil
from io import BytesIO
from pathlib import Path

from mutagen import File as MutagenFile
from PIL import Image


AUDIO_EXTS = {".flac", ".mp3", ".m4a", ".aac", ".ogg", ".opus", ".wav", ".aiff", ".aif"}


def detect_rec(path: Path) -> str:
    with path.open("rb", buffering=0) as handle:
        head = handle.read(64)
    if head.startswith(b"\xff\xd8\xff"):
        return "jpeg"
    if head.startswith(b"#EXTM3U") or head.startswith(b"#PLAYLIST"):
        return "m3u"
    if head.startswith(b"fLaC"):
        return "flac"
    if head.startswith(b"ID3") or (len(head) >= 2 and head[0] == 0xFF and head[1] in (0xFB, 0xF3, 0xF2, 0xFA)):
        return "mp3"
    if len(head) >= 12 and head[4:8] == b"ftyp":
        return "m4a"
    if head.startswith(b"OggS"):
        return "ogg"
    return "unknown"


def clean_part(value: str) -> str:
    value = re.sub(r'[<>:"/\\|?*\x00-\x1f]', "_", value)
    value = re.sub(r"\s+", " ", value).strip(" .")
    return value[:96] or "Unknown"


def embedded_art_bytes(audio_path: Path) -> bytes | None:
    try:
        audio = MutagenFile(str(audio_path))
    except Exception:
        return None
    if audio is None:
        return None
    pictures = getattr(audio, "pictures", None)
    if pictures:
        return pictures[0].data
    tags = getattr(audio, "tags", None)
    if not tags:
        return None
    for value in tags.values():
        if getattr(value, "FrameID", "") == "APIC" and getattr(value, "data", None):
            return value.data
    covr = tags.get("covr") if hasattr(tags, "get") else None
    if covr:
        return bytes(covr[0])
    for key in ("metadata_block_picture", "METADATA_BLOCK_PICTURE"):
        values = tags.get(key) if hasattr(tags, "get") else None
        if not values:
            continue
        try:
            decoded = base64.b64decode(values[0])
        except Exception:
            continue
        for marker in (b"\xff\xd8", b"\x89PNG"):
            offset = decoded.find(marker)
            if offset >= 0:
                return decoded[offset:]
    return None


def write_cover(data: bytes, target: Path) -> None:
    with Image.open(BytesIO(data)) as image:
        image = image.convert("RGB")
        image.thumbnail((600, 600), Image.Resampling.LANCZOS)
        target.parent.mkdir(parents=True, exist_ok=True)
        image.save(target, "JPEG", quality=90, optimize=True, progressive=False)


def stage_missing_covers(root: Path, stage: Path) -> list[dict[str, str]]:
    music = root / "Music"
    albums: dict[Path, list[Path]] = {}
    for path in music.rglob("*"):
        if path.is_file() and path.suffix.lower() in AUDIO_EXTS:
            albums.setdefault(path.parent, []).append(path)

    rows: list[dict[str, str]] = []
    for album_dir in sorted(albums):
        rel = album_dir.relative_to(root)
        target_on_device = album_dir / "cover.jpg"
        if target_on_device.is_file():
            rows.append({"kind": "cover", "source": "", "target": str(rel / "cover.jpg"), "status": "exists"})
            continue
        status = "missing_embedded"
        source = ""
        for audio in sorted(albums[album_dir]):
            data = embedded_art_bytes(audio)
            if not data:
                continue
            target = stage / rel / "cover.jpg"
            try:
                write_cover(data, target)
            except Exception as exc:
                status = f"error:{exc.__class__.__name__}"
            else:
                status = "staged"
                source = str(audio.relative_to(root))
            break
        rows.append({"kind": "cover", "source": source, "target": str(rel / "cover.jpg"), "status": status})
    return rows


def stage_rec_report(root: Path) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    for path in sorted(root.glob("FSCK*.REC")):
        try:
            kind = detect_rec(path)
        except OSError as exc:
            kind = f"read_error:{exc.__class__.__name__}"
        if kind == "jpeg":
            target = Path("Recovered FSCK") / "Images" / f"{path.stem}.jpg"
        elif kind == "m3u":
            target = Path("Recovered FSCK") / "Playlists" / f"{path.stem}.m3u"
        elif kind in {"flac", "mp3", "m4a", "ogg"}:
            target = Path("Recovered FSCK") / "Audio Stragglers" / f"{path.stem}.{kind}"
        else:
            target = Path("Recovered FSCK") / "Unknown" / path.name
        rows.append({"kind": "rec", "source": path.name, "target": str(target), "status": kind})
    return rows


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("ipod_root", type=Path)
    parser.add_argument("--stage", type=Path, default=Path("tmp/ipod_recovery_stage"))
    parser.add_argument("--report", type=Path, default=Path("tmp/ipod_recovery_and_covers.tsv"))
    args = parser.parse_args()

    if args.stage.exists():
        shutil.rmtree(args.stage)
    args.stage.mkdir(parents=True, exist_ok=True)

    rows = []
    rows.extend(stage_missing_covers(args.ipod_root, args.stage))
    rows.extend(stage_rec_report(args.ipod_root))

    args.report.parent.mkdir(parents=True, exist_ok=True)
    with args.report.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=["kind", "source", "target", "status"], delimiter="\t")
        writer.writeheader()
        writer.writerows(rows)

    counts: dict[str, int] = {}
    for row in rows:
        key = f"{row['kind']}:{row['status']}"
        counts[key] = counts.get(key, 0) + 1
    for key in sorted(counts):
        print(f"{key}\t{counts[key]}")
    print(f"stage\t{args.stage}")
    print(f"report\t{args.report}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
