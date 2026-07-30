"""Album-list thumbnail/slide export helpers for Rockbox visual surfaces."""

from __future__ import annotations

import json
import os
import shutil
import struct
from pathlib import Path
from typing import Iterable

from PIL import Image

from services.artwork_manager import ArtworkManager


ALBUMLIST_REL = Path(".rockbox") / "albumlist"

# thumbs.pack: fixed-size raw RGB565 thumbnail records aligned to the
# index.tsv data-line order, so the firmware album browser loads a row's
# art with one seek+read instead of a per-file BMP decode.
THUMB_PACK_MAGIC = b"ALTB"
THUMB_PACK_VERSION = 1
THUMB_PACK_SIZE = 40


def _rgb565_bytes(image: Image.Image) -> bytes:
    pixels = image.tobytes()
    record = bytearray(len(pixels) // 3 * 2)
    for i in range(len(pixels) // 3):
        value = (
            ((pixels[i * 3] >> 3) << 11)
            | ((pixels[i * 3 + 1] >> 2) << 5)
            | (pixels[i * 3 + 2] >> 3)
        )
        record[i * 2] = value & 0xFF
        record[i * 2 + 1] = value >> 8
    return bytes(record)


def _write_thumb_pack(pack_path: Path, thumb_sources: list[str | None]) -> bool:
    size = THUMB_PACK_SIZE
    record_pixel_bytes = size * size * 2
    temp_path = pack_path.with_name(pack_path.name + ".tmp")
    pack_path.parent.mkdir(parents=True, exist_ok=True)
    with open(temp_path, "wb") as handle:
        handle.write(THUMB_PACK_MAGIC)
        handle.write(
            struct.pack(
                "<HHHHI", THUMB_PACK_VERSION, size, size, 0, len(thumb_sources)
            )
        )
        for source in thumb_sources:
            payload = b""
            width = height = 0
            if source and os.path.isfile(source):
                try:
                    with Image.open(source) as image:
                        image = image.convert("RGB")
                        if image.size[0] > size or image.size[1] > size:
                            image.thumbnail((size, size), Image.Resampling.LANCZOS)
                        width, height = image.size
                        payload = _rgb565_bytes(image)
                except OSError:
                    payload = b""
                    width = height = 0
            present = 1 if payload else 0
            handle.write(struct.pack("<BBBB", present, 0, width, height))
            handle.write(payload)
            handle.write(b"\x00" * (record_pixel_bytes - len(payload)))
    os.replace(temp_path, pack_path)
    return True


def _row_dict(row):
    return dict(row) if hasattr(row, "keys") else dict(row)


def _album_key(row):
    artist = row.get("album_artist") or row.get("artist") or "Unknown Artist"
    album = row.get("album") or "Unknown Album"
    return row.get("album_group_key") or f"{artist}\0{album}"


def _album_groups(rows: Iterable[dict], limit: int = 0):
    groups = {}
    for row in rows:
        if str(row.get("media_type") or "audio").lower() != "audio":
            continue
        key = _album_key(row)
        item = groups.setdefault(
            key,
            {
                "group_key": key,
                "artist": row.get("album_artist") or row.get("artist") or "Unknown Artist",
                "album": row.get("album") or "Unknown Album",
                "tracks": [],
                "device_dirs": set(),
            },
        )
        item["tracks"].append(row)
        device_path = str(row.get("device_path") or "").strip()
        if device_path:
            item["device_dirs"].add(os.path.dirname(device_path))
        if limit and len(groups) >= limit:
            break
    return dict(sorted(groups.items(), key=lambda item: str(item[0]).casefold()))


def _copy_if_changed(src: str, dst: Path) -> bool:
    if not src:
        return False
    src_path = Path(src)
    if not src_path.is_file():
        return False
    dst.parent.mkdir(parents=True, exist_ok=True)
    if dst.is_file() and dst.stat().st_size == src_path.stat().st_size:
        try:
            if dst.read_bytes() == src_path.read_bytes():
                return False
        except OSError:
            pass
    shutil.copy2(src_path, dst)
    return True


def _deploy_bundle(bundle_albumlist: Path, mount: str | os.PathLike[str]) -> dict:
    mount_path = Path(mount)
    target = mount_path / ALBUMLIST_REL
    target.mkdir(parents=True, exist_ok=True)
    copied = 0
    for src in bundle_albumlist.rglob("*"):
        if not src.is_file():
            continue
        rel = src.relative_to(bundle_albumlist)
        if _copy_if_changed(str(src), target / rel):
            copied += 1
    return {"mount": str(mount_path), "target": str(target), "copied": copied}


def generate_albumlist_art(
    db,
    config,
    *,
    output_root: str | os.PathLike[str] | None = None,
    mount: str | os.PathLike[str] | None = None,
    force: bool = False,
    synced_only: bool = False,
    limit: int = 0,
) -> dict:
    """Render a Rockbox `.rockbox/albumlist` bundle from the RockPod library."""

    where = "WHERE media_type = 'audio'"
    if synced_only:
        where += " AND (synced_to_device = 1 OR COALESCE(device_path, '') != '')"
    rows = [
        _row_dict(row)
        for row in db.fetchall(
            f"SELECT * FROM tracks {where} "
            "ORDER BY COALESCE(NULLIF(album_artist, ''), artist), album, disc_number, track_number"
        )
    ]

    artwork = ArtworkManager(config.artwork_cache_dir, config)
    try:
        groups = _album_groups(rows, limit=limit)
        output_base = Path(output_root or (Path(config.cache_dir) / "albumlist_export"))
        bundle_albumlist = output_base / ALBUMLIST_REL
        thumb_dir = bundle_albumlist / "thumbs"
        slide_dir = bundle_albumlist / "slides"
        thumb_dir.mkdir(parents=True, exist_ok=True)
        slide_dir.mkdir(parents=True, exist_ok=True)

        entries = []
        created = {"thumbs": 0, "slides": 0}
        missing = {"thumbs": 0, "slides": 0}

        for album_key, album_info in groups.items():
            thumb_src, _thumb_hash, thumb_name, album_id = (
                artwork.export_album_list_thumbnail(album_info, force=force)
            )
            if not album_id:
                album_id = artwork.album_list_id(album_key)
            thumb_rel = ""
            if thumb_src and thumb_name:
                thumb_rel = f"thumbs/{thumb_name}"
                if _copy_if_changed(thumb_src, thumb_dir / thumb_name):
                    created["thumbs"] += 1
            else:
                missing["thumbs"] += 1

            slide_src, _slide_hash, slide_name, slide_album_id = (
                artwork.export_album_list_slide(album_info, force=force)
            )
            if not album_id and slide_album_id:
                album_id = slide_album_id
            slide_rel = ""
            if slide_src and slide_name:
                slide_rel = f"slides/{slide_name}"
                if _copy_if_changed(slide_src, slide_dir / slide_name):
                    created["slides"] += 1
            else:
                missing["slides"] += 1

            entries.append(
                {
                    "album_id": album_id,
                    "thumb": thumb_rel,
                    "slide": slide_rel,
                    "artist": album_info.get("artist", ""),
                    "album": album_info.get("album", ""),
                    "group_key": album_key,
                    "device_dirs": "|".join(sorted(album_info.get("device_dirs") or [])),
                }
            )

        manifest_src, _manifest_hash = artwork.export_album_list_manifest(entries)
        manifest_dst = bundle_albumlist / "index.tsv"
        manifest_copied = _copy_if_changed(manifest_src, manifest_dst)

        manifest_rows = artwork.album_list_manifest_rows(entries)
        thumb_sources = [
            str(bundle_albumlist / row["thumb"]) if row["thumb"] else None
            for row in manifest_rows
        ]
        pack_dst = bundle_albumlist / "thumbs.pack"
        _write_thumb_pack(pack_dst, thumb_sources)

        report = {
            "album_count": len(groups),
            "output_root": str(output_base),
            "albumlist_dir": str(bundle_albumlist),
            "manifest": str(manifest_dst),
            "thumb_pack": str(pack_dst),
            "created": created,
            "missing": missing,
            "manifest_copied": manifest_copied,
            "synced_only": synced_only,
            "limit": limit,
            "deployed": None,
        }
        if mount:
            report["deployed"] = _deploy_bundle(bundle_albumlist, mount)

        return report
    finally:
        artwork.shutdown()


def format_albumlist_art_report(report: dict) -> str:
    lines = [
        "Album-list art export",
        f"Albums: {report['album_count']}",
        f"Output: {report['albumlist_dir']}",
        f"Manifest: {report['manifest']}",
        f"Thumbnails copied/refreshed: {report['created']['thumbs']}",
        f"Slides copied/refreshed: {report['created']['slides']}",
        f"Missing thumbnail art: {report['missing']['thumbs']}",
        f"Missing slide art: {report['missing']['slides']}",
    ]
    if report.get("deployed"):
        deployed = report["deployed"]
        lines.append(f"Deployed: {deployed['copied']} files to {deployed['target']}")
    return "\n".join(lines)


def albumlist_art_report_json(report: dict) -> str:
    return json.dumps(report, indent=2)
