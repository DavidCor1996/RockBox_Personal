"""Read Rockbox tagcache (.tcd) files into RockPod device inventory rows."""

from __future__ import annotations

import os
import struct
from pathlib import Path

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
TAG_BITRATE = 13
TAG_LENGTH = 14
TAG_COUNT = 23
FLAG_DELETED = 0x0001

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
}


class TagcacheError(RuntimeError):
    """Raised when Rockbox tagcache files are invalid or unreadable."""


def _rel_device_path(raw_path: str) -> str:
    text = str(raw_path or "").strip().replace("\\", "/")
    while text.startswith("/"):
        text = text[1:]
    return text


def _codec_for_path(device_path: str) -> str:
    suffix = Path(device_path).suffix.lower().lstrip(".")
    return suffix.upper()


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
