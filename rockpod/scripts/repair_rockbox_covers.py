#!/usr/bin/env python3
"""Create missing Rockbox cover.jpg files and sync them to a mounted device."""

from __future__ import annotations

import argparse
import base64
import csv
import hashlib
import os
import shutil
from io import BytesIO
from pathlib import Path

from PIL import Image
from mutagen import File as MutagenFile


AUDIO_EXTS = {
    ".mp3",
    ".flac",
    ".ogg",
    ".m4a",
    ".aac",
    ".alac",
    ".aiff",
    ".aif",
    ".wav",
    ".wma",
    ".ape",
    ".wv",
    ".opus",
}
COVER_CANDIDATES = (
    "cover.jpg",
    "cover.jpeg",
    "cover.png",
    "folder.jpg",
    "folder.jpeg",
    "folder.png",
    "front.jpg",
    "front.jpeg",
    "front.png",
    "album.jpg",
    "album.jpeg",
    "album.png",
    "albumart.jpg",
    "albumart.jpeg",
    "albumart.png",
    "artwork.jpg",
    "artwork.jpeg",
    "artwork.png",
    "Cover.jpg",
    "Cover.jpeg",
    "Cover.png",
    "Folder.jpg",
    "Folder.jpeg",
    "Folder.png",
)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def album_dirs(music_root: Path) -> list[Path]:
    albums = set()
    for path in music_root.rglob("*"):
        if path.is_file() and path.suffix.lower() in AUDIO_EXTS:
            albums.add(path.parent)
    return sorted(albums)


def sidecar_cover(album_dir: Path) -> Path | None:
    for name in COVER_CANDIDATES:
        path = album_dir / name
        if path.is_file() and path.name != "cover.jpg":
            return path
    return None


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
        first = covr[0]
        return bytes(first)

    for key in ("metadata_block_picture", "METADATA_BLOCK_PICTURE"):
        values = tags.get(key) if hasattr(tags, "get") else None
        if not values:
            continue
        try:
            decoded = base64.b64decode(values[0])
        except Exception:
            continue
        marker = b"\xff\xd8"
        offset = decoded.find(marker)
        if offset >= 0:
            return decoded[offset:]
        marker = b"\x89PNG"
        offset = decoded.find(marker)
        if offset >= 0:
            return decoded[offset:]
    return None


def first_embedded_cover(album_dir: Path) -> tuple[Path | None, bytes | None]:
    for path in sorted(album_dir.iterdir()):
        if not path.is_file() or path.suffix.lower() not in AUDIO_EXTS:
            continue
        data = embedded_art_bytes(path)
        if data:
            return path, data
    return None, None


def write_jpeg_from_bytes(data: bytes, target: Path, max_side: int, quality: int) -> None:
    with Image.open(BytesIO(data)) as image:
        image = image.convert("RGB")
        if max_side > 0:
            image.thumbnail((max_side, max_side), Image.Resampling.LANCZOS)
        target.parent.mkdir(parents=True, exist_ok=True)
        image.save(target, "JPEG", quality=quality, optimize=True, progressive=False)


def create_local_cover(album_dir: Path, apply: bool, max_side: int, quality: int) -> tuple[str, str]:
    target = album_dir / "cover.jpg"
    if target.is_file():
        return "existing", str(target)

    sidecar = sidecar_cover(album_dir)
    if sidecar:
        if not apply:
            return "would_create_from_sidecar", str(sidecar)
        if sidecar.suffix.lower() in {".jpg", ".jpeg"}:
            shutil.copy2(sidecar, target)
        else:
            write_jpeg_from_bytes(sidecar.read_bytes(), target, max_side, quality)
        return "created_from_sidecar", str(sidecar)

    audio_path, data = first_embedded_cover(album_dir)
    if data:
        if not apply:
            return "would_extract_embedded", str(audio_path)
        write_jpeg_from_bytes(data, target, max_side, quality)
        return "extracted_embedded", str(audio_path)

    return "missing_source", ""


def sync_device_cover(local_cover: Path, local_album: Path, music_root: Path, device_music_root: Path, apply: bool) -> tuple[str, str]:
    try:
        rel_album = local_album.relative_to(music_root)
    except ValueError:
        return "outside_music_root", ""
    device_album = device_music_root / rel_album
    if not device_album.is_dir():
        return "device_album_missing", str(device_album)

    device_cover = device_album / "cover.jpg"
    if device_cover.is_file():
        try:
            if sha256(local_cover) == sha256(device_cover):
                return "device_up_to_date", str(device_cover)
        except OSError:
            pass
        if not apply:
            return "would_update_device", str(device_cover)
    else:
        if not apply:
            return "would_copy_device", str(device_cover)

    if apply:
        shutil.copy2(local_cover, device_cover)
        return "copied_device", str(device_cover)
    return "would_copy_device", str(device_cover)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--music-root", default=str(Path.home() / "Music"))
    parser.add_argument("--device-root", default="")
    parser.add_argument("--apply", action="store_true")
    parser.add_argument("--sync-device", action="store_true")
    parser.add_argument("--max-side", type=int, default=1000)
    parser.add_argument("--quality", type=int, default=90)
    parser.add_argument("--report", default="")
    args = parser.parse_args()

    music_root = Path(args.music_root).expanduser().resolve()
    device_music_root = Path(args.device_root).expanduser() / "Music" if args.device_root else None
    if device_music_root is not None:
        device_music_root = device_music_root.resolve()

    rows = []
    counts = {}
    for album_dir in album_dirs(music_root):
        local_status, source = create_local_cover(album_dir, args.apply, args.max_side, args.quality)
        local_cover = album_dir / "cover.jpg"
        device_status = "not_requested"
        device_path = ""
        if args.sync_device and device_music_root is not None and local_cover.is_file():
            device_status, device_path = sync_device_cover(
                local_cover,
                album_dir,
                music_root,
                device_music_root,
                args.apply,
            )
        counts[local_status] = counts.get(local_status, 0) + 1
        counts[device_status] = counts.get(device_status, 0) + 1
        rows.append(
            {
                "album": str(album_dir.relative_to(music_root)),
                "local_status": local_status,
                "source": source,
                "device_status": device_status,
                "device_path": device_path,
            }
        )

    if args.report:
        report = Path(args.report)
        report.parent.mkdir(parents=True, exist_ok=True)
        with report.open("w", newline="") as handle:
            writer = csv.DictWriter(handle, fieldnames=["album", "local_status", "source", "device_status", "device_path"])
            writer.writeheader()
            writer.writerows(rows)

    for key in sorted(counts):
        print(f"{key}: {counts[key]}")
    print(f"albums_scanned: {len(rows)}")
    if args.report:
        print(f"report: {args.report}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
