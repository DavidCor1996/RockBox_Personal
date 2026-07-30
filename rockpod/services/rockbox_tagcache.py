"""Read and write Rockbox tagcache (.tcd) files for RockPod devices."""

from __future__ import annotations

import errno
import glob
import os
import struct
from pathlib import Path
from shutil import copy2, rmtree

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
FIRMWARE_COMMIT_MARKER = "database_commit.tcd"
HOST_COMMIT_MARKER = "database_hostcommit.tcd"
HOST_TRANSACTION_DIR = ".rockpod_tagcache_transaction"
RECOVERY_BACKUP_DIR = "tagcache_backup"
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


def _fsync_file(path: Path) -> None:
    try:
        with path.open("rb") as handle:
            os.fsync(handle.fileno())
    except OSError as exc:
        raise TagcacheError(f"Could not make {path.name} durable: {exc}") from exc


def _fsync_directory(path: Path) -> None:
    try:
        fd = os.open(path, os.O_RDONLY)
        try:
            os.fsync(fd)
        finally:
            os.close(fd)
    except OSError as exc:
        if exc.errno not in {errno.EBADF, errno.EINVAL, errno.ENOTSUP}:
            raise TagcacheError(f"Could not sync database directory: {exc}") from exc


def _matching_host_tagcache_files(rockbox_dir: Path) -> list[Path]:
    matches = set()
    for pattern in HOST_TAGCACHE_REMOVE_GLOBS:
        for raw_path in glob.glob(str(rockbox_dir / pattern)):
            path = Path(raw_path)
            if path.is_file() and path.name != HOST_COMMIT_MARKER:
                matches.add(path)
    return sorted(matches)


def _remove_existing_host_tagcache_files(
    rockbox_dir: Path,
    preserve_names: set[str] | None = None,
) -> None:
    preserve = preserve_names or set()
    for path in _matching_host_tagcache_files(rockbox_dir):
        if path.parent == rockbox_dir and path.name in preserve:
            continue
        path.unlink()


def _copy_database_set(source: Path, destination: Path) -> None:
    for source_path in _matching_host_tagcache_files(source):
        relative_path = source_path.relative_to(source)
        destination_path = destination / relative_path
        destination_path.parent.mkdir(parents=True, exist_ok=True)
        copy2(source_path, destination_path)
        _fsync_file(destination_path)
    _fsync_directory(destination)


def _replace_recovery_snapshot(rockbox_dir: Path) -> None:
    """Publish a verified copy for firmware's low-memory rollback path."""
    snapshot = rockbox_dir / RECOVERY_BACKUP_DIR
    staging = rockbox_dir / f".{RECOVERY_BACKUP_DIR}.new"

    rmtree(staging, ignore_errors=True)
    staging.mkdir(parents=False, exist_ok=False)
    try:
        for filename in HOST_TAGCACHE_FILES:
            source = rockbox_dir / filename
            destination = staging / filename
            copy2(source, destination)
            _fsync_file(destination)
        _fsync_directory(staging)

        rmtree(snapshot, ignore_errors=True)
        os.replace(staging, snapshot)
        _fsync_directory(rockbox_dir)
    finally:
        rmtree(staging, ignore_errors=True)


def _rollback_host_transaction(rockbox_dir: Path, transaction_dir: Path) -> None:
    marker = rockbox_dir / HOST_COMMIT_MARKER
    backup_dir = transaction_dir / "backup"

    _remove_existing_host_tagcache_files(rockbox_dir)
    if backup_dir.is_dir():
        _copy_database_set(backup_dir, rockbox_dir)
    _fsync_directory(rockbox_dir)

    if marker.exists():
        marker.unlink()
        _fsync_directory(rockbox_dir)
    rmtree(transaction_dir, ignore_errors=True)
    _fsync_directory(transaction_dir.parent)


def _recover_host_transaction(mount: Path, rockbox_dir: Path) -> None:
    transaction_dir = mount / HOST_TRANSACTION_DIR
    marker = rockbox_dir / HOST_COMMIT_MARKER

    if marker.exists():
        _rollback_host_transaction(rockbox_dir, transaction_dir)
    elif transaction_dir.exists():
        rmtree(transaction_dir, ignore_errors=True)
        _fsync_directory(mount)


def write_rockbox_tagcache_tracks(
    mount_path: str,
    tracks: list[dict],
    require_tracks: bool = False,
) -> dict:
    """Generate Rockbox tagcache files under ``mount_path/.rockbox``.

    The writer builds and validates a new set before publishing it. The old set
    is kept in a durable transaction directory until every replacement has
    reached storage, allowing rollback after an exception or process crash.
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

    rockbox_dir = mount / ".rockbox"
    rockbox_dir.mkdir(parents=True, exist_ok=True)
    _recover_host_transaction(mount, rockbox_dir)

    transaction_dir = mount / HOST_TRANSACTION_DIR
    temp_root = transaction_dir / "new"
    temp_rockbox = temp_root / ".rockbox"
    backup_dir = transaction_dir / "backup"
    temp_rockbox.mkdir(parents=True, exist_ok=False)
    backup_dir.mkdir(parents=True, exist_ok=False)
    marker = rockbox_dir / HOST_COMMIT_MARKER
    try:
        _write_tagcache_files(temp_rockbox, entries)
        for filename in HOST_TAGCACHE_FILES:
            _fsync_file(temp_rockbox / filename)
        _fsync_directory(temp_rockbox)

        parsed = read_rockbox_tagcache_tracks(str(temp_root))
        if len(parsed) != len(entries):
            raise TagcacheError(
                f"Generated tagcache validation failed: expected {len(entries)} tracks, read {len(parsed)}"
            )
        if _mount_identity(mount) != mount_identity:
            raise TagcacheError("Device changed while the Rockbox database was being generated")

        _copy_database_set(rockbox_dir, backup_dir)
        marker.write_bytes(struct.pack("<I", TAGCACHE_MAGIC))
        _fsync_file(marker)
        _fsync_directory(rockbox_dir)

        for filename in HOST_TAGCACHE_FILES:
            os.replace(temp_rockbox / filename, rockbox_dir / filename)
            _fsync_file(rockbox_dir / filename)

        _remove_existing_host_tagcache_files(
            rockbox_dir,
            preserve_names=set(HOST_TAGCACHE_FILES) | {HOST_COMMIT_MARKER},
        )
        _fsync_directory(rockbox_dir)

        parsed = read_rockbox_tagcache_tracks(str(mount), allow_transaction=True)
        if len(parsed) != len(entries):
            raise TagcacheError(
                f"Published tagcache validation failed: expected {len(entries)} tracks, read {len(parsed)}"
            )

        marker.unlink()
        _fsync_directory(rockbox_dir)
        _replace_recovery_snapshot(rockbox_dir)
        rmtree(transaction_dir)
        _fsync_directory(mount)

        return {
            "success": True,
            "track_count": len(entries),
            "files": [str(rockbox_dir / filename) for filename in HOST_TAGCACHE_FILES],
        }
    except Exception:
        if marker.exists():
            _rollback_host_transaction(rockbox_dir, transaction_dir)
        raise
    finally:
        if not marker.exists():
            rmtree(transaction_dir, ignore_errors=True)


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


def _decode_tag_entry(blob: bytes, offset: int, endian: str) -> str:
    if offset < 0 or offset + 8 > len(blob):
        raise TagcacheError("String tag offset is outside its database file")
    tag_length, _idx_id = struct.unpack_from(f"{endian}ii", blob, offset)
    if tag_length <= 0:
        raise TagcacheError("String tag has an invalid length")
    data_start = offset + 8
    data_end = data_start + tag_length
    if data_end > len(blob):
        raise TagcacheError("String tag extends past the end of its database file")
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


def read_rockbox_tagcache_tracks(
    mount_path: str,
    allow_transaction: bool = False,
) -> list[dict]:
    """Return audio device-track rows from Rockbox tagcache files."""
    base = Path(mount_path) / ".rockbox"
    if not allow_transaction and (
        (base / HOST_COMMIT_MARKER).exists() or
        (base / FIRMWARE_COMMIT_MARKER).exists()
    ):
        raise TagcacheError("Rockbox database transaction is incomplete")

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

    _magic, datasize, entry_count, _serial, _commitid, dirty = struct.unpack_from(
        f"{endian}6i", master_blob, 0
    )
    if dirty:
        raise TagcacheError("database_idx.tcd is marked dirty")
    if datasize < 0 or entry_count < 0:
        raise TagcacheError("database_idx.tcd has invalid size fields")
    row_size = (TAG_COUNT + 1) * 4
    expected_size = 24 + (entry_count * row_size)
    if len(master_blob) < expected_size:
        raise TagcacheError("database_idx.tcd is shorter than expected")

    tag_blobs = {}
    for tag, filename in STRING_TAGS.items():
        path = base / filename
        if not path.is_file():
            raise TagcacheError(f"{filename} is missing")
        blob = path.read_bytes()
        if len(blob) < 12:
            raise TagcacheError(f"{filename} is truncated")
        magic, tag_datasize, tag_entries = struct.unpack_from(f"{endian}iii", blob, 0)
        if magic != TAGCACHE_MAGIC or tag_datasize < 0 or tag_entries < 0:
            raise TagcacheError(f"{filename} has an invalid header")
        if len(blob) < 12 + tag_datasize:
            raise TagcacheError(f"{filename} is shorter than expected")
        tag_blobs[tag] = blob

    tracks = []
    for idx in range(entry_count):
        row_offset = 24 + (idx * row_size)
        row = list(struct.unpack_from(f"{endian}{TAG_COUNT + 1}i", master_blob, row_offset))
        # Rockbox does not guarantee that the tag offsets retained in a
        # deleted master row still reference live string-table entries. Skip
        # tombstones before dereferencing those offsets; _parse_row() also
        # checks the flag as a defensive backstop for normal callers.
        if row[-1] & FLAG_DELETED:
            continue
        for tag, blob in tag_blobs.items():
            row[tag] = _decode_tag_entry(blob, row[tag], endian)
        parsed = _parse_row(row, mount_path)
        if parsed:
            tracks.append(parsed)

    return tracks
