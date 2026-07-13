"""Read and write Rockbox tagcache (.tcd) files for RockPod devices."""

from __future__ import annotations

import glob
import os
import struct
import tempfile
from pathlib import Path
from shutil import rmtree

from app.config import SUPPORTED_FORMATS
from models.track import compute_metadata_hash

TAGCACHE_MAGIC = 0x54434810
UNTAGGED = "<Untagged>"

TAG_ARTIST = 0
TAG_ALBUM = 1
TAG_GENRE = 2
TAG_TITLE = 3
TAG_FILENAME = 4
TAG_COMPOSER = 5
TAG_COMMENT = 6
TAG_ALBUMARTIST = 7
TAG_GROUPING = 8
TAG_YEAR = 9
TAG_DISCNUMBER = 10
TAG_TRACKNUMBER = 11
TAG_VIRT_CANONICALARTIST = 12
TAG_BITRATE = 13
TAG_LENGTH = 14
TAG_PLAYCOUNT = 15
TAG_RATING = 16
TAG_PLAYTIME = 17
TAG_LASTPLAYED = 18
TAG_COMMITID = 19
TAG_MTIME = 20
TAG_LASTELAPSED = 21
TAG_LASTOFFSET = 22
TAG_COUNT = 23
FLAG_DELETED = 0x0001
UNTAGGED_INDEX_ID = 0xFFFF

STRING_TAGS = {
    TAG_ARTIST: "database_0.tcd",
    TAG_ALBUM: "database_1.tcd",
    TAG_GENRE: "database_2.tcd",
    TAG_TITLE: "database_3.tcd",
    TAG_FILENAME: "database_4.tcd",
    TAG_COMPOSER: "database_5.tcd",
    TAG_COMMENT: "database_6.tcd",
    TAG_ALBUMARTIST: "database_7.tcd",
    TAG_GROUPING: "database_8.tcd",
    TAG_VIRT_CANONICALARTIST: "database_12.tcd",
}

ROW_SIZE = (TAG_COUNT + 1) * 4
HOST_TAGCACHE_FILES = [
    "database_idx.tcd",
    "database_0.tcd",
    "database_1.tcd",
    "database_2.tcd",
    "database_3.tcd",
    "database_4.tcd",
    "database_5.tcd",
    "database_6.tcd",
    "database_7.tcd",
    "database_8.tcd",
    "database_12.tcd",
]
HOST_TAGCACHE_REMOVE_GLOBS = [
    "database*.tcd",
    "tagcache*.tcd",
    os.path.join("database", "database*.tcd"),
    os.path.join("database", "tagcache*.tcd"),
]
UNIQUE_STRING_TAGS = {
    TAG_ARTIST,
    TAG_ALBUM,
    TAG_GENRE,
    TAG_COMPOSER,
    TAG_COMMENT,
    TAG_ALBUMARTIST,
    TAG_GROUPING,
    TAG_VIRT_CANONICALARTIST,
}
SORTED_STRING_TAGS = UNIQUE_STRING_TAGS | {TAG_TITLE}


class TagcacheError(RuntimeError):
    """Raised when Rockbox tagcache files are invalid or unreadable."""


def _mount_identity(path: Path) -> tuple[int, int, int]:
    stat = path.stat()
    vfs = os.statvfs(path)
    return stat.st_dev, stat.st_ino, vfs.f_blocks * vfs.f_frsize


def write_rockbox_tagcache_tracks(
    mount_path: str,
    tracks: list[dict],
    require_tracks: bool = False,
) -> dict:
    """Generate Rockbox tagcache files under ``mount_path/.rockbox``.

    The writer builds into a temporary root first and validates the generated
    files with ``read_rockbox_tagcache_tracks`` before swapping them into place.
    Only Rockbox database/tagcache files are removed or replaced.
    """
    mount = Path(mount_path)
    if not mount.is_dir():
        raise TagcacheError("Device mount path is unavailable")
    mount_identity = _mount_identity(mount)

    entries = [_normalize_writer_entry(track, str(mount)) for track in tracks]
    entries = [entry for entry in entries if entry]
    entries.sort(key=lambda item: item["filename"].casefold())
    if require_tracks and not entries:
        raise TagcacheError("No supported audio tracks are available for database repair")

    temp_root = Path(tempfile.mkdtemp(prefix=".rockpod_tagcache_", dir=str(mount)))
    try:
        temp_rockbox = temp_root / ".rockbox"
        temp_rockbox.mkdir(parents=True, exist_ok=True)
        _write_tagcache_files(temp_rockbox, entries)
        parsed = read_rockbox_tagcache_tracks(str(temp_root))
        if len(parsed) != len(entries):
            raise TagcacheError(
                f"Generated tagcache validation failed: expected {len(entries)} tracks, read {len(parsed)}"
            )
        if _mount_identity(mount) != mount_identity:
            raise TagcacheError("Device changed while the Rockbox database was being generated")

        rockbox_dir = mount / ".rockbox"
        rockbox_dir.mkdir(parents=True, exist_ok=True)
        _remove_existing_host_tagcache_files(rockbox_dir)
        for filename in HOST_TAGCACHE_FILES:
            os.replace(temp_rockbox / filename, rockbox_dir / filename)

        return {
            "success": True,
            "track_count": len(entries),
            "files": [str(rockbox_dir / filename) for filename in HOST_TAGCACHE_FILES],
        }
    finally:
        rmtree(temp_root, ignore_errors=True)


def write_rockbox_tagcache_from_device_inventory(
    db,
    device,
    device_key: str = "",
    require_tracks: bool = False,
) -> dict:
    """Generate Rockbox tagcache from RockPod's cached device inventory."""
    mount_path = getattr(device, "mount_path", "") if device else ""
    if not mount_path:
        raise TagcacheError("Device mount path is unavailable")
    key = device_key or getattr(device, "stable_device_key", "") or f"rockbox:{mount_path}"
    tracks = [dict(row) for row in db.get_all_device_tracks(key)]
    missing_ids = [
        track.get("id")
        for track in tracks
        if track.get("id") is not None
        and track.get("device_path")
        and not os.path.isfile(os.path.join(mount_path, _rel_device_path(track.get("device_path", ""))))
    ]
    if missing_ids:
        placeholders = ",".join(["?"] * len(missing_ids))
        db.execute(
            f"UPDATE device_tracks SET present_on_device = 0, last_scan = datetime('now') "
            f"WHERE id IN ({placeholders})",
            tuple(missing_ids),
        )
        missing_id_set = set(missing_ids)
        tracks = [
            track for track in tracks
            if track.get("id") not in missing_id_set
        ]
    return write_rockbox_tagcache_tracks(
        mount_path,
        tracks,
        require_tracks=require_tracks,
    )


def _rel_device_path(raw_path: str) -> str:
    text = str(raw_path or "").strip().replace("\\", "/")
    while text.startswith("/"):
        text = text[1:]
    return text


def _codec_for_path(device_path: str) -> str:
    suffix = Path(device_path).suffix.lower().lstrip(".")
    return suffix.upper()


def _to_int(value, default=0):
    try:
        if value in (None, ""):
            return default
        return int(float(value))
    except (TypeError, ValueError):
        return default


def _string_tag(value: object) -> str:
    text = str(value or "").strip()
    return text if text else UNTAGGED


def _sort_tag_key(value: str) -> tuple[int, str]:
    text = str(value or "")
    if text == UNTAGGED:
        return (0, "")
    return (1, text.casefold())


def _normalize_writer_entry(track: dict, mount_path: str) -> dict | None:
    filename = _rel_device_path(track.get("device_path", ""))
    if not filename:
        return None
    suffix = Path(filename).suffix.lower()
    if suffix not in SUPPORTED_FORMATS:
        return None

    full_path = os.path.join(mount_path, filename)
    if not os.path.isfile(full_path):
        return None

    artist = _string_tag(track.get("artist"))
    album_artist = _string_tag(track.get("album_artist") or track.get("artist"))
    title = _string_tag(track.get("title") or Path(filename).stem)
    grouping = _string_tag(track.get("grouping") or title)

    return {
        TAG_ARTIST: artist,
        TAG_ALBUM: _string_tag(track.get("album")),
        TAG_GENRE: _string_tag(track.get("genre")),
        TAG_TITLE: title,
        TAG_FILENAME: "/" + filename,
        TAG_COMPOSER: _string_tag(track.get("composer")),
        TAG_COMMENT: _string_tag(track.get("comment")),
        TAG_ALBUMARTIST: album_artist,
        TAG_GROUPING: grouping,
        TAG_VIRT_CANONICALARTIST: artist if artist != UNTAGGED else album_artist,
        TAG_YEAR: _to_int(track.get("year")),
        TAG_DISCNUMBER: _to_int(track.get("disc_number"), 1) or 1,
        TAG_TRACKNUMBER: _to_int(track.get("track_number")),
        TAG_BITRATE: _to_int(track.get("bitrate")),
        TAG_LENGTH: max(_to_int(float(track.get("duration") or 0) * 1000), 0),
        TAG_PLAYCOUNT: _to_int(track.get("play_count")),
        TAG_RATING: _to_int(track.get("rating")),
        TAG_PLAYTIME: _to_int(track.get("play_time")),
        TAG_LASTPLAYED: _to_int(track.get("last_played")),
        TAG_COMMITID: 1,
        TAG_MTIME: _to_int(os.path.getmtime(full_path)),
        TAG_LASTELAPSED: 0,
        TAG_LASTOFFSET: 0,
        "filename": filename,
    }


def _pack_header(datasize: int, entry_count: int) -> bytes:
    return struct.pack("<iii", TAGCACHE_MAGIC, datasize, entry_count)


def _tag_entry_bytes(value: str, idx_id: int) -> bytes:
    encoded = str(value).encode("utf-8") + b"\0"
    return struct.pack("<ii", len(encoded), idx_id) + encoded


def _write_tagcache_files(rockbox_dir: Path, entries: list[dict]) -> None:
    offsets_by_entry = [{tag: 0 for tag in STRING_TAGS} for _entry in entries]

    for tag, filename in STRING_TAGS.items():
        body = bytearray()
        items = []
        if tag in UNIQUE_STRING_TAGS:
            grouped = {}
            for entry_idx, entry in enumerate(entries):
                value = entry[tag]
                grouped.setdefault(value, []).append(entry_idx)
            for value, entry_indices in grouped.items():
                items.append((value, UNTAGGED_INDEX_ID, entry_indices))
            items.sort(key=lambda item: _sort_tag_key(item[0]))
        else:
            for entry_idx, entry in enumerate(entries):
                items.append((entry[tag], entry_idx, [entry_idx]))
            if tag in SORTED_STRING_TAGS:
                items.sort(key=lambda item: (_sort_tag_key(item[0]), item[1]))

        for value, idx_id, entry_indices in items:
            offset = 12 + len(body)
            body.extend(_tag_entry_bytes(value, idx_id))
            for entry_idx in entry_indices:
                offsets_by_entry[entry_idx][tag] = offset

        (rockbox_dir / filename).write_bytes(_pack_header(len(body), len(items)) + body)

    master_body = bytearray()
    for entry_idx, entry in enumerate(entries):
        row = [0] * TAG_COUNT + [0]
        for tag in range(TAG_COUNT):
            if tag in STRING_TAGS:
                row[tag] = offsets_by_entry[entry_idx][tag]
            else:
                row[tag] = _to_int(entry.get(tag))
        master_body.extend(struct.pack(f"<{TAG_COUNT + 1}i", *row))

    master_header = struct.pack(
        "<6i",
        TAGCACHE_MAGIC,
        len(master_body),
        len(entries),
        0,
        1,
        0,
    )
    (rockbox_dir / "database_idx.tcd").write_bytes(master_header + master_body)


def _remove_existing_host_tagcache_files(rockbox_dir: Path) -> None:
    seen = set()
    for pattern in HOST_TAGCACHE_REMOVE_GLOBS:
        for raw_path in glob.glob(str(rockbox_dir / pattern)):
            path = Path(raw_path)
            if path.name in {".", ".."} or path in seen:
                continue
            seen.add(path)
            if path.is_file():
                path.unlink()


def _decode_tag_entry(blob: bytes, offset: int, endian: str) -> str:
    if offset < 0 or offset + 8 > len(blob):
        return ""
    tag_length, _idx_id = struct.unpack_from(f"{endian}ii", blob, offset)
    if tag_length <= 0:
        return ""
    data_start = offset + 8
    data_end = min(data_start + tag_length, len(blob))
    raw = blob[data_start:data_end]
    value = raw.split(b"\0", 1)[0].decode("utf-8", errors="replace").strip()
    return "" if value == UNTAGGED else value


def _parse_row(row, mount_path: str) -> dict | None:
    flag = row[-1]
    if flag & FLAG_DELETED:
        return None

    filename = _rel_device_path(row[TAG_FILENAME])
    if not filename:
        return None
    suffix = Path(filename).suffix.lower()
    if suffix not in SUPPORTED_FORMATS:
        return None

    full_path = os.path.join(mount_path, filename)
    file_size = 0
    try:
        file_size = os.path.getsize(full_path)
    except OSError:
        pass

    title = row[TAG_TITLE] or Path(filename).stem
    artist = row[TAG_ARTIST]
    album_artist = row[TAG_ALBUMARTIST] or artist
    album = row[TAG_ALBUM]
    genre = row[TAG_GENRE]
    composer = row[TAG_COMPOSER]
    duration = max(float(row[TAG_LENGTH] or 0) / 1000.0, 0.0)
    bitrate = max(int(row[TAG_BITRATE] or 0), 0)
    disc_number = int(row[TAG_DISCNUMBER] or 1) or 1
    track_number = int(row[TAG_TRACKNUMBER] or 0) or None
    year = int(row[TAG_YEAR] or 0) or None
    codec = _codec_for_path(filename)
    metadata_hash = compute_metadata_hash(
        title,
        artist,
        album,
        album_artist,
        track_number,
        disc_number,
        genre,
        year,
        composer,
        duration,
        bitrate,
        codec,
        "audio",
    )

    return {
        "device_path": filename,
        "file_size": file_size,
        "title": title,
        "artist": artist,
        "album": album,
        "album_artist": album_artist,
        "genre": genre,
        "year": year,
        "track_number": track_number,
        "disc_number": disc_number,
        "duration": duration,
        "bitrate": bitrate,
        "codec": codec,
        "metadata_hash": metadata_hash,
        "file_hash": "",
        "present_on_device": 1,
    }


def read_rockbox_tagcache_tracks(mount_path: str) -> list[dict]:
    """Return audio device-track rows from Rockbox tagcache files."""
    base = Path(mount_path) / ".rockbox"
    master_path = base / "database_idx.tcd"
    if not master_path.is_file():
        raise TagcacheError("database_idx.tcd is missing")

    master_blob = master_path.read_bytes()
    if len(master_blob) < 24:
        raise TagcacheError("database_idx.tcd is truncated")

    magic_le = struct.unpack_from("<I", master_blob, 0)[0]
    magic_be = struct.unpack_from(">I", master_blob, 0)[0]
    if magic_le == TAGCACHE_MAGIC:
        endian = "<"
    elif magic_be == TAGCACHE_MAGIC:
        endian = ">"
    else:
        raise TagcacheError("database_idx.tcd has an unknown header")

    _magic, _datasize, entry_count, _serial, _commitid, _dirty = struct.unpack_from(
        f"{endian}6i", master_blob, 0
    )
    row_size = (TAG_COUNT + 1) * 4
    expected_size = 24 + (entry_count * row_size)
    if len(master_blob) < expected_size:
        raise TagcacheError("database_idx.tcd is shorter than expected")

    tag_blobs = {}
    for tag, filename in STRING_TAGS.items():
        path = base / filename
        if not path.is_file():
            raise TagcacheError(f"{filename} is missing")
        tag_blobs[tag] = path.read_bytes()

    tracks = []
    for idx in range(entry_count):
        row_offset = 24 + (idx * row_size)
        row = list(struct.unpack_from(f"{endian}{TAG_COUNT + 1}i", master_blob, row_offset))
        for tag, blob in tag_blobs.items():
            row[tag] = _decode_tag_entry(blob, row[tag], endian)
        parsed = _parse_row(row, mount_path)
        if parsed:
            tracks.append(parsed)

    return tracks
